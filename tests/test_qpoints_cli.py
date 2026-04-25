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


def _load_qpoints_commands_module():
    repo_root = Path(__file__).resolve().parents[1]
    repo_root_str = str(repo_root)
    inserted_repo_root = False
    if repo_root_str not in sys.path:
        sys.path.insert(0, repo_root_str)
        inserted_repo_root = True
    try:
        from commands import qpoints as module

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


def test_prepare_snapshot_gem5_uarch_skips_when_qflex_uarch_missing(capsys):
    module = _load_qpoints_commands_module()
    with mock.patch.object(module.subprocess, "run") as run_mock:
        module._prepare_snapshot_gem5_uarch(
            qpoints_root=Path("/tmp/qpoints-root"),
            qflex_ckp_dir="/tmp/qflex-ckpts",
            gem5_ckp_dir="/tmp/gem5-ckpts",
            snapshot="snapshot_0",
        )

    run_mock.assert_not_called()
    assert "skipping gem5 uarch preparation" in capsys.readouterr().out


def test_prepare_snapshot_gem5_uarch_invokes_qpoints_postprocessor(tmp_path: Path):
    module = _load_qpoints_commands_module()

    qpoints_root = tmp_path / "QPoints"
    script_path = qpoints_root / "scripts" / "uarch_restore" / "prepare_gem5_uarch.py"
    script_path.parent.mkdir(parents=True)
    script_path.write_text("#!/usr/bin/env python3\n")

    qflex_ckp_dir = tmp_path / "qflex-ckpts"
    (qflex_ckp_dir / "run" / "snapshot_0.uarch").mkdir(parents=True)
    gem5_ckp_dir = tmp_path / "checkpoints"
    gem5_ckp_dir.mkdir()

    with mock.patch.object(module.shutil, "which", return_value="/usr/bin/zstd"), \
         mock.patch.object(module.subprocess, "run") as run_mock:
        module._prepare_snapshot_gem5_uarch(
            qpoints_root=qpoints_root,
            qflex_ckp_dir=str(qflex_ckp_dir),
            gem5_ckp_dir=str(gem5_ckp_dir),
            snapshot="snapshot_0",
        )

    run_mock.assert_called_once_with(
        [
            "python3",
            str(script_path),
            "--qflex-run-dir",
            str(qflex_ckp_dir / "run"),
            "--gem5-workload-root",
            str(gem5_ckp_dir),
            "--snapshot",
            "snapshot_0",
            "--overwrite",
        ],
        text=True,
        check=True,
    )


def test_prepare_snapshot_gem5_uarch_skips_when_zstd_missing(
    tmp_path: Path, capsys
):
    module = _load_qpoints_commands_module()

    qpoints_root = tmp_path / "QPoints"
    script_path = qpoints_root / "scripts" / "uarch_restore" / "prepare_gem5_uarch.py"
    script_path.parent.mkdir(parents=True)
    script_path.write_text("#!/usr/bin/env python3\n")

    qflex_ckp_dir = tmp_path / "qflex-ckpts"
    (qflex_ckp_dir / "run" / "snapshot_0.uarch").mkdir(parents=True)
    gem5_ckp_dir = tmp_path / "checkpoints"
    gem5_ckp_dir.mkdir()

    with mock.patch.object(module.shutil, "which", return_value=None), \
         mock.patch.object(module.subprocess, "run") as run_mock:
        module._prepare_snapshot_gem5_uarch(
            qpoints_root=qpoints_root,
            qflex_ckp_dir=str(qflex_ckp_dir),
            gem5_ckp_dir=str(gem5_ckp_dir),
            snapshot="snapshot_0",
        )

    run_mock.assert_not_called()
    assert "zstd not found; skipping gem5 uarch preparation" in capsys.readouterr().err


def test_prepare_snapshot_gem5_uarch_continues_when_postprocessor_fails(
    tmp_path: Path, capsys
):
    module = _load_qpoints_commands_module()

    qpoints_root = tmp_path / "QPoints"
    script_path = qpoints_root / "scripts" / "uarch_restore" / "prepare_gem5_uarch.py"
    script_path.parent.mkdir(parents=True)
    script_path.write_text("#!/usr/bin/env python3\n")

    qflex_ckp_dir = tmp_path / "qflex-ckpts"
    (qflex_ckp_dir / "run" / "snapshot_0.uarch").mkdir(parents=True)
    gem5_ckp_dir = tmp_path / "checkpoints"
    gem5_ckp_dir.mkdir()

    with mock.patch.object(module.shutil, "which", return_value="/usr/bin/zstd"), \
         mock.patch.object(
             module.subprocess,
             "run",
             side_effect=subprocess.CalledProcessError(1, ["python3"]),
         ) as run_mock:
        module._prepare_snapshot_gem5_uarch(
            qpoints_root=qpoints_root,
            qflex_ckp_dir=str(qflex_ckp_dir),
            gem5_ckp_dir=str(gem5_ckp_dir),
            snapshot="snapshot_0",
        )

    run_mock.assert_called_once()
    assert "continuing without gem5 uarch artifacts" in capsys.readouterr().err


def test_prepare_snapshot_gem5_uarch_skips_when_postprocessor_script_missing(
    tmp_path: Path, capsys
):
    module = _load_qpoints_commands_module()

    qpoints_root = tmp_path / "QPoints"
    qpoints_root.mkdir()

    qflex_ckp_dir = tmp_path / "qflex-ckpts"
    (qflex_ckp_dir / "run" / "snapshot_0.uarch").mkdir(parents=True)
    gem5_ckp_dir = tmp_path / "checkpoints"
    gem5_ckp_dir.mkdir()

    with mock.patch.object(module.shutil, "which", return_value="/usr/bin/zstd"), \
         mock.patch.object(module.subprocess, "run") as run_mock:
        module._prepare_snapshot_gem5_uarch(
            qpoints_root=qpoints_root,
            qflex_ckp_dir=str(qflex_ckp_dir),
            gem5_ckp_dir=str(gem5_ckp_dir),
            snapshot="snapshot_0",
        )

    run_mock.assert_not_called()
    assert "prepare_gem5_uarch.py not found; skipping gem5 uarch preparation" in (
        capsys.readouterr().err
    )
