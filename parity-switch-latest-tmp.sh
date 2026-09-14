#!/usr/bin/env bash
# Switch the four C repos to the branch head (single-node-pdes) in each C repo, rebuild them in qflex_test, stash the binaries as label "latest".
# Refuses if any C repo has uncommitted tracked changes. Usage: ./parity-switch-latest-tmp.sh
set -euo pipefail
. "$(dirname "$0")/parity-lib-tmp.sh"
switch_repos latest
python -m tests.container_compiler
stash_build latest
