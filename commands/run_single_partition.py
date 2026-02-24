import os

import glob

from commands import Executor
from .config import ExperimentContext
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
        for i in range(max(self.idxs)+1):
            if i not in self.idxs:
                raise ValueError(f"Missing snapshot for index {i} in partition {self.experiment_context.get_partition_folder()}. Found snapshots for indices {self.idxs}.")
            
        print(f"Found snapshots for indices {self.idxs} in partition {self.experiment_context.get_partition_folder()}.")


    def cmd(self) -> str:
        commands = []
        for idx in self.idxs:
            run_idx = RunIdxCommand(self.experiment_context, self.detailed_warming_ratio, self.measurement_ratio, idx)
            commands += run_idx.cmd()
        print(f"Commands to run for partition {self.experiment_context.get_partition_folder()}:\n" + "\n".join(commands))
        return commands