from typing import Annotated, Optional
import typer
from commands.config import create_experiment_context, ExperimentContext
from commands.workload import parse_groups
from .typer_base import TyperDataClassMeta

class ExperimentContextTyper(TyperDataClassMeta):
    """
    A class to hold metadata for Typer CLI dataclass inputs.
    """
    def __init__(self, name):
        super().__init__(name=name, init_function=self.experiment_context_typer)

    def experiment_context_typer(
        self,
        quantum_size_ns: Annotated[int, typer.Option(help="Quantum size for the simulator in nanoseconds.")],
        llc_size_per_tile_mb: Annotated[int, typer.Option(help="LLC size per tile in MB.")],
        parallel: Annotated[bool, typer.Option(help="Whether the simulation is parallel or not.")],
        # Machine and workload: read from the config.json of --prebuilt-workload when not given
        core_count: Annotated[Optional[int], typer.Option(help="Number of simulated cores. Default with --prebuilt-workload: from its config.json.")] = None,
        double_cores: Annotated[Optional[bool], typer.Option(help="Double the vCPUs for the client (phantom cores). Default with --prebuilt-workload: from its config.json.")] = None,
        network: Annotated[Optional[str], typer.Option(help="Network mode, either user or none. Default with --prebuilt-workload: from its config.json.")] = None,
        memory_gb: Annotated[Optional[int], typer.Option(help="Memory size for the VM in GB. Default with --prebuilt-workload: from its config.json.")] = None,
        workload_name: Annotated[Optional[str], typer.Option(help="Workload name. Default with --prebuilt-workload: from its config.json.")] = None,
        groups: Annotated[Optional[str], typer.Option(
            help='vCPU groups as name:cores[:ipc], e.g. "dc:0-7:0.6,ws:8-15:4.2,phantom:16-31:4" (IPC defaults: 2, phantom 4). '
                 "Default: from the config.json of --prebuilt-workload, else one group over all cores (plus phantom with --double-cores)."
        )] = None,
        population_seconds: Annotated[Optional[float], typer.Option(
            help="Population length of functional warming in simulated seconds. Default with --prebuilt-workload: its suggested population."
        )] = None,
        # experiment context with default values
        experiment_name: Annotated[str, typer.Option(
            help="Name of the experiment. Used for organizing output files."
        )] = 'default-experiment',
        image_name:  Annotated[str, typer.Option(
            help="Name of the image file to load."
        )] = 'root.qcow2',
        image_folder: Annotated[str, typer.Option(
            help="Folder where images are stored."
        )] = './images',
        unique: Annotated[bool, typer.Option(
            help="Whether to keep the experiment folder unique by adding a timestamp."
        )] = True,
        use_image_directly: Annotated[bool, typer.Option(
            help="Whether to use the image directly from the image folder or copy it to the experiment folder."
        )] = False,
        loadvm_name: Annotated[str, typer.Option(
            help="Name of the loadvm to use in QEMU, optional"
        )] = "",
        mounting_folder: Annotated[str, typer.Option(
            help="Mounting directory where the experiment folders will be created."
        )] = ".",
        check_period_quantum_coeff: Annotated[float, typer.Option(
            help="Coefficient to determine the check period based on quantum size. The value multiplied by quantum size to get check period."
        )] = 53.0,
        use_cd_rom: Annotated[bool, typer.Option(help="Whether to use a CD-ROM for initial setup.")]=False,
        machine_freq_ghz: Annotated[float, typer.Option(help="Machine frequency in GHz.")]=2.0,
        prebuilt_workload: Annotated[str, typer.Option(
            help="Start from a prebuilt workload, e.g. 16c/dc (see ./qflex image list; download it with ./qflex image pull). "
                 "The first command copies its disk into the experiment's run folder; later commands reuse it. --loadvm-name defaults to base."
        )]="",
    ):
        print("quantum_size_ns:", quantum_size_ns)
        if unique:
            print("Unique experiment is deprecated, will skip adding timestamp.")
            unique = False
            # TODO remove this in future
        experiment_context: ExperimentContext = create_experiment_context(
            experiment_name=experiment_name,
            image_name=image_name,
            quantum_size=quantum_size_ns,
            llc_size_per_tile_mb=llc_size_per_tile_mb,
            is_parallel=parallel,
            core_count=core_count,
            doubled_vcpu=double_cores,
            network=network,
            memory_gb=memory_gb,
            workload_name=workload_name,
            groups=parse_groups(groups) if groups else None,
            population_seconds=population_seconds,
            image_folder=image_folder,
            keep_experiment_unique=unique,
            use_image_directly=use_image_directly,
            loadvm_name=loadvm_name,
            mounting_folder=mounting_folder,
            check_period_quantum_coeff=check_period_quantum_coeff,
            use_cd_rom=use_cd_rom,
            machine_freq_ghz=machine_freq_ghz,
            prebuilt_workload=prebuilt_workload,
        )
        return experiment_context
