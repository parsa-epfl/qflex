from .parameterloader import ParameterLoader
from ..config import ExperimentContext

class FlexusCheckpointConfigLoader(ParameterLoader):
    def __init__(self,
                    experiment_context: ExperimentContext):
            super().__init__(
                experiment_context=experiment_context,
                parameter_template='flexus_configuration.json.j2',
                output_name='flexus_configuration.json',
                out_folder='cfg'
            )
            self.experiment_context = experiment_context
            self.simulation_context = experiment_context.simulation_context
    def get_context(self):
        return {
            "L2_SET" : self.experiment_context.simulation_context.l2_set,
            "L2_WAY" : self.experiment_context.simulation_context.l2_way,
            "DIRECTORY_SET" : self.experiment_context.simulation_context.directory_set,
            "DIRECTORY_WAY": self.experiment_context.simulation_context.directory_way,
            "DOUBLED_VCPU": self.experiment_context.simulation_context.doubled_vcpu,
            "CORE_COUNT": self.experiment_context.simulation_context.core_count,
        }