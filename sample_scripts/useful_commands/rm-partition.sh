#!/usr/bin/env bash

set -euo pipefail
pushd "$(dirname "$0")/../.." >/dev/null
python ./qflex unpartition -c $1
popd >/dev/null
