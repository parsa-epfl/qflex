#!/usr/bin/env bash

set -euo pipefail
pushd "$(dirname "$0")/../.." >/dev/null
python ./qflex initialize --syncs-list true -c $1
popd >/dev/null
