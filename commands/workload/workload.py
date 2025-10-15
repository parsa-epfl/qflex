from pydantic import BaseModel, Field


class IPCInfo(BaseModel):
    is_consolidated: bool = Field(description="Whether the workload is consolidated")
    primary_ipc: float = Field(description="Primary IPC value")
    # TODO check the document on the description of secondary_ipc and primary_ipc
    secondary_ipc: float = Field(description="Secondary IPC value for client")
    phantom_cpu_ipc: float = Field(description="Phantom CPU IPC value")
    # TODO see if machine frequency should be moved to simulation config
    machine_freq_ghz: float = Field(2.0, description="Machine frequency in GHz")
    min_ipc: float = Field(description="Minimum IPC value observed")
    max_ipc: float = Field(description="Maximum IPC value observed")
    scaling_factor: float = Field(description="Scaling factor based on max IPC and machine frequency")
    

class CoreRange(BaseModel):
    primary_core_start: int = Field(description="Start of the primary core range")
    secondary_core_start: int = Field(description="Start of the secondary core range")

class Workload(BaseModel):
    name: str = Field(description="Name of the workload")
    IPC_info: IPCInfo = Field(description="IPC information for the workload")
    core_range: CoreRange = Field(description="Core range information for the workload")
    # TODO add some checks for the * 1000000000 conversion
    population: int = Field(description="Population size for this workload in ns.")

def get_ipc_info(
    is_consolidated: bool,
    primary_ipc: float,
    secondary_ipc: float,
    phantom_cpu_ipc: float,
    machine_freq_ghz: float,
) -> IPCInfo:
    min_ipc = min(primary_ipc, secondary_ipc) if is_consolidated else primary_ipc
    max_ipc = max(primary_ipc, secondary_ipc) if is_consolidated else primary_ipc
    scaling_factor = max_ipc / (round(primary_ipc * machine_freq_ghz, 2))
    return IPCInfo(
        is_consolidated=is_consolidated,
        primary_ipc=primary_ipc,
        secondary_ipc=secondary_ipc,
        phantom_cpu_ipc=phantom_cpu_ipc,
        machine_freq_ghz=machine_freq_ghz,
        min_ipc=min_ipc,
        max_ipc=max_ipc,
        scaling_factor=scaling_factor
    )
    

def create_workload(
    workload_name: str,
    # CoreRange section
    primary_core_start: int,
    secondary_core_start: int,
    # IPCInfo section
    is_consolidated: bool,
    primary_ipc: float,
    secondary_ipc: float,
    phantom_cpu_ipc: float,
    population_seconds: float,
    machine_freq_ghz: float = 2.0,  # Default frequency, can be modified later
):
    ipc_info = get_ipc_info(
        is_consolidated=is_consolidated,
        primary_ipc=primary_ipc,
        secondary_ipc=secondary_ipc,
        phantom_cpu_ipc=phantom_cpu_ipc,
        machine_freq_ghz=machine_freq_ghz,
    )

    core_range = CoreRange(
        primary_core_start=primary_core_start,
        secondary_core_start=secondary_core_start,
    )

    # TODO add checks to make sure the length of sample_intervals is bigger than quantum and the check period
    return Workload(
        name=workload_name,
        IPC_info=ipc_info,
        core_range=core_range,
        population=population_seconds * 1000000000,
    )


