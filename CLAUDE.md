# CLAUDE.md

@MULTI_NODE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Coding style for this repo (read first)

Keep edits minimal. The shortest correct version wins.

- **No defensive ceremony.** No `trap`, no try/except, no fallback constants, no error handling for failure modes that can't actually happen here. If you reach for one, you should be able to name the concrete failure it catches.
- **Don't re-document `--help`.** Shell wrappers and small utilities should not carry multi-paragraph headers explaining what `./qflex <cmd>` already prints. A `set -euo pipefail` + the command itself is usually enough.
- **Trust simple primitives.** `pushd`/`popd`, `set -euo pipefail`, `exec`, plain bash conditionals — use them as-is, don't wrap them in safety nets.
- **No hardcoded fallback for values the host already provides.** Read `/run/systemd/resolve/resolv.conf`, the host's `uname -r`, etc. directly, instead of baking site-specific constants into the source.
- **No `sleep N` as a timeout.** Polling loops with a `sleep N` between checks are fine, but capping them with a count or wall-clock budget so they "fail fast" duplicates whatever timeout already wraps the operation (test timeout, `_exec_in_container` timeout, expect `set timeout`). Strip the cap; let the outer timeout catch hangs. Exception: in **expect scripts**, Tcl `while`/`sleep` loops are NOT covered by the script's `set timeout` — that timeout only fires inside `expect` blocks. A polling loop in expect that races a resource that can transition from "starting" to "dead" (e.g. telnet to a qemu monitor port) will spin forever once dead. Such loops need their own wall-clock deadline (`expr {[clock seconds] + N}`) — the only place a budget is justified, because the outer timeout genuinely doesn't apply. Use these named budgets in every expect script, no magic numbers: `set BUDGET_INITIAL_S 10` (first connect to qemu — listener may not be up yet), `set BUDGET_DEFAULT_S 5` (anything else — qemu was alive, anything past 5s is dead), `set BUDGET_CHECKPOINTING_S 1800` (savevm/loadvm or any drain-coordinated op — large state can legitimately take many minutes).
- **Stop polling once the script's job is done.** When the expect script has finished its real work (captured the file, snapshotted, etc.), every remaining cleanup operation is best-effort. If `connect_telnet` fails reaching the monitor port for shutdown, the image already exited — return immediately, don't keep retrying. Wrap the cleanup connect in `if {[catch {connect_telnet …} sid]} { return }`. The shell that's `wait`-ing on the expect should never sit there for additional minutes after the captures are written.
- **Before moving / generalising a piece of behaviour, grep for existing callers.** When you take something a few specific files do (e.g. each call site appending `quantum_args()` to its qemu command line) and push it into a shared helper or base method, do a quick `grep -rn` for the symbol or string first. If only a few files match, open them and check whether they already do the thing — otherwise you'll add it twice. Cheap to check, expensive to debug.
- **Single Responsibility — one place owns each cross-cutting concern.** When you spot the same logic in N places (qemu cmdline assembly, telnet retry, sentinel coordination, …), don't fix the symptom in just one of them. Pick the file that *should* own it (`commands/qemu.py` for qemu args, `commands/executer.py` for dispatch, etc.), put the logic there, and have callers parameterise via constructor / function args rather than reaching into helpers themselves. The recent example: time-discipline (`-quantum` / `-icount`) belongs to `commands/qemu.py` — `boot.py`, `load.py`, `fw.py`, `init_warm.py`, and `multinode.py` should not each manually concatenate `parser.quantum_args()` onto their cmdline. They call one method; the parser decides. SOLID's S — applied as a guideline, not a religion.
- **Ask before touching core code.** Don't change cross-cutting machinery (executor dispatch, PDES wire, qemu base args, gdb wrap, signal handling, sentinel scheme) on your own to work around a symptom. Surface the symptom + your proposed change to the user first; let them say yes. A "small" tweak in shared code can ripple across every phase and become very expensive to debug.
- **Comments are rare.** No multi-paragraph comment blocks; no "Poll telnet until …" + "Each spawn fails fast …" + "The outer timeout caps …" stacked on one helper. One short line at most, only when the *why* isn't obvious from the code. Never repeat the same explanation across files. The PR description is where prose belongs, not the source.
- **DRY > WET, but rule of three.** Don't repeat yourself: when the same logic appears in two files (e.g. `gdb -ex run --args …` across boot/load/init_warm/fw), pull it into one helper that the call sites parameterise. Conversely, don't pre-abstract on the first occurrence — wait until you have at least two real call sites before factoring out, otherwise the abstraction shape is a guess.
- **Run things the way the user would.** When invoking anything that already has a make target / `./dep` subcommand / `./qflex` subcommand, use that surface as-is. Don't shell out to `pytest …` when `make test-real-one TEST=…` exists; don't issue raw `docker run`/`docker exec` when `./dep start-docker`/`./dep exec` exists. No extra files written, no debug-only flags, no env shims that wouldn't be there for the user. If the surface is missing a flag you need, add the flag to the surface — don't bypass it.
- **Real-run test settings live in YAML, not in python.** Every knob a real-run test depends on (experiment_name, interaction_script, loadvm_name, use_gdb, …) belongs in a per-test YAML under [tests/realrun/](tests/realrun/), not as a CLI override in the test code. The python test should be reduced to `_exec_in_container("./qflex <phase> -c tests/realrun/<x>.yaml")` plus the assertions on the captured output. Test-specific expect scripts (`*.exp`) live next to their YAMLs in [tests/realrun/](tests/realrun/) so the fixture file set is self-contained. The YAMLs `extends: ../../conf/DC/<base>` to inherit production defaults — change the production base, every test follows. Knobs that aren't already on `ExperimentContext` (`use_gdb` was the recent one) need to be added there so YAML can set them.
- **Never write to `/tmp`, `/`, `~/.cache`, or any boot-disk path. Period.** Every output (redirects, log captures, intermediate artifacts, pytest output, build logs, debug dumps, monitor scripts' tracking files, anything) goes under the user's mount: `<mounting_folder>/` (e.g. `/mnt/sdc/data-caching-1c/experiments/<experiment_name>/`). The boot disk is small and shared with the OS; filling it kills bash with `ENOSPC` and the user has to manually free space before anything can move. **This rule extends to Claude-side tooling:** the Bash tool's `run_in_background: true` and the Monitor tool both auto-write their output capture to `/tmp/claude-291753/...` — using them on long-running commands ALSO fills the boot disk. For long real-run tests use **foreground** Bash with a sufficient `timeout` (output streams back as the tool result, no disk transit), and explicitly redirect any subcommand output that you DO need persisted to `<mounting_folder>/...`. If a tool insists on `~/.cache`, override via env (e.g. `XDG_CACHE_HOME=<mount>/cache`).
- **Before creating any new log file: check if one already exists.** Most things you'd want to capture are already on disk somewhere under the experiment folder — `Load.log`, `Load.err`, `Boot.log`, `Boot.err`, `expect_log.txt`, `ls_after_*.txt` are all written by the executor / expect scripts already. Read them with `Read` or `tail`/`grep`. Don't redirect commands like `./qflex load > /tmp/foo` to inspect output you can already see in `<mounting>/experiments/<exp>/Load.log`. Same applies during debugging — your first move should be to look at the existing log, not to rerun the command with output redirected somewhere new.
- **Poll long-running tests, don't wait passively.** When you launch a real-run test in the background and the user-visible output suggests the work is done, do NOT sit on the runtime's "command completed" notification — pytest's session-finalize / fixture-teardown can hang for many minutes after PASSED prints if a peer process is stuck in PDES sync, and the user shouldn't have to point that out. While the background task is running, periodically (every ~5 seconds) `tail` the output file plus check `ps -ef | grep qemu-system` and `ls /dev/shm/pdes*`. If the captures are written, the assertions printed, and qemu is still chewing CPU with no progress in the log, that's stuck — diagnose immediately rather than waiting another 30 minutes for the outer timeout. The polling lives on the Claude side (Bash + Read tool calls); never bake it into the python test code.

These rules apply to both Python and shell. If a defensive construct doesn't catch a real failure mode the user hits today, drop it.

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
5. [Makefile](Makefile) — drives Conan/Ninja builds of the submodules and stages their artifacts into `./parallel-qemu-saved/`, `./qemu-saved/`, and `./kraken_out/` (each `make <name>-build` target ends with `rm -rf <name>-saved && cp -r <name>/build <name>-saved/build`). `set_up_folders()` then `cp -u`'s those staged binaries into each experiment's `run/` dir. The image build runs `make qemu-build` + `make parallel-qemu-build` in both [Dockerfile.qemu.debug](Dockerfile.qemu.debug) and [Dockerfile.qemu.release](Dockerfile.qemu.release), so the `-saved/` dirs are present in every variant (debug, release, base, +worm); the WormCacheQFlex layer inherits them and only adds compatibility symlinks. Each qemu Dockerfile asserts the binaries actually landed (`RUN test -x …-saved/build/qemu-system-aarch64`) so a silently-failed compile fails the image build instead of leaking through.

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

A pytest suite lives under [tests/](tests/), one file per pipeline component (`test_boot.py`, `test_load.py`, …, `test_run_partition.py`). Default `make test` ([Makefile](Makefile)) runs only the dry-run-based tests — fast and hermetic. A separate set of real-run files ([tests/test_real_runs.py](tests/test_real_runs.py), [tests/test_real_runs_savevm.py](tests/test_real_runs_savevm.py), [tests/test_real_runs_docker_image.py](tests/test_real_runs_docker_image.py)) drive `./dep start-docker --background` + `./dep exec` to actually run qflex inside the dev container; gated behind `QFLEX_REAL_RUN_TESTS=1` so default `make test` skips them. To run a single real-mode test or file, use `make test-real-one TEST=tests/test_real_runs_docker_image.py[::test_name]`. See the `testing` and `dep` / `run-in-dev-container` skills for the conventions used there (capture stdout, parse `[py-wait]` / `[py-touch]` / `[bash]` / `[tmux]` markers, master-first invariants, and the start-bg → exec → stop-bg session pattern).

**ALWAYS ASK BEFORE TRIGGERING REAL-RUN TESTS.** Anything under `make test-real-one` / `QFLEX_REAL_RUN_TESTS=1` boots actual QEMU under docker — minutes per test, mutates the host's `/dev/shm`, writes large qcow2 deltas, may leave a `qflex_test` container running, and can collide with other docker work the user is doing. Treat them as non-reversible side-effecting operations: confirm with the user before running, then run **one test at a time** so progress is visible and a hang is easy to abort. The default `make test` (dry-run, hermetic) can always be run without asking.

**TESTS MUST EXERCISE THE CODE UNDER TEST.** Never invoke binaries (qemu, flexus, kraken libs) or other artifacts directly from a test to assert behaviour or "probe" capabilities — that duplicates logic that already lives in the CLI / [commands/](commands/) classes and gives a separate, can-disagree path. Real-run tests drive `./qflex <subcommand>` (or `./dep exec ...`) so they walk the same `data_class_wrap → create_experiment_context → Executor.execute → cmd()` pipeline production does. Dry-run / unit tests import the command class (`Boot`, `RunIdxCommand`, …) or factory directly. If a real-run test fails because a binary is stale or missing, that's the *correct* failure signal — surface the actual error from the real code path, don't paper over it with a hand-rolled check. Same applies to fixtures: a fixture that subprocess-runs a binary to gate skip behaviour is a smell; gate on env / docker availability / repo state instead.

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

Every command class in [commands/](commands/) ultimately ends up emitting a bash command that drives one of the submodules (a QEMU binary, a Flexus run script, etc.). The shared mechanism is [commands/executer.py](commands/executer.py): it defines `Executor` (abstract), `SimpleCMDExecutor`, and `SequentialGroupExecutor`. Each command returns a string (or list of strings joined by ` && `) from `cmd()` and the base `execute()` runs it via `subprocess.run(shell=True)`. `run_in_background=True` is currently `NotImplementedError` — don't pass it.

`Executor.execute()` dispatches on `ExperimentContext.has_sub_experiments()`: when the context carries a list of `sub_experiments` (multi-experiment / multi-node), the base spawns one `multiprocessing.Process` per sub, mutates `self.experiment_context = sub` in the child process, and re-enters `execute()` recursively. This is the single mechanism for every parallelism axis (multi-node × per-partition × …) — the legacy `ParallelExecutor` class was removed in favour of this. Per-leaf ordering is enforced by Python-side sentinel files (`<group>/.sentinels/<phase>_part<P>_idx<I>_node<N>.{started,done}`) plus an `ExperimentContext.wait_for_nodes: list[int]` field that lists upstream nodes to block on. See the `executor` and `multi-node` skills for the full mechanics.

Adding a new pipeline step almost always means: add a class that subclasses `Executor`, takes `ExperimentContext` in its constructor, builds the shell invocation in `cmd()` from the **current** `self.experiment_context` (no caching of context-derived state in `__init__` — multi-experiment dispatch mutates the context per sub), and is exposed by a thin `@app.command()` wrapper in [qflex](qflex).

### Per-experiment filesystem layout

`ExperimentContext.set_up_folders()` ([commands/config.py:245](commands/config.py#L245)) creates a self-contained tree under `<mounting_folder>/experiments/<experiment_name>[-<timestamp>]/` with subdirs `bin/ cfg/ flags/ lib/ run/ scripts/ images/`. **`set_up_folders()` (and `setup_nic_args()`) is called by the executor right before the leaf bash runs** — via `ExperimentContext.prepare_for_execution()` from `Executor._execute_leaf` — **not** by the factory. `create_experiment_context` is pure: constructing the YAML/DI graph builds every sub-experiment's context object up front but doesn't materialise any folder until that leaf actually runs. It also:

- Copies QEMU binaries from `parallel-qemu-saved/build/qemu-system-aarch64` (→ `run/qemu-system-aarch64`, the FW/emulation binary) and `qemu-saved/build/qemu-system-aarch64` (→ `run/vanilla-qemu-system-aarch64`, the timing binary). The `-saved/` dirs are populated by the Makefile (`make parallel-qemu-build` / `make qemu-build` end with `rm -rf <name>-saved && cp -r <name>/build <name>-saved/build`) and baked into the docker image during build. **If you bump parallel-qemu / qemu source you must rebuild the docker image (`./dep build-docker --debug` / `--release`, optionally with `--worm`) — the `-saved/` dirs are docker-image layers, not host-mounted, so a host-side rebuild won't reach them.** The qemu Dockerfiles assert the binaries actually landed so a broken build fails the image build, not the runtime.
- Hard-codes the kraken libs path to `/home/dev/qflex/kraken_out/lib{knotty,semi}kraken.so` ([commands/config.py:323](commands/config.py#L323)) — this matches the in-container layout but will fail on a host build.
- Copies [partition.py](partition.py) and [result.py](result.py) into the experiment folder. These are run from inside the experiment dir, not the repo root.

If `keep_experiment_unique=True` (default), a `-YYYYMMDD-HHMMSS` suffix is appended; otherwise repeated runs reuse the directory.

### Multi-node specifics

A node has `node_number >= 0` (0 = master); single-node uses `-1`. Connectivity is described by three parallel lists on `ExperimentContext`: `neighbor_node_list`, `latencies_ns_list`, `syncs_list` (`'true'`/`'false'` strings, not bools), plus `pdes_net_devs` (`'e1000'` or `'virtio-net-pci'`). They must all be the same length and `node_number` must be set when any are non-empty (asserted in `create_experiment_context`).

A single `./qflex <phase> -c <multi.yaml>` now runs every node in the multi-node setup: the YAML places per-node leaves under the unnamed `experiment_context` group component as `_deps_.sub_experiments`, and the executor's `_execute_group` fans them out via `multiprocessing.Process`. Per-leaf ordering uses Python-side sentinels (master sets `wait_for_nodes=[]`, node 1 sets `[0]`, etc.); the sentinel basename includes `_part<P>_idx<I>` for per-(partition, idx) coordination at the `RunIdxCommand` granularity. See [conf/DC/dc-multi.yaml](conf/DC/dc-multi.yaml) for the canonical example and the `multi-node` skill for the full design.

Inter-node traffic flows over POSIX shared memory (`/dev/shm/pdes_<from>_to_<to>...`), wired in by `ExperimentContext.setup_nic_args()`. The master node is responsible for clearing stale shm files before launch — non-master nodes do not. After a crashed run, [clean_up.sh](clean_up.sh) removes `/dev/shm/pdes*` and kills lingering qemu processes; use it before retrying.

Each node also gets its own copy of the disk image (suffix `-node<N>`) via `copy_image_for_node()`.

For `boot` and `load` specifically, two opt-in per-leaf modes let the user actually interact with QEMU under multi-node: `interaction_script: <path>` (Path A — drives QEMU automatically via expect against telnet serial + telnet monitor; auto-enables both telnet endpoints) and `interactive_tmux: true` (Path B — opens one tmux window per leaf via `libtmux`; the executor blocks until QEMU exits in every window). See the `boot-load-interactive` skill for the channel-by-channel mental model.

**Critical invariant for `interaction_script` under multi-node: the script that runs on the master (node 0) is NOT the same script that runs on non-master nodes. Different work, different YAML wiring per leaf.** Concretely:

- **Only the master issues `savevm` over its monitor.** PDES `savevm` triggers a `DRAIN_START` / `DRAIN_END` coordination across every node; each node's CPU+memory state is persisted as part of the distributed snapshot. If a non-master also sends `savevm`, you double-drive the drain and produce a corrupt snapshot. If no node sends it, nothing happens.
- **Non-master nodes must STAY ALIVE during the master's savevm.** Their QEMU processes participate in the drain; quitting before the master finishes tears down the PDES wire mid-snapshot. The standard pattern is: master writes a sentinel file (e.g. `<dirname $EXP_FOLDER>/savevm_done.flag`) after `savevm` returns, and every non-master script polls for that file before quitting.
- **Same applies to other monitor-issued, drain-coordinated operations** — anything that walks the PDES wire (currently `savevm`; potentially future `loadvm`-side or sync commands) is master-only by convention.
- The `dc-multi-savevm-create.yaml` fixture in [conf/DC/](conf/DC/) is the canonical example: node 0 → `boot_create_and_savevm_master.exp`, node 1 → `boot_create_and_wait.exp`. Never wire the same `interaction_script` to both leaves when a snapshot is involved.

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
