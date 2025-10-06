import os
from commands import Executor
from .config import get_experiment_folder_address

class PartitionCommand(Executor):

    def __init__(self,
                 experiment_name: str,
                 working_directory: str,
                 partition_count: int,
                 warming_ratio: int, 
                 measurement_ratio: int):
        self.experiment_name = experiment_name
        self.working_directory = working_directory
        self.partition_count = partition_count
        self.detailed_warming_ratio = warming_ratio
        self.measurement_ratio = measurement_ratio
        # TODO potential problem that the potential script is created at the init_warm stage change later
        # For now just check the file exists and if not throw an error saying run init_warm first
        self.experiment_folder = get_experiment_folder_address(
            experiment_name=experiment_name,
            working_directory=working_directory
        )
        assert os.path.exists(f"{self.experiment_folder}/scripts/run_flexus.sh"), "Error: run_flexus.sh not found. Please run the init_warm command first to generate necessary scripts."

        # Check if partition.py exists in experiments folder and if not copy it from root folder
        if not os.path.exists(f"{self.experiment_folder}/partition.py"):
            assert os.path.exists("./partition.py"), "Error: partition.py not found in root folder."
            os.system(f"cp ./partition.py {self.experiment_folder}/")


    def cmd(self) -> str:
        # TODO add run_flexus.sh to scripts folder
        # TODO get rid of partition at some point and move run_flexus.sh in our python commands
        return [
            f"{self.experiment_folder}/partition.py {self.partition_count}",
            f"{self.experiment_folder}/scripts/run_flexus.sh {self.detailed_warming_ratio} {self.measurement_ratio}"
        ]