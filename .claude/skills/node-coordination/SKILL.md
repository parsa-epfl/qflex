---
name: node-coordination
description: Use when working on the lifecycle handshake between multi-node leaves — how nodes wait to start, signal that they've started, signal that they're done, wait to exit, and clean each other up. Covers the Python-side `.sentinels/` folder under the group experiment folder (`<group>/.sentinels/<phase>[_part<P>][_idx<I>]_node<N>.{started,done,killed_by_peer}`), the `wait_for_nodes: list[int]` field on `ExperimentContext`, `_wait_for_sentinels` / `_touch_sentinel` / `_kill_peer_qemus` / `_post_exit_grace` in [commands/executer.py](../../../commands/executer.py), the symmetric peer-kill workaround for parallel-qemu's PDES exit bug, and the parallel coordination layer used inside expect scripts (`savevm_done.flag`, `node<N>_at_login.flag` under `<group_folder>/`). TRIGGER when the user mentions wait_for_nodes / `.sentinels/` / `_wait_for_sentinels` / `_touch_sentinel` / `_kill_peer_qemus` / `.killed_by_peer` / `.started` / `.done` / `_post_exit_grace` / "master must boot first" / savevm_done.flag / node<N>_at_login.flag / "how do nodes know peer is up" / "how do nodes know peer is done" / per-leaf ordering invariant. SKIP for the broader PDES wire / WWT / virtual-time topology (multi-node skill), the executor dispatch internals at large (executor skill), or the YAML wiring of `wait_for_nodes` (qflex-dependency-injection skill).
---

# node-coordination — how multi-node leaves synchronise their lifecycle

Multi-node QFlex runs N independent QEMU processes glued together host-side by PDES shared memory. The *guest-time* synchronisation between them is WWT (see the `multi-node` skill). The *wall-clock* synchronisation — when each node may start, when each may quit, and how peers clean each other up if one dies — is the topic of this skill.

There are **three layers** of coordination, each with its own sentinel files in its own location:

| Layer | Where it lives | Filenames | Owner |
|---|---|---|---|
| Python executor (per-leaf) | `<group_folder>/.sentinels/` | `<phase>[_part<P>][_idx<I>]_node<N>.{started,done,killed_by_peer}` | [commands/executer.py](../../../commands/executer.py) |
| Expect scripts (per-savevm) | `<group_folder>/` | `node<N>_at_login.flag`, `node<N>_savevm_done.flag`, `savevm_done.flag` | [tests/realrun/*.exp](../../../tests/realrun/) |
| Bash group cleanup (post-exit) | (no files; `pkill` + 30 s sleep) | n/a | `Executor._post_exit_grace` + `_kill_peer_qemus` |

`<group_folder>` here means the **outermost group's** experiment folder — `<mounting>/experiments/<top_experiment_name>/`. For `dc-multi.yaml` that resolves to `<mounting>/experiments/data-caching-comparison/`. Per-leaf sub-experiments (`-node-0`, `-node-1`) put their OWN files inside their own per-leaf folders, but the inter-node sentinels deliberately live one level UP so every leaf agrees on a single path. The expect-side scripts compute `set group_folder [file dirname $exp_folder]` to find this same path from inside the leaf's `EXP_FOLDER`.

## Layer 1 — Python executor: `wait_for_nodes` + `.sentinels/`

This is the load-bearing layer. **The other two layers exist to plug specific gaps in this one.**

### Field

`ExperimentContext.wait_for_nodes: list[int]` — the `node_number`s of upstream peers this leaf must observe as *started* before it begins its own bash. Master typically has `[]`; node 1 has `[0]`; longer chains can specify `[0, 2]`, etc. Set per-leaf in YAML (see [conf/DC/dc-multi.yaml](../../../conf/DC/dc-multi.yaml)). Empty list = no waiting.

### Files

Sentinel basenames are computed by `Executor._sentinel_basename(node_number)`:

```
<phase>[_part<P>][_idx<I>]_node<N>.<suffix>
```

* `<phase>` = `self._phase_name()`, defaulting to the executor's class name (`Boot`, `Load`, `FunctionalWarming`, `RunIdxCommand`, …).
* `_part<P>` is appended when the leaf's `experiment_context.partition_number >= 0`. Skipped on the multi-node leaves of `Boot`/`Load`/`FW` (those don't have a partition axis).
* `_idx<I>` is appended when `experiment_context.idx >= 0`. Only the timing-phase `RunIdxCommand` leaves carry this.
* `<suffix>` ∈ `started`, `done`, `killed_by_peer`.

So one timing-phase per-(node, partition, idx) leaf produces sentinels like `RunIdxCommand_part0_idx2_node1.started`. The matching `node_number` field in the basename is the node that *owns* the sentinel — its absence/presence is what other leaves test against.

### Lifecycle (in `Executor._execute_leaf`)

The base executor's leaf path runs in this exact order:

1. **Default log paths** — fill in `log_path` / `err_path` from `<exp_folder>/<phase>.{log,err}` if not already set.
2. **Leaf-level prep** — `experiment_context.prepare_for_execution()` runs *first* (folder setup, image cp, NIC args). Done **before** any sentinel waiting so multi-node followers can do their qcow2 cp in parallel with the master's prep.
3. **Wait for upstream `.started`** — `_wait_for_sentinels(sentinel_dir)` polls every 0.5 s for each `node_number` in `wait_for_nodes`, looking for `<sentinel_dir>/<basename>.started`. No timeout (intentional — outer pytest / `_exec_in_container` timeout is the bound).
4. **Touch own `.started`** — `_touch_sentinel(sentinel_dir, "started")` writes `<basename>.started`. From this point on, downstream peers (whose `wait_for_nodes` includes our number) will unblock.
5. **Run the bash** — `subprocess.run(["bash", "-c", arg], ...)` (Force `bash`, not `/bin/sh`, so `>(tee ...)` process substitution works.)
6. **Post-exit grace** — `_post_exit_grace(exp, returncode)` (multi-node only): print a `[executor] node N: bash exited rc=…; waiting 30s grace…` banner and `time.sleep(30)`. Gives peers' QEMUs a beat to finish post-savevm finalisation and graceful `quit_qemu` before the symmetric kill below pkills them.
7. **Symmetric peer-kill** — `_kill_peer_qemus(sentinel_dir)`: for each `neighbor` in `neighbor_node_list`, `pkill -9 -f shm-send=/pdes_<neighbor>_to_<self>[part_<P>_][idx_<I>_]` and write `<basename(neighbor)>.killed_by_peer`. See "Why the symmetric kill exists" below.
8. **Touch own `.done`** — only if `subprocess.run` returned 0. A failed bash leaves no `.done`.

### Group dispatch (in `Executor._execute_group`)

The fanout side handles the sentinel directory lifecycle:

* **Outermost group** sets `sentinel_dir = "<group_folder>/.sentinels"` and `shutil.rmtree(sentinel_dir)` + recreate at start. Owner-only — nested groups inherit the parent's dir (so `RunPartitionCommand`'s per-partition sub-fanout doesn't blow away the per-node wait sentinels).
* Each child leaf is spawned via `mp.Process(target=_multi_experiment_target, args=(self, sub, kw))`. The mp.Process gets a pickled copy of the parent executor; the target sets `executor.experiment_context = sub` and re-enters `executor.execute()`.
* On join, the parent counts a `p.exitcode != 0` child as a *real* failure ONLY if the corresponding `<basename>.killed_by_peer` is missing. Killed-by-peer leaves don't count against `RuntimeError` — they're expected casualties of the symmetric kill above.

### Why the symmetric kill exists

`parallel-qemu`'s PDES exit handling (`net/pdes-engine.c`) has a known bug: when one node's QEMU exits, peers don't follow — they spin in WWT sync waiting forever. Without the kill, `pytest` / `./qflex` would block until the outer 30-min `_exec_in_container` timeout. The symmetric kill is the *temporary workaround* until the engine is fixed; do **not** undo it, and do **not** paper over its symptoms in expect scripts. Scope: the `pkill -9 -f shm-send=/pdes_<peer>_to_<self>[…]` pattern includes the `part_<P>_idx_<I>_` suffix when set, so unrelated experiments and unrelated (partition, idx) leaves on the same host are NOT touched.

The `<basename>.killed_by_peer` file is the executor's contract that "this peer's exit code 137 was caused by us, not by the bash failing on its own merits."

## Layer 2 — Expect scripts: per-node sentinels around savevm

Path A interaction scripts (under [tests/realrun/](../../../tests/realrun/)) need cross-node coordination at points the executor doesn't see — specifically *inside* a single leaf's bash, while QEMU is still running. There are two patterns in the tree, used by two different init/snapshot flows:

### Init flow: `node<N>_at_login.flag` + `node<N>_savevm_done.flag` (deterministic)

Used by `boot_login_savevm.exp` (master / single-node) + `boot_login_wait.exp` (follower). The init flow is the canonical example — fully deterministic, no sleeps, two-way handshake:

* Each non-master follower touches `<group_folder>/node<N>_at_login.flag` as soon as it sees `[Ll]ogin: ` on serial.
* Master polls for the expected follower's flag (currently hard-coded `node1_at_login.flag` for the 2-node case) before sending `delvm` + `savevm` on its monitor.
* Master sends `savevm`, then `expect "(qemu) "` — that prompt comes back AFTER master's local `save_snapshot()` returns (i.e. after master's `savevm log 7` print).
* Follower's per-node savevm runs in parallel as part of the same PDES drain and finishes a beat later. Follower's expect tails its own `Boot.log` (which captures qemu's stdout via the bash `> Boot.log` redirect) for the literal substring `savevm log 7` — the last printf inside `save_snapshot()` in [parallel-qemu/migration/savevm.c](../../../parallel-qemu/migration/savevm.c). Seeing it in the file means the follower's local savevm finalised.
* Follower writes `<group_folder>/node<N>_savevm_done.flag`.
* Master polls for that flag with `BUDGET_CHECKPOINTING_S` budget; once it appears, master sends `quit` over its monitor.
* Follower runs `quit_qemu` after writing its done flag.

Both nodes' qcow2s are guaranteed to have the snapshot fully written before either process exits. The `_post_exit_grace` (Layer 3) is a backstop, not the primary mechanism.

### Older flow: `savevm_done.flag` (master fires; follower polls)

Used by `boot_create_and_savevm_master.exp` (master-only) + `boot_create_and_wait.exp` (follower) for the savevm-create test. Older pattern, kept because that test also does post-savevm `ls` capture from each leaf:

* Master: connect monitor, `delvm` + `savevm` (`(qemu)` prompt back), then `set f [open $savevm_done_flag "w"]; close $f` — single global flag at `<group_folder>/savevm_done.flag`. Then post-savevm serial `\r` + `ls` capture, then quit.
* Follower: poll for the global flag with `wait_for_savevm_done`. Then post-savevm serial `\r` + `ls` capture (relies on the master having finished savevm first so the captured `ls` reflects the snapshotted state). Then quit.

The 2 s heuristic that previously bridged the master-savevm-vs-follower-savevm gap has been removed in favour of the deterministic per-node-flag handshake in the init flow above. The savevm-create flow is somewhat protected because each follower waits for the global flag (which is written *after* master's own savevm completes), but it's still possible for the follower's *own* savevm to be in flight when it tries to read the next post-`\r` prompt over serial. The post-savevm serial-prompt expect was bumped to `BUDGET_CHECKPOINTING_S` in `boot_create_and_savevm_master.exp` for exactly that reason; see the TODO note below for the proper unification.

Master-only-issues-savevm is a hard rule — wiring the same `interaction_script` to every leaf when a snapshot is involved produces a corrupt (double-driven) snapshot.

### Where the flags live

Always `<group_folder>/<flag>`. Each script computes:

```tcl
set group_folder [file dirname $exp_folder]
```

`$exp_folder` is `EXP_FOLDER`, the per-leaf experiment folder set by `boot.py`/`load.py`'s env-var prelude. `dirname` of e.g. `<mounting>/experiments/qflex_test_multi-node-0` gives `<mounting>/experiments/`, which means **all multi-node tests using the same `<mounting>` share this directory** — the prefix on the flag name matters (`node<N>_…` for per-node flags, plain global names like `savevm_done.flag` for cross-node). Stale flags from a prior run are deleted at script startup by whichever script writes them.

### Open issue (TODO)

`boot_create_and_savevm_master.exp` + `boot_create_and_wait.exp` (savevm-create test) still use the older single-global-flag pattern. They should be migrated to the deterministic `node<N>_savevm_done.flag` handshake the init flow uses. The blocker is that the savevm-create scripts also do a post-savevm `ls` capture, so the migration needs to move that capture under the new flag instead of the global one. Tracked separately.

## Layer 3 — Bash group post-exit grace + symmetric peer-kill

This is the layer wrapping everything when a leaf's bash exits. The post-exit logic is in `Executor._execute_leaf`:

```
subprocess.run(["bash", "-c", arg], ...)
  → _post_exit_grace(exp, returncode)        # 30 s sleep, multi-node only
  → _kill_peer_qemus(sentinel_dir)           # symmetric pkill, writes .killed_by_peer
  → clean_up()
```

The 30 s grace plus the killed-by-peer marker *together* let one node quit cleanly while the other is still finishing its end of a coordinated operation. Without the grace, master's `quit` → bash exit → `_kill_peer_qemus` happens too quickly and SIGKILLs the follower mid-savevm-finalisation, sometimes corrupting the per-node qcow2. With it, the follower has time to either complete naturally or be cleanly noticed as `.killed_by_peer`.

`POST_EXIT_GRACE_SECONDS` is a class attribute on `Executor` so tests can monkeypatch it to 0 (e.g. [tests/test_run_partition.py](../../../tests/test_run_partition.py)`::test_run_partition_runs_partitions_in_parallel` uses real subprocesses and wall-clock asserts — paying 30 s per leaf would blow the budget and isn't what the test is exercising).

## Adding new coordination

If you find yourself reaching for a new sentinel:

1. **Per-leaf lifecycle** (start / done / failed) — extend Layer 1. Add a new suffix to `_sentinel_basename` callers, or add a new wait method on `Executor`. Don't write your own sentinel files in expect scripts for things that should be the executor's job.
2. **Inside a single leaf's bash, between leaves** — Layer 2. Put the file under `<group_folder>/` (not under a per-leaf folder), prefix the name with `node<N>_` if it's per-node, and *always* delete-on-startup the version your script will (re)write so a stale flag from a prior run can't unblock anyone.
3. **Cleanup-on-exit** — Layer 3. Don't add `pkill` to bash; the executor owns it.

## Key file references

* [commands/executer.py](../../../commands/executer.py) — `_sentinel_basename`, `_wait_for_sentinels`, `_touch_sentinel`, `_kill_peer_qemus`, `_post_exit_grace`, `KILLED_BY_PEER_SUFFIX`.
* [commands/config.py](../../../commands/config.py) — `wait_for_nodes` field on `ExperimentContext`; `is_master_node()` / `is_multi_node()`.
* [conf/DC/dc-multi.yaml](../../../conf/DC/dc-multi.yaml) — canonical wiring: master sets `wait_for_nodes: []` (omitted = `[]`), node 1 sets `wait_for_nodes: [0]`.
* [tests/realrun/boot_login_savevm.exp](../../../tests/realrun/boot_login_savevm.exp) / [boot_login_wait.exp](../../../tests/realrun/boot_login_wait.exp) — Layer 2 reference scripts (init).
* [tests/realrun/boot_create_and_savevm_master.exp](../../../tests/realrun/boot_create_and_savevm_master.exp) / [boot_create_and_wait.exp](../../../tests/realrun/boot_create_and_wait.exp) — Layer 2 reference scripts (savevm-create test).
* [tests/conftest.py](../../../tests/conftest.py) — `assert_two_node_master_first` helper that asserts the Layer-1 wait/touch invariants on dry-run output.

## See also

* `multi-node` skill — broader topology (PDES wire, WWT, virtual-time sourcing, distributed snapshots). This skill is a deep-dive on one slice of that picture.
* `executor` skill — the dispatch internals (`_execute_group` / `_execute_leaf`, mp.Process, the `execute()` flag matrix). Layer 1 details live there too, from a different angle.
* `boot-load-interactive` skill — the Path A / Path B mechanics on Boot/Load. Layer 2's expect-side primitives (`savevm_done.flag`, `node<N>_at_login.flag`) live in those scripts.
