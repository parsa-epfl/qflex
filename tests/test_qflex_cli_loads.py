"""Smoke test that the `qflex` Typer script imports and every subcommand's
`--help` rendering succeeds.

The dry-run suite imports command classes directly (e.g. `FunctionalWarming`,
`RunIdxCommand`) and never runs `qflex` itself, so it can't catch a
decorator-time failure in `@data_class_wrap`. The failure mode this guards
against is `inspect.Signature(parameters=typer_params + func_params)` raising
`ValueError: duplicate parameter name: '<x>'` because a Typer body kwarg
shadows an auto-generated factory-derived flag (or vice versa).
"""
import os
import runpy
import subprocess
import sys

import pytest

from .conftest import REPO_ROOT


QFLEX_PATH = os.path.join(REPO_ROOT, "qflex")


def test_qflex_script_imports_cleanly():
    """Loading the script must not raise — every `@data_class_wrap` decorator
    on every `@app.command()` body runs at module-import time."""
    runpy.run_path(QFLEX_PATH, run_name="__not_main__")


@pytest.mark.parametrize("cmd", [
    "boot",
    "load",
    "initialize",
    "fw",
    "partition",
    "partition-cleanup",
    "unpartition",
    "run-idx",
    "run-single-partition",
    "run-partition",
    "result",
    "get-experiment-folder",
    "multi",
    "create-base-image",
])
def test_qflex_subcommand_help_renders(cmd):
    """Each subcommand's `--help` must render cleanly. Catches the
    duplicate-parameter-name regression in `data_class_wrap` and any other
    decorator-time signature breakage."""
    r = subprocess.run(
        [sys.executable, QFLEX_PATH, cmd, "--help"],
        cwd=REPO_ROOT, capture_output=True, text=True, timeout=30,
    )
    assert r.returncode == 0, (
        f"./qflex {cmd} --help failed (rc={r.returncode}).\n"
        f"stdout:\n{r.stdout}\n"
        f"stderr:\n{r.stderr}"
    )
