import os
from commands.config import ExperimentContext

class QemuCommonArgParser:
    def __init__(self, experiment_context: ExperimentContext):
        self.experiment_context = experiment_context
        self.simulation_context = self.experiment_context.simulation_context
        self.image_address = self.experiment_context.get_local_image_address()
        self.memory_size_mb = self.experiment_context.simulation_context.memory_gb * 1024
        self.cores = self.experiment_context.simulation_context.core_count
        self.double_cores = self.experiment_context.simulation_context.doubled_vcpu
        self.core_coeff = 1
        if self.double_cores:
            self.core_coeff = 2

        
        
        self.nic_command = self.experiment_context.simulation_context.qemu_nic.strip().lower()

        self.loadvm = ''
        if self.experiment_context.loadvm_name is not None and len(self.experiment_context.loadvm_name) > 0:
            self.loadvm = f'-loadvm {self.experiment_context.loadvm_name}'

        check_period_quantum_coeff = self.simulation_context.check_period_quantum_coeff
        self.quantum_size = self.simulation_context.quantum_size
        self.check_period = int(
            self.simulation_context.quantum_size * check_period_quantum_coeff
        )

        self.cd_rom = ''
        self.use_cd_rom = self.simulation_context.use_cd_rom
        if self.use_cd_rom:
            alpine_image_name = 'alpine-virt-3.22.1-aarch64.iso'
            experiment_folder = self.experiment_context.get_experiment_folder_address()
            if not os.path.isfile(f'{experiment_folder}/images/{alpine_image_name}'):
                alpine_url = f'https://dl-cdn.alpinelinux.org/alpine/v3.22/releases/aarch64/{alpine_image_name}'
                os.system(f'wget {alpine_url} -O {experiment_folder}/images/{alpine_image_name}')
            self.cd_rom = f'-cdrom {experiment_folder}/images/{alpine_image_name} '
        


        
    def get_qemu_base_args(self, monitor_port: int | None = None) -> str:
        drive_arg = f"-drive if=virtio,file={self.image_address},format=qcow2"

        qemu_args = f""" -M virt,gic-version=max,virtualization=off,secure=off \
        -smp {self.core_coeff * self.cores} \
        -cpu max,pauth=off -m {self.memory_size_mb} \
        -boot order=d,menu=on \
        -bios ./QEMU_EFI.fd \
        {drive_arg} \
        {self.nic_command} \
        -rtc clock=vm \
        {self.loadvm} \
        {'-monitor telnet::%d,server,nowait' % monitor_port if monitor_port is not None else ''} \
        {self.cd_rom} \
        -nographic -no-reboot"""
        print("="*50+"QEMU command arguments:"+"="*50)
        print(qemu_args)
        return qemu_args

    def quantum_args(self) -> str:
        quantum_command = ''
        # TODO check why 53 : checked this is a check done to see whether or not we need to do checkpointing, with the assumption being it will usually be way less than the sampling interval
        if self.simulation_context.is_parallel:
            quantum_command = (
                f'   -quantum size={self.quantum_size},check_period={self.check_period} '
            )
        else:
            quantum_command = (
                f'   -icount shift=0,align=off,sleep=off,q={self.quantum_size},'
                f'check_period={self.check_period} '
            )
        print("="*50+"Quantum command arguments:"+"="*50)
        print(quantum_command)
        return quantum_command
 

    
