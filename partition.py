#!/usr/bin/env python3

import os
import sys
import math
import glob

'''
How this program works:
1. Scan the snapshot_*.zstd file under the current folder, and get the count.
2. Calculate the number of files that each partition should have.
3. Create a folder for each partition.
4. Move the snapshot files to the corresponding partition folder.
5. Create symbolic links for the necessary bindary files in the current folder.
6. Copy the starting_script to each partition folder.
'''

# By default the partition is equal to the number of CPU cores.
if len(sys.argv) != 2:
    print("Default PARTITION_COUNT is the number of CPU cores.")
    cpu_count = os.cpu_count()
    if cpu_count is None:
        print("Error: Unable to determine the number of CPU cores.")
        sys.exit(1)
    PARTITION_COUNT: int = cpu_count
else:
    PARTITION_COUNT = int(sys.argv[1])

# Make sure flags/GEN_CHECKPOINT_DONE exists.
if not os.path.exists("flags/GEN_CHECKPOINT_DONE"):
    print("Error: flags directory does not exist.")
    sys.exit(1)

os.chdir("./run")

# Scan the snapshot files
snapshot_files = []
for file in os.listdir('.'):
    if file.startswith('snapshot_') and file.endswith('.loc'):
        snapshot_files.append(file)

PARTITION_COUNT = min(PARTITION_COUNT, len(snapshot_files))

# Create partition folder.
for p in range(PARTITION_COUNT):
    os.makedirs(f'partition_{p}', exist_ok=False)

# Move the snapshot files to the corresponding partition folder.
for snapshot_idx in range(len(snapshot_files)):
    partition_idx = snapshot_idx // (math.ceil(len(snapshot_files) / PARTITION_COUNT))

    # Move the file
    os.rename(
        f"snapshot_{snapshot_idx}.loc",
        f"partition_{partition_idx}/snapshot_{snapshot_idx}.loc"
    )

    os.rename(
        f"snapshot_{snapshot_idx}.state.zstd",
        f"partition_{partition_idx}/snapshot_{snapshot_idx}.state.zstd"
    )

    # Move the folder
    os.rename(
        f"snapshot_{snapshot_idx}.uarch",
        f"partition_{partition_idx}/snapshot_{snapshot_idx}.uarch"
    )

# find the incremental memory checkpoint file in the current folder. It should be named as "*.mem"
mem_folder_name = glob.glob("*.mem")
assert len(mem_folder_name) == 1, "There should be exactly one memory checkpoint file in the current folder."
mem_folder_name = mem_folder_name[0]

# Create symbolic links for the necessary bindary files in the current folder.
NECESSARY_BINARY_FILES = [
    "QEMU_EFI.fd",
    "root.qcow2",
    "debug.cfg",
    "../cfg/flexus_configuration.json",
    "../cfg/timing.cfg",
    "../bin/checkpoint_conversion",
    mem_folder_name,
]

for p in range(PARTITION_COUNT):
    for file in NECESSARY_BINARY_FILES:
        os.symlink(
            f"../{file}",
            f"partition_{p}/{file.split('/')[-1]}"
        )

folder_list = []

# Copy the starting_script to each partition folder.
for p in range(PARTITION_COUNT):
    os.system(f"cp ../scripts/run_flexus.sh partition_{p}/run_flexus.sh")
    folder_list.append(f"run/partition_{p}")


cwd_basename = os.path.basename(os.path.dirname(os.getcwd()))

with open('../run_partitions.sh', 'w') as f:
    f.write("#!/bin/bash\n")
    # generate the script list.
    f.write("folders=(" + " ".join(folder_list) + ")\n")
    # use xarg to parallel run the scripts.
    f.write("printf '%s\\n' \"${folders[@]}\" | xargs -n 1 -P " + str(PARTITION_COUNT) + " -I {} bash -c \"cd {}; ./run_flexus.sh $1 $2 > log 2> err;\"\n")


os.system("chmod +x ../run_partitions.sh")