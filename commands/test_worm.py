import os
from typing import List

from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser
from .jinja_loaders import wormloader, FlexusCheckpointConfigLoader, TimingLoader, FlexusScriptLoader


class TestWorm(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 skip_generate_cfg: bool = False,
                 branch_trace: bool = False,
                 collect_gem5_bbl_btb: bool = False,
                 monitor_port: int | None = None):
        self.experiment_context = experiment_context
        self.simulation_context = self.experiment_context.simulation_context
        self.qemu_common_parser = QemuCommonArgParser(experiment_context)
        self.branch_trace = branch_trace
        self.collect_gem5_bbl_btb = collect_gem5_bbl_btb
        self.monitor_port = monitor_port

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

        self.flexus_script_loader = FlexusScriptLoader(
            experiment_context=experiment_context,
        )
        self.flexus_script = self.flexus_script_loader.load_parameters()

    def build_worm_cache(self) -> List[str]:
        experiment_folder = self.experiment_context.get_experiment_folder_address()
        return [
            f"cp {self.worm_params_address} {experiment_folder}/lib/WormCacheQFlex/src/parameter.rs",
            f"cd {experiment_folder}/lib/WormCacheQFlex",
            "cargo build --release",
            f"cd {experiment_folder}/run",
            f"cp {experiment_folder}/lib/WormCacheQFlex/target/release/libworm_cache.so {self.experiment_context.get_experiment_folder_address()}/lib/",
            f"cp {experiment_folder}/lib/WormCacheQFlex/target/release/checkpoint_conversion {self.experiment_context.get_experiment_folder_address()}/bin/checkpoint_conversion",
        ]

    def cmd(self) -> str:
        branch_trace_opt = ""
        if self.branch_trace:
            branch_trace_opt = ",branch_trace=1"
        collect_gem5_bbl_btb_opt = ""
        if self.collect_gem5_bbl_btb:
            collect_gem5_bbl_btb_opt = ",collect_gem5_bbl_btb=1"
        test_cmd = f"""
        ./qemu-system-aarch64 \
        {self.qemu_common_parser.get_qemu_base_args(monitor_port=self.monitor_port)} \
        {self.qemu_common_parser.quantum_args()} \
        -plugin ../lib/libworm_cache.so,mode=normal{branch_trace_opt}{collect_gem5_bbl_btb_opt}
        """

        return self.build_worm_cache() + [
            f"cd {self.experiment_context.get_experiment_folder_address()}/run",
            "ls",
            test_cmd
        ]
