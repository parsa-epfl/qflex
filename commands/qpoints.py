import os
import re
import shutil
import subprocess
import time
from pathlib import Path
import sys
import signal


def _ensure_executable(path: Path) -> None:
    mode = path.stat().st_mode
    path.chmod(mode | 0o111)


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
) -> None:
    start_time = time.time()

    repo_root = Path(__file__).resolve().parents[1]
    qpoints_root = repo_root / "QPoints"
    run_dir = Path(qflex_ckp_dir) / "run"
    if not run_dir.is_dir():
        raise RuntimeError(f"run directory not found: {run_dir}")

    snapshot_idx = 0
    match = re.search(r"_([0-9]+)$", snapshot)
    if match:
        snapshot_idx = int(match.group(1))

    monitor_port = monitor_base + snapshot_idx
    qmp_port = qmp_base + snapshot_idx
    ssh_port = ssh_base + snapshot_idx

    run_qemu_emu = run_dir / "run_qemu_emu.sh"
    if not run_qemu_emu.exists():
        src = qpoints_root / "scripts" / "qflex" / "run_qemu_emu.sh"
        shutil.copy2(src, run_qemu_emu)
    _ensure_executable(run_qemu_emu)

    qemu_log = run_dir / f"qemu_emu_{snapshot}.log"
    qemu_proc = None
    def _terminate_qemu() -> None:
        if qemu_proc is None:
            return
        if qemu_proc.poll() is not None:
            return
        qemu_proc.terminate()
        try:
            qemu_proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            qemu_proc.kill()
            qemu_proc.wait()

    def _handle_signal(_signum, _frame) -> None:
        _terminate_qemu()
        raise KeyboardInterrupt
    try:
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
            )

        ssh_env = os.environ.copy()
        ssh_env["SSHPASS"] = "qflex"
        while True:
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
            print(f"[{snapshot}] waiting for vm ssh ({ssh_user}@{ssh_host}:{ssh_port})...")
            time.sleep(0.5)

        print(f"[{snapshot}] generating gem5 checkpoint")
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
            raise RuntimeError(f"[{snapshot}] Destination directory does not exist: {img_dest_dir}")

        print(f"[{snapshot}] converting disk image")
        convert_sh = run_dir / "convert.sh"
        if not convert_sh.exists():
            src = qpoints_root / "scripts" / "qflex" / "convert.sh"
            shutil.copy2(src, convert_sh)
        _ensure_executable(convert_sh)

        subprocess.run(
            ["bash", str(convert_sh), base, snapshot],
            cwd=str(run_dir),
            text=True,
            check=True,
        )

        img_src = run_dir / f"{snapshot}.img"
        if img_src.is_file():
            shutil.move(str(img_src), str(img_dest_dir / img_src.name))
        else:
            raise RuntimeError(f"[{snapshot}] Converted image not found: {img_src}")
    except KeyboardInterrupt:
        _terminate_qemu()
        raise
    finally:
        _terminate_qemu()
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
) -> None:
    repo_root = Path(__file__).resolve().parents[1]
    qpoints_root = repo_root / "QPoints"
    run_all_multi = qpoints_root / "run_all_multi_snapshot.sh"
    subprocess.run(
        [
            "bash",
            str(run_all_multi),
            "--first",
            first,
            "--last",
            last,
            "--parallel",
            str(parallel),
            "--qflex-ckp-dir",
            qflex_ckp_dir,
            "--gem5-ckp-dir",
            gem5_ckp_dir,
            "--core-count",
            str(core_count),
            "--memory-gb",
            str(memory_gb),
            "--base",
            base,
            "--ssh-host",
            ssh_host,
            "--ssh-user",
            ssh_user,
            "--monitor-base",
            str(monitor_base),
            "--qmp-base",
            str(qmp_base),
            "--ssh-base",
            str(ssh_base),
        ],
        cwd=str(qpoints_root),
        text=True,
        check=True,
    )
