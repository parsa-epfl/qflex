from pydantic import BaseModel, Field


class Context(BaseModel):
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
    phantom_cpu_ipc: float = Field(description="phantom CPU IPC")
    memory: int = Field(description="memory size in GB")
    qemu_nic: str = Field(description="type of NIC to use in QEMU")
    quantum_size: int = Field(description="quantum size for the simulator in nanoseconds")

def create_context(
    core_count: int,
    quantum_size: int,
    doubled_vcpu: bool,
    llc: int,
    phantom_cpu_ipc: float = 2.0,
    network: str = "e1000",
    memory_gb: int = 16,
) -> Context:
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

    Context(
        core_count=core_count,
        doubled_vcpu=doubled_vcpu,
        l2_set=core_count * llc * 1024 * 1024 // (64*l2_way),
        l2_way=l2_way,
        directory_set=16,
        directory_way=512 * core_count,
        mem_controller_count=memory_controller_count,
        mem_controller_positions=memory_controller_positions,
        phantom_cpu_ipc=phantom_cpu_ipc,
        memory=memory_gb,
        qemu_nic=network,
        quantum_size=quantum_size,
    )



def get_capital_dict(variable: BaseModel):
    """
    Converts a list of variable names to a dictionary with uppercase keys.
    """
    return {var.upper(): getattr(variable, var) for var in variable.__fields__.keys()}
