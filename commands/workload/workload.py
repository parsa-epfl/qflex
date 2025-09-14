from pydantic import BaseModel, Field


class IPCInfo(BaseModel):
    is_consolidated: bool = Field(description="Whether the workload is consolidated")
    primary_ipc: float = Field(description="Primary IPC value")
    # TODO check the document on the description of secondary_ipc and primary_ipc
    secondary_ipc: float = Field(description="Secondary IPC value for client")
    phantom_cpu_ipc: float = Field(description="Phantom CPU IPC value")
    # TODO add some checks for the * 1000000000 conversion
    population: int = Field(description="Population size for this workload")
    sample_size: int = Field(description="Sample size for the workload")

class CoreRange(BaseModel):
    primary_core_start: int = Field(description="Start of the primary core range")
    secondary_core_start: int = Field(description="Start of the secondary core range")

class Workload(BaseModel):
    name: str = Field(description="Name of the workload")
    IPC_info: IPCInfo = Field(description="IPC information for the workload")
    core_range: CoreRange = Field(description="Core range information for the workload")

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
    population: int,
    sample_size: int,
):
    ipc_info = IPCInfo(
        is_consolidated=is_consolidated,
        primary_ipc=primary_ipc,
        secondary_ipc=secondary_ipc,
        phantom_cpu_ipc=phantom_cpu_ipc,  # Default value, can be modified later
        population=population,
        sample_size=sample_size,
    )

    core_range = CoreRange(
        primary_core_start=primary_core_start,
        secondary_core_start=secondary_core_start,
    )

    return Workload(
        name=workload_name,
        IPC_info=ipc_info,
        core_range=core_range,
    )


