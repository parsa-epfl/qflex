import math

from commands import SimulationCommand
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser, wrap_with_gdb

class FunctionalWarming(SimulationCommand):
    """
    This class handles the functional warming phase of the experiment and generates checkpoints.
    """

    def __init__(self,
                 experiment_context: ExperimentContext):
        self.experiment_context = experiment_context

    def cmd(self) -> str:
        self._assert_syncs_true()
        exp = self.experiment_context
        # Multi-fidelity phantom node: no detailed warming — run a plain parallel qemu like load
        # (no worm plugin). It feeds PDES and advances at the phantom IPC; the master's distributed
        # savevm still persists this node's per-idx checkpoints (without microarch state).
        # The uniform phantom node (not multi_modal) falls through to the normal worm command below
        # — it keeps the plugin loaded but warms zero cores (REAL_CORE_COUNT=0 in its recompiled
        # parameter.rs), so its checkpoints have the same (empty) .uarch layout as a normal node.
        if exp.all_phantom_cores and exp.multi_modal:
            return [
                f"cd {exp.get_experiment_folder_address()}/run",
                QemuCommonArgParser(exp).get_qemu_command(),
            ]
        # Build per-context derived state fresh so this method works whether
        # self.experiment_context was set at __init__ or mutated later (multi-experiment dispatch).
        parser = QemuCommonArgParser(self.experiment_context)
        sample_size = self.experiment_context.sample_size
        sampling_interval = math.ceil(
            (self.experiment_context.workload.population + sample_size - 1) / sample_size
        )

        fw_cmd = wrap_with_gdb(
            f"./qemu-system-aarch64 {parser.get_qemu_base_args()} "
            f"-plugin ../lib/libworm_cache.so,mode=warm,"
            f"init_threshold={sampling_interval},interval={sampling_interval},count={sample_size}",
            self.experiment_context.use_gdb,
        )
        tock_command = " tock=$(($(date +%s%N) / 1000000)) "
        time_command = ' echo "Elapsed: $((tock - tick)) ms " '
        return [
            f"cd {self.experiment_context.get_experiment_folder_address()}/run",
            tock_command,
            fw_cmd,
            time_command,
            # TODO add the proper conditions to only create log and fp_gen_speed at the right time
            "rm -rf fp_gen_speed",
            "mkdir fp_gen_speed",
            # parallel-qemu doesn't emit `.log` files by default, so on a normal
            # FW run there's nothing to move and `mv *.log` errors out with
            # "cannot stat '*.log'". `|| true` tolerates that. Without it, the
            # bash group exits with mv's rc=1, which then races the executor's
            # symmetric peer-kill marker (parent joins fast-finisher before
            # slow-finisher writes the killed_by_peer marker) and surfaces as
            # a non-deterministic test failure.
            "mv *.log ./fp_gen_speed 2>/dev/null || true",
        ]
