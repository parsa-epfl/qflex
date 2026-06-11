import os
from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser
from typing import List
from .jinja_loaders import wormloader, FlexusCheckpointConfigLoader, TimingLoader, FlexusScriptLoader
from .gem5_bbl_btb import record_init_warmed_bbl_btb_policy

class InitWarm(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 skip_generate_cfg: bool = False,
                 fallback_cycles: int | None = None,
                 collect_gem5_bbl_btb: bool = False):
        self.experiment_context = experiment_context
        self.simulation_context = self.experiment_context.simulation_context
        self.qemu_common_parser = QemuCommonArgParser(experiment_context)
        self.fallback_cycles = fallback_cycles
        self.collect_gem5_bbl_btb = collect_gem5_bbl_btb

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
    def cmd(self) -> str:
        if self.fallback_cycles is not None and self.fallback_cycles < 0:
            raise ValueError("fallback_cycles must be a non-negative integer")

        fallback_plugin_arg = ""
        if self.fallback_cycles is not None:
            fallback_plugin_arg = f",fallback_cycles={self.fallback_cycles}"

        collect_gem5_bbl_btb_arg = ""
        if self.collect_gem5_bbl_btb:
            collect_gem5_bbl_btb_arg = ",collect_gem5_bbl_btb=1"

        # TODO check if we need variables for the plugin
        init_cmd = f"""
        ./qemu-system-aarch64 \
        {self.qemu_common_parser.get_qemu_base_args()} \
        {self.qemu_common_parser.quantum_args()} \
        -plugin ../lib/libworm_cache.so,mode=pure_fill,prefix=init{fallback_plugin_arg}{collect_gem5_bbl_btb_arg}
        """


        return self.experiment_context.get_wormcache_build_commands() + [
            f"cd {self.experiment_context.get_experiment_folder_address()}/run",
            "ls",
            init_cmd
        ]

    def execute(self, to_stdio: bool = True, run_in_background: bool = False):
        result = super().execute(to_stdio=to_stdio, run_in_background=run_in_background)
        if not run_in_background and getattr(result, "returncode", 1) == 0:
            record_init_warmed_bbl_btb_policy(
                self.experiment_context,
                self.collect_gem5_bbl_btb,
            )
        return result







    

    
