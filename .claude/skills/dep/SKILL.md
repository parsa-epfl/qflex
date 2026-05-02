---
name: dep
description: Use when working with the host-side `./dep` Typer CLI — the launcher that builds the QFlex Docker dev image, starts containers, and execs commands into a long-running one. Covers `./dep build-docker`, `./dep start-docker` (interactive default + `--background` for a detached named container), `./dep exec` (docker exec into the running container, NOT a fresh `docker run --rm`), `./dep stop-docker`, the canonical container name `qflex-dev`, the docker image-name resolution (`get_docker_image_name` + version), the mount layout (host repo → `/home/dev/qflex/...`, host mounting folder mapped at the same path inside, `sample_scripts/` and `tests/` also mounted), and the host-side requirements (`QEMU_EFI.fd`, `templates/`, `typer_inputs/`, `commands/`, `partition.py`, `result.py` must be in cwd). TRIGGER when the user mentions `./dep`, `dep start-docker`, `dep build-docker`, `dep exec`, `dep stop-docker`, dev container, qflex-dev, the worm / debug variants, or asks about the Dockerfile / image tags. SKIP for what to run *inside* the container (qflex-cli / qflex-commands skills) or for the workflow of driving qflex commands against a real container (run-in-dev-container skill).
---

# `./dep` — the host-side container launcher

[./dep](../../../dep) is the Typer app that runs **outside** the dev container. It builds the QFlex docker image and starts containers (interactive or non-interactive). The pipeline (`./qflex …`) runs **inside** the container; `./dep` is just the door.

## CLI surface

```sh
./dep build-docker [--debug] [--worm] [--worm-only] [--push]
./dep start-docker --mounting-folder <path> [--debug] [--worm] [--start-directory <dir>] [--background] [--container-name qflex-dev]
./dep exec --command "<bash>" [--container-name qflex-dev] [--working-directory /home/dev/qflex]
./dep stop-docker [--container-name qflex-dev]
```

(`./dep --help` prints the same with autocompletion hints.)

| Subcommand | What it does | Notes |
|---|---|---|
| `build-docker` | `docker buildx` for the dev image (`Dockerfile` → deps; `Dockerfile.qemu.{release,debug}` → qflex; optionally `Dockerfile.WormCacheQFlex`). Tags `ghcr.io/parsa-epfl/qflex:<variant>-<version>`. `--push` pushes to GHCR (auth required). | Implemented by `DockerBuild` in [commands/docker.py](../../../commands/docker.py). |
| `start-docker` (default) | `docker run -it --entrypoint /bin/bash <image>` with all the QFlex mounts. Drops the user into an interactive bash inside the container. | Implemented by `DockerStarter` ([commands/docker.py](../../../commands/docker.py)). `--mounting-folder` is required — that path is mounted into the container at the same absolute path. |
| `start-docker --background` | `docker run -d --name qflex-dev --entrypoint /bin/bash <image> -c "tail -f /dev/null"`. Same mounts as the interactive variant, but detached + named + kept alive. Subsequent `./dep exec` calls land inside this container. | The `tail -f /dev/null` keep-alive is the standard "do nothing forever" idiom. The container stays up until `./dep stop-docker`. |
| `exec` | `docker exec -w /home/dev/qflex <name> /bin/bash -c "<command>"` against the running container. Fast — no per-call container start. **Requires** `start-docker --background` to have been run first. | New `DockerExec` class in [commands/docker.py](../../../commands/docker.py); uses `shlex.quote` so internal quotes / spaces in the command Just Work. |
| `stop-docker` | `docker rm -f <name>` — stops + removes in one step. Idempotent (`|| true` swallows the rc when the container doesn't exist). | New `DockerStop` class. |

The user's typical interactive workflow:

```sh
python3 dep start-docker --worm --debug --mounting-folder /mnt/sdc/data-caching-1c/
# then inside the container:
./qflex boot -c conf/DC/dc.yaml
```

The non-interactive (test / agent) workflow:

```sh
./dep start-docker --mounting-folder /mnt/sdc/data-caching-1c/ --worm --background
./dep exec --command "./qflex --help"
./dep exec --command "./qflex boot -c conf/DC/dc.yaml"
./dep exec --command "make test"
./dep stop-docker
```

## Image name resolution

`get_docker_image_name(debug, worm)` ([commands/utils.py](../../../commands/utils.py)) maps the two flags to one of four base names. `get_version()` ([commands/version.py](../../../commands/version.py)) reads [VERSION](../../../VERSION). The full tag is:

```
ghcr.io/parsa-epfl/qflex:<variant>-<version>
```

E.g. `--worm` (no `--debug`) on version `3.4.0` resolves to `ghcr.io/parsa-epfl/qflex:qflex-worm-release-3.4.0`. Variants are pulled from GHCR by default; `build-docker --push` is the way to refresh them.

## Mount layout

`DockerStarter.cmd()` ([commands/docker.py](../../../commands/docker.py)) bind-mounts every host-side qflex artefact into `/home/dev/qflex/<same-name>`:

```
host cwd                          →  /home/dev/qflex/...
  QEMU_EFI.fd                       /home/dev/qflex/QEMU_EFI.fd
  templates/                        /home/dev/qflex/templates
  typer_inputs/                     /home/dev/qflex/typer_inputs
  commands/                         /home/dev/qflex/commands
  flexus/                           /home/dev/qflex/flexus
  parallel-qemu/                    /home/dev/qflex/parallel-qemu
  qemu/                             /home/dev/qflex/qemu
  WormCacheQFlex/                   /home/dev/qflex/WormCacheQFlex
  conf/                             /home/dev/qflex/conf
  Makefile, partition.py, result.py, clean_up.sh, qflex (each individually mounted)
  multi-node-scripts/, multi-node-experiments/, multi-node-dc/, multi-node-dc-old-shanqing/,
  multi-node-web-search/, multi-node-web-search_virtio/, experiments/, micro_scripts/ (when present)

host mounting_folder              →  same absolute path inside the container
                                     (NOT remapped — paths in YAML / CLI Just Work either way)
```

Plus these process-level flags (mostly for multi-node QEMU + gdb):

| Flag | Why |
|---|---|
| `--security-opt seccomp=unconfined` | gdb needs to ptrace; QEMU needs IOCTLs the default seccomp profile blocks. |
| `--cap-add SYS_PTRACE --cap-add SYS_ADMIN` | Same. SYS_ADMIN is needed for some KVM/virtio paths. |
| `--pid=host` | Multi-node nodes need to see each other's PIDs. |
| `--cap-add NET_ADMIN --device=/dev/net/tun` | virtio-net-pci / e1000 setup inside the guest. |
| `--shm-size=128g` | PDES rings under `/dev/shm/pdes_*` need real shared memory; smaller shm = silent multi-node breakage. |

## Preconditions in the host cwd

`DockerStarter.cmd()` `assert`s on the presence of `QEMU_EFI.fd`, `templates/`, `typer_inputs/`, `commands/`, `partition.py`, `result.py` in the cwd. Run `./dep` from the repo root — running it from anywhere else fails on these asserts before docker is invoked.

## When to use which subcommand

- **`start-docker`** (no `--background`) when the user wants an interactive shell to poke around, run builds (`make flexus-build`, `make qemu-build`), or chain qflex commands by hand.
- **`start-docker --background`** to bring up a long-running container that the assistant / CI / tests will exec into. The container persists until `stop-docker`.
- **`exec`** when running a single qflex command against the long-running container. Cheap (no per-call container start) — re-invoke as many times as you like.
- **`stop-docker`** to clean up the long-running container at the end of a session (or as a fixture teardown).

For **actually running qflex commands inside the dev container from this assistant** (not interactive), see the `run-in-dev-container` skill — it owns the start-bg → exec → stop-bg session pattern.

## How the four classes in `commands/docker.py` map to subcommands

| Class | Subcommand | Resulting docker invocation |
|---|---|---|
| `DockerBuild` | `build-docker` | `docker buildx build …` for each Dockerfile variant. |
| `DockerStarter(background=False)` | `start-docker` | `docker run -it --entrypoint /bin/bash … <image>` (interactive). |
| `DockerStarter(background=True)` | `start-docker --background` | `docker run -d --name qflex-dev --entrypoint /bin/bash … <image> -c "tail -f /dev/null"` (detached + named + kept alive). |
| `DockerExec` | `exec` | `docker exec -w /home/dev/qflex qflex-dev /bin/bash -c '<shlex.quote(command)>'`. |
| `DockerStop` | `stop-docker` | `docker rm -f qflex-dev 2>/dev/null \|\| true` (idempotent). |

## Common pitfalls

- **`--mounting-folder` is the host path; the container sees the same absolute path.** Don't try to remap it. YAMLs that set `mounting_folder: /mnt/sdc/...` work identically inside and outside the container because of this.
- **`-it` requires a tty.** Running interactive `./dep start-docker` from a script / agent context errors with `the input device is not a TTY`. Use `./dep start-docker --background` + `./dep exec` instead.
- **`./dep exec` requires the named container to be running.** Without it, `docker exec` fails with `No such container: qflex-dev`. Run `./dep start-docker --background` first, or check with `docker ps --filter name=qflex-dev`.
- **Container name conflict.** Re-running `./dep start-docker --background` while one is already up errors with `container name "/qflex-dev" is already in use`. `./dep stop-docker` first, or pass a different `--container-name`.
- **Version mismatch.** The image tag uses the `VERSION` file's value. If you `bump-my-version` without rebuilding (or pulling), `start-docker` will hang on `docker pull` for a tag that doesn't exist. Rebuild with `./dep build-docker [--worm] [--debug]`.
- **`./dep` shells out via `subprocess.run(shell=True, …)` like every other Executor.** It honours `QFLEX_DRY_RUN=1` (would print the docker invocation instead of running it), though that's rarely useful — just inspect `DockerStarter.cmd()` / `DockerExec.cmd()` / `DockerStop.cmd()` directly if you want to read the bash.
- **Versions of `--worm` and `--debug` must match between build and start.** If you built with `--worm --debug` but start with just `--worm`, `start-docker` resolves a different tag and either pulls (slow) or fails.
- **`./dep exec` no longer takes `--mounting-folder` / `--worm` / `--debug`.** Those are baked in when the container was started; exec just runs against the running one. If you need different mounts, stop and re-start the container.

## Key files

- [./dep](../../../dep) — Typer entry point; three subcommands.
- [commands/docker.py](../../../commands/docker.py) — `DockerStarter`, `DockerBuild`. The `command=` constructor arg is what powers `./dep exec`.
- [commands/utils.py](../../../commands/utils.py) — `get_docker_image_name(debug, worm)`.
- [commands/version.py](../../../commands/version.py) + [VERSION](../../../VERSION) — image tag suffix.
- [Dockerfile](../../../Dockerfile), [Dockerfile.qemu.release](../../../Dockerfile.qemu.release), [Dockerfile.qemu.debug](../../../Dockerfile.qemu.debug), [Dockerfile.WormCacheQFlex](../../../Dockerfile.WormCacheQFlex) — build inputs.
