import glob

from .executer import SimulationCommand, _is_dry_run
from .config import ExperimentContext, clone_experiment_context
from .run_single_partition import RunSinglePartitionCommand


class RunPartitionCommand(SimulationCommand):
    """Per-partition parallelism via sub_experiments + Executor._execute_group.
    Tree: top group ctx → node ctx (we generate per-partition subs) → partition ctx
    → RunSinglePartitionCommand → RunIdxCommand leaf."""

    # vanilla-qemu's PDES exit handshake is clean; no peer-kill needed here. See RunIdxCommand.
    NEEDS_PDES_PEER_KILL = False

    def __init__(self,
                 experiment_context: ExperimentContext):
        self.experiment_context = experiment_context
        self.use_stdio = False

    def cmd(self) -> str:
        raise NotImplementedError(
            "RunPartitionCommand dispatches via sub_experiments; cmd() is unused. "
            "See execute()."
        )

    def execute(self, to_stdio: bool = False, run_in_background: bool = False,
                dry_run: bool = False, *,
                sentinel_dir: str = None, log_path: str = None, err_path: str = None) -> bool:
        self._assert_syncs_true()
        exp = self.experiment_context

        # Already a multi-node group from the YAML — let the base dispatch fan out.
        if exp.has_sub_experiments():
            return self._execute_group(to_stdio=to_stdio,
                                       run_in_background=run_in_background,
                                       dry_run=dry_run,
                                       outer_sentinel_dir=sentinel_dir)

        # Node-level context (no partition set yet): dynamically generate per-partition
        # sub_experiments from the on-disk partition_* folders, then dispatch.
        #
        # Node-level handshake using the existing wait_for_nodes + sentinel
        # mechanism: before fanning out partitions on this node, wait for any
        # upstream node's RunPartitionCommand_node<N>.started sentinel, then
        # touch our own. Replaces the old `nohup ./total-run-partitions.sh`
        # workaround (which launched node 0's partitions, slept 60s, then
        # launched node 1's partitions). Honours whatever `wait_for_nodes`
        # the YAML put on this leaf — empty for master, [0] for follower.
        # Skipped in dry-run (the same way `_execute_leaf` skips them).
        if exp.partition_number < 0:
            if _is_dry_run(dry_run):
                self._print_node_handshake_dry_run(exp, sentinel_dir)
            else:
                self._wait_for_sentinels(sentinel_dir)
                self._touch_sentinel(sentinel_dir, "started")

            partition_idxs = self._discover_partitions(exp)
            sub_experiments = [
                clone_experiment_context(exp, partition_number=p) for p in partition_idxs
            ]
            self.experiment_context = exp.model_copy(update={"sub_experiments": sub_experiments})
            return self._execute_group(
                to_stdio=to_stdio, run_in_background=run_in_background, dry_run=dry_run,
                outer_sentinel_dir=sentinel_dir,
                per_child_kwargs_fn=self._partition_child_kwargs,
                sub_label_fn=lambda s: f"part_{s.partition_number}",
            )

        # Partition-level leaf context: run RunSinglePartitionCommand inline. Its own
        # children (RunIdxCommand instances) carry the per-(partition,idx) sentinel
        # coordination via the sentinel_dir we forward.
        inner = RunSinglePartitionCommand(
            exp,
            use_stdio=self.use_stdio,
        )
        return inner.execute(to_stdio=to_stdio,
                             run_in_background=run_in_background,
                             dry_run=dry_run,
                             sentinel_dir=sentinel_dir,
                             log_path=log_path,
                             err_path=err_path)

    def _print_node_handshake_dry_run(self, exp: ExperimentContext,
                                      sentinel_dir: str) -> None:
        """Mirror the executor's dry-run `[py-wait]` / `[py-touch]` markers for
        the node-level handshake we add in run-partition (so the dispatch tree
        in dry-run reflects what the real run will do)."""
        if sentinel_dir is None:
            return
        lines = [f"[dry-run] {self.__class__.__name__} (node {exp.node_number}) handshake:"]
        for n in exp.wait_for_nodes:
            lines.append(f"  [py-wait]  {self._sentinel_path(sentinel_dir, n, 'started')}")
        lines.append(
            f"  [py-touch] {self._sentinel_path(sentinel_dir, exp.node_number, 'started')}"
        )
        print("\n".join(lines))

    def _partition_child_kwargs(self, sub: ExperimentContext, sentinel_dir: str) -> dict:
        """Per-partition log path override — each partition writes to its own `partition_<P>/run-partition.{log,err}` rather than the shared phase log."""
        partition_folder = sub.get_partition_folder()
        return dict(
            to_stdio=False, run_in_background=False, sentinel_dir=sentinel_dir,
            log_path=f"{partition_folder}/run-partition.log",
            err_path=f"{partition_folder}/run-partition.err",
        )

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
