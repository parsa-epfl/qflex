---
name: run-in-dev-container
description: Use this skill when the user asks the assistant to *actually* run a qflex command (or any bash) end-to-end inside the QFlex dev container — i.e. real QEMU / real Flexus, not the dry-run shape the test suite verifies. The pattern is: bring the dev container up once in background (`./dep start-docker --background`), `./dep exec --command "..."` for each command (no per-call container start), `./dep stop-docker` when done. Defaults the host mounting folder to `/mnt/sdc/data-caching-1c/` to match the user's standard workflow; that default is overridable per session. TRIGGER when the user says "actually run X", "run for real", "do a live run", "run this against real qemu", "use the dev container to …", or attaches output expectations that only a real run could satisfy. SKIP when dry-run output is enough (use the test suite + the `testing` skill instead) — real runs are slow (full QEMU boot is minutes), file-system-mutating, and require a working docker daemon + the prebuilt qflex image.
---

# Running qflex commands for real, inside the dev container

This skill is a runbook for the assistant. When the user explicitly asks for a real run — boot, fw, run-partition, even just `./qflex --help` against the real image — follow this skill instead of falling back to dry-run.

The mechanism is a **session pattern**: bring up the dev container once in background, `docker exec` into it many times, tear it down at the end.

```
session start  →  ./dep start-docker --mounting-folder <mf> --worm --background
                  (long-running container named "qflex-dev", kept alive with
                   tail -f /dev/null)

per command    →  ./dep exec --command "<bash>"
                  (docker exec into qflex-dev — no container start cost,
                   container state persists across calls)

session end    →  ./dep stop-docker
                  (docker rm -f qflex-dev; idempotent)
```

This is much cheaper than the previous `docker run --rm` per command — you pay startup once instead of per call. State (running QEMU processes, files written under `/home/dev`, …) is preserved across exec invocations within a session, just like a human's interactive workflow.

## Default mounting folder

Default to **`/mnt/sdc/data-caching-1c/`** — the user's usual scratch directory. That's what shows up in `conf/DC/dc.yaml` and is the path their existing experiment tree lives under.

Override per-session via the `--mounting-folder` flag on `start-docker` (NOT on `exec` — exec runs against a container that already has its mounts).

## Standard session

Always wrap real-run work in a "start → exec... → stop" session. If you want to run just one command, the session is still: start, exec the one command, stop.

```bash
# Bring it up (once per session)
./dep start-docker --mounting-folder /mnt/sdc/data-caching-1c/ --worm --background

# Run as many commands as needed
./dep exec --command "./qflex --help"
./dep exec --command "./qflex boot -c conf/DC/dc.yaml"
./dep exec --command "make test"

# Tear down when done
./dep stop-docker
```

If a previous session crashed and left `qflex-dev` running, `./dep start-docker --background` will fail with "container name in use" — `./dep stop-docker` first to clear it (idempotent).

`--worm` is the user's default. Pass `--debug` only when the user asks for it.

## Examples

```bash
# Smoke-test that the image and mount work
./dep exec --command "./qflex --help"

# Actual single-node boot from YAML
./dep exec --command "./qflex boot -c conf/DC/dc.yaml"

# Multi-node group dispatch — one process per leaf, mp.Process under the hood
./dep exec --command "./qflex boot -c conf/DC/dc-multi.yaml"

# Path A interaction script (auto-flips telnet) — see the boot-load-interactive skill
./dep exec --command "./qflex boot -c conf/DC/dc-multi.yaml \\
                       --interaction-script ./sample_scripts/login_and_ls.exp"

# Run the pytest suite inside the container (same image, same Python env qflex uses)
./dep exec --command "make test"
```

Wrap multi-line commands in `"…"` so bash treats them as one `--command` argument. Inside the container they execute as `bash -c "<command>"`, so any normal bash works (`&&`, `;`, env vars, redirection).

## Pre-flight checklist

Before bringing the container up, confirm:

1. **Docker is reachable.** `docker info` should succeed.
2. **The qflex image is available locally.** `docker image ls | grep parsa-epfl/qflex`. If not, the user needs `./dep build-docker [--worm] [--debug]` first (or to be online to pull from GHCR).
3. **The mounting folder exists.** `test -d <mounting-folder>` — bind-mounted with `-v <path>:<path>`; a missing host path fails with `mount: permission denied`.
4. **The host cwd has the qflex source files.** `DockerStarter` `assert`s on `QEMU_EFI.fd`, `templates/`, `typer_inputs/`, `commands/`, `partition.py`, `result.py` being in cwd. Always run `./dep` from the repo root.
5. **No stale `qflex-dev` container.** If `docker ps --filter name=qflex-dev` shows one, either reuse it (skip `start-docker`) or `./dep stop-docker` first.

If any of these fail, surface the failure to the user before going further.

## Output handling

`./dep exec` runs `subprocess.run(shell=True)` with `to_stdio=True`, so output streams live to your Bash tool's stdout/stderr capture. For long-running commands (any QEMU boot is at least minutes), invoke via `Bash` with a generous `timeout` (e.g. `timeout=600000` for 10 minutes) and consider `run_in_background=true` for things like `make qemu-build` so you can poll while doing other work.

When output is large (full QEMU log, gdb backtraces), prefer redirecting inside the bash to a host-visible file — the mounting folder is bind-mounted, so paths under it are durable:

```bash
./dep exec --command "./qflex boot -c conf/DC/dc.yaml > /mnt/sdc/data-caching-1c/boot.log 2>&1"
```

Then `tail` the log from the host afterwards to summarize for the user.

## When to use this skill vs. dry-run

| Situation | Real run (this skill) | Dry-run (testing skill) |
|---|---|---|
| Verifying the bash structure of a phase | ❌ overkill | ✅ |
| Master-first ordering invariants | ❌ overkill | ✅ — `assert_two_node_master_first` |
| Smoke that the image + mounts actually work | ✅ | ❌ |
| Reproducing a runtime crash the user saw | ✅ | ❌ |
| End-to-end on a real disk image (alpine boot, savevm) | ✅ | ❌ |
| Adding a regression test | usually ❌ — keep CI fast | ✅ |
| User explicitly says "actually run" / "for real" | ✅ | ❌ |

Default to dry-run when adding tests; default to real runs only when the user asks or the failure shape can't be reproduced without one.

## Failure modes & gotchas

- **`./dep exec` exit codes propagate.** A QEMU crash inside the container surfaces as a non-zero exit. The Bash tool will report it and the live-streamed output usually has the gdb backtrace.
- **The container runs as root inside.** Files written under the mounting folder (especially `experiments/<name>/run/...`) are owned by root from the host's perspective. If the user later wants to delete them outside the container, they may need `sudo`.
- **`--shm-size=128g` is required for multi-node.** The default 64m breaks PDES silently. `start-docker --background` keeps the same flag as the interactive variant, but if you ever construct your own docker invocation, don't shrink it.
- **Stale `/dev/shm/pdes_*` from a crashed prior run.** Multi-node real runs that crashed leave shm files behind. Run `./dep exec --command "./clean_up.sh"` first.
- **Multi-node tmux mode (`interactive_tmux: true`) doesn't fit `./dep exec`.** That path needs a tmux server attached to a tty; the bg container has no tty. For the tmux interactive workflow the user must use `./dep start-docker` (no `--background`) first, then run `./qflex boot ...` inside.
- **Path A scripts can be invoked via `./dep exec`** — the `interaction_script` runs alongside QEMU inside the container. `sample_scripts/` IS mounted into the container at `/home/dev/qflex/sample_scripts/`, so `--interaction-script /home/dev/qflex/sample_scripts/login_and_ls.exp` works.
- **Don't forget `stop-docker` at the end.** A stray `qflex-dev` container blocks the next session start. Tests should always teardown in a fixture finally-block.

## Wiring this into a test

The pytest suite stays dry-run-only by default (fast, hermetic). Real-run tests live in [tests/test_real_runs.py](../../../tests/test_real_runs.py), gated behind `QFLEX_REAL_RUN_TESTS=1` so default `make test` skips them.

The session-scoped `dev_container` fixture there is the canonical pattern:

```python
@pytest.fixture(scope="session")
def dev_container():
    if not os.environ.get("QFLEX_REAL_RUN_TESTS"):
        pytest.skip(...)
    # 1) cleanup any stale container
    subprocess.run(["./dep", "stop-docker"], cwd=REPO_ROOT, capture_output=True)
    # 2) start fresh
    subprocess.run(["./dep", "start-docker", "--mounting-folder", ..., "--worm", "--background"],
                   cwd=REPO_ROOT, check=True, ...)
    yield mounting
    # 3) teardown
    subprocess.run(["./dep", "stop-docker"], cwd=REPO_ROOT, capture_output=True)


def test_something_real(dev_container):
    r = subprocess.run(["./dep", "exec", "--command", "./qflex --help"],
                       cwd=REPO_ROOT, capture_output=True, text=True, timeout=60)
    assert r.returncode == 0
```

Run with `QFLEX_REAL_RUN_TESTS=1 make test` (or `pytest tests/test_real_runs.py`) when you want them; ordinary `make test` skips them. Override the mounting folder via `QFLEX_REAL_RUN_MOUNTING=<path>` if needed.

## Key files

- [./dep](../../../dep) — `start-docker`, `exec`, `stop-docker` subcommands.
- [commands/docker.py](../../../commands/docker.py) — `DockerStarter` (with `background=` flag), `DockerExec`, `DockerStop`. The `DEFAULT_CONTAINER_NAME = "qflex-dev"` constant lives at module top.
- [tests/test_real_runs.py](../../../tests/test_real_runs.py) — the smoke + alpine-login test, plus the `dev_container` fixture pattern.
- [.claude/skills/dep/SKILL.md](../dep/SKILL.md) — full reference for the host-side CLI.
- [.claude/skills/testing/SKILL.md](../testing/SKILL.md) — the dry-run-based test pattern (default for new tests).
- [.claude/skills/boot-load-interactive/SKILL.md](../boot-load-interactive/SKILL.md) — Path A and Path B for boot/load, relevant when the user is asking for a real run that includes guest interaction.
