---
title: Getting Started
description: Use a shared args file to avoid repeating flags, then boot the OS to install requirements.
tags: [getting-started, args, qflex, boot]
---

# Let's get started

To avoid repeating long flag lists, keep common options in a single file and pass them to `qflex` with `xargs`.

---

## 1) Create a shared args file (and the run pattern)

Create `./qflex.args` (a.k.a. *qflex.common.args*) with **one argument per line**.

**Template (replace `<MOUNT_DIR>`):**
```text
--core-count 8
--double-cores
--quantum-size-ns 2000
--llc-size-per-tile-mb 2
--parallel
--network none
--memory-gb 32
--host-name SAPHIRE
--workload-name web-search
--population-seconds 0.00001
--sample-size 10
--no-consolidated
--primary-ipc 4.2
--primary-core-start 0
--phantom-cpu-ipc 8
--experiment-name experiment004
--image-name root.qcow2
--image-folder <MOUNT_DIR>
--working-directory <MOUNT_DIR>
--check-period-quantum-coeff 2
--no-unique
--use-image-directly
```

!!! tip "Inline flags are possible, but"
    You can pass these flags directly on the command line, but keeping them in `qflex.args` avoids duplication and reduces mistakes.

**Run pattern using `xargs`**  
Use `xargs` to feed the file’s flags to `./qflex`:
```bash
xargs -a ./qflex.args -- ./qflex
```
Add any **run-specific** flags after the command:
```bash
xargs -a ./qflex.args -- ./qflex boot --use-cd-rom
```

---

## 2) Boot to bring up the OS and install requirements

Use the `boot` action to start the OS with your common arguments. Once the VM is up, install your workload’s dependencies inside the guest (e.g., package manager installs, copying configs, etc.).

```bash
xargs -a ./qflex.args -- ./qflex boot
```

- This brings up the VM using the CPU/memory/topology and paths defined in `qflex.args`.
- After installing requirements and preparing the environment, you can proceed to your next workflow steps (e.g., sampling).
- With `--image-folder` and `--working-directory` pointing to your mount, you can place existing qemu images in `<MOUNT_DIR>` and access them from within the docker contianer and skip this step.

---

## 3) Save a snapshot of the image

Once the VM is in the desired state:

1. Open the QEMU monitor: press **Ctrl+A**, pause briefly, then press **C**.  
   You should see a prompt like `(qemu)`.

2. Save a snapshot:
   ```text
   (qemu) savevm <your snapshot name>
   ```

!!! info "What you have now"
    You’ve created a reusable image snapshot that can be used by the CLI for the next steps (e.g., statistical sampling).



---


## 4) Explore the CLI

Get a feel for available commands:

```bash
./qflex --help
```

For details on `boot` options:

```bash
./qflex boot --help
```

---

## Next steps

- Proceed to the sampling workflows in the CLI once your snapshot is ready.
- Learn more about **`./qflex`** commands in the [CLI](reference/qflex.md) section.
- For Docker-specific usage and dependency handling, see [Docker](reference/docker.md) and the `dep` helper:
  ```bash
  ./dep --help
  ```
