import glob
import multiprocessing as mp
import os
import shutil

from .executer import (
    KILLED_BY_PEER_SUFFIX,
    SimulationCommand,
    _is_dry_run,
    _multi_experiment_target,
)
from .config import ExperimentContext, clone_experiment_context
from .run_single_partition import RunSinglePartitionCommand


class RunPartitionCommand(SimulationCommand):
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
            exp_with_subs = exp.model_copy(update={"sub_experiments": sub_experiments})
            return self._execute_partitions(
                exp_with_subs, sentinel_dir, to_stdio=to_stdio, dry_run=dry_run,
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

    def _execute_partitions(self, exp_with_subs: ExperimentContext,
                            outer_sentinel_dir: str,
                            *, to_stdio: bool, dry_run: bool) -> bool:
        """Same shape as Executor._execute_group, but routes each partition's
        log/err to its own `<run>/partition_<P>/run-partition.{log,err}` via
        `get_partition_folder()` — otherwise every partition on this node would
        race-write to the same `<exp>/RunPartitionCommand.log`.

        The cross-node handshake (node 1 waits on node 0's started sentinel
        before getting here) lives in `execute()` above, not in this method.
        """
        if outer_sentinel_dir is None:
            sentinel_dir = f"{exp_with_subs.get_experiment_folder_address()}/.sentinels"
            sentinel_dir_owner = True
        else:
            sentinel_dir = outer_sentinel_dir
            sentinel_dir_owner = False

        def per_child_kwargs(sub: ExperimentContext) -> dict:
            partition_folder = sub.get_partition_folder()
            return dict(
                to_stdio=False,
                run_in_background=False,
                sentinel_dir=sentinel_dir,
                log_path=f"{partition_folder}/run-partition.log",
                err_path=f"{partition_folder}/run-partition.err",
            )

        if _is_dry_run(dry_run):
            for sub in exp_with_subs.sub_experiments:
                original = self.experiment_context
                self.experiment_context = sub
                try:
                    self.execute(**per_child_kwargs(sub), dry_run=True)
                finally:
                    self.experiment_context = original
            return True

        if sentinel_dir_owner:
            shutil.rmtree(sentinel_dir, ignore_errors=True)
            os.makedirs(sentinel_dir, exist_ok=True)

        procs = []
        for sub in exp_with_subs.sub_experiments:
            kw = per_child_kwargs(sub)
            p = mp.Process(target=_multi_experiment_target, args=(self, sub, kw))
            procs.append((sub, p))
            p.start()

        # See Executor._execute_group's matching comment for why this is two
        # passes (join all → then check markers): the killed_by_peer marker
        # written by one child for another is only written after the writer's
        # own _post_exit_grace, so single-loop join+check would race.
        for _, p in procs:
            p.join()

        failures = []
        for sub, p in procs:
            if p.exitcode == 0:
                continue
            killed_marker = (
                f"{sentinel_dir}/"
                f"{self._sentinel_basename(sub.node_number)}.{KILLED_BY_PEER_SUFFIX}"
            )
            if os.path.exists(killed_marker):
                continue
            failures.append((sub, p.exitcode))

        if failures:
            names = ", ".join(
                f"part_{s.partition_number}(exit={c})" for s, c in failures
            )
            raise RuntimeError(
                f"{self.__class__.__name__}: partitions failed: {names}"
            )
        return True

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
