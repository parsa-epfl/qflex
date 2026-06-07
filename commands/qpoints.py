import os
import re
import shutil
import subprocess
import time
import json
import tempfile
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
    protocol_order = [ruby_protocol] + [
        protocol
        for protocol in ("mesi_two_level", "moesi_cmp_directory")
        if protocol != ruby_protocol
    ]
    try:
        for protocol in protocol_order:
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
                    protocol,
                    "--overwrite",
                ],
                text=True,
                check=True,
            )
        manifest_file = (
            Path(gem5_ckp_dir)
            / snapshot
            / "gem5_uarch"
            / ruby_protocol
            / "manifest.json"
        )
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
    memory_gb: int,
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
                    f"{memory_gb * 1024}MiB",
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
        "engine": summaries[0].get("engine", "gem5"),
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


def _require_experiment_file(path: Path) -> Path:
    if not path.exists():
        raise RuntimeError(f"Required Flexus experiment artifact not found: {path}")
    return path


def _parse_flexus_commit_counters(path: Path, core_count: int) -> dict[int, dict[str, int]]:
    counters = {
        core: {"commits": 0, "nonspin_user": 0}
        for core in range(core_count)
    }
    total_pattern = re.compile(r"^\s*(\d+)-uarch-Commits\s+(\d+)\s*$")
    user_pattern = re.compile(r"^\s*(\d+)-uarch-Commits:NonSpin:User\s+(\d+)\s*$")

    with path.open(encoding="utf-8") as infile:
        for line in infile:
            total_match = total_pattern.match(line)
            if total_match:
                core = int(total_match.group(1))
                if core < core_count:
                    counters[core]["commits"] = int(total_match.group(2))
                continue
            user_match = user_pattern.match(line)
            if user_match:
                core = int(user_match.group(1))
                if core < core_count:
                    counters[core]["nonspin_user"] = int(user_match.group(2))
    return counters


def _patch_flexus_timing_cfg(timing_cfg: Path, flexus_cfg: dict) -> None:
    text = timing_cfg.read_text(encoding="utf-8")
    replacements = {
        "-fag:btbsets": str(int(flexus_cfg["btb_sets"])),
        "-fag:btbways": str(int(flexus_cfg["btb_associativity"])),
        "-mmu:itlb_set": str(int(flexus_cfg["itlb_sets"])),
        "-mmu:itlb_assoc": str(int(flexus_cfg["itlb_associativity"])),
        "-mmu:dtlb_set": str(int(flexus_cfg["dtlb_sets"])),
        "-mmu:dtlb_assoc": str(int(flexus_cfg["dtlb_associativity"])),
        "-mmu:stlb_sete": str(int(flexus_cfg["stlb_sets"])),
        "-mmu:stlb_assoc": str(int(flexus_cfg["stlb_associativity"])),
    }

    for key, value in replacements.items():
        pattern = rf'(flexus\.set\s+"{re.escape(key)}"\s+)"[^"]+"'
        text, count = re.subn(pattern, rf'\1"{value}"', text)
        if count == 0:
            text += f'\nflexus.set "{key}"          "{value}"\n'

    timing_cfg.write_text(text, encoding="utf-8")


def _prepare_flexus_snapshot_workspace(
    experiment_root: Path,
    snapshot: str,
    core_count: int,
) -> tuple[Path, Path]:
    snapshot_idx = _snapshot_index(snapshot)
    run_dir = experiment_root / "run"
    cfg_dir = experiment_root / "cfg"
    lib_dir = experiment_root / "lib"
    scripts_dir = experiment_root / "scripts"
    bin_dir = experiment_root / "bin"

    workspace_root = Path(tempfile.mkdtemp(prefix=f"flexus_run_sample_{snapshot}_"))
    workspace = workspace_root / "run" / "partition_0"
    (workspace_root / "cfg").mkdir(parents=True, exist_ok=True)
    (workspace_root / "lib").mkdir(parents=True, exist_ok=True)
    workspace.mkdir(parents=True, exist_ok=True)

    flexus_cfg_path = _require_experiment_file(cfg_dir / "flexus_configuration.json")
    flexus_cfg = json.loads(flexus_cfg_path.read_text(encoding="utf-8"))

    shutil.copy2(flexus_cfg_path, workspace_root / "cfg" / "flexus_configuration.json")
    timing_cfg_src = _require_experiment_file(cfg_dir / "timing.cfg")
    timing_cfg_dst = workspace_root / "cfg" / "timing.cfg"
    shutil.copy2(timing_cfg_src, timing_cfg_dst)
    _patch_flexus_timing_cfg(timing_cfg_dst, flexus_cfg)

    shutil.copy2(_require_experiment_file(scripts_dir / "run_flexus.sh"), workspace / "run_flexus.sh")
    _ensure_executable(workspace / "run_flexus.sh")
    script_text = (workspace / "run_flexus.sh").read_text(encoding="utf-8")
    script_text = re.sub(r"(-smp\s+)\d+", rf"\g<1>{core_count}", script_text)
    (workspace / "run_flexus.sh").write_text(script_text, encoding="utf-8")

    image_candidates = sorted(run_dir.glob("*.qcow2"))
    if not image_candidates:
        raise RuntimeError(f"No qcow2 image found in {run_dir}")
    image_path = image_candidates[0]

    symlinks = {
        workspace / "QEMU_EFI.fd": _require_experiment_file(run_dir / "QEMU_EFI.fd"),
        workspace / "efi-virtio.rom": _require_experiment_file(run_dir / "efi-virtio.rom"),
        workspace / "debug.cfg": _require_experiment_file(run_dir / "debug.cfg"),
        workspace / "root.qcow2": image_path,
        workspace / f"{snapshot}.loc": _require_experiment_file(run_dir / f"{snapshot}.loc"),
        workspace / f"{snapshot}.state.zstd": _require_experiment_file(run_dir / f"{snapshot}.state.zstd"),
        workspace / f"{snapshot}.uarch": _require_experiment_file(run_dir / f"{snapshot}.uarch"),
        workspace / "init_warmed.mem": _require_experiment_file(run_dir / "init_warmed.mem"),
        workspace / "checkpoint_conversion": _require_experiment_file(bin_dir / "checkpoint_conversion"),
        workspace_root / "lib" / "libknottykraken.so": _require_experiment_file(lib_dir / "libknottykraken.so"),
        workspace_root / "run" / "vanilla-qemu-system-aarch64": _require_experiment_file(run_dir / "vanilla-qemu-system-aarch64"),
    }

    for target, source in symlinks.items():
        target.symlink_to(source)

    return workspace_root, workspace / f"result_{snapshot_idx}"


def _build_flexus_snapshot_summary(
    snapshot: str,
    start_counters: dict[int, dict[str, int]],
    end_counters: dict[int, dict[str, int]],
    core_count: int,
    measurement_cycles: int,
) -> dict:
    cores = []
    aggregate_ipc = 0.0
    aggregate_uipc = 0.0

    for core in range(core_count):
        delta_commits = end_counters[core]["commits"] - start_counters[core]["commits"]
        delta_ucommits = end_counters[core]["nonspin_user"] - start_counters[core]["nonspin_user"]
        ipc = delta_commits / measurement_cycles
        uipc = delta_ucommits / measurement_cycles
        aggregate_ipc += ipc
        aggregate_uipc += uipc
        cores.append({"core": core, "ipc": ipc, "uipc": uipc})

    return {
        "engine": "flexus",
        "snapshot": snapshot,
        "cores": cores,
        "aggregate": {"ipc": aggregate_ipc, "uipc": aggregate_uipc},
    }


def run_sample_flexus(
    experiment: str,
    first: str,
    last: str,
    core_count: int,
    warmup_cycles: int,
    measurement_cycles: int,
    mounting_folder: str,
) -> Path:
    if measurement_cycles <= 0:
        raise RuntimeError("measurement_cycles must be > 0")
    if warmup_cycles < 0:
        raise RuntimeError("warmup_cycles must be >= 0")
    if warmup_cycles % 100000 != 0 or measurement_cycles % 100000 != 0:
        raise RuntimeError(
            "Flexus run_sample currently requires warmup and measurement windows "
            "to be multiples of 100000 cycles."
        )

    repo_root = Path(__file__).resolve().parents[1]
    qpoints_root = repo_root / "QPoints"
    snapshots = _snapshot_names(first, last)
    experiment_root = Path(mounting_folder) / "experiments" / experiment
    if not experiment_root.is_dir():
        raise RuntimeError(f"Flexus experiment folder not found: {experiment_root}")

    warming_ratio = warmup_cycles // 100000
    measurement_ratio = measurement_cycles // 100000
    summaries = []

    for snapshot in snapshots:
        workspace_root, result_dir = _prepare_flexus_snapshot_workspace(
            experiment_root, snapshot, core_count
        )
        workspace = workspace_root / "run" / "partition_0"
        try:
            subprocess.run(
                ["bash", "./run_flexus.sh", str(warming_ratio), str(measurement_ratio)],
                cwd=str(workspace),
                text=True,
                check=True,
            )
            start_counters = {
                core: {"commits": 0, "nonspin_user": 0}
                for core in range(core_count)
            }
            if warmup_cycles > 0:
                warmup_log = result_dir / f"all.measurement.{warmup_cycles:010d}.log"
                start_counters = _parse_flexus_commit_counters(warmup_log, core_count)
            end_counters = _parse_flexus_commit_counters(
                result_dir / "all.measurement.end.log", core_count
            )
            summary = _build_flexus_snapshot_summary(
                snapshot,
                start_counters,
                end_counters,
                core_count,
                measurement_cycles,
            )
            output_dir = qpoints_root / "sim_outs" / experiment / snapshot
            output_dir.mkdir(parents=True, exist_ok=True)
            for artifact_name in (
                "all.measurement.end.log",
                "qemu-timing.log",
                "debug.log",
            ):
                artifact = result_dir / artifact_name
                if artifact.is_file():
                    shutil.copy2(artifact, output_dir / artifact_name)
            summary_path = output_dir / "uipc_summary.json"
            summary_path.write_text(
                json.dumps(summary, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
            summaries.append(summary)
        finally:
            shutil.rmtree(workspace_root, ignore_errors=True)

    report_path = _write_uipc_report(qpoints_root, experiment, summaries)
    print(f"uIPC report written to {report_path}")
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
            memory_gb,
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
    memory_gb: int,
    warmup_cycles: int,
    measurement_cycles: int,
    timing_ruby: bool = False,
    timing_ruby_moesi: bool = False,
    cache_hierarchy_restore: bool = True,
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
            memory_gb=memory_gb,
            timing_ruby=timing_ruby,
            timing_ruby_moesi=timing_ruby_moesi,
            cache_hierarchy_restore=cache_hierarchy_restore,
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
    memory_gb: int = 16,
    branch_trace: bool = False,
    tage_decision_trace: bool = False,
    data_trace: bool = False,
    dump_cache_state: bool = False,
    timing_ruby: bool = False,
    timing_ruby_moesi: bool = False,
    cache_hierarchy_restore: bool = True,
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
        "--experiment-name",
        experiment,
        "--snapshot",
        snapshot,
        "--core-count",
        str(core_count),
        "--memory-gb",
        str(memory_gb),
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
    if not cache_hierarchy_restore:
        args.append("--no-cache-hierarchy-restore")
    if sim_config:
        args.extend(["--sim-config", sim_config])
    subprocess.run(
        args,
        cwd=str(qpoints_root),
        text=True,
        check=True,
    )
