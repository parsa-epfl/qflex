from commands import Executor


class Load(Executor):

    def __init__(self,
                 image_name: str = 'base.qcow2',
                 memory_mb: int = 2048,
                 cores: int = 1,
                 double_cores: bool = False,
                 is_parallel: bool = False,
                 quantum_size_ns: int = 2000,):
        self.image_name = image_name
        self.memory_size_mb = memory_mb
        self.cores = cores
        self.double_cores = double_cores
        self.is_parallel = is_parallel
        self.quantum_size_ns = quantum_size_ns
        self.core_coeff = 1
        if self.double_cores:
            self.core_coeff = 2

    
    def cmd(self) -> str:
        clock_command = ''
        if self.is_parallel:
            clock_command = f'-quantum size={self.quantum_size_ns},check_period={self.quantum_size_ns * 53} '
        else:
            clock_command = f'-icount shift=0,align=off,sleep=off,q={self.quantum_size_ns},check_period={self.quantum_size_ns * 53} '
        # TODO this plugin exists outside, needs to be moved in the qemu folder later
        return f"""
        /qemu/build/qemu-system-aarch64 \
        -smp {self.core_coeff * self.cores} \
        -M virt,gic-version=max,virtualization=off,secure=off \
        -cpu max,pauth=off -m {self.memory_size_mb} \
        -boot menu=on \
        -bios ./qemu/build/pc-bios/edk2-aarch64-code.fd \
        -drive if=virtio,file=./images/{self.image_name},format=qcow2 \
        -nic {{ QEMU_NIC }} \
        -rtc clock=vm \
        {clock_command} \
        -plugin ../lib/libworm_cache.so,mode=pure_fill,prefix=init \
        -loadvm base \
        -nographic -no-reboot
        """
