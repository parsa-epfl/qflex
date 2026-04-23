import importlib.machinery
import importlib.util
import subprocess
import sys
from pathlib import Path
from unittest import mock


def _load_qflex_module():
    repo_root = Path(__file__).resolve().parents[1]
    script = repo_root / "qflex"
    if str(repo_root) not in sys.path:
        sys.path.insert(0, str(repo_root))
    loader = importlib.machinery.SourceFileLoader("qflex_cli", str(script))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


def test_qflex_qpoints_run_gem5_help_exposes_tracing_options():
    repo_root = Path(__file__).resolve().parents[1]
    script = repo_root / "qflex"

    result = subprocess.run(
        [sys.executable, str(script), "qpoints", "run-gem5", "--help"],
        check=True,
        capture_output=True,
        text=True,
    )

    assert "--branch-trace" in result.stdout
    assert "--data-trace" in result.stdout
    assert "--dump-cache-state" in result.stdout
    assert "--timing-ruby" in result.stdout
    assert "--sim-config" in result.stdout


def test_qpoints_run_gem5_forwards_tracing_options():
    module = _load_qflex_module()
    with mock.patch.object(module, "qpoints_run_gem5") as forwarded:
        module.qpoints_run_gem5_cmd(
            gem5_ckp_dir="/tmp/gem5_ckp",
            experiment="exp",
            snapshot="snapshot_0",
            inst=1000,
            core_count=1,
            branch_trace=True,
            data_trace=True,
            dump_cache_state=True,
            timing_ruby=True,
            sim_config="/tmp/override.args",
        )

    forwarded.assert_called_once_with(
        gem5_ckp_dir="/tmp/gem5_ckp",
        experiment="exp",
        snapshot="snapshot_0",
        inst=1000,
        core_count=1,
        branch_trace=True,
        data_trace=True,
        dump_cache_state=True,
        timing_ruby=True,
        sim_config="/tmp/override.args",
    )
