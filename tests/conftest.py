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
    cls_name: str          # e.g. "Boot", "RunIdxCommand"
    waits: list[str]       # full sentinel paths from [py-wait]
    started: Optional[str] # sentinel path from the .started [py-touch]
    bash: str              # the [bash] line
    done: Optional[str]    # sentinel path from the .done [py-touch]
    raw: str               # the unparsed block text

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
            elif stripped.startswith("[bash]"):
                bash = stripped[len("[bash]"):].strip()
            i += 1
        blocks.append(DryRunBlock(
            cls_name=cls_name,
            waits=waits,
            started=started,
            bash=bash,
            done=done,
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
