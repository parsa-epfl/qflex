import sys
import logging
import argparse

import core.system as system

def run_system(argv):
    # Parse arguments
    help_text = """Run system:
    Runs a single or multiple qflex instances as specified in a system config file.
    Each qflex instance is configured in an instance config file.
    Run system handles setting up the NS3 network when running multiple instances.
    Run system provides a QMP shell to interface with the qflex instances"""
    parser = argparse.ArgumentParser(description=help_text, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument(dest="file", metavar="SETUP_FILE", help="Run qflex instance(s) based on the system config file")
    parser.add_argument("-p", action="store_true", dest="print_command", help="Print qflex commands for all instances instead of launching the system")
    parser.add_argument("-qmp", action="store_true", dest="qmp", help="Launch a qmp shell to interface with the running qemu instances")
    parser.add_argument("-o", dest="output_path", help="Save output of each instance in a log file of the same name under given directory")
    parser.add_argument("-f", dest="log_file", help="Redirect logs to a file")
    parser.add_argument("-v", dest="log_level", help="Set the verbosity of the logs", choices=['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL'])
    args = parser.parse_args(argv)

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

    # Parse system
    qflex_system = system.system(args.file)
    if not qflex_system.parse_system():
        return False

    # Print command and exit
    if args.print_command:
        qflex_system.print_cmd()
        return True

    # Set save output
    if args.output_path:
        qflex_system.set_save_output(args.output_path)

    # Set QMP interface
    if args.qmp:
        qflex_system.set_qmp()

    # Start system
    qflex_system.start_execution()

    # Start QMP interface
    if args.qmp:
        qflex_system.start_qmp()

    # Wait for termination
    while not qflex_system.check_exit():
        pass

    # Report termination cause
    logging.debug("Exit Code: {0}".format(qflex_system.get_exit_message()))
    return True

if __name__ == "__main__":
    run_system(sys.argv[1:])