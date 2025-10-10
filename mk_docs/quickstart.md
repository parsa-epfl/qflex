---
title: Getting Started
description: Use a shared args file to avoid repeating flags, then boot the OS to install requirements.
tags: [getting-started, args, qflex, boot]
---

# Let's get started

To avoid repeating long flag lists, keep common options in a single file and pass them to `qflex` with `xargs`.

---
title: Section 0 — Create Base Image
description: Create the initial QEMU base image in the same folder used by your shared args file.
tags: [qflex, images, base-image]
---

# Section 0: Create the base image

Before booting, create a base image in the **same directory** you use for `--image-folder` and `--working-directory` in your shared args file.

## Command

```bash
./qflex create-base-image --image-folder <YOUR_FOLDER>
```

- Set `<YOUR_FOLDER>` to the path you use in `qflex.args`:
  ```text
  --image-folder <YOUR_FOLDER>
  ```
- By default, the image name aligns with your args (e.g., `--image-name root.qcow2` if you kept the template).

!!! tip "See all options"
    Explore additional flags for sizing, format, or naming:
    ```bash
    ./qflex create-base-image --help
    ```
    Keep `--image-folder` consistent with your later steps so snapshots and artifacts stay together.


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
xargs -a ./qflex.args -- ./qflex boot
```
Add any **run-specific** flags after the command:
```bash
xargs -a ./qflex.args -- ./qflex boot --use-cd-rom
```

---

## 2) Boot to bring up the OS and install requirements

Use the `boot` action to start the OS with your common arguments. Once the VM is up, install your workload’s dependencies inside the guest (e.g., package manager installs, copying configs, etc.).

```bash
xargs -a ./qflex.args -- ./qflex boot --use-cd-rom
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
   (qemu) savevm boot
   ```
!!! tip "Snapshot name"
    You can change the snapshot name if you want but would not to replace it for the next steps if you do.

!!! info "What you have now"
    You’ve created a reusable image snapshot that can be used by the CLI for the next steps (e.g., statistical sampling).


---
title: Section 4 — Load
description: Restore the boot snapshot and bring up workload components with ordering and timing.
tags: [qflex, load, snapshots, orchestration]
---

## 4) Load

**Load** restores the prepared VM state you saved after boot and then brings up the workload components that require ordering or timing (e.g., start a server before a client on different cores).

---

## Restore the snapshot and start the workload

Use the snapshot saved in the previous step (created from the QEMU monitor with `savevm boot`) and load it:

```bash
xargs -a ./qflex.args -- ./qflex load \
  --loadvm-name boot
```

Afterwards you can run your commands to install and start your workload within qemu.

- The value for `--loadvm-name` should match the snapshot you saved in **Section 2 (Boot)**:
  ```text
  (qemu) savevm boot
  ```
- Loading returns the VM to that exact ready-to-run state so you **don’t repeat OS or package installs**.

Once you have started your workload you can save a snapshot by bringing up qemu monitor and saving a snapshot:
  ```text
  (qemu) savevm loaded
  ```
---

## Why “Load” is needed

Workloads often have multiple components that must respect **order** and **time**:

- **Ordering:** Start upstream services first (e.g., server), then downstream components (e.g., client).
- **Timing:** A client may wait for a server socket to be ready or for a specific time boundary.

---

## Tips

!!! tip "Snapshot naming"
    Use descriptive names (e.g., `boot`, `boot-deps`, `baseline-webapp`) so you can load the right prep stage for your experiment.

!!! tip "Readiness checks"
    Inside the guest, ensure launch scripts perform **readiness probes** (e.g., wait for port open) so dependent components don’t start prematurely.

---
title: Section 5 — Init Warm
description: Initialize long-term microarchitectural state (cache, predictors, TLB) before functional warming.
tags: [qflex, init_warm, microarchitecture, snapshots]
---

## 5) Init Warm

**Init Warm** initializes long-term microarchitectural states—such as **caches**, **branch predictors**, and **TLBs**—to a realistic, steady baseline **before functional warming** begins.

---

## Run init warm

Load from the prior **Load** snapshot and initialize the microarchitectural state:

```bash
xargs -a ./qflex.args -- ./qflex initialize \
  --loadvm-name loaded
```

- `--loadvm-name loaded` should match the snapshot produced in **Section 4 (Load)**.

---

## Snapshot created automatically

Upon successful completion, a QEMU snapshot named **`init_warmed`** is created on the image.  
This snapshot captures the initialized microarchitectural state so subsequent stages can start from a consistent baseline.

!!! info "What you have now"
    A VM snapshot (**`init_warmed`**) with warmed long-term microarchitectural state, ready for the next step.

---
title: Section 6 — Functional Warming
description: Simulate for the population duration to produce checkpoints for later detailed simulation.
tags: [qflex, functional-warming, checkpoints, sampling]
---

## 6) Functional Warming

**Functional warming** runs the VM forward for the configured **population length** (in seconds) and emits **checkpoints** that will be used later for detailed simulation. This stage advances time and activity enough to expose representative program phases while remaining much faster than full detailed simulation.

---

## Run functional warming

Start from the **init_warmed** snapshot created in the previous step:

```bash
xargs -a ./qflex.args -- ./qflex fw \
  --loadvm-name init_warmed
```

- The warm length is controlled by your common args (e.g., `--population-seconds 0.00001` and `--sample-size 10`).
- Output includes **checkpoint markers** that later stages can load to run short, focused detailed slices.

---

## Parameters used during functional warming

The `fw` step is controlled mostly by values you set in your shared args file. Here’s what matters and how to pick them:

- **`--population-seconds <float>`**  
  How long to *functionally* run the workload to mine candidate regions. Larger values explore more behavior (more coverage) but take longer.  
  *Rule of thumb:* start small to iterate (e.g., `1e-5`–`1e-3`) and grow if checkpoints don’t look representative.

- **`--sample-size <int>`**  
  How many **checkpoints** to produce from the population window. More samples increase representativeness but will cost more time later when you run detailed slices.  
  *Common patterns:*  
  - Quick profiling: `5–10`  
  - Deeper studies: `20–50`

- **`--check-period-quantum-coeff <int>`**  
  Controls how frequently `fw` evaluates whether to create a checkpoint relative to your execution quantum (see `--quantum-size-ns`). Higher values mean **fewer checks** (coarser cadence), lower values mean **more frequent** opportunities. Use this to avoid over/under-checkpointing.

- **`--quantum-size-ns <int>`**  
  The scheduler “tick” used during fast-forwarding and warming. Smaller quanta can make progress/decision points finer-grained (potentially more overhead), larger quanta coarser (fewer decision points).

- **`--no-consolidated`**  
  Keep checkpoints as separate artifacts rather than collapsing/merging. This is helpful when you want explicit per-slice control later.

- **`--experiment-name`, `--workload-name`**  
  Used to organize/namescope artifacts and logs. Choose stable names so results from different runs don’t collide.

- **`--working-directory`, `--image-folder`**  
  Locations where run outputs and the QEMU image (and its snapshots) live. Ensure these have enough space and are consistent across steps so later phases can find the checkpoints.

!!! tip "Choosing values"
    Pick `--population-seconds` for **coverage**, then set `--sample-size` to match how many detailed slices you can afford. If checkpoints cluster too closely (or too sparsely), adjust `--check-period-quantum-coeff` (and, if needed, `--quantum-size-ns`) to tune cadence.


