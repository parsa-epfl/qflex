import os
import shlex
from commands import Executor
from .utils import get_docker_image_name
from .version import get_version

DEFAULT_CONTAINER_NAME = "qflex-dev"


def _read_host_dns() -> tuple[list[str], list[str]]:
    """Return ``(servers, search_domains)`` discovered from the host's resolv.conf.

    Why this exists: on systemd-resolved hosts (the common case on Ubuntu) the
    real /etc/resolv.conf is a stub pointing at 127.0.0.53, which Docker filters
    out when copying the file into the container — Docker then falls back to
    8.8.8.8, which can be unreachable on locked-down networks. Discovering the actual upstream resolvers here
    and passing them via ``--dns`` flags sidesteps both the stub and the 8.8.8.8
    fallback.

    Order: prefer ``/run/systemd/resolve/resolv.conf`` (systemd-resolved's
    upstream-list view) over ``/etc/resolv.conf`` (which may be the stub).
    Localhost addresses are always skipped — they aren't reachable from inside
    the container. Returns empty lists if nothing usable is found, in which case
    the caller should pass no ``--dns`` flag and let Docker do its default
    thing.
    """
    candidates = ("/run/systemd/resolve/resolv.conf", "/etc/resolv.conf")
    for path in candidates:
        if not os.path.exists(path):
            continue
        try:
            with open(path) as f:
                lines = f.readlines()
        except OSError:
            continue
        servers: list[str] = []
        searches: list[str] = []
        for raw in lines:
            line = raw.strip()
            if line.startswith("nameserver "):
                addr = line.split(None, 1)[1].split("#", 1)[0].strip()
                if addr.startswith("127.") or addr == "::1":
                    continue
                if addr and addr not in servers:
                    servers.append(addr)
            elif line.startswith("search ") or line.startswith("domain "):
                for d in line.split()[1:]:
                    d = d.split("#", 1)[0].strip()
                    if d and d not in searches:
                        searches.append(d)
        if servers:
            return servers, searches
    return [], []


class DockerStarter(Executor):

    def __init__(self,
                #  TODO change to experiment context
                 mounting_folder: str,
                 debug: bool = False,
                 worm: bool = False,
                 start_directory: str = None,
                 background: bool = False,
                 container_name: str = DEFAULT_CONTAINER_NAME):
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
        # When `background` is True, the container is started detached, named, and
        # kept alive with `tail -f /dev/null`. Subsequent `./dep exec` calls then
        # `docker exec` into it without paying the per-command container-start cost.
        # See the `dep` and `run-in-dev-container` skills.
        self.background = background
        self.container_name = container_name


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

        sample_scripts = ''
        if os.path.exists(f'{cwd}/sample_scripts'):
            sample_scripts = f'-v {cwd}/sample_scripts:/home/dev/qflex/sample_scripts '

        tests_mount = ''
        if os.path.exists(f'{cwd}/tests'):
            tests_mount = f'-v {cwd}/tests:/home/dev/qflex/tests '

        # Background mode: detach + name the container + keep it alive so subsequent
        # `./dep exec` calls land inside the same container. tail -f /dev/null is the
        # standard "do nothing forever" idiom.
        if self.background:
            attach_flags = f"-d --name {self.container_name}"
            tail = ' -c "tail -f /dev/null"'
        else:
            attach_flags = "-it"
            tail = ""

        # DNS is discovered from the host (not exposed on the CLI) — see
        # _read_host_dns above for why the stub /etc/resolv.conf isn't enough.
        host_dns_servers, host_dns_search = _read_host_dns()
        dns_parts = [f"--dns {srv}" for srv in host_dns_servers]
        dns_parts += [f"--dns-search {dom}" for dom in host_dns_search]
        dns_flags = " ".join(dns_parts)

        # TODO remove unecessary mounts including .sh ones and micro_scripts
        # TODO make shared memory size equal to 512 * number of partitions
        return f"""
        docker run {attach_flags} --entrypoint /bin/bash \
        -v {self.mounting_folder}:{self.mounting_folder} \
        -v {cwd}/QEMU_EFI.fd:/home/dev/qflex/QEMU_EFI.fd \
        -v {cwd}/templates:/home/dev/qflex/templates \
        -v {cwd}/typer_inputs:/home/dev/qflex/typer_inputs \
        -v {cwd}/dep_injection:/home/dev/qflex/dep_injection \
        -v {cwd}/commands:/home/dev/qflex/commands \
        -v {cwd}/flexus:/home/dev/qflex/flexus \
        {qflex_args} \
        -v {cwd}/partition.py:/home/dev/qflex/partition.py \
        -v {cwd}/result.py:/home/dev/qflex/result.py \
        -v {cwd}/Makefile:/home/dev/qflex/Makefile \
        -v {cwd}/multi-node-scripts/:/home/dev/qflex/multi-node-scripts \
        -v {cwd}/parallel-qemu/:/home/dev/qflex/parallel-qemu \
        -v {cwd}/qemu/:/home/dev/qflex/qemu \
        -v {cwd}/qemu-pdes/:/home/dev/qflex/qemu-pdes \
        -v {cwd}/multi-node-web-search/:/home/dev/qflex/multi-node-web-search \
        -v {cwd}/WormCacheQFlex:/home/dev/qflex/WormCacheQFlex \
        -v {cwd}/clean_up.sh:/home/dev/qflex/clean_up.sh\
        -v {cwd}/multi-node-experiments:/home/dev/qflex/multi-node-experiments \
        -v {cwd}/multi-node-web-search_virtio/:/home/dev/qflex/multi-node-web-search_virtio \
        -v {cwd}/multi-node-dc:/home/dev/qflex/multi-node-dc \
        -v {cwd}/multi-node-dc-old-shanqing:/home/dev/qflex/multi-node-dc-old-shanqing \
        -v {cwd}/experiments:/home/dev/qflex/experiments \
        -v {cwd}/conf:/home/dev/qflex/conf \
        -v {cwd}/qflex:/home/dev/qflex/qflex \
        -v {cwd}/result.py:/home/dev/qflex/result.py \
        -v {cwd}/collect.py:/home/dev/qflex/collect.py \
        -v {cwd}/result_new.py:/home/dev/qflex/result_new.py \
        -v {cwd}/requirements.txt:/home/dev/qflex/requirements.txt \
        {sample_scripts} \
        {tests_mount} \
        {micro_scripts} \
        --security-opt seccomp=unconfined \
        --cap-add SYS_PTRACE --cap-add SYS_ADMIN \
        --pid=host \
        --cap-add=NET_ADMIN --device=/dev/net/tun  \
        --shm-size=128g \
        {dns_flags} \
        {self.start_directory} \
        {commands_mount} {binary_mount} {self.docker_image_name}{tail}
        """


class DockerExec(Executor):
    """Run a single command inside the long-running QFlex dev container that was
    started with `./dep start-docker --background`. Uses `docker exec` (not a fresh
    `docker run`) so we don't pay the container-start cost per command and any state
    that lives on tmpfs / /home/dev / running processes is preserved across calls."""

    def __init__(self,
                 command: str,
                 container_name: str = DEFAULT_CONTAINER_NAME,
                 working_directory: str = "/home/dev/qflex"):
        self.command = command
        self.container_name = container_name
        self.working_directory = working_directory

    def cmd(self) -> str:
        # shlex.quote handles internal quotes / spaces — the user's command can be
        # arbitrary bash. The outer shell sees `docker exec ... /bin/bash -c '<cmd>'`.
        return (
            f"docker exec -w {self.working_directory} {self.container_name} "
            f"/bin/bash -c {shlex.quote(self.command)}"
        )


class DockerStop(Executor):
    """Stop and remove the named QFlex dev container. `docker rm -f` stops the
    container if running and removes it in one step. Idempotent: a missing container
    isn't an error (we swallow the rc with `|| true`) so this can be called as cleanup
    in fixtures or after crashed runs."""

    def __init__(self, container_name: str = DEFAULT_CONTAINER_NAME):
        self.container_name = container_name

    def cmd(self) -> str:
        return f"docker rm -f {self.container_name} 2>/dev/null || true"


class DockerBuild(Executor):

    def __init__(self,
                 debug: bool = False,
                 worm: bool = False,
                 worm_only: bool = False,
                 push: bool = False,
                 no_cache: bool = False):
        self.debug = debug
        self.worm = worm
        if self.worm:
            # Check if folder "WormCacheQFlex" exists
            assert os.path.isdir('./WormCacheQFlex'), "WormCacheQFlex folder not found. Please clone the WormCacheQFlex repository."
        self.docker_base_image_name = get_docker_image_name(debug=self.debug, worm=False)
        self.docker_image_name_with_worm = get_docker_image_name(debug=self.debug, worm=True)
        self.build_type = 'release'
        if self.debug:
            self.build_type = 'debug'
        self.worm_only = worm_only
        self.push = push
        self.no_cache = no_cache
        self.version = get_version()
        # TODO add checks to prevent overwriting existing images
        print(f"============== Building QFlex version: {self.version} ==============")

    def cmd(self) -> str:
        # TODO add a debug build with all the files so it can be used for development without remaking the image and compiled again

        local_qflex_name = f"{self.docker_base_image_name}:{self.version}"
        ghcr_qflex_name = f"ghcr.io/parsa-epfl/qflex:{self.docker_base_image_name}-{self.version}"

        local_worm_name = f"{self.docker_image_name_with_worm}:{self.version}"
        ghcr_worm_name = f"ghcr.io/parsa-epfl/qflex:{self.docker_image_name_with_worm}-{self.version}"

        cache_flag = " --no-cache" if self.no_cache else ""

        # TODO do some docker renamig
        dep_image_name = "qflex-dependencies"
        dep_docker_build_cmd = [
            f"""docker buildx build{cache_flag} -t {dep_image_name} . -f Dockerfile
            """,
        ]


        # TODO centeralize the ghcr.io/parsa-epfl/qflex part
        if not self.worm_only:
            base_image_build_cmd = [
                f"""
                docker buildx build{cache_flag} -t {local_qflex_name} . -f Dockerfile.qemu.{self.build_type} --build-arg BASE_IMAGE={dep_image_name}
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
            docker buildx build{cache_flag} -t {local_worm_name} --build-arg BASE_IMAGE={ghcr_qflex_name} -f Dockerfile.WormCacheQFlex .
            """,
            f"docker tag {local_worm_name} {ghcr_worm_name}"
        ]
        # TODO add a seperate debug image that has the files that can be used for compilation and developement without remaking the docker image
        worm_image_push_cmd = [
            f"docker push {ghcr_worm_name}"
        ]

        base_cmd = dep_docker_build_cmd + base_image_build_cmd
        worm_cmd = worm_image_cmd
        if self.push:
            base_cmd += base_image_push_cmd
            worm_cmd += worm_image_push_cmd



        cmd = base_cmd
        if self.worm:
            cmd += worm_cmd

        return cmd
