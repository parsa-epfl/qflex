import os
from commands import Executor
from .utils import get_docker_image_name

class DockerStarter(Executor):
    
    def __init__(self, 
                #  TODO change to experiment context
                 image_folder: str,
                 experiment_name: str = 'default_experiment',
                 debug: bool = False,
                 worm: bool = False):
        self.debug = debug
        self.worm = worm
        self.docker_image_name = get_docker_image_name(debug=self.debug, worm=self.worm)
        self.images_folder = './images'
        self.experiment_name = experiment_name
        self.image_folder = image_folder

    def cmd(self) -> str:
        cwd = os.getcwd()
        
        # if images folder doesn't exist load it
        if not os.path.isdir(self.images_folder):
            os.makedirs(self.images_folder)
            # download alpine image
        

        commands_mount = f" -v {cwd}/commands:/qflex/commands"
        if not os.path.isdir('./commands'):
            commands_mount = ''

        binary_mount = f" -v {cwd}/qflex:/qflex/qflex "
        if not os.path.exists('./qflex'):
            binary_mount = ''


        # make sure cfg folder exists
        if not os.path.isdir('./cfg'):
            os.makedirs('./cfg')
        
        if not os.path.isdir(f'./cfg/{self.experiment_name}'):
            os.makedirs(f'./cfg/{self.experiment_name}')

        # TODO remove unecessary mounts
        return f"""
        docker run -it --entrypoint bash \
        -v {cwd}/images:/qflex/images \
        -v {cwd}/cfg/{self.experiment_name}:/qflex/cfg/{self.experiment_name} \
        -v {cwd}/experiments:/qflex/experiments \
        -v {cwd}/typer_inputs:/qflex/typer_inputs \
        -v {cwd}/commands:/qflex/commands \
        -v {self.image_folder}:{self.image_folder} \
        {commands_mount} {binary_mount} {self.docker_image_name}
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

