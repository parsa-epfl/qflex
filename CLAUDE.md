# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

QFlex (PARSA-EPFL) is a full-system computer-architecture simulator that performs **timing (microarchitecture) simulation on top of QEMU emulation**, with statistical sampling so long workloads stay tractable. A run goes through four phases:

1. **Emulation** — boot a guest in fast QEMU, install/configure the workload, snapshot it.
2. **Functional warming (fast-forward)** — replay the workload in fast QEMU while a plugin keeps long-lived microarchitectural state (caches, TLBs, branch predictor) warm. Cheap-but-not-detailed.
3. **Sampling on FW** — pick *sampling units* (short measurement windows) along the FW timeline and snapshot the guest at each. One round of phases 2+3 is one *sample* (see terminology below).
4. **Timing simulation** — for each sampling unit, run a *different* QEMU bound to the Flexus timing model via a middleware shim, producing cycle-accurate stats.

The repo glues together vendored projects (all git submodules; run `git submodule update --init --recursive` after clone — submodules are large):

- [parallel-qemu/](parallel-qemu/) — the **fast** QEMU used for emulation and functional warming. Built without `--enable-libqflex`. In a prepared experiment's `run/` folder this is the unprefixed `qemu-system-aarch64` binary. See [parallel-qemu/CLAUDE.md](parallel-qemu/CLAUDE.md) for its internals (PDES multi-node stack, plugin loading).
- [WormCacheQFlex/](WormCacheQFlex/) — plugin loaded by **parallel-qemu during FW only**. Models long-term microarchitectural state (TLBs, branch predictor, caches). Not used in the timing phase, where Flexus has its own models. Expected at the repo root; `set_up_folders()` copies it into each experiment's `lib/`. See [WormCacheQFlex/CLAUDE.md](WormCacheQFlex/CLAUDE.md) for its internals (modes, checkpoint format, `parameter.rs` regeneration).
- [qemu/](qemu/) — the **timing** QEMU. Built with `--enable-libqflex --enable-snapvm-external`. Its job is mostly committing instructions in lockstep with the timing model so the model can be verified. Has its own submodule, `middleware`, that brokers the QEMU↔Flexus connection. In `run/` this is `vanilla-qemu-system-aarch64` (here "vanilla" means *non-parallel*, not unpatched). See [qemu/CLAUDE.md](qemu/CLAUDE.md) and [qemu/middleware/CLAUDE.md](qemu/middleware/CLAUDE.md) for the QFlex-specific delta and the QEMU↔Flexus IPC API.
- [flexus/](flexus/) — the timing model itself (Conan + CMake; build targets `knottykraken`, `semikraken`). Connects to the timing QEMU through the middleware shim. See [flexus/CLAUDE.md](flexus/CLAUDE.md) for its internals (FLEXUS_API/QEMU_API surface, QMP commands, wiring files).

Branch `cli-yaml-overhaul` is mid-migration: a YAML+DI config path was added in parallel to the per-flag Typer CLI (see [Architecture](#architecture) below). Both paths now share `create_experiment_context` as their single source of truth — adding a field there exposes it on both the CLI and the YAML schema. Old-style `*.args` files under [multi-node-experiments/](multi-node-experiments/), [multi-node-dc/](multi-node-dc/), [multi-node-dc-old-shanqing/](multi-node-dc-old-shanqing/), [multi-node-web-search/](multi-node-web-search/) predate the overhaul — assume any flag not present in `create_experiment_context`'s signature in [commands/config.py](commands/config.py) is stale.

## Statistical sampling terminology

The sampling vocabulary is project-specific. Use these meanings consistently when reading code or writing config:

- **Population** — the wall-clock duration of guest workload that is functionally warmed and sampled from. Set per workload via `population_seconds` (see [commands/workload/](commands/workload/)).
- **Sample** — one round of FW + sampling over the population. Triggered by `./qflex fw` (with `--sample-size N`). A run can do multiple samples: if the first sample's confidence interval is too wide, do another, larger one.
- **Sampling unit** — an individual measurement window inside a sample. With a 5-second population, a first sample might pick 100 sampling units; if [result.py](result.py) decides the result isn't tight enough, a follow-up sample with e.g. 500 sampling units is taken. The "is it enough?" decision is computed by [result.py](result.py) / [commands/result.py](commands/result.py); `analyze_sampling_result.py` is the related analysis helper.
- **Per-unit warming vs measurement** — every sampling unit in the **timing phase** is itself split into a *detailed-warming* prefix (where Flexus warms shorter-term µarch state — pipeline, store buffers, etc. — that WormCacheQFlex doesn't capture during FW) followed by an *actual measurement* segment that contributes to the reported numbers. The split is controlled by `--warming-ratio` and `--measurement-ratio` on `run-partition` / `run-single-partition` / `run-idx`.

Mental model: **population → samples → sampling units → (detailed warming + measurement) per unit**. Long-term µarch state is carried in via WormCacheQFlex during FW; short-term state is warmed per-unit in the timing phase.

## Where to look first

Before diving anywhere else, the **five minimum-core areas** that explain the whole tool are:

1. [qflex](qflex) — Typer CLI for running the simulation pipeline (one subcommand per phase).
2. [dep](dep) — Typer CLI for building/starting the Docker dev image (everything else runs inside it).
3. [typer_inputs/](typer_inputs/) — adapter layer: turns CLI flags into a populated `ExperimentContext`.
4. [commands/](commands/) — the actual logic. Every CLI subcommand resolves to a class here that takes an `ExperimentContext`, builds a bash command, and runs it via [commands/executer.py](commands/executer.py). [commands/config.py](commands/config.py) defines `ExperimentContext` and is the central data model.
5. [Makefile](Makefile) — drives Conan/Ninja builds of the submodules and stages their artifacts (`./qemu-saved/`, `./parallel-qemu-saved/`, `./kraken_out/`) for `set_up_folders()` to symlink into per-experiment `run/` dirs.

If a path of investigation doesn't touch one of these five, you're probably looking at vendored or auxiliary code.

## Two top-level CLIs

Both are Typer apps; run with `--help` for full options.

- [./dep](dep) — host-side. Builds and starts the QFlex Docker dev container.
  - `./dep build-docker [--debug] [--worm] [--push]`
  - `./dep start-docker --mounting-folder <path> [--debug] [--worm] [--start-directory <dir>]`
- [./qflex](qflex) — runs **inside** the container; orchestrates the simulation pipeline (see Pipeline below).

[./runq](runq) is a separate, lower-level launcher that takes a QEMU config file plus `+arg`/`-arg` overrides and `execvp`s a qemu-aarch64 binary directly. The pipeline doesn't use it; only reach for it when poking at QEMU directly.

## Build commands

All builds happen **inside** the dev container (started via `./dep start-docker ...`). The Makefile is the source of truth.

```sh
# One-time
make install-dev-requirements
make flexus-config

# Flexus (re-run when flexus/ changes)
make flexus-build MODE=release          # or MODE=debug
make flexus-clean-build MODE=release    # also runs `conan cache clean`

# QEMU variants — both are needed; they're built into separate saved trees
make qemu-config MODE=release && make qemu-build         # → ./qemu-saved/
make parallel-qemu-config && make parallel-qemu-build    # → ./parallel-qemu-saved/

# Docs (mkdocs)
make serve-docs        # 0.0.0.0:8888
make build-docs
```

The [build](build) shell script is an alternative entry point (`./build <sim>` where `<sim>` ∈ `knottykraken`, `semikraken`, `cq`/`qemu`, `q`, `docker`); the Makefile targets cover the same ground and are preferred.

There is **no test suite** in this repo. Don't fabricate `pytest` or `make test` instructions.

## Pipeline (the `./qflex` subcommands)

Every pipeline command builds the same `ExperimentContext` and accepts the same options for doing so: a YAML config via `-c <path>` (or `$QFLEX_CONFIG`), and/or per-field flags (`--core-count`, `--workload-name`, `--neighbor-node-list`, …). Plus a few command-specific flags that aren't part of the context (`--sample-size`, `--warming-ratio`, `--vanilla`, …). The per-field flags are auto-derived from `create_experiment_context`'s signature by `data_class_wrap`; for the full precedence rules and concrete invocations, see [Architecture](#configuration-factory-function-as-the-single-source-of-truth). The phases map onto the four-part flow described above:

| Phase | Subcommand(s) | What it does | Which QEMU |
|---|---|---|---|
| Emulation | `create-base-image`, `boot` | Alpine qcow2 in `./images/`; boot, install packages, `savevm base`, quit | parallel-qemu |
| FW prep | `load`, `initialize` | Long-term µarch warm-up; emit Flexus configs (`timing.cfg`, `flexus_configuration.json`, …) | parallel-qemu |
| FW + sample selection | `fw` | Run workload with WormCacheQFlex plugin attached; produce one FW checkpoint per sampling unit. `--sample-size N` sets N = number of sampling units in this sample | parallel-qemu + WormCacheQFlex |
| Parallelism prep | `partition`, `partition-cleanup`, `unpartition` | Split the per-sampling-unit checkpoints into `partition_count` chunks for parallel timing runs; emit per-partition run scripts. Not a sampling step — purely about scheduling work | (none) |
| Timing | `run-partition`, `run-single-partition`, `run-idx` | Detailed cycle-accurate simulation per sampling unit (each unit = detailed-warming prefix + measurement segment) | qemu (`vanilla-`) + middleware + flexus |
| Aggregate | `result` | Aggregate per-partition output into `core_info.csv`. Entry point: [result.py](result.py), driver in [commands/result.py](commands/result.py) | (none) |
| Multi-node | `multi` | Currently wraps a `gdb --args ./qemu-system-aarch64 ...` invocation ([commands/multinode.py](commands/multinode.py)) | parallel-qemu |

## Architecture

### Configuration: factory function as the single source of truth

The `ExperimentContext` Pydantic model in [commands/config.py:103](commands/config.py#L103) is the single object every command consumes. The factory `create_experiment_context` ([commands/config.py:467](commands/config.py#L467)) is the **single source of truth** for how to build one — its parameter list, defaults, and `Annotated[T, Field(description="...")]` metadata drive both the CLI flag set, the YAML schema, and the docs site simultaneously. Add a field to the factory and it shows up everywhere automatically.

Two paths exist for actually invoking the factory: a per-field CLI, and a YAML/DI graph. Both go through `data_class_wrap` ([typer_inputs/config_wrapper.py](typer_inputs/config_wrapper.py)), which inspects the runtime flags and env vars to decide which path applies (and lets them mix — see [precedence](#where-each-fields-value-comes-from-precedence) below):

```
./qflex <cmd>  [-c foo.yaml | $QFLEX_CONFIG=foo.yaml | --core-count 16 ...]
   │
   ▼  Typer parses the synthetic signature (factory params + injected --config/-c)
qflex (@data_class_wrap(create_experiment_context, name="experiment_context"))
   │
   ▼  data_class_wrap wrapper picks one path:
   │
   ├─[a]─ if --config or $QFLEX_CONFIG resolves to a path (flag wins):
   │        per-field flags the user explicitly passed are collected as
   │        component_overrides and merged on top of the YAML at the
   │        OmegaConf level. Flags omitted on the CLI keep their YAML
   │        values (or the factory default if also absent from YAML).
   │        ▼
   │     dep_injection/builder.py        build_experiment_context(path, component_overrides={name: cli_overrides})
   │        │
   │        ├─► dep_injection/config_loader.py
   │        │       load_config(path)        # OmegaConf + recursive extends:
   │        │       apply_cli_overrides(...) # OmegaConf dotlist merge (empty for now)
   │        │
   │        ▼
   │     dep_injection/di_loader.py      ConfigDrivenModule(cfg)
   │        │   two-pass walk of cfg.components → injector bindings
   │        ▼
   │     Injector([ConfigDrivenModule(cfg)]).get(ExperimentContext)
   │        │   resolves _target_: commands.config.create_experiment_context with YAML kwargs
   │        ▼
   │     ExperimentContext
   │
   └─[b]─ else: harvest non-None per-field flag values, call the factory directly.
            Factory-required params that weren't passed → friendly error pointing
            at --config / $QFLEX_CONFIG.
            ▼
         commands/config.py             create_experiment_context(**harvested)
```

Real example YAMLs live in [conf/](conf/). [conf/dc.yaml](conf/dc.yaml) is a single-node base; [conf/dc-node-0.yaml](conf/dc-node-0.yaml) extends it for a multi-node setup. Param names in YAML match the factory verbatim (`doubled_vcpu`, `is_parallel`, `is_consolidated`, `neighbor_node_list`), which is the same set of names the CLI uses.

#### How to invoke a pipeline command

Every pipeline command builds an `ExperimentContext` the same way. Pick whichever of these matches your situation:

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

Each command also takes its own command-specific flags (not part of `ExperimentContext`): e.g. `--sample-size` on `fw`, `--warming-ratio` / `--measurement-ratio` on the `run-*` commands, `--vanilla` on `boot` and `load`, `--partition-count` on `partition`. Those always come from the CLI.

#### Where each field's value comes from (precedence)

For any field of `ExperimentContext`, the final value is resolved in this order (highest priority first):

1. **Explicit CLI flag** (e.g. `--core-count 32`). A flag is "explicit" only when the user actually typed it — Typer's auto-rendered `[factory default: X]` is documentation, not a value that gets applied as an override.
2. **YAML value** under `components.experiment_context.<field>` in the file resolved via `-c <path>` or `$QFLEX_CONFIG` (flag wins over env).
3. **YAML extends chain.** When the YAML has `extends: <name>`, the parent doc loads first and the child merges over it. Recursive. Child keys win on conflict.
4. **Factory default** — the `= default` on `create_experiment_context`'s parameter. Visible in `./qflex <cmd> --help` as `[factory default: X]`.
5. **Required-with-no-source.** A factory-required param (no default) that comes from neither CLI nor YAML produces a `typer.BadParameter` listing every missing flag and pointing at `--config` / `$QFLEX_CONFIG`.

Resolution precedence for *which* YAML to load: explicit `-c <path>` > `$QFLEX_CONFIG` env var > no YAML (fall back to pure CLI). There is no implicit `./config.yaml` lookup.

#### YAML shape

The config file is a single OmegaConf document. Two top-level keys are recognized:

- `extends: <sibling-name-without-.yaml>` — optional. Loaded first, current doc merged on top. Recursive. Sibling lookup only (parent must live in the same directory).
- `components:` — required for anything to bind. A map of `<component-name>: { _target_: ..., ...params }` entries. The component-name is a free-form key used for diagnostics and `_deps_` references; the actual injection key is derived from the component's *return type* plus its `_name_`.

Reserved keys inside a component (everything else is passed as a constructor kwarg):

| Key | Purpose |
|---|---|
| `_target_` | **Required.** Fully-qualified callable, `pkg.module.Name`. Class → returns itself. Factory function → must have a `-> ReturnType` annotation (enforced by `get_return_type`). |
| `_scope_` | Optional. Only `singleton` is recognized. Any other value raises. |
| `_name_` | Optional. Named binding — lets two components produce the same return type. Internally implemented as a dynamic subclass `type(f"{cls.__name__}__{name}", (cls,), {})` so the injector treats the two as distinct types. |
| `_deps_` | Optional. Explicit wiring for non-primitive constructor params. Map of `param_name: {name: '<other-component-name>'}` for scalar deps, or `[{name: '...'}, {name: '...'}]` for `list[T]` deps. |

Example (see [conf/dc.yaml](conf/dc.yaml) for the real shape):

```yaml
extends: base                                  # bare name, NOT "base.yaml"
components:
  experiment_context:
    _target_: commands.config.create_experiment_context
    _scope_: singleton
    core_count: 16
    quantum_size: 1000
    workload_name: web_search
    primary_core_start: 0
    is_consolidated: false
    primary_ipc: 1.5
    population_seconds: 5
    host_name: zen3
    _deps_:
      # if create_experiment_context took a typed dep with multiple candidates,
      # you'd disambiguate it here:
      # some_param: { name: special_variant }
```

#### What gets resolved through the graph vs. passed as a scalar

Inside `ConfigDrivenModule.configure`:

1. Each component's params are partitioned. Anything in `PRIMITIVE_TYPES` (`int, str, float, bool, bytes, list, dict, tuple, set`) is left as a scalar passed straight to the target.
2. Typed deps are everything else — discovered by `find_typed_deps`, which inspects the target's signature + `get_type_hints`. It unwraps `Optional[T]` and `list[T]` (single-arg only).
3. Resolution rules per typed dep:
   - In `_deps_`: look up `(dep_type, _deps_[param].name)` in `name_lookup`. List-typed: same but per element.
   - Not in `_deps_` and `Optional[T]`: skip with `_OMIT` so the constructor default applies.
   - Not in `_deps_` and required `list[T]`: **error** — required lists must be explicit.
   - Not in `_deps_` and required scalar `T`: bind to the unnamed `T` provider (i.e. assumes one component without `_name_` produces it).
4. Provider gets `(target, scalar_params, resolved_deps)` and at `.get()` time calls `target(**kwargs)`, recursively pulling each typed dep from the injector.

#### Practical implications

- Adding a field to `ExperimentContext` (or any `_target_`-pointed class): edit the factory's parameter list (and the Pydantic model if the field is stored on `ExperimentContext` itself). The CLI flag, `--help` text, and YAML key all come along for free as long as you put `Annotated[T, Field(description="...")] = default` on the factory param. No `data_class_wrap` change needed.
- Two providers for the same base type *only* coexist when at least one has `_name_`. Duplicate `(return_type, _name_)` raises at module-configure time, which is much earlier than `injector.get`.
- Factory functions (like `create_experiment_context`) are perfectly supported, but they **must** annotate `-> ExperimentContext`. Without it, `get_return_type` raises with a clear message — don't bypass it.
- Two override mechanisms exist on the YAML path inside [build_experiment_context](dep_injection/builder.py):
  - `overrides: list[str]` — OmegaConf dotlists, applied via `apply_cli_overrides`. Library-only API; no CLI flag exposes it currently. Override anything addressable: `components.experiment_context.core_count=16`, `components.foo._name_=bar`, etc.
  - `component_overrides: dict[str, dict]` — structured `{component_name: {field: value}}` merged via `OmegaConf.merge` after `apply_cli_overrides`. This is what `data_class_wrap` uses to splice per-flag CLI overrides onto a `-c` invocation; values pass through OmegaConf typed.

### CLI ↔ factory glue: `data_class_wrap`

[typer_inputs/config_wrapper.py](typer_inputs/config_wrapper.py) defines `@data_class_wrap(target, *, name)`, used on every pipeline command in [qflex](qflex) as `@data_class_wrap(create_experiment_context, name="experiment_context")`. At decorator time it:

1. Reads `inspect.signature(target).parameters`.
2. For each parameter, builds a synthetic Typer param. The annotation is widened to `Annotated[Optional[T], typer.Option(help=..., show_default=False)]` and the default is set to `None` — **regardless** of whether the factory's param was required or had a default. The factory's actual default (if any) is preserved separately by being formatted into the help text as `[factory default: X]`.
3. Appends one extra synthetic param: `--config / -c` (`Optional[str] = None`).
4. Splices the resulting parameter list into the wrapped Typer command's signature, in addition to whatever per-command flags the command body declares.

The `Optional[T] = None` widening is what makes `flag-overrides-yaml` work: when the user omits a flag, the kwarg arrives as `None`; when they pass a flag (even `--no-foo` to set a bool to `False`), the kwarg arrives as the typed value. Without this, we couldn't distinguish "user explicitly passed `--core-count -1`" from "user didn't pass it; Typer filled in the factory's `-1`."

At call time the wrapper harvests every per-field flag whose value is non-`None` and non-empty-list (Click renders an unset multi-value option as `[]`, not `None`) into a `cli_overrides` dict, then picks between two paths (see the diagram in [Configuration](#configuration-factory-function-as-the-single-source-of-truth)):

- `--config <path>` was passed, or `$QFLEX_CONFIG` is set in the environment (flag wins): import `dep_injection.builder.build_experiment_context` lazily and call it with `component_overrides={name: cli_overrides}`. Inside the builder these are merged on top of the YAML at the OmegaConf level — so per-field flags override YAML for the keys the user passed, while everything else takes the YAML value (or its inherited value, or the factory default).
- Otherwise: call `target(**cli_overrides)`. Factory defaults apply for any kwarg the user didn't pass (we never put `None`s into `cli_overrides` in the first place, so the factory's signature defaults kick in for those). If any factory-required field is still missing, raise a `typer.BadParameter` listing the missing flag names and pointing at `--config` / `$QFLEX_CONFIG` as alternatives.

Either way the result is injected as `kwargs[name]`, so each `@app.command()` body still receives an `experiment_context: ExperimentContext` kwarg directly.

The `build_experiment_context` import is deferred to call time so that environments without `injector`/`omegaconf` (e.g. host machines that only need to render `--help` for `mkdocs-click`) can still load `qflex` cleanly.

If you add a new pipeline command, follow the existing pattern — don't try to take an `ExperimentContext` directly as a Typer parameter, it won't work. If you add a new field, add it to `create_experiment_context`'s parameter list with `Annotated[T, Field(description="...")] = default`; both the CLI surface and the YAML schema pick it up automatically.

`mkdocs-click` ([docs_shim/qflex.py](docs_shim/qflex.py), [mk_docs/reference/qflex.md](mk_docs/reference/qflex.md)) walks the resulting Typer command tree to render the CLI reference page, so factory-side `Field(description=...)` text ends up published on the docs site too.

### Execution model

Every command class in [commands/](commands/) ultimately ends up emitting a bash command that drives one of the submodules (a QEMU binary, a Flexus run script, etc.). The shared mechanism is [commands/executer.py](commands/executer.py): it defines `Executor` (abstract), `SequentialGroupExecutor`, and `ParallelExecutor`. Each command returns a string (or list of strings joined by ` && `) from `cmd()` and the base `execute()` runs it via `subprocess.run(shell=True)`. `ParallelExecutor` uses `multiprocessing.Pool` and **terminates the pool on first failure**. `run_in_background=True` is currently `NotImplementedError` — don't pass it.

Adding a new pipeline step almost always means: add a class that subclasses `Executor`, takes `ExperimentContext` in its constructor, builds the shell invocation in `cmd()`, and is exposed by a thin `@app.command()` wrapper in [qflex](qflex).

### Per-experiment filesystem layout

`ExperimentContext.set_up_folders()` ([commands/config.py:245](commands/config.py#L245)) creates a self-contained tree under `<mounting_folder>/experiments/<experiment_name>[-<timestamp>]/` with subdirs `bin/ cfg/ flags/ lib/ run/ scripts/ images/`. It also:

- Symlinks/copies QEMU binaries from `parallel-qemu-saved/build/qemu-system-aarch64` (becomes `qemu-system-aarch64` — the FW/emulation binary) and `qemu-saved/build/qemu-system-aarch64` (becomes `vanilla-qemu-system-aarch64` — the timing binary) into `run/`.
- Hard-codes the kraken libs path to `/home/dev/qflex/kraken_out/lib{knotty,semi}kraken.so` ([commands/config.py:323](commands/config.py#L323)) — this matches the in-container layout but will fail on a host build.
- Copies [partition.py](partition.py) and [result.py](result.py) into the experiment folder. These are run from inside the experiment dir, not the repo root.

If `keep_experiment_unique=True` (default), a `-YYYYMMDD-HHMMSS` suffix is appended; otherwise repeated runs reuse the directory.

### Multi-node specifics

A node has `node_number >= 0` (0 = master); single-node uses `-1`. Connectivity is described by three parallel lists on `ExperimentContext`: `neighbor_node_list`, `latencies_ns_list`, `syncs_list` (`'true'`/`'false'` strings, not bools), plus `pdes_net_devs` (`'e1000'` or `'virtio-net-pci'`). They must all be the same length and `node_number` must be set when any are non-empty (asserted in `create_experiment_context`).

Inter-node traffic flows over POSIX shared memory (`/dev/shm/pdes_<from>_to_<to>...`), wired in by `ExperimentContext.setup_nic_args()`. The master node is responsible for clearing stale shm files before launch — non-master nodes do not. After a crashed run, [clean_up.sh](clean_up.sh) removes `/dev/shm/pdes*` and kills lingering qemu processes; use it before retrying.

Each node also gets its own copy of the disk image (suffix `-node<N>`) via `copy_image_for_node()`.

### Code generation (Jinja templates)

[templates/](templates/) holds the Flexus/QEMU runtime config templates: `timing.cfg.j2`, `flexus_configuration.json.j2`, `run_flexus.sh.j2`, `parameter.rs.j2`. Loaders in [commands/jinja_loaders/](commands/jinja_loaders/) populate them from `ExperimentContext` and write into the experiment's `cfg/`, `scripts/`, `flags/` dirs. When a Flexus config field changes, you usually need to touch *both* the Pydantic model and the template.

### Hosts and workloads

[commands/host/](commands/host/) — fixed registry of host topologies (`zen3`, `saphire`) selected by name; controls core-affinity sequence used in `core_info.csv`.
[commands/workload/](commands/workload/) — workload definitions including `IPC_info` (primary/secondary/phantom IPC, machine freq) and `core_range` (consolidation layout). `is_consolidated` toggles single-IPC vs split primary/secondary cores.

## Conventions worth knowing

- The dev image expects everything at `/home/dev/qflex/...`. Hard-coded paths exist (notably the `kraken_out/` lookup); when running outside the container, expect path-based failures.
- The container uses `--shm-size=128g` and `--pid=host`. Multi-node simulations need real shared memory — don't shrink it.
- `./qflex --show-completion >> ~/.bashrc && source ~/.bashrc` is the documented way to get autocompletion (already wired into the Dockerfile via `completion_docker.txt`).
- Version is sourced from [VERSION](VERSION) and managed by `bump-my-version` ([.bumpversion.toml](.bumpversion.toml)).
- There is no `.cursor/rules`, `.cursorrules`, or `.github/copilot-instructions.md`. The only AI-relevant instruction file is this one.
