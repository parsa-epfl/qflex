#!/usr/bin/env python3

import os

CORE_COUNT = [8]
MEMORY_CONTROLLER = [1]

TARGETS = [
    "knottykraken",
    "semikraken"
]


for (core_count, mem_count) in zip(CORE_COUNT, MEMORY_CONTROLLER):
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

    
        