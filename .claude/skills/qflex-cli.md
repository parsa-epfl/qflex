---
name: qflex-cli
description: Use when working on the qflex CLI surface — adding/modifying pipeline subcommands, touching the [data_class_wrap](../../typer_inputs/config_wrapper.py) decorator, debugging Typer flag parsing, understanding precedence between CLI flags and YAML, or wiring help text. TRIGGER when the user mentions Typer, click flags, --config/-c, $QFLEX_CONFIG, the [qflex](../../qflex) script, mkdocs-click, or "how does the CLI build an ExperimentContext". SKIP for changes purely inside the YAML/DI layer or inside command body logic that doesn't touch flag handling.
---

# qflex CLI

The repo has two top-level CLIs. Both are Typer apps with `--help` autocompletion and a public docs page rendered via `mkdocs-click`.

- **[./dep](../../dep)** — host-side. Builds and starts the QFlex Docker dev container.
- **[./qflex](../../qflex)** — runs **inside** the container. Orchestrates the simulation pipeline; one subcommand per phase. **This is the interesting one.**

A separate, lower-level launcher [./runq](../../runq) takes a QEMU config file plus `+arg`/`-arg` overrides and `execvp`s a qemu-aarch64 binary directly. The pipeline doesn't use it; only reach for it when poking at QEMU directly.

## Pipeline command shape

Every pipeline command in [qflex](../../qflex) accepts the same `ExperimentContext`-building options plus a few command-specific flags. The per-field flags are **auto-derived** from `create_experiment_context`'s signature by the `@data_class_wrap` decorator — they are never hand-declared.

```python
@app.command()
@data_class_wrap(create_experiment_context, name="experiment_context")
def boot(
    experiment_context: ExperimentContext,
    vanilla: Annotated[bool, typer.Option(help="...")] = False,  # command-specific flag
):
    Boot(experiment_context=experiment_context, vanilla=vanilla).execute(...)
```

`@app.command()` bodies receive a fully-built `experiment_context: ExperimentContext` kwarg. Don't try to take an `ExperimentContext` directly as a Typer parameter — Typer can't introspect it as a flag tree.

## Four ways the user invokes a command

```bash
# 1) YAML only — the practical path; everything from the file
./qflex boot -c conf/dc.yaml

# 2) YAML via env var (same effect, no flag — useful when scripting)
QFLEX_CONFIG=conf/dc.yaml ./qflex boot

# 3) Pure CLI — no YAML; every factory-required field must be passed as a flag
./qflex boot --core-count 16 --quantum-size 1000 --doubled-vcpu \
             --llc-size-per-tile-mb 2 --is-parallel --network none \
             --memory-gb 64 --host-name zen3 --workload-name web_search \
             --primary-core-start 0 --is-consolidated --primary-ipc 1.5 \
             --population-seconds 5

# 4) YAML + selective CLI overrides — YAML is the base, passed flags win
./qflex boot -c conf/dc.yaml --core-count 32 --no-doubled-vcpu
```

## Precedence rule (highest to lowest)

For any `ExperimentContext` field, the final value is resolved in this order:

1. **Explicit CLI flag** (e.g. `--core-count 32`). A flag is "explicit" only when the user actually typed it — Typer's `[factory default: X]` in `--help` is documentation, not a value applied as an override.
2. **YAML value** under `components.experiment_context.<field>` from the file resolved via `-c <path>` or `$QFLEX_CONFIG` (flag wins over env).
3. **YAML extends chain.** `extends: <name>` loads parent first; child wins on conflict. Recursive.
4. **Factory default** — the `= default` on `create_experiment_context`'s parameter. Visible in `--help` as `[factory default: X]`.
5. **Required-with-no-source** → `typer.BadParameter` listing every missing flag. The check runs **once** after merging YAML keys (if any) with CLI override keys, so the same friendly error fires whether the gap is in pure-CLI, pure-YAML, or a YAML+CLI mix. The hint is path-aware: pure-CLI points at `--config` / `$QFLEX_CONFIG`; YAML path points at the loaded YAML file.

Resolution precedence for *which* YAML to load: explicit `-c <path>` > `$QFLEX_CONFIG` env var > no YAML. There is no implicit `./config.yaml` lookup.

## How `data_class_wrap` works ([typer_inputs/config_wrapper.py](../../typer_inputs/config_wrapper.py))

### Decorator time

1. Reads `inspect.signature(target).parameters`.
2. For each parameter, builds a synthetic Typer param. The annotation is widened to `Annotated[Optional[T], typer.Option(help=..., show_default=False)]` and the default is set to `None` — **regardless** of whether the factory's param was required or had a default. The factory's actual default (if any) is preserved separately by being formatted into the help text as `[factory default: X]`.
3. **List-typed factory params** (`List[T]` / `Optional[List[T]]`) get special handling: the CLI-facing type becomes `Optional[str]` (so users pass `--neighbor-node-list "1,2,3"`, not multi-flag repetition), `(comma-separated)` is appended to the help text, and a per-param converter `str -> [T(part) for part in s.split(",")]` is registered for call time. Click natively treats typed lists as `multiple=True`, which is the wrong UX here. See `_cli_type_and_converter` in [config_wrapper.py](../../typer_inputs/config_wrapper.py).
4. Prepends one synthetic param: `--config / -c` (`Optional[str] = None`).
5. Splices the resulting parameter list into the wrapped Typer command's signature, in addition to whatever per-command flags the command body declares.

The `Optional[T] = None` widening is the trick that lets the wrapper distinguish "user passed `--core-count 16`" from "user didn't pass anything; Typer filled in the factory's default" — without that, every CLI default would look like an override and clobber the YAML.

### Call time

1. Resolve `config_path = kwargs.pop("config", None) or os.environ.get("QFLEX_CONFIG")`. Flag wins over env via short-circuit OR.
2. Harvest per-field kwargs into a `cli_overrides` dict. For each field: drop if `None`; if a converter was registered, run it (string → list of typed elements); drop if the result is an empty list/tuple. So `--neighbor-node-list ""` collapses to "not passed", same as omitting the flag.
3. **Unified required-field check**: lazy-import `load_config` from `dep_injection.config_loader`; if `config_path`, load the YAML (with `extends:` resolved) and collect `components.<name>` keys plus any `_deps_` entries. Union with `cli_overrides.keys()`, subtract from the factory's required-param set. Any leftover → `typer.BadParameter` with a path-specific hint. This is the **only** missing-field check; it covers pure-CLI, pure-YAML, and YAML+CLI mixes alike.
4. Branch:
   - **YAML path** (`config_path` truthy): import `dep_injection.builder.build_experiment_context` lazily (so host envs without `injector`/`omegaconf` can still load `qflex` for `--help` and `mkdocs-click`); call it with `component_overrides={name: cli_overrides}` (or `None` if empty).
   - **CLI path** (`config_path` falsy): call `target(**cli_overrides)`. Factory defaults apply for any kwarg not in the dict.
5. Inject the result as `kwargs[name]` and invoke the wrapped command body.

## Adding a new pipeline command

```python
@app.command()
@data_class_wrap(create_experiment_context, name="experiment_context")
def my_phase(
    experiment_context: ExperimentContext,
    # any command-specific Typer flags here, with Annotated[T, typer.Option(help=...)]
):
    """Docstring becomes the command's help text."""
    MyPhaseExecutor(experiment_context=experiment_context, ...).execute(to_stdio=True, run_in_background=False)
```

Then implement `MyPhaseExecutor` per the qflex-commands skill (subclass `Executor`, build the bash invocation in `cmd()`).

## Adding a new ExperimentContext field

Edit `create_experiment_context`'s parameter list with `Annotated[T, Field(description="...")] = default`. The CLI flag, `--help` text, and YAML key all come along for free. No change to `data_class_wrap`. (For non-primitive deps, also see the qflex-dependency-injection skill.)

For **list-typed** fields (`List[int]`, `List[str]`, `Optional[List[float]]`, …), the CLI auto-renders as `Optional[str]` and parses comma-separated input back into the typed list. The element type just needs to accept a single-string constructor (`int(s)`, `float(s)`, `str(s)`). YAML keeps using real YAML lists (`[1, 2, 3]`); the conversion only happens on the CLI side.

## mkdocs-click integration

[docs_shim/qflex.py](../../docs_shim/qflex.py) loads the Typer `app` and converts it to a click `Command` via `typer.main.get_command`. [mk_docs/reference/qflex.md](../../mk_docs/reference/qflex.md) embeds it via `::: mkdocs-click`. Help text on every flag — including the auto-derived per-field flags — ends up on the published docs page automatically. Don't strip `Field(description=...)` from factory params unless you're OK with a blank doc cell.

## Common errors

- **`ValueError: Cannot wrap X: it has a parameter named 'config'`** — the factory has a kwarg colliding with the injected `--config / -c` flag. Rename the factory param.
- **`Missing required option(s): --foo, --bar, ...`** — fires on any path where YAML+CLI together don't cover every factory-required param. The trailing hint differs: pure-CLI suggests `--config` / `$QFLEX_CONFIG`; YAML path names the file and suggests adding the keys to it or passing them as flags.
- **`ModuleNotFoundError: No module named 'injector'` / `'omegaconf'`** when running with `-c` outside the container — both the YAML loader (used for the unified missing-field check) and `build_experiment_context` need these. Either install them or run inside the dev image. `--help` rendering itself stays clean because the imports are lazy.
- **`ValueError: invalid literal for int() with base 10: '1, 2, three'`** — a comma-separated list flag had an element the target type can't parse. Fix the input.

## Key files

- [qflex](../../qflex) — the Typer app; one `@app.command()` per pipeline phase.
- [typer_inputs/config_wrapper.py](../../typer_inputs/config_wrapper.py) — `data_class_wrap`, `_typer_param_for`, `_cli_type_and_converter`, `_config_param`. The load-bearing piece.
- [commands/config.py:467](../../commands/config.py#L467) — `create_experiment_context`, the factory whose signature drives the entire CLI flag set.
- [docs_shim/qflex.py](../../docs_shim/qflex.py) — the `mkdocs-click` adapter.
