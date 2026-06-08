import json
import os
import shlex
import socket
import subprocess
import time
from pathlib import Path
from telnetlib import Telnet

from commands import Executor
from .config import ExperimentContext
from commands.qemu import DEFAULT_MONITOR_PORT, QEMU_MONITOR_PORT_ENV_VAR, QemuCommonArgParser


class Boot(Executor):
    SNAPSHOT_NAME = "boot"
    SSH_HOST = "127.0.0.1"
    SSH_PORT = 2222
    SSH_USER = "qflex"
    SSH_PASSWORD = "qflex"
    MONITOR_HOST = "127.0.0.1"
    MONITOR_WAIT_SECONDS = 60
    QEMU_EXIT_WAIT_SECONDS = 10
    CAPTURE_WAIT_SECONDS = 900
    INVENTORY_WAIT_SECONDS = 120
    SNAPSHOT_WAIT_SECONDS = 600

    def __init__(self, experiment_context: ExperimentContext):
        self.experiment_context = experiment_context
        self.qemu_common_parser = QemuCommonArgParser(experiment_context)

    def cmd(self) -> str:
        raise NotImplementedError("Boot uses a Python-managed execute() flow.")

    def _repo_root(self) -> Path:
        return Path(__file__).resolve().parents[1]

    def _experiment_root(self) -> Path:
        return Path(self.experiment_context.get_experiment_folder_address())

    def _run_root(self) -> Path:
        return self._experiment_root() / "run"

    def _machine_config_path(self) -> Path:
        return Path(self.experiment_context.get_machine_config_path())

    def _kernel_dir(self) -> Path:
        return Path(self.experiment_context.get_kernel_bundle_dir())

    def _kernel_log_path(self) -> Path:
        return self._kernel_dir() / "capture.log"

    def _boot_log_path(self) -> Path:
        return self._run_root() / "boot.log"

    def _monitor_port(self) -> int:
        return int(os.environ.get(QEMU_MONITOR_PORT_ENV_VAR, DEFAULT_MONITOR_PORT))

    @staticmethod
    def _port_is_listening(host: str, port: int) -> bool:
        try:
            with socket.create_connection((host, port), timeout=1):
                return True
        except OSError:
            return False

    def _ensure_required_ports_available(self) -> None:
        monitor_port = self._monitor_port()
        if self._port_is_listening(self.MONITOR_HOST, monitor_port):
            raise RuntimeError(
                f"Monitor port {self.MONITOR_HOST}:{monitor_port} is already in use. "
                "Refusing to start boot because it could attach to the wrong VM."
            )
        if (
            self.experiment_context.simulation_context.network_mode == "user"
            and self._port_is_listening(self.SSH_HOST, self.SSH_PORT)
        ):
            raise RuntimeError(
                f"SSH forward port {self.SSH_HOST}:{self.SSH_PORT} is already in use. "
                "Refusing to start boot because kernel capture could connect to the wrong VM."
            )

    def _qemu_command(self, monitor_port: int) -> list[str]:
        qemu_args = self.qemu_common_parser.get_qemu_base_args(monitor_port=monitor_port)
        return shlex.split(f"./qemu-system-aarch64 {qemu_args}")

    def _wait_for_monitor(self, process: subprocess.Popen, monitor_port: int) -> None:
        deadline = time.time() + self.MONITOR_WAIT_SECONDS
        while time.time() < deadline:
            if process.poll() is not None:
                raise RuntimeError(
                    "QEMU exited before the monitor became ready. "
                    f"See {self._boot_log_path()}."
                )
            if self._port_is_listening(self.MONITOR_HOST, monitor_port):
                return
            else:
                time.sleep(1)
        raise RuntimeError(
            "Timed out waiting for the QEMU monitor on "
            f"{self.MONITOR_HOST}:{monitor_port}. See {self._boot_log_path()}."
        )

    @staticmethod
    def _wait_for_monitor_prompt(tn: Telnet, timeout: int) -> None:
        tn.read_until(b"(qemu)", timeout=timeout)

    def _monitor_command(self, tn: Telnet, command: str, timeout: int = 30) -> None:
        tn.write(command.encode("utf-8") + b"\n")
        self._wait_for_monitor_prompt(tn, timeout)

    def _save_snapshot_and_quit(self, monitor_port: int) -> None:
        tn = Telnet(self.MONITOR_HOST, monitor_port, timeout=10)
        try:
            self._wait_for_monitor_prompt(tn, 30)
            self._monitor_command(
                tn,
                f"savevm {self.SNAPSHOT_NAME}",
                timeout=self.SNAPSHOT_WAIT_SECONDS,
            )
            # In our live power-on boot path, QEMU acknowledges `quit` but may
            # still stay alive. We keep the explicit `quit` request here, then
            # rely on the bounded-workflow fallback in `_wait_for_qemu_exit()`
            # to terminate the process if it does not exit promptly.
            tn.write(b"quit\n")
        finally:
            tn.close()

    def _quit_qemu(self, process: subprocess.Popen, monitor_port: int) -> None:
        if process.poll() is not None:
            return
        try:
            tn = Telnet(self.MONITOR_HOST, monitor_port, timeout=5)
            try:
                self._wait_for_monitor_prompt(tn, 10)
                tn.write(b"quit\n")
            finally:
                tn.close()
        except OSError:
            process.terminate()

    def _wait_for_qemu_exit(self, process: subprocess.Popen) -> bool:
        try:
            process.wait(timeout=self.QEMU_EXIT_WAIT_SECONDS)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=10)
            return True
        if process.returncode not in (0, None):
            raise RuntimeError(
                f"QEMU exited with code {process.returncode}. "
                f"See {self._boot_log_path()}."
            )
        return False

    def _run_capture_helper(self, kernel_log: Path) -> subprocess.CompletedProcess:
        machine_config = self._machine_config_path()
        repo_root = self._repo_root()
        kernel_log.parent.mkdir(parents=True, exist_ok=True)

        if self.experiment_context.simulation_context.network_mode != "user":
            reason = (
                "Automated boot requires guest SSH over --network user. "
                "Kernel capture and boot snapshot automation were not attempted."
            )
            command = [
                "python3",
                "-m",
                "commands.capture_kernel",
                "--machine-config",
                str(machine_config),
                "--user-action-required-reason",
                reason,
            ]
        else:
            command = [
                "python3",
                "-m",
                "commands.capture_kernel",
                "--machine-config",
                str(machine_config),
                "--ssh-host",
                self.SSH_HOST,
                "--ssh-port",
                str(self.SSH_PORT),
                "--ssh-user",
                self.SSH_USER,
                "--ssh-password",
                self.SSH_PASSWORD,
                "--wait-seconds",
                str(self.CAPTURE_WAIT_SECONDS),
                "--inventory-wait-seconds",
                str(self.INVENTORY_WAIT_SECONDS),
            ]

        with kernel_log.open("w", encoding="utf-8") as capture_log:
            return subprocess.run(
                command,
                cwd=repo_root,
                stdout=capture_log,
                stderr=subprocess.STDOUT,
                text=True,
                check=False,
            )

    def _verify_snapshot_exists(self) -> None:
        snapshot_list = subprocess.run(
            [
                "./qemu-img",
                "snapshot",
                "-l",
                self.experiment_context.get_local_image_address(),
            ],
            cwd=self._run_root(),
            capture_output=True,
            text=True,
            check=True,
        )
        if self.SNAPSHOT_NAME not in snapshot_list.stdout:
            raise RuntimeError(
                f"Boot snapshot {self.SNAPSHOT_NAME!r} was not found in "
                f"{self.experiment_context.get_local_image_address()}."
            )

    def _load_machine_config(self) -> dict:
        return json.loads(self._machine_config_path().read_text(encoding="utf-8"))

    def execute(self, to_stdio: bool = True, run_in_background: bool = False):
        if run_in_background:
            raise RuntimeError("Boot does not support run_in_background.")
        if not to_stdio:
            raise RuntimeError("Boot expects to stream status to stdio.")
        if self.experiment_context.simulation_context.network_mode != "user":
            reason = (
                "Automated boot requires guest SSH over --network user. "
                "This lineage was not booted automatically, and kernel capture "
                "now requires explicit user action."
            )
            subprocess.run(
                [
                    "python3",
                    "-m",
                    "commands.capture_kernel",
                    "--machine-config",
                    str(self._machine_config_path()),
                    "--user-action-required-reason",
                    reason,
                ],
                cwd=self._repo_root(),
                check=True,
                text=True,
            )
            raise RuntimeError(reason)

        boot_log = self._boot_log_path()
        kernel_log = self._kernel_log_path()
        kernel_manifest = self._kernel_dir() / "kernel_manifest.json"
        monitor_port = self._monitor_port()
        qemu_command = self._qemu_command(monitor_port)

        self._ensure_required_ports_available()

        print("Boot mode: bounded QEMU boot workflow.")
        print(f"Boot log: {boot_log}")
        print(f"Kernel capture log: {kernel_log}")
        print(f"Kernel manifest target: {kernel_manifest}")
        print(f"Snapshot target: {self.SNAPSHOT_NAME}")

        boot_log.parent.mkdir(parents=True, exist_ok=True)
        kernel_log.parent.mkdir(parents=True, exist_ok=True)

        capture_result: subprocess.CompletedProcess | None = None
        boot_error: Exception | None = None
        process: subprocess.Popen | None = None
        forced_qemu_termination = False

        with boot_log.open("w", encoding="utf-8") as boot_stream:
            process = subprocess.Popen(
                qemu_command,
                cwd=self._run_root(),
                stdout=boot_stream,
                stderr=subprocess.STDOUT,
                text=True,
            )
            try:
                self._wait_for_monitor(process, monitor_port)
                capture_result = self._run_capture_helper(kernel_log)
                self._save_snapshot_and_quit(monitor_port)
            except Exception as exc:
                boot_error = exc
                self._quit_qemu(process, monitor_port)
            else:
                forced_qemu_termination = self._wait_for_qemu_exit(process)
                process = None

        if process is not None:
            forced_qemu_termination = self._wait_for_qemu_exit(process) or forced_qemu_termination

        if boot_error is not None:
            raise boot_error

        self._verify_snapshot_exists()

        machine_config = self._load_machine_config()
        kernel_status = machine_config.get("kernel_capture_status", "")
        kernel_reason = machine_config.get("kernel_capture_reason", "")

        if capture_result is None:
            raise RuntimeError("Kernel capture did not run during boot.")
        if capture_result.returncode != 0:
            raise RuntimeError(
                "Kernel capture failed during boot. "
                f"See {kernel_log}."
            )

        print("Boot completed.")
        print(f"VM snapshot created: {self.SNAPSHOT_NAME}")
        print(f"Kernel capture status: {kernel_status}")
        if kernel_reason:
            print(f"Kernel capture reason: {kernel_reason}")
        if forced_qemu_termination:
            print(
                "QEMU did not exit promptly after the monitor quit request; "
                "the bounded boot workflow terminated it explicitly."
            )
        if kernel_status == "ready":
            print("Next step: qflex load")
        elif kernel_status == "user_action_required":
            print(
                "Next step: qflex could not complete the kernel bundle for this "
                "lineage automatically. Provide the matching vmlinux ELF explicitly with "
                "`qflex kernel adopt`, then proceed to load once the lineage is ready."
            )
        else:
            print(
                "Next step: inspect the kernel capture state before proceeding. "
                "This is an unexpected internal state."
            )
