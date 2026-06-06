import contextlib

from commands import SimulationCommand
from .config import ExperimentContext
from commands.qemu import VanillaQemuArgParser
from .executer import _is_dry_run


class RunIdxCommand(SimulationCommand):
    NEEDS_PDES_PEER_KILL = False

    def __init__(self,
                 experiment_context: ExperimentContext,
                 use_stdio: bool = True):
        self.experiment_context = experiment_context
        self.use_stdio = use_stdio

    def execute(self, to_stdio: bool = True, run_in_background: bool = False,
                dry_run: bool = False, *, sentinel_dir: str = None,
                log_path: str = None, err_path: str = None, log_append: bool = False) -> bool:
        # Send this idx's Python prep noise (set_up_folders copies, shm cleanup, the qemu-args dump from
        # cmd()) into the shared partition log/err — the same file the qemu output is appended to —
        # rather than the console. Interactive/dry runs keep the console (no redirect).
        with contextlib.ExitStack() as stack:
            if not to_stdio and not _is_dry_run(dry_run):
                lf = stack.enter_context(open(self.get_log_file_address(), "a", buffering=1))
                ef = stack.enter_context(open(self.get_err_file_address(), "a", buffering=1))
                stack.enter_context(contextlib.redirect_stdout(lf))
                stack.enter_context(contextlib.redirect_stderr(ef))
            return super().execute(to_stdio=to_stdio, run_in_background=run_in_background,
                                   dry_run=dry_run, sentinel_dir=sentinel_dir,
                                   log_path=log_path, err_path=err_path, log_append=log_append)

    def get_err_file_address(self):
        return f"{self.experiment_context.get_partition_folder()}/err"

    def get_log_file_address(self):
        return f"{self.experiment_context.get_partition_folder()}/log"

    def cmd(self) -> str:
        self._assert_syncs_true()
        # TODO turn this into a param, for now each ratio represents 100000 cycles
        ratio_coefficient = 100000
        total_cycles = ((self.experiment_context.warming_ratio * ratio_coefficient)
                        + (self.experiment_context.measurement_ratio * ratio_coefficient)) + 1
        idx = self.experiment_context.idx
        vanilla_parser = VanillaQemuArgParser(self.experiment_context, idx, total_cycles,
                                              use_stdio=self.use_stdio)
        partition_folder = self.experiment_context.get_partition_folder()

        setup_commands = [
            f"cd {partition_folder}",
            f'echo "===== qflex idx {idx}: starting in {partition_folder} ====="',
            f'rm -rf "snapshot_{idx}-flexus"',
            f"mkdir snapshot_{idx}-flexus",
            f"./checkpoint_conversion ./snapshot_{idx}.uarch ../../cfg/flexus_configuration.json ./snapshot_{idx}-flexus true",
            f'rm -rf "result_{idx}"',
            f'mkdir "result_{idx}"',
        ]

        output = ""
        log_banner = []
        if not self.use_stdio:
            # Append (>>) so multiple idxs in the same partition (sequenced by
            # RunSinglePartitionCommand) accumulate into one shared log/err
            # rather than each idx truncating the prior idx's content.
            # RunSinglePartitionCommand wipes the files once at the start of
            # the partition, so the first idx finds them empty.
            output = f">> {self.get_log_file_address()} 2>> {self.get_err_file_address()}"
            log_banner = [
                f'echo "===== qflex idx {idx}: stdout =====" >> {self.get_log_file_address()}',
                f'echo "===== qflex idx {idx}: stderr =====" >> {self.get_err_file_address()}',
            ]

        # No gdb wrap. With vanilla-qemu's clean exit handshake the only way
        # qemu returns non-zero is a real crash — let it propagate up through
        # the && chain rather than masking it. Cleanup (`mv` / `cp`) runs only
        # on success, which is fine because qemu DOES exit cleanly here.
        qemu_cmd = (
            f"../vanilla-qemu-system-aarch64 "
            f"{vanilla_parser.get_qemu_base_args()} {output} < /dev/null"
        )
        return setup_commands + log_banner + [
            "tick=$(($(date +%s%N) / 1000000))",
            qemu_cmd,
            "tock=$(($(date +%s%N) / 1000000))",
            'echo "Elapsed: $((tock - tick)) ms"',
            f'mv *.log "result_{idx}/" 2>/dev/null || true',
            f'cp -f "result_{idx}/run-partition.log" '
            f'"{partition_folder}/run-partition.log" 2>/dev/null || true',
        ]
