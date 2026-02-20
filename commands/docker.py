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
                 qpoints: bool = False,
                 all_ext: bool = False,
                 start_directory: str = None):
        self.debug = debug
        self.worm = worm
        self.qpoints = qpoints
        self.all_ext = all_ext or (worm and qpoints)
        self.version = get_version()
        self.docker_image_name = (
            f"ghcr.io/parsa-epfl/qflex:"
            f"{get_docker_image_name(debug=self.debug, worm=self.worm, qpoints=self.qpoints, all_ext=self.all_ext)}"
            f"-{self.version}"
        )
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

        qflex_args = ''
        if os.path.exists(f'{cwd}/qflex.args'):
            qflex_args = f'-v {cwd}/qflex.args:/home/dev/qflex/qflex.args '
        
        assert os.path.exists(f'{cwd}/QEMU_EFI.fd'), "QEMU_EFI.fd file not found in the current directory."
        assert os.path.exists(f'{cwd}/templates'), "templates folder not found in the current directory."
        assert os.path.exists(f'{cwd}/typer_inputs'), "typer_inputs folder not found in the current directory."
        assert os.path.exists(f'{cwd}/commands'), "commands folder not found in the current directory."
        assert os.path.exists(f'{cwd}/partition.py'), "partition.py file not found in the current directory."
        assert os.path.exists(f'{cwd}/result.py'), "result.py file not found in the current directory."
        
        micro_scripts = ''
        if os.path.exists(f'{cwd}/micro_scripts'):
            micro_scripts = f'-v {cwd}/micro_scripts:/home/dev/qflex/micro_scripts '


        
        # TODO remove unecessary mounts including .sh ones and micro_scripts
        return f"""
        docker run -it --entrypoint /bin/bash \
        -v {self.mounting_folder}:{self.mounting_folder} \
        -v {cwd}/QEMU_EFI.fd:/home/dev/qflex/QEMU_EFI.fd \
        -v {cwd}/templates:/home/dev/qflex/templates \
        -v {cwd}/typer_inputs:/home/dev/qflex/typer_inputs \
        -v {cwd}/commands:/home/dev/qflex/commands \
        {qflex_args} \
        -v {cwd}/partition.py:/home/dev/qflex/partition.py \
        -v {cwd}/result.py:/home/dev/qflex/result.py \
        {micro_scripts} \
        --security-opt seccomp=unconfined \
        --cap-add SYS_PTRACE \
        {self.start_directory} \
        {commands_mount} {binary_mount} {self.docker_image_name}
        """


class DockerBuild(Executor):
    
    def __init__(self, 
                 debug: bool = False,
                 worm: bool = False,
                 qpoints: bool = False,
                 all_ext: bool = False,
                 worm_only: bool = False,
                 push: bool = False):
        self.debug = debug
        self.worm = worm
        self.qpoints = qpoints
        self.all_ext = all_ext or (worm and qpoints)
        if self.worm_only and (not self.worm or self.qpoints or self.all_ext):
            raise AssertionError("--worm-only can only be used with --worm.")
        if self.worm or self.all_ext:
            # Check if folder "WormCacheQFlex" exists
            assert os.path.isdir('./WormCacheQFlex'), "WormCacheQFlex folder not found. Please clone the WormCacheQFlex repository."
        if self.qpoints or self.all_ext:
            assert os.path.isdir('./QPoints'), "QPoints folder not found. Please clone the QPoints repository."
        self.docker_base_image_name = get_docker_image_name(debug=self.debug, worm=False, qpoints=False, all_ext=False)
        self.docker_image_name_with_worm = get_docker_image_name(debug=self.debug, worm=True, qpoints=False, all_ext=False)
        self.docker_image_name_with_qpoints = get_docker_image_name(debug=self.debug, worm=False, qpoints=True, all_ext=False)
        self.docker_image_name_with_all = get_docker_image_name(debug=self.debug, worm=False, qpoints=False, all_ext=True)
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

        local_qpoints_name = f"{self.docker_image_name_with_qpoints}:{self.version}"
        ghcr_qpoints_name = f"ghcr.io/parsa-epfl/qflex:{self.docker_image_name_with_qpoints}-{self.version}"

        local_all_name = f"{self.docker_image_name_with_all}:{self.version}"
        ghcr_all_name = f"ghcr.io/parsa-epfl/qflex:{self.docker_image_name_with_all}-{self.version}"

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
            docker buildx build -t {local_worm_name} --build-arg BASE_IMAGE={ghcr_qflex_name} -f Dockerfile.WormCacheQFlex .
            """,
            f"docker tag {local_worm_name} {ghcr_worm_name}"
        ]
        # TODO add a seperate debug image that has the files that can be used for compilation and developement without remaking the docker image
        worm_image_push_cmd = [
            f"docker push {ghcr_worm_name}"
        ]

        qpoints_image_cmd = [
            f"""
            docker buildx build -t {local_qpoints_name} --build-arg BASE_IMAGE={ghcr_qflex_name} -f Dockerfile.QPoints .
            """,
            f"docker tag {local_qpoints_name} {ghcr_qpoints_name}"
        ]
        qpoints_image_push_cmd = [
            f"docker push {ghcr_qpoints_name}"
        ]

        all_image_cmd = [
            f"""
            docker buildx build -t {local_all_name} --build-arg BASE_IMAGE={ghcr_qflex_name} -f Dockerfile.QFlexAll .
            """,
            f"docker tag {local_all_name} {ghcr_all_name}"
        ]
        all_image_push_cmd = [
            f"docker push {ghcr_all_name}"
        ]

        base_cmd = base_image_build_cmd
        worm_cmd = worm_image_cmd
        qpoints_cmd = qpoints_image_cmd
        all_cmd = all_image_cmd
        if self.push:
            base_cmd += base_image_push_cmd
            worm_cmd += worm_image_push_cmd
            qpoints_cmd += qpoints_image_push_cmd
            all_cmd += all_image_push_cmd



        cmd = base_cmd
        if self.all_ext:
            cmd += all_cmd
        elif self.worm:
            cmd += worm_cmd
        elif self.qpoints:
            cmd += qpoints_cmd

        return cmd
