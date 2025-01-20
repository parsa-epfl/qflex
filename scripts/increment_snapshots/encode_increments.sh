#!/usr/bin/env bash

set -e

# Using the square bracket syntax
if ! [ -f $1 ] || ! [ -r $1 ]; then
    echo "The file does not exist or is not readable."
    exit -1
fi

SNAPSHOT_DIR=$(dirname $1)

# Expect files to hold list from last made snapshot to first (base) snapshot
readarray -t FILES < $1

if [ ${#FILES[@]} -lt 2 ]; then
    echo "Error: At least two files are required."
    exit -1
fi


pushd ${SNAPSHOT_DIR}

# Create incremental patches
parallel --joblog run.log --bar --color --tag --cf -j$(nproc) 'xdelta3 -e -0 -R -f -s <(zstdcat {1}) <(zstdcat {2}) {2.}.patch' ::: "${FILES[@]:1}" :::+ "${FILES[@]:0:$((${#FILES[@]}-1))}"

popd
