import os
from commands import Executor
from .config import ExperimentContext

class RunPartitionCommand(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 warming_ratio: int, 
                 measurement_ratio: int):
        self.experiment_context = experiment_context
        self.detailed_warming_ratio = warming_ratio
        self.measurement_ratio = measurement_ratio
        self.experiment_folder = self.experiment_context.get_experiment_folder_address()
        assert os.path.exists(f"{self.experiment_folder}/scripts/run_flexus.sh"), "Error: run_flexus.sh not found. Please run the init_warm command first to generate necessary scripts."
        assert os.path.exists(f"{self.experiment_folder}/run_partitions.sh"), "Error: run_partitions.sh not found. Make sure partition command has been run."


    def cmd(self) -> str:
        # TODO get rid of partition at some point and move run_flexus.sh in our python commands
        return [
            f"cd {self.experiment_folder}",
            f"{self.experiment_folder}/run_partitions.sh {self.detailed_warming_ratio} {self.measurement_ratio}"
        ]