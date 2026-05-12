from commands import SimulationCommand
from .config import ExperimentContext
from commands.qemu import VanillaQemuArgParser


class RunIdxCommand(SimulationCommand):
    NEEDS_PDES_PEER_KILL = False

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
