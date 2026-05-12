import glob

from .executer import SequentialGroupExecutor, SimpleCMDExecutor, SimulationCommand
from .config import ExperimentContext, clone_experiment_context
from .run_idx import RunIdxCommand


# `SequentialGroupExecutor` first so its `__init__(children)` and `execute()`
# win MRO resolution; `SimulationCommand` adds the sync-asserts mixin. Both
# subclass `Executor`; Python's MRO collapses the diamond cleanly.
class RunSinglePartitionCommand(SequentialGroupExecutor, SimulationCommand):
    # vanilla-qemu's PDES exit handshake is clean; no peer-kill needed. See RunIdxCommand.
    NEEDS_PDES_PEER_KILL = False

    def __init__(self,
                 experiment_context: ExperimentContext,
                 use_stdio: bool = True):
        # Init lean — children are built lazily in _build_children() so this
        # executor can be constructed before its partition snapshots exist
        # (true for the group case in multi-experiment dispatch).
        super().__init__([])
        self.experiment_context = experiment_context
        self.use_stdio = use_stdio
        self._assert_syncs_true()

    def _build_children(self):
        partition_folder = self.experiment_context.get_partition_folder()
        snapshots = glob.glob("snapshot_*.loc", root_dir=partition_folder)
        idxs = sorted(int(f.removeprefix("snapshot_").removesuffix(".loc")) for f in snapshots)
        if len(idxs) == 0:
            return []
        for i in range(min(idxs), max(idxs) + 1):
            if i not in idxs:
                raise ValueError(
                    f"Missing snapshot for index {i} in partition "
                    f"{partition_folder}. Found {idxs}."
                )

        # Wipe the per-partition log/err once before the first idx runs. Each
        # RunIdxCommand below appends (>>/2>>) so all idxs accumulate into
        # one shared log/err with per-idx banners (see run_idx.py). Without
        # this wipe, leftover content from a prior partition run would mix
        # with the new run's output.
        children = [
            SimpleCMDExecutor("rm -rf output_state"),
            SimpleCMDExecutor(
                f': > "{partition_folder}/log" && : > "{partition_folder}/err"'
            ),
        ]
        for idx in idxs:
            sub_ctx = clone_experiment_context(self.experiment_context, idx=idx)
            children.append(
                RunIdxCommand(
                    sub_ctx,
                    use_stdio=self.use_stdio,
                )
            )
            # 5-second sleep between indexes to give the system time to recover. TODO fix this
            children.append(SimpleCMDExecutor("sleep 5"))
        return children

    def cmd(self) -> str:
        raise NotImplementedError("RunSinglePartitionCommand does not support cmd. Use execute instead.")
