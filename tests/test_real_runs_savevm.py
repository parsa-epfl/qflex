"""Multi-node savevm/loadvm round-trip — actual QEMU runs against the user's
Alpine image, two nodes, one snapshot tag (`qflex-makefile-test`).

Two tests, run in order via name prefixes (pytest collects in declaration order
within a file; the `test_01_` / `test_02_` prefixes are insurance against any
ordering plugin that sorts alphabetically):

  test_01_boot_two_nodes_create_files_and_savevm
    Boots both nodes via `./qflex boot -c conf/DC/dc-multi-savevm-create.yaml`.
    Each leaf's interaction_script logs in as qflex/qflex, creates testN.txt
    (test1.txt on node 0, test2.txt on node 1), runs `ls`, and savevm's the
    guest state under tag `qflex-makefile-test` via the QEMU monitor before
    quitting. The host then asserts each per-node ls captured the right file.

  test_02_load_two_nodes_verify_files
    Loads both nodes via `./qflex load -c conf/DC/dc-multi-savevm-verify.yaml`,
    which sets `loadvm_name: qflex-makefile-test` per leaf so QEMU starts with
    `-loadvm qflex-makefile-test`. The interaction_script just runs `ls` on the
    restored guest and quits. The host asserts each node's post-load ls still
    contains the file the previous test created.

Both tests share the session-scoped `dev_container` fixture from conftest.py,
so the dev container starts once and both tests run inside it.

Enabled by default — same gating as test_real_runs.py (QFLEX_SKIP_REAL_RUN=1
to disable + docker reachable + qflex image local). Both tests skip themselves
if the per-node qcow2s don't already carry the `boot-login` snapshot bootstrapped
by tests/test_real_runs_init.py (`QFLEX_INIT_TEST=1`-gated).
"""
import os
import sys

import pytest

from .conftest import (
    _docker_available,
    _exec_in_container,
    _qflex_image_present,
    _real_run_disabled,
    require_boot_login_zstd,
    require_snapshot,
)

SNAPSHOT_NAME = "qflex-makefile-test"
# Test-world names — distinct from `data-caching-comparison*` in dc-multi.yaml
# so a user's real-experiment runs never collide with these. Shared with
# tests/test_real_runs_init.py and tests/realrun/dc-multi-*.yaml.
NODE_SUB_NAMES = ("qflex_test_multi-node-0", "qflex_test_multi-node-1")


pytestmark = [
    pytest.mark.skipif(_real_run_disabled(), reason="QFLEX_SKIP_REAL_RUN is set"),
    pytest.mark.skipif(not _docker_available(), reason="docker daemon not reachable"),
    pytest.mark.skipif(not _qflex_image_present(),
                       reason="ghcr.io/parsa-epfl/qflex image not present locally"),
]


def _read_capture(mounting: str, sub_name: str, filename: str) -> str:
    path = f"{mounting}/experiments/{sub_name}/{filename}"
    assert os.path.exists(path), (
        f"missing {path} — did the expect script reach the capture step?"
    )
    with open(path) as f:
        return f.read()


def test_01_boot_two_nodes_create_files_and_savevm(dev_container):
    """Boot both nodes from `boot-login`, login, `touch test{N+1}.txt` per
    node, ls, savevm `qflex-makefile-test`, quit. Verify each node's ls captured
    the right file.

    The per-node qcow2s carry the bootstrapped `boot-login` snapshot from
    test_real_runs_init.py — DO NOT wipe them here, otherwise `copy_image_for_node`
    re-pulls from the master qcow2 (which has the single-node boot-login, NOT
    the multi-node-compatible one), and loadvm fails on device topology.
    Re-run the multi-node init test if per-node qcow2s need refreshing."""
    mounting = dev_container

    # Skip cleanly if the bootstrap snapshot hasn't been created yet — we check
    # the external boot-login.zstd in each per-node run/ (the real VM state).
    require_boot_login_zstd([
        f"{mounting}/experiments/{sub}/run" for sub in NODE_SUB_NAMES
    ])
    # parallel-qemu's copy_image_for_node names per-node qcow2s
    # `<basename>-node<N>.<ext>` (root-single-node-node0.qcow2), NOT
    # `<filename>-node<N>` — referenced in require_snapshot below as well.

    # No experiment-folder wipe: init shares these per-node folders and drops
    # boot-login.zstd into each run/ — wiping would discard the snapshot. Per-
    # node qcow2s are preserved for the same reason.

    cmd = "./qflex boot -c tests/realrun/dc-multi-savevm-create.yaml"
    # Multi-node Alpine boots are slow under quantum sync; 30 min upper bound.
    r = _exec_in_container(cmd, timeout=1800)
    assert r.returncode == 0, (
        f"boot+savevm failed (rc={r.returncode}).\n"
        f"stdout (last 2k):\n{r.stdout[-2000:]}\n"
        f"stderr (last 2k):\n{r.stderr[-2000:]}"
    )

    for node, sub_name in enumerate(NODE_SUB_NAMES):
        ls_output = _read_capture(mounting, sub_name, "ls_after_create.txt")
        expected = f"test{node + 1}.txt"
        # Visible in `pytest -s` / `make test-verbose`.
        print(f"\n[savevm-create node {node}] ls_after_create:\n{ls_output}\n",
              file=sys.stderr)
        assert expected in ls_output, (
            f"expected {expected!r} in node {node}'s ls_after_create, got:\n{ls_output}"
        )


def test_02_load_two_nodes_verify_files(dev_container):
    """Load both nodes from the snapshot, ls, quit. Verify each node's file
    survived the savevm/loadvm round-trip."""
    mounting = dev_container

    # The verify snapshot is qflex-makefile-test, which test_01 wrote into
    # each per-node qcow2. Skip cleanly if it isn't there — typically because
    # test_01 was skipped/failed. Per-node qcow2 path is `<base>-node<N>.<ext>`
    # (see commands/config.py:copy_image_for_node), NOT `<filename>-node<N>`.
    require_snapshot(
        [f"{mounting}/root-single-node-node{n}.qcow2" for n in range(len(NODE_SUB_NAMES))],
        SNAPSHOT_NAME,
        "run test_01_boot_two_nodes_create_files_and_savevm first to create it",
    )

    cmd = "./qflex load -c tests/realrun/dc-multi-savevm-verify.yaml"
    r = _exec_in_container(cmd, timeout=1800)
    assert r.returncode == 0, (
        f"load+verify failed (rc={r.returncode}).\n"
        f"stdout (last 2k):\n{r.stdout[-2000:]}\n"
        f"stderr (last 2k):\n{r.stderr[-2000:]}"
    )

    for node, sub_name in enumerate(NODE_SUB_NAMES):
        ls_output = _read_capture(mounting, sub_name, "ls_after_load.txt")
        expected = f"test{node + 1}.txt"
        print(f"\n[savevm-load node {node}] ls_after_load:\n{ls_output}\n",
              file=sys.stderr)
        assert expected in ls_output, (
            f"expected {expected!r} in node {node}'s ls_after_load (post-snapshot "
            f"restore), got:\n{ls_output}"
        )
