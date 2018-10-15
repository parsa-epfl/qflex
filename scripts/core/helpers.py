import re
import os
import subprocess

# Convert input string-numbers to integers and checks it's validity
# Accepts:{1,1k,1K,1m,1M,1b,1B}
def input_to_number(input_number):
    parsed_input = re.match('(\d+)([kmbKMB]?)', input_number)
    if not parsed_input:
        raise ValueError
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