import os
import sys
import argparse
import ConfigParser

def generate_instance(qflex_path, target_path, instance_id):
    instance_config = ConfigParser.ConfigParser()

    instance_config.add_section("Environment")
    instance_config.set("Environment", "qflex_path", qflex_path)
    instance_config.set("Environment", "image_repo_path", os.path.join(qflex_path, "images"))
    instance_config.set("Environment", "disk_image_path", "ubuntu-16.04-blank/ubuntu-16.04-lts-blank.qcow2")
    instance_config.set("Environment", "kernel_path", "ubuntu-16.04-blank/vmlinuz-4.4.0-83-generic")
    instance_config.set("Environment", "initrd_path", "ubuntu-16.04-blank/initrd.img-4.4.0-83-generic")

    instance_config.add_section("Machine")
    instance_config.set("Machine", "qemu_core_count", "1")
    instance_config.set("Machine", "memory_size", "2000")
    instance_config.set("Machine", "machine", "virt")
    instance_config.set("Machine", "cpu", "cortex-a57")
    instance_config.set("Machine", "user_network", "on")
    instance_config.set("Machine", "user_network_id", "net1")
    instance_config.set("Machine", "user_network_mac", "52:54:00:00:02:"+str(12+instance_id))
    instance_config.set("Machine", "user_network_hostfwd_protocol", "tcp")
    instance_config.set("Machine", "user_network_hostfwd_hostport", str(2220+instance_id))
    instance_config.set("Machine", "user_network_hostfwd_guestport", "22")
    instance_config.set("Machine", "qmp", "on")
    instance_config.set("Machine", "qmp_dev", "unix:/tmp/qmp-sock"+str(instance_id)+",server,nowait")

    instance_file = open(target_path, "w")
    instance_config.write(instance_file)
    instance_file.close()

def generate_system(target_path, qflex_path, instance_count):
    sys_config = ConfigParser.ConfigParser()

    sys_config.add_section("Instances")
    for i in range(instance_count):
        instance_name = "instance{0}.ini".format(i)
        instance_path = os.path.join(target_path, instance_name)
        sys_config.set("Instances", instance_name, instance_path)
        generate_instance(qflex_path, instance_path, i)

    sys_config.add_section("Multinode")
    sys_config.set("Multinode", "ns3_path", os.path.join(qflex_path, "3rdparty/ns3"))

    sys_file = open(os.path.join(target_path, "system.ini"), "w")
    sys_config.write(sys_file)
    sys_file.close()

def add_snapshot(target_path, snapshot_name):
    instance_config = ConfigParser.ConfigParser()
    instance_config.read(target_path)
    instance_config.set("Environment", "starting_snapshot", snapshot_name)
    instance_file = open(target_path, "w")
    instance_config.write(instance_file)
    instance_file.close()

def add_trace_simulation(target_path, qflex_path):
    instance_config = ConfigParser.ConfigParser()
    instance_config.read(target_path)

    instance_config.add_section("Simulation")
    instance_config.set("Simulation", "flexus_path", os.path.join(qflex_path, "flexus"))
    instance_config.set("Simulation", "flexus_trace_path", "simulators/KeenKraken/libflexus_KeenKraken_arm_iface_gcc.so")
    # FixMe place user postload path in new script destination
    instance_config.set("Simulation", "user_postload_path", os.path.join(qflex_path, ""))
    instance_config.set("Simulation", "simulation_type", "trace")
    instance_config.set("Simulation", "simulation_length", "10000000")

    instance_file = open(target_path, "w")
    instance_config.write(instance_file)
    instance_file.close()

def add_phase_generation(target_path, qflex_path):
    instance_config = ConfigParser.ConfigParser()
    instance_config.read(target_path)

    instance_config.add_section("Simulation")
    instance_config.set("Simulation", "simulation_type", "phases")
    instance_config.set("Simulation", "phases_name", "phases")
    instance_config.set("Simulation", "phases_length", "1m:1m:1m:1m:1m")

    instance_file = open(target_path, "w")
    instance_config.write(instance_file)
    instance_file.close()

def add_checkpoint_simulation(target_path, qflex_path):
    instance_config = ConfigParser.ConfigParser()
    instance_config.read(target_path)

    instance_config.add_section("Simulation")
    instance_config.set("Simulation", "flexus_path", os.path.join(qflex_path, "flexus"))
    instance_config.set("Simulation", "flexus_trace_path", "simulators/KeenKraken/libflexus_KeenKraken_arm_iface_gcc.so")
    # FixMe place user postload path in new script destination
    instance_config.set("Simulation", "user_postload_path", os.path.join(qflex_path, ""))
    instance_config.set("Simulation", "simulation_type", "checkpoints")
    instance_config.set("Simulation", "checkpoints_number", "10")
    instance_config.set("Simulation", "checkpoints_length", "100k")

    instance_file = open(target_path, "w")
    instance_config.write(instance_file)
    instance_file.close()

def add_timing_simulation(target_path, qflex_path):
    instance_config = ConfigParser.ConfigParser()
    instance_config.read(target_path)

    instance_config.add_section("Simulation")
    instance_config.set("Simulation", "flexus_path", os.path.join(qflex_path, "flexus"))
    # FixMe get location of timing simulator
    instance_config.set("Simulation", "flexus_timing_path", "simulators/KnottyKraken/")
    # FixMe place user postload path in new script destination
    instance_config.set("Simulation", "user_postload_path", os.path.join(qflex_path, ""))
    instance_config.set("Simulation", "simulation_type", "timing")
    instance_config.set("Simulation", "simulation_length", "10000000")

    instance_file = open(target_path, "w")
    instance_config.write(instance_file)
    instance_file.close()

if __name__ == '__main__':
    # Argument parser setup
    help_text = ""
    parser = argparse.ArgumentParser(description = help_text, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("type",
        choices=[
        "generate_system",
        "add_snapshot",
        "add_trace_simulation",
        "add_phase_generation",
        "add_checkpoint_simulation",
        "add_timing_simulation"
        ]
    )
    parser.add_argument("-t", dest="target_path")
    parser.add_argument("-q", dest="qflex_path")
    parser.add_argument("-c", dest="instance_count")
    parser.add_argument("-s", dest="snapshot_name")
    args = parser.parse_args()

    if args.type = "generate_system":
        generate_system(args.target_path, args.qflex_path, args.instance_count)
    elif args.type = "add_snapshot":
        add_snapshot(args.target_path, args.snapshot_name)
    elif args.type = "add_trace_simulation":
        add_trace_simulation(args.target_path, args.qflex_path)
    elif args.type = "add_phase_generation":
        add_trace_simulation(args.target_path, args.qflex_path)
    elif args.type = "add_checkpoint_simulation":
        add_trace_simulation(args.target_path, args.qflex_path)
    elif args.type = "add_timing_simulation":
        add_trace_simulation(args.target_path, args.qflex_path)
