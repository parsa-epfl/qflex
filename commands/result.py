import os
from commands import Executor
from .config import ExperimentContext

class RunResultCommand(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext):
        self.experiment_context = experiment_context
        self.experiment_folder = self.experiment_context.get_experiment_folder_address()
        assert os.path.exists(f"{self.experiment_folder}/result.py"), "Error: result.py not found. Make sure initialization command has been run."
        assert os.path.exists(f"{self.experiment_folder}/run_partitions.sh"), "Error: run_partitions.sh not found. Make sure partition command has been run."

    def cmd(self) -> str:
        # TODO move all root old replica scripts to proper folders
        return [
            f"cd {self.experiment_folder}",
            f"python {self.experiment_folder}/result.py",
            "rm ./run/core_info.csv",
            "mv ./cfg/core_info.csv ./cfg/old_core_info.csv",
            "mv ./core_info_new.csv ./cfg/core_info.csv",
            "echo replaced old core_info.csv with new core_info.csv",
            
            
            # TODO check why this cd is needed
            "cd ./run",
            "ln -s ../cfg/core_info.csv core_info.csv",
            "echo linked new core_info.csv to run folder, ready for another round of fw and partitioning",
            "cat ./core_info.csv",
            "cd ..",


        ]