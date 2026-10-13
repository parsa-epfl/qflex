from .parameterloader import ParameterLoader, get_microarchitecture_context
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
        return get_microarchitecture_context(self.experiment_context)
