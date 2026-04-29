import os
from typing import Annotated, List, Optional
import typer

from commands.config import ExperimentContext
from dep_injection import build_experiment_context

from .typer_base import TyperDataClassMeta


CONFIG_ENV_VAR = "QFLEX_EX_Y"
DEFAULT_CONFIG_PATH = "config.yaml"


def _resolve_config_path(cli_value: Optional[str]) -> str:
    """
    Resolution order:
      1. --config CLI flag (if non-empty)
      2. QFLEX_EX_Y env var (if non-empty)
      3. config.yaml in the current directory (if it exists)
      4. raise
    """
    result = ""
    default_config_path = os.path.join(os.getcwd(), DEFAULT_CONFIG_PATH)
    env_value = os.environ.get(CONFIG_ENV_VAR, "").strip()
    
    if cli_value:
        result = cli_value

    elif env_value:
        result = env_value

    elif os.path.exists(default_config_path):
        result = default_config_path

    if result is not None and result.strip() != "":
        print(f"Using config file: {result}")
        return result

    raise typer.BadParameter(
        f"No config file specified. Pass --config / -c, "
        f"or set the {CONFIG_ENV_VAR} environment variable, "
        f"or ensure {DEFAULT_CONFIG_PATH} exists in the current directory."
    )


class ExperimentContextTyper(TyperDataClassMeta):
    def __init__(self, name):
        super().__init__(name=name, init_function=self.experiment_context_typer)

    def experiment_context_typer(
        self,
        config: Annotated[str, typer.Option(
            "--config", "-c",
            help=f"Path to YAML config. If empty, falls back to the "
                 f"{CONFIG_ENV_VAR} environment variable. and then to {DEFAULT_CONFIG_PATH} if it exists."
        )] = "",
        set_: Annotated[List[str], typer.Option(
            "--set", "-s",
            help="Override a config value, e.g. "
                 "-s components.experiment_context.core_count=16. "
                 "Can be passed multiple times."
        )] = None,
    ) -> ExperimentContext:
        config_path = _resolve_config_path(config)
        return build_experiment_context(config_path, set_ or [])