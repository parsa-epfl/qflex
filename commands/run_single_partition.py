import glob

from .executer import SequentialGroupExecutor, SimpleCMDExecutor, SimulationCommand, _is_dry_run
from .config import ExperimentContext, clone_experiment_context
from .run_idx import RunIdxCommand
from .run_progress import progress_channel, node_spec


# `SequentialGroupExecutor` first so its `__init__(children)` and `execute()`
# win MRO resolution; `SimulationCommand` adds the sync-asserts mixin. Both
# subclass `Executor`; Python's MRO collapses the diamond cleanly.
class RunSinglePartitionCommand(SequentialGroupExecutor, SimulationCommand):
    # vanilla-qemu's PDES exit handshake is clean; no peer-kill needed. See RunIdxCommand.
    NEEDS_PDES_PEER_KILL = False

    def __init__(self,
                 experiment_context: ExperimentContext,
                 use_stdio: bool = True,
                 owns_progress: bool = True):
        # Init lean — children are built lazily in _build_children() so this
        # executor can be constructed before its partition snapshots exist
        # (true for the group case in multi-experiment dispatch).
        super().__init__([])
        self.experiment_context = experiment_context
        self.use_stdio = use_stdio
        # False when delegated from RunPartitionCommand (it owns the channel); True for a direct run.
        self.owns_progress = owns_progress
        self._idx_total = 0
        self._idx_done = 0
        self._assert_syncs_true()

    def _on_child_completed(self, child):
        # Count only real idx completions (not the rm/sleep helpers) and report up the channel.
        if isinstance(child, RunIdxCommand):
            self._idx_done += 1
            self._emit_progress(self._idx_done, self._idx_total)

    def _build_children(self):
        partition_folder = self.experiment_context.get_partition_folder()
        snapshots = glob.glob("snapshot_*.loc", root_dir=partition_folder)
        idxs = sorted(int(f.removeprefix("snapshot_").removesuffix(".loc")) for f in snapshots)
        if len(idxs) == 0:
            return []
        self._idx_total = len(idxs)
        self._idx_done = 0
        self._emit_progress(0, self._idx_total)  # let the renderer learn this partition's total up front
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

    def execute(self, to_stdio=True, run_in_background=False, dry_run: bool = False, *,
                sentinel_dir: str = None, log_path: str = None, err_path: str = None,
                log_append: bool = False):
        run = lambda: super(RunSinglePartitionCommand, self).execute(
            to_stdio=to_stdio, run_in_background=run_in_background, dry_run=dry_run,
            sentinel_dir=sentinel_dir, log_path=log_path, err_path=err_path, log_append=log_append)
        # Direct `run-single-partition`: own the progress channel for this one experiment. When delegated
        # from RunPartitionCommand (owns_progress=False) the top already owns it and set our queue.
        p = self.experiment_context.partition_number
        own = (self.owns_progress and not _is_dry_run(dry_run)
               and not self.experiment_context.has_sub_experiments()
               and p is not None and p >= 0)
        if own:
            with progress_channel(self, [node_spec(self.experiment_context)]):
                return run()
        return run()

    def cmd(self) -> str:
        raise NotImplementedError("RunSinglePartitionCommand does not support cmd. Use execute instead.")
