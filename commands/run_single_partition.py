import os

import glob

from .executer import SequentialGroupExecutor, SimpleCMDExecutor
from .config import ExperimentContext, clone_experiment_context
from .run_idx import RunIdxCommand


class RunSinglePartitionCommand(SequentialGroupExecutor):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 warming_ratio: int,
                 measurement_ratio: int,
                 use_stdio: bool = True):
        self.experiment_context = experiment_context
        self.detailed_warming_ratio = warming_ratio
        self.measurement_ratio = measurement_ratio
        self.use_stdio = use_stdio
        self.snapshots = glob.glob("snapshot_*.loc", root_dir=self.experiment_context.get_partition_folder())
        self.idxs = [int(f.removeprefix("snapshot_").removesuffix(".loc")) for f in self.snapshots]
        self.idxs.sort()
        print("snapshot idx are " + str(self.idxs))
        for i in range(min(self.idxs), max(self.idxs)+1):
            if i not in self.idxs:
                raise ValueError(f"Missing snapshot for index {i} in partition {self.experiment_context.get_partition_folder()}. Found snapshots for indices {self.idxs}.")
            
        print(f"Found snapshots for indices {self.idxs} in partition {self.experiment_context.get_partition_folder()}.")

        setup_command = SimpleCMDExecutor("rm -rf output_state")
        children = [
            setup_command,
        ]
        for idx in self.idxs:
            cloned_experiment_context = clone_experiment_context(self.experiment_context, idx=idx)
            run_idx = RunIdxCommand(cloned_experiment_context, self.detailed_warming_ratio, self.measurement_ratio, use_stdio=self.use_stdio)
            children.append(run_idx)
        
        super().__init__(children)
    
    def cmd(self) -> str:
        raise NotImplementedError("RunSinglePartitionCommand does not support cmd. Use execute instead.")

        
        