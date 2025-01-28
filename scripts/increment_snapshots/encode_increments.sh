#!/usr/bin/env bash

set -e


show_help() {
  echo "Usage: $0 [--zstd] [-h|--help] <filename>"
  echo "  --zstd    Expect zstd compression"
  echo "  -h, --help  Show this help message"
  exit 0
}

# Parse arguments
use_zstd=false
filename=""

# Check for '--zstd' flag
while [ $# -gt 0 ]; do
  case $1 in
    --zstd)
      use_zstd=true
      shift
      ;;
    -h|--help)
      show_help
      shift
      ;;
    *)
      if [ -z "$filename" ]; then
        filename=$1
      else
          show_help
      fi
      shift
      ;;
  esac
done

# Do not allow non-readable file
if ! [ -f $filename ] || ! [ -r $filename ]; then
    echo "The file does not exist or is not readable."
    exit -1
fi

SNAPSHOT_DIR=$(dirname $filename)

# Expect files to hold list from last made snapshot to first (base) snapshot
readarray -t FILES < $filename

if [ ${#FILES[@]} -lt 2 ]; then
    echo "Error: At least two files are required."
    exit -1
fi

# Unpack if file are zstd compressed
xdelta_cmd='xdelta3 -e -D -f -s {1} {2} {2.}.patch'
if $use_zstd; then
    xdelta_cmd='xdelta3 -e -0 -D -f -s <(zstdcat {1}) <(zstdcat {2}) {2.}.patch'
fi

pushd ${SNAPSHOT_DIR}

# Create incremental patches
parallel --joblog encode.log --bar --color --tag --cf -j$(nproc) ${xdelta_cmd} ::: "${FILES[@]:1}" :::+ "${FILES[@]:0:$((${#FILES[@]}-1))}"

popd
