---
name: testing
description: Use when adding to or debugging the pytest suite under [tests/](../../../tests/), or when the user asks how to verify a behaviour change in the dispatch / sentinel / boot-load-interactive paths. Covers the `make test` Makefile target, the per-component file layout (one `test_<phase>.py` per pipeline phase), the dry-run-stdout-parser approach in [tests/conftest.py](../../../tests/conftest.py) (`parse_dry_run_blocks`, `DryRunBlock`, `capture_dry_run_stdout`, `mock_mounting_folder`, `multi_context`, `login_ls_script` fixtures + the shared `pin_per_sub` and `assert_two_node_master_first` helpers), the `[py-wait]`/`[py-touch]`/`[bash]`/`[tmux]`/`[py-poll]` markers the executor's dry-run printer emits, the wall-clock parallelism test pattern, and the real-run test class in [tests/test_real_runs.py](../../../tests/test_real_runs.py) (gated behind `QFLEX_REAL_RUN_TESTS=1`, drives `./dep` for actual docker-based execution). TRIGGER when the user asks about the test suite, how to write a new test, how to assert master-first ordering, the dry-run parser, the partition-axis parallelism test (real subprocess + monkeypatched leaves), `assert_two_node_master_first`, or how to add a real-run test. SKIP for the executor dispatch internals themselves (executor skill), what specific commands do (qflex-commands skill), or how `./dep` itself works (dep skill).
---

# Testing — pytest + dry-run output parser

Tests live under [tests/](../../../tests/), one file per pipeline phase. They're driven by a parser that consumes the executor's dry-run stdout and asserts on the structured markers it prints. Run with `make test` (or directly: `python -m pytest tests/ -v`).

The whole strategy hinges on the dry-run printer in [`Executor._print_dry_run_actions`](../../../commands/executer.py) emitting one structured block per leaf. Tests don't actually invoke QEMU — they capture the stdout, parse the markers, and assert the dispatch order / sentinel basenames / bash content match expectations.

A separate set of **real-run** test files (`tests/test_real_runs*.py`) bring up the dev container via `./dep` and exec real commands inside it. Those are gated behind `QFLEX_REAL_RUN_TESTS=1` so the default `make test` skips them.

## Files

| Path | Purpose |
|---|---|
| [Makefile](../../../Makefile) | `test:` → all tests; `test-verbose:` → `-v -s --tb=long`; `test-real-one TEST=<nodeid>` → single test/file with `QFLEX_REAL_RUN_TESTS=1` set. |
| [tests/__init__.py](../../../tests/__init__.py) | Empty — makes `tests` a package. |
| [tests/conftest.py](../../../tests/conftest.py) | Dry-run parser (`parse_dry_run_blocks`, `DryRunBlock`), stdout capture (`capture_dry_run_stdout`), fixtures (`mock_mounting_folder`, `multi_context`, `login_ls_script`), shared helpers (`pin_per_sub`, `assert_two_node_master_first`). Also the **real-run helpers**: `_docker_available`, `_qflex_image_present`, `_exec_in_container`, and the session-scoped `dev_container` fixture (start `qflex_test` in background, yield mounting folder, stop on teardown). |
| [tests/test_boot.py](../../../tests/test_boot.py) | Boot phase: vanilla two-node, Path A (interaction_script) for single-node + two-node, Path B (interactive_tmux). |
| [tests/test_load.py](../../../tests/test_load.py) | Load phase: vanilla two-node + Path A. |
| [tests/test_init_warm.py](../../../tests/test_init_warm.py) | InitWarm phase. |
| [tests/test_functional_warming.py](../../../tests/test_functional_warming.py) | FunctionalWarming phase + the `SUPPORTS_INTERACTIVE` gate (FW must ignore `interactive_tmux`). |
| [tests/test_partition.py](../../../tests/test_partition.py) | PartitionCommand / CleanPartitionCommand / UnPartitionCommand. |
| [tests/test_result.py](../../../tests/test_result.py) | RunResultCommand. |
| [tests/test_run_idx.py](../../../tests/test_run_idx.py) | RunIdxCommand — per-(partition, idx) sentinel coordination at the leaf. |
| [tests/test_run_single_partition.py](../../../tests/test_run_single_partition.py) | RunSinglePartitionCommand — sequential idxs within a partition. |
| [tests/test_run_partition.py](../../../tests/test_run_partition.py) | RunPartitionCommand — full multi-node × per-partition × per-idx tree, plus the wall-clock parallelism test. |
| [tests/test_real_runs.py](../../../tests/test_real_runs.py) | Real docker-based runs (skipped by default). Smoke + `./qflex --help` + alpine-login-and-ls. |
| [tests/test_real_runs_savevm.py](../../../tests/test_real_runs_savevm.py) | Real two-node `boot` (savevm-create) → `load` (savevm-verify). Skipped by default. |
| [tests/test_real_runs_docker_image.py](../../../tests/test_real_runs_docker_image.py) | Real assertions on the dev image itself: `iputils-ping`, `perf` shim, `rustc`/`cargo`, both `inferno-*` binaries, and that every host-discovered DNS server / search domain (via `commands.docker._read_host_dns`) propagates into the container's `/etc/resolv.conf`. Skipped by default. |

**One file per phase / component.** When adding tests for a new phase, create a new `test_<phase>.py` rather than appending to an existing one — keeps each file focused and easy to scan.

## The dry-run printer's output shape

Every leaf execution that hits `_execute_leaf` (or `_execute_in_tmux`) in dry-run mode prints a block like:

```
[dry-run] <Class> (cwd=<wd>):
  [py-wait]  <full sentinel path>          # one per upstream node in wait_for_nodes
  [py-touch] <basename>.started
  [tmux]     would open new window 'Phase-nodeN' and send-keys the bash    # only when interactive_tmux
  [bash]     <joined cmd, embedded \n collapsed>
  [py-touch] <basename>.done               # subprocess mode
  [py-poll]  <basename>.done               # tmux mode (Python polls, doesn't touch)
```

`_execute_group` in dry-run **iterates sub_experiments sequentially in-process** (no mp.Process spawn), so the order of printed blocks reflects the dispatch order. For 2 nodes × 2 partitions × 2 idxs you'll see 8 leaf blocks in the master-first, partition-major, idx-minor order.

## Parser: `parse_dry_run_blocks`

Returns a list of `DryRunBlock(cls_name, waits, started, bash, done, tmux_window, raw)`. Each block corresponds to one `[dry-run] <Class>` header plus its indented marker lines.

Important parser details:

- Lines starting with `[dry-run]` start a new block; subsequent lines starting with two spaces are part of it; anything else ends it.
- `[py-poll]` paths land in `done` (tmux mode "polls" instead of "touches", but conceptually it's the same .done sentinel).
- `[tmux]` lines extract the window name from `'<name>'`.
- The bash line collapses embedded newlines to `\n` before printing — the parser sees one logical line.

Helper properties on `DryRunBlock`:

```python
b.waits_basenames   → ["RunIdxCommand_part5_idx3_node0.started", ...]
b.started_basename  → "RunIdxCommand_part5_idx3_node1.started"
b.done_basename     → ".done" suffix
```

Use these to assert sentinel names without coupling to absolute paths.

## Stdout capture: `capture_dry_run_stdout`

Context manager that sets `QFLEX_DRY_RUN=1` in the environment, redirects stdout to an `io.StringIO`, and restores both on exit. Yields the buffer:

```python
with capture_dry_run_stdout() as buf:
    Boot(experiment_context=top).execute()
blocks = parse_dry_run_blocks(buf.getvalue())
```

## Fixtures: `mock_mounting_folder` and `multi_context`

`mock_mounting_folder` (per-test `tmp_path`-rooted): pre-populates the per-node experiment folders (`run/`, `cfg/`, `scripts/`, `partition.py`, `result.py`, `run_partitions.sh`) plus 2 partitions × 2 snapshot files. Phases that have init-time precondition asserts will be happy without anyone needing to run a real boot/fw/partition first.

`multi_context`: builds [conf/DC/dc-multi.yaml](../../../conf/DC/dc-multi.yaml) via `dep_injection.builder.build_experiment_context`, passing `component_overrides` so all three components (the unnamed group + the two named subs) point their `mounting_folder` and `image_folder` at the temp dir.

For tests that need to override leaf fields (e.g. set `partition_number=5, idx=3` for a `RunIdxCommand` test, or set `interaction_script="./drive.exp"` for a Path A test), use the `pin_per_sub` helper in [tests/conftest.py](../../../tests/conftest.py) which clones each sub via `model_copy(update=...)` and rebuilds the top group. Used by `test_boot.py`, `test_load.py`, `test_functional_warming.py`, etc.

## The master-first ordering helper

`assert_two_node_master_first(blocks, leaf_cls_name, expected_basename_prefix)` is the workhorse assertion shared across phases. It:

1. Filters `blocks` to only the leaves of the given class.
2. Pairs them by sentinel-key (everything before `_node<N>`).
3. For each pair, asserts:
   - master leaf has empty `waits`, valid `.started`/`.done` ending in `_node0.*`.
   - non-master leaf waits on exactly one sentinel — the master's matching `.started`.
   - master's block index < non-master's (dispatch order).

This is the single invariant every multi-node test needs. Phase tests just call `assert_two_node_master_first(blocks, "Boot", "Boot_node")`, etc.

## Wall-clock parallelism test (`test_run_partition_runs_partitions_in_parallel`)

The dry-run path can't observe parallelism — it iterates sequentially in-process. To prove that `RunPartitionCommand` actually fans the partition axis out concurrently via `mp.Process`, this single test runs in **non-dry-run mode** with three monkeypatches:

- `ExperimentContext.prepare_for_execution` → no-op (so we don't need real QEMU binaries / shm).
- `ExperimentContext.clean_up` → no-op.
- `RunIdxCommand.cmd` → return `["sleep 1"]`.
- `SimpleCMDExecutor.cmd` → return `:` (shell no-op; suppresses the inter-idx 5-second sleep).

Then it picks one sub-experiment (the master), runs `RunPartitionCommand(...).execute()`, and times the wall-clock elapsed. With 2 partitions × 2 idxs = 4 leaves and `sleep 1` per leaf:

- Parallel partitions, sequential idxs within → ~2 seconds.
- Sequential partitions → ~4 seconds.

The assertion is `1.5s < elapsed < 3.5s`. Comes in at ~2s reliably.

The fork-vs-spawn caveat: monkeypatching survives across mp.Process on Linux (default `fork`), but not on macOS/Windows (default `spawn`). If we ever need cross-platform tests we'd switch to a different strategy (e.g. wrap the leaf bash itself rather than monkeypatching the class).

## Cardinal rule: tests exercise the code under test

Tests must drive the code we ship — never duplicate it.

- **Dry-run / unit tests** import the relevant class from [commands/](../../../commands/) (or the factory `create_experiment_context`) directly and call `executor.execute()` under `capture_dry_run_stdout()`. The dispatch path being tested is the same one production hits.
- **Real-run tests** drive the CLI: `./qflex <subcommand> -c <yaml>` via `_exec_in_container`, or — for host-side workflows — run the actual `./dep` Typer commands. They walk the full `data_class_wrap → create_experiment_context → Executor.execute → cmd() → subprocess.run` pipeline.
- **Never invoke a binary directly from a test or fixture** to "probe" capabilities (e.g. running `qemu-system-aarch64 -netdev pdes,...,latencyns=...` to gate a skip). That duplicates parsing/wiring logic in a place that can disagree with the real code path — and when it does disagree, the test silently lies. If a binary is stale, the *real* run will fail with the *real* error message; that's the signal you want. Gate skips on env vars, docker availability, or repo state — not on subprocess outputs of the very thing you're testing.
- The same rule applies to "test helpers" that build commands by hand: if a helper duplicates what `cmd()` would emit, it'll drift from the real implementation. Either call the class or assert against its `cmd()` output.

## Adding a new test

For a new pipeline phase (or any executor that follows the dispatch contract):

```python
def test_my_phase_two_nodes(multi_context):
    from commands.my_phase import MyPhase
    with capture_dry_run_stdout() as buf:
        MyPhase(experiment_context=multi_context, my_extra_flag=...).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "MyPhase", "MyPhase_node")
```

If your phase has init-time preconditions (file existence checks etc.), add them to `mock_mounting_folder` so every test that uses the fixture has them satisfied — keep test setup centralised.

For a phase that needs specific leaf fields (e.g. `interaction_script`, `partition_number`):

```python
top = _pin_per_sub(multi_context, interaction_script="./drive.exp")
# or
top = multi_context.model_copy(update={"sub_experiments": [
    s.model_copy(update={"partition_number": 0, "idx": 1})
    for s in multi_context.sub_experiments
]})
```

Then assert what your phase-specific bash should contain in `block.bash`, the sentinel basename via `block.started_basename`, etc.

For tmux-mode tests, assert `block.tmux_window == "<Phase>-node<N>"` and that the bash ends with `; touch <done>` (the executor appends this so the Python poll knows when QEMU exits).

## Common pitfalls

- **Embedded newlines in bash break naive parsers.** `RunIdxCommand`'s bash includes a multi-line gdb python block. The dry-run printer collapses these via `.replace("\n", "\\n")` so each `[bash]` marker stays on one line. Don't undo this — the parser depends on it.
- **`mock_mounting_folder` precondition files.** If you add a new precondition check inside any executor's `cmd()` or `execute()` (e.g. assert some file exists), update the fixture to materialise that file too — otherwise existing tests will break.
- **Tests that override `interactive_tmux=True`** will hit the dry-run path that emits `[tmux]` markers. Don't expect them to actually open windows or call libtmux — the dry-run path skips the libtmux import entirely.
- **The wall-clock test is the only non-dry-run test.** It depends on `fork`-style multiprocessing inheriting monkeypatches. If that ever changes, that test will need restructuring.
- **Don't add tests that require real QEMU / shm / kraken libs.** They won't run in CI or on a contributor's host. Keep verification at the dry-run / monkeypatched level.
