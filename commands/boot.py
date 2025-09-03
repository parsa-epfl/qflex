import os
from commands import Executor

class Boot(Executor):

    def __init__(self, 
                 image_name: str = 'base.qcow2',
                 memory_mb: int = 2048,
                 cores: int = 1,
                 double_cores: bool = False,):
        self.image_name = image_name
        self.memory_size_mb = memory_mb
        self.cores = cores
        self.double_cores = double_cores
        self.core_coeff = 1
        if self.double_cores:
            self.core_coeff = 2
    
    def cmd(self) -> str:

        alpine_image_name = 'alpine-standard-3.22.1-aarch64.iso'
        if not os.path.isfile(f'images/{alpine_image_name}'):
            alpine_url = f'https://dl-cdn.alpinelinux.org/alpine/v3.22/releases/aarch64/{alpine_image_name}'
            os.system(f'wget {alpine_url} -O images/{alpine_image_name}')

        return f"""
        ./qemu-aarch64 \
        -M virt,gic-version=max,virtualization=off,secure=off \
        -smp {self.core_coeff * self.cores}\
        -cpu max,pauth=off -m {self.memory_size_mb} \
        -bios ./qemu/build/pc-bios/edk2-aarch64-code.fd \
        -drive if=virtio,file=./images/{self.image_name},format=qcow2 \
        -cdrom ./images/{alpine_image_name} \
        -boot d \
        -nic user,model=virtio-net-pci \
        -rtc clock=vm \
        -display none \
        -serial mon:stdio 
        """