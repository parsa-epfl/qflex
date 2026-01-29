import os
from .parameterloader import ParameterLoader
from ..config import ExperimentContext


class LoadExpectLoader(ParameterLoader):
    def __init__(self,
                 experiment_context: ExperimentContext,
                 trigger_phrase: str = "WARMUP COMPLETED",
                 monitor_port: int = 55555,
                 spawn_command: str = "",
                 save_vm_name: str = "vm-snapshot"):
        super().__init__(
            experiment_context=experiment_context,
            parameter_template='load.expect.j2',
            output_name='load.expect',
            out_folder='run'
        )
        self.trigger_phrase = trigger_phrase
        self.monitor_port = monitor_port
        self.spawn_command = spawn_command
        self.save_vm_name = save_vm_name
    
    def load_parameters(self) -> str:
        result = super().load_parameters()
        # Make the script executable
        os.system(f"chmod +x {result}")
        return result

    def get_context(self):
        return {
            "TRIGGER_PHASE": self.trigger_phrase,
            "MONITOR_PORT": self.monitor_port,
            "SPAWN_COMMAND": self.spawn_command,
            "SAVE_VM_NAME": self.save_vm_name,
        }
