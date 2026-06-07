import gzip
import importlib.machinery
import importlib.util
import json
import sys
from pathlib import Path


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def _load_qflex_module():
    repo_root = _repo_root()
    script = repo_root / "qflex"
    repo_root_str = str(repo_root)
    inserted_repo_root = False
    if repo_root_str not in sys.path:
        sys.path.insert(0, repo_root_str)
        inserted_repo_root = True
    try:
        loader = importlib.machinery.SourceFileLoader("qflex_cli_manifest_tests", str(script))
        spec = importlib.util.spec_from_loader(loader.name, loader)
        module = importlib.util.module_from_spec(spec)
        loader.exec_module(module)
        return module
    finally:
        if inserted_repo_root and sys.path and sys.path[0] == repo_root_str:
            sys.path.pop(0)


def _load_checkpoint_module():
    repo_root = _repo_root()
    module_path = repo_root / "create_gem5_checkpoint.py"
    loader = importlib.machinery.SourceFileLoader("create_gem5_checkpoint_tests", str(module_path))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


def _load_qpoints_commands_module():
    repo_root = _repo_root()
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


def test_qflex_args_file_expands_in_command_position(tmp_path: Path):
    module = _load_qflex_module()
    args_file = tmp_path / "ws.args"
    args_file.write_text(
        "--core-count 8\n"
        "--memory-gb 32\n"
        "# comment\n"
        "--root-device /dev/vda\n",
        encoding="utf-8",
    )

    expanded = module._expand_args_files(
        [
            "qpoints",
            "run-gem5",
            "--args-file",
            str(args_file),
            "--snapshot",
            "snapshot_0",
        ]
    )

    assert expanded == [
        "qpoints",
        "run-gem5",
        "--core-count",
        "8",
        "--memory-gb",
        "32",
        "--root-device",
        "/dev/vda",
        "--snapshot",
        "snapshot_0",
    ]


def test_write_checkpoint_machine_config_records_extended_machine_fields(tmp_path: Path):
    module = _load_qpoints_commands_module()
    root_dir = tmp_path / "checkpoints"
    snapshot_dir = root_dir / "snapshot_0"
    snapshot_dir.mkdir(parents=True)
    disk_image = snapshot_dir / "snapshot_0.img"
    disk_image.write_bytes(b"")

    module._write_checkpoint_machine_config(
        gem5_ckp_dir=root_dir,
        checkpoint_dir=snapshot_dir,
        snapshot="snapshot_0",
        core_count=8,
        memory_gb=32,
        kernel="/tmp/kernel.elf",
        bootloader="/tmp/bootloader.arm64",
        root_device="/dev/vda",
        itb_size=96,
        dtb_size=128,
        have_large_asid_64=True,
        disk_image=disk_image,
    )

    root_manifest = json.loads((root_dir / "machine_config.json").read_text(encoding="utf-8"))
    snapshot_manifest = json.loads((snapshot_dir / "machine_config.json").read_text(encoding="utf-8"))

    assert root_manifest["core_count"] == 8
    assert root_manifest["memory_gb"] == 32
    assert root_manifest["memory_bytes"] == 32 * 1024**3
    assert root_manifest["bootmem_size_bytes"] == 64 * 1024**2
    assert root_manifest["itb_size"] == 96
    assert root_manifest["dtb_size"] == 128
    assert root_manifest["have_large_asid_64"] is True
    assert root_manifest["root_device"] == "/dev/vda"
    assert snapshot_manifest["snapshot"] == "snapshot_0"
    assert snapshot_manifest["disk_image"] == str(disk_image.resolve())


def test_validate_memory_artifacts_rejects_gzip_store0(tmp_path: Path):
    module = _load_checkpoint_module()
    gzip_store0 = tmp_path / "system.physmem.store0.pmem"
    with gzip.open(gzip_store0, "wb") as fh:
        fh.write(b"\x00" * 16)
    store1 = tmp_path / "system.physmem.store1.pmem"
    store1.write_bytes(b"\x00" * 64)

    try:
        module.validate_memory_artifacts(
            tmp_path,
            bootmem_size_bytes=16,
            physmem_store1_range_size=64,
        )
    except ValueError as exc:
        assert "gzip-compressed" in str(exc)
    else:
        raise AssertionError("Expected validate_memory_artifacts to reject gzip-compressed store0")


def test_validate_memory_artifacts_accepts_raw_ranges(tmp_path: Path):
    module = _load_checkpoint_module()
    (tmp_path / "system.physmem.store0.pmem").write_bytes(b"\x00" * 16)
    (tmp_path / "system.physmem.store1.pmem").write_bytes(b"\x00" * 64)

    module.validate_memory_artifacts(
        tmp_path,
        bootmem_size_bytes=16,
        physmem_store1_range_size=64,
    )
