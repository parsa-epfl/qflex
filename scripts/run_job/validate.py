import os
import sys
import subprocess
import ConfigParser
import argparse

def validate_local(arguments):
    valid = True

    # Setup argument parser
    parser = argparse.ArgumentParser(description = "Validates a Qflex run job configuration file")
    parser.add_argument("config_path", help = "Set path to .ini configuration file")
    args = parser.parse_args(arguments)

    # Setup config parser
    config = ConfigParser.ConfigParser(allow_no_value=True)
    config.read(args.config_path)
    qemu_path          = "qemu/aarch64-softmmu/qemu-system-aarch64"
    qflex_path         = config.get("Environment", "QFLEX_PATH")
    flexus_path        = config.get("Environment", "FLEXUS_PATH")
    image_repo_path    = config.get("Environment", "IMAGE_REPO_PATH")
    disk_image_path    = config.get("Environment", "DISK_IMAGE_PATH")
    kernel_path        = config.get("Environment","KERNEL_PATH")
    initrd_path        = config.get("Environment", "INITRD_PATH")
    flexus_trace_path  = config.get("Environment", "FLEXUS_TRACE_PATH")
    flexus_timing_path = config.get("Environment", "FLEXUS_TIMING_PATH")
    user_postload_path = config.get("Environment", "USER_POSTLOAD_PATH")
    starting_snapshot  = config.get("Environment", "STARTING_SNAPSHOT")
    smp                = config.getint("Machine", "QEMU_CORE_NUM")
    mem                = config.getint("Machine", "MEM_SIZE")

    # Check QEMU path
    if qflex_path is None:
        print("Error, QFLEX reporsitory path not set in configuration file")
        valid = False
    else:
        if not os.path.isdir(qflex_path):
            print("Error, QFLEX reporsitory not found at " + qflex_path)
            valid = False
        else:
            if not os.path.isfile(os.path.join(qflex_path, qemu_path)):
                print("Error, QEMU not found at " + os.path.join(qflex_path, qemu_path))
                valid = False

    # Check Flexus path
    if flexus_path is None:
        print("Error, Flexus reporsitory path not set in configuration file")
        valid = False
    else:
        if not os.path.isdir(flexus_path):
            print("Error, Flexus reporsitory not found at " + qflex_path)
            valid = False

    # Check image repository path
    if image_repo_path is None:
        print("Error, disk images reporsitory path not set in configuration file")
        valid = False
    else:
        if not os.path.isdir(image_repo_path):
            print("Error, Qdisk images reporsitory not found at " + image_repo_path)
            valid = False

    # Check disk image path
    if disk_image_path is None:
        print("Error, disk image path not set in configuration file")
        valid = False
    else:
        if not os.path.isfile(os.path.join(image_repo_path, disk_image_path)):
            print("Error, disk image not found at " + os.path.join(image_repo_path, disk_image_path))
            valid = False

    # Check kernel path
    if kernel_path is None:
        print("Error, kernel path not set in configuration file")
        valid = False
    else:
        if not os.path.isfile(os.path.join(image_repo_path, kernel_path)):
            print("Error, kernel not found at " + os.path.join(image_repo_path, kernel_path))
            valid = False

    # Check initial RAM disk path
    if initrd_path is None:
        print("Error, initial RAM disk path not set in configuration file")
        valid = False
    else:
        if not os.path.isfile(os.path.join(image_repo_path, initrd_path)):
            print("Error, initial RAM disk not found at " + os.path.join(image_repo_path, initrd_path))
            valid = False

    # Check flexus trace simulator path
    if flexus_trace_path is None:
        print("Error, Flexus trace simulator path not set in configuration file")
        valid = False
    else:
        if not os.path.isfile(os.path.join(flexus_path, flexus_trace_path)):
            print("Error, Flexus trace simulator not found at " + os.path.join(flexus_path, flexus_trace_path))
            valid = False

    # Check flexus timing simulator path
    if flexus_timing_path is None:
        print("Error, Flexus timing simulator path not set in configuration file")
        valid = False
    else:
        if not os.path.isfile(os.path.join(flexus_path, flexus_timing_path)):
            print("Error, Flexus timing simulator not found at " + os.path.join(flexus_path, flexus_timing_path))
            valid = False

    # Check user postload path
    if user_postload_path is None:
        print("Error, user postload path not set in configuration file")
        valid = False
    else:
        if not os.path.isfile(user_postload_path):
            print("Error, user postload not found at " + user_postload_path)
            valid = False

    # Check external snapshot path
    if starting_snapshot is not None:
        snapshot_path = os.path.join(os.path.dirname(os.path.join(image_repo_path, disk_image_path)), starting_snapshot)
        if not os.path.isdir(snapshot_path):
            print("Error, disk image starting snapshot not found at " + os.path.join(os.path.dirname(os.path.join(image_repo_path, disk_image_path)), starting_snapshot))
            valid = False

    # Check number of cores
    if not smp:
        print("Error, number of qemu cores not set in configuration file")
        valid = False

    # Check memory size
    if not mem:
        print("Error, memory size not set in configuration file")
        valid = False

    return valid


def validate_docker(config_path, docker_image, local_flexus_path, local_image_repo_path, flexus_path, image_repo_path):
    # Start a container in the background
    subprocess.call(["docker", "run", "--name", "test-container", "--rm", "-t", "-d", "-e", "LOCAL_USER_ID=" + str(os.getuid()), "-e", "LOCAL_GROUP_ID=" + str(os.getgid()),
        "-v", local_flexus_path + ":" + flexus_path, "-v", local_image_repo_path + ":" + image_repo_path, docker_image])
    # Copy the configuration file to the container
    subprocess.call(["docker", "cp", config_path, "test-container:/home/user"])
    # Copy this validation script to the container
    subprocess.call(["docker", "cp", "validate.py", "test-container:/home/user"])
    # Run validation script in the container
    output = subprocess.check_output(["docker", "exec", "test-container","python", "/home/user/validate.py", "/home/user/" + os.path.basename(config_path)])
    # Terminate the container
    subprocess.call(["docker", "stop", "test-container"])
    # Check for errors
    if "Error" in output:
        print(output)
        return False
    else:
        return True

## Execute script
if __name__ == '__main__':
    validate_local(sys.argv[1:])