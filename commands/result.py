import os
import sys
from commands import Executor
from .config import ExperimentContext

def backup_and_replace_core_info(new_core_info: str) -> list[str]:
    """
    Shell steps that back up cfg/core_info.csv with a timestamp and replace it with new_core_info.
    run/core_info.csv is a symlink to cfg/core_info.csv, so it picks up the new content directly.
    """
    return [
        f"test -f {new_core_info}",
        'backup="./cfg/core_info.csv.$(date +%Y%m%d_%H%M%S)"',
        'cp ./cfg/core_info.csv "$backup"',
        'echo "backed up cfg/core_info.csv to $backup"',
        f"cp {new_core_info} ./cfg/core_info.csv",
        "ln -sfn ../cfg/core_info.csv ./run/core_info.csv",
        f"echo replaced cfg/core_info.csv with {new_core_info}",
        "cat ./cfg/core_info.csv",
    ]


class RunResultCommand(Executor):
    """
    Collects the detailed simulation results into timing.csv and analyzes the sampling error.
    Writes NEXT_SAMPLE_SIZE and exits with 255 if the sampling error bound is not met.
    Does not modify core_info.csv.
    """

    def __init__(self,
                 experiment_context: ExperimentContext,
                 unit_size: int = 1,
                 index: int = 2,
                 core_groups: str = "",
                 confidence: float = 95.0,
                 error: float = 0.05,
                 plot: bool = True):
        self.experiment_context = experiment_context
        self.experiment_folder = self.experiment_context.get_experiment_folder_address()
        self.unit_size = unit_size
        self.index = index
        self.core_groups = core_groups
        self.confidence = confidence
        self.error = error
        self.plot = plot
        assert os.path.exists(f"{self.experiment_folder}/result.py"), "Error: result.py not found. Make sure initialization command has been run."
        assert os.path.exists(f"{self.experiment_folder}/collect.py"), "Error: collect.py not found. Make sure initialization command has been run."
        assert os.path.exists(f"{self.experiment_folder}/run_partitions.sh"), "Error: run_partitions.sh not found. Make sure partition command has been run."

    def cmd(self) -> str:
        core_groups_arg = f'--core-groups "{self.core_groups}"' if self.core_groups else ""
        return [
            f"cd {self.experiment_folder}",
            # collect.py walks the current folder for result_* directories, so run it in ./run
            "cd ./run",
            f"{sys.executable} ../collect.py",
            "mv ./timing.csv ../timing.csv",
            "cd ..",
            "echo collected detailed simulation results into timing.csv",
            f"{sys.executable} ./result.py --timing-csv ./timing.csv --unit-size {self.unit_size} --index {self.index} "
            f"{core_groups_arg} --confidence {self.confidence} --error {self.error} "
            f"{'--plot' if self.plot else '--no-plot'}",
        ]


class TrainCommand(Executor):
    """
    Trains the IPC model from timing.csv (equivalent of train.sh):
      1. per-ASID models -> asid_info.csv, copied to run/asid_info.csv (skipped with a warning
         when there is not enough data)
      2. per-core-group IPC model -> core_info_ipc_model_new.csv, which replaces cfg/core_info.csv
    """

    def __init__(self,
                 experiment_context: ExperimentContext,
                 core_groups: str,
                 interval_index: int = 2,
                 cycle_range: str = "200000-300000"):
        self.experiment_context = experiment_context
        self.experiment_folder = self.experiment_context.get_experiment_folder_address()
        self.core_groups = core_groups
        self.interval_index = interval_index
        self.cycle_range = cycle_range
        assert self.core_groups, "Error: core groups must be set, e.g. \"0-7,8-15\"."
        for script in ["training_model.py", "training_by_asid.py"]:
            assert os.path.exists(f"{self.experiment_folder}/{script}"), f"Error: {script} not found. Make sure initialization command has been run."
        assert os.path.exists(f"{self.experiment_folder}/timing.csv"), "Error: timing.csv not found. Make sure result command has been run."
        assert os.path.exists(f"{self.experiment_folder}/cfg/core_info.csv"), "Error: cfg/core_info.csv not found. Make sure initialization command has been run."

    def cmd(self) -> str:
        return [
            f"cd {self.experiment_folder}",
            f"echo training per-ASID models with cycle range {self.cycle_range}",
            f"{{ {{ {sys.executable} ./training_by_asid.py ./timing.csv --cycle-range {self.cycle_range} "
            "&& cp ./asid_info.csv ./run/asid_info.csv "
            "&& echo copied asid_info.csv to run/asid_info.csv; } "
            "|| echo 'WARNING: per-ASID training failed, likely insufficient training data. Keeping the current run/asid_info.csv.'; }",
            f"echo training IPC model with interval index {self.interval_index} and core groups {self.core_groups}",
            "rm -f ./core_info_ipc_model_new.csv",
            f'{sys.executable} ./training_model.py ./timing.csv {self.interval_index} -c ./cfg/core_info.csv -g "{self.core_groups}"',
        ] + backup_and_replace_core_info("./core_info_ipc_model_new.csv") + [
            "echo ready for another round of fw and partitioning",
        ]
