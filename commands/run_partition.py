import glob

from .executer import Executor
from .config import ExperimentContext, clone_experiment_context
from .run_single_partition import RunSinglePartitionCommand


class RunPartitionCommand(Executor):
    """Per-partition parallelism expressed via sub_experiments + the base Executor's
    multi-experiment dispatch (which uses mp.Process). At the node level the sub_experiments
    aren't set on the YAML context — they're generated here from the on-disk partition_*
    folders. At the partition leaf, this delegates to RunSinglePartitionCommand which
    handles the per-idx sequential execution.

    Tree shape end-to-end:
        top group ctx (from YAML)        ← multi-node sub_experiments=[node_0, node_1]
          └─ node ctx (per-node)         ← we generate sub_experiments=[part_0..N]
              └─ partition ctx (leaf)    ← RunSinglePartitionCommand runs idxs sequentially
                  └─ idx ctx             ← RunIdxCommand (the actual leaf bash)
    """

    def __init__(self,
                 experiment_context: ExperimentContext,
                 warming_ratio: int,
                 measurement_ratio: int):
        self.experiment_context = experiment_context
        self.detailed_warming_ratio = warming_ratio
        self.measurement_ratio = measurement_ratio
        self.use_stdio = False

    def cmd(self) -> str:
        raise NotImplementedError(
            "RunPartitionCommand dispatches via sub_experiments; cmd() is unused. "
            "See execute()."
        )

    def execute(self, to_stdio: bool = False, run_in_background: bool = False,
                dry_run: bool = False, *,
                sentinel_dir: str = None, log_path: str = None, err_path: str = None) -> bool:
        exp = self.experiment_context

        # Already a multi-node group from the YAML — let the base dispatch fan out.
        if exp.has_sub_experiments():
            return self._execute_group(to_stdio=to_stdio,
                                       run_in_background=run_in_background,
                                       dry_run=dry_run,
                                       outer_sentinel_dir=sentinel_dir)

        # Node-level context (no partition set yet): dynamically generate per-partition
        # sub_experiments from the on-disk partition_* folders, then dispatch.
        if exp.partition_number < 0:
            partition_idxs = self._discover_partitions(exp)
            sub_experiments = [
                clone_experiment_context(exp, partition_number=p) for p in partition_idxs
            ]
            # Build a temporary context with sub_experiments populated, then dispatch.
            # Use model_copy so we don't mutate the input.
            exp_with_subs = exp.model_copy(update={"sub_experiments": sub_experiments})
            original = self.experiment_context
            self.experiment_context = exp_with_subs
            try:
                return self._execute_group(to_stdio=to_stdio,
                                           run_in_background=run_in_background,
                                           dry_run=dry_run,
                                           outer_sentinel_dir=sentinel_dir)
            finally:
                self.experiment_context = original

        # Partition-level leaf context: run RunSinglePartitionCommand inline. Its own
        # children (RunIdxCommand instances) carry the per-(partition,idx) sentinel
        # coordination via the sentinel_dir we forward.
        inner = RunSinglePartitionCommand(
            exp,
            self.detailed_warming_ratio,
            self.measurement_ratio,
            use_stdio=self.use_stdio,
        )
        return inner.execute(to_stdio=to_stdio,
                             run_in_background=run_in_background,
                             dry_run=dry_run,
                             sentinel_dir=sentinel_dir,
                             log_path=log_path,
                             err_path=err_path)

    def _discover_partitions(self, exp: ExperimentContext) -> list[int]:
        run_folder = exp.get_experiment_folder_address() + "/run"
        partition_folders = sorted(glob.glob("partition_*", root_dir=run_folder))
        if len(partition_folders) == 0:
            raise ValueError(
                f"No partition folders found in {run_folder}. "
                "Expected folders with prefix 'partition_'."
            )
        idxs = sorted(int(f.removeprefix("partition_")) for f in partition_folders)
        for i in range(min(idxs), max(idxs) + 1):
            if i not in idxs:
                raise ValueError(f"Missing partition folder for index {i}. Found {idxs}.")
        return idxs
