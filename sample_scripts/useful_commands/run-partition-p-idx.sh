#!/usr/bin/env bash

set -euo pipefail
pushd "$(dirname "$0")/../.." >/dev/null
python ./qflex run-idx -c $1 --partition-number $2 --idx $3
popd >/dev/null
