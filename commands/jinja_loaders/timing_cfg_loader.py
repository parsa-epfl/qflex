from .parameterloader import ParameterLoader, get_microarchitecture_context
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
        return get_microarchitecture_context(self.experiment_context)
