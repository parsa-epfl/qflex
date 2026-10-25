from .executer import SequentialGroupExecutor, SimulationCommand
from .config import ExperimentContext, clone_experiment_context
from .partition import PartitionCommand
from .run_idx import RunIdxCommand


class FullTimingIdx(RunIdxCommand):
    """The run-idx recipe on the init_warmed base snapshot with the full-timing cycle budget."""

    def snapshot_name(self) -> str:
        return self.experiment_context.loadvm_name or "init_warmed"

    def total_cycles(self) -> int:
        return self.experiment_context.full_timing_total_cycles()


class FullTimingCommand(SequentialGroupExecutor, SimulationCommand):
    """One continuous detailed run per node: `partition 1` prepares run/partition_0 (links + run_partitions.sh,
    refuses if partitions exist), then one FullTimingIdx leaf writes run/partition_0/result_0 for `result`."""
    NEEDS_PDES_PEER_KILL = False

    def __init__(self, experiment_context: ExperimentContext, use_stdio: bool = False):
        super().__init__([])
        self.experiment_context = experiment_context
        self.use_stdio = use_stdio
        self._assert_syncs_true()

    def _build_children(self):
        exp = self.experiment_context
        return [
            PartitionCommand(clone_experiment_context(exp, partition_count=1)),
            FullTimingIdx(clone_experiment_context(exp, partition_number=0, idx=0), use_stdio=self.use_stdio),
        ]

    def cmd(self) -> str:
        raise NotImplementedError("FullTimingCommand dispatches its children; cmd() is unused.")
