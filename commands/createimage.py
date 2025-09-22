from commands import Executor
import os


class CreateImage(Executor):

    def __init__(self, 
                 image_name: str = 'base.qcow2',
                 size_gb: int = 8):
        self.image_name = image_name
        self.size_gb = size_gb
    
    def cmd(self) -> str:
        if not os.path.isdir('./images'):
            os.makedirs('./images')
        cwd = os.getcwd()
        return f'{cwd}/qemu-img create -f qcow2 {cwd}/images/{self.image_name} {self.size_gb}G'
        

