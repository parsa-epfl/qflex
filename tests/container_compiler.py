"""
In-container compile driver — NOT a pytest test.

Recompiles `qemu/` + `parallel-qemu/` (and optionally `flexus/`) inside the
long-lived `qflex_test` container shared with the real-run tests
(TEST_CONTAINER_NAME from tests/conftest.py). The container reuses the
prebuilt `-saved/` trees baked into the image, so each invocation only
recompiles changed files — orders of magnitude faster than a full
`./dep build-docker` rebuild when iterating on PDES / QEMU / Flexus source.

The container is **left running** after the compile so the next real-run
test session reuses it — `dev_container` in conftest.py detects a
pre-existing `qflex_test` and reuses it instead of tearing it down.

Invoke via either Makefile target:

* `make test-iterate` → `python -m tests.container_compiler` (qemu + parallel-qemu).
* `make test QFLEX_RECOMPILE=1` → `python -m tests.container_compiler --include-flexus`
  (qemu + parallel-qemu + flexus + kraken libs) before the pytest run.

Claude must NEVER invoke this autonomously — it is intended for the user to
trigger when iterating on phase-1 code changes between real-run runs.

To clean up the container manually:
    ./dep stop-docker --container-name qflex_test

Override env vars:
  QFLEX_ITERATE_MODE     -- debug (default) or release
  QFLEX_MOUNTING_FOLDER  -- defaults to /mnt/sdc/data-caching-1c/
"""

import os
import subprocess
import sys

from tests.conftest import TEST_CONTAINER_NAME, _container_running


MOUNTING_FOLDER = os.environ.get("QFLEX_MOUNTING_FOLDER", "/mnt/sdc/data-caching-1c/")
MODE = os.environ.get("QFLEX_ITERATE_MODE", "debug")


def compile_in_container(*, include_flexus: bool) -> None:
    if not _container_running(TEST_CONTAINER_NAME):
        subprocess.run(
            [
                "./dep", "start-docker", "--background",
                "--container-name", TEST_CONTAINER_NAME,
                "--debug", "--worm",
                "--mounting-folder", MOUNTING_FOLDER,
            ],
            check=True,
        )

    parts = [
        f"make qemu-config MODE={MODE}",
        f"make qemu-build MODE={MODE}",
        "make parallel-qemu-config",
        "make parallel-qemu-build",
    ]
    if include_flexus:
        parts += [
            "make flexus-config",
            f"make flexus-build MODE={MODE}",
        ]

    subprocess.run(
        [
            "./dep", "exec",
            "--container-name", TEST_CONTAINER_NAME,
            "--command", " && ".join(parts),
        ],
        check=True,
    )


if __name__ == "__main__":
    compile_in_container(include_flexus="--include-flexus" in sys.argv)
