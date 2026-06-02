import os
import re
import shutil
import subprocess
import time
from pathlib import Path
import sys
import signal
import threading
import concurrent.futures
from typing import Optional


def _ensure_executable(path: Path) -> None:
    mode = path.stat().st_mode
    path.chmod(mode | 0o111)


def _snapshot_index(snapshot: str) -> int:
    match = re.fullmatch(r"snapshot_(\d+)", snapshot)
    if not match:
        raise RuntimeError(
            f"Snapshot must be in the form snapshot_N, got: {snapshot!r}."
        )
    return int(match.group(1))


def _require_qpoints_file(qpoints_root: Path, relative_path: str) -> Path:
    path = qpoints_root / relative_path
    if not path.is_file():
        raise RuntimeError(
            f"Required QPoints file not found: {path}. "
            "Initialize/update the QPoints submodule or use a qflex image "
            "built with ./dep build-docker --qpoints or --all-ext."
        )
    return path


def _prepare_qpoints_root(qpoints_root: Path, required_paths) -> None:
    if not qpoints_root.is_dir():
        raise RuntimeError(
            f"QPoints directory not found: {qpoints_root}. "
            "Initialize/update the QPoints submodule or use a qflex image "
            "built with ./dep build-docker --qpoints or --all-ext."
        )
    for relative_path in required_paths:
        _require_qpoints_file(qpoints_root, relative_path)


def _refresh_qpoints_helper(
    qpoints_root: Path,
    run_dir: Path,
    relative_path: str,
    dest_name: Optional[str] = None,
) -> Path:
    src = _require_qpoints_file(qpoints_root, relative_path)
    dest = run_dir / (dest_name or src.name)
    shutil.copy2(src, dest)
    _ensure_executable(dest)
    return dest


def _resolve_qemu_efi_fd(run_dir: Path) -> Path:
    for candidate in (
        run_dir / "QEMU_EFI.fd",
        Path("/usr/share/qemu-efi-aarch64/QEMU_EFI.fd"),
        Path("/usr/share/AAVMF/AAVMF_CODE.fd"),
        Path("/usr/share/edk2/aarch64/QEMU_EFI.fd"),
    ):
        if candidate.is_file():
            return candidate
    raise RuntimeError(
        "Could not find UEFI firmware. Set QEMU_EFI_FD or install a valid "
        "QEMU_EFI.fd/AAVMF_CODE.fd."
    )


def _remove_path(path: Path) -> None:
    if not path.exists() and not path.is_symlink():
        return
    if path.is_symlink() or path.is_file():
        path.unlink()
        return
    shutil.rmtree(path)


def _symlink_file(target: Path, source: Path) -> None:
    if not source.is_file():
        raise RuntimeError(f"Required producer artifact not found: {source}")
    if target.exists() or target.is_symlink():
        target.unlink()
    target.symlink_to(source)


def _prepare_snapshot_gem5_uarch(
    qpoints_root: Path,
    qflex_ckp_dir: str,
    gem5_ckp_dir: str,
    snapshot: str,
    ruby_protocol: str = "mesi_two_level",
) -> None:
    qflex_uarch_dir = Path(qflex_ckp_dir) / "run" / f"{snapshot}.uarch"
    if not qflex_uarch_dir.is_dir():
        print(
            f"[{snapshot}] no qflex uarch directory found at {qflex_uarch_dir}; "
            "skipping gem5 uarch preparation"
        )
        return
    if shutil.which("zstd") is None:
        print(
            f"[{snapshot}] zstd not found; skipping gem5 uarch preparation "
            f"for {qflex_uarch_dir}",
            file=sys.stderr,
        )
        return

    try:
        prepare_script = _require_qpoints_file(
            qpoints_root, "scripts/uarch_restore/prepare_gem5_uarch.py"
        )
    except RuntimeError:
        print(
            f"[{snapshot}] prepare_gem5_uarch.py not found; skipping gem5 "
            f"uarch preparation for {qflex_uarch_dir}",
            file=sys.stderr,
        )
        return

    print(f"[{snapshot}] preparing gem5 uarch artifacts")
    try:
        subprocess.run(
            [
                sys.executable,
                str(prepare_script),
                "--qflex-run-dir",
                str(Path(qflex_ckp_dir) / "run"),
                "--gem5-workload-root",
                gem5_ckp_dir,
                "--snapshot",
                snapshot,
                "--ruby-protocol",
                ruby_protocol,
                "--overwrite",
            ],
            text=True,
            check=True,
        )
    except subprocess.CalledProcessError:
        print(
            f"[{snapshot}] gem5 uarch preparation failed for "
            f"{qflex_uarch_dir}; continuing without gem5 uarch artifacts",
            file=sys.stderr,
        )


def _positive_int_env(name: str, default: int) -> int:
    value = os.environ.get(name, str(default))
    if not re.fullmatch(r"[1-9][0-9]*", value):
        raise RuntimeError(
            f"{name} must be a positive integer, got: {value!r}."
        )
    return int(value)


def _tail_file(path: Path, max_lines: int = 20) -> str:
    if not path.is_file():
        return ""
    lines = path.read_text(errors="replace").splitlines()
    tail = "\n".join(lines[-max_lines:])
    if not tail:
        return ""
    return f"\nLast {min(len(lines), max_lines)} lines of {path}:\n{tail}"


def convert_single(
    qflex_ckp_dir: str,
    gem5_ckp_dir: str,
    core_count: int,
    memory_gb: int,
    base: str,
    snapshot: str,
    ssh_host: str = "127.0.0.1",
    ssh_user: str = "qflex",
    monitor_base: int = 45454,
    qmp_base: int = 4444,
    ssh_base: int = 2222,
    overwrite: bool = False,
    cancel_event: Optional[threading.Event] = None,
    ruby_protocol: str = "mesi_two_level",
) -> None:
    start_time = time.time()

    repo_root = Path(__file__).resolve().parents[1]
    qpoints_root = repo_root / "QPoints"
    create_gem5_checkpoint = repo_root / "create_gem5_checkpoint.py"
    if not create_gem5_checkpoint.is_file():
        raise RuntimeError(
            f"Required checkpoint composer not found: {create_gem5_checkpoint}"
        )
    _prepare_qpoints_root(qpoints_root, ("scripts/uarch_restore/prepare_gem5_uarch.py",))

    run_dir = Path(qflex_ckp_dir) / "run"
    if not run_dir.is_dir():
        raise RuntimeError(f"run directory not found: {run_dir}")

    img_dest_dir = Path(gem5_ckp_dir) / snapshot
    qemu_gem5_dump_dir = run_dir / f"{snapshot}.gem"
    register_info = qemu_gem5_dump_dir / "register-info.json"
    dev_info = qemu_gem5_dump_dir / "dev.info"
    physmem = qemu_gem5_dump_dir / "system.physmem.store1.pmem"
    checkpoint_files_exist = (
        register_info.is_file() and dev_info.is_file() and physmem.is_file()
    )

    qemu_bin = run_dir / "qemu-system-aarch64"
    qemu_img = run_dir / "qemu-img"
    if not qemu_bin.is_file():
        raise RuntimeError(f"qemu-system-aarch64 not found: {qemu_bin}")
    if not qemu_img.is_file():
        raise RuntimeError(f"qemu-img not found: {qemu_img}")

    qemu_efi_fd = Path(os.environ.get("QEMU_EFI_FD", "")).expanduser()
    if not qemu_efi_fd.is_file():
        qemu_efi_fd = _resolve_qemu_efi_fd(run_dir)

    qemu_log = run_dir / f"qemu_emu_{snapshot}.log"
    converted_img_tmp = None

    def _check_cancelled() -> None:
        if cancel_event is not None and cancel_event.is_set():
            raise RuntimeError(f"[{snapshot}] conversion cancelled")
    try:
        _check_cancelled()
        if img_dest_dir.exists():
            if overwrite:
                _remove_path(img_dest_dir)
            elif sys.stdin.isatty():
                resp = input(
                    f"Destination directory already exists: {img_dest_dir}\n"
                    "Delete it and proceed? [y/n]: "
                )
                if resp.strip().lower() not in {"y", "yes"}:
                    raise RuntimeError("Aborting.")
                _remove_path(img_dest_dir)
            else:
                raise RuntimeError(
                    f"Destination directory already exists: {img_dest_dir}. "
                    "Re-run with --overwrite to remove it."
                )

        if not checkpoint_files_exist:
            print(f"[{snapshot}] generating .gem checkpoint bundle")
            with qemu_log.open("w") as log_file:
                subprocess.run(
                    [
                        str(qemu_bin),
                        "-M", "virt,gic-version=max,virtualization=off,secure=off",
                        "-smp", str(core_count),
                        "-cpu", "max,pauth=off",
                        "-m", f"{memory_gb}G",
                        "-boot", "order=d,menu=on",
                        "-bios", str(qemu_efi_fd),
                        "-drive", f"if=virtio,file={base},format=qcow2",
                        "-nic", "user,model=virtio-net-pci",
                        "-rtc", "clock=vm",
                        "-nographic",
                        "-no-reboot",
                        "-convert-to-gem5-chkp", snapshot,
                    ],
                    cwd=str(run_dir),
                    stdout=log_file,
                    stderr=subprocess.STDOUT,
                    text=True,
                    check=True,
                )

        if not qemu_gem5_dump_dir.is_dir():
            raise RuntimeError(
                f"[{snapshot}] expected .gem producer bundle not found: "
                f"{qemu_gem5_dump_dir}.{_tail_file(qemu_log)}"
            )

        print(f"[{snapshot}] materializing canonical checkpoint directory")
        img_dest_dir.parent.mkdir(parents=True, exist_ok=True)
        img_dest_dir.mkdir(parents=True, exist_ok=True)
        _symlink_file(img_dest_dir / "register-info.json", register_info)
        _symlink_file(img_dest_dir / "dev.info", dev_info)
        _symlink_file(
            img_dest_dir / "system.physmem.store1.pmem",
            physmem,
        )

        print(f"[{snapshot}] converting disk image")
        _check_cancelled()
        converted_img = img_dest_dir / f"{snapshot}.img"
        converted_img_tmp = (
            img_dest_dir
            / f".{snapshot}.img.tmp.{os.getpid()}.{threading.get_ident()}"
        )
        for path in (converted_img, converted_img_tmp):
            if path.exists():
                path.unlink()

        subprocess.run(
            [
                str(qemu_img),
                "convert",
                "-f",
                "qcow2",
                "-O",
                "raw",
                "-l",
                snapshot,
                base,
                str(converted_img_tmp),
            ],
            cwd=str(run_dir),
            text=True,
            check=True,
        )

        if converted_img_tmp.is_file():
            converted_img_tmp.replace(converted_img)
        else:
            raise RuntimeError(
                f"[{snapshot}] Converted image not found: {converted_img_tmp}"
            )

        store0 = run_dir / "system.physmem.store0.pmem"
        if store0.is_file():
            shutil.copy2(store0, img_dest_dir / store0.name)

        print(f"[{snapshot}] composing base m5.cpt")
        subprocess.run(
            [
                sys.executable,
                str(create_gem5_checkpoint),
                str(img_dest_dir),
                "--num-cores",
                str(core_count),
            ],
            cwd=str(repo_root),
            text=True,
            check=True,
        )

        _check_cancelled()
        _prepare_snapshot_gem5_uarch(
            qpoints_root, qflex_ckp_dir, gem5_ckp_dir, snapshot, ruby_protocol
        )
    except KeyboardInterrupt:
        raise
    finally:
        if converted_img_tmp is not None and converted_img_tmp.exists():
            converted_img_tmp.unlink()
        elapsed = int(time.time() - start_time)
        print(f"[{snapshot}] convert-single completed in {elapsed}s")


def convert_multi(
    first: str,
    last: str,
    parallel: int,
    qflex_ckp_dir: str,
    gem5_ckp_dir: str,
    core_count: int,
    memory_gb: int,
    base: str,
    ssh_host: str = "127.0.0.1",
    ssh_user: str = "qflex",
    monitor_base: int = 45454,
    qmp_base: int = 4444,
    ssh_base: int = 2222,
    overwrite: bool = False,
    ruby_protocol: str = "mesi_two_level",
) -> None:
    first_match = re.fullmatch(r"snapshot_(\d+)", first)
    last_match = re.fullmatch(r"snapshot_(\d+)", last)
    if not first_match or not last_match:
        raise RuntimeError("First/last must be in the form snapshot_N.")
    first_idx = int(first_match.group(1))
    last_idx = int(last_match.group(1))
    if first_idx > last_idx:
        raise RuntimeError("First snapshot index must be <= last snapshot index.")
    if parallel < 1:
        raise RuntimeError("Parallel must be >= 1.")

    snapshots = [f"snapshot_{i}" for i in range(first_idx, last_idx + 1)]
    start_time = time.time()
    errors = []
    cancel_event = threading.Event()

    def _run(snapshot: str) -> None:
        convert_single(
            qflex_ckp_dir=qflex_ckp_dir,
            gem5_ckp_dir=gem5_ckp_dir,
            core_count=core_count,
            memory_gb=memory_gb,
            base=base,
            snapshot=snapshot,
            ssh_host=ssh_host,
            ssh_user=ssh_user,
            monitor_base=monitor_base,
            qmp_base=qmp_base,
            ssh_base=ssh_base,
            overwrite=overwrite,
            cancel_event=cancel_event,
            ruby_protocol=ruby_protocol,
        )

    with concurrent.futures.ThreadPoolExecutor(max_workers=parallel) as executor:
        future_map = {executor.submit(_run, snap): snap for snap in snapshots}
        for future in concurrent.futures.as_completed(future_map):
            snap = future_map[future]
            try:
                future.result()
            except Exception as exc:
                errors.append((snap, exc))
                cancel_event.set()
                for pending in future_map:
                    pending.cancel()
                break

    if errors:
        snap, exc = errors[0]
        raise RuntimeError(f"convert-multi failed at {snap}: {exc}") from exc

    elapsed = int(time.time() - start_time)
    print(f"convert-multi completed in {elapsed}s")


def run_gem5(
    gem5_ckp_dir: str,
    experiment: str,
    snapshot: str,
    inst: int,
    core_count: int,
    branch_trace: bool = False,
    tage_decision_trace: bool = False,
    data_trace: bool = False,
    dump_cache_state: bool = False,
    timing_ruby: bool = False,
    timing_ruby_moesi: bool = False,
    sim_config: Optional[str] = None,
) -> None:
    if timing_ruby and timing_ruby_moesi:
        raise RuntimeError(
            "Choose only one timing Ruby protocol flag: --timing-ruby or --timing-ruby-moesi."
        )
    if dump_cache_state and not timing_ruby:
        raise RuntimeError("--dump-cache-state requires --timing-ruby.")

    repo_root = Path(__file__).resolve().parents[1]
    qpoints_root = repo_root / "QPoints"
    _prepare_qpoints_root(qpoints_root, ("run_gem5.sh",))
    run_gem5_sh = qpoints_root / "run_gem5.sh"
    args = [
        "bash",
        str(run_gem5_sh),
        "--gem5-ckp-dir",
        gem5_ckp_dir,
        "--experiment",
        experiment,
        "--snapshot",
        snapshot,
        "--inst",
        str(inst),
        "--cores",
        str(core_count),
    ]
    if branch_trace:
        args.append("--branch-trace")
    if tage_decision_trace:
        args.append("--tage-decision-trace")
    if data_trace:
        args.append("--data-trace")
    if dump_cache_state:
        args.append("--dump-cache-state")
    if timing_ruby:
        args.append("--timing-ruby")
    if timing_ruby_moesi:
        args.append("--timing-ruby-moesi")
    if sim_config:
        args.extend(["--sim-config", sim_config])
    subprocess.run(
        args,
        cwd=str(qpoints_root),
        text=True,
        check=True,
    )
