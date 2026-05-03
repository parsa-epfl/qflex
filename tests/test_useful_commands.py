"""Static checks on sample_scripts/useful_commands/.

Each script is a thin wrapper over `python ./qflex <subcmd> -c $1`. We verify:
  - the right subcommand is invoked,
  - script-specific flags (--interactive-tmux on boot/load, --syncs-list on
    boot/init) are present,
  - the file shell-parses.

End-to-end multi-node dispatch behavior for these YAMLs is already covered by
the per-phase tests against the `multi_context` fixture (built from the same
conf/DC/dc-multi.yaml the scripts pass through) — see test_boot.py,
test_load.py, test_init_warm.py, test_functional_warming.py, test_partition.py,
test_run_partition.py, test_run_single_partition.py, test_run_idx.py,
test_result.py.
"""
import os
import subprocess

import pytest

from .conftest import REPO_ROOT

SCRIPTS = f"{REPO_ROOT}/sample_scripts/useful_commands"


def _read(name: str) -> str:
    return open(f"{SCRIPTS}/{name}").read()


@pytest.mark.parametrize("script,expected_substrings", [
    ("boot.sh",              ["./qflex boot",                "--interactive-tmux", "--syncs-list false"]),
    ("load.sh",              ["./qflex load",                "--interactive-tmux"]),
    ("init.sh",              ["./qflex initialize",          "--syncs-list true"]),
    ("fw.sh",                ["./qflex fw"]),
    ("partition.sh",         ["./qflex partition "]),
    ("run-partition.sh",     ["./qflex run-partition "]),
    ("run-partition-p.sh",   ["./qflex run-single-partition", "--partition-number $2"]),
    ("run-partition-p-idx.sh", ["./qflex run-idx",            "--partition-number $2", "--idx $3"]),
    ("result.sh",            ["./qflex result"]),
    ("rm-partition.sh",      ["./qflex unpartition"]),
])
def test_script_invokes_expected_qflex_command(script, expected_substrings):
    body = _read(script)
    for s in expected_substrings:
        assert s in body, f"{script}: missing expected fragment '{s}' in:\n{body}"


def test_cleanup_targets_clean_up_sh():
    body = _read("cleanup.sh")
    assert "./clean_up.sh" in body
    assert os.path.exists(f"{REPO_ROOT}/clean_up.sh"), \
        "cleanup.sh expects ./clean_up.sh at the repo root"


@pytest.mark.parametrize("script", [
    "boot.sh", "load.sh", "init.sh", "fw.sh", "partition.sh",
    "run-partition.sh", "run-partition-p.sh", "run-partition-p-idx.sh",
    "result.sh", "rm-partition.sh", "cleanup.sh",
])
def test_script_shell_parses(script):
    r = subprocess.run(["bash", "-n", f"{SCRIPTS}/{script}"],
                       capture_output=True, text=True)
    assert r.returncode == 0, f"{script} failed shell parse: {r.stderr}"
