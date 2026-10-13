from pydantic import BaseModel, Field

# Starting IPC estimates, used in core_info.csv until `./qflex train` fits the IPC model.
DEFAULT_IPC = 2.0
DEFAULT_PHANTOM_IPC = 4.0
# Name of the group holding the client vCPUs added by --double-cores (simulated as phantom CPUs in Flexus).
PHANTOM = "phantom"


def parse_core_range(cores: str) -> list[int]:
    """ "0-7" -> [0, ..., 7]; "3" -> [3]."""
    first, _, last = cores.partition("-")
    return list(range(int(first), int(last or first) + 1))


class CoreGroup(BaseModel):
    name: str = Field(description="Workload running on these vCPUs, or phantom for the client vCPUs")
    cores: str = Field(description='Inclusive vCPU range "first-last"')
    ipc: float = Field(description="Starting IPC estimate of these vCPUs")

    def core_list(self) -> list[int]:
        return parse_core_range(self.cores)


class Workload(BaseModel):
    name: str = Field(description="Name of the workload")
    groups: list[CoreGroup] = Field(description="Partition of the vCPUs; more than one measured group means a consolidated workload")
    population: int | None = Field(default=None, description="Population length in ns, required by functional warming")

    def measured_groups(self) -> list[CoreGroup]:
        return [g for g in self.groups if g.name != PHANTOM]

    def phantom_ipc(self) -> float:
        return next((g.ipc for g in self.groups if g.name == PHANTOM), DEFAULT_PHANTOM_IPC)

    def ipc_per_vcpu(self) -> list[float]:
        ipc = {core: g.ipc for g in self.groups for core in g.core_list()}
        return [ipc[core] for core in sorted(ipc)]

    def core_groups_arg(self) -> str:
        """Measured groups in the --core-groups format of result and train, e.g. "0-7,8-15"."""
        return ",".join(g.cores for g in self.measured_groups())


def parse_groups(text: str) -> list[CoreGroup]:
    """ "dc:0-7:0.6,ws:8-15:4.2,phantom:16-31:4" -> groups; the IPC is optional."""
    groups = []
    for item in text.split(","):
        name, cores, *ipc = item.strip().split(":")
        default = DEFAULT_PHANTOM_IPC if name == PHANTOM else DEFAULT_IPC
        groups.append(CoreGroup(name=name, cores=cores, ipc=float(ipc[0]) if ipc else default))
    return groups


def default_groups(name: str, core_count: int, doubled_vcpu: bool) -> list[CoreGroup]:
    """One group over the measured cores, plus the phantom group with --double-cores."""
    groups = [CoreGroup(name=name, cores=f"0-{core_count - 1}", ipc=DEFAULT_IPC)]
    if doubled_vcpu:
        groups.append(CoreGroup(name=PHANTOM, cores=f"{core_count}-{2 * core_count - 1}", ipc=DEFAULT_PHANTOM_IPC))
    return groups


def validate_groups(groups: list[CoreGroup], core_count: int, doubled_vcpu: bool):
    measured = sorted(core for g in groups if g.name != PHANTOM for core in g.core_list())
    phantom = sorted(core for g in groups if g.name == PHANTOM for core in g.core_list())
    expected_phantom = list(range(core_count, 2 * core_count)) if doubled_vcpu else []
    described = ", ".join(f"{g.name}:{g.cores}" for g in groups)
    # The phantom group is checked first: a misplaced one would otherwise surface as a measured-core error.
    second_half = f"{core_count}-{2 * core_count - 1}"
    if phantom != expected_phantom:
        if not doubled_vcpu:
            raise ValueError(f"Error: a phantom group needs --double-cores (groups: {described}).")
        if not phantom:
            raise ValueError(f"Error: --double-cores needs a phantom group on the second half of the vCPUs, {PHANTOM}:{second_half} (groups: {described}).")
        raise ValueError(f"Error: the phantom group must be the second half of the vCPUs, {second_half} (groups: {described}).")
    if measured != list(range(core_count)):
        raise ValueError(f"Error: the measured groups must cover the cores 0-{core_count - 1} exactly once (groups: {described}).")
    if any(g.ipc <= 0 for g in groups):
        raise ValueError(f"Error: the IPC of every group must be positive ({described}).")


def create_workload(
    workload_name: str,
    groups: list[CoreGroup] | None,
    core_count: int,
    doubled_vcpu: bool,
    population_seconds: float | None,
) -> Workload:
    if groups is None:
        groups = default_groups(workload_name, core_count, doubled_vcpu)
    validate_groups(groups, core_count, doubled_vcpu)
    return Workload(
        name=workload_name,
        groups=groups,
        population=None if population_seconds is None else round(population_seconds * 1_000_000_000),
    )
