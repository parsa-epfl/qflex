from typing import Annotated, Callable, Optional, get_args, get_origin
import functools
import inspect
import os

import typer
from pydantic.fields import FieldInfo


CONFIG_ENV_VAR = "QFLEX_CONFIG"


def _typer_param_for(param: inspect.Parameter) -> inspect.Parameter:
    """
    Translate a factory parameter into the form Typer expects:
    Annotated[Optional[T], typer.Option(help=...)] with default None.

    All CLI flags become Optional[T] = None so "not passed" is distinguishable
    from "passed with the factory's default value" — critical when --config is
    used: only flags the user actually set should override YAML values. Factory
    defaults still apply (the wrapper drops None kwargs before calling the
    factory, so the factory's own defaults kick in).

    Pulls help text from pydantic Field(description=...) attached via Annotated.
    Suffixes the factory default into the help text so --help still tells the
    user what they get when they omit a flag.
    """
    annotation = param.annotation
    inner_type = annotation
    help_text = ""

    if get_origin(annotation) is Annotated:
        annotation_args = get_args(annotation)
        inner_type = annotation_args[0]
        for meta in annotation_args[1:]:
            if isinstance(meta, FieldInfo) and meta.description:
                help_text = meta.description
                break

    if param.default is not inspect.Parameter.empty:
        suffix = f"[factory default: {param.default!r}]"
        help_text = f"{help_text}  {suffix}" if help_text else suffix

    new_annotation = Annotated[Optional[inner_type], typer.Option(help=help_text, show_default=False)]
    return param.replace(annotation=new_annotation, default=None)


def _config_param() -> inspect.Parameter:
    """The injected --config / -c flag, common to every wrapped command."""
    annotation = Annotated[
        Optional[str],
        typer.Option(
            "--config",
            "-c",
            help=(
                f"Path to a YAML config file. If set (or ${CONFIG_ENV_VAR} is exported), "
                "the YAML is used to build the experiment context and per-field flags are ignored."
            ),
        ),
    ]
    return inspect.Parameter(
        "config",
        kind=inspect.Parameter.POSITIONAL_OR_KEYWORD,
        default=None,
        annotation=annotation,
    )


def data_class_wrap(target: Callable, *, name: str):
    """
    Decorator that splices `target`'s parameters into the wrapped Typer command,
    plus a `--config / -c` flag for YAML-driven config. At call time:

    - If --config was passed, or $QFLEX_CONFIG is set in the environment, resolve
      the YAML path (flag wins over env), build the experiment context via DI,
      and inject the result as kwargs[name]. Per-field flag values are discarded.
    - Otherwise, harvest the per-field flag values that were actually set, call
      target(**harvested), and inject the result as kwargs[name]. Raises a
      friendly error if any factory-required field wasn't provided.

    `target` is the factory whose signature is the single source of truth for
    both the YAML/DI path and the CLI. Pydantic Field(description=...) on
    target's parameters becomes the Typer --help text.
    """
    target_params = list(inspect.signature(target).parameters.values())
    target_param_names = {p.name for p in target_params}
    target_required_param_names = {
        p.name for p in target_params if p.default is inspect.Parameter.empty
    }
    if "config" in target_param_names:
        raise ValueError(
            f"Cannot wrap {target.__name__}: it has a parameter named 'config' "
            "which collides with the injected --config/-c flag."
        )
    typer_params = [_config_param()] + [_typer_param_for(p) for p in target_params]

    def func_wrapper(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            config_path = kwargs.pop("config", None) or os.environ.get(CONFIG_ENV_VAR)

            cli_overrides = {}
            for n in target_param_names:
                if n in kwargs:
                    v = kwargs.pop(n)
                    if v is None:
                        continue
                    # Click renders multi-value options (List[T]) as `()` / `[]`
                    # when the flag is omitted; treat that as "not passed".
                    if isinstance(v, (list, tuple)) and len(v) == 0:
                        continue
                    cli_overrides[n] = v

            if config_path:
                from dep_injection.builder import build_experiment_context
                kwargs[name] = build_experiment_context(
                    config_path,
                    component_overrides={name: cli_overrides} if cli_overrides else None,
                )
            else:
                missing = target_required_param_names - cli_overrides.keys()
                if missing:
                    flag_names = sorted(f"--{m.replace('_', '-')}" for m in missing)
                    raise typer.BadParameter(
                        "Missing required option(s): "
                        + ", ".join(flag_names)
                        + f". Either pass them as flags or provide --config <path> (or set ${CONFIG_ENV_VAR})."
                    )
                kwargs[name] = target(**cli_overrides)
            return func(*args, **kwargs)

        func_params = [
            p for p_name, p in inspect.signature(func).parameters.items()
            if p_name != name
        ]
        wrapper.__signature__ = inspect.Signature(parameters=typer_params + func_params)
        return wrapper

    return func_wrapper
