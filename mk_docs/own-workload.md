---
title: Your own workload
description: Create a disk image, install and start your workload, and save the snapshot QFlex experiments start from.
tags: [workload, image, boot, load, snapshot]
---

# Your own workload

The [Quickstart](quickstart.md) starts from a prebuilt workload. To simulate another workload, prepare the disk
and the snapshot yourself in three steps, then continue with the Quickstart's [Init Warm](quickstart.md#3-init-warm):

1. **Create** an empty disk image;
2. **Boot** it, install the operating system and the workload, and save the snapshot `boot`;
3. **Load** `boot`, start the workload, and save the snapshot `loaded`, in which it runs in steady state.

---

## The args file

Without a prebuilt workload, `qflex.args` describes the machine and the workload itself:

```text
--core-count 8
--double-cores
--quantum-size-ns 2000
--llc-size-per-tile-mb 2
--parallel
--network none
--memory-gb 32
--workload-name web-search
--population-seconds 5
--experiment-name web-search-8c-001
--image-name root.qcow2
--working-folder <WORK_DIR>
--no-unique
--use-image-directly
```

- `--core-count` is the number of simulated cores: 1, 2, 4, 8, 16, 32, 48 or 64.
- `--double-cores` gives the VM twice as many vCPUs: the second half runs the workload's client, as the `phantom`
  [core group](quickstart.md#core-groups). Add `--groups` to describe several workloads or other IPC estimates.
- `--use-image-directly` makes the experiment use `<WORK_DIR>/images/<image name>` itself instead of a copy, so the
  snapshots saved in the steps below stay in your image.
- Keep the machine settings (`--core-count`, `--double-cores`, `--memory-gb`, `--network`) the same in every step:
  a snapshot only restores on the machine it was taken on.

As in the Quickstart, write absolute paths: `xargs` does not expand `~` or `$VARIABLES`.

---

## 1) Create the base image

<!-- termynal -->
```bash

$ ./qflex create-base-image --working-folder <WORK_DIR> --image-name root.qcow2

```

This creates an empty qcow2 disk `<WORK_DIR>/images/root.qcow2` (8 GB; change it with `--image-size-gb`). Use the
same `--working-folder` and `--image-name` as in `qflex.args`.

---

## 2) Boot and install the workload

<!-- termynal -->
```bash

$ xargs -a ./qflex.args -- ./qflex boot --use-cd-rom

```

`--use-cd-rom` attaches the Alpine Linux installer (downloaded on first use). Log in as `root` without a password,
install Alpine onto the disk (`setup-alpine`), then install your workload and its dependencies. Later boots of the
disk do not need `--use-cd-rom`.

When the VM is in the state you want to keep, save it from the QEMU monitor:

1. Press **Ctrl+A**, then **C**. The prompt becomes `(qemu)`.
2. Save the snapshot:
   ```text
   (qemu) savevm boot
   ```
3. Quit with `(qemu) quit`.

---

## 3) Load and start the workload

<!-- termynal -->
```bash

$ xargs -a ./qflex.args -- ./qflex load --loadvm-name boot

```

This restores `boot` with one clock per core, as in the later steps. Start the workload's components inside the
VM, wait until it runs in steady state, and save the snapshot from the QEMU monitor:
```text
(qemu) savevm loaded
```

!!! info "Why Load is a separate step"
    Workloads often have components that must start in **order** and within a **time window** of each other: a
    web server starts after its database is up, and components on different cores must start close together.
    Load lets you script and orchestrate this start-up before saving the snapshot.

---

## Continue with the Quickstart

Run [Init Warm](quickstart.md#3-init-warm) from your snapshot, then follow the Quickstart from there:

<!-- termynal -->
```bash

$ xargs -a ./qflex.args -- ./qflex initialize --loadvm-name loaded

```

Make sure the workload is running in `loaded`: Init Warm waits for the workload to touch the caches, so an idle VM
makes it run for a long time.
