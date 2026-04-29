from injector import Injector

from commands.config import ExperimentContext
from .config_loader import load_config, apply_cli_overrides
from .di_loader import ConfigDrivenModule


def build_experiment_context(
    config_path: str,
    overrides: list[str] | None = None,
) -> ExperimentContext:
    cfg = load_config(config_path)
    cfg = apply_cli_overrides(cfg, overrides or [])
    inj = Injector([ConfigDrivenModule(cfg)])
    return inj.get(ExperimentContext)