#!/usr/bin/env bash

set -euo pipefail
pushd "$(dirname "$0")/../.." >/dev/null
python ./qflex run-idx -c $1
popd >/dev/null
