import os
from commands.config import ExperimentContext

class QemuCommonArgParser:
    def __init__(self, experiment_context: ExperimentContext):
        self.experiment_context = experiment_context
        self.simulation_context = self.experiment_context.simulation_context
        self.image_address = self.experiment_context.get_local_image_address()
        self.memory_size_mb = self.experiment_context.simulation_context.memory * 1024
        self.cores = self.experiment_context.simulation_context.core_count
        self.double_cores = self.experiment_context.simulation_context.doubled_vcpu
        self.core_coeff = 1
        if self.double_cores:
            self.core_coeff = 2

        
        
        self.nic_command = self.experiment_context.simulation_context.qemu_nic.strip().lower()
        if self.nic_command == 'none':
            self.nic_command = ''
        else:
            self.nic_command = f'-nic {self.nic_command},model=virtio-net-pci'

        self.loadvm = ''
        if self.experiment_context.loadvm_name is not None and len(self.experiment_context.loadvm_name) > 0:
            self.loadvm = f'-loadvm {self.experiment_context.loadvm_name}'
    
        check_period_quantum_coeff = self.simulation_context.check_period_quantum_coeff
        self.quantum_command = ''
        # TODO check why 53 : checked this is a check done to see whether or not we need to do checkpointing, with the assumption being it will usually be way less than the sampling interval
        if self.simulation_context.is_parallel:
            self.quantum_command = f'   -quantum size={self.simulation_context.quantum_size},check_period={self.simulation_context.quantum_size * check_period_quantum_coeff} '
        else:
            self.quantum_command = f'   -icount shift=0,align=off,sleep=off,q={self.simulation_context.quantum_size},check_period={self.simulation_context.quantum_size * check_period_quantum_coeff} '


        
    def get_qemu_base_args(self) -> str:
        return f""" -M virt,gic-version=max,virtualization=off,secure=off \
        -smp {self.core_coeff * self.cores} \
        -cpu max,pauth=off -m {self.memory_size_mb} \
        -boot menu=on \
        -bios ./QEMU_EFI.fd \
        -drive if=virtio,file={self.image_address},format=qcow2 \
        {self.nic_command} \
        -rtc clock=vm \
        {self.loadvm} \
        -nographic -no-reboot"""

    def quantum_args(self) -> str:
        return self.quantum_command
 

    






