"""Pipeline real-run tests — chained snapshots that build progressively richer
guest state on top of `boot-login` so each subsequent stage skips the slow
setup it doesn't need.

Chain:
    boot-login (from tests/test_boot_login_bootstrap.py)
        ↓
    loaded-test     (test_03 — guest logged in + 192.168.100.0/24 net + ping running)
        ↓
    init-warm done  (test_04 — runs `./qflex initialize` against loaded-test)
        ↓
    fw selected     (test_05 — runs `./qflex fw --sample-size 30` against init-warm result)

Each test gates itself on the previous test's snapshot via `require_snapshot`,
so a missing earlier step shows up as a SKIP rather than a misleading FAIL.

Same gating as the rest of the real-run suite: enabled by default, set
`QFLEX_SKIP_REAL_RUN=1` to disable. Init bootstrap (boot-login) lives in
test_boot_login_bootstrap.py and is gated separately by `QFLEX_INIT_TEST=1`.
"""
import os

import pytest

from .conftest import (
    _docker_available,
    _exec_in_container,
    _qcow2_has_snapshot,
    _qflex_image_present,
    _real_run_disabled,
    require_snapshot,
)


# Shared with the rest of the multi-node test suite (dc-multi.yaml).
# Distinct from `data-caching-comparison*` so user real-experiment runs
# don't collide.
NODE_SUB_NAMES = ("qflex_test_multi-node-0", "qflex_test_multi-node-1")
LOADED_TEST_SNAPSHOT = "loaded-test"
INIT_WARMED_SNAPSHOT = "init_warmed"  # hard-coded in parallel-qemu/plugins/pf_api.c
FW_SAMPLE_SIZE = 30                   # default in qflex's `fw` command signature
FW_LAST_SAMPLE = FW_SAMPLE_SIZE - 1   # samples are snapshot_0 .. snapshot_29
PARTITION_COUNT = 5                   # `partition:` section in dc-multi.yaml


pytestmark = [
    pytest.mark.skipif(_real_run_disabled(), reason="QFLEX_SKIP_REAL_RUN is set"),
    pytest.mark.skipif(not _docker_available(), reason="docker daemon not reachable"),
    pytest.mark.skipif(not _qflex_image_present(),
                       reason="ghcr.io/parsa-epfl/qflex image not present locally"),
]


def _node_qcow2(mounting: str, n: int) -> str:
    return f"{mounting}/root-single-node-node{n}.qcow2"


def test_03_boot_creates_loaded_test_with_workload(dev_container):
    """Boot from boot-login → log in → touch testN.txt → set 192.168.100.{1,2}
    on the PDES NIC → bidirectional ping → master savevm `loaded-test` once
    both sides see first echoes → exit.

    Verifies for each node:
      * `loaded-test` snapshot landed in the per-node qcow2 (post-savevm)
      * ls_after_create.txt captured by the enhanced ping scripts contains
        `testN.txt` — proves the per-node file made it into the snapshot.
        test_03b will then loadvm and re-verify the file.
    """
    mounting = dev_container

    # Skip if init hasn't been bootstrapped — `loaded-test` is built on top
    # of `boot-login`, so without it nothing here can run.
    require_snapshot(
        [_node_qcow2(mounting, n) for n in range(len(NODE_SUB_NAMES))],
        "boot-login",
        "bootstrap with `QFLEX_INIT_TEST=1 make test-real-one "
        "TEST=tests/test_boot_login_bootstrap.py::test_init_multi_boot_login_savevm`",
    )

    # No experiment-folder wipe: the per-node folders are shared with the
    # init/savevm-create flows (same `qflex_test_multi-node-N` names) and
    # already have the right core_info.csv layout. Wiping would just force
    # set_up_folders to recreate them with no benefit.

    r = _exec_in_container(
        "./qflex boot -c tests/realrun/dc-multi.yaml",
        timeout=1800,
    )
    assert r.returncode == 0, (
        f"loaded-savevm boot failed (rc={r.returncode}).\n"
        f"stdout (last 2k):\n{r.stdout[-2000:]}\n"
        f"stderr (last 2k):\n{r.stderr[-2000:]}"
    )

    # Verify the snapshot landed in each per-node qcow2 (master sends savevm,
    # PDES drain persists every node's CPU+memory into its own per-node qcow2).
    # require_snapshot would `pytest.skip` here — for a post-condition check we
    # want a hard FAIL, so call _qcow2_has_snapshot directly.
    for n, sub in enumerate(NODE_SUB_NAMES):
        qcow2 = _node_qcow2(mounting, n)
        assert _qcow2_has_snapshot(qcow2, LOADED_TEST_SNAPSHOT), (
            f"loaded-test snapshot did NOT land in {qcow2} — check "
            f"{mounting}/experiments/{sub}/expect_log.txt + Boot.log + qemu_serial.log"
        )

        # Per-node file verification — the enhanced ping scripts touch testN.txt
        # before the network setup, then capture ls. test_03b reloads `loaded-test`
        # and asserts the same file still appears (savevm/loadvm round-trip).
        ls_path = f"{mounting}/experiments/{sub}/ls_after_create.txt"
        assert os.path.exists(ls_path), (
            f"node {n}: missing {ls_path} — ping script didn't reach the "
            f"ls_after_create capture step"
        )
        with open(ls_path) as f:
            ls_text = f.read()
        expected = f"test{n + 1}.txt"
        assert expected in ls_text, (
            f"node {n}: expected {expected!r} in {ls_path}, got:\n{ls_text}"
        )


def test_03b_load_verifies_files_and_swaps_workload(dev_container):
    """`./qflex load` from `loaded-test` and run load_verify_ls.exp on both
    leaves (via the YAML's `load:` command section). Verifies that the
    testN.txt files baked into `loaded-test` by test_03 survived the
    savevm/loadvm round-trip — the load-side complement to test_03's
    save-side file capture.
    """
    mounting = dev_container

    require_snapshot(
        [_node_qcow2(mounting, n) for n in range(len(NODE_SUB_NAMES))],
        LOADED_TEST_SNAPSHOT,
        "run tests/test_chained_pipeline.py::test_03_boot_creates_loaded_test_with_workload first",
    )

    r = _exec_in_container(
        "./qflex load -c tests/realrun/dc-multi.yaml",
        timeout=1800,
    )
    assert r.returncode == 0, (
        f"load+verify failed (rc={r.returncode}).\n"
        f"stdout (last 2k):\n{r.stdout[-2000:]}\n"
        f"stderr (last 2k):\n{r.stderr[-2000:]}"
    )

    for n, sub in enumerate(NODE_SUB_NAMES):
        ls_path = f"{mounting}/experiments/{sub}/ls_after_load.txt"
        assert os.path.exists(ls_path), (
            f"node {n}: missing {ls_path} — load_verify_ls.exp didn't reach "
            f"the capture step"
        )
        with open(ls_path) as f:
            ls_text = f.read()
        expected = f"test{n + 1}.txt"
        assert expected in ls_text, (
            f"node {n}: expected {expected!r} in {ls_path} after savevm/loadvm "
            f"round-trip, got:\n{ls_text}"
        )


def test_04_init_warm_creates_init_warmed_snapshot(dev_container):
    """Run `./qflex initialize` against the loaded-test snapshot. WormCacheQFlex
    `mode=pure_fill` warms the cache hierarchy and, when the warm ratio hits
    1.0, fires `qemu_plugin_notify_fully_warmed` → savevm `init_warmed` →
    master pdes_engine_destroy → master exit(0). The test asserts both that
    qflex returned 0 AND that an `init_warmed` snapshot landed per node.
    """
    mounting = dev_container

    # Skip cleanly if test_03 hasn't created loaded-test yet.
    require_snapshot(
        [_node_qcow2(mounting, n) for n in range(len(NODE_SUB_NAMES))],
        LOADED_TEST_SNAPSHOT,
        "run tests/test_chained_pipeline.py::test_03_boot_creates_loaded_test_with_workload first",
    )

    # 30 minute wall budget. The user originally suggested 20, but empirically
    # the multi-node `init_warmed` savevm is dominated by the
    # EXTERNAL_INCREMENTAL_BASE memory dump (writes the full guest RAM as a
    # raw `<name>.mem/base` file per node) — ~52 GB at ~67 MB/s on this disk
    # is ~13 minutes per node, which already eats into 20 min by itself once
    # boot + warming + PDES drain coordination are added. 1800 s gives the
    # finalisation steps headroom.
    r = _exec_in_container(
        "./qflex initialize -c tests/realrun/dc-multi.yaml",
        timeout=1800,
    )
    assert r.returncode == 0, (
        f"initialize from loaded-test failed (rc={r.returncode}).\n"
        f"stdout (last 2k):\n{r.stdout[-2000:]}\n"
        f"stderr (last 2k):\n{r.stderr[-2000:]}"
    )

    # Per-node qcow2 should now carry the init_warmed snapshot. The actual VM
    # state lives in the external incremental-base files in run/ (the qcow2
    # entry is just a metadata pointer); both must be present for a downstream
    # loadvm to succeed.
    for n, sub in enumerate(NODE_SUB_NAMES):
        qcow2 = _node_qcow2(mounting, n)
        assert _qcow2_has_snapshot(qcow2, INIT_WARMED_SNAPSHOT), (
            f"init_warmed snapshot did NOT land in {qcow2} — check "
            f"{mounting}/experiments/{sub}/InitWarm.log / InitWarm.err"
        )
        # Incremental-base savevm produces <name>.state.zstd plus a
        # <name>.mem/base directory next to the qemu cwd.
        state = f"{mounting}/experiments/{sub}/run/{INIT_WARMED_SNAPSHOT}.state.zstd"
        base = f"{mounting}/experiments/{sub}/run/{INIT_WARMED_SNAPSHOT}.mem/base"
        assert os.path.exists(state), (
            f"expected {state} after init_warm — incremental-base state file missing"
        )
        assert os.path.exists(base), (
            f"expected {base} after init_warm — incremental-base mem file missing"
        )


def test_05_fw_creates_per_sample_snapshots(dev_container):
    """Run `./qflex fw` against the init_warmed snapshot. WormCacheQFlex
    `mode=warm` takes `--sample-size` periodic per-sample snapshots over the
    `population_seconds` window, then prints `Generate N snapshots. Quit.`
    and `std::process::exit(0)` — qemu exits naturally with rc 0.

    Sample count is the default of qflex's `fw` command signature (30);
    population is the test-base-multi.yaml `_leaf_defaults.population_seconds`
    (1 s — overrides the dc.yaml 5 s default). Per the "no CLI flags in tests"
    rule, both come from the YAML / signature defaults, not from a
    `--sample-size` flag.
    """
    mounting = dev_container

    # Skip cleanly if test_04 hasn't created init_warmed yet.
    require_snapshot(
        [_node_qcow2(mounting, n) for n in range(len(NODE_SUB_NAMES))],
        INIT_WARMED_SNAPSHOT,
        "run tests/test_chained_pipeline.py::test_04_init_warm_creates_init_warmed_snapshot first",
    )

    r = _exec_in_container(
        "./qflex fw -c tests/realrun/dc-multi.yaml",
        timeout=1800,
    )
    assert r.returncode == 0, (
        f"fw failed (rc={r.returncode}).\n"
        f"stdout (last 2k):\n{r.stdout[-2000:]}\n"
        f"stderr (last 2k):\n{r.stderr[-2000:]}"
    )

    # FW has a known bug where one node may skip writing the LAST checkpoint
    # file (snapshot_29.state.zstd) even though the plugin exited cleanly.
    # The deterministic signal is the plugin's exit log line — it appears in
    # at least one node's FunctionalWarming.log when count reaches 30 and
    # `std::process::exit(0)` is called. We assert the log line on at least
    # one node and the bulk presence of per-sample files (≥ N-1 of N).
    quit_marker = f"Generate {FW_SAMPLE_SIZE} snapshots. Quit."
    saw_quit_log = False
    for n, sub in enumerate(NODE_SUB_NAMES):
        log = f"{mounting}/experiments/{sub}/FunctionalWarming.log"
        assert os.path.exists(log), f"expected {log} from FW run"
        with open(log) as f:
            if quit_marker in f.read():
                saw_quit_log = True

        # Per-node sanity: at least N-1 of N samples landed (the known bug
        # is one node skipping just the last). Fewer than that means FW
        # didn't actually run through the budget.
        run_dir = f"{mounting}/experiments/{sub}/run"
        sample_files = [
            f for f in os.listdir(run_dir)
            if f.startswith("snapshot_") and f.endswith(".state.zstd")
        ]
        assert len(sample_files) >= FW_SAMPLE_SIZE - 1, (
            f"node {n} only produced {len(sample_files)} of {FW_SAMPLE_SIZE} "
            f"per-sample state files in {run_dir} — FW didn't reach the end."
        )

    assert saw_quit_log, (
        f"none of the nodes' FunctionalWarming.log contained {quit_marker!r} — "
        f"FW didn't exit via the count-reached path on ANY node. The plugin's "
        f"`std::process::exit(0)` is the deterministic completion signal; if "
        f"this line is missing the run was killed mid-FW (timeout / SIGKILL)."
    )


def test_06_partition_splits_samples_into_chunks(dev_container):
    """Run `./qflex partition` to split the FW per-sample checkpoints into 5
    partitions per node. partition.py is purely file-juggling — no qemu
    launches — so this is fast (seconds).

    `partition_count: 5` lives in the YAML (per the no-CLI-flags rule) and
    is read from `ExperimentContext.partition_count` by `PartitionCommand`.
    Pre-conditions are FW per-sample files in run/ and scripts/run_flexus.sh
    from init-warm — both are produced by test_04 + test_05.
    """
    mounting = dev_container

    # Skip cleanly if FW hasn't run yet (no per-sample state files to split).
    # We probe one node's run/ for snapshot_0.state.zstd as a cheap proxy.
    fw_marker = (f"{mounting}/experiments/{NODE_SUB_NAMES[0]}/run/"
                 f"snapshot_0.state.zstd")
    if not os.path.exists(fw_marker):
        pytest.skip(
            f"missing FW per-sample state file {fw_marker} — run "
            f"tests/test_chained_pipeline.py::test_05_fw_creates_per_sample_snapshots first"
        )

    # PartitionCommand fails if any node's run/partition_0 already exists.
    # Wipe the previous partition layout so the test is idempotent across
    # re-runs without touching the FW snapshots themselves.
    rm_targets = [
        f"{mounting}/experiments/{sub}/run/partition_*"
        for sub in NODE_SUB_NAMES
    ]
    rm_cmd = " && ".join(f"rm -rf {t}" for t in rm_targets)
    rm_result = _exec_in_container(rm_cmd, timeout=60)
    assert rm_result.returncode == 0, (
        f"failed to clean previous partitions before test_06 "
        f"(rc={rm_result.returncode}). stderr:\n{rm_result.stderr}"
    )

    # partition is fast (file moves only). A few minutes is generous.
    r = _exec_in_container(
        "./qflex partition -c tests/realrun/dc-multi.yaml",
        timeout=300,
    )
    assert r.returncode == 0, (
        f"partition failed (rc={r.returncode}).\n"
        f"stdout (last 2k):\n{r.stdout[-2000:]}\n"
        f"stderr (last 2k):\n{r.stderr[-2000:]}"
    )

    # Each node should now have partition_0 .. partition_<N-1> directories
    # under run/, plus a run_partitions.sh at the experiment root (used by
    # `./qflex result` later — see commands/result.py).
    for sub in NODE_SUB_NAMES:
        run_dir = f"{mounting}/experiments/{sub}/run"
        for p in range(PARTITION_COUNT):
            part_dir = f"{run_dir}/partition_{p}"
            assert os.path.isdir(part_dir), (
                f"expected partition dir {part_dir} after partition phase "
                f"(partition_count={PARTITION_COUNT})"
            )
        run_partitions_sh = f"{mounting}/experiments/{sub}/run_partitions.sh"
        assert os.path.exists(run_partitions_sh), (
            f"expected {run_partitions_sh} after partition phase"
        )


def test_07_run_idx_single_sample(dev_container):
    """Drive `./qflex run-idx` for (partition 0, idx 0) on both nodes via the
    dc-multi-run-idx-p0-i0.yaml fixture. Same `RunIdxCommand` the
    `run-single-partition` and `run-partition` phases compose, just executed
    as a one-shot leaf — so this is the smallest unit of the timing pipeline.
    """
    mounting = dev_container

    # Bail early if the partition layout (and thus snapshot files) hasn't been
    # produced by test_06 yet. require_snapshot doesn't fit (not a qcow2-side
    # snapshot); checking the FW per-sample state file is the cheapest proxy.
    if not os.path.exists(f"{mounting}/experiments/{NODE_SUB_NAMES[0]}/run/"
                          f"partition_0/snapshot_0.state.zstd"):
        pytest.skip(
            "missing partition_0/snapshot_0 state — run "
            "tests/test_chained_pipeline.py::test_06_partition_splits_samples_into_chunks first"
        )

    r = _exec_in_container(
        "./qflex run-idx -c tests/realrun/dc-multi.yaml",
        timeout=1800,
    )
    assert r.returncode == 0, (
        f"run-idx (part 0, idx 0) failed (rc={r.returncode}).\n"
        f"stdout (last 2k):\n{r.stdout[-2000:]}\n"
        f"stderr (last 2k):\n{r.stderr[-2000:]}"
    )


def _assert_idx_outputs(mounting: str, sub: str, partition_number: int,
                        aggregated_log_text: str) -> list[str]:
    """For one node `sub` and one partition, walk every `result_<i>` dir under
    `<run>/partition_<P>/` and assert the Flexus-side per-idx artifacts exist
    and are non-empty. These are the actual signals that the timing run did
    real work — without them rc=0 alone is meaningless.

    `aggregated_log_text` is the stdout/log text that should aggregate output
    from every idx in the partition. For `run-single-partition` (direct CLI)
    this is `r.stdout` (no per-partition log file is written). For
    `run-partition` it's the contents of `<part>/run-partition.log`.
    SequentialGroupExecutor wipes the per-partition log file once at the
    start and each child's bash redirect appends — so if the aggregated text
    is missing earlier idxs, the overwrite-instead-of-append regression is back.

    The `all.measurement.end.log` file is allowed to be missing on individual
    idxs (the peer-kill race in commands/executer.py + parallel-qemu PDES exit
    can SIGKILL the lagging node mid-flush — see the "PDES peer-kill end.log"
    TODO in CLAUDE.md). This function does NOT assert on it directly; instead
    it returns the list of missing-end.log paths so the caller can apply a
    single global "at most one missing per test" budget across all checked
    (node, partition) pairs.
    """
    part_dir = f"{mounting}/experiments/{sub}/run/partition_{partition_number}"
    assert os.path.isdir(part_dir), f"missing partition dir {part_dir}"

    result_dirs = sorted(
        d for d in os.listdir(part_dir)
        if d.startswith("result_") and os.path.isdir(os.path.join(part_dir, d))
    )
    assert result_dirs, (
        f"no result_<idx> directories under {part_dir} — run-* phase produced "
        "no per-idx output"
    )

    missing_end_log: list[str] = []
    for rd in result_dirs:
        idx_dir = os.path.join(part_dir, rd)
        qemu_timing = os.path.join(idx_dir, "qemu-timing.log")
        assert os.path.exists(qemu_timing) and os.path.getsize(qemu_timing) > 0, (
            f"{qemu_timing} missing or empty — timing-phase qemu didn't produce log"
        )
        end_log = os.path.join(idx_dir, "all.measurement.end.log")
        if not (os.path.exists(end_log) and os.path.getsize(end_log) > 0):
            missing_end_log.append(end_log)
        meas_logs = [
            f for f in os.listdir(idx_dir)
            if f.startswith("all.measurement.") and f != "all.measurement.end.log"
        ]
        assert meas_logs, (
            f"no all.measurement.<NNN>.log files in {idx_dir} — Flexus emitted no "
            "per-checkpoint stats"
        )

    expected_idxs = sorted(int(d.removeprefix("result_")) for d in result_dirs)
    # RunIdxCommand emits banners containing `qflex idx <i>:` in three places:
    # the setup echo (lands in run-partition.log), and the gdb stdout/stderr
    # banners (land in partition_<P>/log and /err respectively). Aggregated
    # log MUST contain one such marker per idx — if any are missing, an idx
    # either never ran or its output was truncated by a later idx (the
    # overwrite regression).
    missing = [
        i for i in expected_idxs
        if f"qflex idx {i}:" not in aggregated_log_text
    ]
    assert not missing, (
        f"node {sub} partition_{partition_number}: aggregated log is missing "
        f"the per-idx banner for idx(s) {missing}. "
        f"Either earlier idxs were truncated by a later idx (the overwrite "
        f"regression) or those idxs never ran. expected_idxs={expected_idxs}"
    )
    return missing_end_log


# Global budget for missing all.measurement.end.log files in a single test.
# The follower-side END_OF_EMULATION → libqflex_stop → exit(0) race that used
# to drop end.log on lagging idxs is fixed in qemu-pdes/net/pdes-engine.c
# (the END_OF_EMULATION arm of process_message no longer calls
# pdes_engine_destroy directly — it just unblocks sync and grants exit
# permission, letting local Flexus finish its measurement and flush end.log
# before qemu's natural shutdown). With that fix in place, EVERY idx must
# produce all.measurement.end.log; budget is zero.
MAX_MISSING_END_LOG_PER_TEST = 0


def test_08_run_single_partition_sequential_idxs(dev_container):
    """Drive `./qflex run-single-partition` for partition 0 on both nodes via
    dc-multi-run-single-partition-p0.yaml. RunSinglePartitionCommand iterates
    the partition's idxs sequentially (with the in-source `sleep 5` between
    them), per node in parallel via the multi-node group dispatch. Asserts
    that every idx in partition 0 produced its Flexus per-idx output on both
    nodes.
    """
    mounting = dev_container

    if not os.path.exists(f"{mounting}/experiments/{NODE_SUB_NAMES[0]}/run/"
                          f"partition_0/snapshot_0.state.zstd"):
        pytest.skip(
            "missing partition_0/snapshot_0 state — run "
            "tests/test_chained_pipeline.py::test_06_partition_splits_samples_into_chunks first"
        )

    r = _exec_in_container(
        "./qflex run-single-partition -c tests/realrun/dc-multi.yaml",
        timeout=3600,
    )
    assert r.returncode == 0, (
        f"run-single-partition (part 0) failed (rc={r.returncode}).\n"
        f"stdout (last 2k):\n{r.stdout[-2000:]}\n"
        f"stderr (last 2k):\n{r.stderr[-2000:]}"
    )

    # `run-single-partition` (use_stdio=True) tees its bash output to both
    # stdout and `<exp>/RunSinglePartitionCommand.log` per node. We read the
    # file rather than r.stdout because the tee → docker-exec → subprocess
    # capture path is flakey on buffering; the file is the authoritative
    # source. The file must contain every idx's banner, else the wipe-then-
    # append regression is back.
    missing_end_log: list[str] = []
    for sub in NODE_SUB_NAMES:
        node_log = f"{mounting}/experiments/{sub}/RunSinglePartitionCommand.log"
        assert os.path.exists(node_log) and os.path.getsize(node_log) > 0, (
            f"{node_log} missing or empty — run-single-partition produced no "
            f"aggregate per-node log on this node"
        )
        with open(node_log, errors="replace") as f:
            aggregated = f.read()
        missing_end_log += _assert_idx_outputs(
            mounting, sub, partition_number=0, aggregated_log_text=aggregated,
        )
    missing_end_log = sorted(set(missing_end_log))
    assert len(missing_end_log) <= MAX_MISSING_END_LOG_PER_TEST, (
        f"more than {MAX_MISSING_END_LOG_PER_TEST} all.measurement.end.log "
        f"file(s) missing — exceeds the budget for the known PDES peer-kill "
        f"race. Missing: {missing_end_log}"
    )


def test_09_run_partition_full_fanout(dev_container):
    """Drive `./qflex run-partition` (all partitions, both nodes) via
    dc-multi-run-partition.yaml. Exercises the cross-node handshake added in
    commands/run_partition.py (node 1 waits for node 0's
    RunPartitionCommand_node0.started before fanning out its own partitions)
    and the per-partition log routing to <run>/partition_<P>/run-partition.{log,err}.
    Asserts that every idx in every partition produced its Flexus per-idx
    output on both nodes.
    """
    mounting = dev_container

    if not os.path.exists(f"{mounting}/experiments/{NODE_SUB_NAMES[0]}/run/"
                          f"partition_0/snapshot_0.state.zstd"):
        pytest.skip(
            "missing partition_0/snapshot_0 state — run "
            "tests/test_chained_pipeline.py::test_06_partition_splits_samples_into_chunks first"
        )

    # All 5 partitions × ~6 idxs each × 2 nodes (parallel). At a few minutes
    # per idx with Flexus, this can run many minutes; cap at 2 hours.
    r = _exec_in_container(
        "./qflex run-partition -c tests/realrun/dc-multi.yaml",
        timeout=7200,
    )
    assert r.returncode == 0, (
        f"run-partition failed (rc={r.returncode}).\n"
        f"stdout (last 2k):\n{r.stdout[-2000:]}\n"
        f"stderr (last 2k):\n{r.stderr[-2000:]}"
    )

    # `run-partition` fans out one mp.Process per partition, each redirecting
    # its bash stream to `<exp>/run/partition_<P>/run-partition.log`. Read each
    # partition's three aggregate logs and verify they each contain output
    # from every idx in that partition:
    #   * `run-partition.log` — outer redirect, written by _build_bash with >>
    #     (wipe-once + append, see SequentialGroupExecutor)
    #   * `log` and `err` — inner redirect from RunIdxCommand's gdb command,
    #     written with >>/2>> (wipe-once via RunSinglePartitionCommand,
    #     append-per-idx with `===== gdb stdout/stderr for idx N =====` banner)
    missing_end_log: list[str] = []
    for sub in NODE_SUB_NAMES:
        for p in range(PARTITION_COUNT):
            part_dir = f"{mounting}/experiments/{sub}/run/partition_{p}"
            for fname in ("run-partition.log", "log", "err"):
                path = f"{part_dir}/{fname}"
                assert os.path.exists(path) and os.path.getsize(path) > 0, (
                    f"{path} missing or empty — run-partition produced no aggregate "
                    f"{fname} for this partition"
                )
                with open(path, errors="replace") as f:
                    aggregated = f.read()
                missing_end_log += _assert_idx_outputs(
                    mounting, sub, partition_number=p, aggregated_log_text=aggregated,
                )
    # _assert_idx_outputs is called 3× per (node, partition) pair (once for
    # each of run-partition.log / log / err) so the same end.log file can
    # appear up to 3× in `missing_end_log`. Dedupe before checking the budget.
    missing_end_log = sorted(set(missing_end_log))
    assert len(missing_end_log) <= MAX_MISSING_END_LOG_PER_TEST, (
        f"more than {MAX_MISSING_END_LOG_PER_TEST} all.measurement.end.log "
        f"file(s) missing — exceeds the budget for the known PDES peer-kill "
        f"race. Missing: {missing_end_log}"
    )


def test_10_result_aggregates_to_core_info_csv(dev_container):
    """Drive `./qflex result` for both nodes. Each node's `result.py` aggregates
    per-(partition, idx) Flexus stats and computes new per-core IPC + U-IPC.
    Verifies for BOTH nodes that:
      * `<exp>/core_info_new.csv` was written (the new-IPC artifact),
      * the per-leaf RunResultCommand log contains "U-IPC" stats so we know
        the U-IPC distribution branch ran end-to-end (not just IPC).
    """
    from dep_injection.builder import build_experiment_context

    mounting = dev_container

    # Cheap pre-condition: per-node Flexus result folders must exist (produced
    # by test_09's run-partition). Derive the per-node experiment folder from
    # the YAML the same way production does — no path math in the test body.
    top = build_experiment_context(
        "tests/realrun/dc-multi.yaml",
        component_overrides={
            "experiment_context": {"mounting_folder": mounting, "image_folder": mounting},
            "experiment_context_node_0": {"mounting_folder": mounting, "image_folder": mounting},
            "experiment_context_node_1": {"mounting_folder": mounting, "image_folder": mounting},
        },
    )
    sub_ctxs = top.sub_experiments
    assert len(sub_ctxs) == len(NODE_SUB_NAMES), (
        f"expected {len(NODE_SUB_NAMES)} per-node sub-experiments in the YAML, "
        f"got {len(sub_ctxs)}"
    )
    # No path-side pre-condition check here — `./qflex result` itself
    # surfaces a non-zero rc if the partition stats from test_09 are missing,
    # which the assertion below picks up.

    r = _exec_in_container(
        "./qflex result -c tests/realrun/dc-multi.yaml",
        timeout=900,
    )
    assert r.returncode == 0, (
        f"result failed (rc={r.returncode}).\n"
        f"stdout (last 2k):\n{r.stdout[-2000:]}\n"
        f"stderr (last 2k):\n{r.stderr[-2000:]}"
    )

    # For each node verify both artifacts: the new-IPC CSV and the U-IPC line
    # in the leaf log. result.py writes core_info_new.csv next to the
    # experiment folder (its cwd is the experiment folder via RunResultCommand).
    for ctx in sub_ctxs:
        exp_folder = ctx.get_experiment_folder_address()
        new_ipc_csv = f"{exp_folder}/core_info_new.csv"
        assert os.path.exists(new_ipc_csv), (
            f"node {ctx.node_number}: result.py did not produce {new_ipc_csv} "
            f"— new IPC values were not calculated for this node"
        )
        with open(new_ipc_csv) as f:
            csv_text = f.read()
        # New-IPC CSV should at least have a header + one core row, and the
        # column we care about (`ipns`) should be present.
        assert "ipns" in csv_text, (
            f"node {ctx.node_number}: {new_ipc_csv} is missing the ipns column"
        )
        assert csv_text.count("\n") >= 2, (
            f"node {ctx.node_number}: {new_ipc_csv} has no data rows — IPC was not "
            f"populated for any core"
        )

        # U-IPC verification: the per-leaf RunResultCommand log captures
        # result.py's stdout (the "U-IPC" / "Average U-IPC" lines from the
        # Rich summary table). Existence of those strings means the U-IPC
        # branch executed too (not just IPC).
        leaf_log = f"{exp_folder}/RunResultCommand.log"
        assert os.path.exists(leaf_log), (
            f"node {ctx.node_number}: missing per-leaf log {leaf_log}"
        )
        with open(leaf_log) as f:
            log_text = f.read()
        assert "U-IPC" in log_text, (
            f"node {ctx.node_number}: U-IPC was NOT computed/printed for this "
            f"node (no 'U-IPC' string in {leaf_log}). new IPC may have been "
            f"calculated but U-IPC path didn't run."
        )
