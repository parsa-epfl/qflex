import os
from commands import Executor
from .config import ExperimentContext

class PartitionCommand(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 partition_count: int):
        self.experiment_context = experiment_context
        self.partition_count = partition_count
        # TODO potential problem that the potential script is created at the init_warm stage change later
        # For now just check the file exists and if not throw an error saying run init_warm first
        self.experiment_folder = self.experiment_context.get_experiment_folder_address()
        assert os.path.exists(f"{self.experiment_folder}/scripts/run_flexus.sh"), "Error: run_flexus.sh not found. Please run the init_warm command first to generate necessary scripts."

        # Check if partition.py exists in experiments folder and if not copy it from root folder
        if not os.path.exists(f"{self.experiment_folder}/partition.py"):
            raise FileNotFoundError(f"Error: partition.py not found in {self.experiment_folder}. Make sure init_warm command has been run.")

    def clean_partition_command(self):
        return f"""rm -rf {self.experiment_folder}/run/partition_* && \
        rm -f {self.experiment_folder}/mem/[0-9]*
        """

    def cmd(self) -> str:
        # TODO add run_flexus.sh to scripts folder
        # TODO get rid of partition at some point and move run_flexus.sh in our python commands
        # TODO seperate creating partitions and running partitions
        if os.path.exists(f"{self.experiment_folder}/run/partition_0"): 
            print("============== PARTITION ALREADY EXISTS ==============")
            print("Error: Partitions already exist. Please remove all existing partitions before creating new ones (and rerun fw).")
            print("If you want to remove previous partitions, you can run the following:")
            print(f"{self.clean_partition_command()}")
            print("If you'd rather just unpartition you can run:")
            print(f"mv {self.experiment_folder}/run/partition_*/snapshot_* {self.experiment_folder}/run")
            print(f"rm -rf {self.experiment_folder}/run/partition_*")
            raise FileExistsError("Partitions already exist.")
        return [
            f"cd {self.experiment_folder}",
            f"{self.experiment_folder}/partition.py {self.partition_count}",
        ]


class CleanPartitionCommand(PartitionCommand):

    def __init__(self, experiment_context: ExperimentContext):
        self.experiment_context = experiment_context
        self.experiment_folder = self.experiment_context.get_experiment_folder_address()

    def cmd(self):
        return [
            f"echo running '{self.clean_partition_command()}'",
            self.clean_partition_command(),
            "echo removed partitions successfully",
        ]

