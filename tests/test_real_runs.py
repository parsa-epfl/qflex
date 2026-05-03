"""Real-run tests — actually invoke `./dep` to start a background dev container,
exec qflex commands inside it, and (in the heavyweight case) run a real QEMU boot.

Enabled by default (provided docker is reachable + the qflex image is local).
Set `QFLEX_SKIP_REAL_RUN=1` to disable. The bootstrap snapshot these tests
loadvm from is created by tests/test_real_runs_init.py, gated separately by
`QFLEX_INIT_TEST=1` — see that file's docstring.

All knobs the test depends on (experiment name, interaction_script, use_gdb,
loadvm_name, …) live in the per-test YAML under tests/realrun/. The python here
just `_exec_in_container("./qflex <phase> -c <yaml>")` and asserts on the
captured output file. No CLI flag overrides — see the testing skill for why.
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
)

pytestmark = [
    pytest.mark.skipif(_real_run_disabled(), reason="QFLEX_SKIP_REAL_RUN is set"),
    pytest.mark.skipif(not _docker_available(), reason="docker daemon not reachable"),
    pytest.mark.skipif(not _qflex_image_present(),
                       reason="ghcr.io/parsa-epfl/qflex image not present locally; "
                              "run `./dep build-docker` or pull it first"),
]


def test_dev_container_exec_smoke(dev_container):
    """The container is up and `./dep exec` round-trips a trivial command."""
    r = _exec_in_container("echo qflex-real-run-ok", timeout=30)
    assert r.returncode == 0, f"./dep exec failed:\nstdout:\n{r.stdout}\nstderr:\n{r.stderr}"
    assert "qflex-real-run-ok" in r.stdout


def test_qflex_help_inside_container(dev_container):
    """`./qflex --help` runs inside the container and returns help text. Validates
    that the qflex script + its imports work in the dev image."""
    r = _exec_in_container("./qflex --help", timeout=60)
    assert r.returncode == 0, f"./qflex --help failed:\nstdout:\n{r.stdout}\nstderr:\n{r.stderr}"
    assert "Usage" in r.stdout or "usage" in r.stdout.lower()


# Mirrors `experiment_name` in tests/realrun/dc-alpine-login.yaml — the only
# value the python side needs to read back so it can locate the captured file.
ALPINE_EXPERIMENT_NAME = "qflex_real_run_test"


def test_alpine_login_and_ls_real_run(dev_container):
    """Boot Alpine from the `boot-login` snapshot, drive login + ls via
    tests/realrun/login_and_ls.exp, verify the captured ls output is non-empty."""
    mounting = dev_container

    # Skip cleanly if the bootstrap snapshot hasn't been created yet. We check
    # the external .zstd file (which carries the real VM state) rather than
    # only the qcow2 internal entry — `delvm` removes the qcow2 entry but not
    # the .zstd, so the two can disagree.
    run_dir = f"{mounting}/experiments/{ALPINE_EXPERIMENT_NAME}/run"
    require_boot_login_zstd([run_dir])

    # No experiment-folder wipe: init shares this folder and drops boot-login.zstd
    # into run/ — wiping would discard the snapshot. set_up_folders() reuses the
    # existing folder safely (it cp -u's binaries and only writes core_info.csv
    # if absent; init created it with the correct core_count / doubled_vcpu).

    r = _exec_in_container("./qflex boot -c tests/realrun/dc-alpine-login.yaml", timeout=900)
    assert r.returncode == 0, (
        f"alpine login + ls failed (rc={r.returncode}).\n"
        f"stdout:\n{r.stdout}\nstderr:\n{r.stderr}"
    )

    ls_path = f"{mounting}/experiments/{ALPINE_EXPERIMENT_NAME}/ls_output.txt"
    assert os.path.exists(ls_path), (
        f"expected ls output at {ls_path} but the file is missing — "
        "did the expect script reach the ls step?"
    )
    with open(ls_path) as f:
        ls_output = f.read()
    print(f"\n[alpine-ls] captured from {ls_path}:\n{ls_output}\n", file=sys.stderr)

    assert ls_output.strip(), f"ls_output.txt exists but is empty — path: {ls_path}"
