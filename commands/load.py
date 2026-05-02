from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser
# TODO IMPORTANT, remove vanilla option


class Load(Executor):
    # See Boot — Load is the other interactive-eligible phase.
    SUPPORTS_INTERACTIVE = True

    def __init__(self,
                 experiment_context: ExperimentContext,
                 vanilla: bool = False):
        self.experiment_context = experiment_context
        self.vanilla = vanilla

    def cmd(self) -> str:
        exp = self.experiment_context
        # Path A (scripted): drop stdio so the parser emits serial-on-telnet via get_stdio().
        use_stdio = not bool(exp.interaction_script)
        parser = QemuCommonArgParser(exp, use_stdio=use_stdio)

        if not self.vanilla:
            load_cmd = (
                f"gdb -ex run --args ./qemu-system-aarch64 "
                f"{parser.get_qemu_base_args()}"
            )
        else:
            load_cmd = (
                f"./vanilla-qemu-system-aarch64 "
                f"{parser.get_qemu_base_args()}"
            )

        if not exp.interaction_script:
            # WormCacheQFlex/src/parameter.rss
            return [
                f"cd {exp.get_experiment_folder_address()}/run",
                load_cmd,
            ]

        env_vars = (
            f"TELNET_SERIAL_PORT={exp.serial_telnet_port} "
            f"TELNET_MONITOR_PORT={exp.telnet_port} "
            f"SERIAL_LOG_PATH=./serial.log "
            f"EXP_FOLDER={exp.get_experiment_folder_address()} "
            f"NODE_NUMBER={exp.node_number}"
        )
        return (
            f"cd {exp.get_experiment_folder_address()}/run && "
            f"{{ {env_vars} {exp.interaction_script} & "
            f"SCRIPT_PID=$!; "
            f"{load_cmd}; "
            f"wait $SCRIPT_PID 2>/dev/null; }}"
        )
