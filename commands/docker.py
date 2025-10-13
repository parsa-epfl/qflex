import os
from commands import Executor
from .utils import get_docker_image_name
from .version import get_version

class DockerStarter(Executor):
    
    def __init__(self, 
                #  TODO change to experiment context
                 mounting_folder: str,
                 debug: bool = False,
                 worm: bool = False,
                 start_directory: str = None):
        self.debug = debug
        self.worm = worm
        self.version = get_version()
        self.docker_image_name = f"ghcr.io/parsa-epfl/qflex:{get_docker_image_name(debug=self.debug, worm=self.worm)}-{self.version}"
        self.images_folder = './images'
        self.mounting_folder = os.path.abspath(mounting_folder)
        print(f"============== Using QFlex version: {self.version} ==============")
        self.start_directory = ''
        if start_directory is not None and len(start_directory) > 0:
            self.start_directory = f" -w {start_directory} "


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


        
        # TODO remove unecessary mounts including .sh ones and micro_scripts
        return f"""
        docker run -it --entrypoint /bin/bash \
        -v {self.mounting_folder}:{self.mounting_folder} \
        -v {cwd}/sample_scripts:/home/dev/qflex/sample_scripts \
        -v {cwd}/QEMU_EFI.fd:/home/dev/qflex/QEMU_EFI.fd \
        -v {cwd}/templates:/home/dev/qflex/templates \
        -v {cwd}/typer_inputs:/home/dev/qflex/typer_inputs \
        -v {cwd}/commands:/home/dev/qflex/commands \
        -v {cwd}/qflex.args:/home/dev/qflex/qflex.args \
        -v {cwd}/partition.py:/home/dev/qflex/partition.py \
        -v {cwd}/result.py:/home/dev/qflex/result.py \
        -v {cwd}/micro_scripts:/home/dev/qflex/micro_scripts \
        --security-opt seccomp=unconfined \
        --cap-add SYS_PTRACE \
        {self.start_directory} \
        {commands_mount} {binary_mount} {self.docker_image_name}
        """


class DockerBuild(Executor):
    
    def __init__(self, 
                 debug: bool = False,
                 worm: bool = False,
                 worm_only: bool = False,
                 push: bool = False):
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
        self.push = push
        self.version = get_version()
        # TODO add checks to prevent overwriting existing images
        print(f"============== Building QFlex version: {self.version} ==============")

    def cmd(self) -> str:
        # TODO add a debug build with all the files so it can be used for development without remaking the image and compiled again
        
        local_qflex_name = f"{self.docker_base_image_name}:{self.version}"
        ghcr_qflex_name = f"ghcr.io/parsa-epfl/qflex:{self.docker_base_image_name}-{self.version}"

        local_worm_name = f"{self.docker_image_name_with_worm}:{self.version}"
        ghcr_worm_name = f"ghcr.io/parsa-epfl/qflex:{self.docker_image_name_with_worm}-{self.version}"

        # TODO centeralize the ghcr.io/parsa-epfl/qflex part
        if not self.worm_only:
            base_image_build_cmd = [
                f"""
                docker buildx build -t {local_qflex_name} --build-arg MODE={self.build_type} .
                """,
                f"docker tag {local_qflex_name} {ghcr_qflex_name}"
            ]
            base_image_push_cmd = [
                f"docker push {ghcr_qflex_name}"
            ]
        else:
            base_image_build_cmd = [
                f"echo 'Skipping base image build as --worm-only is set.'"
            ]
            base_image_push_cmd = [
                f"echo 'Skipping base image push as --worm-only is set.'"
            ]

        worm_image_cmd = [
            f"""
            docker buildx build -t {local_worm_name} --build-arg BASE_IMAGE={ghcr_qflex_name} -f Dockerfile.WormCache .
            """,
            f"docker tag {local_worm_name} {ghcr_worm_name}"
        ]
        # TODO add a seperate debug image that has the files that can be used for compilation and developement without remaking the docker image
        worm_image_push_cmd = [
            f"docker push {ghcr_worm_name}"
        ]

        base_cmd = base_image_build_cmd
        worm_cmd = worm_image_cmd
        if self.push:
            base_cmd += base_image_push_cmd
            worm_cmd += worm_image_push_cmd



        cmd = base_cmd
        if self.worm:
            cmd += worm_cmd

        return cmd

