---
name: executor
description: Use when working with the [Executor](../../../commands/executer.py) hierarchy — the shared `cmd()` → `subprocess.run(shell=True)` machinery plus the multi-experiment dispatch (`mp.Process` over `sub_experiments`) that every pipeline subcommand routes through. Covers `Executor`, `SimpleCMDExecutor`, `SequentialGroupExecutor`, the `_execute_group` / `_execute_leaf` split, sentinel coordination (`wait_for_nodes` + `<phase>_part<P>_idx<I>_node<N>.started/.done`), the `execute()` flag matrix (`to_stdio`, `run_in_background`, `dry_run`), the `QFLEX_DRY_RUN` env var, the `[py-wait]`/`[py-touch]`/`[bash]`/`[tmux]`/`[py-poll]` dry-run markers, the libtmux Path B for boot/load, failure semantics, log/err file capture, and `clean_up()`. TRIGGER when the user mentions Executor / SimpleCMDExecutor / SequentialGroupExecutor / `_execute_group` / `_execute_leaf` / `_make_child` / sub_experiments dispatch / mp.Process / sentinels / `wait_for_nodes` / "how does the command actually get run" / `subprocess.run` / `shell=True` / dry-run / `QFLEX_DRY_RUN` / `get_log_file_address` / `get_err_file_address` / `run_in_background=True` / `interactive_tmux` / `SUPPORTS_INTERACTIVE`. SKIP for changes to what a specific command's `cmd()` *contains* (qflex-commands skill) or to CLI/DI wiring (their own skills).
---

# Executor — the shared command-execution layer

Every pipeline subcommand in [commands/](../../../commands/) ultimately produces a bash string and shells out to it. The shared mechanism lives in a single file: [commands/executer.py](../../../commands/executer.py). This skill explains that file end-to-end and how each concrete command in the repo plugs into it.

The CLI side ([qflex](../../../qflex)) and the DI/YAML side ([dep_injection/](../../../dep_injection/)) build the `ExperimentContext`; the per-command class then wraps it into an executor; `executor.execute(...)` is what turns it into a running QEMU/Flexus/docker process. **This skill is about the `execute(...)` step itself** — not about what the bash string contains.

## **READ THIS FIRST: three dispatch paths inside `execute()`**

Every concrete executor's `execute()` follows the same top-level branching, defined on the base [`Executor`](../../../commands/executer.py):

1. **Group dispatch** (`_execute_group`) — fires when `self.experiment_context.has_sub_experiments()`. Spawns one `multiprocessing.Process` per sub; each child sets `executor.experiment_context = sub` and re-enters `execute()`. Used for the multi-node axis (sub_experiments wired from YAML) and dynamically for per-partition fan-out (`RunPartitionCommand` populates them at execute time). This is the **single mechanism** for every parallelism axis — the legacy `ParallelExecutor` class was removed.
2. **Tmux dispatch** (`_execute_in_tmux`) — fires when `self.SUPPORTS_INTERACTIVE` is True (set on `Boot` and `Load` only) **and** `experiment_context.interactive_tmux` is True. Sends the leaf bash to a fresh tmux window via `libtmux` and blocks until the user quits QEMU.
3. **Leaf dispatch** (`_execute_leaf`) — the default. Calls `experiment_context.prepare_for_execution()`, builds the bash via `_build_bash`, and runs it via `subprocess.run(arg, shell=True, cwd=os.getcwd())`.

`SequentialGroupExecutor` overrides `execute()` to run a heterogeneous list of children sequentially (used by `RunSinglePartitionCommand` to interleave `SimpleCMDExecutor("rm -rf output_state")` + `RunIdxCommand` + `SimpleCMDExecutor("sleep 5")`). It still routes to `_execute_group` first when its context has sub_experiments, then falls through to its children loop.

So when reading any executor:

1. Find its base class. If it's `Executor` → look at `cmd()` (leaf bash) and let the base do the dispatch. If it's `SequentialGroupExecutor` → look at `_build_children()` (or the children passed to `super().__init__`) and at the parent's `execute()` for orchestration semantics.
2. If the executor needs to dynamically generate per-partition (or per-anything) sub_experiments, do it in `execute()` via `experiment_context.model_copy(update={"sub_experiments": ...})` and let the base `_execute_group` fan them out — see `RunPartitionCommand` for the canonical example.

## File layout

[commands/executer.py](../../../commands/executer.py) is the entire dispatch surface:

| Symbol | What it is |
|---|---|
| `DRY_RUN_ENV_VAR` | The string `"QFLEX_DRY_RUN"`. |
| `_is_dry_run(dry_run)` | Caller flag wins; otherwise checks `QFLEX_DRY_RUN ∈ {"1","true","yes"}` (case-insensitive). |
| `_multi_experiment_target(executor, sub, kw)` | Module-level mp.Process target. Mutates `executor.experiment_context = sub` and calls `executor.execute(**kw)`. Picklable. |
| `Executor` (ABC) | Base class. Subclasses implement `cmd()`. Owns the dispatch (`execute` → `_execute_group` / `_execute_in_tmux` / `_execute_leaf`), sentinel helpers, and the dry-run printer. |
| `SimpleCMDExecutor` | Wraps a literal string. |
| `SequentialGroupExecutor` | Runs `children: list[Executor]` one-by-one (after dispatching on sub_experiments first). Subclasses override `_build_children()` to lazily produce them. |

**`ParallelExecutor` is gone.** Its job (per-children parallel fan-out via `multiprocessing.Pool`) was redundant once `_execute_group` existed. Per-partition parallelism in `RunPartitionCommand` now uses `_execute_group` directly by populating sub_experiments at execute time.

Re-exported at the package level via [commands/__init__.py](../../../commands/__init__.py): only `Executor` is in `__all__`. The other classes are imported directly from `.executer` by callers that need them.

## The `Executor` ABC

```python
class Executor(abc.ABC):
    SUPPORTS_INTERACTIVE: bool = False           # opt-in; True on Boot and Load only

    @abc.abstractmethod
    def cmd(self) -> str | list[str]: ...        # subclass leaf bash

    def get_experiment(self) -> ExperimentContext | None: ...
    def execute(self, to_stdio=True, run_in_background=False, dry_run=False,
                *, sentinel_dir=None, log_path=None, err_path=None) -> bool: ...

    # Dispatch branches
    def _execute_group(self, ..., outer_sentinel_dir=None) -> bool: ...
    def _execute_leaf(self, ...) -> bool: ...
    def _execute_in_tmux(self, ...) -> bool: ...

    # Sentinels (Python-side polling and touch)
    def _phase_name(self) -> str: ...                              # default = class name
    def _sentinel_basename(self, node_number: int) -> str: ...     # adds part<P>_idx<I> when set
    def _sentinel_path(self, sentinel_dir, node_number, suffix) -> str: ...
    def _wait_for_sentinels(self, sentinel_dir): ...               # blocks on wait_for_nodes
    def _touch_sentinel(self, sentinel_dir, suffix): ...

    # Bash + dry-run
    def _build_bash(self, log_path, err_path) -> str: ...          # joins cmd() with " && "
    def _print_dry_run_actions(self, cwd, sentinel_dir, log_path, err_path) -> None: ...

    def clean_up(self): ...                       # delegates to experiment.clean_up()
    def get_log_file_address(self): raise NotImplementedError
    def get_err_file_address(self): raise NotImplementedError
```

### `cmd()` — what subclasses override

`cmd()` may return either a single bash string or a `list[str]`. `_build_bash` joins lists with ` && ` (after `.strip()`-ing each element). When the base passes a `log_path` / `err_path` (i.e. running as a sub-experiment leaf), the joined bash is wrapped with `( ... ) > <log> 2> <err>` so output goes to per-leaf files.

**Critical convention:** `cmd()` must build everything from `self.experiment_context` *fresh* on every call — no `__init__`-time caching of context-derived state (e.g. `QemuCommonArgParser` instances). The multi-experiment dispatch mutates `self.experiment_context` to point at each sub before calling `execute()` recursively, so any state baked at `__init__` time goes stale.

### `execute(...)` — the unified entry point

Returns `True` on success, `False` on non-zero exit. Behaviour matrix for the **base** `execute()` (group/tmux/leaf path picked at the top):

| Condition | Path |
|---|---|
| `experiment_context.has_sub_experiments()` | → `_execute_group` (mp.Process per sub) |
| `SUPPORTS_INTERACTIVE` + `interactive_tmux=True` | → `_execute_in_tmux` (libtmux window per leaf) |
| Otherwise | → `_execute_leaf` (subprocess.run) |

Flag matrix on the leaf branch:

| Flag combination | Behaviour |
|---|---|
| `dry_run=True` (or `QFLEX_DRY_RUN=1`) | Calls `experiment_context.compute_runtime_settings()` (pure: auto-flips telnet flags, auto-computes ports), then prints the structured dry-run block (see "Dry-run" below) and returns `True`. **Skips** filesystem prep and `clean_up()`. |
| `run_in_background=True` | **`NotImplementedError`** — cleanup path isn't wired. There's a half-finished `subprocess.Popen` block in the source — gated, do not enable. |
| `to_stdio=True` (default) | `subprocess.run(arg, shell=True, text=True, cwd=os.getcwd())`. Child inherits stdio (you see output live). Calls `self.clean_up()` after. |
| `to_stdio=False` | Same call but with `capture_output=True`. Caller doesn't see stdout/stderr unless they read them off the result. |

`cwd` is **always** `os.getcwd()` snapshotted at `execute()` entry. Subclasses that need a different working dir do it inside the bash string itself (`cd <experiment_folder>/run && ...` is the universal pattern).

`sentinel_dir` / `log_path` / `err_path` are only set when this leaf is being run as a child of a `_execute_group` dispatch — `_execute_leaf` then redirects bash output to the per-leaf log/err files and Python performs the sentinel wait/touch around the subprocess call. Single-node invocations have `sentinel_dir=None` and these effects are skipped.

### Sub-experiment dispatch (`_execute_group`)

```
exp.has_sub_experiments() == True
        │
        ▼
sentinel_dir = <group folder>/.sentinels      # or inherited via outer_sentinel_dir for nested groups
mkdir + wipe stale sentinels                   # only the outermost group does this
        │
        ▼  for each sub:
mp.Process(target=_multi_experiment_target,
           args=(self, sub, kw)).start()
   │
   ▼  in child process:
self.experiment_context = sub
self.execute(**kw)   # re-enters; sub has no sub_experiments → falls through to leaf branch
        │
        ▼
join all; aggregate failures by node_number
```

The per-child `kw` carries `sentinel_dir` + per-sub `log_path` / `err_path` so leaves can wrap their output redirect correctly. Nested groups (e.g. multi-node group → per-partition group) inherit the outermost sentinel dir via `outer_sentinel_dir` so all leaves coordinate against the same set of files.

### Sentinel coordination (`wait_for_nodes` + per-leaf basenames)

The sentinel basename is built by `_sentinel_basename(node_number)`:

```
<phase>[_part<P>][_idx<I>]_node<N>     # P/I appended only when set on the context
```

So for `Boot` on a multi-node setup, master node touches `Boot_node0.started`. For `RunIdxCommand` at (partition=5, idx=3), master touches `RunIdxCommand_part5_idx3_node0.started` — coordinating per-(partition, idx) pair across nodes (matching the granularity of the multi-node shm rings, which are also keyed by part/idx).

In `_execute_leaf`:

1. `_wait_for_sentinels(sentinel_dir)` polls `<phase>_..._node<n>.started` for every `n` in `self.experiment_context.wait_for_nodes`. Sleeps 0.5s between checks.
2. `experiment_context.prepare_for_execution()` runs leaf prep (folders, nic args, telnet ports). Side effects happen *only here*, never in the factory — `create_experiment_context` is pure.
3. `_touch_sentinel(sentinel_dir, "started")` opens-then-closes the `.started` file.
4. Bash runs.
5. On rc=0, `_touch_sentinel(sentinel_dir, "done")` does the same for `.done`.

`wait_for_nodes` is a generic primitive: master sets `[]`, node 1 sets `[0]`, node 3 can set `[2]`. Chained orderings without code changes.

### Tmux dispatch (`_execute_in_tmux`)

Only fires when `SUPPORTS_INTERACTIVE=True` (set on `Boot` and `Load`) and `experiment_context.interactive_tmux=True`. Imports `libtmux` lazily; raises `RuntimeError("interactive_tmux requires a running tmux server …")` if no tmux server exists. Picks the active session via the `TMUX` env var, otherwise the first session.

Sequence:

1. `_wait_for_sentinels` (same as leaf).
2. `prepare_for_execution`.
3. `_touch_sentinel(..., "started")`.
4. `session.new_window(window_name=f"{phase}-node{N}", attach=False)` — never reuses windows; the user can find each by name.
5. `pane.send_keys(bash + "; touch <done-marker>", enter=True)` — Python returns immediately from send_keys, but the bash itself is still running in the tmux pane.
6. **Block** in Python by polling `os.path.exists(<done-marker>)` until the user quits QEMU (the appended `touch` fires after the bash finishes). This is what makes `./qflex boot -c <multi.yaml>` block until the user has actually completed their interactive work in every window.

See the `boot-load-interactive` skill for the full QEMU-channel context (telnet serial vs telnet monitor vs tmux mon:stdio).

### `_build_bash(log_path, err_path)`

Joins `cmd()` with ` && ` and, when both `log_path` and `err_path` are set, wraps with `( ... ) > <log> 2> <err>`. The wrapping fires only when running as a multi-experiment leaf (the outer `_execute_group` provides the paths). Single-node `./qflex` invocations leave the bash unwrapped so output streams to the user's terminal.

The redirect is a pure shell `>` — no Python-side stdio capture is involved.

### `get_experiment()` and `clean_up()`

`get_experiment()` returns `self.experiment_context` if it's an `ExperimentContext` (with a fallback to the legacy `self.experiment` attribute for any executor that pre-dates the rename). `clean_up()` is called automatically at the end of every successful and failed foreground run (but **not** for dry-runs); it delegates to `experiment.clean_up()` which clears the per-node `/dev/shm/pdes_*` files.

### `get_log_file_address()` / `get_err_file_address()`

Both raise `NotImplementedError` by default. They exist for `SequentialGroupExecutor` to grab on a child failure (see below). Override them in any executor class you intend to nest under a `SequentialGroupExecutor` and want diagnostic output for. The convention in the repo is:

```python
def get_log_file_address(self):
    return f"{self.experiment_context.get_partition_folder()}/log"
def get_err_file_address(self):
    return f"{self.experiment_context.get_partition_folder()}/err"
```

(see [run_idx.py:18-22](../../../commands/run_idx.py#L18)). The bash string then redirects `> <log> 2> <err>` for its own internal use (orthogonal to the multi-experiment leaf-level redirect which targets `<sub_folder>/<phase>.log/.err`).

## `SimpleCMDExecutor`

Trivial wrapper around a literal string. Implements `cmd()` returning the string verbatim; uses the base `execute()`. Useful when you need an `Executor` slot in a sequential group but the work is a one-liner:

```python
SimpleCMDExecutor("rm -rf output_state")
SimpleCMDExecutor("sleep 5")
```

Both real call sites are in [run_single_partition.py](../../../commands/run_single_partition.py) — interleaving setup/teardown around the heavyweight `RunIdxCommand` children inside a `SequentialGroupExecutor`.

## `SequentialGroupExecutor`

`__init__(children: list[Executor])`. Overrides `execute()` to:

1. **Dispatch on sub_experiments first** — if the context has them, route to `_execute_group` exactly like `Executor` does (so multi-node still works for sequential-children orchestrators).
2. Otherwise, build children lazily via `self._build_children()` (subclasses override; default returns whatever was passed to `__init__`).
3. Run each child sequentially, forwarding `sentinel_dir` / `log_path` / `err_path` so leaves down the tree can do their own sentinel touches.
4. On any child returning False, read its `get_log_file_address()` / `get_err_file_address()` content and raise `RuntimeError(f"{child} failed\nstdout:\n{log}\nstderr:\n{err}")`.

Children that get nested under a sequential group must override `get_log_file_address()` and `get_err_file_address()` — otherwise the failure-reporting code itself crashes on the default `NotImplementedError`. `SimpleCMDExecutor` doesn't override them; only short shell snippets should sit in `SimpleCMDExecutor` slots.

Real subclass: `RunSinglePartitionCommand` ([run_single_partition.py](../../../commands/run_single_partition.py)) — discovers per-sampling-unit snapshot files in `_build_children()`, builds a list of `RunIdxCommand` interleaved with `SimpleCMDExecutor("sleep 5")` per idx, calls `super().__init__([])` lazily.

## Concrete commands and their dispatch paths

| Class | File | Path | Notes |
|---|---|---|---|
| `CreateImage` | [createimage.py](../../../commands/createimage.py) | leaf | No `ExperimentContext`. |
| `Boot` | [boot.py](../../../commands/boot.py) | leaf or tmux | `SUPPORTS_INTERACTIVE=True`. Path A bash when `interaction_script` set. |
| `Load` | [load.py](../../../commands/load.py) | leaf or tmux | Same. |
| `InitWarm` | [init_warm.py](../../../commands/init_warm.py) | leaf | Renders Jinja templates inside `cmd()`, not `__init__` (so it works under sub-experiment context mutation). |
| `FunctionalWarming` | [functional_warming.py](../../../commands/functional_warming.py) | leaf | |
| `PartitionCommand` / `CleanPartitionCommand` / `UnPartitionCommand` | [partition.py](../../../commands/partition.py) | leaf | Preconditions checked inside `cmd()`, not `__init__`. |
| `RunIdxCommand` | [run_idx.py](../../../commands/run_idx.py) | leaf | Sentinel basename includes `part<P>_idx<I>` because both fields are set on the cloned context. |
| `RunSinglePartitionCommand` | [run_single_partition.py](../../../commands/run_single_partition.py) | sequential group | Lazy `_build_children()`. |
| `RunPartitionCommand` | [run_partition.py](../../../commands/run_partition.py) | dynamic group | Generates per-partition sub_experiments via `model_copy` + `clone_experiment_context(..., partition_number=p)` inside `execute()`, then dispatches via `_execute_group`. At partition-leaf level, delegates to `RunSinglePartitionCommand` inline. |
| `RunResultCommand` | [result.py](../../../commands/result.py) | leaf | |
| `Multi` | [multinode.py](../../../commands/multinode.py) | leaf | Single-node placeholder; the proper multi-node dispatch is via `sub_experiments` from YAML, not this command. |

## Multi-experiment tree shape (the canonical case)

```
RunPartitionCommand on a multi-node YAML:

  top group ctx (sub_experiments=[node_0, node_1] from YAML)
    └─ Executor._execute_group → mp.Process per node
        │
        ▼  (in each node's process: self.experiment_context = node_n, partition_number==-1)
        RunPartitionCommand.execute()
          dynamically build sub_experiments=[clone(part=0), clone(part=1), ...]
          └─ Executor._execute_group → mp.Process per partition
              │
              ▼  (in each partition's process: partition_number==P)
              RunPartitionCommand.execute() → leaf-level → delegates to:
                RunSinglePartitionCommand(_SequentialGroupExecutor_)
                  iterates idxs sequentially, each via:
                    RunIdxCommand.execute()  ← LEAF: actually runs QEMU bash
                    sentinel basename: RunIdxCommand_part<P>_idx<I>_node<N>
```

So: 2 nodes × 16 partitions × N idxs each. Nodes parallel via mp.Process; partitions parallel via mp.Process; idxs sequential via SequentialGroupExecutor. Every (partition, idx) leaf coordinates master-first with its peer on the other node via the per-(part, idx) sentinel files.

## Driving from the CLI

[qflex](../../../qflex) does the same dance for every subcommand:

```python
executor = SomeCommand(experiment_context=..., ...extras...)
executor.execute(to_stdio=True, run_in_background=False)
```

The result of `execute()` is **discarded** at the CLI layer — failures surface either via the raised `RuntimeError` (from `_execute_group` aggregation or `SequentialGroupExecutor`) or via the child binary's own non-zero exit being visible in stdio.

## Dry-run

Two ways to enable:

1. Pass `dry_run=True` to `execute()`.
2. Set `QFLEX_DRY_RUN=1` (or `true`/`yes`, case-insensitive) in the env. Caller-passed `True` always wins.

The dry-run printer emits a structured block per leaf:

```
[dry-run] <Class> (cwd=<wd>):
  [py-wait]  <sentinel-path>            (one line per upstream node)
  [py-touch] <basename>.started
  [tmux]     would open new window 'Phase-nodeN' and send-keys the bash   (only when interactive_tmux)
  [bash]     <joined cmd, embedded \n collapsed>
  [py-touch] <basename>.done            (subprocess mode)
  [py-poll]  <basename>.done            (tmux mode — Python polls, not touches)
```

Tests parse these markers via [tests/conftest.py](../../../tests/conftest.py) `parse_dry_run_blocks`. See the `testing` skill.

`_execute_group` in dry-run mode iterates sub_experiments **sequentially** in-process (no mp.Process spawn), printing each leaf's block in order. So the dry-run output reflects the tree's dispatch order even though the real run is concurrent.

`compute_runtime_settings()` (the pure part of leaf prep) IS called in dry-run so the rendered bash shows the correct auto-computed telnet ports / auto-flipped flags. The fs side effects (`set_up_folders`, shm cleanup, `qemu_nic` mutation) are still skipped, which means the QEMU args in dry-run still show the default `-nic none` rather than the multi-node `-netdev pdes,...` — pre-existing limitation, not introduced by the dispatch refactor.

`clean_up()` is **not** called on the dry-run path.

## Common pitfalls

- **No more `ParallelExecutor`.** If old code or docs reference it, that's stale — sub_experiments + `_execute_group` is the replacement. Per-partition fan-out in `RunPartitionCommand` does this dynamically.
- **`run_in_background=True` is `NotImplementedError`.** Don't pass it.
- **Don't cache context-derived state in `__init__`.** Multi-experiment dispatch mutates `self.experiment_context` per sub via `mp.Process` pickle. Build per-context state (e.g. `QemuCommonArgParser`) inside `cmd()` so each sub gets its own. Touching this rule was the root cause of every "works for one node, breaks for two" bug encountered during the refactor.
- **`shell=True` everywhere.** Every `execute()` path uses `shell=True`. None of the current commands interpolate user-controlled strings, but if you ever do, quote them.
- **`SequentialGroupExecutor` calls `get_log/err_file_address` on child failure.** Unless the child overrides both, it'll `NotImplementedError` *inside* the failure-reporting code. Either override them in the child or wrap shell-only steps in something other than `SimpleCMDExecutor`.
- **`cwd` is `os.getcwd()` at call time.** Always. If you want a different working dir, prefix the bash string with `cd <path> && ...` like the rest of the repo does.
- **mp.Process pickling.** Concrete executors must be picklable so `_multi_experiment_target` can ship them across the process boundary. Any Pydantic-or-builtin attribute is fine; open file handles / locks would break this. None of the existing executors trip on this today.
- **Tmux session must already exist.** `_execute_in_tmux` raises if `libtmux.Server().sessions` is empty. We never silently spawn a tmux server because the user wouldn't see it.

## Adding a new executor

For a single-step command, copy the boot.py shape — implement `cmd()`, inherit `execute()`:

```python
from commands import Executor
from .config import ExperimentContext

class MyPhase(Executor):
    def __init__(self, experiment_context: ExperimentContext, my_flag: int):
        self.experiment_context = experiment_context
        self.my_flag = my_flag

    def cmd(self) -> str:
        exp = self.experiment_context  # always read current; don't cache from __init__
        return [
            f"cd {exp.get_experiment_folder_address()}/run",
            f"./qemu-system-aarch64 ...",
        ]
```

For a sequential orchestrator with heterogeneous children, subclass `SequentialGroupExecutor` and override `_build_children()`:

```python
class MyOrchestrator(SequentialGroupExecutor):
    def __init__(self, experiment_context, ...):
        super().__init__([])    # children built lazily
        self.experiment_context = experiment_context
        # ... stash flags

    def _build_children(self):
        return [SimpleCMDExecutor("setup"), MyPhase(self.experiment_context, ...), ...]

    def cmd(self) -> str:
        raise NotImplementedError("Use execute().")
```

For a parallel-fan-out orchestrator (the old `ParallelExecutor` use case), don't subclass anything new — populate `experiment_context.sub_experiments` dynamically in `execute()` and let the base `_execute_group` do the work. See `RunPartitionCommand` for the pattern (clone with each axis value, set on the context via `model_copy`, recurse).

If your executor will be nested under `SequentialGroupExecutor`, override `get_log_file_address()` and `get_err_file_address()` to return real paths and have the bash redirect into them — otherwise failure reporting will itself crash.

Expose the new executor as a thin `@app.command()` wrapper in [qflex](../../../qflex) (see the `qflex-commands` and `qflex-cli` skills for the wrapping pattern).

## Key files

- [commands/executer.py](../../../commands/executer.py) — the entire executor hierarchy + dispatch + sentinels + dry-run printer.
- [commands/__init__.py](../../../commands/__init__.py) — re-exports `Executor`. Group classes are imported directly from `.executer` by callers.
- [commands/run_partition.py](../../../commands/run_partition.py) — canonical example of dynamic sub_experiments generation for parallel fan-out.
- [commands/run_single_partition.py](../../../commands/run_single_partition.py) — canonical `SequentialGroupExecutor` subclass with lazy `_build_children`.
- [commands/run_idx.py](../../../commands/run_idx.py) — example of overriding `get_log/err_file_address` for use under a sequential group.
- [tests/](../../../tests/) — assertions about the dispatch + sentinel behaviour. One `test_<phase>.py` per pipeline phase (`test_boot.py`, `test_load.py`, `test_run_partition.py`, `test_run_idx.py`, …), all driven by the dry-run parser in [tests/conftest.py](../../../tests/conftest.py). See the `testing` skill.
- [qflex](../../../qflex) — the per-subcommand `executor.execute(...)` call sites.
