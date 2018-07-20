import re
import os
import mrun.mrun as mrun

# Convert input string-numbers to integers and checks it's validity
# Accepts:{1,1k,1K,1m,1M,1b,1B}
def input_to_number(input_number):
    parsed_input = re.match('(\d+)([kmbKMB]?)', input_number)
    if not parsed_input:
        raise ValueError("Input not valid")
    base = parsed_input.groups()[0]
    power = parsed_input.groups()[1]
    if power == 'k'or power == 'K':
        return int(base) * 1000;
    elif power == 'm'or power == 'M':
        return int(base) * 1000000;
    elif power == 'k'or power == 'K':
        return int(base) * 1000000000;
    else:
        return int(base)

# Check validity of input phases_length
def check_phases_length(phases_length):
    for i in re.split(':', phases_length):
        input_to_number(i)

# Count number of phases generated
def phases_count(phases_length):
    check_phases_length(phases_length)
    return len(re.split(':', phases_length))

# Create an instance xml file using mrun
def create_instance_xml(qemu_command, output_file):
    mrun_arg_list = [ "-c", qemu_command, output_file]
    mrun.main(mrun_arg_list)

# Create an setup xml file compatible with mrun
# It assumes all instances have "instance$i.xml" as their representing instance xml file present in the same directory as the setup file
def create_setup_xml(folder_path, instances_number):
    f = open(os.path.join(folder_path, "setup_file.xml"), "w+")
    for i in range(instances_number):
        f.write("<setup><instance><file>" + os.path.join(folder_path, "instance" + str(i) + ".xml") + "</file></instance></setup>\n")
    f.close()

# Create an setup xml file compatible with mrun to be run in kubernetes
# It assumes all instances have "instance$i.xml" as their representing instance xml file present in the same directory as the setup file
def create_setup_xml_docker(folder_path, docker_xml_path, instances_number):
    f = open(os.path.join(folder_path, "setup_file.xml"), "w+")
    for i in range(instances_number):
        f.write("<setup><instance><file>" + os.path.join(docker_xml_path, "instance" + str(i) + ".xml") + "</file></instance></setup>\n")
    f.close()