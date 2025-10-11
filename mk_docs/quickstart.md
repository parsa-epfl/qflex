---
title: Getting Started
description: Use a shared args file to avoid repeating flags, then boot the OS to install requirements.
tags: [getting-started, args, qflex, boot]
---

# Let's get started

!!! note "Before you begin"
    Make sure your environment is ready using **one** of the following ways:
    
    - **All requirements installed locally**, **or**
    - **You’ve started the Docker environment with the `dep` helper** (see [Installation](installation.md) and [Docker](reference/docker.md)):
      ```bash
      ./dep start-docker --worm --mounting-folder $PWD
      ```

---

## 0) Create the base image

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

To avoid repeating long flag lists, keep common options in a single file and pass them to `qflex` with `xargs`.
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

Use the `boot` action to start the OS with your common arguments. Once the VM is up, install your workload’s dependencies inside the qemu image (e.g., package manager installs, copying configs, etc.).

```bash
xargs -a ./qflex.args -- ./qflex boot --use-cd-rom
```

- This brings up the VM using the CPU/memory/ defined in `qflex.args`.
- With `--image-folder` and `--working-directory` pointing to your mount, you can place existing qemu images in `<MOUNT_DIR>` and access them from within the docker contianer and skip this step.

---

### Save a snapshot of the image and qemu monitor

Once the VM is in the desired state:

1. Open the QEMU monitor: press **Ctrl+A**, pause briefly, then press **C**.  
   You should see a prompt like `(qemu)`.

2. Save a snapshot:
   ```text
   (qemu) savevm boot
   ```
!!! tip "Snapshot name"
    You can change the snapshot name if you want but this name is used in later steps in this tutorial.

!!! info "What you have now"
    You’ve created a reusable image snapshot that has your workload dependencies installed and is ready for the next steps.


---

## 3) Load

**Load** restores the prepared VM state you saved after boot and where you can start the workload, and each core will have a clock.

---

## Restore the snapshot and start the workload

Use the snapshot saved in the previous step (created from the QEMU monitor with `savevm boot`) and load it:

```bash
xargs -a ./qflex.args -- ./qflex load \
  --loadvm-name boot
```

Afterwards you can run your commands to start your workload within qemu.

- The value for `--loadvm-name` should match the snapshot you saved in **Section 2 (Boot)**:
  ```text
  (qemu) savevm boot
  ```

Once you have started your workload started you can save a snapshot by bringing up qemu monitor and saving a snapshot:
  ```text
  (qemu) savevm loaded
  ```
---

### Why “Load” is needed

Workloads often have multiple components that must respect **order** and **time** as they are starting, i.e. a web server must start after the database is up and running and if the components are running on different cores they need to be started within a certain time window of each other. The **Load** step allows you to script and orchestrate this process.

---

## 5) Init Warm

**Init Warm** initializes long-term microarchitectural states—such as **caches**, **branch predictors**, and **TLBs** so that **functional warming** can start.

---

### Run init warm

Load from the prior **loaded** snapshot and initialize the microarchitectural state:

```bash
xargs -a ./qflex.args -- ./qflex initialize \
  --loadvm-name loaded
```

- `--loadvm-name loaded` should match the snapshot produced in **Section 4 (Load)**.

---

### Snapshot created automatically

Upon successful initialization, a QEMU snapshot named **`init_warmed`** is created on the image.  
This snapshot captures the initialized microarchitectural state so subsequent stages can start from a consistent baseline.

!!! info "What you have now"
    A VM snapshot (**`init_warmed`**) with warmed long-term microarchitectural state, ready for the next step.
    This step and all the steps before it only need to be done once per workload, unless changing the workload configuration.

---

## 6) Functional Warming

**Functional warming** runs the VM forward for the configured **population length** (in seconds) and emits **checkpoints** that will be used later for timing (detailed) simulation.

---

### Run functional warming

Start from the **init_warmed** snapshot created in the previous step:

```bash
xargs -a ./qflex.args -- ./qflex fw \
  --loadvm-name init_warmed
```

- The warm length is controlled by your common args (e.g., `--population-seconds 0.00001` and `--sample-size 10`).
- Output includes **checkpoints** that later stages can load to run short, timing simulation.

---

## 7) Partitioning

After checkpoints are created from functional warming, you can split the checkpoints into partitions to be ran in parallel during detailed simulation (using flexus). 
```bash
xargs -a ./qflex.args -- ./qflex partition --partition-count 16
```

You can adjust the `--partition-count` to fit your needs.

- This creates folders named `partition_0`, `partition_1`, ..., `partition_15` in the run folder of the experiments folder (for `--partition-count 16`).


---

## 8) Running Partitions
After creating the partitions you will have the script that manages running all partitions in timing simulation using flexus. 
```
xargs -a ./qflex.args -- ./qflex --warming-ratio 2 --measurement-ratio 1
```
This command will run all the partitions with spending twice as much cycles warming as measuring. You can adjust the `--warming-ratio` and `--measurement-ratio`.

---
## 9) Collecting Results
After the detailed simulation is done you can collect the results and decided whether or not you want to reiterate from the functional warming step as it uses an estimate IPC. The result command creates estimated IPCs that can be more accurate and with more accurate IPC you can re-iterate. 
```bash
xargs -a ./qflex.args -- ./qflex result
```
This will automatically create a new core_info.csv file in the experiment folder with the updated IPC values. ready for you to re-iterate the experiment. 

However if you decide to reiterate you should first remove the partitions folders.
```bash
xargs -a ./qflex.args -- ./qflex partition-cleanup
```

<!-- Warning that this will remove all partition folders and their contents -->
!!! warning "Warning"
    This will remove all partition folders and their contents. Make sure you have collected any necessary data before running this command.



