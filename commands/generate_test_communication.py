from commands import SimulationCommand
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser

# Generic, workload-agnostic capture script: read-only /proc/net/dev + /proc/interrupts before/after a
# host-clock interval. ONE script for single-node AND every multi-node leaf — no per-experiment expect.
DATA_MOVEMENT_SCRIPT = "/home/dev/qflex/conf/expects/diag_data_movement.exp"


class GenerateTestCommunication(SimulationCommand):
    """Resume the `loaded` snapshot and let the already-running workload generate traffic for
    `duration_seconds` of GUEST time (a guest-side sleep — host-resource-independent, so single-vs-multi
    deltas are comparable), capturing data-movement counters — WITHOUT savevm and WITHOUT modifying the
    qcow. NOTE: under PDES sync one guest-second can take many host-minutes.

    Guarantees:
      * Runs on a `cp -u` copy of the qcow in a shared `tmp/` under the images folder, keyed by the
        final image name (-snapshot can't be used: its overlay hides the internal `loaded` snapshot).
        The ORIGINAL qcow is never opened by qemu → byte-identical; the copy is shared across
        experiments/runs and re-copied only if the source changed. Nothing is ever savevm'd.
      * Load-phase / parallel-qemu (no Flexus) → measures NO U-IPC; the timing sweep is untouched.
      * Arms the (otherwise-off) PDES data counters via QFLEX_MEASURE_DATA_MOVEMENT so multi-node
        runs report cross-node bytes/#chunks; single-node has no PDES wire (loopback only).
    result_new reads the produced files (netdev_*.txt, interrupts_*.txt, [PDES-WIRE]) if present.
    """

    def __init__(self, experiment_context: ExperimentContext, duration_seconds: int = 1):
        self.experiment_context = experiment_context
        self.duration_seconds = duration_seconds

    def cmd(self) -> str:
        self._assert_syncs_true()
        exp = self.experiment_context
        # Systematic: every leaf resumes `loaded` and runs the generic capture script. Set both
        # programmatically (this command has no YAML phase block, so nothing wires them otherwise).
        exp.loadvm_name = "loaded"
        exp.interaction_script = DATA_MOVEMENT_SCRIPT
        # We set interaction_script after the executor already ran prepare_for_execution, so the
        # Path A telnet ports weren't auto-assigned yet — recompute (pure/idempotent) to pick them up.
        exp.compute_runtime_settings()
        # Run on a COPY of the qcow (not -snapshot: -snapshot's transient overlay hides the internal
        # `loaded` snapshot, so -loadvm fails). The copy lives in a shared `tmp/` under the images
        # folder, keyed by the final per-node image name — so distinct base images each get ONE copy
        # reused across experiments/runs (cp -u skips unless the source changed); the original qcow is
        # never opened by qemu here, so it stays byte-identical.
        src_qcow = exp.get_local_image_address()
        run_dir = f"{exp.get_experiment_folder_address()}/run"
        tmp_dir = f"{exp.image_folder}/tmp"
        tmp_qcow = f"{tmp_dir}/{exp.image_name}"
        exp.image_address = tmp_qcow
        # Path A (scripted): serial-on-telnet so the capture script can drive the guest.
        parser = QemuCommonArgParser(exp, use_stdio=False)

        # QFLEX_MEASURE_DATA_MOVEMENT=1: arm the (otherwise-off) PDES counters for THIS run only.
        # No gdb (avoids quit-time prompts). No savevm anywhere.
        run_cmd = (
            f"QFLEX_MEASURE_DATA_MOVEMENT=1 ./qemu-system-aarch64 "
            f"{parser.get_qemu_base_args()}"
        )
        env_vars = (
            f"TELNET_SERIAL_PORT={exp.serial_telnet_port} "
            f"TELNET_MONITOR_PORT={exp.telnet_port} "
            f"SERIAL_LOG_PATH=./serial.log "
            f"EXP_FOLDER={exp.get_experiment_folder_address()} "
            f"NODE_NUMBER={exp.node_number} "
            f"GROUP_EXP_FOLDER={exp.parent_experiment_folder or exp.get_experiment_folder_address()} "
            f"DATA_MOVE_DURATION_S={self.duration_seconds}"
        )
        return (
            f"cd {run_dir} && "
            f"mkdir -p {tmp_dir} && cp -u {src_qcow} {tmp_qcow} && "
            f"{{ {env_vars} {exp.interaction_script} & "
            f"SCRIPT_PID=$!; "
            f"{run_cmd}; "
            f"wait $SCRIPT_PID 2>/dev/null; }}"
        )
