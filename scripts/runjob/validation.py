import os
import re
import logging
import argparse
import ConfigParser

import helpers

qemu_path = "qemu/aarch64-softmmu/qemu-system-aarch64"

def validate_instance(config_path):

    ## Check if config file exists
    if not os.path.isfile(config_path):
        logging.error("Config file '{0}' does not exist".format(config_path))
        return False

    ## Set up config parser
    config = ConfigParser.ConfigParser()
    try:
        config.read(config_path)
    except ConfigParser.ParsingError:
        logging.error("Parsing errors in config file '{0}'".format(config_path))
        return False

    ## Sections
    if not config.has_section("Environment"):
        logging.error("'Environment' section missing from config file '{0}' ".format(config_path))
        return False
    if not config.has_section("Machine"):
        logging.error("'Machine' section missing from config file '{0}' ".format(config_path))
        return False

    ## Set valid to False if any config variable is not properly set, else return true
    valid = True

    ## Environment
    # qemu path
    try:
        qflex_path = config.get("Environment", "qflex_path")
        if not os.path.isdir(qflex_path):
            logging.error("qflex repository is not found at '{0}'".format(qflex_path))
            valid = False
        else:
            if not os.path.isfile(os.path.join(qflex_path, qemu_path)):
                logging.error("qemu is not found at '{0}'".format(os.path.join(qflex_path, qemu_path)))
                valid = False
    except ConfigParser.NoOptionError:
        logging.error("qflex repository path is not set in config file")
        valid = False
    # Image repository path
    try:
        image_repo_path = config.get("Environment", "image_repo_path")
        if not os.path.isdir(image_repo_path):
            logging.error("Disk images repository is not found at '{0}'".format(image_repo_path))
            valid = False
    except ConfigParser.NoOptionError:
        logging.error("Disk images repository path is not set in config file")
        valid = False
    # Disk image path
    try:
        disk_image_path = config.get("Environment", "disk_image_path")
        disk_image_path = os.path.join(image_repo_path, disk_image_path)
        if not os.path.isfile(disk_image_path):
            logging.error("Disk image is not found at '{0}'".format(disk_image_path))
            valid = False
        flash_path = os.path.join(os.path.dirname(disk_image_path),"flash")
        if not os.path.isfile(flash_path+"0.img") or not os.path.isfile(flash_path+"1.img"):
            logging.error("Flash image is not found at '{0}'".format(flash_path))
    except ConfigParser.NoOptionError:
        logging.error("Disk image path is not set in config file")
        valid = False
    # External snapshot path
    if config.has_option("Environment", "starting_snapshot"):
        starting_snapshot = config.get("Environment", "starting_snapshot")
        if starting_snapshot:
            snapshot_path = os.path.join(os.path.dirname(os.path.join(image_repo_path, disk_image_path)), starting_snapshot)
            if not os.path.isdir(snapshot_path):
                logging.error("Disk image starting snapshot is not found at '{0}'".format(os.path.join(os.path.dirname(os.path.join(image_repo_path, disk_image_path)), starting_snapshot)))
                valid = False

    ## Machine
    # Number of cores
    try:
        smp = config.getint("Machine", "qemu_core_count")
    except ValueError:
        logging.error("qemu core count value '{0}' is not valid".format(smp))
        valid = False
    except ConfigParser.NoOptionError:
        logging.error("Number of qemu cores is not set in config file")
        valid = False
    # Memory size
    try:
        mem = config.getint("Machine", "memory_size")
    except ValueError:
        logging.error("Memory size value '{0}' is not valid".format(mem))
        valid = False
    except ConfigParser.NoOptionError:
        logging.error("Memory size is not set in config file")
        valid = False
    # Machine type
    try:
        machine = config.get("Machine", "machine")
    except ConfigParser.NoOptionError:
        logging.error("Machine type is not set in config file")
        valid = False
    # CPU type
    try:
        cpu = config.get("Machine", "cpu")
    except ConfigParser.NoOptionError:
        logging.error("CPU is not set in config file")
        valid = False
    # Network
    # Only a limited set of the network parameters is implemented
    # If more parameters are required, please request their addition
    # FixMe: Validate conflicts in network configuration across instances
    if config.has_option("Machine", "user_network"):
        user_network = config.get("Machine", "user_network")
        if user_network == "on":
            try:
                net_id = config.get("Machine", "user_network_id")
                net_mac = config.get("Machine", "user_network_mac")
                net_hostfwd_protocol = config.get("Machine", "user_network_hostfwd_protocol")
                net_hostfwd_hostport = config.get("Machine", "user_network_hostfwd_hostport")
                net_hostfwd_guestport = config.get("Machine", "user_network_hostfwd_guestport")
                if not net_id or not net_mac or not net_hostfwd_protocol or not net_hostfwd_hostport or not net_hostfwd_guestport:
                    logging.error("User network parameters are not completely set in config file")
                    valid = False
                if not re.match("[0-9a-f]{2}([-:]?)[0-9a-f]{2}(\\1[0-9a-f]{2}){4}$", net_mac.lower()):
                    logging.error("User network mac address is not valid")
                    valid = False
                if net_hostfwd_protocol not in {"tcp", "udp"}:
                    logging.error("User network protocol must be either 'tcp' or 'udp'")
                    valid = False
                try:
                    int(net_hostfwd_hostport)
                    int(net_hostfwd_guestport)
                except ValueError:
                    logging.error("User network port values are not valid")
                    valid = False
            except ConfigParser.NoOptionError:
                logging.error("User network parameters are not completely set in config file")
                valid = False
    # External Port Forwarding
    # Device option is not verified, however, it must comply to qemu device options
    for port_option in ["serial", "parallel", "monitor", "qmp"]:
        if config.has_option("Machine", port_option):
            port_option_setting = config.get("Machine", port_option)
            if port_option_setting == "on":
                try:
                    port_option_dev = config.get("Machine", port_option + "_DEV")
                    if not port_option_dev:
                        logging.error("'{0}' parameter is not set in config file".format(port_option))
                        valid = False
                except ConfigParser.NoOptionError:
                    logging.error("'{0}' parameter is not set in config file".format(port_option))
                    valid = False

    ## Simulation
    if config.has_section("Simulation"):
        # Simulation type
        try:
            simulation_type = config.get("Simulation", "simulation_type")
        except ConfigParser.NoOptionError:
            logging.error("Simulation type is not set in config file")
            valid = False
        else:
            if simulation_type not in {"emulation", "trace", "timing", "phases", "checkpoints"}:
                logging.error("Simulation type is not valid")
                valid = False
            else:
                # Phases
                if simulation_type == "phases":
                    # Phases name
                    try:
                        phases_name = config.get("Simulation", "phases_name")
                        if not phases_name:
                            logging.error("Phases name is not set in config file")
                    except ConfigParser.NoOptionError:
                        logging.error("Phases name is not set config file")
                        valid = False
                    # Phases length
                    try:
                        phases_length = config.get("Simulation", "phases_length")
                    except ConfigParser.NoOptionError:
                        logging.error("Phases length is not set in config file")
                        valid = False
                    try:
                        helpers.check_phases_length(phases_length)
                    except ValueError:
                        logging.error("Phases length value '{0}' is not valid".format(phases_length))
                        valid = False
                # Flexus
                if simulation_type in {"trace", "timing", "checkpoints"}:
                    # Flexus path
                    try:
                        flexus_path = config.get("Simulation", "flexus_path")
                        if not os.path.isdir(flexus_path):
                            logging.error("Flexus repository is not found at '{0}'".format(flexus_path))
                            valid = False
                    except ConfigParser.NoOptionError:
                        logging.error("Flexus repository path is not set in config file")
                        valid = False
                    # User postload path
                    try:
                        user_postload_path = config.get("Simulation", "user_postload_path")
                        if not os.path.isfile(user_postload_path):
                            logging.error("User postload is not found at '{0}'".format(user_postload_path))
                            valid = False
                    except ConfigParser.NoOptionError:
                        logging.error("User postload path is not set in config file")
                        valid = False
                    # Flexus trace simulator path
                    if simulation_type in {"trace", "checkpoints"}:
                        try:
                            flexus_trace_path = config.get("Simulation", "flexus_trace_path")
                            if not os.path.isfile(os.path.join(flexus_path, flexus_trace_path)):
                                logging.error("Flexus trace simulator is not found at '{0}'".format(os.path.join(flexus_path, flexus_trace_path)))
                                valid = False
                        except ConfigParser.NoOptionError:
                            logging.error("Flexus trace simulator path is not set in config file")
                            valid = False
                    # Flexus timing simulator path
                    if simulation_type == "timing":
                        try:
                            flexus_timing_path = config.get("Simulation", "flexus_timing_path")
                            if not os.path.isfile(os.path.join(flexus_path, flexus_timing_path)):
                                logging.error("Flexus timing simulator is not found at '{0}'".format(os.path.join(flexus_path, flexus_timing_path)))
                                valid = False
                        except ConfigParser.NoOptionError:
                            logging.error("Flexus timing simulator path is not set in config file")
                            valid = False
                    # Simulation length
                    if simulation_type in {"trace", "timing"}:
                        try:
                            simulation_length = config.get("Simulation", "simulation_length")
                            helpers.input_to_number(simulation_length)
                        except ConfigParser.NoOptionError:
                            logging.error("Simulation length is not set in config file")
                            valid = False
                        except ValueError:
                            logging.error("Simulation length value '{0}' is not valid".format(simulation_length))
                            valid = False
                    # Checkpoints
                    if simulation_type == "checkpoints":
                        # Checkpoints number
                        try:
                            checkpoints_number = config.get("Simulation", "checkpoints_number")
                            helpers.input_to_number(checkpoints_number)
                        except ConfigParser.NoOptionError:
                            logging.error("Checkpoints number is not set in config file")
                            valid = False
                        except ValueError:
                            logging.error("Checkpoints number value '{0}' is not valid".format(checkpoints_number))
                            return False
                        # Checkpoints length
                        try:
                            checkpoints_length = config.get("Simulation", "checkpoints_length")
                            helpers.input_to_number(checkpoints_length)
                        except ConfigParser.NoOptionError:
                            logging.error("Checkpoints length is not set in config file")
                            valid = False
                        except ValueError:
                            logging.error("Checkpoints length value '{0}' is not valid".format(checkpoints_length))
                            valid = False

    ## Return output
    return valid

def validate_system(config_path):

    ## Check if config file exists
    if not os.path.isfile(config_path):
        logging.error("Config file '{0}' does not exist".format(config_path))
        return False

    ## Set up config parser
    config = ConfigParser.ConfigParser()
    try:
        config.read(config_path)
    except ConfigParser.ParsingError:
        logging.error("Parsing errors in config file '{0}'".format(config_path))
        return False

    ## Set valid to False if any config variable is not properly set, else return true
    valid = True

    ## Instances
    try:
        instances = config.items("Instances")
        if not instances:
            logging.error("No instances found in config file '{0}'".format(config_path))
            valid = False
        else:
            for (instance_name, instance_path) in instances:
                if not os.path.isfile(instance_path):
                    logging.error("Config file for instance '{0}' is not found at '{0}'".format(instance_name, instance_path))
                    valid = False
                else:
                    if not validate_instance(instance_path):
                        logging.error("Config file '{0}' is not valid".format(instance_path))
                        valid = False
    except ConfigParser.NoSectionError:
        logging.error("No instances found in config file '{0}'".format(config_path))
        valid = False
    except ConfigParser.NoOptionError:
        logging.error("No instances found in config file '{0}'".format(config_path))
        valid = False

    ## System
    # ICount
    if config.has_section("System"):
        if config.has_option("System", "icount"):
            icount = config.get("System", "icount")
            if icount == "on":
                try:
                    icount_shift = config.get("System", "icount_shift")
                    icount_sleep = config.get("System", "icount_sleep")
                    if not icount_shift or not icount_sleep:
                        logging.error("ICount parameters are not completely set in config file")
                        valid = False
                    if icount_shift is not "auto":
                        try:
                            int(icount_shift)
                        except ValueError:
                            logging.error("ICount shift value '{0}' is not valid".format(icount_shift))
                            valid = False
                    if icount_sleep not in {"on", "off"}:
                        logging.error("ICount sleep value '{0}' is not valid".format(icount_sleep))
                        valid = False
                except ConfigParser.NoOptionError:
                    logging.error("ICount parameters are not completely set in config file")
                    valid = False

    ## Multinode
    if len(instances) > 1 :
        if not config.has_section("Multinode"):
            logging.error("Multinode parameters are required for more than one instance")
        else:
            # NS3
            try:
                ns3_path = config.get("Multinode", "ns3_path")
                if not os.path.isdir(ns3_path):
                    logging.error("NS3 repository is not found at '{0}'".format(ns3_path))
                    valid = False
            except ConfigParser.NoOptionError:
                logging.error("NS3 repository path is not set in config file")
                valid = False

            # Quantum
            if config.has_option("Multinode", "quantum"):
                quantum = config.get("Multinode", "quantum")
                if quantum:
                    try:
                        helpers.input_to_number(quantum)
                    except ValueError:
                        logging.error("Quantum value '{0}' is not valid".format(quantum))
                        valid = False

    ## Return output
    return valid

def validate_job(config_path):

    ## Check if config file exists
    if not os.path.isfile(config_path):
        logging.error("Config file '{0}' does not exist".format(config_path))
        return False

    ## Set up config parser
    config = ConfigParser.ConfigParser()
    try:
        config.read(config_path)
    except ConfigParser.ParsingError:
        logging.error("Parsing errors in config file '{0}'".format(config_path))
        return False

    ## Set valid to False if any config variable is not properly set, else return true
    valid = True

    ## A job config file is a system config file extended with a simulation section
    valid = validate_system(config_path)

    if valid:
        ## Job
        # Job section is similar to the Simulation section in instance validation, to some extent
        if not config.has_section("Job"):
            logging.error("'Job' section missing from job config file")
            return False

        # Master instance
        try:
            master_instance = config.get("Job", "master_instance")
            if not master_instance:
                logging.error("Master instance is not set in config file")
                valid = False
            else:
                # Get the name of all instances and check if master instance is one of them
                instances = config.items("Instances")
                instances_names = [instance_tuple[0] for instance_tuple in instances]
                if master_instance not in instances_names:
                    logging.error("Master instance '{0}' does not correspond to any of the available instances".format(master_instance))
                    valid = False
        except ConfigParser.NoOptionError:
            logging.error("Master instance is not set in config file")
            valid = False

        # Image repository path
        try:
            image_repo_path = config.get("Job", "image_repo_path")
            if not os.path.isdir(image_repo_path):
                logging.error("Disk images repository is not found at '{0}'".fromat(image_repo_path))
                valid = False
        except ConfigParser.NoOptionError:
            logging.error("Disk images repository path is not set in config file")
            valid = False

        # Flexus path
        try:
            flexus_path = config.get("Job", "flexus_path")
            if not os.path.isdir(flexus_path):
                logging.error("Flexus repository is not found at '{0}'".format(flexus_path))
                valid = False
        except ConfigParser.NoOptionError:
            logging.error("Flexus repository path is not set in config file")
            valid = False

        # Flexus trace simulator path
        try:
            flexus_trace_path = config.get("Job", "flexus_trace_path")
            if not os.path.isfile(os.path.join(flexus_path, flexus_trace_path)):
                logging.error("Flexus trace simulator is not found at '{0}'".format(os.path.join(flexus_path, flexus_trace_path)))
                valid = False
        except ConfigParser.NoOptionError:
            logging.error("Flexus trace simulator path is not set in config file")
            valid = False

        # Flexus timing simulator path
        try:
            flexus_timing_path = config.get("Job", "flexus_timing_path")
            if not os.path.isfile(os.path.join(flexus_path, flexus_timing_path)):
                logging.error("Flexus timing simulator is not found at '{0}'".format(os.path.join(flexus_path, flexus_timing_path)))
                valid = False
        except ConfigParser.NoOptionError:
            logging.error("Flexus timing simulator path is not set in config file")
            valid = False

        # User postload path
        try:
            user_postload_path = config.get("Job", "user_postload_path")
            if not os.path.isfile(user_postload_path):
                logging.error("User postload is not found at '{0}'".format(user_postload_path))
                valid = False
        except ConfigParser.NoOptionError:
            logging.error("User postload path is not set in config file")
            valid = False

        # Phases name
        try:
            phases_name = config.get("Job", "phases_name")
            if not phases_name:
                logging.error("Phases name is not set in config file")
        except ConfigParser.NoOptionError:
            logging.error("Phases name is not set config file")
            valid = False

        # Phases length
        try:
            phases_length = config.get("Job", "phases_length")
        except ConfigParser.NoOptionError:
            logging.error("Phases length is not set in config file")
            valid = False
        try:
            helpers.check_phases_length(phases_length)
        except ValueError:
            logging.error("Phases length value '{0}' is not valid".format(phases_length))
            valid = False

        # Checkpoints number
        try:
            checkpoints_number = config.get("Job", "checkpoints_number")
            helpers.input_to_number(checkpoints_number)
        except ConfigParser.NoOptionError:
            logging.error("Checkpoints number is not set in config file")
            valid = False
        except ValueError:
            logging.error("Checkpoints number value '{0}' is not valid".format(checkpoints_number))
            return False

        # Checkpoints length
        try:
            checkpoints_length = config.get("Job", "checkpoints_length")
            helpers.input_to_number(checkpoints_length)
        except ConfigParser.NoOptionError:
            logging.error("Checkpoints length is not set in config file")
            valid = False
        except ValueError:
            logging.error("Checkpoints length value '{0}' is not valid".format(checkpoints_length))
            valid = False

        # Simulation length
        try:
            simulation_length = config.get("Job", "simulation_length")
            helpers.input_to_number(simulation_length)
        except ConfigParser.NoOptionError:
            logging.error("Simulation length is not set in config file")
            valid = False
        except ValueError:
            logging.error("Simulation length value '{0}' is not valid".format(simulation_length))
            valid = False

    ## Return output
    return valid

def validate_docker(
    job_config_name,
    host_config_path,
    host_flexus_path,
    host_image_repo_path,
    target_config_path,
    target_flexus_path,
    target_image_repo_path,
    docker_image
):
    # Start a container in the background
    subprocess.call([
        "docker", "run", "--name", "test-container", "--rm", "-t", "-d",
        "-e", "LOCAL_USER_ID=" + str(os.getuid()),
        "-e", "LOCAL_GROUP_ID=" + str(os.getgid()),
        "-v", host_config_path + ":" + target_config_path,
        "-v", host_flexus_path + ":" + target_flexus_path,
        "-v", host_image_repo_path + ":" + target_image_repo_path,
        docker_image
    ])
    # Copy this validation script to the container
    subprocess.call(["docker", "cp", "validation.py", "test-container:/home/user"])
    # Run validation script in the container
    output = subprocess.check_output([
        "docker", "exec", "test-container",
        "python", "/home/user/validatation.py",
        "-p", os.path.join(target_config_path, job_config_name)
    ])
    # Terminate the container
    subprocess.call(["docker", "stop", "test-container"])
    # Check for errors
    if "Error" in output:
        print(output)
        return False
    else:
        return True

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-j", "--job")
    parser.add_argument("-s", "--system")
    parser.add_argument("-i", "--instance")
    args = parser.parse_args()

    if args.job:
        validate_job(args.job)
    elif args.system:
        validate_system(args.system)
    elif args.instance:
        validate_instance(args.instance)