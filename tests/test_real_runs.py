"""Real-run tests — actually invoke `./dep` to start a background dev container,
exec qflex commands inside it, and (in the heavyweight case) run a real QEMU boot.

Disabled by default. Enable by setting `QFLEX_REAL_RUN_TESTS=1` (and optionally
`QFLEX_REAL_RUN_MOUNTING=<path>` to override the default mounting folder).

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
)

pytestmark = [
    pytest.mark.skipif(
        not os.environ.get("QFLEX_REAL_RUN_TESTS"),
        reason="set QFLEX_REAL_RUN_TESTS=1 to enable real docker-based runs",
    ),
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
    """Boot Alpine, drive login + ls via tests/realrun/login_and_ls.exp,
    verify the captured ls output is non-empty."""
    mounting = dev_container

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
