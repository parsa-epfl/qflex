"""Shared pytest fixtures + helpers used by every test file under tests/.

* The dry-run side: `parse_dry_run_blocks`, `DryRunBlock`, `capture_dry_run_stdout`,
  `mock_mounting_folder`, `multi_context`, `pin_per_sub`, `assert_two_node_master_first`.
* The real-run side: `dev_container` session fixture (start/stop the
  `qflex_test` container via `./dep`) plus `_docker_available`, `_qflex_image_present`
  and `_exec_in_container` so test_chained_pipeline.py / test_dev_*.py / test_boot_login_bootstrap.py files can share them.
"""
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile
import contextlib
from dataclasses import dataclass
from typing import Optional

import pytest

# Make the repo root importable when pytest is run from here.
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

# Constants shared by every real-run test file.
DEFAULT_MOUNTING = "/mnt/sdc/data-caching-1c/"
TEST_CONTAINER_NAME = "qflex_test"            # never collides with the user's qflex-dev


@dataclass
class DryRunBlock:
    """One [dry-run] entry parsed out of stdout."""
    cls_name: str            # e.g. "Boot", "RunIdxCommand"
    waits: list[str]         # full sentinel paths from [py-wait]
    started: Optional[str]   # sentinel path from the .started [py-touch]
    bash: str                # the [bash] line
    done: Optional[str]      # sentinel path from the .done [py-touch] OR [py-poll] (tmux mode)
    tmux_window: Optional[str] = None  # window name from [tmux] marker (Path B)
    raw: str = ""            # the unparsed block text

    @property
    def waits_basenames(self) -> list[str]:
        return [os.path.basename(p) for p in self.waits]

    @property
    def started_basename(self) -> Optional[str]:
        return os.path.basename(self.started) if self.started else None

    @property
    def done_basename(self) -> Optional[str]:
        return os.path.basename(self.done) if self.done else None


def parse_dry_run_blocks(stdout: str) -> list[DryRunBlock]:
    """Parse all [dry-run] <Class> blocks out of captured stdout. Returns them in order."""
    blocks: list[DryRunBlock] = []
    lines = stdout.split("\n")
    i = 0
    while i < len(lines):
        m = re.match(r"^\[dry-run\] (\w+) \(cwd=", lines[i])
        if not m:
            i += 1
            continue
        cls_name = m.group(1)
        waits: list[str] = []
        started = None
        bash = ""
        done = None
        tmux_window = None
        raw_lines = [lines[i]]
        i += 1
        while i < len(lines) and lines[i].startswith("  "):
            raw_lines.append(lines[i])
            stripped = lines[i].strip()
            if stripped.startswith("[py-wait]"):
                waits.append(stripped[len("[py-wait]"):].strip())
            elif stripped.startswith("[py-touch]"):
                path = stripped[len("[py-touch]"):].strip()
                if path.endswith(".started"):
                    started = path
                elif path.endswith(".done"):
                    done = path
            elif stripped.startswith("[py-poll]"):
                # tmux mode: Python polls for the sentinel that the bash itself touches.
                done = stripped[len("[py-poll]"):].strip()
            elif stripped.startswith("[tmux]"):
                rest = stripped[len("[tmux]"):].strip()
                # Extract the window name from "would open new window 'Foo' and ..."
                wm = re.search(r"window '([^']+)'", rest)
                tmux_window = wm.group(1) if wm else rest
            elif stripped.startswith("[bash]"):
                bash = stripped[len("[bash]"):].strip()
            i += 1
        blocks.append(DryRunBlock(
            cls_name=cls_name,
            waits=waits,
            started=started,
            bash=bash,
            done=done,
            tmux_window=tmux_window,
            raw="\n".join(raw_lines),
        ))
    return blocks


@contextlib.contextmanager
def capture_dry_run_stdout():
    """Capture stdout while QFLEX_DRY_RUN=1 is set in the environment."""
    buf = io.StringIO()
    old_env = os.environ.get("QFLEX_DRY_RUN")
    os.environ["QFLEX_DRY_RUN"] = "1"
    try:
        with contextlib.redirect_stdout(buf):
            yield buf
    finally:
        if old_env is None:
            os.environ.pop("QFLEX_DRY_RUN", None)
        else:
            os.environ["QFLEX_DRY_RUN"] = old_env


@pytest.fixture
def login_ls_script() -> str:
    """Absolute path to tests/realrun/login_and_ls.exp.
    Used by the Path A interaction-script tests for both single-node and two-node
    boot configurations."""
    path = os.path.join(REPO_ROOT, "tests", "realrun", "login_and_ls.exp")
    assert os.path.exists(path), f"sample script missing: {path}"
    return path


# ----- Shared assertion helpers used by every per-component test file ------

def pin_per_sub(multi_context, **field_updates):
    """Clone each sub-experiment with the given field updates and rebuild the top
    group with the pinned subs. Returns the new group context."""
    pinned = [s.model_copy(update=field_updates) for s in multi_context.sub_experiments]
    return multi_context.model_copy(update={"sub_experiments": pinned})


def assert_two_node_master_first(blocks, leaf_cls_name: str,
                                 expected_basename_prefix: str):
    """Assert the standard 2-node leaf-ordering invariant on a list of dry-run blocks
    that contain at least two master+node-1 leaves of the given class.

    Pairs leaves by (phase[, part, idx]) and asserts: master has no [py-wait]; node 1
    waits exactly on master's matching .started; master appears before node 1 in
    dispatch order. See the testing skill for the full rationale."""
    leaves = [b for b in blocks if b.cls_name == leaf_cls_name]
    assert len(leaves) >= 2, (
        f"expected at least 2 {leaf_cls_name} leaves, got {len(leaves)}: "
        f"{[b.cls_name for b in blocks]}"
    )

    # Pair up master + node-1 leaves by sentinel suffix (everything except _nodeN).
    by_pair: dict[str, dict[int, object]] = {}
    for b in leaves:
        assert b.started is not None, f"missing .started touch on {b.cls_name}: {b.raw}"
        base = b.started_basename
        # base looks like e.g. "RunIdxCommand_part0_idx1_node0.started"
        assert base.startswith(expected_basename_prefix), (
            f"{b.cls_name} sentinel '{base}' missing expected prefix "
            f"'{expected_basename_prefix}'"
        )
        # extract the leading shared key (everything before _nodeN.started)
        key = base.rsplit("_node", 1)[0]
        node = int(base.rsplit("_node", 1)[1].split(".")[0])
        by_pair.setdefault(key, {})[node] = b

    assert by_pair, "found no master/node-1 leaf pairs"
    for key, nodes in by_pair.items():
        assert 0 in nodes and 1 in nodes, (
            f"missing pair member for key {key!r}: have nodes {list(nodes)}"
        )
        master, follower = nodes[0], nodes[1]

        # Master leaf: no waits, .started touch, bash, .done touch.
        assert master.waits == [], (
            f"master leaf for {key} should not wait, has {master.waits}"
        )
        assert master.started_basename.endswith("_node0.started")
        assert master.bash, f"master leaf for {key} has empty bash"
        assert master.done is not None, f"master leaf for {key} missing .done"
        assert master.done_basename.endswith("_node0.done")

        # Non-master leaf: waits on master's .started, then its own touch.
        assert len(follower.waits) == 1, (
            f"follower leaf for {key} should wait on exactly one sentinel, "
            f"got {follower.waits}"
        )
        assert follower.waits_basenames[0] == master.started_basename, (
            f"follower for {key} waits on {follower.waits_basenames[0]}, "
            f"expected {master.started_basename}"
        )
        assert follower.started_basename.endswith("_node1.started")
        assert follower.done is not None
        assert follower.done_basename.endswith("_node1.done")

    # Master appears before follower in the dispatch order (dry-run is sequential).
    for key, nodes in by_pair.items():
        master_idx = blocks.index(nodes[0])
        follower_idx = blocks.index(nodes[1])
        assert master_idx < follower_idx, (
            f"for {key}, master leaf at idx {master_idx} should precede "
            f"follower leaf at idx {follower_idx}"
        )


@pytest.fixture
def mock_mounting_folder(tmp_path):
    """Per-test temp mounting folder pre-populated with the artifacts each phase
    needs: per-node experiment folders + a few partitions/snapshots for the
    timing-phase tests + the result-phase trigger files."""
    mf = str(tmp_path)
    os.makedirs(f"{mf}/images", exist_ok=True)

    for sub_name in ("data-caching-comparison-node-0", "data-caching-comparison-node-1"):
        exp_folder = f"{mf}/experiments/{sub_name}"
        os.makedirs(f"{exp_folder}/run", exist_ok=True)
        os.makedirs(f"{exp_folder}/cfg", exist_ok=True)
        os.makedirs(f"{exp_folder}/scripts", exist_ok=True)

        # Partition phase preconditions: scripts/run_flexus.sh and partition.py.
        open(f"{exp_folder}/scripts/run_flexus.sh", "w").close()
        open(f"{exp_folder}/partition.py", "w").close()

        # Result phase preconditions.
        open(f"{exp_folder}/result.py", "w").close()
        open(f"{exp_folder}/run_partitions.sh", "w").close()

        # Two partitions × two snapshots so RunPartitionCommand and friends find work.
        for p in (0, 1):
            pf = f"{exp_folder}/run/partition_{p}"
            os.makedirs(pf, exist_ok=True)
            for idx in (0, 1):
                open(f"{pf}/snapshot_{idx}.loc", "w").close()

    return mf


def _multi_context_overrides(mock_mounting_folder):
    return {
        "experiment_context": {
            "mounting_folder": mock_mounting_folder,
            "image_folder": mock_mounting_folder,
        },
        "experiment_context_node_0": {
            "mounting_folder": mock_mounting_folder,
            "image_folder": mock_mounting_folder,
        },
        "experiment_context_node_1": {
            "mounting_folder": mock_mounting_folder,
            "image_folder": mock_mounting_folder,
        },
    }


@pytest.fixture
def multi_context(mock_mounting_folder):
    """Build the dc-multi.yaml ExperimentContext rooted at the temp mounting folder."""
    from dep_injection.builder import build_experiment_context
    return build_experiment_context("conf/DC/dc-multi.yaml",
                                    component_overrides=_multi_context_overrides(mock_mounting_folder))


@pytest.fixture
def multi_context_for_phase(mock_mounting_folder):
    """Factory: returns build_experiment_context(...) with the phase overlay
    applied for the given cmd_name. Use this in tests that need to exercise
    per-phase YAML knobs (sample_size, warming_ratio, measurement_ratio, ...)
    that production reaches via the loader's _apply_phase_overlay path."""
    from dep_injection.builder import build_experiment_context
    overrides = _multi_context_overrides(mock_mounting_folder)

    def _build(cmd_name: str):
        return build_experiment_context("conf/DC/dc-multi.yaml",
                                        cmd_name=cmd_name,
                                        component_overrides=overrides)
    return _build


# ============================================================================
# Real-run helpers — shared by tests/test_chained_pipeline.py / test_dev_*.py / test_boot_login_bootstrap.py.
#
# Real-run tests are ENABLED by default (the qcow2 snapshot bootstrapped by
# tests/test_boot_login_bootstrap.py keeps them fast — minutes instead of dozens of
# minutes). Set QFLEX_SKIP_REAL_RUN=1 to disable them.
#
# The init tests under tests/test_boot_login_bootstrap.py are gated separately by
# QFLEX_INIT_TEST=1 because they DO take a full Alpine boot — they only need
# to be re-run after the qcow2 is reset or QEMU binaries change.
# ============================================================================


def _real_run_disabled() -> bool:
    return os.environ.get("QFLEX_SKIP_REAL_RUN", "").lower() in ("1", "true", "yes")


def _qcow2_has_snapshot(qcow2_path: str, snapshot_name: str) -> bool:
    """True iff `qemu-img snapshot -l <qcow2>` lists a snapshot tagged
    `snapshot_name`. Used by the regular real-run tests to skip when the
    boot-login snapshot hasn't been bootstrapped (init tests not yet run)."""
    if not os.path.exists(qcow2_path):
        return False
    try:
        r = subprocess.run(
            ["qemu-img", "snapshot", "-l", qcow2_path],
            capture_output=True, text=True, timeout=30,
        )
    except (subprocess.TimeoutExpired, OSError):
        return False
    if r.returncode != 0:
        return False
    # Output columns: ID TAG VM_SIZE DATE VM_CLOCK ICOUNT — match on the TAG column.
    for line in r.stdout.splitlines():
        toks = line.split()
        if len(toks) >= 2 and toks[1] == snapshot_name:
            return True
    return False


def require_snapshot(qcow2_paths: list[str], snapshot_name: str,
                     bootstrap_hint: str) -> None:
    """pytest.skip if any of `qcow2_paths` is missing the named snapshot.
    Tests call this from the body (after the dev_container fixture) so the skip
    message can name the missing files concretely."""
    missing = [p for p in qcow2_paths if not _qcow2_has_snapshot(p, snapshot_name)]
    if missing:
        pytest.skip(
            f"missing {snapshot_name!r} snapshot in: "
            + ", ".join(missing)
            + f" — {bootstrap_hint}"
        )


def require_boot_login_snapshot(qcow2_paths: list[str]) -> None:
    """Convenience wrapper: skip if `boot-login` is missing, with the canonical
    init-test bootstrap hint."""
    require_snapshot(
        qcow2_paths,
        "boot-login",
        "bootstrap with `QFLEX_INIT_TEST=1 make test-real-one "
        "TEST=tests/test_boot_login_bootstrap.py`",
    )


def require_boot_login_zstd(experiment_run_dirs: list[str]) -> None:
    """Skip if `boot-login.zstd` is missing in any of the given experiment
    `<run/>` dirs. parallel-qemu's savevm writes the actual VM state to an
    external `<name>.zstd` next to the qemu cwd; the qcow2 internal entry is
    just a bookkeeping pointer and isn't sufficient on its own — `loadvm`
    fails with "Not a migration stream" when the data file is missing."""
    missing = [
        f"{d}/boot-login.zstd"
        for d in experiment_run_dirs
        if not os.path.exists(f"{d}/boot-login.zstd")
    ]
    if missing:
        pytest.skip(
            "missing boot-login.zstd in: "
            + ", ".join(missing)
            + " — bootstrap with `QFLEX_INIT_TEST=1 make test-real-one "
            "TEST=tests/test_boot_login_bootstrap.py`"
        )


def _docker_available() -> bool:
    if shutil.which("docker") is None:
        return False
    try:
        r = subprocess.run(["docker", "info"], capture_output=True, timeout=10)
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


def _container_running(name: str) -> bool:
    """True if a docker container with this exact name is currently running."""
    try:
        r = subprocess.run(
            ["docker", "ps", "-q", "-f", f"name=^{name}$"],
            capture_output=True, text=True, timeout=10,
        )
    except (subprocess.TimeoutExpired, OSError):
        return False
    return r.returncode == 0 and r.stdout.strip() != ""


def _exec_in_container(command: str, timeout: int = 60) -> subprocess.CompletedProcess:
    """Run `./dep exec --command "<bash>" --container-name qflex_test`.
    Returns the CompletedProcess so callers can assert on rc / stdout / stderr."""
    return subprocess.run(
        ["./dep", "exec", "--command", command,
         "--container-name", TEST_CONTAINER_NAME],
        cwd=REPO_ROOT, capture_output=True, text=True, timeout=timeout,
    )


@pytest.fixture(scope="session")
def dev_container():
    """Session-scoped: start the QFlex dev container in background once (named
    `qflex_test`), yield the host mounting folder, stop+remove on teardown.

    The container name is intentionally distinct from the default `qflex-dev` so
    a user running `./dep start-docker --background` in another terminal isn't
    disturbed by the test suite.

    Mounting folder defaults to /mnt/sdc/data-caching-1c/; override via the
    QFLEX_REAL_RUN_MOUNTING env var. Image variant defaults to --worm --debug
    (the user's standard); override via QFLEX_REAL_RUN_WORM / QFLEX_REAL_RUN_DEBUG.

    Always `./dep stop-docker` first to clear any leftover container from a
    prior crashed run.
    """
    if _real_run_disabled():
        pytest.skip("QFLEX_SKIP_REAL_RUN is set")
    if not _docker_available():
        pytest.skip("docker daemon not reachable")
    if not _qflex_image_present():
        pytest.skip("ghcr.io/parsa-epfl/qflex image not present locally; "
                    "run `./dep build-docker` or pull it first")

    mounting = os.environ.get("QFLEX_REAL_RUN_MOUNTING", DEFAULT_MOUNTING)
    if not os.path.isdir(mounting):
        pytest.skip(f"mounting folder does not exist: {mounting}")

    def _envflag(name: str, default: str) -> bool:
        return os.environ.get(name, default).lower() not in ("0", "false", "no", "")

    use_worm = _envflag("QFLEX_REAL_RUN_WORM", "1")
    use_debug = _envflag("QFLEX_REAL_RUN_DEBUG", "1")

    # If `qflex_test` is already running (e.g. left up by `make test-iterate`
    # so the recompiled qemu/parallel-qemu binaries inside it are used by
    # this real-run session), REUSE it: don't tear down at start, don't tear
    # down at end. Otherwise: fresh start, fresh stop, as before.
    pre_existing = _container_running(TEST_CONTAINER_NAME)

    if not pre_existing:
        subprocess.run(
            ["./dep", "stop-docker", "--container-name", TEST_CONTAINER_NAME],
            cwd=REPO_ROOT, capture_output=True, timeout=30,
        )

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

    # Sweep stale qemu processes / `/dev/shm/pdes_*` files left by prior runs
    # (the user may have been driving qflex-dev manually in parallel; the
    # container is `--pid=host` so host-level leftovers would otherwise
    # collide with our test's ports / shm names).
    subprocess.run(
        ["./dep", "exec", "--container-name", TEST_CONTAINER_NAME,
         "--command", "./clean_up.sh"],
        cwd=REPO_ROOT, capture_output=True, text=True, timeout=60,
    )

    try:
        yield mounting
    finally:
        if not pre_existing:
            subprocess.run(
                ["./dep", "stop-docker", "--container-name", TEST_CONTAINER_NAME],
                cwd=REPO_ROOT, capture_output=True, timeout=30,
            )


# ============================================================================
# Per-test summary table — markdown style (Test / Time / Status). Prints at
# the very bottom of every `make test` run so wall-clock per real-run test is
# visible at a glance without scrolling back through pytest's verbose output.
# ============================================================================


def _format_duration(seconds: float) -> str:
    if seconds < 60:
        return f"{seconds:.2f}s"
    m, s = divmod(int(seconds), 60)
    if m < 60:
        return f"{m}:{s:02d}"
    h, m = divmod(m, 60)
    return f"{h}:{m:02d}:{s:02d}"


@pytest.hookimpl(trylast=True)
def pytest_terminal_summary(terminalreporter, exitstatus, config):
    """Markdown-style table of every collected test (name | time | status).
    Aggregates per nodeid using the `call`-phase duration when present, falling
    back to whatever phase the report was emitted from (setup-phase reports
    are what skipif emits, for example)."""
    status_map = {"passed": "PASS", "failed": "FAIL",
                  "skipped": "SKIP", "error": "ERROR"}
    rows: dict[str, tuple[float, str]] = {}
    for category, reports in terminalreporter.stats.items():
        if category not in status_map:
            continue
        for report in reports:
            nid = getattr(report, "nodeid", None)
            if not nid:
                continue
            duration = getattr(report, "duration", 0.0)
            existing = rows.get(nid)
            # Prefer the `call`-phase report (real test body duration) over
            # setup/teardown reports for the same nodeid.
            if existing is None or getattr(report, "when", None) == "call":
                rows[nid] = (duration, status_map[category])

    if not rows:
        return

    name_w = max(len("Test"), max(len(n) for n in rows))
    time_strs = {n: _format_duration(d) for n, (d, _) in rows.items()}
    time_w = max(len("Time"), max(len(t) for t in time_strs.values()))
    status_w = max(len("Status"), max(len(s) for _, s in rows.values()))

    tw = terminalreporter
    tw.write_sep("=", "per-test summary")
    tw.write_line(f"| {'Test':<{name_w}} | {'Time':<{time_w}} | {'Status':<{status_w}} |")
    tw.write_line(f"|{'-' * (name_w + 2)}|{'-' * (time_w + 2)}|{'-' * (status_w + 2)}|")
    for nid in sorted(rows):
        duration, status = rows[nid]
        tw.write_line(
            f"| {nid:<{name_w}} | {time_strs[nid]:<{time_w}} | {status:<{status_w}} |"
        )

