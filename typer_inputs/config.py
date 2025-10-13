from typing import Annotated
import typer
from commands.config import create_experiment_context, ExperimentContext
from .typer_base import TyperDataClassMeta

class ExperimentContextTyper(TyperDataClassMeta):
    """
    A class to hold metadata for Typer CLI dataclass inputs.
    """
    def __init__(self, name):
        super().__init__(name=name, init_function=self.experiment_context_typer)

    def experiment_context_typer(
        self,
        core_count: Annotated[int, typer.Option(help="Number of CPU cores for the VM.")],
        double_cores: Annotated[bool, typer.Option(help="Double the number of CPU cores for the client.")],
        quantum_size_ns: Annotated[int, typer.Option(help="Quantum size for the simulator in nanoseconds.")],
        llc_size_per_tile_mb: Annotated[int, typer.Option(help="LLC size per tile in MB.")],
        parallel: Annotated[bool, typer.Option(help="Whether the simulation is parallel or not.")],
        network: Annotated[str, typer.Option(help="Network mode, either user or none")],
        memory_gb: Annotated[int, typer.Option(help="Memory size for the VM in GB.")],
        # Host section
        host_name:Annotated[str, typer.Option(help="Host name, used to create initial ipns file")],
        # Workload section
        # TODO check where workload name is used
        workload_name:Annotated[str, typer.Option(help="Workload name")],
        # TODO possibly move this to be only in fw?
        population_seconds: Annotated[float, typer.Option(
            help="Population size for the workload in seconds."
        )],
        sample_size: Annotated[int, typer.Option(
            help="Sample size for the workload."
        )],
        consolidated: Annotated[bool, typer.Option(
            help="Whether the workload is consolidated or not."
        )],
        primary_ipc: Annotated[float, typer.Option(
            help="Target IPC for primary workload."
        )],
        primary_core_start: Annotated[int, typer.Option(
            help="Starting core for primary workload."
        )],
        secondary_core_start: Annotated[int, typer.Option(
            help="Starting core for secondary workload. Only used if consolidated is True."
        )] = -1,
        secondary_ipc: Annotated[float, typer.Option(
            help="Target IPC for secondary workload. Only used if consolidated is True."
        )] = 0.0,
        phantom_cpu_ipc: Annotated[float, typer.Option(
            help="Target IPC for phantom CPU. This is used for the client in the same node. Only used in double core mode."
        )] = -1.0,
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
    ):
        print("quantum_size_ns:", quantum_size_ns)
        if unique:
            print("Unique experiment is deprecated, will skip adding timestamp.")
            unique = False
            # TODO remove this in future
        experiment_context: ExperimentContext = create_experiment_context(
            experiment_name=experiment_name,
            image_name=image_name,
            core_count=core_count,
            quantum_size=quantum_size_ns,
            doubled_vcpu=double_cores,
            llc_size_per_tile_mb=llc_size_per_tile_mb,
            is_parallel=parallel,
            network=network,
            memory_gb=memory_gb,
            # Host section:
            host_name=host_name,
            # Workload section:
            workload_name=workload_name,
            is_consolidated=consolidated,
            population_seconds=population_seconds,
            sample_size=sample_size,
            primary_core_start=primary_core_start,
            primary_ipc=primary_ipc,
            secondary_core_start=secondary_core_start,
            secondary_ipc=secondary_ipc,
            phantom_cpu_ipc=phantom_cpu_ipc,
            image_folder=image_folder,
            keep_experiment_unique=unique,
            use_image_directly=use_image_directly,
            loadvm_name=loadvm_name,
            mounting_folder=mounting_folder,
            check_period_quantum_coeff=check_period_quantum_coeff,
            use_cd_rom=use_cd_rom,
            machine_freq_ghz=machine_freq_ghz,
        )
        return experiment_context
