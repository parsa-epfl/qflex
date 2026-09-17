

def get_docker_image_name(
        debug: bool = False,
        worm: bool = False,
        qpoints: bool = False,
        all_ext: bool = False,
):
    docker_image_name = 'qflex'
    if all_ext:
        docker_image_name += '-all'
    elif worm:
        docker_image_name += '-worm'
    elif qpoints:
        docker_image_name += '-qpoints'

    if debug:
        docker_image_name += '-debug'
    else:
        docker_image_name += '-release'
    
    return docker_image_name
