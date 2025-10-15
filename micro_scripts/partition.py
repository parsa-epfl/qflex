#!/usr/bin/env python3

import os
import sys


if __name__ == "__main__":
    # Get first and second argument
    first_arg = sys.argv[1]
    command = f"""xargs -a ./micro_scripts/qflex.micro.args -- ./qflex partition --partition-count {first_arg}"""

    full_command = " && ".join([
        "cd ..",
        command,
    ])
    os.system(full_command)





