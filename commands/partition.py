import os
from commands import Executor
from .config import ExperimentContext

class PartitionCommand(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext):
        self.experiment_context = experiment_context

    @property
    def partition_count(self) -> int:
        return self.experiment_context.partition_count

    @property
    def experiment_folder(self) -> str:
        return self.experiment_context.get_experiment_folder_address()

    def clean_partition_command(self):
        return f"""rm -rf {self.experiment_folder}/run/partition_* && \
        rm -f {self.experiment_folder}/mem/[0-9]*
        """

    def cmd(self) -> str:
        # Preconditions checked at run time (not __init__) so the executor can be
        # constructed for a group context whose leaf folders aren't materialised yet.
        # TODO potential problem that the potential script is created at the init_warm stage change later
        assert os.path.exists(f"{self.experiment_folder}/scripts/run_flexus.sh"), "Error: run_flexus.sh not found. Please run the init_warm command first to generate necessary scripts."
        if not os.path.exists(f"{self.experiment_folder}/partition.py"):
            raise FileNotFoundError(f"Error: partition.py not found in {self.experiment_folder}. Make sure init_warm command has been run.")

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
        # A multi-fidelity phantom node has plain qemu checkpoints only (no worm .uarch / .mem /
        # Flexus configs), so tell partition.py to skip those. The uniform phantom node keeps the
        # worm plugin (warm-zero), so it has the normal checkpoint layout — partition it like the
        # master, no --phantom.
        exp = self.experiment_context
        phantom = " --phantom" if (exp.all_phantom_cores and exp.multi_modal) else ""
        return [
            f"cd {self.experiment_folder}",
            f"{self.experiment_folder}/partition.py {self.partition_count}{phantom}",
        ]


class CleanPartitionCommand(PartitionCommand):

    def __init__(self, experiment_context: ExperimentContext):
        self.experiment_context = experiment_context

    def cmd(self):
        return [
            f"echo running '{self.clean_partition_command()}'",
            self.clean_partition_command(),
            "echo removed partitions successfully",
        ]


class UnPartitionCommand(PartitionCommand):

    def __init__(self, experiment_context: ExperimentContext):
        self.experiment_context = experiment_context

    def cmd(self):
        return [
            f"echo running 'mv {self.experiment_folder}/run/partition_*/snapshot_* {self.experiment_folder}/run'",
            f"mv {self.experiment_folder}/run/partition_*/snapshot_* {self.experiment_folder}/run",
            f"echo running 'rm -rf {self.experiment_folder}/run/partition_*'",
            f"rm -rf {self.experiment_folder}/run/partition_*",
            "echo unpartitioned successfully",
        ]
