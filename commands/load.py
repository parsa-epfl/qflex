from commands import Executor
from .config import ExperimentContext
from .parameterloader import ParameterLoader
class Load(Executor):

    def __init__(self,
                 experiment_context: ExperimentContext,):
        self.experiment_context = experiment_context
        self.simulation_context = experiment_context.simulation_context
        self.core_coeff = 2 if self.simulation_context.doubled_vcpu else 1
        self.image_address = self.experiment_context.get_local_image_address()
        self.worm_parameter_loader = ParameterLoader(
            experiment_context=self.experiment_context,
            parameter_template='worm.rs.j2',
            output_name='worm_parameter.rs'
        )


    def qemu_cmd(self) -> str:
        clock_command = ''
        if self.simulation_context.is_parallel:
            clock_command = f'   -quantum size={self.simulation_context.quantum_size},check_period={self.simulation_context.quantum_size * 53} '
        else:
            clock_command = f'   -icount shift=0,align=off,sleep=off,q={self.simulation_context.quantum_size},check_period={self.simulation_context.quantum_size * 53} '
        # TODO this plugin exists outside, needs to be moved in the qemu folder later
        return f"""
        /qflex/p-qemu_build/qemu-system-aarch64 \
        -smp {self.core_coeff * self.simulation_context.core_count} \
        -M virt,gic-version=max,virtualization=off,secure=off \
        -cpu max,pauth=off -m {self.simulation_context.memory * 1024} \
        -boot menu=on \
        -bios ./qemu/build/pc-bios/edk2-aarch64-code.fd \
        -drive if=virtio,file={self.image_address},format=qcow2 \
        -nic {self.simulation_context.qemu_nic} \
        -rtc clock=vm \
        -plugin ./WormCache/target/release/libworm_cache.so,mode=pure_fill,prefix=init \
        {clock_command} \
        -loadvm base \
        -nographic -no-reboot
        """


    def cmd(self) -> str:
        

        worm_params = self.worm_parameter_loader.load_parameters()

        run_qemu_with_worm = self.qemu_cmd()

        self.experiment_context.get_ipns_csv(target='./cfg/core_info.csv')
        
        # WormCache/src/parameter.rss
        return [
            f"cd {self.experiment_context.get_experiment_folder_address()}",
            f"cp {worm_params} ./WormCache/src/parameter.rs",
            "cd ./WormCache",
            "cargo build --release",
            "cd ../",
            "cp ./WormCache/target/release/libworm_cache.so ",
            run_qemu_with_worm
        ]
