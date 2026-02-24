import os

import glob

from .executer import ParallelExecutor
from .config import ExperimentContext, clone_experiment_context
from .run_single_partition import RunSinglePartitionCommand


class RunPartitionCommand(ParallelExecutor):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 warming_ratio: int, 
                 measurement_ratio: int):
        self.experiment_context = experiment_context
        self.detailed_warming_ratio = warming_ratio
        self.measurement_ratio = measurement_ratio
        # glob all folders with partition_idx
        self.partition_folders = glob.glob(f"partition_*", root_dir=self.experiment_context.get_experiment_folder_address()+"/run")
        self.partition_folders.sort()
        if len(self.partition_folders) == 0:
            raise ValueError(f"No partition folders found in {self.experiment_context.get_experiment_folder_address()}/run. Expected folders with prefix 'partition_'.")
        self.idxs = [int(f.removeprefix(f"partition_")) for f in self.partition_folders]
        self.idxs.sort()
        for i in range(max(self.idxs)+1):
            if i not in self.idxs:
                raise ValueError(f"Missing partition folder for index {i}. Found partition folders for indices {self.idxs}.")
        print(f"Found partition folders for indices {self.idxs}.")
        
        executors = []
        for idx in self.idxs:
            new_experiment_context = clone_experiment_context(self.experiment_context, partition_number=idx)
            single_partition_cmd = RunSinglePartitionCommand(new_experiment_context, self.detailed_warming_ratio, self.measurement_ratio)
            executors.append(single_partition_cmd)

        print(f"Created RunSinglePartitionCommand executors for partition indices {self.idxs}.")
        
        super().__init__(executors)

        