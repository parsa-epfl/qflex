import os
from commands import Executor
from .config import ExperimentContext
from commands.qemu import VanillaQemuArgParser   


class RunIdxCommand(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 warming_ratio: int, 
                 measurement_ratio: int):
        idx = experiment_context.idx
        self.experiment_context = experiment_context
        self.detailed_warming_ratio = warming_ratio
        self.measurement_ratio = measurement_ratio
        # TODO turn this into a param, for now each ratio represents 100000 cycles
        ratio_coefficient = 100000
        self.total_cycles = ((self.detailed_warming_ratio * ratio_coefficient) + (self.measurement_ratio * ratio_coefficient))  + 1
        self.vanilla_qemu_arg_parser = VanillaQemuArgParser(experiment_context, idx, self.total_cycles)



    def cmd(self) -> str:
        # TODO get rid of partition at some point and move run_flexus.sh in our python commands
        idx = self.experiment_context.idx
        partition_folder = self.experiment_context.get_partition_folder()
        setup_commands = [
            f"cd {partition_folder}",
            f'rm -rf "snapshot_{idx}-flexus"',
            f"mkdir snapshot_{idx}-flexus",
            f"./checkpoint_conversion ./snapshot_{idx}.uarch ../../cfg/flexus_configuration.json ./snapshot_{idx}-flexus true",
            "rm -rf output_state",
        ]
        timing_command = f"""
            gdb -ex run --args ../vanilla-qemu-system-aarch64 \
            {self.vanilla_qemu_arg_parser.get_qemu_base_args()} \
        """
        return setup_commands + [
            f"cd {self.experiment_context.get_partition_folder()}",
            timing_command
        ]