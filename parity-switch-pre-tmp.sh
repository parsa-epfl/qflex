#!/usr/bin/env bash
# Switch the four C repos to the PRE-first-commit set (PRE_COMMITS in parity-lib-tmp.sh), rebuild them in qflex_test, stash the binaries as label "pre".
# Refuses if any C repo has uncommitted tracked changes. Usage: ./parity-switch-pre-tmp.sh
set -euo pipefail
. "$(dirname "$0")/parity-lib-tmp.sh"
switch_repos pre
python -m tests.container_compiler
stash_build pre
