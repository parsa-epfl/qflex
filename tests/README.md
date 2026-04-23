# qflex Test Guide

This folder contains the outer `qflex` repo test suite.

The outer repo mainly owns:

- the top-level CLI surface
- argument forwarding into QPoints
- checkpoint conversion and run-gem5 orchestration

So the tests here focus on the wrapper contract rather than re-testing gem5 internals.

## Files

- [testenv.py](testenv.py)
  - shared test-environment loader for the outer repo
  - resolves config from:
    - `QFLEX_TEST_CONFIG`
    - repo-local `.testenv.json`
    - `~/.config/qflex/testenv.json`
  - provides integration defaults:
    - snapshot: `snapshot_1`
    - instructions: `100000`
  - supports artifact retention control through:
    - `keep_artifacts` in the testenv file
    - `QFLEX_TEST_KEEP_ARTIFACTS=1`

- [test_testenv.py](test_testenv.py)
  - verifies testenv path resolution
  - verifies required-key checking
  - verifies default snapshot / instruction settings
  - verifies artifact retention policy parsing

- [test_qpoints_cli.py](test_qpoints_cli.py)
  - checks that `qflex qpoints run-gem5 --help` exposes the expected tracing options
  - verifies that the outer CLI forwards those options into `commands/qpoints.py`
  - protects the wrapper contract for:
    - `--branch-trace`
    - `--data-trace`
    - `--dump-cache-state`
    - `--timing-ruby`
    - `--sim-config`

- [test_qpoints_integration.py](test_qpoints_integration.py)
  - real integration tests for the outer `qflex` CLI
  - creates a writable qcow2 copy from the configured base image
  - converts `snapshot_1`
  - runs gem5 for `100000` instructions by default
  - covers:
    - `qflex qpoints convert-single`
    - classic Atomic run with branch + data trace
    - classic Atomic run with `--sim-config`
    - Ruby O3 run with data trace + cache dump + `--sim-config`

## How to run

Use your project Python environment:

```bash
python -m pytest -q
```

Run only outer-repo tests:

```bash
cd /path/to/qflex
python -m pytest -q
```

Run only integration tests:

```bash
cd /path/to/qflex
python -m pytest -q -m integration
```

Run one specific test:

```bash
cd /path/to/qflex
python -m pytest -q tests/test_qpoints_integration.py::test_qflex_run_gem5_ruby_data_trace_cache_dump_and_sim_config
```

## Test environment file

Integration tests need a local config file that points to machine-specific assets.

Minimal shape:

```json
{
  "qflex_ckp_dir": "/path/to/qflex/checkpoints",
  "gem5_ckp_dir": "/path/to/gem5/checkpoints",
  "core_count": 1,
  "memory_gb": 16,
  "base": "/path/to/base.qcow2"
}
```

Optional fields:

```json
{
  "default_snapshot": "snapshot_1",
  "default_insts": 100000,
  "ssh_host": "127.0.0.1",
  "ssh_user": "qflex",
  "monitor_base": 45454,
  "qmp_base": 4444,
  "ssh_base": 2222,
  "keep_artifacts": false
}
```

## Artifact policy

By default, integration tests clean up the artifacts they create:

- writable qcow2 copy used for the test session
- converted gem5 checkpoint under the configured `gem5_ckp_dir`
- pytest-generated `QPoints/sim_outs/pytest_*` experiment directories

To keep those artifacts for debugging:

```bash
export QFLEX_TEST_KEEP_ARTIFACTS=1
```

or set:

```json
{
  "keep_artifacts": true
}
```

## Notes

- integration tests intentionally use `snapshot_1` by default so `snapshot_0` remains untouched for active work
- if no testenv file is configured, integration tests skip themselves cleanly instead of failing
- these tests validate the outer wrapper behavior, not just whether a local helper function returns the right list of args
