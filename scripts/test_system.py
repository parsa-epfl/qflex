import os
import sys
import time
import logging
import argparse
import threading
import subprocess
import ConfigParser

from core.validation import validate_system
from run_system import run_system

FNULL = open(os.devnull, 'w')

def ssh_connect(port):
    if not os.path.isfile("bash_scripts/ssh_connect.sh"):
        logging.error("Script 'bash_scripts/ssh_connect.sh' is missing")
        return False
    counter = 0
    while counter < 5:
        time.sleep(5)
        try:
            subprocess.check_call(["bash_scripts/ssh_connect.sh", port], stdout=FNULL, stderr=FNULL)
        except subprocess.CalledProcessError:
            counter += 1
        else:
            return True
    return False

def ssh_execute(port):
    if not os.path.isfile("bash_scripts/ssh_execute.sh"):
        logging.error("Script 'bash_scripts/ssh_execute.sh' is missing")
        return False
    if not os.path.isfile("bash_scripts/alive.sh"):
        logging.error("Script 'bash_scripts/alive.sh' is missing")
        return False
    try:
        output = subprocess.check_output(["bash_scripts/ssh_execute.sh", port], stdout=FNULL, stderr=FNULL)
        return "I am alive" in output
    except subprocess.CalledProcessError:
        return False

def save_external_snapshot(snapshot_name, instance_id):
    if not os.path.isfile("bash_scripts/save_external_snapshot.sh"):
        logging.error("Script 'bash_scripts/save_external_snapshot.sh' is missing")
        return False
    try:
        subprocess.check_call(["bash_scripts/save_external_snapshot.sh", snapshot_name, instance_id], stdout=FNULL, stderr=FNULL)
    except subprocess.CalledProcessError:
        return False
    else:
        return True

def test_system(arguments):
    # Note:
    # This script assumes that both TCP forwarding and qmp forwarding are set
    # qmp destination is /tmp/qmp-sock$i where $i is the instance position in the instance list, counting from 0

    # Argument parser setup
    help_text = "Test qflex: tests instances started by run system"
    parser = argparse.ArgumentParser(description = help_text, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("config_path",          help = "Path to config file of the job")
    parser.add_argument("-s", dest="snapshot",  help = "Name of new snapshot to be created")
    parser.add_argument("-f", dest="log_file",  help = "Save the log output in a file")
    parser.add_argument("-v", dest="log_level", help = "Set the verbosity of the log output", choices=['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL'])
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

    # Validate System
    if validate_system(args.config_path):
        logging.info("Configuration valid")
    else:
        logging.error("Configuration invalid")
        return False

    # Launch system
    qflex = threading.Thread(target=run_system, name="qflex_single", args=([args.config_path],))
    qflex.daemon = True
    qflex.start()

    # Collect list of instance config files
    config = ConfigParser.ConfigParser()
    config.read(args.config_path)
    instances_list = config.items("Instances")

    # For each instance
    for instance_id, (instance_name, instance_path) in enumerate(instances_list):

        # Parse TCP Port
        instance_config = ConfigParser.ConfigParser()
        instance_config.read(instance_path)
        port = instance_config.get("Machine", "user_network_hostfwd_hostport")

        # Test connectivity
        if ssh_connect(port):
            logging.info("SSH test passed")
        else:
            logging.error("SSH test failed")
            return False

        # Test snapshots
        if args.snapshot:
            if save_external_snapshot(args.snapshot, instance_id):
                logging.info("Save Snapshot test passed")
            else:
                logging.error("Save Snapshot test failed")
                return False

        # Test execution
        if ssh_execute(port):
            logging.info("SSH test passed")
        else:
            logging.error("SSH test failed")
            return False

# Execute script
if __name__ == '__main__':
    test_system(sys.argv[1:])