from commands import Executor
from .config import ExperimentContext
from commands.qemu import VanillaQemuArgParser


class RunIdxCommand(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 warming_ratio: int,
                 measurement_ratio: int,
                 use_stdio: bool = True):
        self.experiment_context = experiment_context
        self.detailed_warming_ratio = warming_ratio
        self.measurement_ratio = measurement_ratio
        self.use_stdio = use_stdio

    def get_err_file_address(self):
        return f"{self.experiment_context.get_partition_folder()}/err"

    def get_log_file_address(self):
        return f"{self.experiment_context.get_partition_folder()}/log"

    def cmd(self) -> str:
        # Build per-context derived state fresh — see Executor refactor notes.
        # TODO turn this into a param, for now each ratio represents 100000 cycles
        ratio_coefficient = 100000
        total_cycles = ((self.detailed_warming_ratio * ratio_coefficient)
                        + (self.measurement_ratio * ratio_coefficient)) + 1
        idx = self.experiment_context.idx
        vanilla_parser = VanillaQemuArgParser(self.experiment_context, idx, total_cycles,
                                              use_stdio=self.use_stdio)

        partition_folder = self.experiment_context.get_partition_folder()
        setup_commands = [
            f"cd {partition_folder}",
            f"echo running in partition folder {partition_folder}",
            f'rm -rf "snapshot_{idx}-flexus"',
            f"mkdir snapshot_{idx}-flexus",
            f"./checkpoint_conversion ./snapshot_{idx}.uarch ../../cfg/flexus_configuration.json ./snapshot_{idx}-flexus true",
        ]

        tick_command = " tick=$(($(date +%s%N) / 1000000)) "
        output = ""
        if not self.use_stdio:
            output = f"> {self.get_log_file_address()} 2> {self.get_err_file_address()}"
        timing_command = f"""
            gdb -batch -ex run -ex "python try: gdb.execute('bt')\nexcept: pass" -return-child-result --args ../vanilla-qemu-system-aarch64 \
            {vanilla_parser.get_qemu_base_args()} {output}
        """
        prints = []
        if not self.use_stdio:
            prints = [
                f"""echo "log is:""",
                f"""cat {self.get_log_file_address()}""",
                f"""echo "err is:""",
                f"""cat {self.get_err_file_address()}"""
            ]
        tock_command = " tock=$(($(date +%s%N) / 1000000)) "
        time_command = ' echo "Elapsed: $((tock - tick)) ms " '

        backup_commands = [
            f'rm -rf "result_{idx}"',
            f'mkdir "result_{idx}"',
            f'mv *.log "result_{idx}/"',
        ]
        return setup_commands + [
            tick_command,
            timing_command,
            tock_command,
            time_command,
        ] + prints + backup_commands
