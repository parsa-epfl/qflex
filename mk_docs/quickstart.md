---
title: QFlex - Statistical Sampling Quickstart, Fast and Accurate Simulation
description: Run a statistical sampling experiment with QFlex, from a prebuilt workload to the simulation result.
tags: [getting-started, QFlex, args, reproducibility, performance, sampling, statistical-simulation, tutorial]
---

# Let's get started

This guide runs one full iteration of statistical sampling on a **prebuilt workload**, from downloading it to the
simulation result, and then shows how to iterate. Each step ends with a **Check** of what it should produce.

---

## Overview of the workflow

``` mermaid
graph TD
  W[Workload: prebuilt, or Boot + Load] --> I[Init Warm];
  I --> F[Functional Warming];
  F --> P[Partitioning];
  P --> RP[Run Partitions];
  RP --> R[Results];
  R --> C{Sampling error within the bound?};
  C -->|Yes| Fi[Finished];
  C ---->|No| T[Train the timekeeping model];
  T --> CleanPartition[Clean Up Partitions];
  CleanPartition --> F;
```

- **Workload** ([1](#1-download-a-prebuilt-workload)): a disk and a snapshot in which the workload already runs in
  steady state. Download a prebuilt one with `image pull`, or prepare your own with `boot` and `load` (see
  [Your own workload](own-workload.md)).
- **Init Warm** (`initialize`, [3](#3-init-warm)) warms the long-term microarchitectural state (caches, TLBs, branch
  predictors), once per experiment.
- **Functional Warming** (`fw`, [4](#4-functional-warming)) runs the workload over the population and takes a
  checkpoint per sample. It keeps the time of each vCPU with the **timekeeping model** (`cfg/core_info.csv`).
- **Partitioning** (`partition`, [5](#5-partitioning)) splits the checkpoints into groups that run in parallel.
- **Run Partitions** (`run-partition`, [6](#6-run-partitions)) simulates every checkpoint in detail with Flexus.
- **Results** (`result`, [7](#7-results)) computes the sampling error and the sample size that meets the bound. If
  the error is within the bound, the experiment is finished.
- **Train the timekeeping model** (`train`, [8](#8-next-iteration)) fits the timekeeping model to the detailed
  results, so that the next functional warming keeps time more accurately.
- **Clean Up Partitions** (`partition-cleanup`, [8](#8-next-iteration)) removes the partitions and checkpoints of
  the iteration; the next one starts again from Functional Warming, with the suggested sample size.

---

## 0) Before you begin

QFlex is built and `./qflex --help` works (see [Installation](installation.md)). Run every command from the
repository root, and outside the Nix shell if you built with Nix.

All the data lives in the **working folder** you created during the
[installation](installation.md#3-create-a-working-folder), `<WORK_DIR>` in the examples (`--working-folder`); with
Docker, it is the folder you mounted:

- `<WORK_DIR>/images/`: disk images, and the downloaded workloads in `images/prebuilt/<id>/`;
- `<WORK_DIR>/experiments/<experiment name>/`: one folder per experiment.

Images are large: to keep them on another disk, make `<WORK_DIR>/images` a symbolic link.

---

## 1) Download a prebuilt workload

A prebuilt workload is a disk (`root.qcow2`) and a snapshot `base` (`base.zstd`) in which the workload already
runs in steady state, plus a `config.json` describing the machine and the workload. IDs are
`<core count>/<workload>`, e.g. `2c/djd` or `16c/dc-ws`.

<!-- termynal -->
```bash

$ ./qflex image list
$ ./qflex image pull 2c/djd --working-folder <WORK_DIR>

```

- `image list` shows every workload with its size; with `--working-folder`, it also marks the downloaded ones.
- Without an ID, `image pull` lets you choose from a numbered list.
- An interrupted download resumes when you run `image pull` again.

**Check.** `<WORK_DIR>/images/prebuilt/2c/djd/` holds `root.qcow2`, `base.zstd` and `config.json` (read-only), and
`image pull` printed the machine: `cpu_cores=4 measured_core_count=2 memory=64G network_card=none`.

??? info "What is in `config.json`"
    ```json
    {
      "workload": "djd",
      "measured_core_count": 2,
      "cpu_cores": 4,
      "memory": "64G",
      "network_card": "none",
      "suggested_population_seconds": 10,
      "groups": [
        {"name": "djd", "cores": "0-1", "ipc": 2},
        {"name": "phantom", "cores": "2-3", "ipc": 4}
      ],
      ...
    }
    ```
    The machine settings must match the snapshot; the population and the [core groups](#core-groups) are
    defaults you can override.

---

## 2) Create the shared args file

Every command needs the same experiment options. Keep them in `./qflex.args`, **one argument per line**, and
pass them with `xargs`:

```text
--prebuilt-workload 2c/djd
--working-folder <WORK_DIR>
--experiment-name djd-2c-001
--quantum-size-ns 2000
--llc-size-per-tile-mb 2
--parallel
--no-unique
```

<!-- termynal -->
```bash

$ xargs -a ./qflex.args -- ./qflex <command> [command options]

```

!!! warning "Write full paths"
    `xargs` passes the lines as they are: it does not expand `~` or `$VARIABLES`, so write absolute paths.

- `--core-count`, `--double-cores`, `--memory-gb`, `--network` and `--workload-name` come from `config.json`.
  The snapshot only restores on the machine it was taken on, so qflex rejects values that do not match.
- `--population-seconds` defaults to the workload's `suggested_population_seconds`; set it lower (e.g. `1`) for a
  quick test.
- `--groups` defaults to the workload's groups (see [Core groups](#core-groups)).
- `--no-unique` only silences a deprecation message: the experiment folder is always named `--experiment-name`.

### Core groups

The vCPUs are split into groups, given as `name:cores[:ipc]`:
```text
--groups "dc:0-7:0.6,ws:8-15:4.2,phantom:16-31:4"
```

- Each measured group runs one workload; more than one group makes a **consolidated** workload.
- With `--double-cores`, the `phantom` group holds the client vCPUs `N…2N-1` of an `N`-core experiment.
- The IPC is the starting estimate of the timekeeping model, `cfg/core_info.csv` (default 2, phantom 4), until
  [`train`](#8-next-iteration) fits the model to the detailed results.
- Without `--groups` (and without a prebuilt workload), all cores form one group.
- `result` and `train` use the measured groups as their default `--core-groups`.

---

## 3) Init Warm

<!-- termynal -->
```bash

$ xargs -a ./qflex.args -- ./qflex initialize

```

The first command of an experiment sets it up from the prebuilt workload: it copies `root.qcow2` into the
experiment's `run/` folder, links `base.zstd`, and records the workload in `run/prebuilt.json`. Later commands use
this record, so they keep working even without `--prebuilt-workload`. `--loadvm-name` defaults to `base`.

Init Warm then generates the configuration, builds the experiment's WormCache plugin (with `cargo`, see
[Installation](installation.md)), and runs the workload until the long-term state is warm. It saves the result as
the snapshot **`init_warmed`**.

**Check.** In `<WORK_DIR>/experiments/djd-2c-001/`:

- `run/` has `root.qcow2`, `base.zstd` (a link), `prebuilt.json`, and at the end `init_warmed.*`
  (`init_warmed.bxdb`, `.state.zstd`, `.uarch`);
- `cfg/` has `parameter.rs`, `flexus_configuration.json`, `timing.cfg` and `core_info.csv`, with one row per vCPU
  (for `2c/djd`, vCPUs 0–1 at ipns 4.0 and the phantom vCPUs 2–3 at 8.0, i.e. IPC × 2 GHz);
- `lib/` has `libknottykraken.so`, `libsemikraken.so` and `libworm_cache.so`; `bin/` has `checkpoint_conversion`
  and `bxdb`.

!!! info "Once per experiment"
    Init Warm only runs once; every iteration below starts again from `init_warmed`.

---

## 4) Functional Warming

<!-- termynal -->
```bash

$ xargs -a ./qflex.args -- ./qflex fw --loadvm-name init_warmed --sample-size 30

```

Functional warming runs the workload from `init_warmed` over the population and takes `--sample-size` checkpoints,
evenly spaced, keeping the microarchitectural state warm in between.

**Check.** `run/` has `snapshot_0` … `snapshot_29` (`.bxdb-meta`, `.state.zstd` and `.uarch` each).

---

## 5) Partitioning

<!-- termynal -->
```bash

$ xargs -a ./qflex.args -- ./qflex partition --partition-count 16

```

The checkpoints are split into partitions that are simulated in parallel. All partitions run at the same time, so
choose at most the number of host cores you can spare.

**Check.** `run/partition_0` … `run/partition_15`, each with its share of the checkpoints and a `run_flexus.sh`.

To partition differently, `unpartition` moves the checkpoints back to `run/` and removes the partitions (add
`--force` if they already hold simulation results, which it deletes):
<!-- termynal -->
```bash

$ xargs -a ./qflex.args -- ./qflex unpartition

```

---

## 6) Run Partitions

To run detailed simulation with 200-us detailed warming and 100-us measurement:

<!-- termynal -->
```bash

$ xargs -a ./qflex.args -- ./qflex run-partition --warming-ratio 2 --measurement-ratio 1

```

`--warming-ratio` and `--measurement-ratio` are in units of **100 µs** of simulated time: `1` is 100 µs, `2` is
200 µs. Flexus counts time on the uncore clock (1 GHz, while the cores run at 2 GHz), and one unit is 100,000
uncore cycles. Each checkpoint is simulated for the warming, then measured; the statistics are recorded every
100 µs.

qflex first builds the index of the checkpoint database (`run/init_warmed.bxdb/index.bxdb`). Then each partition
simulates its checkpoints one after the other: it converts the checkpoint to Flexus' format
(`snapshot_<n>-flexus/`), runs the detailed simulation, and moves the logs to `result_<n>/`.

**Check.** The output of each partition goes to `run/partition_<i>/log` and `err`, not to the terminal:
<!-- termynal -->
```bash

$ tail -f <WORK_DIR>/experiments/djd-2c-001/run/partition_0/log
$ ls -d <WORK_DIR>/experiments/djd-2c-001/run/partition_*/result_* | wc -l

```
The second command counts the simulated checkpoints; the step is done when it reaches the sample size.

To stop the run, press **Ctrl+C** or kill `./qflex`: every partition and its QEMU stop with it.

!!! tip "Debugging one partition"
    Run a partition by itself, with the same ratios, to see its output directly:
    ```bash
    cd <WORK_DIR>/experiments/djd-2c-001/run/partition_0 && ./run_flexus.sh 2 1
    ```

---

## 7) Results

<!-- termynal -->
```bash

$ xargs -a ./qflex.args -- ./qflex result

```

`result` collects every measurement into `timing.csv` and computes the sampling error of each core group (the
measured [groups](#core-groups) by default, or `--core-groups "0-7,8-15"`).

**Check.** The experiment folder has `timing.csv` and `NEXT_SAMPLE_SIZE`, the sample size that meets the error
bound (`--error`, default 5% at `--confidence` 95%). The exit code tells whether this iteration is enough:

- `0`: the bound is met, the experiment is **finished**;
- `255`: the bound is not met yet, run the next iteration. This is not a failure.

`result` does not modify `cfg/core_info.csv`; `train` does.

!!! note "Other warming or measurement lengths"
    By default, `result` analyzes the third 100 µs interval (`--index 2`), the measurement right after
    `--warming-ratio 2`. With other lengths, point it at the measurement: e.g. after `--warming-ratio 4
    --measurement-ratio 2`, use `result --unit-size 2 --index 2` (units of 200 µs: the warming is units 0–1), and
    `train --interval-index 4 --cycle-range 400000-600000`.

---

## 8) Next iteration

Functional warming keeps the time of each vCPU with the timekeeping model, `cfg/core_info.csv`. The first iteration
used a constant IPC per group (the estimates of the [core groups](#core-groups)); `train` fits an IPC model per core
group to the detailed results in `timing.csv`, which makes the next functional warming more accurate. Then clean up
and warm again with the suggested sample size:

<!-- termynal -->
```bash

$ xargs -a ./qflex.args -- ./qflex train
$ xargs -a ./qflex.args -- ./qflex partition-cleanup
$ xargs -a ./qflex.args -- ./qflex fw --loadvm-name init_warmed --sample-size $(cat <WORK_DIR>/experiments/djd-2c-001/NEXT_SAMPLE_SIZE)
$ xargs -a ./qflex.args -- ./qflex partition --partition-count 16
$ xargs -a ./qflex.args -- ./qflex run-partition --warming-ratio 2 --measurement-ratio 1
$ xargs -a ./qflex.args -- ./qflex result

```

Repeat until `result` exits with `0`.

- `train` replaces `cfg/core_info.csv` (the previous one is kept as `core_info.csv.<timestamp>`) and writes the
  per-ASID models to `run/asid_info.csv`.
- `partition-cleanup` removes the partitions and the checkpoints, and drops them from the checkpoint database,
  keeping `init_warmed`. Collect anything you need from the partitions first.

??? info "Format of `core_info.csv`"
    One row per vCPU:
    ```
    host_core_idx,model_type,ipns,bx_private_icache_miss_coeff,bx_private_dcache_miss_coeff,bx_shared_cache_miss_coeff,bx_bp_miss_coeff,bx_drain_store_buffer_coeff,bx_instruction_coeff
    0,constant,4.0,0.0,0.0,0.0,0.0,0.0,0.0
    ```
    - `host_core_idx`: the host core the vCPU thread is pinned to.
    - `model_type`: `constant` uses a fixed `ipns` (instructions per ns); `ipc-model` requires `ipns=1.0` and
      estimates the time from the coefficients (ns per event).

---

## The experiment folder

| Path | Content |
|------|---------|
| `cfg/` | Generated configuration: `parameter.rs` (WormCache), `flexus_configuration.json`, `timing.cfg`, `core_info.csv` |
| `lib/` | Flexus simulators and the WormCache plugin; `lib/WormCache/` is the copy it was built from |
| `bin/` | `checkpoint_conversion` and `bxdb` |
| `run/` | Disk, snapshots, checkpoints and partitions |
| `timing.csv`, `NEXT_SAMPLE_SIZE` | Output of `result` |

!!! note "Disk use"
    Each experiment has its own copy of `root.qcow2` and its own WormCache build (`lib/WormCache/target`, about
    700 MB). WormCache is built per experiment because its cache configuration (`cfg/parameter.rs`) is compiled
    into the plugin.

---

## Troubleshooting

??? warning "Common issues"
    **`Warning: .../run/prebuilt.json differs from .../config.json`**
    The experiment was set up before the workload's `config.json` changed (e.g. after a new `image pull`). The
    experiment keeps its own copy; use a new `--experiment-name` to start from the new `config.json`.

    **`Error: functional warming needs --population-seconds`**
    The workload's `config.json` has no `suggested_population_seconds` (or the experiment was set up from an older
    one, see above): pass `--population-seconds`.

    **`... prebuilt 64, experiment 32 ... These settings are read from the workload's config.json`**
    Remove `--core-count`, `--double-cores`, `--memory-gb` and `--network` from `qflex.args`.

    **`run/root.qcow2 already exists`**
    The experiment name is taken by an experiment set up from another image; choose a new `--experiment-name`.

    **An experiment was created with an older qflex**
    The helper scripts (`run_partitions.sh`, `result.py`, ...) are updated by every command, but `run_flexus.sh`
    is generated by Init Warm. Regenerate it, including the copies in the partitions:
    ```bash
    xargs -a ./qflex.args -- ./qflex refresh-scripts
    ```
    The configuration in `cfg/` is not regenerated: it is built into the WormCache plugin and the checkpoints, so
    changing it needs a new experiment.

    **`Error: partitions of ... are still running`**
    `unpartition`, `partition-cleanup` and `refresh-scripts` refuse to change partitions while a simulation runs in
    them. Stop `run-partition` first.

    **Paths with `~` or `$VAR` are not found**
    `xargs` does not expand them; write absolute paths in `qflex.args`.

---

## Bring your own workload

To simulate a workload that is not prebuilt, create the image and the snapshot yourself with
`create-base-image`, `boot` and `load`, then continue with [Init Warm](#3-init-warm). See
[Your own workload](own-workload.md).
