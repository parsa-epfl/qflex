#!/usr/bin/env bash

set -euo pipefail
pushd "$(dirname "$0")/../.." >/dev/null
# Never sync on boot, and we don't care about latency, just boot fast and then setup
python ./qflex boot --interactive-tmux --syncs-list false --latencies-ns-list 1000000 -c $1
popd >/dev/null
