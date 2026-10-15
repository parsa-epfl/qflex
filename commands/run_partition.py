import os
import signal
import subprocess
from commands import Executor
from .config import ExperimentContext

CAP_SYS_PTRACE = 19


def userfaultfd_problem() -> str | None:
    """
    Why the timing runs cannot create a userfaultfd, or None. They load each checkpoint with
    `-loadvm snapshot_<n>,on-demand`, which fetches the guest memory on page faults through a userfaultfd;
    without one, QEMU stops on an assertion (on_demand_uffd_fd >= 0).
    """
    if os.geteuid() == 0 or os.access("/dev/userfaultfd", os.R_OK | os.W_OK):
        return None
    try:
        with open("/proc/sys/vm/unprivileged_userfaultfd") as f:
            if f.read().strip() == "1":
                return None
        with open("/proc/self/status") as f:
            capabilities = next(int(line.split()[1], 16) for line in f if line.startswith("CapEff:"))
        if capabilities & (1 << CAP_SYS_PTRACE):
            return None
    except (OSError, StopIteration):
        return None  # cannot tell; let QEMU try
    return ("Error: the timing runs need userfaultfd (on-demand loading of the checkpoints), which this user cannot create. "
            "Allow it with `sudo sysctl -w vm.unprivileged_userfaultfd=1` (add `vm.unprivileged_userfaultfd = 1` to "
            "/etc/sysctl.d/99-userfaultfd.conf to keep it after a reboot), or give your user access to /dev/userfaultfd.")


class RunPartitionCommand(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 warming_ratio: int, 
                 measurement_ratio: int):
        self.experiment_context = experiment_context
        self.detailed_warming_ratio = warming_ratio
        self.measurement_ratio = measurement_ratio
        self.experiment_folder = self.experiment_context.get_experiment_folder_address()
        assert os.path.exists(f"{self.experiment_folder}/scripts/run_flexus.sh"), "Error: run_flexus.sh not found. Please run the init_warm command first to generate necessary scripts."
        assert os.path.exists(f"{self.experiment_folder}/run/partition_0"), "Error: no partitions found. Make sure partition command has been run."
        problem = userfaultfd_problem()
        if problem:
            raise RuntimeError(problem)


    def cmd(self) -> str | list[str]:
        # TODO get rid of partition at some point and move run_flexus.sh in our python commands
        return [
            f"cd {self.experiment_folder}",
            f"{self.experiment_folder}/run_partitions.sh {self.detailed_warming_ratio} {self.measurement_ratio}"
        ]

    def execute(self, to_stdio: bool = True, run_in_background: bool = False):
        """
        Runs run_partitions.sh and stops it when qflex is stopped: SIGTERM and SIGHUP are forwarded to the script,
        whose trap then stops every partition. Ctrl+C reaches the script directly (same process group).
        """
        process = subprocess.Popen(
            [f"{self.experiment_folder}/run_partitions.sh", str(self.detailed_warming_ratio), str(self.measurement_ratio)],
            cwd=self.experiment_folder,
        )

        def forward(signum, frame):
            process.send_signal(signal.SIGTERM)

        previous = {s: signal.signal(s, forward) for s in (signal.SIGTERM, signal.SIGHUP)}
        try:
            while True:
                try:
                    process.wait()
                    break
                except KeyboardInterrupt:
                    # A Ctrl+C from the terminal already reached the script; a `kill -INT` of qflex alone did not.
                    process.send_signal(signal.SIGTERM)
        finally:
            for s, handler in previous.items():
                signal.signal(s, handler)
        return subprocess.CompletedProcess(process.args, process.returncode)
