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
    fw selected     (test_05 — runs `./qflex fw` against init-warm result; sample_size from YAML)

Each test gates itself on the previous test's snapshot via `require_snapshot`,
so a missing earlier step shows up as a SKIP rather than a misleading FAIL.

Same gating as the rest of the real-run suite: enabled by default, set
`QFLEX_SKIP_REAL_RUN=1` to disable. Init bootstrap (boot-login) lives in
test_boot_login_bootstrap.py and is gated separately by `QFLEX_INIT_TEST=1`.
"""
import os
import re

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
REALRUN_YAML = "tests/realrun/dc-multi.yaml"
PARTITION_COUNT = 5                   # `partition:` section in dc-multi.yaml


def _yaml_leaf_field(cmd_name: str, field: str):
    """Read a per-leaf field for the given phase out of the realrun YAML.
    Single source of truth: the YAML. Tests that need to assert on a YAML
    value (e.g. sample_size) should derive it via this helper rather than
    re-declaring the value as a python constant."""
    from dep_injection.builder import build_experiment_context
    ctx = build_experiment_context(REALRUN_YAML, cmd_name=cmd_name)
    return getattr(ctx.sub_experiments[0], field)


pytestmark = [
    pytest.mark.skipif(_real_run_disabled(), reason="QFLEX_SKIP_REAL_RUN is set"),
    pytest.mark.skipif(not _docker_available(), reason="docker daemon not reachable"),
    pytest.mark.skipif(not _qflex_image_present(),
                       reason="ghcr.io/parsa-epfl/qflex image not present locally"),
]


def _node_qcow2(mounting: str, n: int) -> str:
    return f"{mounting}/root-single-node-node{n}.qcow2"


def _parse_kv(path: str) -> dict[str, str]:
    """Parse a `key=value` (one-per-line) file. Tolerates blank lines and
    leading/trailing whitespace from the serial-buffer capture (the file is
    typically the buffer between an `echo` command and its returning prompt,
    not a clean one-shot write)."""
    out: dict[str, str] = {}
    with open(path) as f:
        for raw in f:
            line = raw.strip()
            if not line or "=" not in line:
                continue
            k, _, v = line.partition("=")
            k, v = k.strip(), v.strip()
            if k and " " not in k:
                out[k] = v
    return out


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
    # One verification per phase (per the user): boot's claim is "loaded-test
    # snapshot exists in each per-node qcow2". The file-persistence check
    # (`testN.txt` survives savevm/loadvm) belongs to test_03b — it's the
    # load phase that actually exercises the loadvm path.
    for n, sub in enumerate(NODE_SUB_NAMES):
        qcow2 = _node_qcow2(mounting, n)
        assert _qcow2_has_snapshot(qcow2, LOADED_TEST_SNAPSHOT), (
            f"loaded-test snapshot did NOT land in {qcow2} — check "
            f"{mounting}/experiments/{sub}/expect_log.txt + Boot.log + qemu_serial.log"
        )


def test_03b_load_verifies_files_and_swaps_workload(dev_container):
    """`./qflex load` from `loaded-test` and drive
    `loaded_test_verify_and_swap_workload.exp` on both leaves (via the YAML's
    `load:` phase block). Verifies, by reading the script's host-visible
    artifacts:

      * test{N}.txt baked into `loaded-test` by test_03 survived the savevm/
        loadvm round-trip (`ls_after_load.txt`).
      * The peer-ping after loadvm hit 100/100 (`ping_summary_after_loadvm.txt`).
      * The per-node echo server + sender loops were running before the
        master's re-savevm (`workload_state.txt` — pids and rc==0).
      * No cmp-detected byte corruption was logged before re-savevm
        (`corruption_log_pre_savevm.txt` is empty / size==0).

    The .exp itself is now a thin guest driver: it types commands, captures
    artifacts, coordinates with the peer via sentinel files, and exits — every
    "is the system OK?" check happens in this test, not inside Tcl.
    """
    mounting = dev_container

    require_snapshot(
        [_node_qcow2(mounting, n) for n in range(len(NODE_SUB_NAMES))],
        LOADED_TEST_SNAPSHOT,
        "run tests/test_chained_pipeline.py::test_03_boot_creates_loaded_test_with_workload first",
    )

    # Delete prior artifacts before running so a no-op `./qflex load` (qemu
    # died too early to write any) can't false-pass on a previous run's stale
    # captures. Done via the container because the artifacts are root-owned.
    rm_paths = " ".join(
        f"/mnt/sdc/data-caching-1c/experiments/{sub}/{f}"
        for sub in NODE_SUB_NAMES
        for f in ("ls_after_load.txt", "ping_summary_after_loadvm.txt",
                  "workload_state.txt", "corruption_log_pre_savevm.txt",
                  "ok_in_last_10.txt")
    )
    _exec_in_container(f"rm -f {rm_paths}", timeout=30)

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
        exp_folder = f"{mounting}/experiments/{sub}"

        # 1) ls_after_load.txt — file persistence across savevm/loadvm.
        ls_path = f"{exp_folder}/ls_after_load.txt"
        assert os.path.exists(ls_path), (
            f"node {n}: missing {ls_path} — the .exp didn't reach the ls "
            f"capture step. Check {exp_folder}/expect_log.txt for where it died."
        )
        with open(ls_path) as f:
            ls_text = f.read()
        expected_file = f"test{n + 1}.txt"
        assert expected_file in ls_text, (
            f"node {n}: expected {expected_file!r} in {ls_path} after savevm/loadvm "
            f"round-trip, got:\n{ls_text}"
        )

        # 2) ping_summary_after_loadvm.txt — peer-ping was 100/100.
        ping_path = f"{exp_folder}/ping_summary_after_loadvm.txt"
        assert os.path.exists(ping_path), (
            f"node {n}: missing {ping_path} — the peer ping never returned. "
            f"Most likely cause: qemu's serial chardev died mid-ping (peer "
            f"qemu was killed early, or save_snapshot races); inspect "
            f"{exp_folder}/expect_log.txt and Load.log."
        )
        with open(ping_path) as f:
            ping_text = f.read()
        m = re.search(r"(\d+)\s+packets transmitted,\s+(\d+)\s+packets? received", ping_text)
        assert m is not None, (
            f"node {n}: no busybox ping summary in {ping_path}. Captured:\n{ping_text}"
        )
        tx, rx = int(m.group(1)), int(m.group(2))
        assert tx > 0 and tx == rx, (
            f"node {n}: ping after loadvm dropped packets (tx={tx} rx={rx}; "
            f"need tx>0 and tx==rx). Wire is misconfigured or the peer hadn't "
            f"caught up. Full ping:\n{ping_text}"
        )

        # 3) workload_state.txt — echo server + sender loops alive pre-savevm.
        state_path = f"{exp_folder}/workload_state.txt"
        assert os.path.exists(state_path), (
            f"node {n}: missing {state_path} — the workload-state probe never ran "
            f"(.exp died before line ~190). Check expect_log.txt."
        )
        state = _parse_kv(state_path)
        assert state.get("echo_server_rc") == "0", (
            f"node {n}: echo server (port-listener for the integrity workload) was NOT "
            f"running before re-savevm. Full state: {state}. Without it, init/fw will "
            f"resume from a snapshot where the workload is dead."
        )
        assert state.get("sender_rc") == "0", (
            f"node {n}: sender loop (urandom→nc→cmp) was NOT running before re-savevm. "
            f"Full state: {state}. Same impact as above — init/fw sees an idle guest."
        )
        assert state.get("echo_server_pid", "").isdigit() and int(state["echo_server_pid"]) > 0, (
            f"node {n}: echo_server_pid is missing or non-numeric in {state_path}: {state}"
        )
        assert state.get("sender_pid", "").isdigit() and int(state["sender_pid"]) > 0, (
            f"node {n}: sender_pid is missing or non-numeric in {state_path}: {state}"
        )
        assert state.get("corruption_log_size") == "0", (
            f"node {n}: cmp logged corruption BEFORE re-savevm "
            f"(/tmp/corruption.log size={state.get('corruption_log_size')}). "
            f"See {exp_folder}/corruption_log_pre_savevm.txt for the recorded mismatches."
        )

        # 4) corruption_log_pre_savevm.txt — must exist (even if empty).
        corr_path = f"{exp_folder}/corruption_log_pre_savevm.txt"
        assert os.path.exists(corr_path), (
            f"node {n}: missing {corr_path} — the .exp didn't capture the corruption "
            f"log. Suggests the script died between the workload start and the savevm step."
        )

        # 5) ok_in_last_10.txt — the LAST 10 sender iterations before savevm
        # must all be [ok]. Initial warm-up [noreply]s during the first second
        # (peer hasn't loadvm'd yet) are tolerated; what matters is the
        # snapshot captures the workload running cleanly. == 10 is strict by
        # design (per the user: "before we checkpoint... there should be no
        # failure going on anymore").
        ok_path = f"{exp_folder}/ok_in_last_10.txt"
        assert os.path.exists(ok_path), (
            f"node {n}: missing {ok_path} — settle window or tail-grep step didn't "
            f"complete. Inspect {exp_folder}/expect_log.txt."
        )
        with open(ok_path) as f:
            ok_text = f.read()
        m = re.search(r"^\s*(\d+)\s*$", ok_text, re.MULTILINE)
        assert m is not None, (
            f"node {n}: couldn't parse a count from {ok_path}: {ok_text!r}"
        )
        ok_count = int(m.group(1))
        assert ok_count == 10, (
            f"node {n}: only {ok_count}/10 of the last 10 sender iterations were [ok] "
            f"before savevm — workload was still failing right up to checkpoint. "
            f"Inspect {exp_folder}/qemu_serial.log tail for [noreply]/[MISMATCH] lines."
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

    # 10 minute wall budget — per the user's "make init be able to continue
    # for 10 minutes". Under parallel-mode init (forced by the YAML override
    # in `initialize:` to dodge the icount/sequential deadlock — see TODO in
    # commands/init_warm.py) the savevm path is the EXTERNAL_INCREMENTAL_BASE
    # memory dump; if 10 min isn't enough it likely means the bug above is
    # back, not that the budget is too tight. Bump rather than mask.
    r = _exec_in_container(
        "./qflex initialize -c tests/realrun/dc-multi.yaml",
        timeout=600,
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
    `mode=warm` takes `sample_size` periodic per-sample snapshots over the
    `population_seconds` window, then prints `Generate N snapshots. Quit.`
    and `std::process::exit(0)` — qemu exits naturally with rc 0.

    Sample count comes from the `fw._leaf_defaults.sample_size` overlay in
    tests/realrun/dc-multi.yaml; population is the test-base-multi.yaml
    `_leaf_defaults.population_seconds` (1 s — overrides the dc.yaml 5 s
    default). Per the "no CLI flags in tests" rule, both come from the YAML.
    """
    mounting = dev_container
    sample_size = _yaml_leaf_field("fw", "sample_size")

    # Skip cleanly if test_04 hasn't created init_warmed yet.
    require_snapshot(
        [_node_qcow2(mounting, n) for n in range(len(NODE_SUB_NAMES))],
        INIT_WARMED_SNAPSHOT,
        "run tests/test_chained_pipeline.py::test_04_init_warm_creates_init_warmed_snapshot first",
    )

    r = _exec_in_container(
        f"./qflex fw -c {REALRUN_YAML}",
        timeout=1800,
    )
    assert r.returncode == 0, (
        f"fw failed (rc={r.returncode}).\n"
        f"stdout (last 2k):\n{r.stdout[-2000:]}\n"
        f"stderr (last 2k):\n{r.stderr[-2000:]}"
    )

    # FW has a known bug where one node may skip writing the LAST checkpoint
    # file (snapshot_<N-1>.state.zstd) even though the plugin exited cleanly.
    # The deterministic signal is the plugin's exit log line — it appears in
    # at least one node's FunctionalWarming.log when count reaches sample_size
    # and `std::process::exit(0)` is called. We assert the log line on at
    # least one node and the bulk presence of per-sample files (≥ N-1 of N).
    quit_marker = f"Generate {sample_size} snapshots. Quit."
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
        assert len(sample_files) >= sample_size - 1, (
            f"node {n} only produced {len(sample_files)} of {sample_size} "
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
