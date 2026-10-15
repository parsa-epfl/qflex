import glob
import os
import re
import shutil
import sys
from commands import Executor
from .config import ExperimentContext

# Files of one checkpoint, as written by functional warming and moved into a partition by partition.py.
CHECKPOINT_FILE = re.compile(r"snapshot_\d+\.(bxdb-meta|state\.zstd|uarch)")


def running_partition_pids(experiment_folder: str) -> list[int]:
    """Processes working in a partition of the experiment (run_flexus.sh and its QEMU), i.e. a run in progress."""
    partitions = os.path.realpath(f"{experiment_folder}/run") + "/partition_"
    pids = []
    for proc in glob.glob("/proc/[0-9]*"):
        try:
            if os.readlink(f"{proc}/cwd").startswith(partitions):
                pids.append(int(os.path.basename(proc)))
        except OSError:  # the process ended, or belongs to another user
            pass
    return pids


def check_no_partition_running(experiment_folder: str):
    pids = running_partition_pids(experiment_folder)
    if pids:
        raise RuntimeError(f"Error: partitions of {experiment_folder} are still running (pids {' '.join(map(str, pids))}). Stop run-partition first.")


def unpartition(experiment_folder: str, force: bool):
    """Moves the checkpoints of every partition back to run/ and removes the partitions, so they can be partitioned again."""
    run = f"{experiment_folder}/run"
    partitions = sorted(glob.glob(f"{run}/partition_*"))
    if not partitions:
        print(f"No partitions in {run}.")
        return
    check_no_partition_running(experiment_folder)
    results = glob.glob(f"{run}/partition_*/result_*")
    if results and not force:
        raise RuntimeError(f"Error: the partitions hold {len(results)} simulation results (result_*), which unpartition deletes. "
                           "Run `./qflex result` first if you need them, then rerun with --force.")
    # Check every move before doing any, so a conflict leaves the partitions untouched.
    moves = [(path, f"{run}/{os.path.basename(path)}")
             for partition in partitions for path in sorted(glob.glob(f"{partition}/snapshot_*"))
             if CHECKPOINT_FILE.fullmatch(os.path.basename(path))]
    conflicts = [target for _, target in moves if os.path.lexists(target)]
    if conflicts:
        raise FileExistsError(f"Error: {len(conflicts)} checkpoint files already exist in {run}, e.g. {conflicts[0]}; nothing was moved.")
    for source, target in moves:
        os.rename(source, target)
    for partition in partitions:
        shutil.rmtree(partition)
    print(f"Moved {len(moves) // 3} checkpoints back to {run} and removed {len(partitions)} partitions.")

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
        # Besides the partitions and checkpoint files, reset the bxdb database for the next functional warming:
        # drop the B-tree index built by run_partitions.sh (with it, bxdb would open the database in B-tree
        # mode instead of appending to chunks.log), then purge every snapshot after the base one (id 0).
        run = f"{self.experiment_folder}/run"
        return f"""rm -rf {run}/partition_* {run}/snapshot_*.bxdb-meta {run}/snapshot_*.state.zstd {run}/snapshot_*.uarch && \
        rm -f {run}/*.bxdb/index.bxdb && \
        for db in {run}/*.bxdb; do if [ -f "$db/chunks.log" ]; then {self.experiment_folder}/bin/bxdb purge 0 "$db"; fi; done
        """

    def cmd(self) -> str | list[str]:
        # TODO add run_flexus.sh to scripts folder
        # TODO get rid of partition at some point and move run_flexus.sh in our python commands
        # TODO seperate creating partitions and running partitions
        if os.path.exists(f"{self.experiment_folder}/run/partition_0"): 
            print("============== PARTITION ALREADY EXISTS ==============")
            print("Error: Partitions already exist. Please remove all existing partitions before creating new ones (and rerun fw).")
            print("If you want to remove previous partitions, you can run the following:")
            print(f"{self.clean_partition_command()}")
            print("If you'd rather just unpartition (keep the checkpoints, e.g. to partition them differently), run:")
            print("xargs -a qflex.args -- ./qflex unpartition")
            raise FileExistsError("Partitions already exist.")
        return [
            f"cd {self.experiment_folder}",
            f"{sys.executable} {self.experiment_folder}/partition.py {self.partition_count}",
        ]


class CleanPartitionCommand(PartitionCommand):

    def __init__(self, experiment_context: ExperimentContext):
        self.experiment_context = experiment_context
        self.experiment_folder = self.experiment_context.get_experiment_folder_address()
        check_no_partition_running(self.experiment_folder)

    def cmd(self):
        return [
            f"echo running '{self.clean_partition_command()}'",
            self.clean_partition_command(),
            "echo removed partitions successfully",
        ]

