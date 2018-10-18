import os
import re
import sys
import time
import logging
import argparse
import ConfigParser

import core.helpers as helpers
import core.validation as validation
from deploy import deploy_system
from run_system import run_system

# FixMe Verify deployment code

def run_job(arguments):
    # Parse arguments
    help_text = """Run Job:
    Runs a simulation following the SMARTS methodology as specified in a job config file.
    First it generates phases, then it generates checkpoints for each phase, and finally, it simulates each checkpoint.
    Run Job can run locally or it can be deployed with kubernetes."""
    parser = argparse.ArgumentParser(description=help_text, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("job_name",             help="Name of the job")
    parser.add_argument("config_path",          help="Path to config file of the job")
    parser.add_argument("work_space",           help="Path to the workspace where config files used for the job are placed")
    parser.add_argument("-d", dest="deploy",    help="Deploy with kubernetes", action = "store_true")
    parser.add_argument("-f", dest="log_file",  help="Save the log output in a file")
    parser.add_argument("-v", dest="log_level", help="Set the verbosity of the log output", choices=['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL'])
    args = parser.parse_args(arguments)

    # Configure logging
    if __name__ == "__main__":
        if args.log_level:
            log_level = args.log_level
        else:
            log_level = "INFO"
        if args.log_file:
            logging.basicConfig(filename=args.log_file, level=log_level, format="%(levelname)s: %(message)s")
        else:
            logging.basicConfig(level=log_level, format="%(levelname)s: %(message)s")

    # Workspace
    # Directory for creating temporary config files on host file system
    host_workspace = os.path.join(args.work_space, args.job_name)
    if args.deploy:
        # Mount point for host workspace in the docker image
        target_workspace = "/home/user/workspace"
    # Remove workspace directory if it already exists
    if os.path.isdir(host_workspace):
        logging.warning("Workspace folder already exists, deleting '{0}'".format(host_workspace))
        for root, dirs, files in os.walk(host_workspace, topdown=False):
            for name in files:
                os.remove(os.path.join(root, name))
            for name in dirs:
                os.rmdir(os.path.join(root, name))
        os.rmdir(host_workspace)
    os.makedirs(host_workspace)

    # Config parser setup
    # Check if config file exists
    if not os.path.isfile(args.config_path):
        logging.error("Config file '{0}' does not exist".format(args.config_path))
        return False
    # Set up config parser
    job_config = ConfigParser.ConfigParser()
    try:
        job_config.read(args.config_path)
    except ConfigParser.ParsingError:
        logging.error("Parsing errors in config file '{0}'".format(args.config_path))
        return False
    # Prepare config files
    # Write instance config files in the workspace: Remove simulation section from each file, if available
    # Write job config file in the workspace: Replace instances with their new path in the job config file
    try:
        # Save instances
        instances = job_config.items("Instances")
        if not instances:
            logging.error("No instances found in config file '{0}'".format(args.config_path))
            return False
        # Keep track of all instances names and config paths
        instances_names = []
        instances_configs = []
        # Empty instances section
        job_config.remove_section("Instances")
        job_config.add_section("Instances")
    except ConfigParser.NoSectionError:
        logging.error("No instances found in config file '{0}'".format(args.config_path))
        return False
    except ConfigParser.NoOptionError:
        logging.error("No instances found in config file '{0}'".format(args.config_path))
        return False
    else:
        for (instance_name, instance_path) in instances:
            if os.path.isfile(instance_path):
                # Per-instance config parser
                instance_config = ConfigParser.ConfigParser()
                try:
                    instance_config.read(instance_path)
                except ConfigParser.ParsingError:
                    logging.error("Parsing errors in config file '{0}'".format(instance_path))
                    return False
                else:
                    # Remove simulation section from instance config
                    if instance_config.has_section("Simulation"):
                        instance_config.remove_section("Simulation")
                        logging.warning("Config file '{0}' contains 'Simulation' section, it will be ignored in this job".format(instance_path))
                    # Write new instance config file in the workspace
                    host_instance_path = os.path.abspath(os.path.join(host_workspace, instance_name))
                    instance_file = open(host_instance_path, 'w+')
                    instance_config.write(instance_file)
                    instance_file.close()
                    # Add new instance to the job config file
                    # Keep track of all instances names and configs parsers
                    if args.deploy:
                        target_instance_path = os.path.abspath(os.path.join(target_workspace, instance_name))
                        job_config.set("Instances", instance_name, target_instance_path)
                        instances_names.append(instance_name)
                        instances_configs.append(instance_config)
                    else:
                        job_config.set("Instances", instance_name, host_instance_path)
                        instances_names.append(instance_name)
                        instances_configs.append(instance_config)
            else:
                logging.error("Config file for instance '{0}' is not found at '{0}'".format(instance_name, instance_path))
                return False
    # Write new job config file in the workspace
    job_config_path = os.path.join(host_workspace, os.path.basename(args.config_path))
    job_file = open(job_config_path, 'w+')
    job_config.write(job_file)
    job_file.close()

    # Validate config files
    # Job validation includes per-instance validation
    if args.deploy:
        try:
            docker_image_name      = job_config.get("Host", "docker_image_name")
            host_flexus_path       = job_config.get("Host", "host_flexus_path")
            host_image_repo_path   = job_config.get("Host", "host_image_repo_path")
            target_flexus_path     = job_config.get("Job", "flexus_path")
            target_image_repo_path = job_config.get("Job", "image_repo_path")
        except ConfigParser.NoSectionError:
            logging.error("Host parameters are not set in config file")
            return False
        except ConfigParser.NoOptionError:
            logging.error("Host parameters are not set in config file")
            return False
        if not validation.validate_docker(
            job_config_path,
            host_workspace,
            host_flexus_path,
            host_image_repo_path,
            target_workspace,
            target_flexus_path,
            target_image_repo_path,
            docker_image_name
        ):
            logging.error("Config files are not valid")
            return False
    else:
        if not validation.validate_job(job_config_path):
            logging.error("Config files are not valid")
            return False

    # Kubernetes deployment arguments, dynamically add step_name and host_workspace
    # Step name, host workspace, and config file name are different for each job
    if args.deploy:
        deploy_arg_list = [
            "--job_name", args.job_name,
            "--docker_image_name", docker_image_name,
            "--host_flexus_path", host_flexus_path,
            "--host_image_repo_path", host_image_repo_path,
            "--target_flexus_path", target_flexus_path,
            "--target_image_repo_path", target_image_repo_path,
            "--target_workspace", target_workspace
        ]

    # Collect job parameters
    master_instance    = job_config.get("Job", "master_instance")
    image_repo_path    = job_config.get("Job", "image_repo_path")
    flexus_path        = job_config.get("Job", "flexus_path")
    flexus_trace_path  = job_config.get("Job", "flexus_trace_path")
    flexus_timing_path = job_config.get("Job", "flexus_timing_path")
    user_postload_path = job_config.get("Job", "user_postload_path")
    phases_name        = job_config.get("Job", "phases_name")
    phases_length      = job_config.get("Job", "phases_length")
    checkpoints_number = job_config.get("Job", "checkpoints_number")
    checkpoints_length = job_config.get("Job", "checkpoints_length")
    simulation_length  = job_config.get("Job", "simulation_length")
    phases_num         = helpers.phases_count(phases_length)

    # Convert the job config to system config
    job_config.remove_section("Job")
    job_config.remove_section("Host")
    job_config.remove_section("Instances")

    # Phases
    # Create config files
    phases_path = os.path.join(host_workspace, "phase-gen")
    os.makedirs(phases_path)
    job_config.add_section("Instances")
    for (instance_name, instance_config) in list(zip(instances_names, instances_configs)):
        # Update simulated instance
        if instance_name == master_instance:
            instance_config.add_section("Simulation")
            instance_config.set("Simulation", "simulation_type", "phases")
            instance_config.set("Simulation", "phases_name", phases_name)
            instance_config.set("Simulation", "phases_length", phases_length)
        # Write instance config file in the workspace
        host_instance_path = os.path.abspath(os.path.join(phases_path, instance_name))
        instance_file = open(host_instance_path, 'w+')
        instance_config.write(instance_file)
        instance_file.close()
        # Add new instance to the job config file
        # Keep track of all instances names and config paths
        if args.deploy:
            target_instance_path = os.path.abspath(os.path.join(target_workspace, instance_name))
            job_config.set("Instances", instance_name, target_instance_path)
        else:
            job_config.set("Instances", instance_name, host_instance_path)
        # Remove simulation section as the same config parser is used later
        if instance_name == master_instance:
            instance_config.remove_section("Simulation")
    # Write new job config file in the workspace
    job_config_path = os.path.join(phases_path, "phases_system.ini")
    job_file = open(job_config_path, 'w+')
    job_config.write(job_file)
    job_file.close()
    # Deploy phases
    if args.deploy:
        deploy_arg_list.extend(["--step_name", "phase-gen", "--host_workspace", phases_path, "--config_name", "phases_system.ini"])
        deploy.deploy(deploy_arg_list)
        del deploy_arg_list[-6:]
    else:
        run_system([job_config_path, "-o", phases_path])

    # Verify end of phases
    if args.deploy:
        # Get the relative path of the image from the master instance config file
        for (instance_name, instance_config) in list(zip(instances_names, instances_configs)):
            if instance_name == master_instance:
                disk_image_path = instance_config.get("Environment", "disk_image_path")
        phases_base_dir = os.path.join(host_image_repo_path, os.path.dirname(disk_image_path), phases_name)
        # Check that all phases where generated
        for i in range(phases_num):
            while not os.path.isdir(phases_base_dir + "_{:03d}".format(i)):
                time.sleep(5)

    # Checkpoints
    # For each phase
    for i in range(phases_num):
        # Create config files
        checkpoints_path = os.path.join(host_workspace, "checkpoint-gen-{:03d}".format(i))
        os.makedirs(checkpoints_path)
        # Clear old instances
        job_config.remove_section("Instances")
        job_config.add_section("Instances")
        for (instance_name, instance_config) in list(zip(instances_names, instances_configs)):
            # Update simulated instance
            if instance_name == master_instance:
                instance_config.add_section("Simulation")
                instance_config.set("Simulation", "simulation_type", "checkpoints")
                instance_config.set("Simulation", "flexus_path", flexus_path)
                instance_config.set("Simulation", "flexus_trace_path", flexus_trace_path)
                instance_config.set("Simulation", "user_postload_path", user_postload_path)
                instance_config.set("Simulation", "checkpoints_number", checkpoints_number)
                instance_config.set("Simulation", "checkpoints_length", checkpoints_length)
                instance_config.set("Environment", "starting_snapshot", phases_name + "_{:03d}".format(i))
            # Write instance config file in the workspace
            host_instance_path = os.path.abspath(os.path.join(checkpoints_path, instance_name))
            instance_file = open(host_instance_path, 'w+')
            instance_config.write(instance_file)
            instance_file.close()
            # Add new instance to the job config file
            # Keep track of all instances names and config paths
            if args.deploy:
                target_instance_path = os.path.abspath(os.path.join(target_workspace, instance_name))
                job_config.set("Instances", instance_name, target_instance_path)
            else:
                job_config.set("Instances", instance_name, host_instance_path)
            # Remove simulation section as the same config parser is used later
            if instance_name == master_instance:
                instance_config.remove_section("Simulation")
        # Write new job config file in the workspace
        job_config_path = os.path.join(checkpoints_path, "checkpoint_system.ini")
        job_file = open(job_config_path, 'w+')
        job_config.write(job_file)
        job_file.close()
        # Deploy phases
        if args.deploy:
            deploy_arg_list.extend(["--step_name", "checkpoint-gen-{:03d}".format(i), "--host_workspace", checkpoints_path, "--config_name", "checkpoint_system.ini"])
            deploy.deploy(deploy_arg_list)
            del deploy_arg_list[-6:]
        else:
            run_system([job_config_path, "-o", checkpoints_path])

    # Verify end of checkpoints
    if args.deploy:
        # Get the relative path of the image from the master instance config file
        for (instance_name, instance_config) in list(zip(instances_names, instances_configs)):
            if instance_name == master_instance:
                disk_image_path = instance_config.get("Environment", "disk_image_path")
        checkpoints_base_dir = os.path.join(host_image_repo_path, os.path.dirname(disk_image_path), phases_name)
        # Check that all phases where generated
        for i in range(phases_num):
            for j in range(helpers.input_to_number(checkpoints_number)-1):
                while not os.path.isdir(checkpoints_base_dir + "_{:03d}_ckpt{:03d}".format(i,j)):
                    time.sleep(5)

    # Simulation
    # For each phase
    for i in range(phases_num):
        # For each checkpoint
        # FixMe Subtracting one from the number of checkpoints due to a bug in the number of generated checkpoints by qflex
        for j in range(helpers.input_to_number(checkpoints_number)-1):
            # Create config files
            simulation_path = os.path.join(host_workspace, "simulation-{:03d}-{:03d}".format(i, j))
            os.makedirs(simulation_path)
            # Clear old instances
            job_config.remove_section("Instances")
            job_config.add_section("Instances")
            for (instance_name, instance_config) in list(zip(instances_names, instances_configs)):
                # Update simulated instance
                if instance_name == master_instance:
                    instance_config.add_section("Simulation")
                    instance_config.set("Simulation", "flexus_path", flexus_path)
                    instance_config.set("Simulation", "flexus_timing_path", flexus_timing_path)
                    instance_config.set("Simulation", "user_postload_path", user_postload_path)
                    instance_config.set("Simulation", "simulation_type", "simulation")
                    instance_config.set("Simulation", "simulation_length", simulation_length)
                    instance_config.set("Environment", "starting_snapshot", phases_name + "_{:03d}_ckpt{:03d}".format(i,j))
                # Write instance config file in the workspace
                host_instance_path = os.path.abspath(os.path.join(simulation_path, instance_name))
                instance_file = open(host_instance_path, 'w+')
                instance_config.write(instance_file)
                instance_file.close()
                # Add new instance to the job config file
                # Keep track of all instances names and config paths
                if args.deploy:
                    target_instance_path = os.path.abspath(os.path.join(target_workspace, instance_name))
                    job_config.set("Instances", instance_name, target_instance_path)
                else:
                    job_config.set("Instances", instance_name, host_instance_path)
                # Remove simulation section as the same config parser is used later
                if instance_name == master_instance:
                    instance_config.remove_section("Simulation")
            # Write new job config file in the workspace
            job_config_path = os.path.join(simulation_path, "simulation_system.ini")
            job_file = open(job_config_path, 'w+')
            job_config.write(job_file)
            job_file.close()
            # Deploy phases
            # if args.deploy:
            #     deploy_arg_list.extend(["--step_name", "simulation-{:03d}-{:03d}".format(i, j), "--host_workspace", simulation_path, "--config_name", "simulation_system.ini"])
            #     deploy.deploy(deploy_arg_list)
            #     del deploy_arg_list[-6:]
            # else:
            #     run_system([job_config_path, "-o", simulation_path])

## Execute script
if __name__ == '__main__':
    run_job(sys.argv[1:])