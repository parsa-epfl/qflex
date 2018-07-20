import os
import re
import sys
import time
import argparse
import subprocess
import ConfigParser
import qcg
import deploy
import helpers
import validate
import mrun.mrun as mrun

def run_job(arguments):
    # Argument parser help text
    description_text              = """Run Job: runs a full simulation following the SMARTS methodology: first it generates phases, then it generates checkpoints for each phase, and finally simulates each checkpoint"""
    helptext_job_name             = "Set name for job"
    helptext_sim_config_path      = "Set path to .ini configuration file of simulated instance"
    helptext_config_paths         = "Set path to .ini configuration file of emulated instances"
    helptext_run_local            = "Set to run job locally, unset to use kubernetes deployment"
    helptext_phases_length        = "Set number of phases and length of each phase using a list separated by columns: 'a:b:c' sets 3 steps of length a, b, and c respectively"
    helptext_phases_name          = "Set name of phases generated, use forward slash '/' to create directories"
    helptext_checkpoints_number   = "Set number of checkpoints per phase"
    helptext_checkpoints_length   = "Set length of each checkpoint"
    helptext_simulation_length  = "Set length of timing simulation at each checkpoint"
    epilog_text                   = """Example:
    python job-name config-file-simulated [config-file-emulated-1 config-file-emulated-2 ...] --phases_length 10:10k:10m:10b --phases_name new/phase --checkpoints_length 10K --checkpoints_number 100 --simulation_length 1B"""

    # Argument parser setup
    parser = argparse.ArgumentParser(description = description_text, epilog = epilog_text, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("job_name",             help = helptext_job_name)
    parser.add_argument("sim_config_path",      help = helptext_sim_config_path)
    parser.add_argument("--config_paths",       help = helptext_config_paths, nargs = '+')
    parser.add_argument("--run_local",          help = helptext_run_local, action = "store_true")
    parser.add_argument("--phases_length",      help = helptext_phases_length)
    parser.add_argument("--phases_name",        help = helptext_phases_name)
    parser.add_argument("--checkpoints_number", help = helptext_checkpoints_number)
    parser.add_argument("--checkpoints_length", help = helptext_checkpoints_length)
    parser.add_argument("--simulation_length",  help = helptext_simulation_length)
    args = parser.parse_args(arguments)

    # Verify arguments
    if not args.phases_length:
        parser.error("Argument phases_length must be provided")
    if not args.phases_name:
        parser.error("Argument phases_name must be provided")
    if not args.checkpoints_number:
        parser.error("Argument checkpoints_number must be provided")
    if not args.checkpoints_length:
        parser.error("Argument checkpoints_length must be provided")
    if not args.simulation_length:
        parser.error("Argument simulation_length must be provided")

    # Configuration parser setup
    config = ConfigParser.ConfigParser(allow_no_value=True)
    config.read(args.sim_config_path)
    docker_image_name     = config.get("Host", "DOCKER_IMAGE_NAME")
    local_flexus_path     = config.get("Host", "LOCAL_FLEXUS_PATH")
    local_image_repo_path = config.get("Host", "LOCAL_IMAGE_REPO_PATH")
    flexus_path           = config.get("Environment", "FLEXUS_PATH")
    image_repo_path       = config.get("Environment", "IMAGE_REPO_PATH")

    # Useful variables
    phases_num = helpers.phases_count(args.phases_length)
    xml_path = "/home/user/xml"             # Target mount point for xml files in the docker image
    local_xml_path = os.path.abspath("xml") # Directory for creating temporary xml files on host file system.
                                            # This directory should be empty before running run_job as it will be rewritten.
                                            # It can be converted to a user argument in the future
    if not args.run_local:
        deploy_arg_list = [                 # Kubernetes deployment arguments, dynamically add step_name and local_xml_path
            "--job_name", args.job_name,
            "--docker_image_name", docker_image_name,
            "--local_flexus_path", local_flexus_path,
            "--local_image_repo_path", local_image_repo_path,
            "--flexus_path", flexus_path,
            "--image_repo_path", image_repo_path
        ]

    # Validate config paths
    if not os.path.isfile(args.sim_config_path):
        raise ValueError("Error, configuration file not found at " + os.path.abspath(args.sim_config_path))
    if args.config_paths is not None:
        for config_path in args.config_paths:
            if not os.path.isfile(config_path):
                raise ValueError("Error, configuration file not found at " + os.path.abspath(config_path))

    # Validate config files
    if args.run_local:
        if not validate.validate_local([args.sim_config_path]):
            return
        if args.config_paths is not None:
            for config_path in args.config_paths:
                if not validate.validate_local([config_path]):
                    return
    # else:
    #     if not validate.validate_docker(args.sim_config_path, docker_image_name, local_flexus_path, local_image_repo_path, flexus_path, image_repo_path):
    #         return
    #     if args.config_paths is not None:
    #         for config_path in args.config_paths:
    #             if not validate.validate_docker(config_path, docker_image_name, local_flexus_path, local_image_repo_path, flexus_path, image_repo_path):
    #                 return

    # Remove subdirectory xml if it exists
    if os.path.isdir(local_xml_path):
        for root, dirs, files in os.walk(local_xml_path, topdown=False):
            for name in files:
                os.remove(os.path.join(root, name))
            for name in dirs:
                os.rmdir(os.path.join(root, name))
        os.rmdir(local_xml_path)

    # Phases
    # Create xml files
    phases_path = os.path.join(local_xml_path, "phases")
    os.makedirs(phases_path)
    sim_instance_name = "instance0"
    sim_instance_path = os.path.join(phases_path, sim_instance_name + ".xml")
    phases_args_list = [
        args.sim_config_path,
        "--phases",
        "--phases_length", args.phases_length,
        "--phases_name", args.phases_name,
        "--name", sim_instance_name
    ]
    qflex_command = qcg.generate_qflex_command(phases_args_list)
    helpers.create_instance_xml(qflex_command, sim_instance_path)
    instances_number = 1
    if args.config_paths is not None:
        for i, config_path in enumerate(args.config_paths):
            instance_name = "instance" + str(i+1)
            instance_path = os.join(phases_path, instance_name + ".xml")
            qflex_command = qcg.generate_qflex_command([config_path, "--name", instance_name])
            helpers.create_instance_xml(qflex_command, instance_path)
        instances_number = instances_number + len(args.config_paths)
    # Deploy phases
    if args.run_local:
        helpers.create_setup_xml(phases_path, instances_number)
        mrun_arg_list = ["-r", os.path.join(phases_path, "setup_file.xml")]
        mrun.main(mrun_arg_list)
    else:
        helpers.create_setup_xml_docker(phases_path, xml_path, instances_number)
        deploy_arg_list.extend(["--step_name", "phases", "--local_xml_path", phases_path, "--xml_path", xml_path])
        deploy.deploy(deploy_arg_list)
        del deploy_arg_list[-4:]

    # Verify end of phases
    if not args.run_local:
        disk_image_path = config.get("Environment", "DISK_IMAGE_PATH")
        phases_base_dir = os.path.join(local_image_repo_path, os.path.dirname(disk_image_path), args.phases_name)
        for i in range(phases_num):
            while not os.path.isdir(phases_base_dir + "_" + "%03d"%i):
                time.sleep(5)

    # Checkpoints
    # For each phase
    for j in range(phases_num):
        # Create xml files
        checkpoints_path = os.path.join(local_xml_path, "checkpoints-" + "%03d"%j)
        os.makedirs(checkpoints_path)
        sim_instance_name = "instance0"
        sim_instance_path = os.path.join(checkpoints_path , sim_instance_name + ".xml")
        snapshot_name = args.phases_name + "_" + "%03d"%j
        checkpoints_arg_list = [
            args.sim_config_path,
            "--checkpoints",
            "--checkpoints_number", args.checkpoints_number,
            "--checkpoints_length", args.checkpoints_length,
            "--snapshot", snapshot_name,
            "--name", sim_instance_name
        ]
        qflex_command = qcg.generate_qflex_command(checkpoints_arg_list)
        helpers.create_instance_xml(qflex_command, sim_instance_path)
        instances_number = 1
        if args.config_paths is not None:
            for i, config_path in enumerate(args.config_paths):
                instance_name = "instance" + str(i+1)
                instance_path = os.path.join(checkpoints_path, instance_name + ".xml")
                qflex_command = qcg.generate_qflex_command([config_path, "--name", instance_name])
                helpers.create_instance_xml(qflex_command, instance_path)
            instances_number = instances_number + len(args.config_paths)
        # Deploy checkpoints
        if args.run_local:
            helpers.create_setup_xml(checkpoints_path, instances_number)
            mrun_arg_list = ["-r", os.path.join(checkpoints_path, "setup_file.xml")]
            mrun.main(mrun_arg_list)
        else:
            helpers.create_setup_xml_docker(checkpoints_path, xml_path, instances_number)
            deploy_arg_list.extend(["--step_name", "checkpoints-" + "%03d"%j, "--local_xml_path", checkpoints_path, "--xml_path", xml_path])
            deploy.deploy(deploy_arg_list)
            del deploy_arg_list[-4:]

    # Verify end of checkpoints
    if not args.run_local:
        disk_image_path = config.get("Environment", "DISK_IMAGE_PATH")
        phases_base_dir = os.path.join(local_image_repo_path, os.path.dirname(disk_image_path), args.phases_name)
        for i in range(phases_num):
            checkpoints_base_dir = phases_base_dir + "_" + "%03d"%i + "_" + "ckpt"
            for j in range(int(args.checkpoints_number)-1): # Subtracting one due from the number of checkpoints to the bug in qflex checkpoints number
                while not os.path.isdir(checkpoints_base_dir + "_" + "%03d"%j):
                    time.sleep(5)

    # Simulation
    for j in range(phases_num):
        for k in range(int(args.checkpoints_number)-1): # Subtracting one due from the number of checkpoints to the bug in qflex checkpoints number
            # Create xml files
            simulation_path = os.path.join(local_xml_path, "simulation-" + "%03d"%j + "-" + "%03d"%k)
            os.makedirs(simulation_path)
            sim_instance_name = "instance0"
            sim_instance_path = os.path.join(simulation_path , sim_instance_name + ".xml")
            snapshot_name = args.phases_name + "_" + "%03d"%j + "_ckpt" + "%03d"%k
            simulation_arg_list = [
                args.sim_config_path,
                "--timing",
                "--simulation_length", args.simulation_length,
                "--snapshot", snapshot_name,
                "--name", sim_instance_name
            ]
            qflex_command = qcg.generate_qflex_command(simulation_arg_list)
            helpers.create_instance_xml(qflex_command, sim_instance_path)
            instances_number = 1
            if args.config_paths is not None:
                for i, config_path in enumerate(args.config_paths):
                    instance_name = "instance" + str(i+1)
                    instance_path = os.path.join(simulation_path, instance_name + ".xml")
                    qflex_command = qcg.generate_qflex_command([config_path, "--name", instance_name])
                    helpers.create_instance_xml(qflex_command, instance_path)
                instances_number = instances_number + len(args.config_paths)
            # Deploy simulation
            if args.run_local:
                helpers.create_setup_xml(simulation_path, instances_number)
                mrun_arg_list = ["-r", os.path.join(simulation_path, "setup_file.xml")]
                mrun.main(mrun_arg_list)
            else:
                helpers.create_setup_xml_docker(simulation_path, xml_path, instances_number)
                deploy_arg_list.extend(["--step_name", "checkpoints-" + "%03d"%j, "--local_xml_path", simulation_path, "--xml_path", xml_path])
                deploy.deploy(deploy_arg_list)
                del deploy_arg_list[-4:]

## Execute script
if __name__ == '__main__':
    run_job(sys.argv[1:])