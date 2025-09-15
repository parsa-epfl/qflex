from pydantic import BaseModel, Field
from host import Host, SMTHost, HOSTS, HostType
from workload import Workload, create_workload
import os
import pandas

# TODO double check all the parameters and their descriptions
# TODO check all the variables to match with the variables in templates
# For anyone checking this with old scripts, all worklaod, and core information is combined into the cli, as it's part of the CLI paramers now


# TODO move simulation context to a separate folder
class SimulationContext(BaseModel):
    """
    Context model for holding configuration variables.
    """
    core_count: int = Field(description="Number of CPU cores to use")
    doubled_vcpu: bool = Field(description="Whether to double the core count for the client application or not")
    l2_set: int = Field(description="number of sets in the L2 cache")
    l2_way: int = Field(description="number of ways in the L2 cache")
    directory_set: int = Field(description="number of sets in the directory cache")
    directory_way: int = Field(description="number of ways in the directory cache")
    mem_controller_count: int = Field(description="number of memory controllers")
    mem_controller_positions: str = Field(description="positions of memory controllers")
    memory: int = Field(description="memory size in GB")
    qemu_nic: str = Field(description="type of NIC to use in QEMU")
    quantum_size: int = Field(description="quantum size for the simulator in nanoseconds")
    is_parallel: bool = Field(default=True, description="whether the simulation is parallel or not")

def create_simulation_context(
    core_count: int,
    quantum_size: int,
    doubled_vcpu: bool,
    llc_size_per_tile_mb: int,
    is_parallel: bool,
    network: str,
    memory_gb: int,
) -> SimulationContext:
    l2_way = 16
    if core_count == 64:
        memory_controller_count = 8
        memory_controller_positions = "8,15,24,31,32,40,47,56"
    elif core_count == 16:
        memory_controller_count = 2
        memory_controller_positions = "8,15"
    elif core_count <= 4:
        memory_controller_count = 1
        memory_controller_positions = "0"
    else:
        raise ValueError("Unsupported core count")

    return SimulationContext(
        core_count=core_count,
        doubled_vcpu=doubled_vcpu,
        l2_set=core_count * llc_size_per_tile_mb * 1024 * 1024 // (64*l2_way),
        l2_way=l2_way,
        directory_set=16,
        directory_way=512 * core_count,
        mem_controller_count=memory_controller_count,
        mem_controller_positions=memory_controller_positions,
        memory=memory_gb,
        qemu_nic=network,
        quantum_size=quantum_size,
        is_parallel=is_parallel,
    )

class IPNSInfo(BaseModel):
    core_index: int = Field(description="Index of the core")
    ipns: float = Field(description="IPNs for the core")

class ExperimentContext(BaseModel):
    """
    Context model for holding experiment configuration variables.
    """
    experiment_name: str = Field(description="Name of the experiment")
    # TODO move image name to the workload section
    image_name: str = Field(description="Name of the image file to use")
    simulation_context: SimulationContext = Field(description="Simulation context containing detailed configuration")
    host: Host | SMTHost = Field(description="Host configuration")
    workload: Workload = Field(description="Workload configuration")
    working_directory: str = Field(default=".", description="Base working directory of qflex. use for shared folders")


    def get_experiment_folder_address(self) -> str:
        """
        Returns the full path to the experiment folder. Will have subfolders like bin, cfg, flags, lib, run, scripts for the specific experiment.
        """
        return f'{self.working_directory}/experiments/{self.experiment_name}'

    def set_up_folders(self):
        for subfolder in ["bin", "cfg", "flags", "lib", "run", "scripts"]:
            os.makedirs(f"{self.get_experiment_folder_address()}/{subfolder}", exist_ok=False)

    def get_image_address(self) -> str:
        return self.get_experiment_folder_address() + f'/{self.image_name}'
    
    def get_ipns_per_core(self) -> list[IPNSInfo]:

        # TODO This part looks messy to me, we need to revisit it later 
        is_consolidated = self.workload.IPC_info.is_consolidated
        has_client = self.simulation_context.doubled_vcpu
        core_list = self.host.get_core_sequence_as_list()
        results: list[IPNSInfo] = []
        
        core_count = self.simulation_context.core_count
        if not is_consolidated:
            for core_idx in range(core_count):
                results.append(IPNSInfo(core_index=core_list[core_idx], ipns=self.workload.IPC_info.primary_ipc))
        else:
            primary_core_start = self.workload.core_range.primary_core_start
            secondary_core_start = self.workload.core_range.secondary_core_start
            # TODO check this with shanqing, changed this so you can give core numbers that are actually used unlike the og script
            for core_idx in range(primary_core_start, secondary_core_start):
                results.append(IPNSInfo(core_index=core_list[core_idx], ipns=self.workload.IPC_info.primary_ipc))
            for core_idx in range(secondary_core_start, core_count):
                results.append(IPNSInfo(core_index=core_list[core_idx], ipns=self.workload.IPC_info.secondary_ipc))
            
        if has_client:
            for core_idx in range (core_count, core_count * 2):
                results.append(IPNSInfo(core_index=core_list[core_idx], ipns=self.workload.IPC_info.phantom_cpu_ipc))

        return results
    
    def get_ipns_csv(self, target) -> str:
        # TODO check on this as well as we talked about removing the dependancy between host and target
        df = pandas.DataFrame([[ipns_info.ipns, ipns_info] for ipns_info in self.get_ipns_per_core()], columns=["ipns", "affinity_core_idx"])
        df.to_csv(target, index=False)


        


def create_experiment_context(
    experiment_name: str,
    image_name: str,
    core_count: int,
    quantum_size: int,
    doubled_vcpu: bool,
    llc_size_per_tile_mb: int,
    is_parallel: bool,
    network: str,
    memory_gb: int,
    # Host section:
    host_name: str,
    # Workload section:
    workload_name: str,
    primary_core_start: int,
    secondary_core_start: int,
    is_consolidated: bool,
    primary_ipc: float,
    secondary_ipc: float,
    population: int,
    sample_size: int,
    phantom_cpu_ipc: float,

) -> ExperimentContext:
    simulation_context = create_simulation_context(
        core_count=core_count,
        quantum_size=quantum_size,
        doubled_vcpu=doubled_vcpu,
        llc_size_per_tile_mb=llc_size_per_tile_mb,
        is_parallel=is_parallel,
        network=network,
        memory_gb=memory_gb,
    )

    host_type = HostType[host_name.upper()]
    host = HOSTS[host_type]

    workload = create_workload(
        workload_name=workload_name,
        primary_core_start=primary_core_start,
        secondary_core_start=secondary_core_start,
        is_consolidated=is_consolidated,
        primary_ipc=primary_ipc,
        secondary_ipc=secondary_ipc,
        phantom_cpu_ipc=phantom_cpu_ipc,
        population=population,
        sample_size=sample_size,
    )


    e = ExperimentContext(
        experiment_name=experiment_name,
        image_name=image_name,
        simulation_context=simulation_context,
        host=host,
        workload=workload,
        working_directory=os.getcwd(),
    )

    e.set_up_folders()

    return e
    


def get_capital_dict(variable: BaseModel):
    """
    Converts a list of variable names to a dictionary with uppercase keys.
    """
    return {var.upper(): getattr(variable, var) for var in variable.__fields__.keys()}
