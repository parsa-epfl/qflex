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
"""
import os
import shutil
import subprocess
import sys

import pytest

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DEFAULT_MOUNTING = "/mnt/sdc/data-caching-1c/"
TEST_CONTAINER_NAME = "qflex_test"           # never collides with the user's qflex-dev
TEST_EXPERIMENT_NAME = "qflex_real_run_test"  # owned by this test file; safe to wipe


def _docker_available() -> bool:
    if shutil.which("docker") is None:
        return False
    try:
        r = subprocess.run(
            ["docker", "info"], capture_output=True, timeout=10
        )
        return r.returncode == 0
    except (subprocess.TimeoutExpired, OSError):
        return False


def _qflex_image_present() -> bool:
    """True if at least one ghcr.io/parsa-epfl/qflex image is locally available."""
    try:
        r = subprocess.run(
            ["docker", "image", "ls", "--format", "{{.Repository}}"],
            capture_output=True, text=True, timeout=10,
        )
    except (subprocess.TimeoutExpired, OSError):
        return False
    if r.returncode != 0:
        return False
    return any("parsa-epfl/qflex" in line for line in r.stdout.splitlines())


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


@pytest.fixture(scope="session")
def dev_container():
    """Session-scoped: start the QFlex dev container in background once (named
    `qflex_test`), yield the mounting folder, stop+remove on teardown.

    Subsequent tests use `./dep exec --container-name qflex_test` to talk to this
    container. The container name is intentionally distinct from the default
    `qflex-dev` so a user running `./dep start-docker --background` in another
    terminal isn't disturbed by the test suite.

    Mounting folder defaults to /mnt/sdc/data-caching-1c/; override via the
    QFLEX_REAL_RUN_MOUNTING env var. We always `./dep stop-docker` first to clear
    any leftover container from a prior crashed run."""
    mounting = os.environ.get("QFLEX_REAL_RUN_MOUNTING", DEFAULT_MOUNTING)
    if not os.path.isdir(mounting):
        pytest.skip(f"mounting folder does not exist: {mounting}")

    # The image variant must match what's locally tagged. The user's standard
    # workflow is `--worm --debug`, so default to that. Override via env vars if
    # you've built a different variant (e.g. release).
    def _envflag(name: str, default: str) -> bool:
        return os.environ.get(name, default).lower() not in ("0", "false", "no", "")

    use_worm = _envflag("QFLEX_REAL_RUN_WORM", "1")
    use_debug = _envflag("QFLEX_REAL_RUN_DEBUG", "1")

    # Best-effort cleanup of any prior test container.
    subprocess.run(
        ["./dep", "stop-docker", "--container-name", TEST_CONTAINER_NAME],
        cwd=REPO_ROOT, capture_output=True, timeout=30,
    )

    # Start fresh, detached, named `qflex_test`.
    start_cmd = [
        "./dep", "start-docker", "--mounting-folder", mounting,
        "--background", "--container-name", TEST_CONTAINER_NAME,
    ]
    if use_worm:
        start_cmd.append("--worm")
    if use_debug:
        start_cmd.append("--debug")
    start = subprocess.run(
        start_cmd,
        cwd=REPO_ROOT, capture_output=True, text=True, timeout=120,
    )
    if start.returncode != 0:
        pytest.skip(
            f"./dep start-docker --background failed (rc={start.returncode}):\n"
            f"stdout:\n{start.stdout}\nstderr:\n{start.stderr}"
        )

    # The published images predate the YAML/DI layer (`injector`, `omegaconf`)
    # and the libtmux Path B. Best-effort `pip install` of the new deps so any
    # qflex command that hits the DI loader (anything with `-c <yaml>`) imports
    # cleanly. Skip rather than fail if pip itself errors.
    pip = subprocess.run(
        ["./dep", "exec", "--container-name", TEST_CONTAINER_NAME,
         "--command", "pip install --quiet injector 'omegaconf>=2.3' libtmux"],
        cwd=REPO_ROOT, capture_output=True, text=True, timeout=300,
    )
    if pip.returncode != 0:
        subprocess.run(
            ["./dep", "stop-docker", "--container-name", TEST_CONTAINER_NAME],
            cwd=REPO_ROOT, capture_output=True, timeout=30,
        )
        pytest.skip(
            f"pip install of injector/omegaconf/libtmux failed inside the container "
            f"(rc={pip.returncode}):\n{pip.stderr[-800:]}"
        )

    try:
        yield mounting
    finally:
        subprocess.run(
            ["./dep", "stop-docker", "--container-name", TEST_CONTAINER_NAME],
            cwd=REPO_ROOT, capture_output=True, timeout=30,
        )


def _exec_in_container(command: str, timeout: int = 60) -> subprocess.CompletedProcess:
    """Helper: run `./dep exec --command "<bash>" --container-name qflex_test`
    and return the CompletedProcess."""
    return subprocess.run(
        ["./dep", "exec", "--command", command,
         "--container-name", TEST_CONTAINER_NAME],
        cwd=REPO_ROOT, capture_output=True, text=True, timeout=timeout,
    )


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
