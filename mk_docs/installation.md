---
title: Installation
description: Clone the repository, install CLI requirements, and start the Docker image.
tags: [install, setup, docker]
---

# Installation

Follow these steps to set up the CLI and start the Docker image.

---

## Prerequisites

- **Git** and **Python 3.9+**
- **Docker** running locally (Docker Desktop on Windows, Docker Engine on Linux)
---

## 1) Clone the repository

<!-- termynal -->
``` bash

$ git clone --recursive https://github.com/parsa-epfl/qflex.git
$ cd qflex

```

---

## 2) Install requirements for the CLI

The CLI's Python dependencies are locked in `pyproject.toml` / `uv.lock` and managed by [uv](https://docs.astral.sh/uv/). Install uv once:

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
# or, with Nix
nix profile install nixpkgs#uv
```

Then run the CLI from the repository root. `./qflex` runs itself through uv (see its shebang); the first run creates `.venv` from the lock file (downloading the pinned Python version if needed), and later runs reuse it:

```bash
./qflex --help
uv run ./dep --help
```

!!! note "Run from the repository root"
    uv finds `pyproject.toml` from the current directory, so run `./qflex` from the repository root (its commands also expect that).

!!! tip "Inside Docker"
    The Docker image already contains this environment, so `./qflex` works directly there.

---

## 3) Start the Docker image

=== "Windows (PowerShell)"
    <!-- termynal -->
    ```powershell

    $ uv run python dep start-docker --worm --mounting-folder <mounting_folder>

    ```

=== "Linux"
    <!-- termynal -->
    ```bash

    $ uv run ./dep start-docker --worm --mounting-folder <mounting_folder>

    ```

---

## Verify

After the container starts, run:

<!-- termynal -->
```bash

$ ./qflex --help

```
You should see qflex command-line options.


---

## Need more on Docker or dependencies?

!!! tip "Where to look next"
    - Read the **Docker** guide for container usage, mounts, and troubleshooting: [Docker](reference/docker.md)
    - Use the **dep** helper to explore commands and manage dependencies:
      ```bash
      # Linux/macOS
      uv run ./dep --help

      # Windows (PowerShell)
      uv run python dep --help
      ```
    - To (re)start the container with your workspace mounted:
      ```bash
      # Linux/macOS
      uv run ./dep start-docker --worm --mounting-folder <mounting_folder>

      # Windows (PowerShell)
      uv run python dep start-docker --worm --mounting-folder <mounting_folder>
      ```


## Troubleshooting

??? warning "Common issues"
    **Permission denied on `./dep` (Linux)**  
    Make it executable:  
    ```bash
    chmod +x dep
    ```

    **Docker not found / not running**  
    Start Docker Desktop (Windows) or the Docker daemon (Linux):  
    ```bash
    systemctl status docker
    ```
