from injector import Injector
from omegaconf import OmegaConf

from commands.config import ExperimentContext
from .config_loader import load_config, apply_cli_overrides
from .di_loader import ConfigDrivenModule


def build_experiment_context(
    config_path: str,
    cmd_name: str | None = None,
    overrides: list[str] | None = None,
    component_overrides: dict[str, dict] | None = None,
) -> ExperimentContext:
    cfg = load_config(config_path, cmd_name=cmd_name)
    cfg = apply_cli_overrides(cfg, overrides or [])
    if component_overrides:
        cfg = OmegaConf.merge(
            cfg, OmegaConf.create({"components": component_overrides})
        )
    inj = Injector([ConfigDrivenModule(cfg)])
    return inj.get(ExperimentContext)
