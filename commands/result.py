import math
import os
import shutil

from commands import Executor
from .executer import _is_dry_run
from .config import ExperimentContext

# Files result_new.py writes per leaf in the experiment folder. REQUIRED is the raw
# required sample size (convergence signal); NEXT is result_new's own next size.
REQUIRED_SAMPLE_SIZE_FILE = "REQUIRED_SAMPLE_SIZE"
NEXT_SAMPLE_SIZE_FILE = "NEXT_SAMPLE_SIZE"


class RunResultCommand(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext):
        self.experiment_context = experiment_context

    def should_generate_core_info(self) -> bool:
        exp = self.experiment_context
        return exp.save_next_core_info and (exp.has_sub_experiments() or not exp.is_multi_node())
        

    def cmd(self) -> str:
        # Fully-phantom node ran no detailed model, so it has no result_<idx>/measurement output
        # to aggregate — skip it (the result scripts would otherwise find nothing and error).
        exp = self.experiment_context
        if exp.all_phantom_cores:
            return [f'echo "[result] skipping fully-phantom node {exp.node_number} (no measurement output)"']
        # Preconditions are checked at run time (not __init__) so the executor can
        # be constructed for a group context where leaf artifacts don't exist yet.
        experiment_folder = self.experiment_context.get_experiment_folder_address()
        assert os.path.exists(f"{experiment_folder}/result.py"), "Error: result.py not found. Make sure initialization command has been run."
        assert os.path.exists(f"{experiment_folder}/run_partitions.sh"), "Error: run_partitions.sh not found. Make sure partition command has been run."

        # TODO move all root old replica scripts to proper folders
        freq_ghz = self.experiment_context.workload.IPC_info.machine_freq_ghz
        # Pass the measurement window explicitly so the reporters never fall back to their
        # defaults' silent warming=2/measurement=1 assumption.
        window_args = (f"--interval {exp.stat_interval_cycles} --index {exp.warming_ratio} "
                       f"--measure-units {exp.measurement_ratio}")
        # Always regenerate core_info_new.csv + REQUIRED_SAMPLE_SIZE; --no-exit-on-fail so
        # the cross-node save step below always runs (an unmet bound is not a failure).
        new_result_cmd = ""
        if self.should_generate_core_info():
            new_result_cmd = f"python {experiment_folder}/result_new.py --freq-ghz {freq_ghz} {window_args} --generate-core-info --no-exit-on-fail"
        else:
            new_result_cmd = f"python {experiment_folder}/result_new.py --freq-ghz {freq_ghz} {window_args}"
        return [
            f"cd {experiment_folder}",
            f"python {experiment_folder}/result.py --freq-ghz {freq_ghz} {window_args}",
            f"python {experiment_folder}/collect.py",
            new_result_cmd,
        ]

    def execute(self,
                to_stdio: bool = True,
                run_in_background: bool = False,
                dry_run: bool = False,
                *,
                sentinel_dir: str = None,
                log_path: str = None,
                err_path: str = None,
                log_append: bool = False) -> bool:
        ok = super().execute(to_stdio=to_stdio, run_in_background=run_in_background, dry_run=dry_run,
                             sentinel_dir=sentinel_dir, log_path=log_path, err_path=err_path, log_append=log_append)
        # Run the cross-node save once at the top (group) or for a true single-node run,
        # never inside a multi-node child leaf (which only sees its own node). Off by
        # default (save_next_core_info) so a standalone/rerun `result` doesn't write a new
        # sized file by accident; the statistical-sample loop forces it on.
        exp = self.experiment_context
        if not _is_dry_run(dry_run) and self.should_generate_core_info():
            self._save_next_core_info()
        return ok

    def _save_next_core_info(self) -> None:
        """If the run isn't yet statistically sufficient, save each node's refined IPC
        as core_info_<next>.csv, where next = max required across nodes rounded up to 50.
        This is the experiment-wide step, so it must be the parent's job — a single
        node's result script can't know the max across nodes."""
        # TODO possible bug: rerunning `result` repeatedly with nothing in between (no new
        # fw/run-partition) re-saves core_info_<next> off stale data while core_info.csv has
        # already advanced to the biggest size — current vs data can disagree. Revisit.
        # Fully-phantom nodes emit no REQUIRED_SAMPLE_SIZE (no measurement) — exclude them from
        # both the cross-node required-size max and the per-node core_info_<next> save.
        leaves = [l for l in (self.experiment_context.sub_experiments or [self.experiment_context])
                  if not l.all_phantom_cores]
        required = max(self._read_required(leaf) for leaf in leaves)
        current = leaves[0].sample_size
        if required <= current:
            print(f"[result] sample size {current} is sufficient (required {required}); no new core_info.")
            return
        nxt = int(math.ceil(required / 50.0)) * 50
        for leaf in leaves:
            folder = leaf.get_experiment_folder_address()
            shutil.copy(f"{folder}/core_info_new.csv", f"{folder}/run/core_info_{nxt}.csv")
        print(f"[result] required {required} > {current}; saved core_info_{nxt}.csv on {len(leaves)} node(s).")

    def _read_required(self, leaf: ExperimentContext) -> int:
        path = f"{leaf.get_experiment_folder_address()}/{REQUIRED_SAMPLE_SIZE_FILE}"
        if not os.path.exists(path):
            raise RuntimeError(f"{REQUIRED_SAMPLE_SIZE_FILE} not found at {path}; did the result step run?")
        with open(path) as f:
            return int(f.read().strip())
