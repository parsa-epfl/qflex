"""Real-run tests that verify the dev docker image has the fixes the user
previously applied by hand from multi-node-dc/fixes.sh and docker_fixes.sh.

Each test asserts one image-baked invariant and is independent of the others —
if one regresses we want to see exactly which fix dropped out.

Same gate as tests/test_real_runs.py: enabled by default; set
QFLEX_SKIP_REAL_RUN=1 to disable, plus docker reachable + qflex image local.
Reuses the session-scoped `dev_container` fixture from tests/conftest.py so
every assertion runs against the same long-lived container.
"""
import os

import pytest

from .conftest import (
    _docker_available,
    _exec_in_container,
    _qflex_image_present,
    _real_run_disabled,
)
from commands.docker import _read_host_dns

# Discover DNS once at import time so each parametrized test gets a stable
# label and we can fall back gracefully when the host has no usable upstream.
_HOST_DNS_SERVERS, _HOST_DNS_SEARCH = _read_host_dns()

pytestmark = [
    pytest.mark.skipif(_real_run_disabled(), reason="QFLEX_SKIP_REAL_RUN is set"),
    pytest.mark.skipif(not _docker_available(), reason="docker daemon not reachable"),
    pytest.mark.skipif(not _qflex_image_present(),
                       reason="ghcr.io/parsa-epfl/qflex image not present locally; "
                              "run `./dep build-docker` or pull it first"),
]


def _ok(r, what):
    assert r.returncode == 0, (
        f"{what} failed inside dev container (rc={r.returncode}).\n"
        f"stdout:\n{r.stdout}\nstderr:\n{r.stderr}"
    )


# ---------------------------------------------------------------------------
# Packages added by fixes.sh that used to be missing.
# ---------------------------------------------------------------------------

def test_iputils_ping_installed(dev_container):
    """`ping` from iputils-ping must be on PATH — used to be the very first
    thing the user installed by hand after every `./dep start-docker`."""
    r = _exec_in_container("command -v ping", timeout=15)
    _ok(r, "command -v ping")
    assert "/ping" in r.stdout, f"unexpected `command -v ping` output: {r.stdout!r}"


# ---------------------------------------------------------------------------
# Profiling toolchain (perf + Rust + inferno) — fixes.sh L77-81.
# ---------------------------------------------------------------------------

def test_perf_resolves_to_a_binary(dev_container):
    """`perf --version` must produce a version line. The Dockerfile installs
    linux-tools-generic + a /usr/local/bin/perf shim that points at whatever
    version-suffixed perf binary the package shipped, so this should work even
    when the running kernel doesn't match the kernel headers in the image."""
    r = _exec_in_container("perf --version", timeout=15)
    _ok(r, "perf --version")
    assert "perf version" in r.stdout.lower(), (
        f"`perf --version` returned no version line: {r.stdout!r}"
    )


def test_rust_toolchain_present(dev_container):
    """rustc + cargo must be on PATH (installed via rustup in the base image,
    with $CARGO_HOME/bin prepended to PATH via /etc/bash.bashrc and ENV)."""
    r = _exec_in_container("rustc --version && cargo --version", timeout=30)
    _ok(r, "rustc/cargo --version")
    assert "rustc " in r.stdout, f"missing rustc in: {r.stdout!r}"
    assert "cargo " in r.stdout, f"missing cargo in: {r.stdout!r}"


@pytest.mark.parametrize("binary", ["inferno-flamegraph", "inferno-collapse-perf"])
def test_inferno_binary_on_path(dev_container, binary):
    """`cargo install inferno` should have placed both binaries in $CARGO_HOME/bin
    (which is on PATH). Both are needed for the perf -> folded -> svg/speedscope
    pipeline that produces the multi-node-dc/out.folded artifacts."""
    r = _exec_in_container(f"{binary} --help | head -5", timeout=30)
    _ok(r, f"{binary} --help")
    # Inferno's help text always starts with "Usage:".
    assert "usage" in r.stdout.lower(), (
        f"`{binary} --help` produced no usage banner: {r.stdout!r}"
    )


# ---------------------------------------------------------------------------
# DNS — applied via `docker run --dns`, not baked into the image (Docker
# overwrites /etc/resolv.conf on every run). DNS values are *discovered* from
# the host (see commands.docker._read_host_dns), so the assertions parametrize
# over whatever the host advertised at module-import time. Skip cleanly on a
# host with no usable upstream resolvers.
# ---------------------------------------------------------------------------

@pytest.mark.skipif(not _HOST_DNS_SERVERS,
                    reason="host has no usable upstream DNS servers — nothing to assert")
@pytest.mark.parametrize("server", _HOST_DNS_SERVERS)
def test_host_dns_server_propagates_to_container(dev_container, server):
    """Every non-localhost nameserver discovered on the host should appear as a
    `nameserver` line inside the container's resolv.conf. Proves DockerStarter
    is passing the discovered list as `--dns` flags and that Docker isn't
    silently dropping any of them."""
    r = _exec_in_container("cat /etc/resolv.conf", timeout=10)
    _ok(r, "cat /etc/resolv.conf")
    assert f"nameserver {server}" in r.stdout, (
        f"expected `nameserver {server}` in container resolv.conf, got:\n{r.stdout}"
    )


@pytest.mark.skipif(not _HOST_DNS_SEARCH,
                    reason="host advertises no DNS search domains — nothing to assert")
@pytest.mark.parametrize("domain", _HOST_DNS_SEARCH)
def test_host_dns_search_propagates_to_container(dev_container, domain):
    """Every search domain discovered on the host should appear in the
    container's `search` line. Proves --dns-search wiring."""
    r = _exec_in_container("cat /etc/resolv.conf", timeout=10)
    _ok(r, "cat /etc/resolv.conf")
    search_lines = [ln for ln in r.stdout.splitlines() if ln.startswith("search ")]
    assert search_lines, f"no `search` line in resolv.conf:\n{r.stdout}"
    joined = " ".join(search_lines)
    assert domain in joined, (
        f"expected search domain `{domain}` in resolv.conf, got:\n{r.stdout}"
    )
