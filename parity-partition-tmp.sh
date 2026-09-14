#!/usr/bin/env bash
# Parity level "partition": run-single-partition; diff = every idx of that partition.
# Usage: ./parity-partition-tmp.sh <pre|latest> <container>   # seed the label for this level, then run it
#        ./parity-partition-tmp.sh diff                       # compare pre vs latest at this level
set -euo pipefail
. "$(dirname "$0")/parity-lib-tmp.sh"
level_main partition "$@"
