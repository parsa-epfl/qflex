import os
from commands import Executor
from .utils import get_docker_image_name

class DockerStarter(Executor):
    
    def __init__(self, 
                 debug: bool = False,
                 worm: bool = False):
        self.debug = debug
        self.worm = worm
        self.docker_image_name = get_docker_image_name(debug=self.debug, worm=self.worm)
        self.images_folder = './images'

    def cmd(self) -> str:
        cwd = os.getcwd()
        # TODO remove unecessary mounts
        return f"""
        docker run -it --entrypoint bash \
        -v {cwd}/images:/qflex/images \
        -v {cwd}/commands:/qflex/commands \
        -v {cwd}/qflex:/qflex/qflex {self.docker_image_name}
        """

class DockerBuild(Executor):
    
    def __init__(self, 
                 debug: bool = False,
                 worm: bool = False):
        self.debug = debug
        self.worm = worm
        if self.worm:
            # Check if folder "WormCache" exists
            assert os.path.isdir('./WormCache'), "WormCache folder not found. Please clone the WormCache repository."
        self.docker_base_image_name = get_docker_image_name(debug=self.debug, worm=False)
        self.docker_image_name_with_worm = get_docker_image_name(debug=self.debug, worm=True)
        self.build_type = 'release'
        if self.debug:
            self.build_type = 'debug'
        
    def cmd(self) -> str:
        
        base_image_cmd = f"""
        docker buildx build -t {self.docker_base_image_name}:latest --build-arg MODE={self.build_type} .
        """

        worm_image_cmd = f"""
        docker buildx build -t {self.docker_image_name_with_worm}:latest --build-arg BASE_IMAGE={self.docker_base_image_name} -f WormCache/Dockerfile .
        """
        # TODO add a seperate debug image that has the files that can be used for compilation and developement without remaking the docker image

        if self.worm:
            return [
                base_image_cmd,
                worm_image_cmd
            ]
        else:
            return base_image_cmd

