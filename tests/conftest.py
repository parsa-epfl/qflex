"""Test fixtures for the multi-node ordering tests.

Each test invokes a phase's executor on a 2-node multi config in dry-run, captures
stdout, and asserts on the [py-wait] / [py-touch] / [bash] markers. See
test_multi_node_ordering.py for the assertions; this file only provides setup.
"""
import io
import os
import re
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
    """Absolute path to sample_scripts/login_and_ls.exp.
    Used by the Path A interaction-script tests for both single-node and two-node
    boot configurations."""
    path = os.path.join(REPO_ROOT, "sample_scripts", "login_and_ls.exp")
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

    for sub_name in ("data-caching_yaml_node_0", "data-caching_yaml_node_1"):
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


@pytest.fixture
def multi_context(mock_mounting_folder):
    """Build the dc-multi.yaml ExperimentContext rooted at the temp mounting folder."""
    from dep_injection.builder import build_experiment_context
    overrides = {
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
    return build_experiment_context("conf/DC/dc-multi.yaml",
                                    component_overrides=overrides)
