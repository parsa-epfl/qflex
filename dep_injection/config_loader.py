from pathlib import Path
from omegaconf import OmegaConf, DictConfig


def load_config(path: str | Path) -> DictConfig:
    """Load YAML with `extends:` inheritance (recursive); resolve `_base_:`.

    `_base_` resolution runs ONCE at the outermost call, after the full extends
    chain has been merged. This is what lets a child YAML override a top-level
    block (e.g. `_leaf_defaults: { memory_gb: 4 }`) and have that override
    propagate into every component that uses `_base_: _leaf_defaults` —
    including components defined in a parent YAML. Eager resolution at every
    level (the previous behaviour) bakes the parent's `_leaf_defaults` values
    into components before the child's override gets a chance to merge.
    """
    return _apply_component_bases(_load_raw(Path(path).resolve()))


def _load_raw(path: Path) -> DictConfig:
    """Load YAML + merge `extends:` chain WITHOUT resolving `_base_:`. Components
    keep their `_base_:` references through the whole chain so the outermost
    `_apply_component_bases` sees the fully-merged `_leaf_defaults` (or whatever
    other top-level base block was overridden along the way)."""
    cfg = OmegaConf.load(path)
    parent_name = cfg.pop("extends", None)
    if parent_name is None:
        return cfg
    parent_path = path.parent / f"{parent_name}.yaml"
    if not parent_path.exists():
        raise FileNotFoundError(
            f"{path.name} extends '{parent_name}' but {parent_path} does not exist"
        )
    return OmegaConf.merge(_load_raw(parent_path), cfg)


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