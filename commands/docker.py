import os
from commands import Executor
from .utils import get_docker_image_name
from .version import get_version

class DockerStarter(Executor):
    
    def __init__(self, 
                #  TODO change to experiment context
                 mounting_folder: str,
                 debug: bool = False,
                 start_directory: str = None):
        self.debug = debug
        self.version = get_version()
        self.docker_image_name = (
            f"docker.io/akrishnaams/"
            f"{get_docker_image_name(debug=self.debug)}"
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

        # The image only bakes in the built artifacts for flexus/qemu/parallel-qemu (to save
        # disk space). Mount the sources from the host if present, so they can be rebuilt in-container.
        flexus_mount = f" -v {cwd}/flexus:/home/dev/qflex/flexus "
        if not os.path.isdir('./flexus'):
            flexus_mount = ''

        qemu_mount = f" -v {cwd}/qemu:/home/dev/qflex/qemu "
        if not os.path.isdir('./qemu'):
            qemu_mount = ''

        parallel_qemu_mount = f" -v {cwd}/parallel-qemu:/home/dev/qflex/parallel-qemu "
        if not os.path.isdir('./parallel-qemu'):
            parallel_qemu_mount = ''

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
        {commands_mount} {binary_mount} {flexus_mount} {qemu_mount} {parallel_qemu_mount} {self.docker_image_name}
        """


class DockerBuild(Executor):
    
    def __init__(self, 
                 debug: bool = False,
                 push: bool = False):
        self.debug = debug
        self.push = push
        self.version = get_version()
        self.docker_image_name = get_docker_image_name(debug=self.debug)
        self.build_type = 'debug' if self.debug else 'release'
        # TODO add checks to prevent overwriting existing images
        assert os.path.isdir('./WormCacheQFlex'), "WormCacheQFlex folder not found. Please clone the WormCacheQFlex repository."
        assert os.path.isdir('./QPoints'), "QPoints folder not found. Please clone the QPoints repository."
        print(f"============== Building QFlex version: {self.version} ==============")

    def cmd(self) -> str:
        local_name = f"{self.docker_image_name}:{self.version}"
        remote_name = f"docker.io/akrishnaams/{self.docker_image_name}-{self.version}"

        cmd = [
            f"""
            docker buildx build --load -t {local_name} --build-arg MODE={self.build_type} --target runtime .
            """,
            f"docker tag {local_name} {remote_name}",
        ]
        if self.push:
            cmd.append(f"docker push {remote_name}")

        return cmd

