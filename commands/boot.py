import os
from commands import Executor
from .config import ExperimentContext
from .utils import get_pflex_qemu_build_folder

class Boot(Executor):

    def __init__(self, 
                 experimnt_context: ExperimentContext):
        self.experiment_context = experimnt_context
        self.image_name = self.experiment_context.get_local_image_address()
        self.memory_size_mb = self.experiment_context.simulation_context.memory * 1024
        self.cores = self.experiment_context.simulation_context.core_count
        self.double_cores = self.experiment_context.simulation_context.doubled_vcpu
        self.core_coeff = 1
        if self.double_cores:
            self.core_coeff = 2
        # TODO have to change the CLI to send this in, and also make it a mandetry field
    
    def cmd(self) -> str:

        alpine_image_name = 'alpine-standard-3.22.1-aarch64.iso'
        if not os.path.isfile(f'{self.experiment_context.working_directory}/images/{alpine_image_name}'):
            alpine_url = f'https://dl-cdn.alpinelinux.org/alpine/v3.22/releases/aarch64/{alpine_image_name}'
            os.system(f'wget {alpine_url} -O {self.experiment_context.working_directory}/images/{alpine_image_name}')

        return f"""
        {get_pflex_qemu_build_folder()}/qemu-aarch64 \
        -M virt,gic-version=max,virtualization=off,secure=off \
        -smp {self.core_coeff * self.cores}\
        -cpu max,pauth=off -m {self.memory_size_mb} \
        -bios {get_pflex_qemu_build_folder()}/pc-bios/edk2-aarch64-code.fd \
        -drive if=virtio,file=./images/{self.image_name},format=qcow2 \
        -cdrom {self.experiment_context.working_directory}/images/{alpine_image_name} \
        -boot d \
        -nic user,model=virtio-net-pci \
        -rtc clock=vm \
        -display none \
        -serial mon:stdio 
        """