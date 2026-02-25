import os
from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser
# TODO IMPORTANT, remove vanilla option

class Boot(Executor):

    def __init__(self, 
                 experiment_context: ExperimentContext,
                 vanilla: bool = False):
        self.experiment_context = experiment_context
        self.qemu_common_parser = QemuCommonArgParser(experiment_context)
        self.vanilla = vanilla

    def cmd(self) -> str:
        
        if not self.vanilla:
            boot_cmd = f"""
            gdb -ex run --args ./qemu-system-aarch64 \
            {self.qemu_common_parser.get_qemu_base_args()}
            """
        else:
            raise NotImplementedError("Booting with vanilla QEMU is not implemented yet. This was only meant to be used for loading with vanilla QEMU for functional warming preparation, but we can implement it if needed.")

        print(f"Boot command:")
        print(f"{boot_cmd}")

        return [
            f"cd {self.experiment_context.get_experiment_folder_address()}/run",
            boot_cmd
        ]