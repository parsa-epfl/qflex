from pydantic import BaseModel, Field
import json
import os
import shutil
import subprocess
import pandas

from .workload import Workload, CoreGroup, create_workload
from .prebuilt import get_prebuilt_folder, load_prebuilt_config, parse_memory_gb
import datetime

# IPC-model coefficient columns of core_info.csv, in the order fw-qemu parses them
# (must match bx_coeff_t in fw-qemu/include/sysemu/asid-coeff.h).
CORE_INFO_COEFF_COLUMNS = [
    "bx_private_icache_miss_coeff",
    "bx_private_dcache_miss_coeff",
    "bx_shared_cache_miss_coeff",
    "bx_bp_miss_coeff",
    "bx_drain_store_buffer_coeff",
    "bx_instruction_coeff",
]

# Memory controller locations (mesh node ids) per supported core count. Flexus and WormCache lay the cores out
# as a mesh numbered row by row (buildMesh in flexus/components/NetShim/netcontainer.cpp, mesh_size in
# WormCache's hop_calculator.rs): 2 cores 1x2, 4 cores 2x2, 8 cores 2x4, 16 cores 4x4, 32 cores 4x8,
# 48 cores 8x6 and 64 cores 8x8 (rows x columns). Up to 4 cores, every core has a controller; from 16 cores
# on, there is one per 8 cores, in pairs on the left and right edges (the same positions as experiment-scripts,
# except for 48 cores, whose mesh is 6 columns wide).
# Flexus is compiled for a fixed controller count: `just kraken` builds a variant per count into out/kraken/mem<N>/.
MEMORY_CONTROLLER_POSITIONS = {
    1: "0",
    2: "0,1",
    4: "0,1,2,3",
    8: "0",
    16: "8,15",
    32: "8,15,24,31",
    48: "6,11,24,29,36,41",
    64: "8,15,24,31,32,39,48,55",
}
# Where `just kraken` puts the Flexus simulators (the Docker image copies them to /home/dev/qflex/kraken_out).
KRAKEN_FOLDERS = ["./out/kraken", "/home/dev/qflex/kraken_out"]

# Files copied into each experiment's run folder: name in run/ -> (candidate sources, recipe that builds them).
# The Docker image keeps the QEMU builds in ./*-qemu-saved/ (the QEMU sources are not part of the image),
# while a native build leaves them in the submodules; the first existing candidate is used.
RUN_FILES = {
    "qemu-system-aarch64": (["./fw-qemu-saved/build/qemu-system-aarch64", "./fw-qemu/build/qemu-system-aarch64"], "just fw-qemu"),
    "vanilla-qemu-system-aarch64": (["./timing-qemu-saved/build/qemu-system-aarch64", "./timing-qemu/build/qemu-system-aarch64"], "just timing-qemu"),
    "efi-virtio.rom": (["./fw-qemu-saved/pc-bios/efi-virtio.rom", "./fw-qemu/pc-bios/efi-virtio.rom"], "git submodule update --init fw-qemu"),
    "qemu-img": (["./qemu-img", "./fw-qemu/build/qemu-img"], "just fw-qemu"),
    "QEMU_EFI.fd": (["./QEMU_EFI.fd"], "git checkout QEMU_EFI.fd"),
    "debug.cfg": (["./debug.cfg"], "git checkout debug.cfg"),
}
# bxdb CLI (built by `just bxdb`), used by run_partitions.sh and partition-cleanup
BXDB_CLI = "./bxdb/target/release/bxdb"


def find_run_file(name: str) -> str:
    """Returns the first existing source of RUN_FILES[name], or raises with the recipe that builds it."""
    candidates, recipe = RUN_FILES[name]
    for candidate in candidates:
        if os.path.exists(candidate):
            return candidate
    raise FileNotFoundError(f"Error: {name} not found (looked for {', '.join(candidates)}). Get it with `{recipe}`.")

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
    machine_freq_ghz: float = Field(default=2.0, description="Frequency of the simulated cores in GHz, converts IPC to instructions per ns")

def get_qemu_nic_option(network: str) -> str:
    if 'none' == network.strip().lower():
        return "-nic none"
    elif 'user' == network.strip().lower():
        return "-nic user,model=virtio-net-pci"
    else:
        raise ValueError("Unsupported network type. Supported types are 'none' and 'user'.")

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
    machine_freq_ghz: float=2.0,
) -> SimulationContext:
    l2_way = 16
    if core_count not in MEMORY_CONTROLLER_POSITIONS:
        raise ValueError(f"Unsupported core count {core_count}. Supported: {', '.join(map(str, MEMORY_CONTROLLER_POSITIONS))}.")
    memory_controller_positions = MEMORY_CONTROLLER_POSITIONS[core_count]
    memory_controller_count = len(memory_controller_positions.split(","))

    network = get_qemu_nic_option(network)
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
        machine_freq_ghz=machine_freq_ghz,
    )

class ExperimentContext(BaseModel):
    """
    Context model for holding experiment configuration variables.
    """
    experiment_name: str = Field(description="Name of the experiment")
    # TODO move image name to the workload section
    image_folder: str = Field(description="Address of the image to use")
    image_name: str = Field(description="Name of the image to use")
    simulation_context: SimulationContext = Field(description="Simulation context containing detailed configuration")
    workload: Workload = Field(description="Workload configuration")
    mounting_folder: str = Field(default=".", description="Base working directory of qflex. use for shared folders")
    keep_experiment_unique: bool = Field(default=True, description="Whether to keep the experiment folder unique by adding a timestamp")
    use_image_directly: bool = Field(default=False, description="Whether to use the image directly from image folder instead of copying it to experiments folder")
    loadvm_name: str = Field(default="", description="Name of the loadvm to use in QEMU, optional")
    image_address: str = Field(default="", description="Full address of the image to use. Set up during initialization based on other parameters.")
    prebuilt_workload: str = Field(default="", description="ID of a prebuilt workload (e.g. 16c/dc) to start the experiment from, see ./qflex image list.")

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
        return f'{self.get_experiment_folder_address()}/timing-qemu-saved'
    def get_pflex_qemu_build_folder(self) -> str:
        return f'{self.get_experiment_folder_address()}/fw-qemu-saved'
    
    def set_up_image(self):

        if self.prebuilt_workload or os.path.exists(self.get_prebuilt_record_address()):
            self.set_up_prebuilt_image()
        elif self.use_image_directly:
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
                print(f"cp {self.image_folder}/{self.image_name} {self.get_local_image_address()}")
                if os.system(f"cp -u {self.image_folder}/{self.image_name} {self.get_local_image_address()}") != 0:
                    raise RuntimeError(f"Error: failed to copy {self.image_folder}/{self.image_name} to {self.get_local_image_address()}")
                print("copied image.")

    def get_prebuilt_record_address(self) -> str:
        """config.json of the prebuilt workload the experiment was set up from (absent otherwise)."""
        return f"{self.get_experiment_folder_address()}/run/prebuilt.json"

    def set_up_prebuilt_image(self):
        """
        Sets the experiment up from a prebuilt workload (./qflex image pull). The first command copies the
        disk into run/ (the experiment writes snapshot entries into it) and links the read-only base.zstd;
        the choice is recorded in run/prebuilt.json, so later commands keep using run/root.qcow2 and ignore
        --image-folder/--image-name.
        """
        run = f"{self.get_experiment_folder_address()}/run"
        record = self.get_prebuilt_record_address()
        if os.path.exists(record):
            with open(record) as f:
                config = json.load(f)
            if self.prebuilt_workload and self.prebuilt_workload != config["id"]:
                raise ValueError(f"Error: experiment {self.experiment_name} was set up from prebuilt workload {config['id']}, not {self.prebuilt_workload}.")
            self.prebuilt_workload = config["id"]
            self.validate_prebuilt_config(config)
        else:
            if self.use_image_directly:
                raise ValueError("Error: --prebuilt-workload cannot be combined with --use-image-directly.")
            source = get_prebuilt_folder(self.image_folder, self.prebuilt_workload)
            if not os.path.isfile(f"{source}/config.json"):
                raise FileNotFoundError(f"Error: prebuilt workload {self.prebuilt_workload} not found in {source}. Download it with `./qflex image pull {self.prebuilt_workload} --image-folder {self.image_folder}`.")
            config = load_prebuilt_config(source)
            self.validate_prebuilt_config(config)
            os.makedirs(run, exist_ok=True)
            for file in ["root.qcow2", "base.zstd"]:
                if os.path.lexists(f"{run}/{file}"):
                    raise FileExistsError(f"Error: {run}/{file} already exists; the experiment was already set up from another image. Use a new --experiment-name.")
            print(f"Copying {source}/root.qcow2 to {run}/root.qcow2 ...")
            # The downloaded files are read-only; reflink makes the copy free on filesystems that support it.
            subprocess.run(["cp", "--reflink=auto", f"{source}/root.qcow2", f"{run}/root.qcow2.part"], check=True)
            os.chmod(f"{run}/root.qcow2.part", 0o644)
            os.replace(f"{run}/root.qcow2.part", f"{run}/root.qcow2")
            # base.zstd is only read by -loadvm base, so all experiments share the downloaded file.
            os.symlink(f"{source}/base.zstd", f"{run}/base.zstd")
            with open(record, "w") as f:
                json.dump({"id": self.prebuilt_workload, **config}, f, indent=2)
        self.image_name = "root.qcow2"
        self.image_address = f"{run}/root.qcow2"
        if not self.loadvm_name:
            self.loadvm_name = "base"

    def validate_prebuilt_config(self, config: dict):
        """The base snapshot only restores on the machine it was taken with."""
        sim = self.simulation_context
        cpu_cores = sim.core_count * (2 if sim.doubled_vcpu else 1)
        expected = {
            "core count (--core-count)": (config["measured_core_count"], sim.core_count),
            "vCPU count (--core-count, --double-cores)": (config["cpu_cores"], cpu_cores),
            "memory (--memory-gb)": (parse_memory_gb(config["memory"]), sim.memory_gb),
            "network (--network)": (get_qemu_nic_option(config["network_card"]), sim.qemu_nic),
        }
        mismatches = [f"  {name}: prebuilt {want}, experiment {got}" for name, (want, got) in expected.items() if want != got]
        if mismatches:
            raise ValueError(
                f"Error: the experiment does not match prebuilt workload {self.prebuilt_workload}:\n" + "\n".join(mismatches)
                + "\nThese settings are read from the workload's config.json: remove --core-count, --double-cores, --memory-gb and --network."
            )

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

        # Find the built files first, so a missing build fails before the image is copied.
        run_files = {name: find_run_file(name) for name in RUN_FILES}
        if not os.path.exists(BXDB_CLI):
            raise FileNotFoundError(f"Error: bxdb CLI not found at {BXDB_CLI}. Build it with `just bxdb`.")

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
           "training_model.py",
           "training_by_asid.py",
        ]
        for file in root_sls:
            os.system(f"cp -u ./{file} {self.get_experiment_folder_address()}/{file}")
            print(f"moved {file} to experiment folder")


        # TODO check if rom and bios files can be linked from fw-qemu-saved when using timing-qemu
        for name, source in run_files.items():
            target = f"{self.get_experiment_folder_address()}/run/{name}"
            if not os.path.exists(target):
                shutil.copy2(source, target)
        # TODO turn WormCache address into a parameter
        # Copy WormCache to lib folder, if it doesn't exist we should throw an error
        if not os.path.exists(f"./WormCache"):
            raise FileNotFoundError("WormCache folder not found in the working directory.")
        if not os.path.exists(f"{self.get_experiment_folder_address()}/lib/WormCache"):
            os.system(f"cp -r ./WormCache {self.get_experiment_folder_address()}/lib/WormCache")

        # Copy the Flexus simulators built for the experiment's memory controller count to lib
        kraken_folder = self.get_kraken_folder()
        for f in ["libknottykraken.so", "libsemikraken.so"]:
            if not os.path.exists(f"{kraken_folder}/{f}"):
                raise FileNotFoundError(f"Error: {f} not found in {kraken_folder}. Build it with `just kraken`.")
            if not os.path.exists(f"{self.get_experiment_folder_address()}/lib/{f}"):
                os.system(f"cp {kraken_folder}/{f} {self.get_experiment_folder_address()}/lib/{f}")

        os.system(f"cp -u {BXDB_CLI} {self.get_experiment_folder_address()}/bin/bxdb")

        



    def get_kraken_folder(self) -> str:
        """Folder with the Flexus simulators compiled for this experiment's memory controller count."""
        variant = f"mem{self.simulation_context.mem_controller_count}"
        for folder in KRAKEN_FOLDERS:
            if os.path.isdir(f"{folder}/{variant}"):
                return os.path.abspath(f"{folder}/{variant}")
        raise FileNotFoundError(f"Error: no Flexus simulators for {self.simulation_context.mem_controller_count} memory controllers ({variant}) in {' or '.join(KRAKEN_FOLDERS)}. Build them with `just kraken`.")

    def get_ipns_csv(self) -> None:
        """
        Generates a CSV file containing IPNS information for each core to both cfg and run folders.
        """
        # Check if file exists, if it does not exist create a default one
        target = f'{self.get_experiment_folder_address()}/cfg/core_info.csv'
        if not os.path.exists(target):
            # 9-column format parsed by fw-qemu (tcg_parse_core_info_file). Every vCPU starts
            # with the constant model at the IPC of its group; `./qflex train` later switches cores
            # to the ipc-model. fw-qemu pins vCPU i to host_core_idx, here host core i.
            rows = []
            for vcpu_idx, ipc in enumerate(self.workload.ipc_per_vcpu()):
                ipns = round(ipc * self.simulation_context.machine_freq_ghz, 2)
                rows.append([vcpu_idx, "constant", ipns] + [0.0] * len(CORE_INFO_COEFF_COLUMNS))
            df = pandas.DataFrame(rows, columns=["host_core_idx", "model_type", "ipns"] + CORE_INFO_COEFF_COLUMNS)
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


        


def find_prebuilt_config(mounting_folder: str, experiment_name: str, image_folder: str, prebuilt_workload: str) -> dict | None:
    """
    config.json of the prebuilt workload of the experiment: run/prebuilt.json once the experiment is set up
    (so later commands do not need --prebuilt-workload), else the downloaded one. None without a prebuilt workload.
    """
    record = f"{mounting_folder}/experiments/{experiment_name}/run/prebuilt.json"
    if os.path.exists(record):
        with open(record) as f:
            return json.load(f)
    if not prebuilt_workload:
        return None
    source = get_prebuilt_folder(image_folder, prebuilt_workload)
    if not os.path.isfile(f"{source}/config.json"):
        raise FileNotFoundError(f"Error: prebuilt workload {prebuilt_workload} not found in {source}. Download it with `./qflex image pull {prebuilt_workload} --image-folder {image_folder}`.")
    return load_prebuilt_config(source)


def create_experiment_context(
    quantum_size: int,
    llc_size_per_tile_mb: int,
    is_parallel: bool,
    # Machine and workload: read from the config.json of --prebuilt-workload when not given
    core_count: int | None,
    doubled_vcpu: bool | None,
    network: str | None,
    memory_gb: int | None,
    workload_name: str | None,
    groups: list[CoreGroup] | None,
    population_seconds: float | None,
    # experiment sections
    image_folder: str,
    # Default parameters that can be induced from others
    experiment_name=None,
    image_name: str | None=None,
    keep_experiment_unique: bool = True,
    use_image_directly: bool = False,
    loadvm_name: str = "",
    mounting_folder: str = ".",
    # Default for simulation context
    check_period_quantum_coeff: float = 53.0,
    use_cd_rom: bool = False,
    machine_freq_ghz: float = 2.0,
    prebuilt_workload: str = "",
) -> ExperimentContext:
    # TODO check this to make sure it doesn't have edge cases
    mounting_folder = os.path.abspath(mounting_folder)
    image_folder = os.path.abspath(image_folder)

    if image_name is None:
        image_name = f"root.qcow2"

    if experiment_name is None:
        experiment_name: str = 'default-experiment'
    
    if keep_experiment_unique:
        # Add date time to prevent overwriting
        experiment_name = experiment_name + '-' + datetime.datetime.now().strftime("%Y%m%d-%H%M%S")

    # A prebuilt workload describes its machine and workload; given flags take precedence
    # (set_up_prebuilt_image rejects machine flags that do not match the snapshot).
    config = find_prebuilt_config(mounting_folder, experiment_name, image_folder, prebuilt_workload)
    if config is not None:
        measured = config["measured_core_count"]
        core_count = measured if core_count is None else core_count
        doubled_vcpu = config["cpu_cores"] == 2 * measured if doubled_vcpu is None else doubled_vcpu
        network = config["network_card"] if network is None else network
        memory_gb = parse_memory_gb(config["memory"]) if memory_gb is None else memory_gb
        workload_name = workload_name or config["workload"]
        # Older config.json files have no groups nor population: one group with the default IPCs.
        if groups is None and "groups" in config:
            groups = [CoreGroup(**g) for g in config["groups"]]
        if population_seconds is None:
            population_seconds = config.get("suggested_population_seconds")
    missing = [flag for flag, value in [("--core-count", core_count), ("--double-cores/--no-double-cores", doubled_vcpu),
                                        ("--network", network), ("--memory-gb", memory_gb)] if value is None]
    if missing:
        raise ValueError(f"Error: missing {', '.join(missing)} (required without --prebuilt-workload).")
    assert core_count is not None and doubled_vcpu is not None and network is not None and memory_gb is not None

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
        machine_freq_ghz=machine_freq_ghz,
    )

    workload = create_workload(
        workload_name=workload_name or "workload",
        groups=groups,
        core_count=core_count,
        doubled_vcpu=doubled_vcpu,
        population_seconds=population_seconds,
    )

    e = ExperimentContext(
        experiment_name=experiment_name,
        image_folder=image_folder,
        image_name=image_name,
        keep_experiment_unique=keep_experiment_unique,
        simulation_context=simulation_context,
        workload=workload,
        mounting_folder=mounting_folder,
        use_image_directly=use_image_directly,
        image_address="", # will be set up during initialization based on other parameters
        loadvm_name=loadvm_name,
        prebuilt_workload=prebuilt_workload,
    )

    e.set_up_folders()

    # TODO add a print config so every one sees the final config
    return e
    


def get_capital_dict(variable: BaseModel):
    """
    Converts a list of variable names to a dictionary with uppercase keys.
    """
    return {var.upper(): getattr(variable, var) for var in type(variable).model_fields.keys()}
