import re
import subprocess
import concurrent.futures
from pathlib import Path

from commands import Executor
from commands.config import ExperimentContext
from commands.qemu import QemuCommonArgParser


class ConvertSingle(Executor):
    def __init__(
        self,
        experiment_context: ExperimentContext,
        snapshot: str,
        gem5_ckp_dir: str,
        overwrite: bool = False,
    ):
        self.experiment_context = experiment_context
        self.simulation_context = experiment_context.simulation_context
        self.snapshot = snapshot
        self.gem5_ckp_dir = gem5_ckp_dir
        self.overwrite = overwrite

        self.qemu_common_parser = QemuCommonArgParser(experiment_context)
        # -convert-to-gem5-chkp handles snapshot loading; suppress any -loadvm
        self.qemu_common_parser.loadvm = ''

    def cmd(self):
        run_dir = f"{self.experiment_context.get_experiment_folder_address()}/run"
        gem5_output = f"{self.gem5_ckp_dir}/{self.snapshot}"
        core_count = self.simulation_context.core_count
        base_image_address = self.qemu_common_parser.image_address
        qpoints_root = Path(__file__).resolve().parents[1] / "QPoints"

        # Check if gem5 checkpoint files already exist
        gem5_dir = f"{run_dir}/{self.snapshot}.gem"
        register_info = Path(f"{gem5_dir}/register-info.json")
        dev_info = Path(f"{gem5_dir}/dev.info")
        physmem = Path(f"{gem5_dir}/system.physmem.store1.pmem")
        checkpoint_files_exist = register_info.exists() and dev_info.exists() and physmem.exists()

        # Check if TLB data exists
        va_file = f"{run_dir}/{self.snapshot}.uarch/mmus-0.json"
        tlb_data_exists = Path(va_file).exists()

        qemu_cmd = f"""
            ./qemu-system-aarch64 \
            {self.qemu_common_parser.get_qemu_base_args()} \
            {self.qemu_common_parser.quantum_args()} \
            -convert-to-gem5-chkp {self.snapshot}
        """

        tlb_output_dir = f"{run_dir}/{self.snapshot}.gem"

        commands = [f"cd {run_dir}"]
        if self.overwrite:
            commands.append(f"rm -rf {gem5_output}")
        
        # Conditionally run qemu_cmd only if checkpoint files don't exist
        if not checkpoint_files_exist:
            commands.append(qemu_cmd)
        
        # Always run these
        commands.extend([
            f"./qemu-img convert -f qcow2 -O raw -l {self.snapshot} {base_image_address} {self.snapshot}.gem/{self.snapshot}.img",
            f"cp ./system.physmem.store0.pmem {self.snapshot}.gem/",
            f"python3 ../create_gem5_checkpoint.py {self.snapshot}.gem --num-cores {core_count}",
        ])
        
        # Conditionally run TLB-related commands only if TLB data exists
        if tlb_data_exists:
            commands.extend([
                f"(cd {qpoints_root} && bash run_gem5.sh --gem5-ckp-dir {run_dir} --experiment experiment_{self.snapshot} --snapshot {self.snapshot} --inst 10000 --cores {core_count} --va-file {va_file} --tlb-output-dir {tlb_output_dir})",
                f"rm -rf {self.snapshot}.gem/m5.cpt",
                f"python3 ../create_gem5_checkpoint.py {self.snapshot}.gem --num-cores {core_count}",
            ])
        
        # Always run final move
        commands.append(f"mv {self.snapshot}.gem {gem5_output}")
        
        return commands


def convert_single(
    experiment_context: ExperimentContext,
    snapshot: str,
    gem5_ckp_dir: str,
    overwrite: bool = False,
) -> None:
    ConvertSingle(
        experiment_context=experiment_context,
        snapshot=snapshot,
        gem5_ckp_dir=gem5_ckp_dir,
        overwrite=overwrite,
    ).execute(to_stdio=True, run_in_background=False)


def convert_multi(
    first: str,
    last: str,
    parallel: int,
    experiment_context: ExperimentContext,
    gem5_ckp_dir: str,
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
    errors = []

    def _run(snapshot: str) -> None:
        convert_single(
            experiment_context=experiment_context,
            snapshot=snapshot,
            gem5_ckp_dir=gem5_ckp_dir,
            overwrite=overwrite,
        )

    with concurrent.futures.ThreadPoolExecutor(max_workers=parallel) as executor:
        future_map = {executor.submit(_run, snap): snap for snap in snapshots}
        for future in concurrent.futures.as_completed(future_map):
            snap = future_map[future]
            try:
                future.result()
            except Exception as exc:
                errors.append((snap, exc))
                for pending in future_map:
                    pending.cancel()
                break

    if errors:
        snap, exc = errors[0]
        raise RuntimeError(f"convert-multi failed at {snap}: {exc}") from exc


def run_gem5(
    gem5_ckp_dir: str,
    experiment: str,
    snapshot: str,
    inst: int,
    core_count: int,
    branch_trace: bool = False,
) -> None:
    repo_root = Path(__file__).resolve().parents[1]
    qpoints_root = repo_root / "QPoints"
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
    subprocess.run(
        args,
        cwd=str(qpoints_root),
        text=True,
        check=True,
    )
