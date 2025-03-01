#!/usr/bin/env python3

import os

# Get the snapshot_*.zstd file under the current directory.
zstd_snapshot_files = [int(f.split("_")[1].split(".")[0]) for f in os.listdir('.') if f.startswith('snapshot_') and f.endswith('.zstd')]

if len(zstd_snapshot_files) == 0:
    print('No snapshot_*.zstd file found under the current directory.')
    exit(1)

# sort the zstd file and make sure small number is in the front
zstd_snapshot_files.sort()

for snapshot_idx in zstd_snapshot_files:
    print(f'Processing snapshot_{snapshot_idx}...')
    # first, decompress the snapshot file
    os.system(f'zstd -d -f snapshot_{snapshot_idx}.zstd')
    # second, create the xdelta file if the index one is more than 0
    if snapshot_idx > 0:
        # the previous snapshot file must exist and must be a file (not a directory)
        if not os.path.exists(f'snapshot_{snapshot_idx - 1}'):
            print(f'snapshot_{snapshot_idx - 1} not found.')
            exit(1)
        if not os.path.isfile(f'snapshot_{snapshot_idx - 1}'):
            print(f'snapshot_{snapshot_idx - 1} is not a file.')
            exit(1)
        # create the xdelta file
        os.system(f'xdelta3 -f -e -s snapshot_{snapshot_idx - 1} snapshot_{snapshot_idx} snapshot_{snapshot_idx}.xdelta3')

        # Now, we don't need the previous snapshot file
        if snapshot_idx > 1:
            os.system(f'rm snapshot_{snapshot_idx - 1}')