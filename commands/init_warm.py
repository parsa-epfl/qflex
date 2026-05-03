import os
from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser, wrap_with_gdb
from typing import List
from .jinja_loaders import wormloader, FlexusCheckpointConfigLoader, TimingLoader, FlexusScriptLoader

class InitWarm(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 skip_generate_cfg: bool = False):
        self.experiment_context = experiment_context
        self.skip_generate_cfg = skip_generate_cfg

    def _generate_cfgs(self):
        """Render Jinja templates into the current experiment_context's cfg/scripts folders.
        Called from cmd() so it operates on the current experiment_context (works correctly
        across multi-experiment dispatch where the context can be mutated to a sub)."""
        experiment_folder = self.experiment_context.get_experiment_folder_address()
        worm_params_address = f"{experiment_folder}/cfg/parameter.rs"

        if not self.skip_generate_cfg:
            worm_parameter_loader = wormloader.WormConfigLoader(experiment_context=self.experiment_context)
            flexus_configuration_loader = FlexusCheckpointConfigLoader(experiment_context=self.experiment_context)
            timing_loader = TimingLoader(experiment_context=self.experiment_context)

            written_worm_params = worm_parameter_loader.load_parameters()
            assert worm_params_address == written_worm_params, "Worm parameter file path mismatch"
            flexus_configuration_loader.load_parameters()
            timing_loader.load_parameters()
        else:
            print("Skipping configuration generation as per user request. Please make sure all necessary configuration files are present in cfg folder.")
            assert os.path.exists(worm_params_address), "Worm parameter file does not exist, cannot skip generation."

        # TODO potential problem that the potential script is created at the init_warm stage change later and move to partition file
        flexus_script_loader = FlexusScriptLoader(experiment_context=self.experiment_context)
        flexus_script_loader.load_parameters()

        return worm_params_address

    def build_worm_cache(self, worm_params_address: str) -> List[str]:
        print("building workm cache")
        # TODO fix bug of not being able to recompile in docker, and let it be compilable and also make this step fully compile things, one file at least is not being removed after recompilation in exp folder
        experiment_folder = self.experiment_context.get_experiment_folder_address()
        return [
            f"cp {worm_params_address} {experiment_folder}/lib/WormCacheQFlex/src/parameter.rs",
            f"cd {experiment_folder}/lib/WormCacheQFlex",
            # TODO check if this needs to be debug
            "cargo build --release",
            f"cd {experiment_folder}/run",
            f"cp {experiment_folder}/lib/WormCacheQFlex/target/release/libworm_cache.so {experiment_folder}/lib/",
            f"cp {experiment_folder}/lib/WormCacheQFlex/target/release/checkpoint_conversion {experiment_folder}/bin/checkpoint_conversion",
            #  TODO check if all needed files are copied (check main file of replica plus necessary files declared in partition.py and result.py)
        ]

    def cmd(self) -> str:
        # Render the cfg files for THIS experiment_context (could be a sub after multi dispatch).
        worm_params_address = self._generate_cfgs()

        parser = QemuCommonArgParser(self.experiment_context)

        # TODO check if we need variables for the plugin
        init_cmd = wrap_with_gdb(
            f"./qemu-system-aarch64 {parser.get_qemu_base_args()} "
            f"-plugin ../lib/libworm_cache.so,mode=pure_fill,prefix=init",
            self.experiment_context.use_gdb,
        )

        return self.build_worm_cache(worm_params_address) + [
            f"cd {self.experiment_context.get_experiment_folder_address()}/run",
            "ls",
            init_cmd
        ]
