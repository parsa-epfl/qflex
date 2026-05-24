from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser, wrap_with_gdb
# TODO IMPORTANT, remove vanilla option

class Boot(Executor):
    # Boot is one of the two phases (with Load) where the user might want to
    # interact with QEMU manually. The base Executor reads this flag to decide
    # whether to dispatch into the libtmux path when interactive_tmux is set.
    SUPPORTS_INTERACTIVE = True

    def __init__(self,
                 experiment_context: ExperimentContext,
                 vanilla: bool = False):
        self.experiment_context = experiment_context
        self.vanilla = vanilla

    def cmd(self) -> str:
        if self.vanilla:
            raise NotImplementedError(
                "Booting with vanilla QEMU is not implemented yet. This was only meant "
                "to be used for loading with vanilla QEMU for functional warming "
                "preparation, but we can implement it if needed."
            )

        exp = self.experiment_context
        # Path A (scripted): drop stdio so the parser emits serial-on-telnet via get_stdio().
        use_stdio = not bool(exp.interaction_script)
        parser = QemuCommonArgParser(exp, use_stdio=use_stdio)
        gdb_cmd = wrap_with_gdb(
            f"./qemu-system-aarch64 {parser.get_qemu_base_args()}",
            exp.use_gdb,
            interactive_tmux=exp.interactive_tmux,
        )

        if not exp.interaction_script:
            return [
                f"cd {exp.get_experiment_folder_address()}/run",
                gdb_cmd,
            ]

        # Path A: run the user's script (background, captures PID), then QEMU
        # (foreground), then wait on the script. cd + everything else in a single
        # `{ ...; }` group so the cwd persists past `cd`.
        env_vars = (
            f"TELNET_SERIAL_PORT={exp.serial_telnet_port} "
            f"TELNET_MONITOR_PORT={exp.telnet_port} "
            f"SERIAL_LOG_PATH=./serial.log "
            f"EXP_FOLDER={exp.get_experiment_folder_address()} "
            f"NODE_NUMBER={exp.node_number} "
            f"GROUP_EXP_FOLDER={exp.parent_experiment_folder or exp.get_experiment_folder_address()}"
        )
        return (
            f"cd {exp.get_experiment_folder_address()}/run && "
            f"{{ {env_vars} {exp.interaction_script} & "
            f"SCRIPT_PID=$!; "
            f"{gdb_cmd}; "
            f"wait $SCRIPT_PID 2>/dev/null; }}"
        )
