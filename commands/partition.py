import os
from commands import Executor
from .config import ExperimentContext

class PartitionCommand(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 partition_count: int,
                 warming_ratio: int, 
                 measurement_ratio: int):
        self.experiment_context = experiment_context
        self.partition_count = partition_count
        self.detailed_warming_ratio = warming_ratio
        self.measurement_ratio = measurement_ratio
        # TODO potential problem that the potential script is created at the init_warm stage change later
        # For now just check the file exists and if not throw an error saying run init_warm first
        self.experiment_folder = self.experiment_context.get_experiment_folder_address()
        assert os.path.exists(f"{self.experiment_folder}/scripts/run_flexus.sh"), "Error: run_flexus.sh not found. Please run the init_warm command first to generate necessary scripts."

        # Check if partition.py exists in experiments folder and if not copy it from root folder
        if not os.path.exists(f"{self.experiment_folder}/partition.py"):
            raise FileNotFoundError(f"Error: partition.py not found in {self.experiment_folder}. Make sure init_warm command has been run.")


    def cmd(self) -> str:
        # TODO add run_flexus.sh to scripts folder
        # TODO get rid of partition at some point and move run_flexus.sh in our python commands
        return [
            f"cd {self.experiment_folder}",
            f"{self.experiment_folder}/partition.py {self.partition_count}",
            # f"{self.experiment_folder}/scripts/run_partitions.sh {self.detailed_warming_ratio} {self.measurement_ratio}"
        ]