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
    assert "tage-decision" in result.stdout
    assert "--data-trace" in result.stdout
    assert "--dump-cache-state" in result.stdout
    assert "--timing-ruby" in result.stdout
    assert "MOESI_CMP_directory" in result.stdout
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
            tage_decision_trace=True,
            data_trace=True,
            dump_cache_state=True,
            timing_ruby=True,
            timing_ruby_moesi=False,
            sim_config="/tmp/override.args",
        )

    forwarded.assert_called_once_with(
        gem5_ckp_dir="/tmp/gem5_ckp",
        experiment="exp",
        snapshot="snapshot_0",
        inst=1000,
        core_count=1,
        branch_trace=True,
        tage_decision_trace=True,
        data_trace=True,
        dump_cache_state=True,
        timing_ruby=True,
        timing_ruby_moesi=False,
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
            timing_ruby_moesi=False,
        )


def test_qpoints_convert_single_rejects_invalid_ruby_protocol():
    module = _load_qflex_module()
    with pytest.raises(module.typer.BadParameter, match="Unsupported --ruby-protocol value"):
        module.qpoints_convert_single_cmd(
            qflex_ckp_dir="/tmp/qflex_ckp",
            gem5_ckp_dir="/tmp/gem5_ckp",
            core_count=8,
            memory_gb=32,
            base="/tmp/base.qcow2",
            snapshot="snapshot_0",
            ruby_protocol="bad_protocol",
        )


def test_qpoints_convert_multi_rejects_invalid_ruby_protocol():
    module = _load_qflex_module()
    with pytest.raises(module.typer.BadParameter, match="Unsupported --ruby-protocol value"):
        module.qpoints_convert_multi_cmd(
            first="snapshot_0",
            last="snapshot_1",
            parallel=2,
            qflex_ckp_dir="/tmp/qflex_ckp",
            gem5_ckp_dir="/tmp/gem5_ckp",
            core_count=8,
            memory_gb=32,
            base="/tmp/base.qcow2",
            ruby_protocol="bad_protocol",
        )


def test_qpoints_convert_multi_forwards_ruby_protocol():
    module = _load_qflex_module()
    with mock.patch.object(module, "qpoints_convert_multi") as forwarded:
        module.qpoints_convert_multi_cmd(
            first="snapshot_0",
            last="snapshot_1",
            parallel=2,
            qflex_ckp_dir="/tmp/qflex_ckp",
            gem5_ckp_dir="/tmp/gem5_ckp",
            core_count=8,
            memory_gb=32,
            base="/tmp/base.qcow2",
            ruby_protocol="moesi_cmp_directory",
        )

    forwarded.assert_called_once_with(
        first="snapshot_0",
        last="snapshot_1",
        parallel=2,
        qflex_ckp_dir="/tmp/qflex_ckp",
        gem5_ckp_dir="/tmp/gem5_ckp",
        core_count=8,
        memory_gb=32,
        base="/tmp/base.qcow2",
        ssh_host="127.0.0.1",
        ssh_user="qflex",
        monitor_base=45454,
        qmp_base=4444,
        ssh_base=2222,
        overwrite=False,
        ruby_protocol="moesi_cmp_directory",
    )


def test_qpoints_run_gem5_forwards_moesi_timing_mode():
    module = _load_qflex_module()
    with mock.patch.object(module, "qpoints_run_gem5") as forwarded:
        module.qpoints_run_gem5_cmd(
            gem5_ckp_dir="/tmp/gem5_ckp",
            experiment="exp",
            snapshot="snapshot_0",
            inst=1000,
            core_count=1,
            timing_ruby=False,
            timing_ruby_moesi=True,
        )

    forwarded.assert_called_once_with(
        gem5_ckp_dir="/tmp/gem5_ckp",
        experiment="exp",
        snapshot="snapshot_0",
        inst=1000,
        core_count=1,
        branch_trace=False,
        tage_decision_trace=False,
        data_trace=False,
        dump_cache_state=False,
        timing_ruby=False,
        timing_ruby_moesi=True,
        sim_config=None,
    )


def test_qpoints_run_gem5_rejects_multiple_timing_protocol_flags():
    module = _load_qflex_module()
    with pytest.raises(module.typer.BadParameter, match="Choose only one timing Ruby protocol flag"):
        module.qpoints_run_gem5_cmd(
            gem5_ckp_dir="/tmp/gem5_ckp",
            experiment="exp",
            snapshot="snapshot_0",
            inst=1000,
            core_count=1,
            timing_ruby=True,
            timing_ruby_moesi=True,
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


def test_is_gem_bundle_ready_requires_expected_files(tmp_path: Path):
    module = _load_qpoints_commands_module()

    run_dir = tmp_path / "run"
    gem_dir = run_dir / "snapshot_0.gem"
    gem_dir.mkdir(parents=True)

    assert module._is_gem_bundle_ready(run_dir, "snapshot_0") is False

    (gem_dir / "register-info.json").write_text("{}\n", encoding="utf-8")
    (gem_dir / "dev.info").write_text("dev\n", encoding="utf-8")
    (gem_dir / "system.physmem.store1.pmem").write_bytes(b"x")

    assert module._is_gem_bundle_ready(run_dir, "snapshot_0") is True


def test_is_checkpoint_ready_requires_protocol_manifest(tmp_path: Path):
    module = _load_qpoints_commands_module()

    checkpoint_dir = tmp_path / "snapshot_0"
    (checkpoint_dir / "gem5_uarch" / "moesi_cmp_directory").mkdir(parents=True)
    for relative in (
        "machine_config.json",
        "m5.cpt",
        "snapshot_0.img",
        "system.physmem.store0.pmem",
        "system.physmem.store1.pmem",
        "register-info.json",
        "dev.info",
        "gem5_uarch/moesi_cmp_directory/manifest.json",
    ):
        path = checkpoint_dir / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("x\n", encoding="utf-8")

    assert (
        module._is_checkpoint_ready(
            str(tmp_path),
            "snapshot_0",
            "moesi_cmp_directory",
        )
        is True
    )
    assert (
        module._is_checkpoint_ready(
            str(tmp_path),
            "snapshot_0",
            "mesi_two_level",
        )
        is False
    )


def test_cleanup_snapshot_gem5_artifacts_removes_only_derived_outputs(tmp_path: Path):
    module = _load_qpoints_commands_module()

    qflex_root = tmp_path / "experiment"
    gem5_root = tmp_path / "checkpoints"
    gem_dir = qflex_root / "run" / "snapshot_0.gem"
    checkpoint_dir = gem5_root / "snapshot_0"
    source_uarch = qflex_root / "run" / "snapshot_0.uarch"
    source_loc = qflex_root / "run" / "snapshot_0.loc"
    source_state = qflex_root / "run" / "snapshot_0.state.zstd"

    gem_dir.mkdir(parents=True)
    checkpoint_dir.mkdir(parents=True)
    source_uarch.mkdir(parents=True)
    source_loc.write_text("loc\n", encoding="utf-8")
    source_state.write_text("state\n", encoding="utf-8")

    module._cleanup_snapshot_gem5_artifacts(
        qflex_ckp_dir=str(qflex_root),
        gem5_ckp_dir=str(gem5_root),
        snapshot="snapshot_0",
    )

    assert not gem_dir.exists()
    assert not checkpoint_dir.exists()
    assert source_uarch.exists()
    assert source_loc.exists()
    assert source_state.exists()


def test_is_checkpoint_ready_for_request_rejects_machine_contract_mismatch(
    tmp_path: Path,
):
    module = _load_qpoints_commands_module()

    checkpoint_dir = tmp_path / "snapshot_0"
    (checkpoint_dir / "gem5_uarch" / "moesi_cmp_directory").mkdir(parents=True)
    machine_config = {
        "core_count": 4,
        "memory_gb": 32,
        "kernel": "/tmp/kernel",
        "bootloader": "/tmp/boot.bin",
        "root_device": "/dev/vda",
        "itb_size": 64,
        "dtb_size": 64,
        "have_large_asid_64": True,
    }
    (checkpoint_dir / "machine_config.json").write_text(
        __import__("json").dumps(machine_config) + "\n",
        encoding="utf-8",
    )
    for relative in (
        "m5.cpt",
        "snapshot_0.img",
        "system.physmem.store0.pmem",
        "system.physmem.store1.pmem",
        "register-info.json",
        "dev.info",
        "gem5_uarch/moesi_cmp_directory/manifest.json",
    ):
        path = checkpoint_dir / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("{}\n", encoding="utf-8")

    assert (
        module._is_checkpoint_ready_for_request(
            str(tmp_path),
            "snapshot_0",
            "moesi_cmp_directory",
            runtime_llc_slice_count=8,
            core_count=8,
            memory_gb=32,
            kernel="/tmp/kernel",
            bootloader="/tmp/boot.bin",
            root_device="/dev/vda",
            itb_size=64,
            dtb_size=64,
            have_large_asid_64=True,
        )
        is False
    )


def test_is_checkpoint_ready_for_request_requires_mmu_sidecars_when_tlb_present(
    tmp_path: Path,
):
    module = _load_qpoints_commands_module()

    checkpoint_dir = tmp_path / "snapshot_0"
    protocol_dir = checkpoint_dir / "gem5_uarch" / "moesi_cmp_directory"
    protocol_dir.mkdir(parents=True)
    machine_config = {
        "core_count": 8,
        "memory_gb": 32,
        "kernel": "/tmp/kernel",
        "bootloader": "/tmp/boot.bin",
        "root_device": "/dev/vda",
        "itb_size": 64,
        "dtb_size": 64,
        "have_large_asid_64": True,
    }
    (checkpoint_dir / "machine_config.json").write_text(
        __import__("json").dumps(machine_config) + "\n",
        encoding="utf-8",
    )
    for relative in (
        "m5.cpt",
        "snapshot_0.img",
        "system.physmem.store0.pmem",
        "system.physmem.store1.pmem",
        "register-info.json",
        "dev.info",
    ):
        path = checkpoint_dir / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("{}\n", encoding="utf-8")
    (protocol_dir / "manifest.json").write_text(
        __import__("json").dumps(
            {
                "llc_slice_count": 8,
                "components": {"tlb": {"source_files": {"0": "cpu0.json"}}},
            }
        )
        + "\n",
        encoding="utf-8",
    )

    assert (
        module._is_checkpoint_ready_for_request(
            str(tmp_path),
            "snapshot_0",
            "moesi_cmp_directory",
            runtime_llc_slice_count=8,
            core_count=8,
            memory_gb=32,
            kernel="/tmp/kernel",
            bootloader="/tmp/boot.bin",
            root_device="/dev/vda",
            itb_size=64,
            dtb_size=64,
            have_large_asid_64=True,
        )
        is False
    )

    (checkpoint_dir / "gem5_uarch" / "mmu-cpu0.cpt").write_text(
        "mmu\n",
        encoding="utf-8",
    )

    assert (
        module._is_checkpoint_ready_for_request(
            str(tmp_path),
            "snapshot_0",
            "moesi_cmp_directory",
            runtime_llc_slice_count=8,
            core_count=8,
            memory_gb=32,
            kernel="/tmp/kernel",
            bootloader="/tmp/boot.bin",
            root_device="/dev/vda",
            itb_size=64,
            dtb_size=64,
            have_large_asid_64=True,
        )
        is True
    )


def test_is_checkpoint_ready_for_request_requires_matching_llc_slice_count(
    tmp_path: Path,
):
    module = _load_qpoints_commands_module()

    checkpoint_dir = tmp_path / "snapshot_0"
    protocol_dir = checkpoint_dir / "gem5_uarch" / "moesi_cmp_directory"
    protocol_dir.mkdir(parents=True)
    machine_config = {
        "core_count": 8,
        "memory_gb": 32,
        "kernel": "/tmp/kernel",
        "bootloader": "/tmp/boot.bin",
        "root_device": "/dev/vda",
        "itb_size": 64,
        "dtb_size": 64,
        "have_large_asid_64": True,
    }
    (checkpoint_dir / "machine_config.json").write_text(
        __import__("json").dumps(machine_config) + "\n",
        encoding="utf-8",
    )
    for relative in (
        "m5.cpt",
        "snapshot_0.img",
        "system.physmem.store0.pmem",
        "system.physmem.store1.pmem",
        "register-info.json",
        "dev.info",
    ):
        path = checkpoint_dir / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("{}\n", encoding="utf-8")
    (protocol_dir / "manifest.json").write_text(
        __import__("json").dumps({"llc_slice_count": 1}) + "\n",
        encoding="utf-8",
    )

    assert (
        module._is_checkpoint_ready_for_request(
            str(tmp_path),
            "snapshot_0",
            "moesi_cmp_directory",
            runtime_llc_slice_count=8,
            core_count=8,
            memory_gb=32,
            kernel="/tmp/kernel",
            bootloader="/tmp/boot.bin",
            root_device="/dev/vda",
            itb_size=64,
            dtb_size=64,
            have_large_asid_64=True,
        )
        is False
    )


def test_is_checkpoint_ready_for_request_treats_corrupt_metadata_as_not_ready(
    tmp_path: Path,
):
    module = _load_qpoints_commands_module()

    checkpoint_dir = tmp_path / "snapshot_0"
    protocol_dir = checkpoint_dir / "gem5_uarch" / "moesi_cmp_directory"
    protocol_dir.mkdir(parents=True)
    (checkpoint_dir / "machine_config.json").write_text(
        "{not-json\n",
        encoding="utf-8",
    )
    for relative in (
        "m5.cpt",
        "snapshot_0.img",
        "system.physmem.store0.pmem",
        "system.physmem.store1.pmem",
        "register-info.json",
        "dev.info",
        "gem5_uarch/moesi_cmp_directory/manifest.json",
    ):
        path = checkpoint_dir / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.name == "manifest.json":
            path.write_text("{also-not-json\n", encoding="utf-8")
        else:
            path.write_text("{}\n", encoding="utf-8")

    assert (
        module._is_checkpoint_ready_for_request(
            str(tmp_path),
            "snapshot_0",
            "moesi_cmp_directory",
            runtime_llc_slice_count=8,
            core_count=8,
            memory_gb=32,
            kernel="/tmp/kernel",
            bootloader="/tmp/boot.bin",
            root_device="/dev/vda",
            itb_size=64,
            dtb_size=64,
            have_large_asid_64=True,
        )
        is False
    )


def test_is_checkpoint_ready_for_request_treats_non_object_machine_config_as_not_ready(
    tmp_path: Path,
):
    module = _load_qpoints_commands_module()

    checkpoint_dir = tmp_path / "snapshot_0"
    protocol_dir = checkpoint_dir / "gem5_uarch" / "moesi_cmp_directory"
    protocol_dir.mkdir(parents=True)
    (checkpoint_dir / "machine_config.json").write_text(
        "[]\n",
        encoding="utf-8",
    )
    for relative in (
        "m5.cpt",
        "snapshot_0.img",
        "system.physmem.store0.pmem",
        "system.physmem.store1.pmem",
        "register-info.json",
        "dev.info",
    ):
        path = checkpoint_dir / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("{}\n", encoding="utf-8")
    (protocol_dir / "manifest.json").write_text(
        __import__("json").dumps({"llc_slice_count": 8}) + "\n",
        encoding="utf-8",
    )

    assert (
        module._is_checkpoint_ready_for_request(
            str(tmp_path),
            "snapshot_0",
            "moesi_cmp_directory",
            runtime_llc_slice_count=8,
            core_count=8,
            memory_gb=32,
            kernel="/tmp/kernel",
            bootloader="/tmp/boot.bin",
            root_device="/dev/vda",
            itb_size=64,
            dtb_size=64,
            have_large_asid_64=True,
        )
        is False
    )


def test_run_sample_converts_each_snapshot_before_running_gem5():
    module = _load_qpoints_commands_module()

    with mock.patch.object(
        module, "convert_single"
    ) as convert_mock, mock.patch.object(
        module, "run_gem5"
    ) as run_gem5_mock, mock.patch.object(
        module, "_cleanup_snapshot_gem5_artifacts"
    ) as cleanup_mock, mock.patch.object(
        module, "_load_uipc_summary",
        side_effect=[
            {"engine": "gem5", "cores": [{"core": 0, "ipc": 1.0, "uipc": 1.0}], "aggregate": {"ipc": 1.0, "uipc": 1.0}},
            {"engine": "gem5", "cores": [{"core": 0, "ipc": 2.0, "uipc": 2.0}], "aggregate": {"ipc": 2.0, "uipc": 2.0}},
        ],
    ), mock.patch.object(
        module, "_write_uipc_report", return_value=Path("/tmp/uipc_report.json")
    ) as write_report_mock:
        report = module.run_sample(
            qflex_ckp_dir="/tmp/experiment",
            gem5_ckp_dir="/tmp/checkpoints/exp",
            experiment="exp",
            base="/tmp/experiment/root.qcow2",
            first="snapshot_0",
            last="snapshot_1",
            core_count=8,
            memory_gb=32,
            bootloader="/tmp/boot.bin",
            root_device="/dev/vda",
            warmup_cycles=200000,
            measurement_cycles=1000000,
            timing_ruby=False,
            timing_ruby_moesi=True,
            cache_hierarchy_restore=True,
            sim_config="/tmp/moesi.args",
            cleanup_conversion_artifacts=True,
        )

    assert report == Path("/tmp/uipc_report.json")
    assert [call.kwargs["snapshot"] for call in convert_mock.call_args_list] == [
        "snapshot_0",
        "snapshot_1",
    ]
    assert [call.kwargs["snapshot"] for call in run_gem5_mock.call_args_list] == [
        "snapshot_0",
        "snapshot_1",
    ]
    assert [call.kwargs["snapshot"] for call in cleanup_mock.call_args_list] == [
        "snapshot_0",
        "snapshot_1",
    ]
    for call in convert_mock.call_args_list:
        assert call.kwargs["qflex_ckp_dir"] == "/tmp/experiment"
        assert call.kwargs["base"] == "/tmp/experiment/root.qcow2"
        assert call.kwargs["ruby_protocol"] == "moesi_cmp_directory"
        assert call.kwargs["overwrite"] is False
    for call in cleanup_mock.call_args_list:
        assert call.kwargs["qflex_ckp_dir"] == "/tmp/experiment"
        assert call.kwargs["gem5_ckp_dir"] == "/tmp/checkpoints/exp"
    write_report_mock.assert_called_once()


def test_run_sample_can_keep_conversion_artifacts():
    module = _load_qpoints_commands_module()

    with mock.patch.object(
        module, "convert_single"
    ) as convert_mock, mock.patch.object(
        module, "run_gem5"
    ) as run_gem5_mock, mock.patch.object(
        module, "_cleanup_snapshot_gem5_artifacts"
    ) as cleanup_mock, mock.patch.object(
        module, "_load_uipc_summary",
        return_value={
            "engine": "gem5",
            "cores": [{"core": 0, "ipc": 1.0, "uipc": 1.0}],
            "aggregate": {"ipc": 1.0, "uipc": 1.0},
        },
    ), mock.patch.object(
        module, "_write_uipc_report", return_value=Path("/tmp/uipc_report.json")
    ):
        module.run_sample(
            qflex_ckp_dir="/tmp/experiment",
            gem5_ckp_dir="/tmp/checkpoints/exp",
            experiment="exp",
            base="/tmp/experiment/root.qcow2",
            first="snapshot_0",
            last="snapshot_0",
            core_count=8,
            memory_gb=32,
            bootloader="/tmp/boot.bin",
            root_device="/dev/vda",
            warmup_cycles=200000,
            measurement_cycles=1000000,
            timing_ruby=False,
            timing_ruby_moesi=True,
            cache_hierarchy_restore=True,
            sim_config="/tmp/moesi.args",
            cleanup_conversion_artifacts=False,
        )

    convert_mock.assert_called_once()
    run_gem5_mock.assert_called_once()
    cleanup_mock.assert_not_called()


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

    (qpoints_root / "configs").mkdir(parents=True)
    (qpoints_root / "configs" / "timing_ruby_gem5.args").write_text(
        "--num-l2caches=1\n",
        encoding="utf-8",
    )
    (qpoints_root / "configs" / "timing_ruby_moesi_gem5.args").write_text(
        "--num-l2caches=8\n",
        encoding="utf-8",
    )

    with mock.patch.object(module.shutil, "which", return_value="/usr/bin/zstd"), \
         mock.patch.object(module.subprocess, "run") as run_mock:
        module._prepare_snapshot_gem5_uarch(
            qpoints_root=qpoints_root,
            qflex_ckp_dir=str(qflex_ckp_dir),
            gem5_ckp_dir=str(gem5_ckp_dir),
            snapshot="snapshot_0",
            ruby_protocol="moesi_cmp_directory",
        )

    assert run_mock.call_count == 2
    assert run_mock.call_args_list[0] == mock.call(
        [
            module.sys.executable,
            str(script_path),
            "--qflex-run-dir",
            str(qflex_ckp_dir / "run"),
            "--gem5-workload-root",
            str(gem5_ckp_dir),
            "--snapshot",
            "snapshot_0",
            "--ruby-protocol",
            "moesi_cmp_directory",
            "--llc-slice-count",
            "8",
            "--overwrite",
        ],
        text=True,
        check=True,
    )
    assert run_mock.call_args_list[1] == mock.call(
        [
            module.sys.executable,
            str(script_path),
            "--qflex-run-dir",
            str(qflex_ckp_dir / "run"),
            "--gem5-workload-root",
            str(gem5_ckp_dir),
            "--snapshot",
            "snapshot_0",
            "--ruby-protocol",
            "mesi_two_level",
            "--llc-slice-count",
            "1",
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
    (qpoints_root / "configs").mkdir(parents=True)
    (qpoints_root / "configs" / "timing_ruby_gem5.args").write_text(
        "--num-l2caches=1\n",
        encoding="utf-8",
    )
    (qpoints_root / "configs" / "timing_ruby_moesi_gem5.args").write_text(
        "--num-l2caches=1\n",
        encoding="utf-8",
    )

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


def test_prepare_snapshot_gem5_uarch_keeps_target_manifest_when_auxiliary_fails(
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
    (gem5_ckp_dir / "snapshot_0" / "gem5_uarch" / "moesi_cmp_directory").mkdir(
        parents=True
    )
    (qpoints_root / "configs").mkdir(parents=True)
    (qpoints_root / "configs" / "timing_ruby_gem5.args").write_text(
        "--num-l2caches=1\n",
        encoding="utf-8",
    )
    (qpoints_root / "configs" / "timing_ruby_moesi_gem5.args").write_text(
        "--num-l2caches=8\n",
        encoding="utf-8",
    )
    manifest_path = (
        gem5_ckp_dir
        / "snapshot_0"
        / "gem5_uarch"
        / "moesi_cmp_directory"
        / "manifest.json"
    )
    manifest_path.write_text('{"components":{"tlb":{"source_files":{"0":"cpu0.json"}}}}')

    side_effects = [
        None,
        subprocess.CalledProcessError(1, ["python3"]),
    ]

    with mock.patch.object(module.shutil, "which", return_value="/usr/bin/zstd"), \
         mock.patch.object(module.subprocess, "run", side_effect=side_effects):
        manifest = module._prepare_snapshot_gem5_uarch(
            qpoints_root=qpoints_root,
            qflex_ckp_dir=str(qflex_ckp_dir),
            gem5_ckp_dir=str(gem5_ckp_dir),
            snapshot="snapshot_0",
            ruby_protocol="moesi_cmp_directory",
        )

    assert manifest == {"components": {"tlb": {"source_files": {"0": "cpu0.json"}}}}
    assert "auxiliary gem5 uarch preparation failed for protocol mesi_two_level" in (
        capsys.readouterr().err
    )


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


def test_test_worm_rejects_trace_limit_without_tracing():
    module = _load_qflex_module()
    with pytest.raises(module.typer.BadParameter, match="requires --tage-decision-trace"):
        module.test_worm.__wrapped__(
            experiment_context=mock.sentinel.ctx,
            tage_decision_trace=False,
            tage_decision_trace_limit=10,
        )


def test_test_worm_rejects_negative_trace_limit():
    module = _load_qflex_module()
    with pytest.raises(module.typer.BadParameter, match="must be non-negative"):
        module.test_worm.__wrapped__(
            experiment_context=mock.sentinel.ctx,
            tage_decision_trace=True,
            tage_decision_trace_limit=-1,
        )
