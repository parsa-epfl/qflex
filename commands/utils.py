def get_vanila_qemu_build_folder() -> str:
    return './qemu_build'
def get_pflex_qemu_build_folder() -> str:
    return './p-qemu_build'

def get_docker_image_name(
        debug: bool = False,
        worm: bool = False
):
    docker_image_name = 'parsa/qflex'
    if worm:
        docker_image_name += '-worm'

    if debug:
        docker_image_name += '-debug'
    else:
        docker_image_name += '-release'
    
    return docker_image_name
