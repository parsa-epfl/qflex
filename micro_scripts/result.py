#!/usr/bin/env python3

import os


if __name__ == "__main__":
    command = f"""xargs -a ./micro_scripts/qflex.micro.args -- ./qflex result"""

    full_command = " && ".join([
        "cd ..",
        command,
        "cp /home/dev/experiments/experiment_micro/cfg/core_info.csv /home/dev/qflex/micro_scripts/core_info_new.csv"
    ])
    os.system(full_command)





