#!/usr/bin/env bash

set -euo pipefail
pushd "$(dirname "$0")/../.." >/dev/null
python ./qflex initialize -c $1
popd >/dev/null
