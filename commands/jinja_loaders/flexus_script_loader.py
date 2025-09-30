from .parameterloader import ParameterLoader
from ..config import ExperimentContext

class FlexusScriptLoader(ParameterLoader):
    def __init__(self,
                    experiment_context: ExperimentContext):
            super().__init__(
                experiment_context=experiment_context,
                parameter_template='run_flexus.sh.j2',
                output_name='run_flexus.sh',
                out_folder='scripts'
            )
    def get_context(self):
        return {
            # TODO add some check to make sure this core_count is not the doubled version as that is being sent in as a separate parameter
            "CORE_COUNT": self.experiment_context.simulation_context.core_count,
            "DOUBLED_VCPU": self.experiment_context.simulation_context.doubled_vcpu,
            "MEMORY": self.experiment_context.simulation_context.memory,
            "QEMU_NIC": self.experiment_context.simulation_context.qemu_nic,
        }