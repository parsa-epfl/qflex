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

## Real-run tests: settings live in YAML, not in python

Real-run tests under `test_real_runs*.py` drive `./dep exec ./qflex <phase> -c <yaml>` against a live dev container. Every knob the test depends on (experiment_name, interaction_script, loadvm_name, use_gdb, …) belongs in the test's YAML, not in a CLI flag the python passes.

The fixture set lives in [tests/realrun/](../../../tests/realrun/):

```
tests/realrun/
├── dc-alpine-login.yaml             # used by test_alpine_login_and_ls_real_run
├── dc-multi-savevm-create.yaml      # used by test_01 (boot+savevm)
├── dc-multi-savevm-verify.yaml      # used by test_02 (load+verify)
├── login_and_ls.exp                 # the expect script for the alpine test
├── boot_create_and_savevm_master.exp
├── boot_create_and_wait.exp
└── load_verify_ls.exp
```

Each YAML `extends: ../../conf/DC/<base>` so production defaults flow through — change `conf/DC/dc-multi.yaml` and every multi-node test follows. The relative path resolves cleanly through [`load_config`](../../../dep_injection/config_loader.py) (it does `path.parent / parent_name.yaml`, which handles `..` segments).

The python test reduces to one line of qflex invocation plus the assertions:

```python
def test_alpine_login_and_ls_real_run(dev_container):
    mounting = dev_container
    r = _exec_in_container("./qflex boot -c tests/realrun/dc-alpine-login.yaml", timeout=900)
    assert r.returncode == 0, ...
    ls_path = f"{mounting}/experiments/{ALPINE_EXPERIMENT_NAME}/ls_output.txt"
    ...
```

No CLI flag overrides, no `--no-gdb`, no `--interaction-script <path>`, no `--experiment-name <name>`. The only python-side constant is whatever the assertions need to read back (e.g. the experiment_name to locate the captured file) — and it's a single literal that mirrors the YAML.

If you want to set something that isn't a YAML knob today (a constructor flag on a phase class), add the field to `ExperimentContext` (and `create_experiment_context`) first so the YAML can drive it. `use_gdb` followed exactly this path: started as a `Boot/Load/InitWarm/FunctionalWarming` constructor flag + a `--gdb / --no-gdb` CLI flag, then got promoted to a context field so test YAMLs can set `use_gdb: false` and the python doesn't carry the flag.

The container side already has `tests/` mounted at `/home/dev/qflex/tests/` (via [commands/docker.py](../../../commands/docker.py)'s `tests_mount`), so YAML paths like `interaction_script: /home/dev/qflex/tests/realrun/<x>.exp` resolve inside the container without further wiring.

## Multi-node real-run tests: making sure both nodes always exit

Multi-node test YAMLs run a leaf per node in parallel. If one node finishes (cleanly or with a bad capture) and the other doesn't notice, the surviving qemu spins forever in PDES sync waiting for the dead peer — the test then sits through pytest's outer 30-min timeout. Two layers of defense ship together; lean on them and don't try to add per-test cross-node coordination on top.

- **Executor `_kill_peer_qemus` (in [commands/executer.py](../../../commands/executer.py))** — symmetric: every leaf, on bash exit, `pkill -9 -f shm-send=/pdes_<neighbor>_to_<my>[part_<P>_][idx_<I>_]` for each of its neighbors. Match scope is per (peer, partition, idx) so concurrent unrelated experiments are unaffected. A `<basename>.killed_by_peer` sentinel is touched so the parent `_execute_group` doesn't treat the SIGKILL'd peer's non-zero rc as a real failure.
- **Expect-script side** — every error branch must call `quit_qemu` before `exit 1`, and `quit_qemu` must short-circuit if the monitor is already gone (`catch {connect_telnet …}; return`). With those two in place, whichever leaf exits first drags the other down within ~5s.

Test fixture-wise, [tests/conftest.py](../../../tests/conftest.py)'s `dev_container` runs `./dep exec --command ./clean_up.sh` immediately after `./dep start-docker` so any leftover qemu / `/dev/shm/pdes_*` from the user's parallel manual runs is swept before the test starts. Don't bypass that — host-level `pkill` on the user's environment is rude.

## Don't wait endlessly when running real tests

For a pass case the test is short (~1-2 minutes); for a stuck case it can sit for the full 30-min `_exec_in_container` timeout. Don't wait passively for the runtime's "command completed" notification — actively check whether the test is making progress. The shape that works:

1. Run the test with **foreground** Bash (NOT `run_in_background: true`, NOT `Monitor`) at the longest reasonable timeout the runtime supports. Foreground returns the captured tool output back to you; `run_in_background` and `Monitor` both write their tracking files into `/tmp/claude-291753/...`, which fills the boot disk and bricks bash with `ENOSPC`.

   ```
   make test-real-one TEST=tests/test_real_runs_savevm.py::test_02_load_two_nodes_verify_files 2>&1 | tail -50
   ```

2. While the test runs, between iterations of waiting, periodically `Read` the existing log files in the experiment folder — they're being written *as the test runs*, no need to add new redirects:
   - `/mnt/sdc/data-caching-1c/experiments/<sub>/Load.log` / `Boot.log` (qemu+gdb stdout)
   - `/mnt/sdc/data-caching-1c/experiments/<sub>/Load.err` / `Boot.err` (qemu+gdb stderr — segfaults land here)
   - `/mnt/sdc/data-caching-1c/experiments/<sub>/expect_log.txt` (full bidirectional dialogue with telnet)
   - `/mnt/sdc/data-caching-1c/experiments/<sub>/ls_after_*.txt` (the captures the test asserts on)

3. Cross-check `ps -ef | grep qemu-system | grep -v grep` and `ls /dev/shm/pdes*`. If the captures are written, qemu is still chewing CPU, and the expect logs show no progress — that's stuck. Diagnose immediately rather than waiting another 30 minutes.

4. NEVER bake any of this polling into the python test code. The python should stay one-line-per-step minimal; the polling lives on the Claude side via Bash + Read.

## Don't write to /tmp / boot disk

This is a footgun that bit this session several times: filling `/tmp` (or any boot-disk path) kills bash with `ENOSPC` and the user has to manually free space before any tool call can succeed. Specific things that do this:

- `cmd > /tmp/log` / `pytest > /tmp/out` redirects you've added.
- `Bash(run_in_background: true)` — the runtime captures the output to `/tmp/claude-291753/...`.
- `Monitor` — same, plus it polls and writes events.

For real-run tests use **foreground** Bash. If you genuinely need to persist intermediate output, redirect into the experiment folder under the user's mount — and check whether the executor / expect script *already* writes what you want there (it usually does: `Load.log`, `Load.err`, `expect_log.txt` are all there for free).

## Tests reuse existing components — never reimplement paths/names

Real-run tests are simple: invoke `./qflex <phase> -c tests/realrun/<x>.yaml`, assert `rc == 0`, and (only if needed) inspect artifacts via the existing helpers on the production code.

**Do not rebuild path / folder / sentinel-name conventions inside the test.** If you find yourself writing something like `f"{mounting}/experiments/{sub}/run/partition_{p}"` in a test, that's a bug — `ExperimentContext.get_partition_folder()` already returns exactly that. Tests pulling those paths together themselves means two implementations to keep in sync; when production renames its layout, the test silently keeps working against the old shape.

Concretely:

* Need a partition folder? `experiment_context.get_partition_folder()`.
* Need the experiment folder? `experiment_context.get_experiment_folder_address()`.
* Need a per-leaf sentinel path? Use the executor's `_sentinel_path()` / `_sentinel_basename()`, not f-strings.
* Need to load a YAML the test depends on? `dep_injection.builder.build_experiment_context(path, component_overrides=...)`. Don't read YAML manually.
* Need to check whether a snapshot exists in a qcow2? `tests/conftest.py::_qcow2_has_snapshot` / `require_snapshot` / `require_boot_login_zstd`. Don't shell out to `qemu-img snapshot -l` from the test body.
* Driving an end-to-end pipeline phase? `_exec_in_container("./qflex <phase> -c <yaml>", timeout=...)`. The CLI command's body is the source of truth for what that phase does — what folders it produces, what files it writes. The test asserts on rc and on helper-derived artifact paths; it does not duplicate the path math.

```python
# bad — reinvents production path layout in the test
def _partition_dir(mounting, sub, p):
    return f"{mounting}/experiments/{sub}/run/partition_{p}"
assert os.path.exists(_partition_dir(mounting, sub, 0))

# good — production already knows where partition 0's folder lives
ctx = build_experiment_context("tests/realrun/dc-multi-run-idx-p0-i0.yaml", ...)
assert os.path.exists(ctx.sub_experiments[0].get_partition_folder())
```

Same rule applies to dry-run tests — when you want to assert on a sentinel file or a leaf log path, route it through the executor's existing path-builders rather than f-string'ing one yourself.

## Common pitfalls

- **Embedded newlines in bash break naive parsers.** `RunIdxCommand`'s bash includes a multi-line gdb python block. The dry-run printer collapses these via `.replace("\n", "\\n")` so each `[bash]` marker stays on one line. Don't undo this — the parser depends on it.
- **`mock_mounting_folder` precondition files.** If you add a new precondition check inside any executor's `cmd()` or `execute()` (e.g. assert some file exists), update the fixture to materialise that file too — otherwise existing tests will break.
- **Tests that override `interactive_tmux=True`** will hit the dry-run path that emits `[tmux]` markers. Don't expect them to actually open windows or call libtmux — the dry-run path skips the libtmux import entirely.
- **The wall-clock test is the only non-dry-run test.** It depends on `fork`-style multiprocessing inheriting monkeypatches. If that ever changes, that test will need restructuring.
- **Don't add tests that require real QEMU / shm / kraken libs.** They won't run in CI or on a contributor's host. Keep verification at the dry-run / monkeypatched level.
