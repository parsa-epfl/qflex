from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser
from typing import List
from .jinja_loaders import wormloader, FlexusCheckpointConfigLoader, TimingLoader


class InitWarm(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext,):
        self.experiment_context = experiment_context
        self.simulation_context = self.experiment_context.simulation_context
        self.qemu_common_parser = QemuCommonArgParser(experiment_context)
        self.worm_parameter_loader = wormloader.WormConfigLoader(
            experiment_context=experiment_context
        )
        self.flexus_configuration_loader = FlexusCheckpointConfigLoader(
            experiment_context=experiment_context
        )
        self.timing_loader = TimingLoader(
            experiment_context=experiment_context
        )

        self.worm_params = self.worm_parameter_loader.load_parameters()
        self.flexus_config = self.flexus_configuration_loader.load_parameters()
        self.timing_config = self.timing_loader.load_parameters()


    def build_worm_cache(self) -> List[str]:
        cwd = self.experiment_context.working_directory
        return [
            f"cp {self.worm_params} {cwd}/WormCache/src/parameter.rs",
            f"cd {cwd}/WormCache",
            # TODO check if this needs to be debug
            "cargo build --release",
            f"cd {cwd}",
            f"cp {cwd}/WormCache/target/release/libworm_cache.so {self.experiment_context.get_experiment_folder_address()}/lib/", 
        ]
    
    def cmd(self) -> str:



        # TODO check if we need variables for the plugin
        init_cmd = f"""
        ./qemu-system-aarch64 \
        {self.qemu_common_parser.get_qemu_base_args()} \
        {self.qemu_common_parser.quantum_args()} \
        -plugin ../lib/libworm_cache.so,mode=pure_fill,prefix=init
        """
        print(init_cmd)


        return self.build_worm_cache() + [
            f"cd {self.experiment_context.get_experiment_folder_address()}/run",
            "ls",
            init_cmd
        ]







    

    



