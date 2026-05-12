"""Static checks on sample_scripts/useful_commands/.

Each script is a thin wrapper over `python ./qflex <subcmd> -c $1`. Per-phase
invariants that the scripts USED to encode inline (--interactive-tmux on
boot/load, --syncs-list false on boot, --syncs-list true on init) now live in
the phase overlays of conf/DC/dc-multi.yaml — they apply uniformly whether
the user runs through a script, calls ./qflex directly, or builds a context
in python. The sweep scripts (run-partition-p, run-partition-p-idx) still
take $2/$3 for partition_number/idx so per-call overrides work without
writing N YAMLs.

End-to-end multi-node dispatch behavior for the same YAML is covered by the
per-phase tests against the `multi_context` fixture (test_boot.py,
test_load.py, …).
"""
import os
import subprocess

import pytest

from .conftest import REPO_ROOT

SCRIPTS = f"{REPO_ROOT}/sample_scripts/useful_commands"


def _read(name: str) -> str:
    return open(f"{SCRIPTS}/{name}").read()


@pytest.mark.parametrize("script,expected_substrings", [
    ("boot.sh",              ["./qflex boot"]),
    ("load.sh",              ["./qflex load"]),
    ("init.sh",              ["./qflex initialize"]),
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


@pytest.mark.parametrize("cmd_name,component,expected_fields", [
    # Boot: relaxed wire (Alpine deadlocks under sync=true / 100us) + tmux.
    ("boot", "experiment_context_node_0", {
        "interactive_tmux": True,
        "latencies_ns_list": [1000000],
        "syncs_list": ["false"],
    }),
    ("boot", "experiment_context_node_1", {
        "interactive_tmux": True,
        "latencies_ns_list": [1000000],
        "syncs_list": ["false"],
    }),
    # Load: tmux on; syncs_list stays at the leaf default (["true"]).
    ("load", "experiment_context_node_0", {
        "interactive_tmux": True,
        "syncs_list": ["true"],
    }),
    # Initialize: leaf default already carries syncs_list=["true"]; no overlay needed.
    ("initialize", "experiment_context_node_0", {
        "syncs_list": ["true"],
    }),
])
def test_dc_multi_yaml_phase_overlays(cmd_name, component, expected_fields):
    """conf/DC/dc-multi.yaml is the production YAML the convenience scripts
    pass through. Per-phase invariants must land on the resolved leaf for the
    right cmd_name — verify via the same loader the production code uses."""
    from dep_injection.config_loader import load_config
    cfg = load_config(f"{REPO_ROOT}/conf/DC/dc-multi.yaml", cmd_name=cmd_name)
    leaf = cfg["components"][component]
    for field, expected in expected_fields.items():
        actual = leaf.get(field)
        assert actual == expected, (
            f"{cmd_name}/{component}.{field}: expected {expected!r}, got {actual!r}"
        )


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
