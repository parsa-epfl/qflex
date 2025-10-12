#!/usr/bin/env python3

import os
import sys


if __name__ == "__main__":
    # sample size will be moved later to only fw as it has no affect here
    command = f"""xargs -a ./micro_scripts/qflex.micro.args -- ./qflex result --sample-size 30 """

    full_command = " && ".join([
        "cd ..",
        command,
    ])
    os.system(full_command)





