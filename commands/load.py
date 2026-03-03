from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser
from .jinja_loaders import LoadExpectLoader   


class Load(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 warmup_magic_phrase: str = "",
                 post_warmup_savevm_name: str = "load"):
        self.experiment_context = experiment_context
        self.warmup_magic_phrase = warmup_magic_phrase
        self.post_warmup_savevm_name = post_warmup_savevm_name
        self.qemu_common_parser = QemuCommonArgParser(experiment_context)

        # Generate load.expect script for automated savevm when magic phrase appears
        if self.warmup_magic_phrase != "":

            spawn_command = f""" ./qemu-system-aarch64 \
            {self.qemu_common_parser.get_qemu_base_args()} \
            {self.qemu_common_parser.get_qemu_telnet_args()} \
            {self.qemu_common_parser.quantum_args()}
            """

            self.load_expect_loader = LoadExpectLoader(
                experiment_context=experiment_context,
                trigger_phrase=self.warmup_magic_phrase,
                monitor_port=self.qemu_common_parser.monitor_port,
                spawn_command=spawn_command,
                save_vm_name=self.post_warmup_savevm_name
            )
            self.load_expect_script = self.load_expect_loader.load_parameters()




    def cmd(self) -> str:

        if self.warmup_magic_phrase == "":

            load_cmd = f"""
            ./qemu-system-aarch64 \
            {self.qemu_common_parser.get_qemu_base_args()} \
            {self.qemu_common_parser.quantum_args()}
            """
        
            # WormCacheQFlex/src/parameter.rss
            return [
                f"cd {self.experiment_context.get_experiment_folder_address()}/run", 
                load_cmd
            ]

        else:
            # Use the generated expect script for automated savevm
            return [
                f"cd {self.experiment_context.get_experiment_folder_address()}/run",
                f"expect {self.load_expect_script}"
            ]
            
