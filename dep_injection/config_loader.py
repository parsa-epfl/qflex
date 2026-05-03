from pathlib import Path
from omegaconf import OmegaConf, DictConfig


def load_config(path: str | Path) -> DictConfig:
    """Load YAML with `extends:` inheritance (recursive); resolve `_base_:`."""
    path = Path(path).resolve()
    cfg = OmegaConf.load(path)

    parent_name = cfg.pop("extends", None)
    if parent_name is not None:
        parent_path = path.parent / f"{parent_name}.yaml"
        if not parent_path.exists():
            raise FileNotFoundError(
                f"{path.name} extends '{parent_name}' but {parent_path} does not exist"
            )
        cfg = OmegaConf.merge(load_config(parent_path), cfg)

    return _apply_component_bases(cfg)


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