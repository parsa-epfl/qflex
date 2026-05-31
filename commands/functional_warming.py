import math
import re
from pathlib import Path

from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser


class FunctionalWarming(Executor):
    """
    This class handles the functional warming phase of the experiment and generates checkpoints.
    """

    def __init__(self,
                 experiment_context: ExperimentContext,
                 sample_size: int,
                 collect_gem5_bbl_btb: bool = False):
        self.experiment_context = experiment_context
        self.simulation_context = self.experiment_context.simulation_context
        self.qemu_common_parser = QemuCommonArgParser(experiment_context)
        self.sample_size = sample_size
        self.collect_gem5_bbl_btb = collect_gem5_bbl_btb
        self.sampling_interval = math.ceil(
            (self.experiment_context.workload.population + self.sample_size - 1) / self.sample_size
        )

    def next_snapshot_init_index(self) -> int:
        run_dir = Path(self.experiment_context.get_experiment_folder_address()) / "run"
        pattern = re.compile(r"snapshot_(\d+)(?:$|\.)")
        max_index = -1
        for path in run_dir.iterdir():
            match = pattern.match(path.name)
            if match:
                max_index = max(max_index, int(match.group(1)))
        return max_index + 1

    def cmd(self) -> str:
        init_index = self.next_snapshot_init_index()
        plugin_args = [
            "mode=warm",
            f"init_threshold={self.sampling_interval}",
            f"interval={self.sampling_interval}",
            f"count={self.sample_size}",
            "prefix=snapshot",
            f"init_index={init_index}",
        ]
        if self.collect_gem5_bbl_btb:
            plugin_args.append("collect_gem5_bbl_btb=1")
        plugin_arg_string = ",".join(plugin_args)
        fw_cmd = f"""
            ./qemu-system-aarch64 \
            {self.qemu_common_parser.get_qemu_base_args()} \
            {self.qemu_common_parser.quantum_args()} \
            -plugin ../lib/libworm_cache.so,{plugin_arg_string} \
        """
        print(f"Functional warming will start emitting snapshots at snapshot_{init_index}.")
        print("fw command:")
        print(fw_cmd)
        return [
            f"cd {self.experiment_context.get_experiment_folder_address()}/run",
            fw_cmd,
            # TODO add the proper conditions to only create log and fp_gen_speed at the right time
            "rm -rf fp_gen_speed",
            "mkdir fp_gen_speed",
            "mv *.log ./fp_gen_speed",
        ]
