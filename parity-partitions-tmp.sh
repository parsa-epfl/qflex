#!/usr/bin/env bash
# Parity level "partitions": run-partition + result; diff = all partitions + timing.csv/core_info_new.csv/REQUIRED_SAMPLE_SIZE.
# Usage: ./parity-partitions-tmp.sh <pre|latest> <container>   # seed the label for this level, then run it
#        ./parity-partitions-tmp.sh diff                       # compare pre vs latest at this level
set -euo pipefail
. "$(dirname "$0")/parity-lib-tmp.sh"
level_main partitions "$@"
