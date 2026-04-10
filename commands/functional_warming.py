import math

from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser   

class FunctionalWarming(Executor):
    """
    This class handles the functional warming phase of the experiment and generates checkpoints.
    """

    def __init__(self,
                 experiment_context: ExperimentContext,
                 sample_size: int,
                 gen_gem5_ckp: bool = False):
        self.experiment_context = experiment_context
        self.simulation_context = self.experiment_context.simulation_context
        self.qemu_common_parser = QemuCommonArgParser(experiment_context)
        self.sample_size = sample_size
        self.gen_gem5_ckp = gen_gem5_ckp
        self.sampling_interval = math.ceil(
            (self.experiment_context.workload.population + self.sample_size - 1) / self.sample_size
        )



    

    def cmd(self) -> str:

        plugin_args = f"mode=warm,init_threshold={self.sampling_interval},interval={self.sampling_interval},count={self.sample_size}"
        create_gem5_ckp_cmd = []

        if self.gen_gem5_ckp:
            plugin_args += ",generate_gem5_chkpt=true"
            base_image_address = self.qemu_common_parser.image_address
            for i in range(self.sample_size):
                create_gem5_ckp_cmd.append(
                    f"python3 ../create_gem5_checkpoint.py snapshot_{i}.gem --num-cores {self.simulation_context.core_count}"
                )
                # Then convert qcow2 to raw
                image_name = f"snapshot_{i}.img"
                create_gem5_ckp_cmd.append(
                    f"./qemu-img convert -f qcow2 -O raw -l snapshot_{i} {base_image_address} snapshot_{i}.gem/{image_name}"
                )
                create_gem5_ckp_cmd.append(
                    f"cp ./system.physmem.store0.pmem snapshot_{i}.gem/"
                )


        fw_cmd = f"""
            ./qemu-system-aarch64 \
            {self.qemu_common_parser.get_qemu_base_args()} \
            {self.qemu_common_parser.quantum_args()} \
            -plugin ../lib/libworm_cache.so,{plugin_args} \
        """
        print("fw command:")
        print(fw_cmd)

        commands = [
            f"cd {self.experiment_context.get_experiment_folder_address()}/run",
            fw_cmd,
            # TODO add the proper conditions to only create log and fp_gen_speed at the right time
            # "rm -rf fp_gen_speed",
            # "mkdir fp_gen_speed",
            # "mv *.log ./fp_gen_speed",
        ]

        commands.extend(create_gem5_ckp_cmd)
        return commands
