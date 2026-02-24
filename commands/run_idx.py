import os
from commands import Executor
from .config import ExperimentContext
from commands.qemu import VanillaQemuArgParser   


class RunIdxCommand(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 warming_ratio: int, 
                 measurement_ratio: int,
                 idx: int):
        self.experiment_context = experiment_context
        self.detailed_warming_ratio = warming_ratio
        self.measurement_ratio = measurement_ratio
        # TODO turn this into a param, for now each ratio represents 100000 cycles
        self.total_cycles = (self.detailed_warming_ratio + self.measurement_ratio) * 100000 + 1
        self.vanilla_qemu_arg_parser = VanillaQemuArgParser(experiment_context, idx, self.total_cycles)



    def cmd(self) -> str:
        # TODO get rid of partition at some point and move run_flexus.sh in our python commands
        timing_command = f"""
            ../vanilla-qemu-system-aarch64 \
            {self.vanilla_qemu_arg_parser.get_qemu_base_args()} \
        """
        return [
            f"cd {self.experiment_context.get_partition_folder()}",
            timing_command
        ]