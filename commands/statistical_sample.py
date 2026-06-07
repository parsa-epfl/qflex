import glob
import os
import shutil

from commands import Executor
from .executer import _is_dry_run
from .config import ExperimentContext
from .functional_warming import FunctionalWarming
from .partition import PartitionCommand, CleanPartitionCommand
from .run_partition import RunPartitionCommand
from .result import RunResultCommand, REQUIRED_SAMPLE_SIZE_FILE, NEXT_SAMPLE_SIZE_FILE


class StatisticalSampleCommand(Executor):
    """Automates fw -> partition -> run-partition -> result, growing the sample
    size until every node has enough sampling units. Pure automation: it runs the
    same commands a user would by hand, then reads disk state to decide whether to
    iterate. All file management (core_info_<size>.csv, the next size) lives in the
    phases themselves (get_ipns_csv, RunResultCommand) — never in the loop.
    Dispatches via execute() (each sub-phase fans out across nodes itself), so
    cmd() is unused — same pattern as RunPartitionCommand."""

    def __init__(self, experiment_context: ExperimentContext, max_iterations: int = 20):
        self.experiment_context = experiment_context
        self.max_iterations = max_iterations

    def cmd(self) -> str:
        raise NotImplementedError("StatisticalSampleCommand orchestrates other phases via execute().")

    def _leaves(self) -> list[ExperimentContext]:
        exp = self.get_experiment()
        return list(exp.sub_experiments) if exp.has_sub_experiments() else [exp]

    @staticmethod
    def _run_folder(leaf: ExperimentContext) -> str:
        return f"{leaf.get_experiment_folder_address()}/run"

    def _initial_sample_size(self) -> int:
        """The sample-unit size given to the experiment context via YAML/CLI. Read
        from the leaves (the contexts that carry the resolved per-experiment value);
        multi-node nodes must agree on it."""
        sizes = {leaf.sample_size for leaf in self._leaves()}
        if len(sizes) != 1:
            raise RuntimeError(f"sample_size differs across nodes: {sizes}; set a single value via YAML/CLI.")
        return sizes.pop()

    def _biggest_size(self) -> int:
        """Biggest core_info_<size>.csv on disk, or the initial sample_size if none
        exist yet. Multi-node: the latest size must match across all nodes, otherwise
        the run is desynced and we error out."""
        per_leaf_max = [max(l.sized_core_info_sizes(), default=None) for l in self._leaves()]
        if all(m is None for m in per_leaf_max):
            return self._initial_sample_size()
        if len(set(per_leaf_max)) != 1:
            raise RuntimeError(
                f"core_info_<size>.csv latest sampling-unit size differs across nodes: {per_leaf_max}. "
                "Delete the mismatched files so every node resumes from the same size."
            )
        return per_leaf_max[0]

    def _set_sample_size(self, size: int) -> None:
        exp = self.get_experiment()
        exp.sample_size = size
        for sub in exp.sub_experiments:
            sub.sample_size = size

    def _iteration_phases(self) -> list[Executor]:
        """The phases run each iteration, in order. Single source of truth for both
        execution and the per-iteration log snapshot (named by Executor._phase_name)."""
        exp = self.experiment_context
        return [
            FunctionalWarming(exp),
            PartitionCommand(exp),
            RunPartitionCommand(exp),
            RunResultCommand(exp),
        ]

    def _preserve_iteration_outputs(self, size: int) -> None:
        """Copy this iteration's logs/artifacts to *_<size> names before cleanup
        wipes the partition folders. Log names are derived from the phase commands,
        not hardcoded, so they can't drift from what the executor writes."""
        artifacts = [REQUIRED_SAMPLE_SIZE_FILE, NEXT_SAMPLE_SIZE_FILE]
        for phase in self._iteration_phases():
            artifacts += [f"{phase._phase_name()}.log", f"{phase._phase_name()}.err"]
        for leaf in self._leaves():
            folder = leaf.get_experiment_folder_address()
            for name in artifacts:
                src = f"{folder}/{name}"
                if os.path.exists(src):
                    base, ext = os.path.splitext(name)
                    shutil.copy(src, f"{folder}/{base}_{size}{ext}")
            for log in glob.glob(f"{self._run_folder(leaf)}/partition_*/*"):
                if os.path.basename(log) in ("log", "err"):
                    part = os.path.basename(os.path.dirname(log))
                    shutil.copy(log, f"{folder}/{part}_{size}.{os.path.basename(log)}")

    def execute(self,
                to_stdio: bool = True,
                run_in_background: bool = False,
                dry_run: bool = False,
                *,
                sentinel_dir: str = None,
                log_path: str = None,
                err_path: str = None,
                log_append: bool = False) -> bool:
        run = lambda phase: phase.execute(to_stdio=to_stdio, run_in_background=run_in_background, dry_run=dry_run)

        if _is_dry_run(dry_run):
            for phase in self._iteration_phases():
                run(phase)
            return True

        # Force the result phase to save core_info_<next> (off by default for a manual
        # standalone result; the loop is exactly the case where we DO want it).
        exp = self.get_experiment()
        for ctx in [exp, *exp.sub_experiments]:
            ctx.save_next_core_info = True

        current = self._biggest_size()
        print(f"[statistical-sample] starting/resuming at sample size {current}")

        for iteration in range(self.max_iterations):
            print(f"[statistical-sample] iteration {iteration + 1}: sample size {current}")
            self._set_sample_size(current)

            # Run the phases exactly as a user would by hand. result saves the next
            # size's core_info_<size>.csv when the run isn't yet sufficient; get_ipns_csv
            # (every phase startup) keeps core_info.csv pointed at the biggest size.
            for phase in self._iteration_phases():
                run(phase)

            self._preserve_iteration_outputs(current)

            biggest = self._biggest_size()
            if biggest <= current:
                print(f"[statistical-sample] converged: all nodes satisfied at sample size {current}")
                return True
            print(f"[statistical-sample] next sample size: {biggest}")

            run(CleanPartitionCommand(self.experiment_context))
            current = biggest

        print(f"[statistical-sample] hit max_iterations={self.max_iterations} without converging (last size {current}).")
        return False
