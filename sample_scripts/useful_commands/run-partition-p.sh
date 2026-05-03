#!/usr/bin/env bash

set -euo pipefail
pushd "$(dirname "$0")/../.." >/dev/null
python ./qflex run-single-partition --partition-number $2 -c $1
popd >/dev/null
