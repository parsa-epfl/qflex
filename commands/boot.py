import os
import re
import shutil
import shlex
from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser

class Boot(Executor):

    def __init__(self, 
                 experiment_context: ExperimentContext):
        self.experiment_context = experiment_context
        self.qemu_common_parser = QemuCommonArgParser(experiment_context)

    @staticmethod
    def _session_name(experiment_name: str) -> str:
        cleaned = re.sub(r"[^A-Za-z0-9_.-]+", "-", experiment_name).strip("-")
        if not cleaned:
            cleaned = "qflex-boot"
        return f"qflex-boot-{cleaned}"

    def cmd(self) -> str:
        repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
        experiment_root = self.experiment_context.get_experiment_folder_address()
        run_root = f"{experiment_root}/run"
        boot_log = f"{run_root}/boot.log"
        session_name = self._session_name(self.experiment_context.experiment_name)
        qemu_args = " ".join(self.qemu_common_parser.get_qemu_base_args().split())
        qemu_session_cmd = (
            f"cd {run_root} && ./qemu-system-aarch64 {qemu_args} > {boot_log} 2>&1"
        )
        tmux_path = shutil.which("tmux")
        screen_path = shutil.which("screen")
        if tmux_path:
            attach_cmd = f"{tmux_path} attach -t {shlex.quote(session_name)}"
            session_start_cmd = (
                f"{tmux_path} has-session -t {shlex.quote(session_name)} 2>/dev/null && "
                f"{{ echo 'Boot session already exists: {session_name}'; exit 1; }} || "
                f"{tmux_path} new-session -d -s {shlex.quote(session_name)} "
                f"{shlex.quote(qemu_session_cmd)}"
            )
        elif screen_path:
            attach_cmd = f"{screen_path} -r {shlex.quote(session_name)}"
            session_start_cmd = (
                f"{screen_path} -ls {shlex.quote(session_name)} >/dev/null 2>&1 && "
                f"{{ echo 'Boot session already exists: {session_name}'; exit 1; }} || "
                f"{screen_path} -S {shlex.quote(session_name)} -dm "
                f"bash -lc {shlex.quote(qemu_session_cmd)}"
            )
        else:
            raise RuntimeError(
                "Detached boot requires tmux or screen, but neither was found in PATH."
            )
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
                f"echo 'Boot session: {session_name}'"
            )
            pre_boot_cmds.append(
                f"echo 'Attach command: {attach_cmd}'"
            )
            pre_boot_cmds.append(
                f"echo 'Boot log: {boot_log}'"
            )
            pre_boot_cmds.append(session_start_cmd)
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
                f"echo 'Boot session: {session_name}'"
            )
            pre_boot_cmds.append(
                f"echo 'Attach command: {attach_cmd}'"
            )
            pre_boot_cmds.append(
                f"echo 'Boot log: {boot_log}'"
            )
            pre_boot_cmds.append(session_start_cmd)
            pre_boot_cmds.append(
                f"cd {shlex.quote(repo_root)} && "
                f"python3 -m commands.capture_kernel "
                f"--machine-config {shlex.quote(machine_config)} "
                f"--manual-required-reason {shlex.quote(reason)}"
            )
        return pre_boot_cmds
