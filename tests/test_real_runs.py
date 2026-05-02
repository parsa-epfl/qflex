"""Real-run tests — actually invoke `./dep` to start a background dev container,
exec qflex commands inside it, and (in the heavyweight case) run a real QEMU boot.

Disabled by default. Enable by setting `QFLEX_REAL_RUN_TESTS=1` (and optionally
`QFLEX_REAL_RUN_MOUNTING=<path>` to override the default mounting folder).

The container used here is named `qflex_test` — distinct from the `qflex-dev`
default — so it never collides with a manually-running container the user has
open in another terminal.

Pre-requisites:
- `docker` daemon reachable from the host running pytest.
- The qflex dev image present locally (or pullable from GHCR).
- The mounting folder exists on the host.
- For the alpine-login test specifically: the alpine image at
  `<mounting-folder>/<image_name>` already has a `qflex` user with password
  `qflex` configured (the user's existing setup).

The session fixture `dev_container` and helpers `_exec_in_container` /
`_docker_available` / `_qflex_image_present` live in tests/conftest.py so they
can be reused by other real-run files (e.g. test_real_runs_savevm.py).
"""
import os
import sys

import pytest

from .conftest import (
    _docker_available,
    _exec_in_container,
    _qflex_image_present,
)

# Module-level skip — applied to every test in this file unless overridden.
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


# ---------------------------------------------------------------------------
# Smoke: docker exec is wired correctly.
# ---------------------------------------------------------------------------

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


# ---------------------------------------------------------------------------
# Heavyweight: actually boot Alpine, log in via the sample expect script,
# capture the `ls` output, and verify the host can read it back.
# ---------------------------------------------------------------------------

TEST_EXPERIMENT_NAME = "qflex_real_run_test"  # owned by this test file


def test_alpine_login_and_ls_real_run(dev_container):
    """End-to-end: boot Alpine inside the dev container, drive the login prompt
    with qflex/qflex via sample_scripts/login_and_ls.exp (Path A), run `ls`,
    write the captured ls output to a host-visible file, then `quit` over the
    monitor.

    Asserts:
      * `./qflex boot` exits cleanly (rc=0).
      * The host can read `<mounting>/experiments/<exp>/ls_output.txt`.
      * The captured ls output is non-empty.

    The captured output is printed to stdout (visible with `pytest -s` or the
    `make test-verbose` target) so the user can eyeball what their image's
    qflex user actually has in $HOME.

    Pre-requisite (not asserted): the alpine image referenced in conf/DC/dc.yaml
    is present at the mounting folder and has a `qflex` user with password
    `qflex`. If the script times out at the login prompt, that's where to look
    first.

    Timeout is 15 minutes — Alpine boot + network + login + ls is comfortably
    under that, but leave headroom for slow hosts."""
    mounting = dev_container

    cmd = (
        f"./qflex boot -c conf/DC/dc.yaml "
        f"--experiment-name {TEST_EXPERIMENT_NAME} "
        f"--interaction-script /home/dev/qflex/sample_scripts/login_and_ls.exp"
    )
    r = _exec_in_container(cmd, timeout=900)
    assert r.returncode == 0, (
        f"alpine login + ls failed (rc={r.returncode}).\n"
        f"stdout:\n{r.stdout}\nstderr:\n{r.stderr}\n"
        "If the failure is a timeout: check that the alpine image at the "
        "mounting folder has the qflex/qflex user configured and that the "
        "interaction_script's expect timeouts are long enough."
    )

    ls_path = f"{mounting}/experiments/{TEST_EXPERIMENT_NAME}/ls_output.txt"
    assert os.path.exists(ls_path), (
        f"expected ls output at {ls_path} but the file is missing — "
        "did the expect script reach the ls step?"
    )
    with open(ls_path) as f:
        ls_output = f.read()

    # Echo the captured output so it shows up in `pytest -s` (`make test-verbose`).
    print(f"\n[alpine-ls] captured from {ls_path}:\n{ls_output}\n", file=sys.stderr)

    assert ls_output.strip(), (
        f"ls_output.txt exists but is empty — the expect script may not have "
        f"matched the prompt regex. Path: {ls_path}"
    )
