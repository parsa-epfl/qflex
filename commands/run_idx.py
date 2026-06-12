import contextlib

from commands import SimulationCommand
from .config import ExperimentContext
from commands.qemu import VanillaQemuArgParser, PhantomTimingArgParser, PhantomUniformTimingArgParser
from .executer import _is_dry_run


class RunIdxCommand(SimulationCommand):
    def __init__(self,
                 experiment_context: ExperimentContext,
                 use_stdio: bool = False):
        self.experiment_context = experiment_context
        self.use_stdio = use_stdio

    def execute(self, to_stdio: bool = True, run_in_background: bool = False,
                dry_run: bool = False, *, sentinel_dir: str = None,
                log_path: str = None, err_path: str = None, log_append: bool = False) -> bool:
        # Per idx, send ALL Python prep + cleanup prints (setup/nic lines, the qemu-args dump, the
        # shm-cleanup "Removing shared memory" lines) into the SAME partition log/err that the qemu/flexus
        # output streams to via _with_shared_log — so partition_<P>/log is the complete single lifecycle
        # (setup → license → PDES setup/finish → libqflex), like the legacy log. Off the terminal so
        # run-partition's progress bars stay clean. A direct run-idx (use_stdio True) keeps the console.
        # Leaf-only: a group context (sub_experiments) has no partition folder of its own — the mp
        # children re-enter execute() with leaf contexts and take the redirect there.
        with contextlib.ExitStack() as stack:
            if (not self.use_stdio and not _is_dry_run(dry_run)
                    and not self.experiment_context.has_sub_experiments()):
                log_f, err_f = self.get_log_file_address(), self.get_err_file_address()
                # A direct run-idx has no RunSinglePartitionCommand to wipe the shared log first
                # (log_append is set only when sequenced under one), so truncate once here; otherwise
                # append so several idxs in a partition accumulate. Both opens use append mode so this
                # redirect's writes and the bash (_with_shared_log) writes always go to the end in order.
                if not log_append:
                    open(log_f, "w").close()
                    open(err_f, "w").close()
                lf = stack.enter_context(open(log_f, "a", buffering=1))
                ef = stack.enter_context(open(err_f, "a", buffering=1))
                stack.enter_context(contextlib.redirect_stdout(lf))
                stack.enter_context(contextlib.redirect_stderr(ef))
            return super().execute(to_stdio=to_stdio, run_in_background=run_in_background,
                                   dry_run=dry_run, sentinel_dir=sentinel_dir,
                                   log_path=log_path, err_path=err_path, log_append=log_append)

    @property
    def NEEDS_PDES_PEER_KILL(self) -> bool:
        # vanilla-qemu's PDES exit handshake is clean → no peer-kill. A multi-fidelity phantom node
        # runs the parallel binary (PDES exit-hang bug) and the master won't kill it, so it needs
        # one. The uniform phantom node runs vanilla+Flexus (clean exit) → no peer-kill.
        exp = self.experiment_context
        return exp.all_phantom_cores and exp.multi_modal

    def get_err_file_address(self):
        return f"{self.experiment_context.get_partition_folder()}/err"

    def get_log_file_address(self):
        return f"{self.experiment_context.get_partition_folder()}/log"

    def _with_shared_log(self, steps: list, idx: int) -> list:
        """Run the WHOLE idx chain inside one subshell redirected to the shared per-partition log/err, so a
        pre-qemu failure (e.g. checkpoint_conversion) is recorded there instead of vanishing to the
        terminal. Appends (>>): RunSinglePartitionCommand wipes the files once before the first idx, then
        every idx accumulates with a per-idx banner. use_stdio keeps the live stream (no redirect)."""
        if self.use_stdio:
            return steps
        log, err = self.get_log_file_address(), self.get_err_file_address()
        inner = " && ".join(s.strip() for s in steps)
        return [
            f'echo "===== qflex idx {idx}: stdout =====" >> {log}',
            f'echo "===== qflex idx {idx}: stderr =====" >> {err}',
            f"( {inner} ) >> {log} 2>> {err}",
        ]

    def cmd(self) -> str:
        self._assert_syncs_true()
        exp = self.experiment_context
        ratio_coefficient = exp.stat_interval_cycles
        total_cycles = ((exp.warming_ratio * ratio_coefficient)
                        + (exp.measurement_ratio * ratio_coefficient)) + 1
        idx = exp.idx
        partition_folder = exp.get_partition_folder()

        # Multi-fidelity phantom node: no Flexus, no checkpoint conversion — a plain parallel qemu
        # that loads the per-idx snapshot and advances at the phantom IPC, pairing with the master
        # over PDES until the master's idx exits. (The uniform phantom node falls through to the
        # master path below, only swapping the Flexus target for libphantomkraken.)
        if exp.all_phantom_cores and exp.multi_modal:
            parser = PhantomTimingArgParser(exp, idx, total_cycles, use_stdio=self.use_stdio)
            return self._with_shared_log([
                f"cd {partition_folder}",
                f'echo "===== qflex idx {idx} (phantom): starting in {partition_folder} ====="',
                "tick=$(($(date +%s%N) / 1000000))",
                f"{parser.qemu_binary} {parser.get_qemu_base_args()} < /dev/null",
                "tock=$(($(date +%s%N) / 1000000))",
                'echo "Elapsed: $((tock - tick)) ms"',
            ], idx)

        # Uniform phantom node (all_phantom_cores and not multi_modal): identical to the master
        # timing leaf — same vanilla binary, checkpoint_conversion, per-idx snapshot — only the
        # Flexus target differs (libphantomkraken, all cores phantom).
        if exp.all_phantom_cores:
            vanilla_parser = PhantomUniformTimingArgParser(exp, idx, total_cycles, use_stdio=self.use_stdio)
        else:
            vanilla_parser = VanillaQemuArgParser(exp, idx, total_cycles, use_stdio=self.use_stdio)

        # No gdb wrap. `; qrc=$?` + final `(exit $qrc)` keep mv/cp best-effort WITHOUT their
        # `|| true` swallowing a qemu crash — a segfaulted idx must fail the leaf, not mint
        # `.done` (which desyncs the PDES pair: this node advances while the peer waits forever).
        return self._with_shared_log([
            f"cd {partition_folder}",
            f'echo "===== qflex idx {idx}: starting in {partition_folder} ====="',
            f'rm -rf "snapshot_{idx}-flexus"',
            f"mkdir snapshot_{idx}-flexus",
            f"./checkpoint_conversion ./snapshot_{idx}.uarch ../../cfg/flexus_configuration.json ./snapshot_{idx}-flexus true",
            f'rm -rf "result_{idx}"',
            f'mkdir "result_{idx}"',
            "tick=$(($(date +%s%N) / 1000000))",
            f"../vanilla-qemu-system-aarch64 {vanilla_parser.get_qemu_base_args()} < /dev/null; qrc=$?",
            "tock=$(($(date +%s%N) / 1000000))",
            'echo "Elapsed: $((tock - tick)) ms"',
            f'mv *.log "result_{idx}/" 2>/dev/null || true',
            f'cp -f "result_{idx}/run-partition.log" '
            f'"{partition_folder}/run-partition.log" 2>/dev/null || true',
            '(exit $qrc)',
        ], idx)
