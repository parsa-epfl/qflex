import os
from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser


class Multi(Executor):

    def __init__(self, 
                 experiment_context: ExperimentContext,):
        self.experiment_context = experiment_context
        self.qemu_common_parser = QemuCommonArgParser(
            experiment_context,
        )

    def cmd(self) -> str:
        
        boot_cmd = f"""
        gdb --args ./qemu-system-aarch64 \
        {self.qemu_common_parser.get_qemu_base_args()} \
        -icount shift=0,align=off,sleep=off
        """
        # -icount shift=0,align=off,sleep=off

        # {self.qemu_common_parser.quantum_args()} 

        print(f"{boot_cmd}")


        return [    
            f"cd {self.experiment_context.get_experiment_folder_address()}/run",
            boot_cmd
        ]