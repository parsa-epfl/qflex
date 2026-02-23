import os
import re
import shutil
import subprocess
import time
from pathlib import Path


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
    try:
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

        gen_snapshot = qpoints_root / "gen_snapshot.sh"
        tmp_log = run_dir / f"gen_snapshot_{snapshot}.log"
        with tmp_log.open("w") as err_log:
            subprocess.run(
                [
                    str(gen_snapshot),
                    gem5_ckp_dir,
                    snapshot,
                    "",
                    "0",
                    str(core_count),
                    str(monitor_port),
                ],
                stderr=err_log,
                text=True,
                check=True,
            )

        img_dest_dir = Path(gem5_ckp_dir) / snapshot
        if img_dest_dir.is_dir():
            shutil.move(str(tmp_log), str(img_dest_dir / tmp_log.name))
        else:
            raise RuntimeError(f"[{snapshot}] Destination directory does not exist: {img_dest_dir}")

        convert_sh = run_dir / "convert.sh"
        if not convert_sh.exists():
            src = qpoints_root / "scripts" / "qflex" / "convert.sh"
            shutil.copy2(src, convert_sh)
        _ensure_executable(convert_sh)

        subprocess.run(
            [str(convert_sh), base, snapshot],
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
        if qemu_proc is not None:
            qemu_proc.terminate()
        raise
    finally:
        elapsed = int(time.time() - start_time)
        print(f"[{snapshot}] convert-single completed in {elapsed}s")
