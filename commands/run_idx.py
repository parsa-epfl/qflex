import contextlib

from commands import SimulationCommand
from .config import ExperimentContext
from commands.qemu import VanillaQemuArgParser, PhantomTimingArgParser, PhantomUniformTimingArgParser
from .executer import _is_dry_run


class RunIdxCommand(SimulationCommand):
    def __init__(self,
                 experiment_context: ExperimentContext,
                 use_stdio: bool = True):
        self.experiment_context = experiment_context
        self.use_stdio = use_stdio

    @property
    def NEEDS_PDES_PEER_KILL(self) -> bool:
        # vanilla-qemu's PDES exit handshake is clean → no peer-kill. A multi-fidelity phantom node
        # runs the parallel binary (PDES exit-hang bug) and the master won't kill it, so it needs
        # one. The uniform phantom node runs vanilla+Flexus (clean exit) → no peer-kill.
        exp = self.experiment_context
        return exp.all_phantom_cores and exp.multi_modal

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
        exp = self.experiment_context
        # TODO turn this into a param, for now each ratio represents 100000 cycles
        ratio_coefficient = 100000
        total_cycles = ((exp.warming_ratio * ratio_coefficient)
                        + (exp.measurement_ratio * ratio_coefficient)) + 1
        idx = exp.idx
        partition_folder = exp.get_partition_folder()

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

        # Multi-fidelity phantom node: no Flexus, no checkpoint conversion — a plain parallel qemu
        # that loads the per-idx snapshot and advances at the phantom IPC, pairing with the master
        # over PDES until the master's idx exits. (The uniform phantom node falls through to the
        # master path below, only swapping the Flexus target for libphantomkraken.)
        if exp.all_phantom_cores and exp.multi_modal:
            parser = PhantomTimingArgParser(exp, idx, total_cycles, use_stdio=self.use_stdio)
            qemu_cmd = f"{parser.qemu_binary} {parser.get_qemu_base_args()} {output} < /dev/null"
            return [
                f"cd {partition_folder}",
                f'echo "===== qflex idx {idx} (phantom): starting in {partition_folder} ====="',
            ] + log_banner + [
                "tick=$(($(date +%s%N) / 1000000))",
                qemu_cmd,
                "tock=$(($(date +%s%N) / 1000000))",
                'echo "Elapsed: $((tock - tick)) ms"',
            ]

        # Uniform phantom node (all_phantom_cores and not multi_modal): identical to the master
        # timing leaf — same vanilla binary, checkpoint_conversion, per-idx snapshot — only the
        # Flexus target differs (libphantomkraken, all cores phantom).
        if exp.all_phantom_cores:
            vanilla_parser = PhantomUniformTimingArgParser(exp, idx, total_cycles, use_stdio=self.use_stdio)
        else:
            vanilla_parser = VanillaQemuArgParser(exp, idx, total_cycles, use_stdio=self.use_stdio)
        setup_commands = [
            f"cd {partition_folder}",
            f'echo "===== qflex idx {idx}: starting in {partition_folder} ====="',
            f'rm -rf "snapshot_{idx}-flexus"',
            f"mkdir snapshot_{idx}-flexus",
            f"./checkpoint_conversion ./snapshot_{idx}.uarch ../../cfg/flexus_configuration.json ./snapshot_{idx}-flexus true",
            f'rm -rf "result_{idx}"',
            f'mkdir "result_{idx}"',
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
