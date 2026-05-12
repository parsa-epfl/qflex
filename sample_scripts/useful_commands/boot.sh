#!/usr/bin/env bash

set -euo pipefail
pushd "$(dirname "$0")/../.." >/dev/null
# Never sync on boot, and we don't care about latency, just boot fast and then setup
python ./qflex boot -c $1
popd >/dev/null
