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

Additional gate: the multi-node PDES netdev's `latencyns` parameter is required
here (every leaf in `dc-multi.yaml` uses `latencies_ns_list`). Older pre-built
dev images (≲ Nov 2024) ship a parallel-qemu that doesn't recognize that
parameter and these tests skip instead of failing confusingly. Rebuild via
`./dep build-docker --worm --debug` to get a current binary.
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


def test_01_boot_two_nodes_create_files_and_savevm(
    dev_container, parallel_qemu_supports_latencyns
):
    """Boot both nodes, login, `touch test{N+1}.txt` per node, ls, savevm,
    quit. Verify each node's ls captured the right file."""
    if not parallel_qemu_supports_latencyns:
        pytest.skip(
            "dev image's parallel-qemu predates the `latencyns` pdes netdev "
            "parameter. Rebuild via `./dep build-docker --worm --debug` to enable "
            "multi-node real-run tests."
        )
    mounting = dev_container

    cmd = (
        "./qflex boot -c conf/DC/dc-multi-savevm-create.yaml"
    )
    r = _exec_in_container(cmd, timeout=900)
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


def test_02_load_two_nodes_verify_files(
    dev_container, parallel_qemu_supports_latencyns
):
    """Load both nodes from the snapshot, ls, quit. Verify each node's file
    survived the savevm/loadvm round-trip."""
    if not parallel_qemu_supports_latencyns:
        pytest.skip(
            "dev image's parallel-qemu predates the `latencyns` pdes netdev "
            "parameter. Rebuild via `./dep build-docker --worm --debug` to enable "
            "multi-node real-run tests."
        )
    mounting = dev_container

    cmd = (
        "./qflex load -c conf/DC/dc-multi-savevm-verify.yaml"
    )
    r = _exec_in_container(cmd, timeout=600)
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
