---
name: executor
description: Use when working with the [Executor](../../../commands/executer.py) hierarchy — the shared `cmd()` → `subprocess.run(shell=True)` machinery that every pipeline subcommand routes through. Covers `Executor`, `SimpleCMDExecutor`, `SequentialGroupExecutor`, `ParallelExecutor`, the `execute()` flag matrix (`to_stdio`, `run_in_background`, `dry_run`), the `QFLEX_DRY_RUN` env var, failure semantics, log/err file capture, and `clean_up()`. TRIGGER when the user mentions Executor / SimpleCMDExecutor / SequentialGroupExecutor / ParallelExecutor, "how does the command actually get run", `subprocess.run`, `shell=True`, dry-run, `QFLEX_DRY_RUN`, `multiprocessing.Pool` failures inside qflex, `get_log_file_address` / `get_err_file_address`, or `run_in_background=True`. SKIP for changes to what a specific command's `cmd()` *contains* (that's the qflex-commands skill's domain) or to CLI/DI wiring (their own skills).
---

# Executor — the shared command-execution layer

Every pipeline subcommand in [commands/](../../../commands/) ultimately produces a bash string and shells out to it. The shared mechanism lives in a single file: [commands/executer.py](../../../commands/executer.py). This skill explains that file end-to-end and how each concrete command in the repo plugs into it.

The CLI side ([qflex](../../../qflex)) and the DI/YAML side ([dep_injection/](../../../dep_injection/)) build the `ExperimentContext`; the per-command class then wraps it into an executor; `executor.execute(...)` is what turns it into a running QEMU/Flexus/docker process. **This skill is about the `execute(...)` step itself** — not about what the bash string contains.

## **READ THIS FIRST: `cmd()` vs `execute()`**

Every concrete executor has *two* possible entry points and which one runs is decided by the **class**, not by the caller:

- **Pattern A — leaf executors (`Executor`, `SimpleCMDExecutor`, and every per-phase command class like `Boot`, `Load`, `RunIdxCommand`, `Multi`, `DockerStarter`, …):** the **subclass implements `cmd()`** and inherits the base `execute()` from `Executor`. The base `execute()` then calls `self.cmd()`, joins the result with `&&` if it's a list, and runs it via `subprocess.run(arg, shell=True, cwd=os.getcwd())`. This is the "build a bash string, shell it out" path that 90% of the codebase uses.

- **Pattern B — group executors (`SequentialGroupExecutor`, `ParallelExecutor`, and their concrete subclasses `RunSinglePartitionCommand`, `RunPartitionCommand`):** the **subclass overrides `execute()` directly** and **`cmd()` is never called** — in fact `ParallelExecutor.cmd()` and `RunSinglePartitionCommand.cmd()` explicitly `raise NotImplementedError(...Use execute instead.)` to make accidental calls fail loudly. Group `execute()` walks `self.children` and calls **their** `execute()` recursively (each child is itself either Pattern A or another group). No bash string is ever produced at the group level — only the leaves shell out.

So when reading any executor:

1. Find its base class. If it's `Executor` or `SimpleCMDExecutor` → look at `cmd()`. If it's `SequentialGroupExecutor` or `ParallelExecutor` → look at `__init__` for the `children` list and at the parent's `execute()` for the orchestration semantics.
2. Group executors that subclass `Executor` directly (none in this repo today) would still need to implement `cmd()`. Don't subclass `Executor` if you're orchestrating — pick the right group base.

`SequentialGroupExecutor` and `ParallelExecutor` propagate the same `to_stdio` / `run_in_background` / `dry_run` flags down to each child's `execute()`, so the flag semantics described below for the base `execute()` apply uniformly to leaves regardless of how deep they're nested.

## File layout

[commands/executer.py](../../../commands/executer.py) defines four classes plus one env-var helper. Nothing else lives here:

| Symbol | What it is |
|---|---|
| `DRY_RUN_ENV_VAR` ([executer.py:9](../../../commands/executer.py#L9)) | The string `"QFLEX_DRY_RUN"`. |
| `_is_dry_run(dry_run)` ([executer.py:12](../../../commands/executer.py#L12)) | Caller flag wins; otherwise checks `QFLEX_DRY_RUN ∈ {"1","true","yes"}` (case-insensitive). |
| `Executor` (ABC) ([executer.py:19](../../../commands/executer.py#L19)) | Base class. Subclasses implement `cmd()`. |
| `SimpleCMDExecutor` ([executer.py:91](../../../commands/executer.py#L91)) | Wraps a literal string. |
| `SequentialGroupExecutor` ([executer.py:99](../../../commands/executer.py#L99)) | Runs `children: list[Executor]` one-by-one; raises on first failure. |
| `ParallelExecutor` ([executer.py:126](../../../commands/executer.py#L126)) | Runs `children: list[Executor]` via `multiprocessing.Pool`; terminates pool on first failure. |

Re-exported at the package level via [commands/__init__.py](../../../commands/__init__.py): only `Executor` is in `__all__`. The other three are imported directly from `.executer` by callers that need them.

## The `Executor` ABC

```python
class Executor(abc.ABC):
    @abc.abstractmethod
    def cmd(self) -> str: ...                        # returns str OR list[str]

    def get_experiment(self) -> ExperimentContext | None: ...
    def execute(self, to_stdio=True, run_in_background=False, dry_run=False) -> bool: ...
    def clean_up(self): ...                          # delegates to experiment.clean_up() if present
    def get_log_file_address(self): raise NotImplementedError
    def get_err_file_address(self): raise NotImplementedError
```

### `cmd()` — what leaf subclasses override

`cmd()` may return either a single bash string or a `list[str]`. The base `execute()` joins lists with ` && ` (after `.strip()`-ing each element), so a list of strings is essentially "run all of these in order, abort on first failure." Trailing/leading whitespace is fine — every element is stripped.

There is a `# TODO see if we need to support other type of concatting args` ([executer.py:37](../../../commands/executer.py#L37)) — currently `&&` is the only join.

### `execute(to_stdio, run_in_background, dry_run) -> bool`

The single entry point. Returns `True` on success, `False` on non-zero exit (when in foreground, non-dry-run mode). Behaviour matrix for the **base** `execute()` (group executors override this — see their sections):

| Flag combination | Behaviour |
|---|---|
| `dry_run=True` (or `QFLEX_DRY_RUN=1`) | Prints `[dry-run] <ClassName> (cwd=...): <cmd>` and returns `True` without running. **Skips `clean_up()`.** |
| `run_in_background=True` | **`NotImplementedError`** — cleanup path isn't wired. There's a half-finished `subprocess.Popen` block right after the raise ([executer.py:48-56](../../../commands/executer.py#L48)) — gated, do not enable. |
| `to_stdio=True` (default) | `subprocess.run(arg, shell=True, text=True, cwd=os.getcwd())`. Child inherits stdio (you see output live). Calls `self.clean_up()` after. |
| `to_stdio=False` | Same call but with `capture_output=True`. Caller doesn't see stdout/stderr unless they read them off the result (note: the base implementation **discards** the captured output — only the return code is checked). |

`cwd` is **always** `os.getcwd()` snapshotted at `execute()` entry — there is no way to ask the executor to run elsewhere via a parameter. Subclasses that need a different working dir do it inside the bash string itself (`cd <experiment_folder>/run && ...` is the universal pattern, see e.g. [boot.py:30](../../../commands/boot.py#L30), [run_idx.py:37](../../../commands/run_idx.py#L37)).

### `get_experiment()` and `clean_up()`

`get_experiment()` returns `self.experiment` if (a) the attribute exists and (b) it is an `ExperimentContext`. Otherwise `None`. **Note the attribute name** — `self.experiment`, *not* `self.experiment_context`. Most concrete commands in the repo store the context as `self.experiment_context`, which means `get_experiment()` returns `None` for them and `clean_up()` is a no-op. If you want post-run hooks to run, alias the attribute (`self.experiment = experiment_context`) or override `get_experiment()`.

`clean_up()` is called automatically at the end of every successful and failed foreground run (but **not** for dry-runs).

### `get_log_file_address()` / `get_err_file_address()`

Both raise `NotImplementedError` by default. They exist for `SequentialGroupExecutor` to grab on a child failure (see below). Override them in any executor class you intend to nest under a `SequentialGroupExecutor` and want diagnostic output for. The convention in the repo is:

```python
def get_log_file_address(self):
    return f"{self.experiment_context.get_partition_folder()}/log"
def get_err_file_address(self):
    return f"{self.experiment_context.get_partition_folder()}/err"
```

(see [run_idx.py:25-29](../../../commands/run_idx.py#L25)). The bash string then redirects `> <log> 2> <err>`.

## `SimpleCMDExecutor`

Trivial Pattern-A wrapper around a literal string. Implements `cmd()` returning the string verbatim; uses the base `execute()`. Useful when you need an `Executor` slot in a group but the work is one-liner:

```python
SimpleCMDExecutor("rm -rf output_state")
SimpleCMDExecutor("sleep 5")
```

Both real call sites are in [run_single_partition.py:35,44](../../../commands/run_single_partition.py#L35) — interleaving setup/teardown around the heavyweight `RunIdxCommand` children inside a `SequentialGroupExecutor`.

## `SequentialGroupExecutor`

`__init__(children: list[Executor])`. **Overrides `execute()`** (does **not** override `cmd()` — calling `cmd()` on it would fall back to the abstract base and `TypeError`).

Behaviour:

1. For each child in order, call `child.execute(to_stdio=..., run_in_background=..., dry_run=...)`. The same flags propagate to every child.
2. If a child returns `False`, the group:
   - Pulls `child.get_err_file_address()` and `child.get_log_file_address()`. If the path exists, reads it.
   - Raises `RuntimeError(f"{child.__class__.__name__} failed \nstdout:\n{log}\nstderr:\n{err}")`.
   - Subsequent children do **not** run.
3. After all children succeed, calls `self.clean_up()` and returns `True`.

The `get_log/err_file_address` lookup is wrapped in `if path is not None and os.path.exists(...)` — but the base implementation *raises* when not overridden, so children intended for use under `SequentialGroupExecutor` **must override** both methods. `SimpleCMDExecutor` doesn't override them, so a `SimpleCMDExecutor` failure inside a sequential group will explode the diagnostic-read step itself rather than the original failure. The only real consumer ([run_single_partition.py](../../../commands/run_single_partition.py)) puts only short shell snippets into `SimpleCMDExecutor` slots, so this hasn't bitten in practice.

Real subclass: `RunSinglePartitionCommand` ([run_single_partition.py:10](../../../commands/run_single_partition.py#L10)) — it is itself a `SequentialGroupExecutor` (subclass-by-inheritance, not composition). Its `__init__` discovers per-sampling-unit snapshot files, builds a list of `RunIdxCommand` interleaved with `SimpleCMDExecutor("sleep 5")`, then calls `super().__init__(children)`. It also overrides `cmd()` to raise `NotImplementedError("...Use execute instead.")` — the convention for executor groups, since they bypass `cmd()`.

## `ParallelExecutor`

`__init__(children: list[Executor])`. Like the sequential variant: **overrides `execute()`**, and `cmd()` explicitly raises (`"ParallelExecutor call childrens execute instead."`).

Behaviour:

1. **Asserts** `not run_in_background` — incompatible. (Background-via-pool is the wrong layering anyway.)
2. **Dry-run path**: iterate children and call `child.execute(to_stdio=to_stdio, dry_run=True)` directly in-process. No pool, no parallelism — purely a "what would I run" walk.
3. **Real path**: spin up `multiprocessing.Pool(processes=len(self.children))` (one worker per child — no concurrency cap) and `apply_async` each child via the static `_execute_child((child, to_stdio))`. Each child runs `child.execute(to_stdio=to_stdio, run_in_background=False)`.
4. Polls `r.ready()` in a 0.1-second `time.sleep` busy loop:
   - Any child that returns `False` triggers `pool.terminate()` and `RuntimeError(f"{child_cls} failed.")`.
   - When all children are ready and none failed, exit the loop, call `self.clean_up()`, return `True`.

Caveats:

- **No partial completion semantics.** First failure kills the pool. Children mid-run get `SIGTERM`. There is no "collect all failures" mode.
- **No diagnostic-file read** on failure — unlike `SequentialGroupExecutor`, the parallel variant only reports the class name. If you need stdout/stderr, it's on you to wire log files into the bash string and tell the user where to look. (`RunPartitionCommand` does this implicitly via `to_stdio=False` causing `RunIdxCommand` to redirect to `get_log_file_address()` files inside each partition folder.)
- **`to_stdio=False` is the default** here (vs `True` on `Executor`/`SequentialGroupExecutor`). When children all stream to the same terminal, the output interleaves and is unreadable; the default protects you. `RunPartitionCommand.__init__` ([run_partition.py:19](../../../commands/run_partition.py#L19)) hardcodes `self.use_stdio = False` to enforce this.
- **Pool-size = `len(children)`.** No host-CPU cap. With 16 partitions and a 16-core host, you'll oversubscribe once each child spawns its own QEMU + Flexus subprocesses.

Real subclass: `RunPartitionCommand` ([run_partition.py:10](../../../commands/run_partition.py#L10)) — discovers `partition_*` folders, clones the experiment context per partition (`clone_experiment_context(..., partition_number=idx)`), wraps each in a `RunSinglePartitionCommand`, hands the list to `super().__init__(executors)`.

## How concrete commands plug in

Two patterns. The first dominates; the second is for orchestration.

### Pattern A — `Executor` directly, build a bash string in `cmd()`

This is the default. The class stores `self.experiment_context`, builds the invocation in `cmd()`, lets the base `execute()` shell it out:

| Class | File | Returns | Notes |
|---|---|---|---|
| `CreateImage` | [createimage.py](../../../commands/createimage.py) | `list[str]` (3 cmds joined by `&&`) | No `ExperimentContext` — operates on a raw image folder. |
| `Boot` | [boot.py](../../../commands/boot.py) | `list[str]` (`cd run` + `gdb --args ./qemu-system-aarch64 ...`) | Single-step; used for emulation. |
| `Load` | [load.py](../../../commands/load.py) | Same shape as `Boot`. | FW-prep boot from snapshot. |
| `InitWarm` | [init_warm.py](../../../commands/init_warm.py) | `list[str]` — `build_worm_cache()` build steps + `cd run` + `gdb ... -plugin libworm_cache.so` | Also runs Jinja loaders at `__init__` time. |
| `FunctionalWarming` | [functional_warming.py](../../../commands/functional_warming.py) | `list[str]` (timing + FW gdb command) | One sample's worth of work. |
| `PartitionCommand` / `CleanPartitionCommand` / `UnPartitionCommand` | [partition.py](../../../commands/partition.py) | `list[str]` | Uses inheritance to share `clean_partition_command()`. |
| `RunIdxCommand` | [run_idx.py](../../../commands/run_idx.py) | `list[str]` (15+ steps) | **Overrides `get_log/err_file_address`** because it's nested under `SequentialGroupExecutor` via `RunSinglePartitionCommand`. |
| `RunResultCommand` | [result.py](../../../commands/result.py) | `list[str]` (cd + python aggregator) | |
| `Multi` | [multinode.py](../../../commands/multinode.py) | `list[str]` (cd + `gdb --args ./qemu-system-aarch64 ...`) | Single-node placeholder for multi-node work. |
| `DockerStarter` / `DockerBuild` | [docker.py](../../../commands/docker.py) | `str` (single multi-line `docker run/buildx ...`) | Used by the host-side `./dep` CLI, not `./qflex`. |

### Pattern B — orchestrator (`SequentialGroupExecutor` / `ParallelExecutor`)

Build a list of child executors in `__init__`, hand it to the parent constructor. **`cmd()` is bypassed**; the orchestrator's `execute()` (inherited from the group base) walks the children and calls *their* `execute()`.

```
RunPartitionCommand(ParallelExecutor)        ← run_partition.py
   └── per partition: RunSinglePartitionCommand(SequentialGroupExecutor)   ← run_single_partition.py
            ├── SimpleCMDExecutor("rm -rf output_state")
            ├── RunIdxCommand(idx=0)
            ├── SimpleCMDExecutor("sleep 5")
            ├── RunIdxCommand(idx=1)
            ├── SimpleCMDExecutor("sleep 5")
            └── ...
```

This is the **only** orchestration tree in the codebase right now. Every other command is a flat Pattern A.

### Driving them from the CLI

[qflex](../../../qflex) does the same dance for every subcommand:

```python
executor = SomeCommand(experiment_context=..., ...extras...)
executor.execute(to_stdio=True, run_in_background=False)
```

The only commands that pass non-defaults are:
- `run-partition`, which exposes `--to-stdio` and `--run-in-background` as Typer flags ([qflex:200-211](../../../qflex#L200)). Note: `run_in_background=True` will hit the `NotImplementedError`. Don't pass it.

The result of `execute()` is **discarded** at the CLI layer — it returns a bool but the wrapper doesn't check it. Foreground failure paths surface either via the raised `RuntimeError` (group executors) or via the child binary's own non-zero exit being visible in stdio. Silent `False` returns at the top level are technically possible but currently unreachable, since the only callers of `Executor.execute()` directly (i.e. not through a group) are simple gdb/qemu invocations whose failures the user sees on stdio.

## Dry-run

Two ways to enable:

1. Pass `dry_run=True` to `execute()`. No CLI flag exposes this directly today; you'd add it per-command (or wrap your call).
2. Set `QFLEX_DRY_RUN=1` (or `true`/`yes`, case-insensitive) in the env. Caller-passed `True` always wins.

Output format: `[dry-run] <ClassName> (cwd=<wd>):\n  <command-string>`. The command string is the post-join (`&& `) rendering, so what you see is exactly what would be passed to `subprocess.run`.

Dry-run propagates to children for both `SequentialGroupExecutor` and `ParallelExecutor`. The parallel variant runs the dry-run walk **in-process, sequentially** — no pool spawn, no actual parallelism. This means the printed order matches the children list order, which is convenient for diff-based debugging.

`clean_up()` is **not** called on the dry-run path. If a downstream command depends on cleanup side effects (e.g. shm wipe), dry-run will not exercise that.

## Common pitfalls

- **`run_in_background=True` is `NotImplementedError`.** Don't pass it. The half-finished `Popen` block at [executer.py:48-56](../../../commands/executer.py#L48) is intentionally gated; backgrounding without cleanup wiring will leak processes.
- **`shell=True` everywhere.** Every `execute()` path uses `shell=True`. None of the current commands interpolate user-controlled strings, but if you ever do, quote them. The CLI flag values flow through OmegaConf/Pydantic typing, not raw — but a future feature might add a string field that lands in `cmd()`.
- **`get_experiment()` looks for `self.experiment`, not `self.experiment_context`.** Most commands store the context under the longer name, so `clean_up()` is effectively a no-op for them. If you want the experiment cleanup hook to fire, alias the attribute.
- **`SequentialGroupExecutor` calls `get_log/err_file_address` on child failure.** Unless the child overrides both, it'll `NotImplementedError` *inside* the failure-reporting code. Either override them in the child or wrap shell-only steps in something other than `SimpleCMDExecutor`.
- **`ParallelExecutor` pool size = N children.** No host-CPU cap; oversubscription is on you. With 16 partitions × QEMU/Flexus per child, the host needs to be sized accordingly.
- **`ParallelExecutor` failure semantics: first failure terminates the pool.** No partial completion, no aggregated report — just the failed class name. Wire log files via `to_stdio=False` + per-partition redirection if you want post-mortem output.
- **Group executors override `execute`, not `cmd`.** Calling `.cmd()` on a `SequentialGroupExecutor` will fall through to the abstract base; `RunSinglePartitionCommand` and `ParallelExecutor` explicitly raise `NotImplementedError` from `cmd()` to make this fail loudly. (See [the very-top section](#read-this-first-cmd-vs-execute) — this is the single most common source of confusion.)
- **`cwd` is `os.getcwd()` at call time.** Always. If you want a different working dir, prefix the bash string with `cd <path> && ...` like the rest of the repo does.

## Adding a new executor

For a single-step command (Pattern A), copy the boot.py shape — implement `cmd()`, inherit `execute()`:

```python
from commands import Executor
from .config import ExperimentContext

class MyPhase(Executor):
    def __init__(self, experiment_context: ExperimentContext, my_flag: int):
        self.experiment_context = experiment_context
        self.my_flag = my_flag

    def cmd(self) -> str:
        exp = self.experiment_context
        return [
            f"cd {exp.get_experiment_folder_address()}/run",
            f"./qemu-system-aarch64 ...",
        ]
```

For an orchestrator (Pattern B), decide sequential-with-stop-on-failure (`SequentialGroupExecutor`) vs all-at-once-fan-out (`ParallelExecutor`). Subclass the appropriate group, build the `children` list in `__init__`, call `super().__init__(children)`, and override `cmd()` to raise (it shouldn't be called — the inherited `execute()` walks `self.children` instead).

If your executor will be nested under `SequentialGroupExecutor`, override `get_log_file_address()` and `get_err_file_address()` to return real paths and have the bash string redirect into them — otherwise failure reporting will itself crash.

Expose the new executor as a thin `@app.command()` wrapper in [qflex](../../../qflex) (see the `qflex-commands` and `qflex-cli` skills for the wrapping pattern).

## Key files

- [commands/executer.py](../../../commands/executer.py) — the entire executor hierarchy.
- [commands/__init__.py](../../../commands/__init__.py) — re-exports `Executor`. The group classes are imported directly from `.executer` by callers.
- [commands/run_partition.py](../../../commands/run_partition.py) — the only `ParallelExecutor` subclass in the repo.
- [commands/run_single_partition.py](../../../commands/run_single_partition.py) — the only `SequentialGroupExecutor` subclass and the only consumer of `SimpleCMDExecutor`.
- [commands/run_idx.py](../../../commands/run_idx.py) — example of overriding `get_log/err_file_address` for use under a group.
- [qflex](../../../qflex) — the per-subcommand `executor.execute(...)` call sites.
