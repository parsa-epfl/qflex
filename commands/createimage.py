from commands import Executor
import os


class CreateImage(Executor):

    def __init__(self, 
                 image_folder: str,
                 image_name: str = 'root.qcow2',
                 size_gb: int = 8):
        self.image_name = image_name
        self.size_gb = size_gb
        self.image_folder = os.path.abspath(image_folder)

    def cmd(self) -> str:
        if not os.path.isdir(self.image_folder):
            raise ValueError(f"Image folder {self.image_folder} does not exist.")

        return [
            f'./qemu-img create -f qcow2 {self.image_folder}/{self.image_name} {self.size_gb}G',
            f'ls -lh {self.image_folder}/{self.image_name}',
            f"echo 'Created base image at {self.image_folder}/{self.image_name}'"
        ]

