#!/usr/bin/env bash
# Parity level "fw-all": fw -> partition -> run-partition -> result; diff = fw level + partitions level.
# Usage: ./parity-fw-all-tmp.sh <pre|latest> <container>   # seed the label for this level, then run it
#        ./parity-fw-all-tmp.sh diff                       # compare pre vs latest at this level
set -euo pipefail
. "$(dirname "$0")/parity-lib-tmp.sh"
level_main fw-all "$@"
