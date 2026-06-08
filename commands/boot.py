import os
import shlex
from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser

class Boot(Executor):

    def __init__(self, 
                 experiment_context: ExperimentContext):
        self.experiment_context = experiment_context
        self.qemu_common_parser = QemuCommonArgParser(experiment_context)

    def cmd(self) -> str:
        repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
        experiment_root = self.experiment_context.get_experiment_folder_address()
        pre_boot_cmds = []
        if self.experiment_context.simulation_context.network_mode == "user":
            machine_config = f"{experiment_root}/machine_config.json"
            kernel_log = f"{experiment_root}/kernel/capture.log"
            kernel_manifest = f"{experiment_root}/kernel/kernel_manifest.json"
            pre_boot_cmds.append(
                "echo 'Kernel capture: automatic guest-based capture is enabled for this boot.'"
            )
            pre_boot_cmds.append(
                f"echo 'Kernel capture log: {kernel_log}'"
            )
            pre_boot_cmds.append(
                f"echo 'Kernel manifest target: {kernel_manifest}'"
            )
            pre_boot_cmds.append(
                f"(cd {shlex.quote(repo_root)} && "
                f"python3 -m commands.capture_kernel "
                f"--machine-config {shlex.quote(machine_config)} "
                f"--ssh-host 127.0.0.1 "
                f"--ssh-port 2222 "
                f"--ssh-user qflex "
                f"--ssh-password qflex "
                f"--wait-seconds 900 "
                f"> {shlex.quote(kernel_log)} 2>&1 &)"
            )
        else:
            machine_config = f"{experiment_root}/machine_config.json"
            kernel_notice = f"{experiment_root}/kernel/MANUAL_CAPTURE_REQUIRED.txt"
            reason = (
                "boot launched without guest user networking/SSH; "
                "automatic kernel capture is unavailable for this lineage"
            )
            pre_boot_cmds.append(
                "echo 'Kernel capture: manual action required because guest SSH is unavailable in this boot configuration.'"
            )
            pre_boot_cmds.append(
                f"echo 'Kernel capture notice: {kernel_notice}'"
            )
            pre_boot_cmds.append(
                f"echo 'Machine config to update later: {machine_config}'"
            )
            pre_boot_cmds.append(
                f"cd {shlex.quote(repo_root)} && "
                f"python3 -m commands.capture_kernel "
                f"--machine-config {shlex.quote(machine_config)} "
                f"--manual-required-reason {shlex.quote(reason)}"
            )

        boot_cmd = f"""
        ./qemu-system-aarch64 \
        {self.qemu_common_parser.get_qemu_base_args()}
        """

        commands = list(pre_boot_cmds)
        commands.extend(
            [
                f"cd {experiment_root}/run",
                boot_cmd,
            ]
        )
        return commands
