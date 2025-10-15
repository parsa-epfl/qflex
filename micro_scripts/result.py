#!/usr/bin/env python3

import os
import sys


if __name__ == "__main__":
    command = f"""xargs -a ./micro_scripts/qflex.micro.args -- ./qflex result"""

    full_command = " && ".join([
        "cd ..",
        command,
    ])
    os.system(full_command)





