import os
import re
import shutil
import subprocess
import time
import json
from pathlib import Path
import sys
import threading
import concurrent.futures
from typing import Optional
from commands.qemu_cpu import resolve_qemu_cpu

DEFAULT_ITB_SIZE = 64
DEFAULT_DTB_SIZE = 64


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
) -> Optional[dict]:
    qflex_uarch_dir = Path(qflex_ckp_dir) / "run" / f"{snapshot}.uarch"
    if not qflex_uarch_dir.is_dir():
        print(
            f"[{snapshot}] no qflex uarch directory found at {qflex_uarch_dir}; "
            "skipping gem5 uarch preparation"
        )
        return None
    if shutil.which("zstd") is None:
        print(
            f"[{snapshot}] zstd not found; skipping gem5 uarch preparation "
            f"for {qflex_uarch_dir}",
            file=sys.stderr,
        )
        return None

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
        return None

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
        manifest_file = Path(gem5_ckp_dir) / snapshot / "gem5_uarch" / "manifest.json"
        if manifest_file.is_file():
            return json.loads(manifest_file.read_text(encoding="utf-8"))
    except subprocess.CalledProcessError:
        print(
            f"[{snapshot}] gem5 uarch preparation failed for "
            f"{qflex_uarch_dir}; continuing without gem5 uarch artifacts",
            file=sys.stderr,
        )
    return None


def _apply_snapshot_gem5_uarch(
    qpoints_root: Path,
    checkpoint_dir: Path,
    snapshot: str,
    core_count: int,
    uarch_manifest: Optional[dict],
) -> None:
    if not uarch_manifest:
        return

    tlb_manifest = uarch_manifest.get("components", {}).get("tlb", {})
    tlb_source_files = tlb_manifest.get("source_files", {})
    if not tlb_source_files:
        return

    gem5_uarch_dir = checkpoint_dir / "gem5_uarch"
    gem5_uarch_dir.mkdir(parents=True, exist_ok=True)

    gem5_bin = qpoints_root / "gem5" / "build" / "ARM" / "gem5.opt"
    gem5_cfg = qpoints_root / "gem5" / "configs" / "example" / "arm" / "starter_fs.py"
    kernel = qpoints_root / "bin" / "m5" / "binaries" / "vmlinux.arm64"
    bootloader = qpoints_root / "bin" / "m5" / "binaries" / "boot_v2_qemu_virt.arm64"
    disk_image = checkpoint_dir / f"{snapshot}.img"
    outdir = checkpoint_dir / ".tlb_apply_out"
    if outdir.exists():
        shutil.rmtree(outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    try:
        for cpu in range(core_count):
            tlb_source = tlb_source_files.get(str(cpu))
            if not tlb_source:
                continue
            mmu_cpt = gem5_uarch_dir / f"mmu-cpu{cpu}.cpt"
            if mmu_cpt.exists():
                mmu_cpt.unlink()
            subprocess.run(
                [
                    str(gem5_bin),
                    f"--outdir={outdir}",
                    str(gem5_cfg),
                    "-I",
                    "10000",
                    f"--disk-image={disk_image}",
                    f"--bootloader={bootloader}",
                    "--caches",
                    "--cpu-type",
                    "AtomicSimpleCPU",
                    "--fdip",
                    "--bp-type",
                    "TAGE",
                    "--restore",
                    str(checkpoint_dir),
                    "--num-cores",
                    str(core_count),
                    "--mem-size",
                    "16384MiB",
                    "--itb-size",
                    str(DEFAULT_ITB_SIZE),
                    "--dtb-size",
                    str(DEFAULT_DTB_SIZE),
                    "--have-large-asid-64",
                    "--va-file",
                    tlb_source,
                    "--va-cpu-id",
                    str(cpu),
                    "--tlb-output-dir",
                    str(gem5_uarch_dir),
                    "--kernel",
                    str(kernel),
                ],
                cwd=str(qpoints_root / "gem5"),
                text=True,
                check=True,
            )
    finally:
        if outdir.exists():
            shutil.rmtree(outdir)


def _finalize_snapshot_checkpoint(
    repo_root: Path,
    checkpoint_dir: Path,
    core_count: int,
) -> None:
    create_gem5_checkpoint = repo_root / "create_gem5_checkpoint.py"
    subprocess.run(
        [
            sys.executable,
            str(create_gem5_checkpoint),
            str(checkpoint_dir),
            "--num-cores",
            str(core_count),
        ],
        cwd=str(repo_root),
        text=True,
        check=True,
    )


def _positive_int_env(name: str, default: int) -> int:
    value = os.environ.get(name, str(default))
    if not re.fullmatch(r"[1-9][0-9]*", value):
        raise RuntimeError(
            f"{name} must be a positive integer, got: {value!r}."
        )
    return int(value)


def _snapshot_names(first: str, last: str) -> list[str]:
    first_match = re.fullmatch(r"snapshot_(\d+)", first)
    last_match = re.fullmatch(r"snapshot_(\d+)", last)
    if not first_match or not last_match:
        raise RuntimeError("First/last must be in the form snapshot_N.")
    first_idx = int(first_match.group(1))
    last_idx = int(last_match.group(1))
    if first_idx > last_idx:
        raise RuntimeError("First snapshot index must be <= last snapshot index.")
    return [f"snapshot_{i}" for i in range(first_idx, last_idx + 1)]


def _load_uipc_summary(path: Path) -> dict:
    if not path.is_file():
        raise RuntimeError(f"Expected per-snapshot uIPC summary not found: {path}")
    return json.loads(path.read_text(encoding="utf-8"))


def _write_uipc_report(qpoints_root: Path, experiment: str, summaries: list[dict]) -> Path:
    if not summaries:
        raise RuntimeError("No per-snapshot uIPC summaries were produced.")

    first_cores = summaries[0].get("cores", [])
    core_ids = [int(item["core"]) for item in first_cores]
    per_core_ipc_totals = {core: 0.0 for core in core_ids}
    per_core_uipc_totals = {core: 0.0 for core in core_ids}
    aggregate_ipc_total = 0.0
    aggregate_uipc_total = 0.0

    for summary in summaries:
        summary_cores = summary.get("cores", [])
        summary_core_ids = [int(item["core"]) for item in summary_cores]
        if summary_core_ids != core_ids:
            raise RuntimeError(
                "Inconsistent per-core uIPC summaries across snapshots; cannot aggregate."
            )
        for item in summary_cores:
            core = int(item["core"])
            per_core_ipc_totals[core] += float(item.get("ipc", 0.0))
            per_core_uipc_totals[core] += float(item.get("uipc", 0.0))
        aggregate_ipc_total += float(summary.get("aggregate", {}).get("ipc", 0.0))
        aggregate_uipc_total += float(summary.get("aggregate", {}).get("uipc", 0.0))

    snapshot_count = len(summaries)
    report_cores = [
        {
            "core": core,
            "ipc": per_core_ipc_totals[core] / snapshot_count,
            "uipc": per_core_uipc_totals[core] / snapshot_count,
        }
        for core in core_ids
    ]
    aggregate_ipc = aggregate_ipc_total / snapshot_count
    aggregate_uipc = aggregate_uipc_total / snapshot_count
    average_ipc = (
        sum(item["ipc"] for item in report_cores) / len(report_cores)
        if report_cores else 0.0
    )
    average_uipc = (
        sum(item["uipc"] for item in report_cores) / len(report_cores)
        if report_cores else 0.0
    )

    report = {
        "engine": "gem5",
        "experiment": experiment,
        "snapshot_count": snapshot_count,
        "cores": report_cores,
        "aggregate": {"ipc": aggregate_ipc, "uipc": aggregate_uipc},
        "average": {"ipc": average_ipc, "uipc": average_uipc},
    }

    report_path = qpoints_root / "sim_outs" / experiment / "uipc_report.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return report_path


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
            qemu_cpu = resolve_qemu_cpu()
            with qemu_log.open("w") as log_file:
                subprocess.run(
                    [
                        str(qemu_bin),
                        "-M", "virt,gic-version=max,virtualization=off,secure=off",
                        "-smp", str(core_count),
                        "-cpu", qemu_cpu,
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
        if not store0.is_file():
            store0 = _require_qpoints_file(
                qpoints_root, "scripts/base_files/system.physmem.store0.pmem"
            )
        shutil.copy2(store0, img_dest_dir / "system.physmem.store0.pmem")

        print(f"[{snapshot}] composing base m5.cpt")
        _finalize_snapshot_checkpoint(repo_root, img_dest_dir, core_count)

        _check_cancelled()
        uarch_manifest = _prepare_snapshot_gem5_uarch(
            qpoints_root, qflex_ckp_dir, gem5_ckp_dir, snapshot, ruby_protocol
        )
        print(f"[{snapshot}] applying gem5 uarch artifacts")
        _apply_snapshot_gem5_uarch(
            qpoints_root,
            img_dest_dir,
            snapshot,
            core_count,
            uarch_manifest,
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
    if parallel < 1:
        raise RuntimeError("Parallel must be >= 1.")

    snapshots = _snapshot_names(first, last)
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


def run_sample(
    gem5_ckp_dir: str,
    experiment: str,
    first: str,
    last: str,
    core_count: int,
    warmup_cycles: int,
    measurement_cycles: int,
    timing_ruby: bool = False,
    timing_ruby_moesi: bool = False,
    sim_config: Optional[str] = None,
) -> Path:
    if timing_ruby and timing_ruby_moesi:
        raise RuntimeError(
            "Choose only one timing Ruby protocol flag: --timing-ruby or --timing-ruby-moesi."
        )
    if not (timing_ruby or timing_ruby_moesi):
        raise RuntimeError(
            "run_sample currently requires --timing-ruby or --timing-ruby-moesi."
        )
    if measurement_cycles <= 0:
        raise RuntimeError("measurement_cycles must be > 0")
    if warmup_cycles < 0:
        raise RuntimeError("warmup_cycles must be >= 0")

    repo_root = Path(__file__).resolve().parents[1]
    qpoints_root = repo_root / "QPoints"
    snapshots = _snapshot_names(first, last)
    summaries = []

    for snapshot in snapshots:
        run_gem5(
            gem5_ckp_dir=gem5_ckp_dir,
            experiment=experiment,
            snapshot=snapshot,
            inst=None,
            warmup_cycles=warmup_cycles,
            measurement_cycles=measurement_cycles,
            core_count=core_count,
            timing_ruby=timing_ruby,
            timing_ruby_moesi=timing_ruby_moesi,
            sim_config=sim_config,
        )
        summary_path = qpoints_root / "sim_outs" / experiment / snapshot / "uipc_summary.json"
        summaries.append(_load_uipc_summary(summary_path))

    report_path = _write_uipc_report(qpoints_root, experiment, summaries)
    print(f"uIPC report written to {report_path}")
    return report_path


def run_gem5(
    gem5_ckp_dir: str,
    experiment: str,
    snapshot: str,
    inst: Optional[int] = None,
    warmup_cycles: Optional[int] = None,
    measurement_cycles: Optional[int] = None,
    core_count: int = 1,
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
    if measurement_cycles is not None or warmup_cycles is not None:
        if not (timing_ruby or timing_ruby_moesi):
            raise RuntimeError(
                "Cycle-window timing requires --timing-ruby or --timing-ruby-moesi."
            )
        if inst is not None:
            raise RuntimeError(
                "Do not mix --inst with --warmup-cycles/--measurement-cycles."
            )
        if measurement_cycles is None:
            raise RuntimeError("--warmup-cycles requires --measurement-cycles.")
    elif inst is None:
        raise RuntimeError("Either inst or measurement_cycles must be provided.")

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
        "--cores",
        str(core_count),
    ]
    if inst is not None:
        args.extend(["--inst", str(inst)])
    if warmup_cycles is not None:
        args.extend(["--warmup-cycles", str(warmup_cycles)])
    if measurement_cycles is not None:
        args.extend(["--measurement-cycles", str(measurement_cycles)])
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
