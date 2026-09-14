#!/usr/bin/env bash
# Parity level "idx": run-idx for the (partition, idx) in the YAML; diff = that idx's all.measurement.*.log.
# Usage: ./parity-idx-tmp.sh <pre|latest> <container>   # seed the label for this level, then run it
#        ./parity-idx-tmp.sh diff                       # compare pre vs latest at this level
set -euo pipefail
. "$(dirname "$0")/parity-lib-tmp.sh"
level_main idx "$@"
