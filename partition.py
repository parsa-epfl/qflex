#!/usr/bin/env python3

import os
import sys
import math
import glob

'''
How this program works:
1. Scan the snapshot_*.bxdb-meta files under the current folder, and get the count.
2. Calculate the number of files that each partition should have.
3. Create a folder for each partition.
4. Move the snapshot files to the corresponding partition folder.
5. Create symbolic links for the necessary bindary files in the current folder.
6. Copy the starting_script to each partition folder.
7. Generate run_partitions.sh, which builds the bxdb B-tree index before running the partitions.
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
    print(f"Setting partition count to {PARTITION_COUNT}")


os.chdir("./run")

# Scan the snapshot files
snapshot_files = []
for file in os.listdir('.'):
    if file.startswith('snapshot_') and file.endswith('.bxdb-meta'):
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
        f"snapshot_{snapshot_idx}.bxdb-meta",
        f"partition_{partition_idx}/snapshot_{snapshot_idx}.bxdb-meta"
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

# Find the bxdb database holding the memory of all checkpoints (named after the base snapshot, e.g. init_warmed.bxdb).
db_folder_name = glob.glob("*.bxdb")
assert len(db_folder_name) == 1, "There should be exactly one bxdb database directory in the current folder."
db_folder_name = db_folder_name[0]

# Create symbolic links for the necessary bindary files in the current folder.
NECESSARY_BINARY_FILES = [
    "QEMU_EFI.fd",
    "root.qcow2",
    "debug.cfg",
    "../cfg/flexus_configuration.json",
    "../cfg/timing.cfg",
    "../bin/checkpoint_conversion",
    db_folder_name,
]

for p in range(PARTITION_COUNT):
    for file in NECESSARY_BINARY_FILES:
        if not os.path.exists(file):
            raise FileNotFoundError(f"Error: {file} not found.")
        os.symlink(
            f"../{file}",
            f"partition_{p}/{file.split('/')[-1]}"
        )
print(f"Created {PARTITION_COUNT} partitions.")

folder_list = []

# Copy the starting_script to each partition folder.
for p in range(PARTITION_COUNT):
    os.system(f"cp ../scripts/run_flexus.sh partition_{p}/run_flexus.sh")
    folder_list.append(f"run/partition_{p}")

print(f"Created {' '.join(folder_list)}.")

cwd_basename = os.path.basename(os.path.dirname(os.getcwd()))

with open('../run_partitions.sh', 'w') as f:
    f.write("#!/bin/bash\n")
    # Functional warming appends checkpoints to the bxdb log (chunks.log); build the B-tree
    # index (index.bxdb) once so the timing runs can look pages up directly.
    f.write(f"(cd run && ../bin/bxdb convert to-btree {db_folder_name}) || exit 1\n")
    # generate the script list.
    f.write("folders=(" + " ".join(folder_list) + ")\n")
    # use xarg to parallel run the scripts.
    f.write("printf '%s\\n' \"${folders[@]}\" | xargs -n 1 -P " + str(PARTITION_COUNT) + " -I {} bash -c \"cd {}; ./run_flexus.sh $1 $2 > log 2> err;\"\n")


os.system("chmod +x ../run_partitions.sh")