from pathlib import Path
from omegaconf import OmegaConf, DictConfig


def load_config(path: str | Path) -> DictConfig:
    """
    Load a YAML config with `extends:` inheritance.
    """
    path = Path(path).resolve()
    cfg = OmegaConf.load(path)

    parent_name = cfg.pop("extends", None)
    if parent_name is None:
        return cfg

    parent_path = path.parent / f"{parent_name}.yaml"
    if not parent_path.exists():
        raise FileNotFoundError(
            f"{path.name} extends '{parent_name}' but {parent_path} does not exist"
        )

    parent_cfg = load_config(parent_path)
    return OmegaConf.merge(parent_cfg, cfg)


def apply_cli_overrides(cfg: DictConfig, overrides: list[str]) -> DictConfig:
    if not overrides:
        return cfg
    return OmegaConf.merge(cfg, OmegaConf.from_dotlist(overrides))