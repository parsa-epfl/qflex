import glob
import os
import re
import sys
from commands import Executor
from .config import ExperimentContext
from .functional_warming import FW_SAMPLE_SIZE_FILE

# Measurement points are taken every INTERVAL uncore cycles (100 us), see templates/run_flexus.sh.j2.
INTERVAL = 100000


def find_incomplete_results(experiment_folder: str) -> list[str]:
    """
    Problems that make the sample smaller than functional warming's sample size: checkpoints that produced no
    result (a failed run_flexus.sh stops its partition), results that stop early, and checkpoints that were never
    created. Empty when every requested checkpoint was simulated to the end.
    """
    run = f"{experiment_folder}/run"
    checkpoints = {}  # index -> partition
    for meta in glob.glob(f"{run}/partition_*/snapshot_*.bxdb-meta"):
        if m := re.fullmatch(r"snapshot_(\d+)\.bxdb-meta", os.path.basename(meta)):
            checkpoints[int(m.group(1))] = os.path.basename(os.path.dirname(meta))
    last_points = {}  # index -> last regular measurement point
    for folder in glob.glob(f"{run}/partition_*/result_*"):
        index = int(os.path.basename(folder).removeprefix("result_"))
        points = [int(m.group(1)) for log in glob.glob(f"{folder}/all.measurement.*.log")
                  if (m := re.fullmatch(r"all\.measurement\.(\d+)\.log", os.path.basename(log))) and int(m.group(1)) % INTERVAL == 0]
        last_points[index] = max(points, default=0)

    def where(indices: list[int]) -> str:
        shown = ", ".join(f"{i} ({checkpoints.get(i, '?')})" for i in indices[:10])
        return shown + (f" and {len(indices) - 10} more" if len(indices) > 10 else "")

    problems = []
    if os.path.isfile(f"{run}/{FW_SAMPLE_SIZE_FILE}"):
        requested = int(open(f"{run}/{FW_SAMPLE_SIZE_FILE}").read())
        if len(checkpoints) != requested:
            problems.append(f"functional warming was asked for {requested} checkpoints, the partitions hold {len(checkpoints)} "
                            f"(see run/fp_gen_speed/ for the functional warming logs)")
    missing = sorted(set(checkpoints) - set(last_points))
    if missing:
        problems.append(f"{len(missing)} checkpoints have no result: {where(missing)}")
    expected = max(last_points.values(), default=0)
    short = sorted(i for i, last in last_points.items() if last < expected)
    if short:
        problems.append(f"{len(short)} results stop before cycle {expected}: {where(short)}")
    return problems

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
                 plot: bool = True,
                 allow_incomplete: bool = False):
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
        assert os.path.exists(f"{self.experiment_folder}/run/partition_0"), "Error: no partitions found. Make sure the partition and run-partition commands have been run."
        problems = find_incomplete_results(self.experiment_folder)
        if problems:
            message = ("the sample is incomplete:\n" + "\n".join(f"  - {p}" for p in problems)
                       + f"\nCheck run/partition_<i>/log and err of the partitions listed above, in {self.experiment_folder}/run.")
            if not allow_incomplete:
                raise RuntimeError(f"Error: {message}\nFix the failures and run them again, or pass --allow-incomplete to analyze the results anyway.")
            print(f"Warning: {message}", file=sys.stderr)

    def cmd(self) -> str | list[str]:
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

    def cmd(self) -> str | list[str]:
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
