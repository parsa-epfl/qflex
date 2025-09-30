from .parameterloader import ParameterLoader
from ..config import ExperimentContext

class WormConfigLoader(ParameterLoader):
    def __init__(self,
                    experiment_context: ExperimentContext):
        super().__init__(
            experiment_context=experiment_context,
            parameter_template='parameter.rs.j2',
            output_name='parameter.rs',
            out_folder='cfg'
        )

    def get_context(self):
        return {
            "L2_SET" : self.experiment_context.simulation_context.l2_set,
            "L2_WAY" : self.experiment_context.simulation_context.l2_way,
            "DIRECTORY_SET" : self.experiment_context.simulation_context.directory_set,
            "DIRECTORY_WAY": self.experiment_context.simulation_context.directory_way,
            "DOUBLED_VCPU": self.experiment_context.simulation_context.doubled_vcpu,
            "CORE_COUNT": self.experiment_context.simulation_context.core_count,
        }