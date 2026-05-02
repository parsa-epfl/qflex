#!/usr/bin/env bash
set -euo pipefail
pushd "$(dirname "$0")/../.." >/dev/null
./qflex load -c "${1:-conf/DC/dc.yaml}" "${@:2}"
popd >/dev/null
