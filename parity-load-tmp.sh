#!/usr/bin/env bash
# Parity level "load": load only from booted (parallel-qemu, isolated); diff = loaded tag VM clock + loaded_in_flight.json.
# Usage: ./parity-load-tmp.sh <pre|latest> <container>   # seed the label for this level, then run it
#        ./parity-load-tmp.sh diff                       # compare pre vs latest at this level
set -euo pipefail
. "$(dirname "$0")/parity-lib-tmp.sh"
level_main load "$@"
