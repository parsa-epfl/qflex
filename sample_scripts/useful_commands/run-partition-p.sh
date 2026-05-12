#!/usr/bin/env bash

set -euo pipefail
pushd "$(dirname "$0")/../.." >/dev/null
python ./qflex run-single-partition -c $1 --partition-number $2
popd >/dev/null
