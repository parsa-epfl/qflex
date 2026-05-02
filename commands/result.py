import os
from commands import Executor
from .config import ExperimentContext

class RunResultCommand(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext):
        self.experiment_context = experiment_context

    def cmd(self) -> str:
        # Preconditions are checked at run time (not __init__) so the executor can
        # be constructed for a group context where leaf artifacts don't exist yet.
        experiment_folder = self.experiment_context.get_experiment_folder_address()
        assert os.path.exists(f"{experiment_folder}/result.py"), "Error: result.py not found. Make sure initialization command has been run."
        assert os.path.exists(f"{experiment_folder}/run_partitions.sh"), "Error: run_partitions.sh not found. Make sure partition command has been run."

        # TODO move all root old replica scripts to proper folders
        return [
            f"cd {experiment_folder}",
            f"python {experiment_folder}/result.py",
        ]
