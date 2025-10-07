import os
from commands import Executor
from .config import ExperimentContext
from commands.qemu import QemuCommonArgParser

class Boot(Executor):

    def __init__(self, 
                 experiment_context: ExperimentContext,
                 use_cd_rom: bool):
        self.experiment_context = experiment_context
        self.use_cd_rom = use_cd_rom
        self.qemu_common_parser = QemuCommonArgParser(experiment_context)

    def cmd(self) -> str:
        
        self.cd_rom = ''
        if self.use_cd_rom:
            alpine_image_name = 'alpine-virt-3.22.1-aarch64.iso'
            experiment_folder = self.experiment_context.get_experiment_folder_address()
            if not os.path.isfile(f'{experiment_folder}/images/{alpine_image_name}'):
                alpine_url = f'https://dl-cdn.alpinelinux.org/alpine/v3.22/releases/aarch64/{alpine_image_name}'
                os.system(f'wget {alpine_url} -O {experiment_folder}/images/{alpine_image_name}')
            self.cd_rom = f'-cdrom {experiment_folder}/images/{alpine_image_name} '

        boot_cmd = f"""
        ./qemu-system-aarch64 \
        {self.qemu_common_parser.get_qemu_base_args()} \
        {self.cd_rom} 
        """

        return [
            f"cd {self.experiment_context.get_experiment_folder_address()}/run",
            boot_cmd
        ]