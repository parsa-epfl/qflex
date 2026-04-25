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


def _prepare_snapshot_gem5_uarch(
    qpoints_root: Path,
    qflex_ckp_dir: str,
    gem5_ckp_dir: str,
    snapshot: str,
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
                "python3",
                str(prepare_script),
                "--qflex-run-dir",
                str(Path(qflex_ckp_dir) / "run"),
                "--gem5-workload-root",
                gem5_ckp_dir,
                "--snapshot",
                snapshot,
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
) -> None:
    start_time = time.time()

    repo_root = Path(__file__).resolve().parents[1]
    qpoints_root = repo_root / "QPoints"
    _prepare_qpoints_root(
        qpoints_root,
        (
            "gen_snapshot.sh",
            "scripts/qflex/run_qemu_emu.sh",
            "scripts/qflex/convert.sh",
        ),
    )

    run_dir = Path(qflex_ckp_dir) / "run"
    if not run_dir.is_dir():
        raise RuntimeError(f"run directory not found: {run_dir}")

    snapshot_idx = _snapshot_index(snapshot)
    helper_suffix = f"{snapshot}.{os.getpid()}.{threading.get_ident()}.sh"
    refreshed_helpers = []

    monitor_port = monitor_base + snapshot_idx
    qmp_port = qmp_base + snapshot_idx
    ssh_port = ssh_base + snapshot_idx

    run_qemu_emu = _refresh_qpoints_helper(
        qpoints_root,
        run_dir,
        "scripts/qflex/run_qemu_emu.sh",
        dest_name=f".run_qemu_emu.{helper_suffix}",
    )
    refreshed_helpers.append(run_qemu_emu)

    qemu_log = run_dir / f"qemu_emu_{snapshot}.log"
    qemu_proc = None
    converted_img_tmp = None

    def _check_cancelled() -> None:
        if cancel_event is not None and cancel_event.is_set():
            raise RuntimeError(f"[{snapshot}] conversion cancelled")

    def _ensure_qemu_running(phase: str) -> None:
        if qemu_proc is not None and qemu_proc.poll() is not None:
            raise RuntimeError(
                f"[{snapshot}] qemu exited during {phase} "
                f"(exit code {qemu_proc.returncode}).{_tail_file(qemu_log)}"
            )

    def _ensure_qemu_not_failed(phase: str) -> None:
        if qemu_proc is None:
            return
        if qemu_proc.poll() not in (None, 0):
            raise RuntimeError(
                f"[{snapshot}] qemu failed before {phase} "
                f"(exit code {qemu_proc.returncode}).{_tail_file(qemu_log)}"
            )

    def _terminate_qemu() -> None:
        if qemu_proc is None:
            return
        if qemu_proc.poll() is not None:
            return
        try:
            os.killpg(qemu_proc.pid, signal.SIGTERM)
        except ProcessLookupError:
            return
        except Exception:
            qemu_proc.terminate()
        try:
            qemu_proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(qemu_proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                return
            except Exception:
                qemu_proc.kill()
            qemu_proc.wait()

    def _handle_signal(_signum, _frame) -> None:
        _terminate_qemu()
        raise KeyboardInterrupt
    try:
        _check_cancelled()
        if threading.current_thread() is threading.main_thread():
            signal.signal(signal.SIGINT, _handle_signal)
            signal.signal(signal.SIGTERM, _handle_signal)
        print(f"[{snapshot}] starting qemu emulation")
        with qemu_log.open("w") as log_file:
            qemu_proc = subprocess.Popen(
                [
                    str(run_qemu_emu),
                    str(core_count),
                    f"{memory_gb}G",
                    base,
                    snapshot,
                    str(monitor_port),
                    str(qmp_port),
                    str(ssh_port),
                ],
                cwd=str(run_dir),
                stdout=log_file,
                stderr=subprocess.STDOUT,
                text=True,
                start_new_session=True,
            )

        ssh_env = os.environ.copy()
        ssh_env["SSHPASS"] = os.environ.get("QPOINTS_SSH_PASSWORD", "qflex")
        max_ssh_attempts = _positive_int_env("QPOINTS_SSH_MAX_ATTEMPTS", 120)
        for ssh_attempt in range(1, max_ssh_attempts + 1):
            _check_cancelled()
            _ensure_qemu_running("SSH readiness wait")
            result = subprocess.run(
                [
                    "sshpass",
                    "-e",
                    "ssh",
                    "-o",
                    "ConnectTimeout=2",
                    "-o",
                    "StrictHostKeyChecking=no",
                    "-o",
                    "UserKnownHostsFile=/dev/null",
                    "-p",
                    str(ssh_port),
                    f"{ssh_user}@{ssh_host}",
                    "true",
                ],
                env=ssh_env,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                text=True,
            )
            if result.returncode == 0:
                break
            print(
                f"[{snapshot}] waiting for vm ssh "
                f"({ssh_user}@{ssh_host}:{ssh_port})... "
                f"({ssh_attempt}/{max_ssh_attempts})"
            )
            time.sleep(0.5)
        else:
            raise RuntimeError(
                f"[{snapshot}] timed out waiting for vm ssh "
                f"({ssh_user}@{ssh_host}:{ssh_port}) after "
                f"{max_ssh_attempts} attempts.{_tail_file(qemu_log)}"
            )

        print(f"[{snapshot}] generating gem5 checkpoint")
        _check_cancelled()
        _ensure_qemu_running("gem5 checkpoint generation")
        img_dest_dir = Path(gem5_ckp_dir) / snapshot
        if img_dest_dir.exists():
            if overwrite:
                shutil.rmtree(img_dest_dir)
            elif sys.stdin.isatty():
                resp = input(
                    f"Destination directory already exists: {img_dest_dir}\n"
                    "Delete it and proceed? [y/n]: "
                )
                if resp.strip().lower() not in {"y", "yes"}:
                    raise RuntimeError("Aborting.")
                shutil.rmtree(img_dest_dir)
            else:
                raise RuntimeError(
                    f"Destination directory already exists: {img_dest_dir}. "
                    "Re-run with --overwrite to remove it."
                )

        gen_snapshot = qpoints_root / "gen_snapshot.sh"
        tmp_log = run_dir / f"gen_snapshot_{snapshot}.log"
        with tmp_log.open("w") as err_log:
            subprocess.run(
                [
                    "bash",
                    str(gen_snapshot),
                    gem5_ckp_dir,
                    snapshot,
                    "",
                    "0",
                    str(core_count),
                    str(monitor_port),
                ],
                stderr=err_log,
                cwd=str(qpoints_root),
                text=True,
                check=True,
            )

        if img_dest_dir.is_dir():
            shutil.move(str(tmp_log), str(img_dest_dir / tmp_log.name))
        else:
            raise RuntimeError(
                f"[{snapshot}] Destination directory does not exist: {img_dest_dir}"
            )

        print(f"[{snapshot}] converting disk image")
        _check_cancelled()
        _ensure_qemu_not_failed("disk image conversion")
        convert_sh = _refresh_qpoints_helper(
            qpoints_root,
            run_dir,
            "scripts/qflex/convert.sh",
            dest_name=f".convert.{helper_suffix}",
        )
        refreshed_helpers.append(convert_sh)

        converted_img = img_dest_dir / f"{snapshot}.img"
        converted_img_tmp = (
            img_dest_dir
            / f".{snapshot}.img.tmp.{os.getpid()}.{threading.get_ident()}"
        )
        for path in (converted_img, converted_img_tmp):
            if path.exists():
                path.unlink()

        subprocess.run(
            ["bash", str(convert_sh), base, snapshot, str(converted_img_tmp)],
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
        _check_cancelled()
        _prepare_snapshot_gem5_uarch(
            qpoints_root, qflex_ckp_dir, gem5_ckp_dir, snapshot
        )
    except KeyboardInterrupt:
        _terminate_qemu()
        raise
    finally:
        if converted_img_tmp is not None and converted_img_tmp.exists():
            converted_img_tmp.unlink()
        _terminate_qemu()
        for helper in refreshed_helpers:
            if helper.exists():
                helper.unlink()
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
    data_trace: bool = False,
    dump_cache_state: bool = False,
    timing_ruby: bool = False,
    sim_config: Optional[str] = None,
) -> None:
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
    if data_trace:
        args.append("--data-trace")
    if dump_cache_state:
        args.append("--dump-cache-state")
    if timing_ruby:
        args.append("--timing-ruby")
    if sim_config:
        args.extend(["--sim-config", sim_config])
    subprocess.run(
        args,
        cwd=str(qpoints_root),
        text=True,
        check=True,
    )
