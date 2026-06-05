"""Init real-run tests — bootstrap the `boot-login` snapshot the regular
real-run tests load from.

Two tests:

  test_init_alpine_boot_login_savevm
    Single-node: boot Alpine in root-single-node.qcow2, wait for the login
    prompt, savevm the running guest as `boot-login`, quit. The driver expect
    is tests/realrun/boot_login_savevm.exp; YAML lives at
    tests/realrun/dc-single-init.yaml.

  test_init_multi_boot_login_savevm
    Two-node: boot both nodes (after wiping the per-node qcow2s so cp -u
    pulls fresh state from the master), wait for each guest's login prompt
    via the master/follower split (boot_login_savevm.exp + boot_login_wait.exp),
    master savevm's `boot-login` which walks the PDES drain and persists every
    node's CPU+memory into its per-node qcow2 (root-single-node.qcow2-node{0,1}).

Disabled by default because each is a full Alpine boot — minutes per node
under PDES. Enable with `QFLEX_INIT_TEST=1`. Re-run after the qcow2 is
reset, the QEMU binaries are rebuilt, or the multi-node topology changes.

Once these have run successfully, the regular real-run tests
(tests/test_real_runs.py, tests/test_real_runs_savevm.py) skip the Alpine
boot via `loadvm_name: boot-login` in their YAMLs and run in seconds-to-
tens-of-seconds rather than minutes.
"""
import os

import pytest

from .conftest import (
    _docker_available,
    _exec_in_container,
    _qcow2_has_snapshot,
    _qflex_image_present,
)


# Shared with the regular real-run tests (dc-alpine-login.yaml,
# dc-multi-savevm-create.yaml, dc-multi-savevm-verify.yaml). QFlex's
# parallel-qemu savevm writes an external `boot-login.zstd` to the qemu cwd
# (`<exp>/run/`) — sharing the experiment folder is what lets subsequent
# loadvm runs find that file.
#
# Names are prefixed `qflex_test_` / `qflex_real_run_test` so they're unique
# to the test world and don't collide with a user's real-experiment runs
# (which typically use `data-caching-comparison*` from tests/realrun/{single,multi}.yaml).
SINGLE_EXPERIMENT_NAME = "qflex_real_run_test"
MULTI_GROUP_NAME = "qflex_test_multi"
MULTI_NODE_SUB_NAMES = (
    "qflex_test_multi-node-0",
    "qflex_test_multi-node-1",
)


pytestmark = [
    pytest.mark.skipif(
        not os.environ.get("QFLEX_INIT_TEST"),
        reason="set QFLEX_INIT_TEST=1 to bootstrap the boot-login snapshot "
               "(slow — only needed once per qcow2 reset)",
    ),
    pytest.mark.skipif(not _docker_available(), reason="docker daemon not reachable"),
    pytest.mark.skipif(not _qflex_image_present(),
                       reason="ghcr.io/parsa-epfl/qflex image not present locally; "
                              "run `./dep build-docker` or pull it first"),
]


def _master_qcow2(mounting: str) -> str:
    return f"{mounting}/root-single-node.qcow2"


def _node_qcow2(mounting: str, n: int) -> str:
    # parallel-qemu's copy_image_for_node splits on '.' and inserts -nodeN
    # before the extension: root-single-node.qcow2 -> root-single-node-node0.qcow2.
    return f"{mounting}/root-single-node-node{n}.qcow2"


def test_init_alpine_boot_login_savevm(dev_container):
    """Bootstrap the single-node `boot-login` snapshot in root-single-node.qcow2.

    Wipes the experiment folder so set_up_folders() regenerates core_info.csv
    against this run's actual core_count / doubled_vcpu (mirrors the cleanup
    used by tests/test_real_runs.py and test_real_runs_savevm.py).
    """
    mounting = dev_container

    rm_cmd = f"rm -rf {mounting}/experiments/{SINGLE_EXPERIMENT_NAME}"
    rm_result = _exec_in_container(rm_cmd, timeout=60)
    assert rm_result.returncode == 0, (
        f"failed to clean experiment folder before init alpine "
        f"(rc={rm_result.returncode}). stderr:\n{rm_result.stderr}"
    )

    r = _exec_in_container(
        "./qflex boot -c tests/realrun/dc-single-init.yaml",
        timeout=1800,  # full Alpine boot can take a few minutes
    )
    assert r.returncode == 0, (
        f"alpine init boot+savevm failed (rc={r.returncode}).\n"
        f"stdout (last 2k):\n{r.stdout[-2000:]}\n"
        f"stderr (last 2k):\n{r.stderr[-2000:]}"
    )

    qcow2 = _master_qcow2(mounting)
    assert _qcow2_has_snapshot(qcow2, "boot-login"), (
        f"expected `boot-login` snapshot in {qcow2} after init, but it is "
        f"missing — did the master expect script reach savevm? "
        f"check {mounting}/experiments/{SINGLE_EXPERIMENT_NAME}/expect_log.txt "
        f"and qemu_serial.log."
    )
    # parallel-qemu's savevm writes an external <name>.zstd in the qemu cwd —
    # the qcow2 internal entry is just a bookkeeping pointer. Both must be
    # present for a subsequent loadvm to succeed.
    zstd = f"{mounting}/experiments/{SINGLE_EXPERIMENT_NAME}/run/boot-login.zstd"
    assert os.path.exists(zstd), (
        f"expected external snapshot file {zstd} after init, but it is "
        f"missing — `delvm` deletes the qcow2 entry but not the .zstd, so a "
        f"left-over qcow2 entry may shadow a missing data file."
    )


def test_init_multi_boot_login_savevm(dev_container):
    """Bootstrap the multi-node `boot-login` snapshot in each per-node qcow2.

    Wipes the experiment folders AND the per-node qcow2s so `cp -u` pulls a
    fresh copy from the master image. The qcow2 wipe is necessary whenever
    the VM topology changes (core_count, doubled_vcpu, memory_gb in some
    paths) — QEMU's migration framework rejects topology-mismatched savevm
    state with "Not a migration stream / Error -22 while loading VM state".

    Cost: rebuilding each per-node qcow2 via `cp -u` from the 6+ GB master
    takes ~20 minutes on this disk. That's the price of `QFLEX_INIT_TEST=1`;
    it's an opt-in bootstrap, not part of the default `make test` path.

    These qcow2s are test-world files (`root-single-node-node{0,1}.qcow2`),
    only populated by the `qflex_test_multi-*` experiment names — they don't
    collide with user real-experiment runs.
    """
    mounting = dev_container

    rm_targets = [
        f"{mounting}/experiments/{MULTI_GROUP_NAME}",  # parent group folder
    ]
    for sub in MULTI_NODE_SUB_NAMES:
        rm_targets.append(f"{mounting}/experiments/{sub}")
    # Wipe per-node qcow2s so any prior savevm state (which was captured at the
    # previous core_count / doubled_vcpu) doesn't survive into the new bootstrap
    # and trip "Error -22 while loading VM state" on subsequent loadvm.
    for n in range(len(MULTI_NODE_SUB_NAMES)):
        rm_targets.append(_node_qcow2(mounting, n))
    rm_cmd = " && ".join(f"rm -rf {t}" for t in rm_targets)
    rm_result = _exec_in_container(rm_cmd, timeout=120)
    assert rm_result.returncode == 0, (
        f"failed to clean before init multi (rc={rm_result.returncode}). "
        f"stderr:\n{rm_result.stderr}"
    )

    r = _exec_in_container(
        "./qflex boot -c tests/realrun/dc-multi-init.yaml",
        timeout=1800,  # multi-node Alpine boot is much slower than single-node
    )
    assert r.returncode == 0, (
        f"multi-node init boot+savevm failed (rc={r.returncode}).\n"
        f"stdout (last 2k):\n{r.stdout[-2000:]}\n"
        f"stderr (last 2k):\n{r.stderr[-2000:]}"
    )

    for n, sub in enumerate(MULTI_NODE_SUB_NAMES):
        qcow2 = _node_qcow2(mounting, n)
        assert _qcow2_has_snapshot(qcow2, "boot-login"), (
            f"expected `boot-login` snapshot in {qcow2} after multi-node init, "
            f"but it is missing — distributed savevm probably failed. "
            f"check {mounting}/experiments/{sub}/expect_log.txt and qemu_serial.log."
        )
        zstd = f"{mounting}/experiments/{sub}/run/boot-login.zstd"
        assert os.path.exists(zstd), (
            f"expected external snapshot file {zstd} after multi-node init, "
            f"but it is missing — see single-node init test for context on the "
            f"qcow2-entry-vs-zstd-data split."
        )
