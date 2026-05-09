#!/usr/bin/env bash

set -euo pipefail
pushd "$(dirname "$0")/../.." >/dev/null
python ./qflex run-single-partition -c $1
popd >/dev/null
