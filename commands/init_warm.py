import os
import shutil
from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser
from typing import List
from .jinja_loaders import wormloader, FlexusCheckpointConfigLoader, TimingLoader, FlexusScriptLoader

QFLEX_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def cargo_command() -> str:
    """
    cargo of the environment the stack was built in (out/build-env, written by `just`): WormCache is built for
    every experiment and must use the same toolchain as the QEMUs it plugs into.
    """
    marker = f"{QFLEX_ROOT}/out/build-env"
    if not os.path.isfile(marker) or open(marker).read().strip() != "nix" or os.environ.get("IN_NIX_SHELL"):
        return "cargo"
    if shutil.which("nix") is None:
        raise FileNotFoundError(f"Error: QFlex was built with Nix ({marker}), but `nix` is not in PATH to build WormCache.")
    # Pure shell, as for building the stack: only the flake's toolchain; HOME is kept for cargo's crate cache.
    return f"nix develop -i --keep HOME {QFLEX_ROOT} --command cargo"


class InitWarm(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 skip_generate_cfg: bool = False):
        self.experiment_context = experiment_context
        self.simulation_context = self.experiment_context.simulation_context
        self.qemu_common_parser = QemuCommonArgParser(experiment_context)

        experiment_folder = self.experiment_context.get_experiment_folder_address()
        self.worm_params_address = f"{experiment_folder}/cfg/parameter.rs"
        if not skip_generate_cfg:
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
            assert self.worm_params_address == self.worm_params, "Worm parameter file path mismatch"
            self.flexus_config = self.flexus_configuration_loader.load_parameters()
            self.timing_config = self.timing_loader.load_parameters()
        else:
            print("Skipping configuration generation as per user request. Please make sure all necessary configuration files are present in cfg folder.")
            assert os.path.exists(self.worm_params_address), "Worm parameter file does not exist, cannot skip generation."

        # TODO potential problem that the potential script is created at the init_warm stage change later and move to partition file
        self.flexus_script_loader = FlexusScriptLoader(
            experiment_context=experiment_context,
        )
        self.flexus_script = self.flexus_script_loader.load_parameters()




    def build_worm_cache(self) -> List[str]:
        experiment_folder = self.experiment_context.get_experiment_folder_address()
        return [
            f"cp {self.worm_params_address} {experiment_folder}/lib/WormCache/src/parameter.rs",
            f"cd {experiment_folder}/lib/WormCache",
            # TODO check if this needs to be debug
            f"{cargo_command()} build --release",
            f"cd {experiment_folder}/run",
            f"cp {experiment_folder}/lib/WormCache/target/release/libworm_cache.so {self.experiment_context.get_experiment_folder_address()}/lib/",
            f"cp {experiment_folder}/lib/WormCache/target/release/checkpoint_conversion {self.experiment_context.get_experiment_folder_address()}/bin/checkpoint_conversion",
            #  TODO check if all needed files are copied (check main file of replica plus necessary files declared in partition.py and result.py)
        ]
    
    def cmd(self) -> str | list[str]:



        # TODO check if we need variables for the plugin
        init_cmd = f"""
        ./qemu-system-aarch64 \
        {self.qemu_common_parser.get_qemu_base_args()} \
        {self.qemu_common_parser.quantum_args()} \
        -plugin ../lib/libworm_cache.so,mode=pure_fill,prefix=init
        """


        return self.build_worm_cache() + [
            f"cd {self.experiment_context.get_experiment_folder_address()}/run",
            "ls",
            init_cmd
        ]







    

    



