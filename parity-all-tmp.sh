#!/usr/bin/env bash
# Parity level "all": boot -> load -> initialize -> fw -> partition -> run-partition -> result; diff = every tag + fw + partitions.
# Usage: ./parity-all-tmp.sh <pre|latest> <container>   # seed the label for this level, then run it
#        ./parity-all-tmp.sh diff                       # compare pre vs latest at this level
set -euo pipefail
. "$(dirname "$0")/parity-lib-tmp.sh"
level_main all "$@"
