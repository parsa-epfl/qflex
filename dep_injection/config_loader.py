from pathlib import Path
from omegaconf import OmegaConf, DictConfig


def load_config(path: str | Path, cmd_name: str | None = None) -> DictConfig:
    """Load YAML and resolve it in this order:

      1. **Recursive `extends:` merge** — the full chain is collapsed into a
         single doc via deep merge, with the child winning at every level
         (including `_leaf_defaults` and any other top-level base block).
      2. **Phase overlay** (`cfg[cmd_name]`, optional) — popped and deep-
         merged onto the doc with the same semantics. The block is shaped
         like a sub-YAML, so callers drive changes via `_leaf_defaults: {...}`
         (rule-level, propagates to every component using `_base_:`) or
         `components: {<name>: {...}}` (per-component). Components keep their
         `_base_:` references through this step.
      3. **`_base_:` resolution** — each component is replaced by
         `merge(<base>, comp)` so component fields still win over the base.

    CLI flags layer on top via the wrapper.
    """
    cfg = _load_raw(Path(path).resolve())
    if cmd_name is not None:
        cfg = _apply_phase_overlay(cfg, cmd_name)
    return _apply_component_bases(cfg)


def _load_raw(path: Path) -> DictConfig:
    """Load YAML + merge `extends:` chain WITHOUT resolving `_base_:`. Components
    keep their `_base_:` references through the whole chain so the outermost
    `_apply_component_bases` sees the fully-merged top-level base blocks."""
    cfg = OmegaConf.load(path)
    parent_name = cfg.pop("extends", None)
    if parent_name is None:
        return cfg
    parent_path = (path.parent / f"{parent_name}.yaml").resolve()
    if not parent_path.exists():
        raise FileNotFoundError(
            f"{path.name} extends '{parent_name}' but {parent_path} does not exist"
        )
    return OmegaConf.merge(_load_raw(parent_path), cfg)


def _apply_phase_overlay(cfg: DictConfig, cmd_name: str) -> DictConfig:
    """Pop `cfg[cmd_name]` (if present) and deep-merge it onto cfg before
    `_base_:` resolution. The phase block mirrors the main YAML's shape:
    `_leaf_defaults: {...}` updates the leaf base (and propagates to every
    component using `_base_: _leaf_defaults`); `components: {<name>: {...}}`
    updates a specific component before its `_base_:` is resolved.

    Top-level keys in the phase block must already exist in cfg (or be
    `components`); flat-key forms like `boot: { latencies_ns_list: [...] }`
    are rejected so they don't silently no-op.
    """
    phase = cfg.pop(cmd_name, None)
    if phase is None:
        return cfg
    if not isinstance(phase, DictConfig):
        raise ValueError(
            f"phase section '{cmd_name}:' must be a mapping, got {type(phase).__name__}"
        )
    unknown = [k for k in phase.keys() if k != "components" and k not in cfg]
    if unknown:
        raise ValueError(
            f"phase section '{cmd_name}:' has unrecognised top-level key(s) "
            f"{unknown}; the phase block mirrors the main YAML — use "
            f"'_leaf_defaults: {{...}}' (rule-level), 'components: {{<name>: {{...}}}}' "
            f"(per-component), or any other top-level base block already defined."
        )
    return OmegaConf.merge(cfg, phase)


def _apply_component_bases(cfg: DictConfig) -> DictConfig:
    """Resolve `_base_: <top-level-key>` on every component via deep merge.

    Component fields override base fields. The `_base_` key is stripped after
    resolution so the DI loader never sees it.
    """
    components = cfg.get("components")
    if components is None:
        return cfg
    for comp_name, comp in list(components.items()):
        base_name = comp.pop("_base_", None)
        if base_name is None:
            continue
        if base_name not in cfg:
            raise ValueError(
                f"component '{comp_name}' has _base_: '{base_name}' "
                f"but no top-level '{base_name}' is defined"
            )
        base = cfg[base_name]
        if not isinstance(base, DictConfig):
            raise ValueError(
                f"component '{comp_name}' _base_: '{base_name}' must point at a "
                f"mapping, got {type(base).__name__}"
            )
        components[comp_name] = OmegaConf.merge(base, comp)
    return cfg


def apply_cli_overrides(cfg: DictConfig, overrides: list[str]) -> DictConfig:
    """Apply ['key.path=value', ...] dotlist overrides on top of cfg."""
    if not overrides:
        return cfg
    return OmegaConf.merge(cfg, OmegaConf.from_dotlist(overrides))
