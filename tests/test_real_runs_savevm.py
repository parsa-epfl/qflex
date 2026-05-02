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

Disabled by default — same gating as test_real_runs.py (QFLEX_REAL_RUN_TESTS=1
+ docker reachable + qflex image local).
"""
import os
import sys

import pytest

from .conftest import (
    _docker_available,
    _exec_in_container,
    _qflex_image_present,
)

SNAPSHOT_NAME = "qflex-makefile-test"
NODE_SUB_NAMES = ("data-caching_yaml_node_0", "data-caching_yaml_node_1")


pytestmark = [
    pytest.mark.skipif(
        not os.environ.get("QFLEX_REAL_RUN_TESTS"),
        reason="set QFLEX_REAL_RUN_TESTS=1 to enable real docker-based runs",
    ),
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
    """Boot both nodes, login, `touch test{N+1}.txt` per node, ls, savevm,
    quit. Verify each node's ls captured the right file.

    Wipes each leaf's experiment folder first so `set_up_folders()` runs end-to-
    end on every test invocation. `keep_experiment_unique=False` (the YAML's
    default) means folders are reused across runs, and `set_up_folders()` uses
    `cp -u` for the qemu binaries — which silently skips the copy when the dest
    is newer than the source. After a docker image rebuild, that mtime check
    can leave the experiment folder pinned to whatever binary the previous
    image had. Removing the folder forces a fresh copy from the freshly-built
    image's `parallel-qemu-saved/` so the test always exercises the binary
    the framework currently produces."""
    mounting = dev_container

    # Cleanup runs inside the container so root-owned files left by previous
    # qemu runs (qcow2 dirs etc.) can be removed without sudo on the host.
    # Also wipes the per-node qcow2 copies — `copy_image_for_node` uses `cp -u`
    # which won't refresh a per-node image whose mtime is newer than the
    # master, so a previous run's dirty state can persist into this run's boot.
    rm_targets = [f"{mounting}/experiments/{sub}" for sub in NODE_SUB_NAMES]
    # Per-node qcow2s live next to the master image: <image_folder>/<image_name>-node<N>.
    # The master is /mnt/sdc/data-caching-1c/root-single-node.qcow2; per-node
    # copies are root-single-node.qcow2-node0 / -node1.
    rm_targets.extend(
        f"{mounting}/root-single-node.qcow2-node{n}" for n in range(len(NODE_SUB_NAMES))
    )
    rm_cmd = " && ".join(f"rm -rf {t}" for t in rm_targets)
    rm_result = _exec_in_container(rm_cmd, timeout=120)
    assert rm_result.returncode == 0, (
        f"failed to clean experiment folders before test_01 "
        f"(rc={rm_result.returncode}). stderr:\n{rm_result.stderr}"
    )

    cmd = (
        "./qflex boot -c conf/DC/dc-multi-savevm-create.yaml"
    )
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

    cmd = (
        "./qflex load -c conf/DC/dc-multi-savevm-verify.yaml"
    )
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
