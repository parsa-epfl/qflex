
import jinja2
from pathlib import Path
from ..config import ExperimentContext, get_capital_dict
from abc import ABC, abstractmethod

# Microarchitecture of the simulated cores: Arm Neoverse N1 (the neoverse-n1 row of experiment-scripts'
# cfg/configuration.csv). The L2 (--llc-size-per-tile-mb) and directory sizes come from the simulation context.
NEOVERSE_N1 = {
    "BTB_SET": 2048,
    "BTB_WAY": 3,
    "L1I_SET": 256,
    "L1I_WAY": 4,
    "L1D_SET": 256,
    "L1D_WAY": 4,
    "STLB_SET": 256,
    "STLB_WAY": 5,
    "SMS_PHT_SET": 1024,
    "SMS_PHT_WAY": 16,
}


def get_microarchitecture_context(experiment_context: ExperimentContext) -> dict:
    """
    Context shared by parameter.rs (WormCache), flexus_configuration.json (checkpoint conversion) and
    timing.cfg (Flexus), so that the three always describe the same caches, TLBs and predictors.
    """
    simulation = experiment_context.simulation_context
    return {
        **NEOVERSE_N1,
        "CORE_COUNT": simulation.core_count,
        "DOUBLED_VCPU": simulation.doubled_vcpu,
        "SLICE_COUNT": simulation.core_count,
        "L2_SET": simulation.l2_set,
        "L2_WAY": simulation.l2_way,
        "DIR_SET": simulation.directory_set,
        "DIR_WAY": simulation.directory_way,
        "MEM_CTL_COUNT": simulation.mem_controller_count,
        "MEM_CTL_POSITION": simulation.mem_controller_positions,
        # Worker threads for parallel checkpoint (de)serialization; must divide the simulated core count.
        "CHECKPOINT_POOL_SIZE": max(1, simulation.core_count // 4) if simulation.is_parallel else 1,
        # Flexus' phantom-cpu:estimated_ipc is an integer.
        "CLIENT_ESTIMATED_IPC": int(experiment_context.workload.IPC_info.phantom_cpu_ipc),
        # WormCache features (all on: full-system simulation with the MMU and the SMS prefetcher).
        "ENABLE_MMU": True,
        "SMS_PREFETCHING": True,
        "NO_MICROARCHITECTURE": False,
        "BYPASS_OS_SIMULATION": False,
    }


class ParameterLoader(ABC):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 parameter_template: str,
                 output_name: str,
                 out_folder: str):
        self.experiment_context = experiment_context
        self.parameter_template = parameter_template
        self.output_name = output_name
        self.folder = out_folder
    
    def load_parameters(self) -> str:
        jinja_env = jinja2.Environment(loader=jinja2.FileSystemLoader("./templates"), undefined=jinja2.StrictUndefined)
        template = jinja_env.get_template(self.parameter_template)
        context = self.get_context()
        parameter_file_path = f"{self.experiment_context.get_experiment_folder_address()}/{self.folder}/{self.output_name}"
        template.stream(context).dump(parameter_file_path)
        return parameter_file_path

    @abstractmethod
    def get_context(self) -> dict:
        # abstractmethod used to be get_capital_dict(self.experiment_context.simulation_context)
        raise NotImplementedError("Subclasses must implement get_context method")





