import math

from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser   

class FunctionalWarming(Executor):
    """
    This class handles the functional warming phase of the experiment and generates checkpoints.
    """

    def __init__(self,
                 experiment_context: ExperimentContext,):
        self.experiment_context = experiment_context
        self.simulation_context = self.experiment_context.simulation_context
        self.qemu_common_parser = QemuCommonArgParser(experiment_context)
        self.sample_size = self.experiment_context.workload.sample_size
        self.sampling_interval = math.ceil(
            (self.experiment_context.workload.population + self.sample_size - 1) / self.sample_size
        )



    

    def cmd(self) -> str:
        fw_cmd = f"""
            ./qemu-system-aarch64 \
            {self.qemu_common_parser.get_qemu_base_args()} \
            {self.qemu_common_parser.quantum_args()} \
            -plugin ../lib/libworm_cache.so,mode=warm,init_threshold={self.sampling_interval},interval={self.sampling_interval},count={self.sample_size} \
        """
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
