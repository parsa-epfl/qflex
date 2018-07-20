import os
import sys
import argparse
import ConfigParser
import helpers

## Main
def generate_qflex_command(arguments):
    # Argument Parser help text
    description_text            = "QFlex Command Generator: Generate valid QFlex command based on a configuration file and input arguments"
    helptext_config_path        = "Set path to .ini configuration file"
    helptext_name               = "Set name for qemu instance"
    helptext_phases             = "Enable creating phases, pass starting point as an external snapshot"
    helptext_phases_length      = "Set number of phases and length of each phase using a list separated by columns: 'a:b:c' sets 3 steps of length a, b, and c respectively"
    helptext_phases_name        = "Set name of phases generated, use forward slash '/' to create directories"
    helptext_checkpoints        = "Enable creating checkpoints, ensure to pass starting phase as an external snapshot"
    helptext_checkpoints_number = "Set number of checkpoints per phase"
    helptext_checkpoints_length = "Set length of each checkpoint"
    helptext_trace              = "Enable trace simulation"
    helptext_timing             = "Enable timing simulation"
    helptext_simulation_length  = "Set length of simulation"
    helptext_snapshot           = "Override configuration file disk image snapshot with a new disk image snapshot"
    helptext_execute            = "Execute command generated"
    epilog_text                 = """
    Examples:
    python qcg.py config_file_path
    python qcg.py config_file_path [--execute]
    python qcg.py config_file_path --phases --phases_length 10:2k:10M:10b --phases_name new/phase [--execute]
    python qcg.py config_file_path --checkpoints --checkpoints_length 5K --checkpoints_number 30 [--execute]
    python qcg.py config_file_path --trace --simulation time 21300 [--execute]
    python qcg.py config_file_path --timing --simulation time 17K [--execute]"""

    # Argument Parser setup
    parser = argparse.ArgumentParser(description = description_text, epilog = epilog_text, formatter_class = argparse.RawTextHelpFormatter)
    parser.add_argument("config_path",          help = helptext_config_path)
    parser.add_argument("--name",               help = helptext_name)
    parser.add_argument("--phases",             help = helptext_phases, action = "store_true")
    parser.add_argument("--phases_length",      help = helptext_phases_length)
    parser.add_argument("--phases_name",        help = helptext_phases_name)
    parser.add_argument("--checkpoints",        help = helptext_checkpoints, action = "store_true")
    parser.add_argument("--checkpoints_number", help = helptext_checkpoints_number)
    parser.add_argument("--checkpoints_length", help = helptext_checkpoints_length)
    parser.add_argument("--trace",              help = helptext_trace, action = "store_true")
    parser.add_argument("--timing",             help = helptext_timing, action = "store_true")
    parser.add_argument("--simulation_length",  help = helptext_simulation_length)
    parser.add_argument("--snapshot",           help = helptext_snapshot)
    parser.add_argument("--execute",            help = helptext_execute, action = "store_true")
    args = parser.parse_args(arguments)

    # Configuration Parser setup
    config = ConfigParser.ConfigParser(allow_no_value=True)
    config.read(args.config_path)
    qemu_path          = "qemu/aarch64-softmmu/qemu-system-aarch64"
    qflex_path         = config.get("Environment", "QFLEX_PATH")
    flexus_path        = config.get("Environment", "FLEXUS_PATH")
    image_repo_path    = config.get("Environment", "IMAGE_REPO_PATH")
    disk_image_path    = config.get("Environment", "DISK_IMAGE_PATH")
    kernel_path        = config.get("Environment", "KERNEL_PATH")
    initrd_path        = config.get("Environment", "INITRD_PATH")
    flexus_trace_path  = config.get("Environment", "FLEXUS_TRACE_PATH")
    flexus_timing_path = config.get("Environment", "FLEXUS_TIMING_PATH")
    user_postload_path = config.get("Environment", "USER_POSTLOAD_PATH")
    starting_snapshot  = config.get("Environment", "STARTING_SNAPSHOT")
    smp                = config.getint("Machine", "QEMU_CORE_NUM")
    mem                = config.getint("Machine", "MEM_SIZE")
    disk_image_path    = os.path.join(image_repo_path, disk_image_path)
    kernel_path        = os.path.join(image_repo_path, kernel_path)
    initrd_path        = os.path.join(image_repo_path, initrd_path)
    flexus_trace_path  = os.path.join(flexus_path, flexus_trace_path)
    flexus_timing_path = os.path.join(flexus_path, flexus_timing_path)

    # Verify Arguments
    if args.phases:
        if args.phases_length is None:
            parser.error("Argument phases_length must be provided when phases is enabled")
        if args.phases_name is None:
            parser.error("Argument phases_name must be provided when phases is enabled")
        helpers.check_phases_length(args.phases_length)
    if args.checkpoints:
        if starting_snapshot is None and args.snapshot is None:
            parser.error("A disk image snapshot must be provided when checkpoints is enabled")
        if args.simulation_length is not None:
            parser.error("Argument simulation_length must not be provided with checkpoints enabled")
        if args.checkpoints_length is None:
            parser.error("Arrgument checkpoints_length must be provided when checkpoints are enabled")
        if args.checkpoints_number is None:
            parser.error("Argument checkpoints_number must be provided when checkpoints are enabled")
        checkpoint_length = str(helpers.input_to_number(args.checkpoints_length))
        checkpoint_total_length = str(int(checkpoint_length) * int(args.checkpoints_number))
    elif args.trace or args.timing:
        if args.simulation_length is None:
            parser.error("Argument simulation_length must be provided with simulation enabled")
        helpers.input_to_number(args.simulation_length)

    # Qflex command setup
    qflex_command = ""
    #Set QEMU path
    qflex_command = os.path.join(qflex_path, qemu_path)
    # Set number of cores
    qflex_command = qflex_command + " -smp " + str(smp)
    # Set memory size
    qflex_command = qflex_command + " -m " + str(mem)
    # Set kernel path
    qflex_command = qflex_command + " -kernel " + kernel_path
    # Set initial RAM disk path
    qflex_command = qflex_command + " -initrd " + initrd_path
    # Set disk image path
    qflex_command = qflex_command + " -global " + "virtio-blk-device.scsi=off"
    qflex_command = qflex_command + " -device " + "virtio-scsi-device,id=scsi"
    qflex_command = qflex_command + " -drive file=" + disk_image_path + ",id=rootimg,cache=unsafe,if=none"
    qflex_command = qflex_command + " -device " + "scsi-hd,drive=rootimg"
    # Enable load disk image snapshot
    if args.snapshot is not None:
        qflex_command = qflex_command + " -loadext " + args.snapshot
    elif starting_snapshot is not None:
        qflex_command = qflex_command + " -loadext " + starting_snapshot
    # Enable phases
    if args.phases:
        qflex_command = qflex_command + " -phases " + "steps=" + args.phases_length + ",name=" + args.phases_name
    # Enable simulation checkpoints
    if args.checkpoints:
        qflex_command = qflex_command + " -flexus " + "mode=" + "trace" + ",length=" + checkpoint_total_length + ",simulator=" + flexus_trace_path + ",config=" + os.path.abspath(user_postload_path)
        qflex_command = qflex_command + " -ckpt " + "every=" + checkpoint_length + ",end=" + checkpoint_total_length
    # Enable trace simulation
    elif args.trace:
        qflex_command = qflex_command + " -flexus " + "mode=" + "trace" + ",length=" + args.simulation_length + ",simulator=" + flexus_timing_path + ",config=" + os.path.abspath(user_postload_path)
    # Enable timing simulation
    elif args.timing:
        qflex_command = qflex_command + " -flexus " + "mode=" + "timing" + ",length=" + args.simulation_length + ",simulator=" + flexus_timing_path + ",config=" + os.path.abspath(user_postload_path)
    # Set name
    if args.name is not None:
        qflex_command = qflex_command + " -name " + args.name
    # Extra commands
    qflex_command = qflex_command + " -machine " + "virt"
    qflex_command = qflex_command + " -cpu " + "cortex-a57"
    qflex_command = qflex_command + " -nographic"
    qflex_command = qflex_command + " -exton"
    qflex_command = qflex_command + " -accel " + "tcg,thread=single"
    qflex_command = qflex_command + " -rtc " + "driftfix=slew"
    qflex_command = qflex_command + " -append " + "'console=ttyAMA0'"
    qflex_command = qflex_command + " -append " + "'root=/dev/sda2'"

    if args.execute:
        os.system(qflex_command)
        return "QCG executed command\n" + qflex_command
    else:
        return qflex_command

## Execute script
if __name__ == '__main__':
    print(generate_qflex_command(sys.argv[1:]))