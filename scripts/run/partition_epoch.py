#!/usr/bin/env python3

import os

'''
How this program works:
1. Scan the snapshot_*.zstd file under the current folder, and get the count.
2. Calculate the number of files that each partition should have.
3. Create a folder for each partition.
4. Move the snapshot files to the corresponding partition folder.
5. Create symbolic links for the necessary bindary files in the current folder.
6. Copy the starting_script to each partition folder.
'''

PARTITION_COUNT=20

# Scan the snapshot files
snapshot_files = []
for file in os.listdir('.'):
    if file.startswith('snapshot_') and file.endswith('.zstd'):
        snapshot_files.append(file)


# Create partition folder.
for p in range(PARTITION_COUNT):
    os.makedirs(f'partition_{p}', exist_ok=False)

# Move the snapshot files to the corresponding partition folder.
for snapshot_idx in range(len(snapshot_files)):
    partition_idx = snapshot_idx // (len(snapshot_files) // PARTITION_COUNT)

    # Move the file
    os.rename(
        f"snapshot_{snapshot_idx}.zstd",
        f"partition_{partition_idx}/snapshot_{snapshot_idx}.zstd"
    )

    # Move the folder
    os.rename(
        f"snapshot_{snapshot_idx}.uarch",
        f"partition_{partition_idx}/snapshot_{snapshot_idx}.uarch"
    )

# Create symbolic links for the necessary bindary files in the current folder.
NECESSARY_BINARY_FILES = [
    "QEMU_EFI.fd",
    "root.qcow2",
    "debug.cfg",
    "flexus_configuration.json",
    "timing.cfg",
    "checkpoint_conversion"
]

for p in range(PARTITION_COUNT):
    for file in NECESSARY_BINARY_FILES:
        os.symlink(
            f"../{file}",
            f"partition_{p}/{file}"
        )

# Copy the starting_script to each partition folder.
for p in range(PARTITION_COUNT):
    os.system(f"cp run_flexus.sh partition_{p}/run_flexus_p{p}.sh")


cwd_basename = os.path.basename(os.getcwd())

# Create a script to run all partitions in zellij.
with open('run_partitions.sh', 'w') as f:
    f.write("#!/bin/bash\n")

    f.write(f"tmux new-session -d -s {cwd_basename}\n")
    for p in range(PARTITION_COUNT):
        
        if p < PARTITION_COUNT / 2:
            NUMA_NODE = 0
        else:
            NUMA_NODE = 1

        f.write(f"tmux new-window -t {cwd_basename}:{p+1} -n partition_{p} 'cd partition_{p}; numactl --cpunodebind {NUMA_NODE} --membind {NUMA_NODE} ./run_flexus_p{p}.sh; fish'\n")

    f.write(f"tmux attach-session -t {cwd_basename}\n")

os.system("chmod +x run_partitions.sh")