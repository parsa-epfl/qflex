from pydantic import BaseModel, Field
import os
import pandas

from .host import Host, SMTHost, HOSTS, HostType
from .workload import Workload, create_workload
import datetime

# TODO double check all the parameters and their descriptions
# TODO check all the variables to match with the variables in templates
# For anyone checking this with old scripts, all worklaod, and core information is combined into the cli, as it's part of the CLI paramers now

def get_experiment_folder_address(
    mounting_folder: str,
    experiment_name: str
) -> str:
    # check if working directory exists
    if not os.path.isdir(mounting_folder):
        raise ValueError(f"Working directory {mounting_folder} does not exist.")
    # create experiments if it doesn't exist
    if not os.path.isdir(f'{mounting_folder}/experiments'):
        os.makedirs(f'{mounting_folder}/experiments', exist_ok=False)
    path = f'{mounting_folder}/experiments/{experiment_name}'
    if not os.path.isdir(path):
        os.makedirs(path, exist_ok=False)
    return os.path.abspath(path)

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
    memory_gb: int = Field(description="memory size in GB")
    qemu_nic: str = Field(description="type of NIC to use in QEMU")
    quantum_size: int = Field(description="quantum size for the simulator in nanoseconds")
    is_parallel: bool = Field(default=True, description="whether the simulation is parallel or not")
    check_period_quantum_coeff: float = Field(default=53.0, description="Coefficient to determine the check period based on quantum size")
    use_cd_rom: bool = Field(default=False, description="Whether to use a CD-ROM for initial setup.")

def create_simulation_context(
    core_count: int,
    quantum_size: int,
    doubled_vcpu: bool,
    llc_size_per_tile_mb: int,
    is_parallel: bool,
    network: str,
    memory_gb: int,
    check_period_quantum_coeff: float=53.0,
    use_cd_rom: bool=False,
) -> SimulationContext:
    l2_way = 16
    if core_count == 64:
        memory_controller_count = 8
        memory_controller_positions = "8,15,24,31,32,40,47,56"
    elif core_count == 16:
        memory_controller_count = 2
        memory_controller_positions = "8,15"
    # TODO check the memory for 8 and the other values
    elif core_count <= 8:
        memory_controller_count = 1
        memory_controller_positions = "0"
    else:
        raise ValueError("Unsupported core count")

    if 'none' == network.strip().lower():
        network = "-nic none"
    elif 'user' == network.strip().lower():
        network = "-nic user,model=virtio-net-pci"
    else:
        raise ValueError("Unsupported network type. Supported types are 'none' and 'user'.")
    return SimulationContext(
        core_count=core_count,
        doubled_vcpu=doubled_vcpu,
        l2_set=core_count * llc_size_per_tile_mb * 1024 * 1024 // (64*l2_way),
        l2_way=l2_way,
        directory_set=512 * core_count,
        directory_way=16,
        mem_controller_count=memory_controller_count,
        mem_controller_positions=memory_controller_positions,
        memory_gb=memory_gb,
        qemu_nic=network,
        quantum_size=quantum_size,
        is_parallel=is_parallel,
        check_period_quantum_coeff=check_period_quantum_coeff,
        use_cd_rom=use_cd_rom,
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
    image_folder: str = Field(description="Address of the image to use")
    image_name: str = Field(description="Name of the image to use")
    simulation_context: SimulationContext = Field(description="Simulation context containing detailed configuration")
    host: Host | SMTHost = Field(description="Host configuration")
    workload: Workload = Field(description="Workload configuration")
    mounting_folder: str = Field(default=".", description="Base working directory of qflex. use for shared folders")
    keep_experiment_unique: bool = Field(default=True, description="Whether to keep the experiment folder unique by adding a timestamp")
    use_image_directly: bool = Field(default=False, description="Whether to use the image directly from image folder instead of copying it to experiments folder")
    loadvm_name: str = Field(default="", description="Name of the loadvm to use in QEMU, optional")
    image_address: str = Field(default="", description="Full address of the image to use. Set up during initialization based on other parameters.")
    include_affinity: bool = Field(default=False, description="Whether or not generate affinity index in core_info.csv.")

    def get_mounting_folder(self) -> str:
        return self.mounting_folder

    def get_experiment_folder_address(self) -> str:
        """
        Returns the full path to the experiment folder. Will have subfolders like run, bin, cfg, flags, lib, run, scripts for the specific experiment.
        """
        # TODO add some more document on how the folder structure works and why this is good that these folders get repeated for each experiment as it keeps them isolated and easy to copy and move
        # TODO clean this up later, but done this for when need to make a directory but don't have the full context
        return get_experiment_folder_address(self.get_mounting_folder(), self.experiment_name)

    def get_local_image_address(self) -> str:
        return self.image_address
    def get_vanila_qemu_build_folder(self) -> str:
        return f'{self.get_experiment_folder_address()}/qemu-saved'
    def get_pflex_qemu_build_folder(self) -> str:
        return f'{self.get_experiment_folder_address()}/parallel-qemu-saved'
    
    def set_up_image(self):

        if self.use_image_directly:
            self.image_address = f"{self.image_folder}/{self.image_name}"
        else:
            # TODO add some checks for this
            # Check if base image exists in root folder
            self.image_address = f"{self.image_folder}/experiments/{self.experiment_name}/{self.image_name}"
            experiment_folder_for_images_exists = os.path.exists(self.get_experiment_folder_address())
            experimage_image_exists = os.path.exists(self.get_local_image_address())

            if not experiment_folder_for_images_exists:
                os.makedirs(self.get_experiment_folder_address(), exist_ok=not self.keep_experiment_unique)

            if experimage_image_exists:
                print(f"Experiment image {self.get_local_image_address()} already exists.")
                
            else:
                # The following is done as images can get big and this way they can be mounted on storage with more space if needed
                # copy the base image based on the experiment name then link it on experiment folder
                print("Creating experiment folder and copying base image...")
                # create folder in image folder
                if not os.path.exists(f"{self.image_folder}/experiments"):
                    os.makedirs(f"{self.image_folder}/experiments", exist_ok=False)
                os.makedirs(f"{self.image_folder}/experiments/{self.experiment_name}", exist_ok=not self.keep_experiment_unique)
                print("created folder in images folder for this experiment, copying image...")
                # Copy file to the new folder
                print(f"cp {self.image_folder}/{self.image_name} {self.image_folder}/experiments/{self.experiment_name}/{self.image_name}")
                os.system(f"cp -u {self.image_folder}/{self.image_name} {self.image_folder}/experiments/{self.experiment_name}/{self.image_name}")
                print("copied image, creating symlink...")
                # Create a symlink to the new image in the experiment folder
                os.symlink(f"{self.image_folder}/experiments/{self.experiment_name}/{self.image_name}", self.get_local_image_address())
                print(f"Linked image to")

            

    def set_up_folders(self):
        # TODO go through all the files being copied with shanqing

        if not os.path.exists(f"{self.get_mounting_folder()}/experiments"):
            os.makedirs(f"{self.get_mounting_folder()}/experiments", exist_ok=False)
        if not os.path.exists(f"{self.get_mounting_folder()}/images"):
            os.makedirs(f"{self.get_mounting_folder()}/images", exist_ok=False)
        if not os.path.exists(self.get_experiment_folder_address()):
            os.makedirs(self.get_experiment_folder_address(), exist_ok=False)
        if not os.path.exists(f"{self.image_folder}"):
            os.makedirs(f"{self.image_folder}")

        self.set_up_image()


        for subfolder in ["bin", "cfg", "flags", "lib", "run", "scripts", "images"]:
            os.makedirs(f"{self.get_experiment_folder_address()}/{subfolder}", exist_ok=not self.keep_experiment_unique)
        self.get_ipns_csv()

        if not os.path.exists(f"{self.get_experiment_folder_address()}/run/{self.image_name}"):
            if not os.path.exists(self.get_local_image_address()):
                raise FileNotFoundError(f"Error: Image file {self.get_local_image_address()} not found.")
            # TODO see why sys link could fail here
            # TODO see if we even need this image
            os.system(f"rm -f {self.get_experiment_folder_address()}/run/{self.image_name}")
            os.symlink(self.get_local_image_address(), f"{self.get_experiment_folder_address()}/run/{self.image_name}")

        root_sls = [
           "partition.py",
           "result.py"
        ]
        for file in root_sls:
            os.system(f"cp -u ./{file} {self.get_experiment_folder_address()}/{file}")
            print(f"moved {file} to experiment folder")


        # check that both build/qemu-system-aarch64 exists in run folder plus efi-virtio.rom
        # link all of them
        # TODO check if rom and bios files can be linked from parallel-qemu-saved when using qemu
        # TODO check why files are being turned into bz2
        
        run_files = [
            "./parallel-qemu-saved/build/qemu-system-aarch64", 
            # TODO if we ever decide to change EFI and bios, this needs to change
            "./QEMU_EFI.fd", 
            "./qemu-saved/build/qemu-system-aarch64",
            "./parallel-qemu-saved/pc-bios/efi-virtio.rom",
            "./qemu-img",
            "debug.cfg",
        ]
        for f in run_files:

            if "parallel-qemu-saved/" in f:
                # Link as the name of the file to run folder
                link_address = f"{self.get_experiment_folder_address()}/run/{f.split('/')[-1]}"
            elif "qemu-saved/" in f:
                # Link as the name of the file to run folder with
                link_address = f"{self.get_experiment_folder_address()}/run/vanilla-{f.split('/')[-1]}"
            else:
                link_address = f"{self.get_experiment_folder_address()}/run/{f.split('/')[-1]}"

            if not os.path.exists(link_address):
                os.system(f"cp -u {f} {link_address}")
        # TODO turn WormCacheQFlex address into a parameter
        # Copy WormCacheQFlex to lib folder, if it doesn't exist we should throw an error
        if not os.path.exists(f"./WormCacheQFlex"):
            raise FileNotFoundError("WormCacheQFlex folder not found in the working directory.")
        if not os.path.exists(f"{self.get_experiment_folder_address()}/lib/WormCacheQFlex"):
            os.system(f"cp -r ./WormCacheQFlex {self.get_experiment_folder_address()}/lib/WormCacheQFlex")

        # Move files to lib
        lib_files = [
            "libknottykraken.so", 
            "libsemikraken.so"
        ]
        for f in lib_files:
            if not os.path.exists(f"/home/dev/qflex/kraken_out/{f}"):
                raise FileNotFoundError(f"Error: {f} not found in ./home/dev/qflex/kraken_out/")
            if not os.path.exists(f"{self.get_experiment_folder_address()}/lib/{f}"):
                os.system(f"cp /home/dev/qflex/kraken_out/{f} {self.get_experiment_folder_address()}/lib/{f}")

        



    def get_ipns_per_core(self) -> list[IPNSInfo]:

        # TODO This part looks messy to me, we need to revisit it later 
        is_consolidated = self.workload.IPC_info.is_consolidated
        has_client = self.simulation_context.doubled_vcpu
        core_list = self.host.get_core_sequence_as_list()
        results: list[IPNSInfo] = []
        
        core_count = self.simulation_context.core_count

        primary_ipc = self.workload.IPC_info.primary_ipc
        secondary_ipc = self.workload.IPC_info.secondary_ipc
        phantom_ipc = self.workload.IPC_info.phantom_cpu_ipc
        min_ipc = self.workload.IPC_info.min_ipc
        machine_freq_ghz = self.workload.IPC_info.machine_freq_ghz
        ipns_primary = round(primary_ipc * machine_freq_ghz, 2)
        ipns_secondary = round(secondary_ipc * machine_freq_ghz, 2)
        ipns_phantom = round(
            phantom_ipc * machine_freq_ghz / min_ipc * primary_ipc,
            2
        )

        if not is_consolidated:
            for core_idx in range(core_count):
                results.append(IPNSInfo(core_index=core_list[core_idx], ipns=ipns_primary))
        else:
            primary_core_start = self.workload.core_range.primary_core_start
            secondary_core_start = self.workload.core_range.secondary_core_start
            # TODO check this with shanqing, changed this so you can give core numbers that are actually used unlike the og script
            for core_idx in range(primary_core_start, secondary_core_start):
                results.append(IPNSInfo(core_index=core_list[core_idx], ipns=ipns_primary))
            for core_idx in range(secondary_core_start, core_count):
                results.append(IPNSInfo(core_index=core_list[core_idx], ipns=ipns_secondary))
            
        if has_client:
            for core_idx in range (core_count, core_count * 2):
                results.append(IPNSInfo(core_index=core_list[core_idx], ipns=ipns_phantom))

        return results
    
    def get_ipns_csv(self) -> str:
        """
        Generates a CSV file containing IPNS information for each core to both cfg and run folders.
        """
        # TODO check on this as well as we talked about removing the dependancy between host and target
        # Check if file exists, if it does not exist create a default one
        target = f'{self.get_experiment_folder_address()}/cfg/core_info.csv'
        if not os.path.exists(target):
            if self.include_affinity:
                df = pandas.DataFrame([[ipns_info.ipns, ipns_info.core_index] for ipns_info in self.get_ipns_per_core()], columns=["ipns", "affinity_core_idx"])
            else:
                df = pandas.DataFrame([[ipns_info.ipns] for ipns_info in self.get_ipns_per_core()], columns=["ipns"])
            df.to_csv(target, index=False)
        else:
            print(f"============== core_info.csv already exists at {target}, not overwriting it. ==============")
        # Create a sym link to the core info in cfg folder
        sym_target = f'{self.get_experiment_folder_address()}/run/core_info.csv'
        try:
            os.system(f"rm {sym_target}")
        except FileNotFoundError:
            # As faulty symlink won't show
            pass
        os.symlink(target, sym_target)


        


def create_experiment_context(
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
    population_seconds: int,
    phantom_cpu_ipc: float,
    # experiment sections
    image_folder: str,
    # Default parameters that can be induced from others
    experiment_name=None,
    image_name: str=None,
    keep_experiment_unique: bool = True,
    use_image_directly: bool = False,
    loadvm_name: str = "",
    mounting_folder: str = ".",
    # Default for simulation context
    check_period_quantum_coeff: float = 53.0,
    use_cd_rom: bool = False,
    machine_freq_ghz: float = 2.0,  # Default frequency, can be modified later
    include_affinity: bool = False
) -> ExperimentContext:
    # assert False
    # TODO add how to create experiment name

    
    # TODO check this to make sure it doesn't have edge cases
    mounting_folder = os.path.abspath(mounting_folder)
    image_folder = os.path.abspath(image_folder)

    workload = create_workload(
        workload_name=workload_name,
        primary_core_start=primary_core_start,
        secondary_core_start=secondary_core_start,
        is_consolidated=is_consolidated,
        primary_ipc=primary_ipc,
        secondary_ipc=secondary_ipc,
        phantom_cpu_ipc=phantom_cpu_ipc,
        population_seconds=population_seconds,
        machine_freq_ghz=machine_freq_ghz,
    )

    if image_name is None:
        image_name = f"root.qcow2"

    if experiment_name is None:
        experiment_name: str = 'default-experiment'
    
    if keep_experiment_unique:
        # Add date time to prevent overwriting
        experiment_name = experiment_name + '-' + datetime.datetime.now().strftime("%Y%m%d-%H%M%S")

    simulation_context = create_simulation_context(
        core_count=core_count,
        quantum_size=quantum_size,
        doubled_vcpu=doubled_vcpu,
        llc_size_per_tile_mb=llc_size_per_tile_mb,
        is_parallel=is_parallel,
        network=network,
        memory_gb=memory_gb,
        check_period_quantum_coeff=check_period_quantum_coeff,
        use_cd_rom=use_cd_rom,
    )

    if host_name.upper() not in HostType.__members__.keys():
        raise ValueError(f"Host type {host_name} not recognized. Available types: {list(HostType.__members__.keys())}")
    host_type = HostType[host_name.upper()]
    host = HOSTS[host_type]

    

    
    e = ExperimentContext(
        experiment_name=experiment_name,
        image_folder=image_folder,
        image_name=image_name,
        keep_experiment_unique=keep_experiment_unique,
        simulation_context=simulation_context,
        host=host,
        workload=workload,
        mounting_folder=mounting_folder,
        use_image_directly=use_image_directly,
        image_address="", # will be set up during initialization based on other parameters
        loadvm_name=loadvm_name,
        include_affinity=include_affinity
    )

    e.set_up_folders()

    # TODO add a print config so every one sees the final config
    return e
    


def get_capital_dict(variable: BaseModel):
    """
    Converts a list of variable names to a dictionary with uppercase keys.
    """
    return {var.upper(): getattr(variable, var) for var in variable.__fields__.keys()}
