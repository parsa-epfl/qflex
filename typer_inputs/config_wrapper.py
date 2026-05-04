from typing import Annotated, Callable, Optional, Union, get_args, get_origin
import functools
import inspect
import os

import typer
from pydantic.fields import FieldInfo


CONFIG_ENV_VAR = "QFLEX_CONFIG"


def _cli_type_and_converter(inner_type):
    """
    Decide how a factory-param's inner type should appear on the CLI.

    Returns (cli_type, converter):
    - cli_type: the type to advertise on the Typer flag.
    - converter: callable str -> list[T] for list-typed params, else None.

    Click natively treats `Annotated[Optional[List[int]], typer.Option(...)]` as
    multi-flag repetition (`--flag 1 --flag 2`), not comma-separated. To get the
    comma-separated UX without per-field custom ParamTypes, list-typed factory
    params are exposed as Optional[str] on the CLI and parsed in the wrapper.
    """
    if get_origin(inner_type) is Union:
        non_none = [a for a in get_args(inner_type) if a is not type(None)]
        if len(non_none) == 1:
            inner_type = non_none[0]

    if get_origin(inner_type) is list:
        (elem_type,) = get_args(inner_type)
        def convert(s: str):
            stripped = s.strip()
            if not stripped:
                return []
            return [elem_type(part.strip()) for part in stripped.split(",")]
        return str, convert
    return inner_type, None


def _typer_param_for(param: inspect.Parameter):
    """
    Translate a factory parameter into the form Typer expects:
    Annotated[Optional[T], typer.Option(help=...)] with default None.
    Returns (typer_param, converter) — converter is non-None for list-typed
    factory params, which become Optional[str] on the CLI and need
    string-to-list parsing at call time.

    All CLI flags become Optional[T] = None so "not passed" is distinguishable
    from "passed with the factory's default value" — critical when --config is
    used: only flags the user actually set should override YAML values. Factory
    defaults still apply (the wrapper drops None kwargs before calling the
    factory, so the factory's own defaults kick in).

    Pulls help text from pydantic Field(description=...) attached via Annotated.
    Suffixes the factory default into the help text so --help still tells the
    user what they get when they omit a flag. Adds a (comma-separated) hint
    when the param is list-typed.
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

    cli_type, converter = _cli_type_and_converter(inner_type)
    if converter is not None:
        help_text = f"{help_text} (comma-separated)".strip()

    if param.default is not inspect.Parameter.empty:
        suffix = f"[factory default: {param.default!r}]"
        help_text = f"{help_text}  {suffix}" if help_text else suffix

    new_annotation = Annotated[Optional[cli_type], typer.Option(help=help_text, show_default=False)]
    return param.replace(annotation=new_annotation, default=None), converter


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
    plus a `--config / -c` flag for YAML-driven config.

    Final-value precedence for any field of `target` (low → high; later wins):

      1. Factory default (`target`'s parameter default).
      2. The YAML's `extends:` chain — recursive merge of parent docs.
      3. The component's `_base_:` resolution against `_leaf_defaults`
         (or whatever top-level base it points at). Applied once, at the
         outermost `load_config` call, so `_leaf_defaults` overrides in
         child YAMLs propagate into components defined in parent YAMLs.
      4. The component's own per-leaf `experiment_context*: { ... }` block
         (e.g. `experiment_context_node_0`'s explicit overrides).
      5. **Command-scoped section: top-level `<func_name>: { ... }` block
         in the YAML.** Only applied when this command runs (matched by
         the wrapped function's Python name — `fw`, `run_idx`,
         `partition_cleanup`, etc.). Overrides every component matching
         `target`'s `_target_`. Use this to keep one YAML across multiple
         pipeline phases instead of forking a YAML per command.
      6. CLI flag the user explicitly passed (`--core-count`, `--memory-gb`, …).

    At call time:

    - If --config was passed, or $QFLEX_CONFIG is set in the environment,
      resolve the YAML path (flag wins over env), build the experiment context
      via DI honouring the precedence above, and inject the result as
      kwargs[name].
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
    typer_params = [_config_param()]
    converters: dict[str, Callable] = {}
    for p in target_params:
        new_param, converter = _typer_param_for(p)
        typer_params.append(new_param)
        if converter is not None:
            converters[p.name] = converter

    def func_wrapper(func):
        # Captured at decoration time so the wrapper can match the YAML's
        # top-level `<func_name>: { ... }` section to the running command.
        # Python form (underscores) — e.g. `fw`, `run_idx`, `partition_cleanup`.
        cmd_name = func.__name__

        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            config_path = kwargs.pop("config", None) or os.environ.get(CONFIG_ENV_VAR)

            cli_overrides = {}
            for n in target_param_names:
                if n in kwargs:
                    v = kwargs.pop(n)
                    if v is None:
                        continue
                    if n in converters:
                        v = converters[n](v)
                    # Click renders multi-value options (List[T]) as `()` / `[]`
                    # when the flag is omitted; treat that as "not passed".
                    if isinstance(v, (list, tuple)) and len(v) == 0:
                        continue
                    cli_overrides[n] = v

            yaml_keys: set[str] = set()
            cmd_overrides: dict = {}
            if config_path:
                from dep_injection.config_loader import load_config
                from dep_injection.di_loader import RESERVED_KEYS
                cfg = load_config(config_path)
                comp = cfg.get("components", {}).get(name)
                if comp is not None:
                    yaml_keys = {k for k in comp.keys() if k not in RESERVED_KEYS}
                    deps = comp.get("_deps_") or {}
                    yaml_keys.update(deps.keys())

                # Command-scoped overrides: top-level `<func_name>: { ... }`
                # block in the YAML applies ONLY when this command runs and
                # replaces the equivalent fields from the extends chain /
                # `_leaf_defaults` / per-component `experiment_context` block.
                # CLI flags still win over these. See data_class_wrap docstring
                # for the full precedence order.
                cmd_section = cfg.get(cmd_name)
                if cmd_section is not None:
                    cmd_overrides = {
                        k: v for k, v in cmd_section.items() if k not in RESERVED_KEYS
                    }
                    yaml_keys.update(cmd_overrides.keys())

            provided = cli_overrides.keys() | yaml_keys
            missing = target_required_param_names - provided
            if missing:
                flag_names = sorted(f"--{m.replace('_', '-')}" for m in missing)
                msg = "Missing required option(s): " + ", ".join(flag_names) + "."
                if config_path:
                    msg += f" Add them to the YAML config at {config_path} or pass them as CLI flags."
                else:
                    msg += f" Either pass them as flags or provide --config <path> (or set ${CONFIG_ENV_VAR})."
                raise typer.BadParameter(msg)

            if config_path:
                from dep_injection.builder import build_experiment_context
                # CLI > command-section > rest of the YAML chain.
                combined_overrides = {**cmd_overrides, **cli_overrides}
                comp_overrides = None
                if combined_overrides:
                    target_path = f"{target.__module__}.{target.__qualname__}"
                    comp_overrides = {
                        cn: combined_overrides
                        for cn, c in cfg.get("components", {}).items()
                        if c.get("_target_") == target_path
                    }
                kwargs[name] = build_experiment_context(
                    config_path,
                    component_overrides=comp_overrides,
                )
            else:
                kwargs[name] = target(**cli_overrides)
            return func(*args, **kwargs)

        func_params = [
            p for p_name, p in inspect.signature(func).parameters.items()
            if p_name != name
        ]
        wrapper.__signature__ = inspect.Signature(parameters=typer_params + func_params)
        return wrapper

    return func_wrapper
