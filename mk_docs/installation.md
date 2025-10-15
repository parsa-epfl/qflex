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
- (Optional) **SSH key** configured for GitHub if using the SSH clone URL

!!! note "Cloning over SSH"
    The command below uses the SSH URL. Ensure your GitHub SSH key is set up and loaded (e.g., `ssh -T git@github.com`).  
    Prefer HTTPS? Replace the URL accordingly.

---

## 1) Clone the repository

<!-- termynal -->
``` bash

$ git clone --recursive git@github.com:parsa-epfl/qflex.git
$ cd qflex

```

---

## 2) Install requirements for the CLI

```bash
pip install -r requirements.txt
```

!!! tip "Use a virtual environment (recommended)"
    ```bash
    python -m venv .venv
    # macOS/Linux
    source .venv/bin/activate
    # Windows (PowerShell)
    .\.venv\Scripts\Activate.ps1

    pip install -r requirements.txt
    ```

---

## 3) Start the Docker image

=== "Windows (PowerShell)"
    <!-- termynal -->
    ```powershell

    $ python dep start-docker --worm --mounting-folder <mounting_folder>

    ```

=== "Linux"
    <!-- termynal -->
    ```bash

    $ ./dep start-docker --worm --mounting-folder <mounting_folder>

    ```

---

## Verify

After the container starts, run:

<!-- termynal -->
```bash

$ python dep status

```

You should see a healthy status indicating the image is running and your working directory is mounted.

---

## Need more on Docker or dependencies?

!!! tip "Where to look next"
    - Read the **Docker** guide for container usage, mounts, and troubleshooting: [Docker](reference/docker.md)
    - Use the **dep** helper to explore commands and manage dependencies:
      ```bash
      # Linux/macOS
      ./dep --help

      # Windows (PowerShell)
      python dep --help
      ```
    - To (re)start the container with your workspace mounted:
      ```bash
      # Linux/macOS
      ./dep start-docker --worm --mounting-folder <mounting_folder>

      # Windows (PowerShell)
      python dep start-docker --worm --mounting-folder <mounting_folder>
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

    **SSH clone fails**  
    Test your SSH setup:  
    ```bash
    ssh -T git@github.com
    ```
