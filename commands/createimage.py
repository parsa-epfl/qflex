from commands import Executor
import os
from .config import get_experiment_folder_address


class CreateImage(Executor):

    def __init__(self, 
                 experiment_name: str,
                 working_directory: str = ".",
                 image_name: str = 'base.qcow2',
                 size_gb: int = 8):
        self.image_name = image_name
        self.size_gb = size_gb
        self.experiment_name = experiment_name
        self.working_directory = working_directory
    
    def cmd(self) -> str:
        if not os.path.isdir('./images'):
            os.makedirs('./images')
        experiment_folder = get_experiment_folder_address(self.experiment_name, self.working_directory)
        return f'{experiment_folder}/run/qemu-img create -f qcow2 {experiment_folder}/images/{self.image_name} {self.size_gb}G'
        

