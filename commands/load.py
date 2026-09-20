from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser   


class Load(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext,):
        self.experiment_context = experiment_context
        self.qemu_common_parser = QemuCommonArgParser(experiment_context)

    def cmd(self) -> str:
        
        load_cmd = f"""
        ./qemu-system-aarch64 \
        {self.qemu_common_parser.get_qemu_base_args()} \
        {self.qemu_common_parser.quantum_args()}
        """
    
        # WormCache/src/parameter.rss
        return [
            f"cd {self.experiment_context.get_experiment_folder_address()}/run", 
            load_cmd
        ]
