#!/usr/bin/env python3

import os
import argparse

# CORE_COUNT = [1, 4, 16, 64]
# MEMORY_CONTROLLER = [1, 1, 2, 8]

# Parse command-line arguments
parser = argparse.ArgumentParser(description='Build Kraken targets with specified configuration.')
parser.add_argument('--core-count', type=int, required=True,
                    help='Number of cores')
parser.add_argument('--memory-controller', type=int, required=True,
                    help='Number of memory controllers')

args = parser.parse_args()

core_count = args.core_count
mem_count = args.memory_controller

TARGETS = [
    "knottykraken",
    "semikraken"
]


for target in TARGETS:
    # first, update the configuration.
    with open(f"flexus/target/{target}/wiring.cpp", "r") as f:
        config = f.readlines()

    for i in range(len(config)):
        if "FLEXUS_INSTANTIATE_COMPONENT_ARRAY( MemoryLoopback, theMemoryCfg, theMemory, FIXED, DIVIDE," in config[i]:
            config[i] = f"FLEXUS_INSTANTIATE_COMPONENT_ARRAY( MemoryLoopback, theMemoryCfg, theMemory, FIXED, DIVIDE, {mem_count} );\n"

    with open(f"flexus/target/{target}/wiring.cpp", "w") as f:
        f.write("".join(config))
    
    # then, execute the build command
    assert os.system(f"./build {target}") == 0

    # finally, move the build output to a specific directory.
    os.system(f"rm -rf flexus/build-{target}-{core_count}c")
    # os.system(f"mv flexus/build-{target} flexus/build-{target}-{core_count}c")
        