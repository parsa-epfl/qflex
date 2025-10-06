

def get_docker_image_name(
        debug: bool = False,
        worm: bool = False
):
    docker_image_name = 'qflex'
    if worm:
        docker_image_name += '-worm'

    if debug:
        docker_image_name += '-debug'
    else:
        docker_image_name += '-release'
    
    return docker_image_name
