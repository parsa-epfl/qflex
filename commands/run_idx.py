from commands import SimulationCommand
from .config import ExperimentContext
from commands.qemu import VanillaQemuArgParser


class RunIdxCommand(SimulationCommand):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 use_stdio: bool = True):
        self.experiment_context = experiment_context
        self.use_stdio = use_stdio

    def get_err_file_address(self):
        return f"{self.experiment_context.get_partition_folder()}/err"

    def get_log_file_address(self):
        return f"{self.experiment_context.get_partition_folder()}/log"

    def cmd(self) -> str:
        self._assert_syncs_true()
        # Build per-context derived state fresh — see Executor refactor notes.
        # TODO turn this into a param, for now each ratio represents 100000 cycles
        ratio_coefficient = 100000
        total_cycles = ((self.experiment_context.warming_ratio * ratio_coefficient)
                        + (self.experiment_context.measurement_ratio * ratio_coefficient)) + 1
        idx = self.experiment_context.idx
        vanilla_parser = VanillaQemuArgParser(self.experiment_context, idx, total_cycles,
                                              use_stdio=self.use_stdio)

        partition_folder = self.experiment_context.get_partition_folder()
        # result_<idx>/ is created BEFORE qemu runs so the trailing block's mv
        # has somewhere to land even when peer-kill SIGKILLs qemu mid-run.
        setup_commands = [
            f"cd {partition_folder}",
            f'echo "===== qflex idx {idx}: starting in {partition_folder} ====="',
            f'rm -rf "snapshot_{idx}-flexus"',
            f"mkdir snapshot_{idx}-flexus",
            f"./checkpoint_conversion ./snapshot_{idx}.uarch ../../cfg/flexus_configuration.json ./snapshot_{idx}-flexus true",
            f'rm -rf "result_{idx}"',
            f'mkdir "result_{idx}"',
        ]

        tick_command = " tick=$(($(date +%s%N) / 1000000)) "
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
                f'echo "===== qflex idx {idx}: gdb stdout =====" >> {self.get_log_file_address()}',
                f'echo "===== qflex idx {idx}: gdb stderr =====" >> {self.get_err_file_address()}',
            ]

        # One trailing block: run qemu, capture rc immediately, do the per-idx
        # backup unconditionally, then exit with qemu's actual rc. The `{ ... }`
        # group sequences statements without `&&`, so mv/cp run even on
        # SIGKILL (137 from gdb's -return-child-result when peer-kill fires).
        # Faithful rc propagation is required so the leaf's `killed_by_peer`
        # branch in commands/executer.py still distinguishes peer-killed from
        # actual qemu crashes.
        timing_and_backup = f"""{{
            gdb -batch -ex run -ex "python try: gdb.execute('bt')\nexcept: pass" -return-child-result --args ../vanilla-qemu-system-aarch64 \
            {vanilla_parser.get_qemu_base_args()} {output} < /dev/null
            __qflex_qemu_rc=$?
            tock=$(($(date +%s%N) / 1000000))
            echo "Elapsed: $((tock - tick)) ms"
            mv *.log "result_{idx}/" 2>/dev/null || true
            cp -f "result_{idx}/run-partition.log" "{partition_folder}/run-partition.log" 2>/dev/null || true
            exit $__qflex_qemu_rc
        }}"""
        return setup_commands + log_banner + [
            tick_command,
            timing_and_backup,
        ]
