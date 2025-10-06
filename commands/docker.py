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
        

        commands_mount = f" -v {cwd}/commands:/home/dev/qflex/commands"
        if not os.path.isdir('./commands'):
            commands_mount = ''

        binary_mount = f" -v {cwd}/qflex:/home/dev/qflex/qflex "
        if not os.path.exists('./qflex'):
            binary_mount = ''


        # make sure cfg folder exists
        if not os.path.isdir('./cfg'):
            os.makedirs('./cfg')
        
        if not os.path.isdir(f'./cfg/{self.experiment_name}'):
            os.makedirs(f'./cfg/{self.experiment_name}')

        # TODO remove unecessary mounts including .sh ones
        return f"""
        docker run -it --entrypoint /bin/bash \
        -v {cwd}/images:/home/dev/qflex/images \
        -v {cwd}/cfg/{self.experiment_name}:/home/dev/qflex/cfg/{self.experiment_name} \
        -v {cwd}/experiments:/home/dev/qflex/experiments \
        -v {cwd}/typer_inputs:/home/dev/qflex/typer_inputs \
        -v {cwd}/commands:/home/dev/qflex/commands \
        -v {cwd}/build-multiple-kraken_vanilla.py:/home/dev/qflex/build-multiple-kraken_vanilla.py \
        -v {self.image_folder}:{self.image_folder} \
        -v {cwd}/sample_scripts:/home/dev/qflex/sample_scripts \
        -v {cwd}/QEMU_EFI.fd:/home/dev/qflex/QEMU_EFI.fd \
        -v {cwd}/templates:/home/dev/qflex/templates \
        -v {cwd}/core_info.csv:/home/dev/qflex/core_info.csv \
        -v {cwd}/WormCache:/home/dev/qflex/WormCache \
        -v {cwd}/tmp_results:/home/dev/qflex/tmp_results \
        {commands_mount} {binary_mount} {self.docker_image_name}
        """
        

class DockerBuild(Executor):
    
    def __init__(self, 
                 debug: bool = False,
                 worm: bool = False,
                 worm_only: bool = False):
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
        self.worm_only = worm_only

    def cmd(self) -> str:
        
        if not self.worm_only:
            base_image_cmd = f"""
            docker buildx build -t {self.docker_base_image_name}:latest --build-arg MODE={self.build_type} .
            """
        else:
            base_image_cmd = f"echo 'Skipping base image build as --worm-only is set.'"

        worm_image_cmd = f"""
        docker buildx build -t {self.docker_image_name_with_worm}:latest --build-arg BASE_IMAGE={self.docker_base_image_name} -f Dockerfile.WormCache .
        """
        # TODO add a seperate debug image that has the files that can be used for compilation and developement without remaking the docker image
        
        if self.worm:
            return [
                base_image_cmd,
                worm_image_cmd
            ]
        else:
            return base_image_cmd

