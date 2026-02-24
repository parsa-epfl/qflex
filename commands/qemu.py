import os
from commands.config import ExperimentContext

class QemuCommonArgParser:
    def __init__(self, 
                 experiment_context: ExperimentContext):
        self.experiment_context = experiment_context
        self.simulation_context = self.experiment_context.simulation_context
        self.image_address = self.experiment_context.get_local_image_address()
        self.memory_size_mb = self.experiment_context.simulation_context.memory_gb * 1024
        self.cores = self.experiment_context.simulation_context.core_count
        self.double_cores = self.experiment_context.simulation_context.doubled_vcpu
        self.core_coeff = 1
        if self.double_cores:
            self.core_coeff = 2

        
        self.node_number = self.experiment_context.node_number


    
        
        self.cd_rom = ''
        self.use_cd_rom = self.simulation_context.use_cd_rom
        if self.use_cd_rom:
            alpine_image_name = 'alpine-virt-3.22.1-aarch64.iso'
            experiment_folder = self.experiment_context.get_experiment_folder_address()
            if not os.path.isfile(f'{experiment_folder}/images/{alpine_image_name}'):
                alpine_url = f'https://dl-cdn.alpinelinux.org/alpine/v3.22/releases/aarch64/{alpine_image_name}'
                os.system(f'wget {alpine_url} -O {experiment_folder}/images/{alpine_image_name}')
            self.cd_rom = f'-cdrom {experiment_folder}/images/{alpine_image_name} '
        


    def get_load_vm(self) -> str:
        loadvm = ''
        if self.experiment_context.loadvm_name is not None and len(self.experiment_context.loadvm_name) > 0:
            loadvm = f' -loadvm {self.experiment_context.loadvm_name}'
        return loadvm

    def get_seed_image_arg(self) -> str:
        return f""" -drive if=virtio,file={self.experiment_context.seed_image_address},format=qcow2 """

    def get_base_image_arg(self) -> str:
        return f"""-drive if=virtio,file={self.image_address},format=qcow2 """

    def get_image_arg(self) -> str:
        image_arg = self.get_base_image_arg()
        if len(self.experiment_context.seed_image_name) > 0:
            print(f"Using seed image {self.experiment_context.seed_image_name} at address {self.experiment_context.seed_image_address}")
            image_arg += self.get_seed_image_arg()
        return image_arg

    def get_qemu_base_args(self) -> str:

        # TODO REMOVE the hardcoded drive 
        # TODO remove second seed file
        # self.node_number = 1
        # self.image_address = '/mnt/sdb/pooria-multi-node/vanilla-image-node1.img'

        image_arg = self.get_image_arg()
        loadvm = self.get_load_vm()

        telnet_monitor_arg = ''
        if self.experiment_context.use_telnet_monitor:
            print(f"Using telnet monitor at port {self.experiment_context.telnet_port}")
            telnet_monitor_arg = f""" -monitor telnet:127.0.0.1:{self.experiment_context.telnet_port},server,nowait """
        
        
        qemu_args = f""" -M virt,gic-version=max,virtualization=off,secure=off \
        -smp {self.core_coeff * self.cores} \
        -cpu max,pauth=off -m {self.memory_size_mb} \
        -boot order=d,menu=on \
        -bios ./QEMU_EFI.fd \
        {image_arg} \
        -rtc clock=vm \
        {loadvm} \
        {self.cd_rom} \
        {self.simulation_context.qemu_nic} \
        {telnet_monitor_arg} \
        -serial mon:stdio -nographic -no-reboot """
        
        
        print("="*50+"QEMU command arguments:"+"="*50)
        print(qemu_args)
        return qemu_args

    def quantum_args(self) -> str:
        # TODO move this to its own class
        check_period_quantum_coeff = self.simulation_context.check_period_quantum_coeff
        quantum_command = ''
        # TODO check why 53 : checked this is a check done to see whether or not we need to do checkpointing, with the assumption being it will usually be way less than the sampling interval
        if self.simulation_context.is_parallel:
            quantum_command = f'   -quantum size={self.simulation_context.quantum_size},check_period={int(self.simulation_context.quantum_size * check_period_quantum_coeff)} '
        else:
            quantum_command = f'   -icount shift=0,align=off,sleep=off,q={self.simulation_context.quantum_size},check_period={int(self.simulation_context.quantum_size * check_period_quantum_coeff)} '

        print("="*50+"Quantum command arguments:"+"="*50)
        print(quantum_command)
        return quantum_command


class VanillaQemuArgParser(QemuCommonArgParser):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 idx: int,
                 total_cycles: int):
        super().__init__(experiment_context)
        self.idx = idx
        self.total_cycles = total_cycles



    def get_load_vm(self):
        return f"""-loadvm snapshot_{self.idx},on-demand"""
    
    def get_seed_image_arg(self) -> str:
        return f"""-drive if=virtio,file={self.image_address},format=qcow2,snapshot=on,tmp-snapshot-name=snapshot_{self.idx} """
    
    def quantum_args(self) -> str:
        return f'   -icount shift=0,align=off,sleep=off '
    
    def get_qemu_base_args(self) -> str:
        base_args = super().get_qemu_base_args()
        # loadvm and image are already in
        quantum_command = self.quantum_args()
        single_step_command = f""" -singlestep -d nochain """
        log_command = f""" -D "qemu-timing.log" """
        lib_qflex_command = f""" -libqflex """
        lib_name = "libsemikraken" if self.double_cores else "libknottykraken"
        mode_command = f""" mode=timing,lib-path=../../lib/"{lib_name}".so,cfg-path=../../cfg/timing.cfg,cycles={self.total_cycles}:100000,debug=crit,ckpt-path=./snapshot_{self.idx}-flexus,freq=2 """
        
        qemu_args = base_args + \
        quantum_command + \
        single_step_command + \
        log_command + \
        lib_qflex_command + \
        mode_command
        print("="*50+"QEMU command arguments:"+"="*50)
        print(qemu_args)
        return qemu_args
        


        










