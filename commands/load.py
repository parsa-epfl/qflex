from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser   
# TODO IMPORTANT, remove vanilla option


class Load(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 vanilla: bool = False):
        self.experiment_context = experiment_context
        self.qemu_common_parser = QemuCommonArgParser(experiment_context)
        self.vanilla = vanilla

    def cmd(self) -> str:
        
        if not self.vanilla:
            perf = ""
            # if self.experiment_context.node_number == 1:
            #     perf = f"perf record -F 999 -g --call-graph dwarf -- "
            load_cmd = perf + f""" gdb -ex run --args ./qemu-system-aarch64 \
            {self.qemu_common_parser.get_qemu_base_args()} \
            {self.qemu_common_parser.quantum_args()}
            """
        else:
            load_cmd = f"""
            ./vanilla-qemu-system-aarch64 \
            {self.qemu_common_parser.get_qemu_base_args()} \
            -icount shift=0,align=off,sleep=off
            """
    
        # WormCacheQFlex/src/parameter.rss
        return [
            f"cd {self.experiment_context.get_experiment_folder_address()}/run", 
            load_cmd
        ]
