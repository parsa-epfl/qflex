#!/usr/bin/env python3

import os

# Get the list of xdelta files under the current directory.

xdelta_files = [int(f.split("_")[1].split(".")[0]) for f in os.listdir('.') if f.startswith('snapshot_') and f.endswith('.xdelta3')]

if len(xdelta_files) == 0:
    print('No snapshot_*.xdelta3 file found under the current directory.')
    exit(1)


# sort the xdelta file and make sure small number is in the front
xdelta_files.sort()

for xdelta_idx in xdelta_files:
    print(f'Processing snapshot_{xdelta_idx}...')
    # first, decompress the snapshot file
    os.system(f'xdelta3 -d snapshot_{xdelta_idx}.xdelta3 snapshot_{xdelta_idx}')