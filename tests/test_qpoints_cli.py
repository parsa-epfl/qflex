import importlib.machinery
import importlib.util
import subprocess
import sys
from pathlib import Path
from unittest import mock

import pytest


def _load_qflex_module():
    repo_root = Path(__file__).resolve().parents[1]
    script = repo_root / "qflex"
    repo_root_str = str(repo_root)
    inserted_repo_root = False
    if repo_root_str not in sys.path:
        sys.path.insert(0, repo_root_str)
        inserted_repo_root = True
    try:
        loader = importlib.machinery.SourceFileLoader("qflex_cli", str(script))
        spec = importlib.util.spec_from_loader(loader.name, loader)
        module = importlib.util.module_from_spec(spec)
        loader.exec_module(module)
        return module
    finally:
        if inserted_repo_root and sys.path and sys.path[0] == repo_root_str:
            sys.path.pop(0)


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


def test_qpoints_run_gem5_rejects_cache_dump_without_ruby():
    module = _load_qflex_module()
    with pytest.raises(module.typer.BadParameter, match="requires --timing-ruby"):
        module.qpoints_run_gem5_cmd(
            gem5_ckp_dir="/tmp/gem5_ckp",
            experiment="exp",
            snapshot="snapshot_0",
            inst=1000,
            core_count=1,
            dump_cache_state=True,
            timing_ruby=False,
        )
