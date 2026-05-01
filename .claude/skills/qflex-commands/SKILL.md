---
name: qflex-commands
description: Use when working inside [commands/](../../../commands/) — adding a new pipeline phase, modifying an existing command's shell invocation, debugging executor failures, touching the [Executor](../../../commands/executer.py) hierarchy, working with the per-experiment filesystem layout, or tracing how `ExperimentContext` flows from CLI/YAML into a running QEMU/Flexus process. TRIGGER when the user mentions Executor/SequentialGroupExecutor/ParallelExecutor, a specific subcommand body (boot.py, fw, run_partition, …), Jinja templates in [templates/](../../../templates/), `set_up_folders`, multi-node shm wiring, or "where does the command actually run qemu". SKIP for changes to CLI flag declarations or DI/YAML wiring (those have their own skills).
---

# qflex commands layer

Every pipeline command class in [commands/](../../../commands/) takes an `ExperimentContext`, builds a bash invocation, and shells out via `subprocess.run`. The CLI side (`qflex-cli` skill) gives them the context; the DI side (`qflex-dependency-injection` skill) builds the context. **This skill is about everything between those two and the actual QEMU/Flexus binary.**

## Core data model: `ExperimentContext`

[commands/config.py:103](../../../commands/config.py#L103) — Pydantic model that every command consumes. Composed of:

- `simulation_context: SimulationContext` — core counts, cache geometry, memory controllers, NIC string, quantum size.
- `host: Host | SMTHost` — selected by `host_name` from [commands/host/](../../../commands/host/) (`zen3`, `saphire`); controls core-affinity sequence in `core_info.csv`.
- `workload: Workload` — from [commands/workload/](../../../commands/workload/); has `IPC_info` (primary/secondary/phantom IPC, machine freq) and `core_range` (consolidation layout).
- Multi-node fields: `node_number`, parallel lists `neighbor_node_list`/`latencies_ns_list`/`syncs_list`/`pdes_net_devs`, `partition_number`, `idx`.
- Filesystem fields: `image_folder`, `image_name`, `mounting_folder`, `loadvm_name`, `experiment_name`, `keep_experiment_unique`, `seed_image_name`.

The factory `create_experiment_context` ([commands/config.py:467](../../../commands/config.py#L467)) is the **single source of truth** for both CLI and YAML — see the qflex-cli and qflex-dependency-injection skills.

### Helpful methods on `ExperimentContext`

| Method | Use |
|---|---|
| `get_experiment_folder_address()` | `<mounting_folder>/experiments/<experiment_name>[-<ts>]/` |
| `get_partition_folder()` | `<exp_folder>/run/partition_<N>/` (asserts `partition_number >= 0`) |
| `is_multi_node()` | `node_number >= 0` |
| `is_master_node()` | `node_number == 0` |
| `get_neighbor_count()` | `len(neighbor_node_list)` |
| `get_shm_names(recieve)` | List of POSIX shm names for inter-node traffic |
| `setup_nic_args()` | Builds the QEMU `-netdev pdes,...` arg string into `simulation_context.qemu_nic` |
| `set_up_folders()` | Creates `bin/cfg/flags/lib/run/scripts/images/`, copies binaries, symlinks images, writes `core_info.csv`. **Has side effects — runs at factory time** |
| `clone_experiment_context(src, **overrides)` | Re-build from `_creation_kwargs` with overrides applied |

## Executor pattern ([commands/executer.py](../../../commands/executer.py))

```
Executor (abstract)
├── cmd() -> str | list[str]              # subclass builds the bash invocation
├── execute(to_stdio, run_in_background, dry_run) -> bool
├── clean_up()                            # delegates to experiment.clean_up() if present
├── get_experiment() -> ExperimentContext | None
SimpleCMDExecutor              # wraps a literal string
SequentialGroupExecutor        # children one-by-one; stop on first failure (raises with stdout/stderr)
ParallelExecutor               # multiprocessing.Pool; terminates pool on first failure
```

Every concrete command class (`Boot`, `Load`, `InitWarm`, `FunctionalWarming`, `PartitionCommand`, `RunPartitionCommand`, `RunIdxCommand`, `RunSinglePartitionCommand`, `RunResultCommand`, `Multi`, …) subclasses `Executor`, takes `experiment_context` (and any extras) in `__init__`, returns the bash string from `cmd()`, and is invoked via `executor.execute(to_stdio=True, run_in_background=False)`.

### Execute conventions

- **`shell=True`** is the default — `cmd()` returns a string, joined with ` && ` if a list. Be wary of injection if you ever interpolate user input (none of the current commands do).
- **`run_in_background=True`** is `NotImplementedError`. Don't pass it. (Background path exists in code but is gated; cleanup isn't wired.)
- **Dry-run** — pass `dry_run=True` or set `QFLEX_DRY_RUN=1` in the env. Prints the command and returns success without running. Useful when iterating on cmd-string construction.
- **`ParallelExecutor` failure semantics** — first child to return `False` triggers `pool.terminate()` and a `RuntimeError`. Don't expect partial completion.
- **Sequential failure** — `SequentialGroupExecutor` reads each child's stdout/stderr file (via `get_log_file_address()` / `get_err_file_address()`) on failure and includes them in the raised `RuntimeError` message. Subclasses that go through this path should override those getters.

## Per-experiment filesystem layout ([commands/config.py:245](../../../commands/config.py#L245))

`set_up_folders()` creates:

```
<mounting_folder>/experiments/<experiment_name>[-YYYYMMDD-HHMMSS]/
├── bin/
├── cfg/         ← Flexus configs (timing.cfg, flexus_configuration.json), core_info.csv
├── flags/
├── lib/         ← libknottykraken.so, libsemikraken.so, WormCacheQFlex/ (copied)
├── run/         ← qemu binaries symlinked here, plus partition_<N>/ subdirs
│   ├── qemu-system-aarch64           (← parallel-qemu-saved/build/, the FW binary)
│   ├── vanilla-qemu-system-aarch64   (← qemu-saved/build/, the timing binary)
│   ├── QEMU_EFI.fd, efi-virtio.rom, efi-e1000.rom, qemu-img, debug.cfg
│   └── <image_name>                  (symlink to <image_folder>/<image_name>)
├── scripts/     ← run_flexus.sh and friends, generated from Jinja templates
├── images/      ← per-node copies (suffix `-node<N>`) when multi-node
└── partition.py, result.py    (copied from repo root; run from inside the experiment)
```

`keep_experiment_unique=True` appends `-YYYYMMDD-HHMMSS` so repeated runs don't collide; `False` reuses the directory. The factory default is now `False` (the old `--unique` flag was deprecated and force-set False).

The kraken libs path is **hardcoded** to `/home/dev/qflex/kraken_out/lib{knotty,semi}kraken.so` ([commands/config.py:323](../../../commands/config.py#L323)) — matches the in-container layout, will fail on a host build.

## Code generation: Jinja templates

[templates/](../../../templates/) holds runtime config templates: `timing.cfg.j2`, `flexus_configuration.json.j2`, `run_flexus.sh.j2`, `parameter.rs.j2`. Loaders in [commands/jinja_loaders/](../../../commands/jinja_loaders/) populate them from `ExperimentContext` and write into the experiment's `cfg/`, `scripts/`, `flags/` dirs.

**When a Flexus config field changes** you usually need to touch *three* places: the Pydantic model field (or sub-model), the factory param, and the template. Forgetting one is a common cause of "everything looks fine but the simulator reads stale values."

## Subcommand summary

The Pipeline table in [CLAUDE.md](../../../CLAUDE.md) maps subcommands to phases at a high level. The class/file layout:

| Subcommand | Class | File | Notes |
|---|---|---|---|
| `create-base-image` | `CreateImage` | [createimage.py](../../../commands/createimage.py) | Alpine qcow2 in `images/`; doesn't need an `ExperimentContext` |
| `boot` | `Boot` | [boot.py](../../../commands/boot.py) | Boot QEMU for emulation/install; `--vanilla` toggles which binary |
| `load` | `Load` | [load.py](../../../commands/load.py) | Boot from a saved snapshot for FW prep |
| `initialize` | `InitWarm` | [init_warm.py](../../../commands/init_warm.py) | Long-term µarch warm-up; emits Flexus configs |
| `fw` | `FunctionalWarming` | [functional_warming.py](../../../commands/functional_warming.py) | Run workload with WormCacheQFlex; one checkpoint per sampling unit. `--sample-size N` |
| `partition` / `partition-cleanup` / `unpartition` | `PartitionCommand` / `CleanPartitionCommand` / `UnPartitionCommand` | [partition.py](../../../commands/partition.py) | Split FW checkpoints into partitions for parallel timing runs. `--partition-count N` |
| `run-partition` / `run-single-partition` / `run-idx` | `RunPartitionCommand` / `RunSinglePartitionCommand` / `RunIdxCommand` | [run_partition.py](../../../commands/run_partition.py), [run_single_partition.py](../../../commands/run_single_partition.py), [run_idx.py](../../../commands/run_idx.py) | Timing simulation. `--warming-ratio` / `--measurement-ratio` per sampling unit |
| `result` | `RunResultCommand` | [result.py](../../../commands/result.py) | Aggregate per-partition output into `core_info.csv` |
| `multi` | `Multi` | [multinode.py](../../../commands/multinode.py) | Wraps `gdb --args ./qemu-system-aarch64 ...`; placeholder/scratch |

Other files in the folder: [config.py](../../../commands/config.py) (the central data model — see top of this skill), [executer.py](../../../commands/executer.py) (Executor hierarchy), [docker.py](../../../commands/docker.py) (used by the `./dep` host CLI), [qemu.py](../../../commands/qemu.py), [measure.py](../../../commands/measure.py), [utils.py](../../../commands/utils.py), [version.py](../../../commands/version.py).

## Multi-node specifics

A node has `node_number >= 0` (`0` = master); single-node uses `-1`.

- The four parallel lists (`neighbor_node_list`, `latencies_ns_list`, `syncs_list`, `pdes_net_devs`) **must all be the same length**, and `node_number` must be set when any are non-empty (asserted in [`create_experiment_context`](../../../commands/config.py#L467)).
- `pdes_net_devs` values must be `'e1000'` or `'virtio-net-pci'`.
- `syncs_list` values are `'true'`/`'false'` **strings, not bools**.
- Inter-node traffic flows over POSIX shared memory (`/dev/shm/pdes_<from>_to_<to>...`), wired in by `ExperimentContext.setup_nic_args()`. **Master node clears stale shm files before launch**; non-master nodes do not. After a crashed run, [clean_up.sh](../../../clean_up.sh) removes `/dev/shm/pdes*` and kills lingering qemu processes — use it before retrying.
- Each node gets its own copy of the disk image (suffix `-node<N>`) via `copy_image_for_node()`.

## Adding a new pipeline step

The pattern is identical across the existing commands:

```python
# commands/my_phase.py
from .config import ExperimentContext
from .executer import Executor

class MyPhase(Executor):
    def __init__(self, experiment_context: ExperimentContext, ...):
        self.experiment = experiment_context
        # ...stash any extras

    def cmd(self) -> str:
        exp = self.experiment
        return f"cd {exp.get_experiment_folder_address()}/run && ./qemu-system-aarch64 ..."
```

Then expose it as a thin Typer wrapper in [qflex](../../../qflex):

```python
@app.command()
@data_class_wrap(create_experiment_context, name="experiment_context")
def my_phase(
    experiment_context: ExperimentContext,
    foo: Annotated[int, typer.Option(help="...")] = 1,
):
    """Docstring becomes --help text."""
    MyPhase(experiment_context=experiment_context, foo=foo).execute(to_stdio=True, run_in_background=False)
```

If the new phase orchestrates multiple sub-steps, use `SequentialGroupExecutor` or `ParallelExecutor` rather than chaining ` && ` inside `cmd()` — you get failure isolation and stdout/stderr capture for free.

## Common pitfalls

- **`run_in_background=True`** is `NotImplementedError`. Don't pass it. There's a half-finished Popen path in [executer.py:44](../../../commands/executer.py#L44) but cleanup isn't wired.
- **`set_up_folders()` runs inside the factory** — repeated runs of the *same* command in the same session can collide unless the YAML/CLI sets a unique `experiment_name` or you flip `keep_experiment_unique` to `True`.
- **Hardcoded `/home/dev/qflex/kraken_out/`** path — anything that runs `set_up_folders()` outside the dev container will hit `FileNotFoundError`. Run inside the container or stub the kraken libs.
- **Jinja template + Pydantic field drift** — when a Flexus config knob changes, search [templates/](../../../templates/) for it; you almost always need to update both sides.

## Key files

- [commands/config.py](../../../commands/config.py) — `ExperimentContext`, `SimulationContext`, `create_experiment_context`, `set_up_folders`, `setup_nic_args`, `get_ipns_per_core`.
- [commands/executer.py](../../../commands/executer.py) — `Executor` ABC, `SimpleCMDExecutor`, `SequentialGroupExecutor`, `ParallelExecutor`, `QFLEX_DRY_RUN` env var.
- [commands/jinja_loaders/](../../../commands/jinja_loaders/) — Jinja template populators.
- [templates/](../../../templates/) — `.j2` source files for Flexus/QEMU config.
- [commands/host/](../../../commands/host/), [commands/workload/](../../../commands/workload/) — fixed registries for host topologies and workload IPC profiles.
- [partition.py](../../../partition.py), [result.py](../../../result.py) — root-level scripts that get copied into each experiment folder and invoked from there (not from the repo root).
- [clean_up.sh](../../../clean_up.sh) — `/dev/shm/pdes*` cleanup + qemu process kill, for after a crashed multi-node run.
