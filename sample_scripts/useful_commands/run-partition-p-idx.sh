#!/usr/bin/env bash

set -euo pipefail
pushd "$(dirname "$0")/../.." >/dev/null
python ./qflex run-idx --partition-number $2 --idx $3 -c $1
popd >/dev/null
