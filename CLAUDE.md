# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

QFlex is a computer-architecture simulation toolkit combining QEMU-based functional
warming (BXKraken) with two timing-simulation backends: Flexus and gem5. This
top-level `qflex` repository is the primary user-facing entrypoint; it orchestrates
several submodules rather than containing the simulators itself:

- `parallel-qemu/` — the fast QEMU used for emulation and functional warming
  (built without `--enable-libqflex`; unprefixed `qemu-system-aarch64` in an
  experiment's `run/`). Loads the `WormCacheQFlex` plugin during functional warming.
- `WormCacheQFlex/` — Rust plugin loaded by `parallel-qemu` **only during functional
  warming**, modeling long-term microarchitectural state (TLBs, branch predictor,
  caches). Not used during timing simulation — Flexus has its own models there.
  Built **per-experiment** (not at repo-build time; see below), from the repo copy
  unless `--refresh-wormcache` is passed.
- `qemu/` — the *timing* QEMU, built with `--enable-libqflex --enable-snapvm-external`
  (`vanilla-qemu-system-aarch64` in `run/`; "vanilla" = non-parallel, not unpatched).
  Its job is committing instructions in lockstep with the Flexus timing model.
  Has its own submodule, `qemu/middleware`, which brokers the QEMU↔Flexus IPC.
- `flexus/` — the timing model itself (Conan + CMake; build targets `knottykraken`,
  `semikraken`). Connects to the timing `qemu/` through the middleware shim.
- `QPoints/` — bridge layer to gem5: stages gem5-consumable checkpoints, prepares
  microarchitectural restore artifacts, hosts gem5 integration configs and tracked
  validation records. Despite the name, it's no longer the primary conversion
  path — it's an implementation detail behind `qflex qpoints ...` / `qflex run_sample`.
  Its own copy of gem5 lives at `QPoints/gem5/`.

Mental model for the Flexus-timing path: population → samples → sampling units →
(detailed warming + measurement) per unit. Long-term µarch state is carried in via
WormCacheQFlex during functional warming; short-term state is warmed per-unit in the
timing phase (see terminology below). The gem5 path reuses the same functional-warming
and snapshot/checkpoint machinery but times via `QPoints`/gem5 instead of Flexus.

The maintained user-facing docs (read these before making workflow changes) are:
- `docs/gem5_conversion/qflex_cli_guide.md` — full CLI reference and the config
  boundary between args-file / sim-config / direct CLI flags
- `docs/gem5_conversion/project_progress.md` — what's implemented, what's
  validated, known caveats and unfinished work (STLB restore missing, kernel ELF
  auto-extraction incomplete, `create-base-image` unverified, etc.)

Each vendored submodule that has QFlex-specific behavior (not just upstream code)
carries its own `CLAUDE.md` documenting that delta — read the relevant one before
touching that submodule's internals, rather than guessing from the outside:
`parallel-qemu/CLAUDE.md`, `WormCacheQFlex/CLAUDE.md`, `qemu/CLAUDE.md`,
`qemu/middleware/CLAUDE.md`, `flexus/CLAUDE.md`.

## Where to look first

The five areas that explain the whole tool:
1. `qflex` (repo root) — Typer CLI, one subcommand per pipeline stage.
2. `dep` — Typer CLI for building/starting the Docker dev image.
3. `typer_inputs/` — adapter layer turning CLI flags into a populated `ExperimentContext`.
4. `commands/` — the actual logic; every CLI subcommand resolves to an `Executor`
   subclass here that takes an `ExperimentContext` and builds/runs a shell command.
   `commands/config.py` defines `ExperimentContext` and is the central data model.
5. `build` + `Dockerfile` — build the submodules and stage their artifacts into
   `./kraken_out/`, `./qemu-saved/`, `./parallel-qemu-saved/` (see Container build below).

If an investigation doesn't touch one of these five, it's probably vendored or
auxiliary code.

## Commands

### CLI entrypoint

`./qflex` (typer-based) is the user-facing CLI. Commands accept a shared
`ExperimentContext` (`--core-count`, `--memory-gb`, `--image-folder`, etc.) which
can be supplied via `--args-file <path>` (a flat file of CLI args, `#` comments
supported, expanded before dispatch — see `_expand_args_files` in `./qflex`).
Typical invocation pattern:

```bash
xargs -a ./qflex.args -- ./qflex <command> [flags]
```

Workflow stage order: `create-base-image` (unverified/legacy) → `boot` → `load`
(interactive, user drives workload state and creates their own snapshot) →
`initialize` (creates `init_warmed`) → `fw` (functional warming, produces
`snapshot_N`) → timing (`run_sample`, or `qpoints convert-single` +
`qpoints run-gem5` for one-off gem5 work). `partition`/`run_partition`/`result`
are the legacy Flexus-only path, superseded by `run_sample --timing-engine flexus`.

Kernel repair when boot's automatic ELF capture is incomplete:
```bash
xargs -a ./qflex.args -- ./qflex kernel adopt --kernel <VMLINUX_ELF>
```

### Building components

```bash
./build <sim>              # sim ∈ {knottykraken,semikraken,cq,qemu,q,docker}
./build cq release          # build libqflex-enabled QEMU (release/debug/relwithdebinfo)
./build docker              # docker buildx build (see Dockerfile)
```
`build` operates on `flexus/` and `qemu/` in place via Conan/CMake/Ninja or `make`.
WormCacheQFlex is **not** built by `build` — it's compiled per-experiment (see
`ExperimentContext.get_wormcache_build_commands` in `commands/config.py`), from the
repo copy unless `--refresh-wormcache` is passed.

```bash
./dep build-docker [--debug] [--push]     # wraps ./build docker
./dep start-docker --mounting-folder <dir> [--debug] [--start-directory <dir>]
```
Before running `./dep build-docker`, ask the user whether they want `--debug` or
release — it changes the image tag and build type, and is not something to guess.
Never pass `--push` (publishes to `docker.io/akrishnaams/...`) without being asked.

`./runq` is a separate, lower-level launcher: it takes a QEMU config file plus
`+arg`/`-arg` overrides and directly execs a `qemu-aarch64` binary. The `qflex`
pipeline doesn't use it — only reach for it when poking at QEMU directly.

### Tests

Two independent pytest suites exist — the outer wrapper contract, and QPoints/gem5
internals — each with unit tests (no external assets needed) and integration tests
(need a local `testenv.json` pointing at real image/checkpoint assets; they skip
cleanly if unconfigured).

```bash
# outer qflex CLI wrapper tests
cd /path/to/qflex && python -m pytest -q
python -m pytest -q -m integration
python -m pytest -q tests/test_qpoints_integration.py::test_qflex_run_gem5_ruby_data_trace_cache_dump_and_sim_config

# QPoints / gem5 integration tests (use the project venv)
cd QPoints && /home/dev/qflex/.venv/bin/python -m pytest -q
/home/dev/qflex/.venv/bin/python -m pytest -q -m integration
/home/dev/qflex/.venv/bin/python -m pytest -q tests/test_integration_run_gem5.py::test_run_gem5_classic_data_trace
```

Test env file resolution order: `QFLEX_TEST_CONFIG` env var → repo-local
`.testenv.json` → `~/.config/qflex/testenv.json`. Set `QFLEX_TEST_KEEP_ARTIFACTS=1`
(or `"keep_artifacts": true` in the testenv file) to retain artifacts for debugging.
Integration tests default to `snapshot_1` so `snapshot_0` stays untouched for
manual work. See `tests/README.md` and `QPoints/tests/README.md` for what each
test file covers.

### Docs site

```bash
make install-dev-requirements   # requirements.txt + requirements.docs.txt + uv/bump-my-version
make serve-docs                 # mkdocs serve -a 0.0.0.0:8888
make build-docs                 # mkdocs build --clean
```

## Architecture notes

### Config boundary: args-file vs sim-config vs direct CLI

This distinction is load-bearing and easy to blur — respect it when adding flags:

1. **Args file** (`--args-file`, expanded by `./qflex` at dispatch): persistent
   *experiment* identity — core count, memory, workload/host/image names, network
   mode, quantum size. `loadvm_name` technically lives here but is usually
   stage-specific in practice — don't let a stale `--loadvm-name` leak across
   `load`/`initialize`/etc. `--emit-gem` (gem5 mode) also lives here: it is
   compiled into WormCacheQFlex as `GEM5_COMPATIBILITY` when `initialize`
   generates `cfg/parameter.rs`.
2. **Sim-config** (`--sim-config <path>`, files under `QPoints/configs/`): gem5
   *model/runtime* parameters — cache geometry, mesh topology, MMU/TLB contract,
   core widths. Conversion and timing must use the **same** sim-config or the
   staged checkpoint won't match the runtime contract.
3. **Direct CLI flags**: invocation-scoped behavior that shouldn't silently
   become persistent context (`--first`/`--last`, warmup/measurement cycles,
   tracing flags, `--cleanup-conversion-artifacts`).

Naming quirk to be aware of: conversion commands use `--ruby-protocol
{mesi_two_level,moesi_cmp_directory}`; timing commands use `--timing-ruby` /
`--timing-ruby-moesi` instead. These select the same underlying protocol family
but are different flags at different CLI layers — a historical artifact, not a bug.

### Statistical sampling terminology (Flexus-timing path)

- **population**: the wall-clock duration of guest workload that is functionally
  warmed and sampled from, set per-workload via `population_seconds`
  (`commands/workload/`)
- **sample**: one round of functional warming + sampling over the population,
  triggered by `qflex fw --sample-size N`; a run can take multiple samples if the
  first sample's confidence interval is too wide
- **sampling unit**: an individual measurement window inside a sample (whether a
  sample's count of sampling units is "enough" is a `result.py` /
  `commands/result.py` decision; `analyze_sampling_result.py` is the related
  analysis helper)
- **per-unit warming vs. measurement**: each sampling unit's timing-phase run is
  itself split into a detailed-warming prefix (Flexus warms shorter-term µarch
  state — pipeline, store buffers — that WormCacheQFlex doesn't capture during FW)
  followed by the measurement segment that contributes to reported numbers; the
  split is controlled by `--warming-ratio`/`--measurement-ratio` on `run_partition`

### Checkpoint/snapshot terminology (used precisely throughout the codebase and docs)

- **snapshot**: a QEMU VM-state snapshot (e.g. `snapshot_0`)
- **checkpoint**: a snapshot plus recorded microarchitectural state from parallel
  functional warming — passive state
- **sampling unit**: a checkpoint actively simulated for a timing window
  (warmup + measurement)
- **sample**: a collection of sampling units satisfying the statistical sampling
  methodology for a workload

Conversion has two layers: architectural (disk/memory/register state) and
microarchitectural (BTB, TAGE, L1I/L1D, LLC, directory/private-state families,
L1 ITLB/DTLB — STLB is not yet supported). `MOESI_CMP_directory` is the current
target protocol (non-inclusive, matching BXKraken-origin checkpoints);
`MESI_Two_Level` is inclusive and kept only for bring-up/legacy comparison.
Restored LLC lines are normalized to clean state with data read from memory
rather than preserving dirty state — this is intentional, not a limitation.

### Command dispatch pattern

`./qflex` (typer app in the repo root) wires CLI subcommands to executor classes
in `commands/*.py`. Each executor subclasses `Executor` (`commands/executer.py`),
implementing `cmd() -> str | list[str]`; `execute()` runs the joined commands via
`subprocess` (shell) either to stdio or captured. `ExperimentContext`
(`commands/config.py`, a pydantic `BaseModel`) is the shared config object most
commands take; it owns experiment folder layout (`set_up_folders`), machine
config JSON generation/symlinking, image setup, and WormCache build-command
generation. `qflex qpoints ...` subcommands dispatch into `commands/qpoints.py`,
which shells out to QPoints' own `run_gem5.sh` / conversion scripts.

### Per-experiment folder layout

Each experiment gets an isolated folder (`<mounting-folder>/experiments/<name>/`)
with `bin/cfg/flags/kernel/lib/run/scripts/images` subfolders, created by
`ExperimentContext.set_up_folders()`. Generated config (`cfg/parameter.rs`,
`cfg/flexus_configuration.json`, `cfg/timing.cfg`, `scripts/run_flexus.sh`) is
templated via `commands/jinja_loaders/*` from `templates/*.j2`, and is
regenerated by `initialize` unless `--skip-generate-cfg` is passed. WormCacheQFlex
is compiled per-experiment into `lib/WormCacheQFlex` from the repo source (or
freshly recopied with `--refresh-wormcache`).

### Hosts and workloads

`commands/host/` is a fixed registry of host topologies selected by name
(`zen3`/`zen3_smt`, `saphire`), each defining core count, cores-per-NUMA, and a
core-sequence string (`"0-127"`-style ranges); `Host.get_core_sequence_as_list()`
expands it. This drives core affinity written into `core_info.csv`.
`commands/workload/` defines workloads (`IPCInfo`: primary/secondary/phantom IPC,
machine frequency; `CoreRange`: primary/secondary core split) — `is_consolidated`
toggles a single shared IPC vs. a split primary/secondary-core layout.

### Container build (Dockerfile)

Single all-in-one image, structured as independent BuildKit stages so changing
one component doesn't invalidate others' cache, and independent stages build in
parallel automatically: `toolchain` (base deps) → `flexus-qemu-build` (Flexus +
libqflex QEMU, bind-mounted sources) → `parallel-qemu-build` (bind-mounted) →
`qpoints-build` (QPoints + gem5, COPY'd since the runtime needs the full tree) →
`runtime` (final assembly, `FROM toolchain`). Bind-mount stages persist only
their build outputs (e.g. `kraken_out/`, `qemu-saved/`), not the mounted source
tree, into the final image. Container/Dockerfile organization is explicitly
called out in `docs/gem5_conversion/project_progress.md` as still needing
consolidation — expect friction here.
