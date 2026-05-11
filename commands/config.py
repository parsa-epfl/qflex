from typing import Annotated, List, Optional

from pydantic import BaseModel, Field, PrivateAttr
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
    # Pure path computation — no filesystem side effects. The folder is
    # materialised by ExperimentContext.set_up_folders() (called from the
    # executor's leaf branch via prepare_for_execution()).
    return os.path.abspath(f'{mounting_folder}/experiments/{experiment_name}')

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
    qemu_nic: str = Field(description="type of NIC to use in QEMU, this is in addition to connecting to internet and other nodes that are there by default.")
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
    use_gdb: bool = Field(default=True, description="Wrap the qemu invocation in `gdb -ex run --args ...`. Set to False (e.g. in test YAMLs) to run qemu directly so the leaf doesn't depend on gdb's interactive prompt handling on segfault.")
    image_address: str = Field(default="", description="Full address of the image to use. Set up during initialization based on other parameters.")
    seed_image_address: str = Field(default="", description="Full address of the seed image to use. Set up during initialization based on other parameters.")
    include_affinity: bool = Field(default=False, description="Whether or not generate affinity index in core_info.csv.")
    node_number: int = Field(default=-1, description="Node number in multi-node setup, -1 means single node. 0 is the master node.")

    neighbor_node_list: List[int] = Field(default=[], description="List of neighbor node numbers in multi-node setup.")
    latencies_ns_list: List[int] = Field(default=[], description="List of latencies to neighbor nodes in nanoseconds.")
    syncs_list: List[str] = Field(default=[], description="List of sync settings ('true' or 'false') for neighbor nodes.")
    # TODO later we need to revisit if partition and idx are well suited to be part of the exp object
    partition_number: int = Field(default=-1, description="Partition number for this node, used for some qemu options.")
    partition_count: int = Field(default=16, description="Number of partitions the per-sampling-unit checkpoints are split into for parallel timing runs (driven by the `partition` phase). Same value is used downstream by `run-partition` to enumerate partitions.")
    sample_size: int = Field(default=30, description="Number of sampling units the `fw` phase emits checkpoints for. Only the `fw` command consumes this; the timing-phase commands ignore it.")
    warming_ratio: int = Field(default=2, description="Detailed-warming prefix length within each sampling unit, in units relative to `measurement_ratio` (each unit = 100k cycles). Consumed by the timing-phase commands `run-partition` / `run-single-partition` / `run-idx`; ignored by every other phase.")
    measurement_ratio: int = Field(default=1, description="Measurement segment length within each sampling unit, in units relative to `warming_ratio`. Consumed by the timing-phase commands `run-partition` / `run-single-partition` / `run-idx`; ignored by every other phase.")
    idx: int = Field(default=-1, description="Index of the partition to run, used for some qemu options.")
    seed_image_name: str = Field(default='', description="Name of the seed image file to use in multi-node setup.")
    telnet_port: int = Field(default=-1, description="Telnet port for QEMU monitor.")
    use_telnet_monitor: bool = Field(default=False, description="Whether to use telnet monitor for QEMU instead of stdio.")
    serial_telnet_port: int = Field(default=-1, description="Telnet port for QEMU serial console (used by Path A interaction_script). -1 -> auto = 55600 + node_number.")
    interaction_script: str = Field(default="", description="Path to an executable script (expect/bash/python/...) that drives QEMU for boot/load on this leaf. Receives TELNET_SERIAL_PORT, TELNET_MONITOR_PORT, SERIAL_LOG_PATH, EXP_FOLDER, NODE_NUMBER as env vars. Setting this auto-enables monitor-on-telnet and serial-on-telnet for the leaf.")
    interactive_tmux: bool = Field(default=False, description="If True, run boot/load in a fresh tmux window (one per leaf). Requires a running tmux server. Other phases ignore this field.")
    pdes_net_devs: List[str] = Field(default=[], description="List of network device models (e.g., 'e1000', 'virtio-net-pci') to use for each neighbor node in multi-node setup.")
    sub_experiments: List["ExperimentContext"] = Field(default_factory=list, description="Optional sub-experiments. If non-empty, this context is a group node; leaf-level fields are unused and the executor recurses into each sub-experiment.")
    wait_for_nodes: List[int] = Field(default_factory=list, description="Node-numbers whose .started sentinel must exist before this leaf may proceed. Empty for the master. Set to e.g. [0] to wait for the master, or [2] to wait for node 2.")
    _creation_kwargs: dict = PrivateAttr(default_factory=dict)


    def has_sub_experiments(self) -> bool:
        return len(self.sub_experiments) > 0

    def compute_runtime_settings(self):
        """Pure (no filesystem, no shm) part of leaf prep: auto-flip flags and
        auto-compute ports based on user-set fields. Safe to call in dry-run so
        the rendered bash reflects what the real run would use."""
        # Path A (scripted boot/load): the script needs separate telnet endpoints for
        # serial console (guest input/output) and monitor (savevm/quit). Auto-enable
        # both so the user only needs to set interaction_script in YAML.
        if self.interaction_script and not self.use_telnet_monitor:
            print(f"[boot/load] interaction_script={self.interaction_script!r} -> auto-enabling use_telnet_monitor for node {self.node_number}.")
            self.use_telnet_monitor = True

        if self.use_telnet_monitor and self.telnet_port == -1:
            self.telnet_port = 55558
            if self.is_multi_node():
                self.telnet_port += self.node_number

        if self.interaction_script and self.serial_telnet_port == -1:
            self.serial_telnet_port = 55600
            if self.is_multi_node():
                self.serial_telnet_port += self.node_number

    def prepare_for_execution(self):
        """One entry point for all leaf-level prep. Called by the executor right before the leaf bash runs.
        Wraps the existing prep functions; do not call these from create_experiment_context."""
        self.compute_runtime_settings()
        self.set_up_folders()
        self.setup_nic_args()

    def get_partition_folder(self) -> str:
        if self.partition_number < 0:
            raise ValueError("Partition number is not set for this experiment context.")
        return self.get_experiment_folder_address() + f"/run/partition_{self.partition_number}"

    def is_multi_node(self) -> bool:
        return self.node_number >= 0

    def get_neighbor_count(self) -> int:
        return len(self.neighbor_node_list)

    def is_master_node(self) -> bool:
        return self.node_number == 0

    def get_shm_names(self, recieve: bool) -> List[str]:
        shm_names = []
        partition_str = ""
        if self.partition_number >= 0:
            partition_str = f"part_{self.partition_number}_"
        
        idx_str = ""
        if self.idx >= 0:
            idx_str = f"idx_{self.idx}_"
        for neighbor in self.neighbor_node_list:
            if recieve:
                shm_names.append(f"pdes_{neighbor}_to_{self.node_number}"+partition_str+idx_str)
            else:
                shm_names.append(f"pdes_{self.node_number}_to_{neighbor}"+partition_str+idx_str)
        return shm_names

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
    

    def copy_image_for_node(self, image_folder: str, parent_file_name: str):

        file_name_parts = parent_file_name.split('.')

        old_address = f"{image_folder}/{parent_file_name}"

        new_file_name = '.'.join(file_name_parts[:-1]) + f'-node{self.node_number}.' + file_name_parts[-1]
        new_address = f"{image_folder}/{new_file_name}"

        if not os.path.exists(new_address):
            print(f"Creating node specific file for node {self.node_number} at {new_address}...")
            os.system(f"cp -u {old_address} {new_address}")

        return new_address, new_file_name

    def set_up_image(self):

        self.seed_image_address = f"{self.image_folder}/{self.seed_image_name}"
        if self.use_image_directly:
            self.image_address = f"{self.image_folder}/{self.image_name}"
        else:
            # TODO add some checks for this
            # Check if base image exists in root folder
            # TODO remove this part, too
            raise Exception("Deprecated")
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
            
        if self.node_number >=0:
            self.image_address, self.image_name = self.copy_image_for_node(self.image_folder, self.image_name)
            if self.seed_image_name is not None and len(self.seed_image_name) > 0:
                self.seed_image_address, self.seed_image_name = self.copy_image_for_node(self.image_folder, self.seed_image_name)

            

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
           "result.py",
           "collect.py",
           "result_new.py"
        ]
        for file in root_sls:
            os.system(f"cp -u ./{file} {self.get_experiment_folder_address()}/{file}")
            print(f"moved {file} to experiment folder")


        # check that both build/qemu-system-aarch64 exists in run folder plus efi-virtio.rom
        # link all of them
        # TODO check if rom and bios files can be linked from parallel-qemu-saved when using qemu
        # TODO check why files are being turned into bz2
        
        
        # Read the staged trees the Makefile produces. `make parallel-qemu-build`
        # copies parallel-qemu/build/ → parallel-qemu-saved/build/ and same for
        # qemu → qemu-saved; the docker image build is expected to run those
        # targets so both `-saved/` dirs are present in the image regardless of
        # variant (release/debug, base/worm).
        # (source, target_basename_in_run_folder)
        run_files: list[tuple[str, str]] = [
            ("./parallel-qemu-saved/build/qemu-system-aarch64", "qemu-system-aarch64"),
            ("./qemu-saved/build/qemu-system-aarch64", "vanilla-qemu-system-aarch64"),
            # TODO if we ever decide to change EFI and bios, this needs to change
            ("./QEMU_EFI.fd", "QEMU_EFI.fd"),
            ("./parallel-qemu-saved/pc-bios/efi-virtio.rom", "efi-virtio.rom"),
            ("./parallel-qemu-saved/pc-bios/efi-e1000.rom", "efi-e1000.rom"),
            ("debug.cfg", "debug.cfg"),
        ]
        for src, basename in run_files:
            link_address = f"{self.get_experiment_folder_address()}/run/{basename}"
            print(f"copying {src} to {link_address}...")
            # TODO add checks for when cp fails
            os.system(f"cp -u {src} {link_address}")
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

        
    def shm_clean_up(self):
        
        shm_recvs = self.get_shm_names(recieve=True)
        shm_sends = self.get_shm_names(recieve=False)
        all_shm_names = shm_recvs + shm_sends

        for shm_name in all_shm_names:
            shm_path = f"/dev/shm/{shm_name}"
            if os.path.exists(shm_path):
                print(f"Removing shared memory file {shm_path}...")
                # Force femove the file
                os.system(f"rm -f {shm_path}")
            else:
                print(f"Shared memory file {shm_path} does not exist, skipping removal.")


    def clean_up(self):
        self.shm_clean_up()


        

    def setup_nic_args(self):
        # TODO move this to simulation context later
        # nic_command = self.simulation_context.qemu_nic.strip().lower()
        nic_command = self.simulation_context.qemu_nic.strip().lower() + " "
        if self.is_multi_node():
            
            shm_recvs = self.get_shm_names(recieve=True)
            shm_sends = self.get_shm_names(recieve=False)
            for i in range(self.get_neighbor_count()):
                shm_recv = shm_recvs[i]
                shm_send = shm_sends[i]
                net_dev = self.pdes_net_devs[i]
                # Make sure no file exists for this shm name
                # rm -f /dev/shm/{shm_recv} /dev/shm/{shm_send}
                # IMPORTANT TODO: this will rely on master being started first, need to automate nodes starting to prevent other things from happening
                if (self.is_master_node()):
                    print(f"Cleaning up shared memory files for neighbor {self.neighbor_node_list[i]}: {shm_recv} and {shm_send}")
                    os.system(f"rm -f /dev/shm/{shm_recv}")
                    os.system(f"rm -f /dev/shm/{shm_send}")

                sync = self.syncs_list[i]
                latency_ns = self.latencies_ns_list[i]
                mac_address = f"mac=52:54:00:aa:bb:{self.node_number * 10 + i:02x}"
                if net_dev == 'e1000':
                    dev = f" -device e1000,netdev=net{i},{mac_address} "
                elif net_dev == 'virtio-net-pci':
                    pci_addr = 0x10 + i
                    dev = f" -device virtio-net-pci,netdev=net{i},bus=pcie.0,addr=0x{pci_addr:02x},{mac_address},rx_queue_size=1024,tx_queue_size=256 "
                else:
                    raise ValueError(f"Unsupported network device {net_dev} for neighbor {self.neighbor_node_list[i]}. Supported devices are 'e1000' and 'virtio-net-pci'.")
                nic_command = nic_command + f"""  -netdev pdes,id=net{i},shm-send=/{shm_send},shm-recv=/{shm_recv},latencyns={latency_ns},sync={sync},master={str(self.is_master_node()).lower()} {dev} """

        internet_pci_addr = 0x10 + self.get_neighbor_count()
        internet_nic = f' -netdev user,id=net_user -device e1000,netdev=net_user,bus=pcie.0,addr=0x{internet_pci_addr:02x} '
        nic_command = nic_command + internet_nic


        self.simulation_context.qemu_nic = nic_command.strip().lower()


        # Port auto-compute moved to compute_runtime_settings() so dry-run can call
        # it without filesystem side effects. Calling it here again is idempotent
        # (no-op when ports are already set) — keep it for safety against any
        # caller that bypasses prepare_for_execution.
        self.compute_runtime_settings()

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
        # Copy file and override if you need to
        os.system(f"cp -u {target} {sym_target}")


        


def create_experiment_context(
    core_count: Annotated[int, Field(description="Number of CPU cores for the VM.")],
    quantum_size: Annotated[int, Field(description="Quantum size for the simulator in nanoseconds.")],
    doubled_vcpu: Annotated[bool, Field(description="Double the number of CPU cores for the client.")],
    llc_size_per_tile_mb: Annotated[int, Field(description="LLC size per tile in MB.")],
    is_parallel: Annotated[bool, Field(description="Whether the simulation is parallel or not.")],
    network: Annotated[str, Field(description="Network mode, either user or none, this is in addition to connecting to internet and other nodes that are there by default.")],
    memory_gb: Annotated[int, Field(description="Memory size for the VM in GB.")],
    # Host section:
    host_name: Annotated[str, Field(description="Host name, used to create initial ipns file.")],
    # Workload section:
    workload_name: Annotated[str, Field(description="Workload name.")],
    primary_core_start: Annotated[int, Field(description="Starting core for primary workload.")],
    is_consolidated: Annotated[bool, Field(description="Whether the workload is consolidated or not.")],
    primary_ipc: Annotated[float, Field(description="Target IPC for primary workload.")],
    population_seconds: Annotated[float, Field(description="Population size for the workload in seconds.")],
    secondary_core_start: Annotated[int, Field(description="Starting core for secondary workload. Only used if consolidated is True.")] = -1,
    secondary_ipc: Annotated[float, Field(description="Target IPC for secondary workload. Only used if consolidated is True.")] = 0.0,
    phantom_cpu_ipc: Annotated[float, Field(description="Target IPC for phantom CPU. This is used for the client in the same node. Only used in double core mode.")] = -1.0,
    # experiment sections
    image_folder: Annotated[str, Field(description="Folder where images are stored.")] = "./images",
    experiment_name: Annotated[str, Field(description="Name of the experiment. Used for organizing output files.")] = "default-experiment",
    image_name: Annotated[str, Field(description="Name of the image file to load.")] = "root.qcow2",
    keep_experiment_unique: Annotated[bool, Field(description="Whether to keep the experiment folder unique by adding a timestamp.")] = False,
    use_image_directly: Annotated[bool, Field(description="Whether to use the image directly from the image folder or copy it to the experiment folder.")] = False,
    loadvm_name: Annotated[str, Field(description="Name of the loadvm to use in QEMU, optional.")] = "",
    use_gdb: Annotated[bool, Field(description="Wrap the qemu invocation in `gdb -ex run --args ...`. Set to False (e.g. in test YAMLs) to run qemu directly.")] = True,
    mounting_folder: Annotated[str, Field(description="Mounting directory where the experiment folders will be created.")] = ".",
    check_period_quantum_coeff: Annotated[float, Field(description="Coefficient to determine the check period based on quantum size. The value multiplied by quantum size to get check period.")] = 53.0,
    use_cd_rom: Annotated[bool, Field(description="Whether to use a CD-ROM for initial setup.")] = False,
    machine_freq_ghz: Annotated[float, Field(description="Machine frequency in GHz.")] = 2.0,
    include_affinity: Annotated[bool, Field(description="Whether or not to generate affinity index in core_info.csv.")] = False,
    # Multi-node parameters
    node_number: Annotated[int, Field(description="Node number in multi-node setup, -1 means single node. 0 is the master node.")] = -1,
    neighbor_node_list: Annotated[Optional[List[int]], Field(description="List of neighbor node numbers in multi-node setup, only used if node_number is not -1.")] = None,
    latencies_ns_list: Annotated[Optional[List[int]], Field(description="List of latencies to neighbor nodes in nanoseconds, only used if node_number is not -1. Order matches neighbor_node_list.")] = None,
    syncs_list: Annotated[Optional[List[str]], Field(description="List of sync options to neighbor nodes ('true' or 'false'), only used if node_number is not -1. Order matches neighbor_node_list.")] = None,
    seed_image_name: Annotated[str, Field(description="Name of the seed image file to use in multi-node setup.")] = "",
    telnet_port: Annotated[int, Field(description="Telnet port for QEMU monitor instead of stdio.")] = -1,
    use_telnet_monitor: Annotated[bool, Field(description="Whether to use telnet monitor for QEMU instead of stdio.")] = False,
    partition_number: Annotated[int, Field(description="Partition number for the nodes to run things in parallel.")] = -1,
    partition_count: Annotated[int, Field(description="Number of partitions the per-sampling-unit checkpoints are split into for parallel timing runs.")] = 16,
    sample_size: Annotated[int, Field(description="Number of sampling units the `fw` phase emits checkpoints for. Only the `fw` command consumes this; other phases ignore it.")] = 30,
    warming_ratio: Annotated[int, Field(description="Detailed-warming prefix length within each sampling unit, in units relative to `measurement_ratio` (each unit = 100k cycles). Consumed by the timing-phase commands `run-partition` / `run-single-partition` / `run-idx`; ignored by every other phase.")] = 2,
    measurement_ratio: Annotated[int, Field(description="Measurement segment length within each sampling unit, in units relative to `warming_ratio`. Consumed by the timing-phase commands `run-partition` / `run-single-partition` / `run-idx`; ignored by every other phase.")] = 1,
    idx: Annotated[int, Field(description="Index of the partition to run, used for some qemu options.")] = -1,
    pdes_net_devs: Annotated[Optional[List[str]], Field(description="List of network device models ('e1000' or 'virtio-net-pci') to use for each neighbor node in multi-node setup. Order matches neighbor_node_list.")] = None,
    sub_experiments: Annotated[Optional[List[ExperimentContext]], Field(description="Optional sub-experiments. If non-empty, this is a group node — leaf-level fields are inherited (e.g. via YAML extends) but unused, and the executor recurses into each sub-experiment in parallel.")] = None,
    wait_for_nodes: Annotated[Optional[List[int]], Field(description="Node-numbers whose .started sentinel must exist before this leaf may proceed. Empty for the master; set e.g. [0] to wait for the master.")] = None,
    serial_telnet_port: Annotated[int, Field(description="Telnet port for QEMU serial console (Path A). -1 -> auto = 55600 + node_number.")] = -1,
    interaction_script: Annotated[str, Field(description="Path to an executable script that drives QEMU on this leaf during boot/load. Receives TELNET_SERIAL_PORT, TELNET_MONITOR_PORT, SERIAL_LOG_PATH, EXP_FOLDER, NODE_NUMBER as env vars. Auto-enables monitor-on-telnet + serial-on-telnet.")] = "",
    interactive_tmux: Annotated[bool, Field(description="If True, run boot/load in a fresh tmux window (one per leaf). Requires a running tmux server. Other phases ignore this field.")] = False,
) -> ExperimentContext:
    neighbor_node_list = neighbor_node_list or []
    latencies_ns_list = latencies_ns_list or []
    syncs_list = syncs_list or []
    pdes_net_devs = pdes_net_devs or []
    sub_experiments = sub_experiments or []
    wait_for_nodes = wait_for_nodes or []

    creation_kwargs = {k: v for k, v in locals().items()}
    is_group = len(sub_experiments) > 0

    # assert False
    # TODO add how to create experiment name

    neighbers_length = min([len(neighbor_node_list), len(latencies_ns_list), len(syncs_list)])
    if not is_group and (neighbers_length > 0 or node_number != -1):

        for value in set(syncs_list):
            assert value in ['true', 'false'], "syncs values must be either 'true' or 'false'"

        assert len(neighbor_node_list) == len(latencies_ns_list) == len(syncs_list) == len(pdes_net_devs)
        assert node_number != -1, "node_number must be set when neighbor nodes are specified."
        assert neighbers_length > 0, "neighbor_node_list, latencies_ns_list, and syncs_list must have at least one entry when node_number is set."
        print(f"Node {node_number} has neighbors: {neighbor_node_list} with latencies {latencies_ns_list} and syncs {syncs_list}")
        pdes_net_devs_set = set(pdes_net_devs)
        for net_devs in pdes_net_devs_set:
            print(f"Net {net_devs} is being used for neighbors.")
            assert net_devs in ['e1000', 'virtio-net-pci'], "pdes_net_devs values must be either 'e1000' or 'virtio-net-pci'"
    
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
        use_gdb=use_gdb,
        include_affinity=include_affinity,
        node_number=node_number,
        neighbor_node_list=neighbor_node_list,
        latencies_ns_list=latencies_ns_list,
        syncs_list=syncs_list,
        seed_image_name=seed_image_name,
        telnet_port=telnet_port,
        use_telnet_monitor=use_telnet_monitor,
        partition_number=partition_number,
        partition_count=partition_count,
        sample_size=sample_size,
        warming_ratio=warming_ratio,
        measurement_ratio=measurement_ratio,
        idx=idx,
        pdes_net_devs=pdes_net_devs,
        sub_experiments=sub_experiments,
        wait_for_nodes=wait_for_nodes,
        serial_telnet_port=serial_telnet_port,
        interaction_script=interaction_script,
        interactive_tmux=interactive_tmux,
    )

    e._creation_kwargs = creation_kwargs

    # TODO add a print config so every one sees the final config
    return e

def clone_experiment_context(
    source: ExperimentContext,
    **overrides
) -> ExperimentContext:
    """
    Re-create an ExperimentContext from scratch via create_experiment_context,
    using the original creation params with any overrides applied.
    """
    if not source._creation_kwargs:
        raise ValueError("Source ExperimentContext has no stored creation kwargs. "
                         "Was it created via create_experiment_context?")
    
    kwargs = {**source._creation_kwargs, **overrides}
    return create_experiment_context(**kwargs)


def get_capital_dict(variable: BaseModel):
    """
    Converts a list of variable names to a dictionary with uppercase keys.
    """
    return {var.upper(): getattr(variable, var) for var in variable.__fields__.keys()}
