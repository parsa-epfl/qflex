from .parameterloader import ParameterLoader
from ..config import ExperimentContext

class TimingLoader(ParameterLoader):
    def __init__(self,
                    experiment_context: ExperimentContext):
            super().__init__(
                experiment_context=experiment_context,
                parameter_template='timing.cfg.j2',
                output_name='timing.cfg',
                out_folder='cfg'
            )
    def get_context(self):
        return {
            "L2_SET" : self.experiment_context.simulation_context.l2_set,
            "L2_WAY" : self.experiment_context.simulation_context.l2_way,
            "DIRECTORY_SET" : self.experiment_context.simulation_context.directory_set,
            "DIRECTORY_WAY": self.experiment_context.simulation_context.directory_way,
            "MEM_CONTROLLER_COUNT": self.experiment_context.simulation_context.mem_controller_count,
            "MEM_CONTROLLER_POSITIONS" : self.experiment_context.simulation_context.mem_controller_positions,
            # Flexus's EstimatedIPC is uint32_t and its lexical_cast rejects "4.0" — the set then
            # silently falls back to the wiring default (5). Render an int so the value lands.
            "PHANTOM_CPU_IPC": int(round(self.experiment_context.workload.IPC_info.phantom_cpu_ipc)),
        }