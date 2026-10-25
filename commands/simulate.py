import os

from commands import Executor
from .boot import Boot
from .load import Load
from .init_warm import InitWarm
from .functional_warming import FunctionalWarming
from .partition import PartitionCommand
from .run_partition import RunPartitionCommand
from .full_timing import FullTimingCommand
from .result import RunResultCommand

# Ordered pipeline. Keys are the phase command names (== build_experiment_context
# cmd_name == the YAML phase-overlay block key) -> the Executor that runs that phase.
PHASE_ORDER = ["boot", "load", "initialize", "fw", "partition", "run_partition", "result"]
FULL_TIMING_ORDER = ["boot", "load", "initialize", "full_timing", "result"]
PHASE_BUILDERS = {
    "full_timing": FullTimingCommand,
    "boot": Boot,
    "load": Load,
    "initialize": InitWarm,
    "fw": FunctionalWarming,
    "partition": PartitionCommand,
    "run_partition": RunPartitionCommand,
    "result": RunResultCommand,
}


class SimulateCommand(Executor):
    """Runs the pipeline from start_point through end_point (inclusive) in one invocation.
    Rebuilds the experiment context per phase (via build_experiment_context with that
    phase's cmd_name) so each phase's YAML overlay block applies exactly as if the phase
    were run on its own. cmd() is unused; each phase fans out across nodes via its own
    execute() — same orchestration pattern as StatisticalSampleCommand."""

    def __init__(self, config_path: str, start_point: str = None, end_point: str = None):
        config_path = config_path or os.environ.get("QFLEX_CONFIG")
        if not config_path:
            raise ValueError("simulate needs a config: pass -c <path> or set $QFLEX_CONFIG.")
        self.config_path = config_path
        self.start_point_override = start_point
        self.end_point_override = end_point

    def cmd(self) -> str:
        raise NotImplementedError("SimulateCommand orchestrates other phases via execute().")

    def execute(self,
                to_stdio: bool = True,
                run_in_background: bool = False,
                dry_run: bool = False,
                *,
                sentinel_dir: str = None,
                log_path: str = None,
                err_path: str = None,
                log_append: bool = False) -> bool:
        # Deferred so qflex can still load (e.g. for --help) without injector/omegaconf.
        from dep_injection.builder import build_experiment_context

        # CLI flag wins; otherwise take the YAML/factory value (the start_point / end_point
        # fields on ExperimentContext) — the same flag-overrides-YAML path every knob uses.
        probe = build_experiment_context(self.config_path, cmd_name="simulate")
        start = self.start_point_override if self.start_point_override is not None else probe.start_point
        end = self.end_point_override if self.end_point_override is not None else probe.end_point
        start, end = start.replace("-", "_"), end.replace("-", "_")
        order = FULL_TIMING_ORDER if probe.full_timing else PHASE_ORDER
        if probe.full_timing and end == "run_partition":   # the factory default; full-timing is its counterpart
            end = "full_timing"
        for name, val in (("start_point", start), ("end_point", end)):
            if val not in order:
                raise ValueError(f"{name} must be one of {order}, got {val!r}")
        start_idx, end_idx = order.index(start), order.index(end)
        if end_idx < start_idx:
            raise ValueError(f"end_point {end!r} is before start_point {start!r}.")

        for cmd_name in order[start_idx:end_idx + 1]:
            exp = build_experiment_context(self.config_path, cmd_name=cmd_name)
            phase = PHASE_BUILDERS[cmd_name](exp)
            # Stop the chain on failure. Multi-node group dispatch raises on its own;
            # a single-node leaf returns False — translate that into a raise, same as
            # SequentialGroupExecutor / _execute_group do.
            if not phase.execute(to_stdio=to_stdio, run_in_background=run_in_background, dry_run=dry_run):
                raise RuntimeError(f"simulate: phase {cmd_name!r} failed; stopping.")
        return True
