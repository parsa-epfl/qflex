---
title: Installation
description: Clone the repository, build QFlex with Nix or Docker, and set up the CLI.
tags: [install, setup, nix, docker]
---

# Installation

QFlex has two parts:

- the **simulation stack**: two QEMUs (`fw-qemu` for functional warming, `timing-qemu` for detailed simulation), the Flexus simulators and `bxdb`. It is built once, with **Nix** or inside **Docker**;
- the **`./qflex` CLI**, a Python script that drives the experiments. It runs through [uv](https://docs.astral.sh/uv/).

---

## Prerequisites

- **Git**
- **uv**, for the CLI (step 2)
- **Nix** with flakes enabled, **or** **Docker** (step 4)
- Disk space for a **working folder** (step 3): prebuilt workloads take a few GB to over 100 GB each

---

## 1) Clone the repository

<!-- termynal -->
```bash

$ git clone --recursive https://github.com/parsa-epfl/qflex.git
$ cd qflex

```

`--recursive` also clones the submodules (the QEMUs, Flexus, WormCache and bxdb).

---

## 2) Install uv for the CLI

The CLI's Python dependencies are locked in `pyproject.toml` / `uv.lock`. Install uv once:

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
# or, with Nix
nix profile install nixpkgs#uv
```

`./qflex` runs itself through uv (see its shebang): the first run creates `.venv` from the lock file, downloading the pinned Python version if needed, and later runs reuse it.

!!! note "Run from the repository root"
    uv finds `pyproject.toml` from the current directory, and the commands expect the repository layout, so always run `./qflex` from the repository root.

---

## 3) Create a working folder

QFlex keeps all its data in one **working folder**, `<WORK_DIR>` in this documentation. It holds:

- `<WORK_DIR>/images/`: the disk images, including the downloaded prebuilt workloads (`images/prebuilt/<id>/`);
- `<WORK_DIR>/experiments/`: one folder per experiment, with its configuration, checkpoints and results.

Choose a location with enough space, outside the repository, and create it:

<!-- termynal -->
```bash

$ mkdir -p <WORK_DIR>

```

Every `./qflex` command takes it as `--working-folder <WORK_DIR>` (written once in `qflex.args`, see the
[Quickstart](quickstart.md)). Images are the largest part: to keep them on another disk, make `<WORK_DIR>/images` a
symbolic link.

---

## 4) Build the simulation stack

=== "Nix"
    The flake provides the whole toolchain (LLVM 19, Boost 1.83, Rust, `just`, ...). Build everything in a **pure** shell (`-i`), so that nothing from the host environment leaks into the build:
    <!-- termynal -->
    ```bash

    $ nix develop -i --keep HOME --command just all

    ```

    - `-i` clears the environment; `--keep HOME` keeps only `HOME`, which cargo needs for its crate cache (`~/.cargo`).
    - For an interactive build shell, run `nix develop -i --keep HOME` and then `just ...` (`just --list` shows every recipe).

    !!! warning "Nix is only the build environment"
        Run `./qflex` **outside** the Nix shell: uv is not part of it. qflex still uses Nix where it compiles: the build records `nix` in `out/build-env`, and every experiment then builds its WormCache plugin with `nix develop -i --keep HOME <qflex>`, the same toolchain as the QEMUs it plugs into. `nix` must therefore stay in your `PATH`.

=== "Docker"
    Start the QFlex container with your working folder mounted:

    === "Linux"
        <!-- termynal -->
        ```bash

        $ uv run ./dep start-docker --worm --mounting-folder <WORK_DIR>

        ```

    === "Windows (PowerShell)"
        <!-- termynal -->
        ```powershell

        $ uv run python dep start-docker --worm --mounting-folder <WORK_DIR>

        ```

    `--mounting-folder` mounts `<WORK_DIR>` into the container **at the same path**, so the images and experiments stay
    on your machine and `--working-folder <WORK_DIR>` works the same inside and outside the container. If
    `<WORK_DIR>/images` is a symbolic link to another disk, mount that disk too, or the link is broken inside the
    container.

    The image contains the built stack and the CLI's environment, so `./qflex` works directly inside the container. See [Docker](reference/docker.md) for the options of `dep`.

---

## 5) Verify

After a Nix build, these files exist:

| Path | What it is |
|------|------------|
| `fw-qemu/build/qemu-system-aarch64`, `fw-qemu/build/qemu-img` | Functional warming QEMU |
| `timing-qemu/build/qemu-system-aarch64` | Detailed (timing) QEMU |
| `out/kraken/mem{1,2,4,6,8}/lib{knottykraken,semikraken}.so` | Flexus simulators, one variant per memory controller count |
| `bxdb/target/release/bxdb` | Checkpoint database tool |
| `out/build-env` | `nix` (or `native` for a build outside Nix) |

Then, from the repository root (outside the Nix shell):

<!-- termynal -->
```bash

$ ./qflex --help

```

You should see the list of commands, including `image`. Continue with the [Quickstart](quickstart.md).

---

## Troubleshooting

??? warning "Common issues"
    **`./qflex`: `uv: command not found`**
    You are probably inside `nix develop`; leave it (`exit`), or install uv (step 2).

    **`error: experimental Nix feature 'flakes' is disabled`**
    Enable flakes, e.g. add `experimental-features = nix-command flakes` to `~/.config/nix/nix.conf`.

    **`QFlex was built with Nix (.../out/build-env), but nix is not in PATH`**
    qflex builds the WormCache plugin of each experiment with Nix (see step 4); make `nix` available where you run `./qflex`.

    **Permission denied on `./dep` or `./qflex` (Linux)**
    Make it executable: `chmod +x dep qflex`.

    **Docker not found / not running**
    Start Docker Desktop (Windows) or the Docker daemon (Linux): `systemctl status docker`.

!!! tip "More on the build"
    The recipes live in `./justfile`; [docs/01-BUILD-FLEXUS.md](https://github.com/parsa-epfl/qflex/blob/main/docs/01-BUILD-FLEXUS.md) covers building without Nix or Docker, the build profiles and the Flexus variants.
