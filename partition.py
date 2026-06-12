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

# A fully-phantom node has normal qemu checkpoints only (no worm .uarch / .mem / Flexus
# configs); --phantom tells us to skip those when moving snapshots and linking binaries.
PHANTOM = '--phantom' in sys.argv
positional = [a for a in sys.argv[1:] if a != '--phantom']

# By default the partition is equal to the number of CPU cores.
if len(positional) != 1:
    print("Default PARTITION_COUNT is the number of CPU cores.")
    cpu_count = os.cpu_count()
    if cpu_count is None:
        print("Error: Unable to determine the number of CPU cores.")
        sys.exit(1)
    PARTITION_COUNT: int = cpu_count
else:
    PARTITION_COUNT = int(positional[0])
    print(f"Setting partition count to {PARTITION_COUNT}")


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

    # The microarch warm state only exists for detailed nodes; a fully-phantom node's
    # checkpoints are plain qemu state with no .uarch.
    if not PHANTOM:
        os.rename(
            f"snapshot_{snapshot_idx}.uarch",
            f"partition_{partition_idx}/snapshot_{snapshot_idx}.uarch"
        )

    # PDES in-flight messages saved at the checkpoint barrier (written only when nonzero).
    if os.path.exists(f"snapshot_{snapshot_idx}_in_flight.json"):
        os.rename(
            f"snapshot_{snapshot_idx}_in_flight.json",
            f"partition_{partition_idx}/snapshot_{snapshot_idx}_in_flight.json"
        )

#TODO this needs to be fixed later to get it from exp config
# Find the file with root*.qcow2 in the current folder, and check if there is exactly one such file.
qcow2_files = glob.glob("root*.qcow2")
assert len(qcow2_files) == 1, "There should be exactly one qcow2 file in the current folder."
qcow2_file_name = qcow2_files[0]

# Create symbolic links for the necessary binary files in the current folder. A fully-phantom
# node carries only the qemu boot files + its own checkpoints; it has no worm memory
# checkpoint or Flexus configs (and won't run Flexus), so skip those.
if PHANTOM:
    # The per-idx snapshots are incremental deltas over the base memory image (<base>.mem);
    # on-demand load reads <base>.mem/base, so a phantom node needs the *.mem too (it has no
    # .uarch / Flexus configs — it runs no Flexus). It does need core_info.csv (per-core IPNS,
    # the parallel binary's PWQ time source) — symlinked so it tracks run/core_info.csv.
    mem_folder_name = glob.glob("*.mem")
    assert len(mem_folder_name) == 1, "There should be exactly one memory checkpoint file in the current folder."
    mem_folder_name = mem_folder_name[0]
    NECESSARY_BINARY_FILES = [
        "QEMU_EFI.fd",
        qcow2_file_name,
        "efi-e1000.rom",
        "efi-virtio.rom",
        "core_info.csv",
        mem_folder_name,
    ]
else:
    # find the incremental memory checkpoint file in the current folder. It should be named as "*.mem"
    mem_folder_name = glob.glob("*.mem")
    assert len(mem_folder_name) == 1, "There should be exactly one memory checkpoint file in the current folder."
    mem_folder_name = mem_folder_name[0]
    NECESSARY_BINARY_FILES = [
        "QEMU_EFI.fd",
        qcow2_file_name,
        "debug.cfg",
        "../cfg/flexus_configuration.json",
        "../cfg/timing.cfg",
        "../bin/checkpoint_conversion",
        "efi-e1000.rom",
        "efi-virtio.rom",
        mem_folder_name,
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
    # generate the script list.
    f.write("folders=(" + " ".join(folder_list) + ")\n")
    # use xarg to parallel run the scripts.
    f.write("printf '%s\\n' \"${folders[@]}\" | xargs -n 1 -P " + str(PARTITION_COUNT) + " -I {} bash -c \"cd {}; ./run_flexus.sh $1 $2 > log 2> err;\"\n")


os.system("chmod +x ../run_partitions.sh")