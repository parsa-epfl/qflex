#!/usr/bin/env bash
# Parity level "fw": fw only from init_warmed (parallel-qemu, isolated); diff = FW snapshot set/.loc/in-flight/uarch stats + qcow2 VM clocks.
# Usage: ./parity-fw-tmp.sh <pre|latest> <container>   # seed the label for this level, then run it
#        ./parity-fw-tmp.sh diff                       # compare pre vs latest at this level
set -euo pipefail
. "$(dirname "$0")/parity-lib-tmp.sh"
level_main fw "$@"
