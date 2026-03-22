import os

import glob

from commands import Executor
from .config import ExperimentContext, clone_experiment_context
from .run_idx import RunIdxCommand


class RunSinglePartitionCommand(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 warming_ratio: int,
                 measurement_ratio: int):
        self.experiment_context = experiment_context
        self.detailed_warming_ratio = warming_ratio
        self.measurement_ratio = measurement_ratio
        self.snapshots = glob.glob("snapshot_*.loc", root_dir=self.experiment_context.get_partition_folder())
        self.idxs = [int(f.removeprefix("snapshot_").removesuffix(".loc")) for f in self.snapshots]
        self.idxs.sort()
        print("snapshot idx are " + str(self.idxs))
        for i in range(min(self.idxs), max(self.idxs)+1):
            if i not in self.idxs:
                raise ValueError(f"Missing snapshot for index {i} in partition {self.experiment_context.get_partition_folder()}. Found snapshots for indices {self.idxs}.")
            
        print(f"Found snapshots for indices {self.idxs} in partition {self.experiment_context.get_partition_folder()}.")


    def cmd(self) -> str:
        commands = [
            "rm -rf output_state",
        ]
        for idx in self.idxs:
            cloned_experiment_context = clone_experiment_context(self.experiment_context, idx=idx)
            run_idx = RunIdxCommand(cloned_experiment_context, self.detailed_warming_ratio, self.measurement_ratio)
            commands += run_idx.cmd()
        print(f"Commands to run for partition {self.experiment_context.get_partition_folder()}:\n" + "\n".join(commands))
        return commands