import os
from commands import Executor

class DockerStarter(Executor):
    
    def __init__(self, 
                 debug: bool = False,):
        self.debug = debug
        self.docker_image_name = 'parsa/qflex-'
        if self.debug:
            self.image_name = self.docker_image_name + 'debug'
        else:
            self.image_name = self.docker_image_name + 'release'
        
        self.images_folder = './images'

    def cmd(self) -> str:
        cwd = os.getcwd()
        # TODO remove unecessary mounts
        return f"""
        docker run -it --entrypoint bash \
        -v {cwd}/images:/qflex/images \
        -v {cwd}/commands:/qflex/commands \
        -v {cwd}/qflex:/qflex/qflex parsa/qflex-release
        """

class DockerBuild(Executor):
    
    def __init__(self, 
                 debug: bool = False,):
        self.debug = debug
        self.docker_image_name = 'parsa/qflex-'
        self.build_type = 'release'
        if self.debug:
            self.build_type = 'debug'
        self.docker_image_name += self.build_type

        
    def cmd(self) -> str:
        return f"""
        docker buildx build -t {self.docker_image_name}:latest --build-arg MODE={self.build_type} .
        """

