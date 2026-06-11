# qflex CLI Guide for gem5 Conversion and Timing

## Scope

This guide describes the current user-facing flow for running the project
through `qflex`, including the gem5 conversion and timing path.

The intended entrypoint is `qflex`. QPoints is part of the implementation
stack, but users should normally work through the top-level CLI.

## Known tested baseline

The CLI work in this project phase was tested primarily against the following
lab baseline:

- image folder:
  - `/mnt/sdb/aansari/ws-image/web-search-fresh-8c`
- experiment folder:
  - `/mnt/sdb/aansari/experiments/ws-image-fresh-8c`
- checkpoint folder:
  - `/mnt/sdb/aansari/checkpoints/ws-image-fresh-8c`

These paths are accessible from `iccluster118`.

This is not a hardcoded project requirement. It is the concrete baseline that
was used to validate the maintained workflow while the CLI and conversion path
were being developed.

## Terms used in this guide

- **sample**: a collection of sampling units that together satisfy the
  statistical sampling methodology for a workload
- **snapshot**: a QEMU snapshot such as `snapshot_0`
- **checkpoint**: a snapshot plus the recorded microarchitectural state from
  parallel functional warming
- **sampling unit**: a checkpoint when it is actively simulated in timing for
  detailed warmup and measurement

## Configuration boundary: args file vs sim-config vs direct CLI

This boundary is important. Without a policy, parameters spread across too many
layers and become hard to reason about.

### 1. Args file: persistent experiment context

Use the shared args file for experiment-level context that should stay stable
across the main qflex stages.

This includes the `ExperimentContext` fields exposed by the CLI, such as:

- core count
- memory size
- workload name
- host name
- image name
- image folder
- mounting folder
- experiment name
- bootloader
- root device
- network mode
- quantum size
- `loadvm_name` when a command needs to load a specific QEMU snapshot

Examples from the current typed args model:

- `--core-count`
- `--memory-gb`
- `--bootloader`
- `--root-device`
- `--image-folder`
- `--image-name`
- `--mounting-folder`
- `--experiment-name`
- `--loadvm-name`

This is the right place for parameters that define the experiment itself rather
than one particular invocation.

One important exception is `loadvm_name`. It is part of the shared experiment
context model, but in practice it is often stage-specific. Users should be
careful not to leave a stale `--loadvm-name` in a shared args file when moving
between `load`, `initialize`, and later stages.

### 2. Sim-config: gem5 model/runtime configuration

Use `--sim-config` for gem5-side model parameters that affect how gem5 should
interpret and run the checkpoint.

In the current implementation, the sim-config is consumed by the gem5-facing
conversion and runtime path. It is used to resolve things such as:

- cache geometry
  - `--l1i_size`
  - `--l1i_assoc`
  - `--l1d_size`
  - `--l1d_assoc`
  - `--l2_size`
  - `--l2_assoc`
- shared-cache and directory topology
  - `--num-l2caches`
  - `--num-dirs`
  - `--topology`
  - `--mesh-rows`
- memory-system geometry
  - `--mem-type`
  - `--mem-channels`
  - `--mem-ranks`
- MMU contract
  - `--itb-size`
  - `--dtb-size`
  - `--have-large-asid-64`
  - `--no-large-asid-64`
- restore-related runtime controls
  - `--restore-btb-state`
  - `--restore-tage-state`
  - `--restore-tlb-state`
- core/frontend timing parameters
  - `--fdip`
  - `--ftqSize`
  - `--btb-entries`
  - `--btb-ways`
  - width and queue-size parameters such as:
    - `--fetch-width`
    - `--decode-width`
    - `--rename-width`
    - `--dispatch-width`
    - `--issue-width`
    - `--wb-width`
    - `--commit-width`
    - `--fetch-queue-size`
    - `--rob-entries`
    - `--sq-entries`

This layer is for gem5 configuration, not general experiment context.

Why it matters:
- conversion uses the sim-config to interpret the checkpoint correctly
- runtime uses the sim-config to run gem5 with the intended machine/model
  contract
- if conversion and timing use mismatched sim-config files, the staged
  checkpoint may no longer match the intended runtime configuration

Reference example:
- [timing_ruby_moesi_ws_flexus_mesh_ref_8c.args](/home/dev/qflex_git/QPoints/configs/timing_ruby_moesi_ws_flexus_mesh_ref_8c.args)

Representative excerpt:

```text
--l1i_size=64kB
--l1i_assoc=8
--l1d_size=64kB
--l1d_assoc=8
--l2_size=1MB
--l2_assoc=16
--num-l2caches=8
--num-dirs=8
--topology=Mesh_XY
--mesh-rows=2
--mem-type=DDR4_2400_8x8
--mem-channels=1
--mem-ranks=4
--itb-size=64
--dtb-size=64
--have-large-asid-64
--restore-btb-state
--restore-tage-state
--restore-tlb-state
```

How to think about it:
- args file: “what experiment/workload am I running?”
- sim-config: “what gem5 machine/model should consume and simulate this
  checkpoint?”
- direct CLI flags: “what should this command do right now?”

### 3. Direct CLI flags: invocation-scoped behavior

Use direct CLI flags for run-specific controls that should not silently become
part of the shared experiment context.

Examples:

- `--first`
- `--last`
- `--warmup-cycles`
- `--measurement-cycles`
- `--timing-engine`
- `--timing-ruby`
- `--timing-ruby-moesi`
- `--cleanup-conversion-artifacts`
- tracing flags
- `--collect-gem5-bbl-btb`
- `--emit-gem`

These flags describe what this invocation should do now. They are not the
right place to encode the experiment’s persistent identity.

## Recommended workflow

The current high-level flow is:

1. create a base image
2. boot the image and capture the `boot` snapshot
3. load the image from a booted snapshot and bring the workload up
4. initialize long-term microarchitectural state
5. run functional warming to create QEMU snapshots and associated checkpoint
   source state
6. run timing simulation on selected sampling units

For gem5, the timing side can use:

- one-off conversion and one-off runs
- or `run_sample` across a snapshot range, where each selected snapshot is
  turned into a checkpoint and then simulated as a sampling unit

## Shared args file

The CLI supports `--args-file`. The file is expanded by qflex before command
dispatch. Lines are parsed as argument fragments, and comments are ignored.

Use it to avoid repeating the shared experiment context.

Current design limitation:
- the current args file is convenient, but it is not well structured
- it bundles many parameters from different workflow layers into one flat list
- in practice, each qflex CLI command looks for the parameters it cares about
  and ignores the rest
- that works, but it is not a clean long-term contract for configuration
  ownership

Future work:
- add clearer structure to the args-file model
- make the boundary between command consumers more explicit
- ensure each command consumes a sound and direct configuration surface rather
  than depending on a large shared flat argument bundle

Typical contents include:

```text
--core-count 8
--double-cores
--quantum-size-ns 2000
--llc-size-per-tile-mb 2
--parallel
--network user
--memory-gb 32
--host-name ZEN3
--workload-name web-search
--population-seconds 5
--no-consolidated
--primary-ipc 2.0
--primary-core-start 0
--phantom-cpu-ipc 4
--experiment-name ws-image-fresh-8c
--image-name root.qcow2
--image-folder <MOUNT_DIR>/images
--mounting-folder <MOUNT_DIR>
--bootloader <BOOTLOADER>
--root-device /dev/vda
--refresh-wormcache
--no-unique
--use-image-directly
```

One shared flag deserves explicit explanation:

- `--refresh-wormcache`
  - this tells qflex to recopy the repository `WormCacheQFlex` tree into the
    experiment-local folder before rebuilding the experiment-local plugin
  - this matters because several commands build and use an experiment-local
    copy under `lib/WormCacheQFlex`, not the repo tree in place
  - use this when you want the experiment-local copy to pick up newer source
    changes from the repository
  - leave it unset when you want to keep reusing the existing experiment-local
    copy

## Step-by-step command guide

### 1. `qflex create-base-image`

Purpose:
- create a base qcow2 image for later experiment use

Current caveat:
- this path is not part of the currently working baseline workflow
- in practice, the project started from an ad hoc baseline image already
  available in the lab because `create-base-image` did not provide a trusted
  starting point
- users should treat it as an unverified utility, not as a trusted reference
  path

Current baseline-image note:
- the lab baseline image used during this project exposes `qflex` as both the
  VM user name and password
- this is why the literal `qflex` guest credentials may appear in scripts and
  workflow assumptions
- that should be understood as a property of the ad hoc starting image, not as
  a general project requirement

Future work:
- test and verify `create-base-image`
- make sure the project has a valid and documented initial baseline image for
  users who are not starting from the current lab-local setup
- ensure the workflow can handle alternative user-provided images correctly
- decide whether it should remain part of the project surface or be retired if
  it does not justify continued maintenance

Typical use:

```bash
./qflex create-base-image --image-folder <IMAGE_DIR>
```

### 2. `qflex boot`

Purpose:
- boot the VM
- wait for guest reachability
- capture kernel provenance
- create the QEMU snapshot named `boot`
- shut down cleanly

Important notes:
- this flow has changed significantly relative to the older main-branch boot
  behavior
- the boot screen is not exposed as a primary manual step in the maintained
  workflow
- the boot process runs in the background and reports progress to the user
  step by step
- the user does not need to manually save the VM state or create the `boot`
  snapshot themselves
- this is the first place where kernel provenance is captured into the
  experiment-local machine contract
- automated boot currently assumes guest SSH over `--network user`

Important caveat:
- if the user does not want to expose guest networking and SSH, that breaks the
  current assumptions of the maintained boot step and must be handled
  separately
- automatic extraction of the matching kernel ELF is still incomplete
- if boot requires user action or if the lineage cannot be handled
  automatically, the user may still need to provide the kernel manually later

If the lineage ends in an incomplete kernel bundle, the current manual repair
path is:

```bash
xargs -a ./qflex.args -- ./qflex kernel adopt --kernel <VMLINUX_ELF>
```

This installs the externally sourced matching kernel ELF into the experiment
kernel bundle and marks it ready for downstream conversion and timing stages.

Typical use:

```bash
xargs -a ./qflex.args -- ./qflex boot
```

### 3. `qflex load`

Purpose:
- restore a QEMU snapshot so the workload can be brought into the desired
  application state before initialization or warming

Important role in the workflow:
- unlike `boot`, this is not a bounded automation stage that creates the next
  snapshot for you
- `load` starts a live QEMU session from the selected snapshot
- the user is expected to drive the workload to the desired application state
  inside that session
- if the next stage should resume from a new baseline snapshot, the user must
  create that snapshot explicitly from the live QEMU session

Naming note:
- `loaded` is only a conventional example name for that user-created
  post-load snapshot
- qflex does not require that exact name
- any snapshot name can be used, as long as later stages refer to the same
  name consistently

Typical use:

```bash
xargs -a ./qflex.args -- ./qflex load --loadvm-name boot
```

In practice, this is one of the commands where overriding `--loadvm-name`
directly on the invocation is often clearer than keeping it permanently in the
shared args file.

### 4. `qflex initialize`

Purpose:
- initialize long-term microarchitectural state before functional warming

What it is for:
- cache state
- predictor state
- TLB-related state on the BXKraken side
- creation of the `init_warmed` snapshot that later lineages normally resume
  from

Typical use:

```bash
xargs -a ./qflex.args -- ./qflex initialize --loadvm-name loaded
```

Here, `loaded` is only an example of a user-created snapshot name from the
prior live `load` session. Replace it with whatever snapshot name you actually
created before starting `initialize`.

Important caveat:
- the normal stopping condition of `initialize` is not just “run for a fixed
  amount of time”
- this phase is intended to emit `init_warmed` once the plugin-side LLC warmup
  condition is satisfied
- some workloads may not satisfy that condition quickly, especially when they
  generate limited LLC activity
- without a fallback, that can leave `initialize` running much longer than the
  user intends

Fallback control:
- `--fallback-cycles <N>`
  - this sets a cycle threshold that forces `init_warmed` snapshot creation if
    the LLC warmup-complete condition has still not been reached
  - use it when a workload shows limited LLC activity, or when you want a
    bounded `initialize` phase instead of waiting indefinitely for the
    LLC-driven stop condition
  - this is a practical escape hatch, not the preferred steady-state path; if
    the workload naturally reaches the LLC warmup condition, no fallback is
    needed

- this command can regenerate configuration state unless `--skip-generate-cfg`
  is used
- the regenerated configuration here means experiment-local generated files such
  as:
  - `cfg/parameter.rs`
  - `cfg/flexus_configuration.json`
  - `cfg/timing.cfg`
  - `scripts/run_flexus.sh`
- those files are derived from the current experiment context and are used by
  the experiment-local WormCache/Flexus/timing flow
- this matters for advanced diagnostics such as custom TLB source geometry or
  any hand-edited experiment-local plugin configuration
- if you are running a focused diagnostic with non-default source-side
  settings, regeneration can overwrite those local edits

When `--skip-generate-cfg` is useful:
- when you intentionally prepared custom experiment-local generated files and
  want `initialize` to use them as-is
- when you do not want the current experiment context to rewrite those files

Experiment-local WormCache rebuild behavior:
- `initialize` also rebuilds the experiment-local WormCache plugin and
  associated helper binaries from the experiment-local source tree
- by default, that rebuild reuses the existing experiment-local
  `lib/WormCacheQFlex` copy
- if `--refresh-wormcache` is present in the shared experiment context, qflex
  first recopies the repository `WormCacheQFlex` tree into the experiment
  folder and then rebuilds from that refreshed copy
- use `--refresh-wormcache` when you want the experiment-local build to pick up
  newer repository changes

Gem5 frontend-export option:
- `initialize` also supports `--collect-gem5-bbl-btb`
- when enabled, `init_warmed` records gem5 BBL-BTB export state so later
  functional warming can continue from that frontend-export baseline
- this is optional; users can decide whether to include BBL-BTB continuity in
  the `initialize -> fw` lineage

Future direction:
- a later cleanup pass may want to make gem5 BBL-BTB simulation/export the
  default BXKraken behavior
- if that happens, qflex would no longer need to expose as much user-facing
  control over whether BBL-BTB is simulated and checkpointed
- that would simplify both the CLI surface and the lineage-tracking story for
  frontend export continuity

### 5. `qflex test-worm`

Purpose:
- run a controlled QEMU + BXKraken session in normal mode without generating
  snapshots
- provide a focused environment for frontend and branch-prediction debugging
- support targeted instrumentation before committing to a full warming run

What it is for:
- branch-stream debugging
- TAGE decision tracing
- gem5-oriented BBL-BTB export debugging
- controlled experiments where you want plugin activity without snapshot
  generation

What it consumes from the shared experiment context:
- the normal experiment-local VM/image and machine context
- the experiment-local generated configuration, unless
  `--skip-generate-cfg` is used
- the same experiment-local WormCache build path used by the other BXKraken
  stages

Direct parameters:
- `--skip-generate-cfg`
  - reuse the existing experiment-local generated configuration instead of
    regenerating it before the run
  - useful when you intentionally prepared custom local config files
- `--branch-trace`
  - enable per-core branch-trace logging
- `--tage-decision-trace`
  - enable compact per-conditional TAGE decision logging
- `--tage-decision-trace-limit <N>`
  - optional upper bound on the number of conditional TAGE decisions logged per
    core
  - only valid together with `--tage-decision-trace`
- `--collect-gem5-bbl-btb`
  - enable collection of the export-only gem5-oriented BBL-BTB view during the
    test run
  - useful when validating frontend export behavior without creating snapshots
- `--monitor-port <PORT>`
  - optional QEMU monitor port for graceful stop or debugging

Typical use:

```bash
xargs -a ./qflex.args -- ./qflex test-worm \
  --branch-trace \
  --tage-decision-trace \
  --tage-decision-trace-limit 1000
```

When to use it instead of `fw`:
- when you want instrumentation without snapshot generation
- when you want to debug frontend behavior in a shorter, more controlled run
- when you want to inspect branch or TAGE behavior before committing to a full
  lineage-generating warmup

### 6. `qflex fw`

Purpose:
- perform functional warming
- generate QEMU snapshots and associated checkpoint source state for later
  timing simulation

Current important options:
- `--sample-size`
- `--collect-gem5-bbl-btb`
- `--emit-gem`

What `.gem` means:
- `.gem` is the gem5-facing architectural checkpoint bundle emitted from the
  warming side for a given snapshot
- in practice, it is the large architectural-state dump that gem5 conversion
  consumes later for disk, memory, and register/checkpoint reconstruction
- the current gem5 checkpoint flow does not use incremental checkpoint storage
  for these `.gem` artifacts, so each emitted `.gem` can consume substantial
  disk space

What it produces:
- QEMU snapshots such as `snapshot_0`, `snapshot_1`, ...
- side artifacts such as:
  - `snapshot_X.loc`
  - `snapshot_X.state.zstd`
  - `snapshot_X.uarch/`
- optionally:
  - `snapshot_X.gem/`

Typical use:

```bash
xargs -a ./qflex.args -- ./qflex fw \
  --sample-size 2 \
  --collect-gem5-bbl-btb
```

Important current behavior:
- `.gem` emission is optional
- if `--emit-gem` is used during `fw`, each snapshot gets its `.gem` bundle at
  snapshot-generation time
- if `--emit-gem` is omitted, `convert-single` can materialize the missing
  `.gem` lazily later when conversion actually needs it
- `fw` also supports `--collect-gem5-bbl-btb`
- if `fw` resumes from `init_warmed`, the current implementation compares the
  `initialize` and `fw` BBL-BTB collection settings
- if they differ, `fw` prints a warning but continues
- the warning is symmetric:
  - if `initialize` collected BBL-BTB and `fw` does not, later snapshots stop
    extending that export from the `fw` phase onward
  - if `initialize` did not collect BBL-BTB and `fw` does, later snapshots
    start BBL-BTB export only from the `fw` phase onward

How to choose `--emit-gem`:
- use `--emit-gem` during `fw` if disk space is available and you want later
  gem5 conversion to be faster
- omit `--emit-gem` if disk space is tight and you prefer to pay the
  architectural-dump cost only for the snapshots that are actually converted
- this tradeoff exists because `.gem` is large, but lazy creation lets the
  workflow avoid paying that space cost for every snapshot up front

How cleanup fits:
- if `.gem` is created lazily during conversion, the gem5 `run_sample` path can
  later delete it again as part of conversion-artifact cleanup
- use `--cleanup-conversion-artifacts` on gem5 `run_sample` when you want the
  CLI to reclaim that disk space after each successful timing run
- use `--no-cleanup-conversion-artifacts` when you prefer to keep the `.gem`
  and converted checkpoint artifacts for reuse or inspection

Current lineage caveat:
- it is possible to continue warming from an existing snapshot, for example by
  starting from the current last snapshot in a lineage
- however, when snapshots are extended from an existing `snapshot_N` in the
  current implementation, the new snapshots do not keep the incremental
  snapshot format
- instead, the resumed path can produce large `snapshot_X.mem` artifacts, which
  consume significant disk space
- if the goal is to preserve the incremental snapshot layout, the reliable
  starting point is currently `init_warmed`

Future work:
- users should be able to continue an existing lineage while still producing
  incremental snapshots
- fixing that would avoid the time and disk cost of restarting lineage
  generation from `init_warmed` just to preserve the better snapshot format

### 7. `qflex qpoints convert-single`

Purpose:
- convert one checkpoint source snapshot into a gem5-consumable checkpoint

What it does now:
- checks whether the `.gem` producer bundle is ready
- checks whether the gem5 checkpoint is already ready for the requested
  machine/model contract
- lazily regenerates `.gem` if needed
- prepares gem5-side microarchitectural restore artifacts

What it consumes from the shared experiment context:
- `--core-count`
  - used as the checkpoint machine-core count
- `--memory-gb`
  - used as the checkpoint machine-memory size
- `--kernel`
  - used for gem5 restore and TLB sidecar generation
- `--bootloader`
  - used for gem5 full-system restore runs
- `--root-device`
  - used for gem5 full-system restore runs
- `--mounting-folder` and `--experiment-name`
  - used to resolve the default qflex experiment root and the default gem5
    checkpoint root
- `--image-folder`, `--image-name`, `--use-image-directly`
  - used to resolve the default base qcow2 image when `--base` is not
    provided

Required direct argument:
- `--snapshot <snapshot_X>`
  - required
  - names the source snapshot to convert
  - use the exact QEMU snapshot name, for example `snapshot_0`

Primary conversion-contract arguments:
- `--ruby-protocol {mesi_two_level|moesi_cmp_directory}`
  - selects the requested primary Ruby protocol contract for the conversion
  - this is not just a folder-label choice
  - it determines:
    - which protocol is treated as the required ready target
    - which protocol-specific sim-config defaults are interpreted
    - which staged manifest is validated for readiness and LLC slice-count
      compatibility
- the conversion flow may still stage auxiliary artifacts for the other
  protocol, but this flag tells qflex which protocol is the requested target
- current guidance:
  - use `moesi_cmp_directory` for the intended non-inclusive workflow
  - use `mesi_two_level` only for legacy comparison or focused diagnostics

Naming caveat:
- the current CLI uses `--ruby-protocol` on conversion commands, but
  `--timing-ruby` / `--timing-ruby-moesi` on timing commands
- these parameters are related, but they live at different workflow layers:
  - `--ruby-protocol` selects the conversion-side protocol contract
  - `--timing-ruby*` selects the runtime timing protocol
- this naming split is a historical artifact of how the CLI evolved across
  different project phases
- future work should consolidate these protocol-selection flags into a more
  uniform naming scheme to avoid user confusion and interface drift

- `--sim-config <path>`
  - optional in syntax, but important in practice
  - points to the QPoints/gem5 args file that defines gem5-side machine/model
    parameters needed during conversion
  - this affects at least:
    - TLB geometry
    - ASID mode
    - LLC slice count
  - use the same sim-config that you expect to use later for timing simulation
  - if conversion and timing use mismatched sim-configs, readiness may fail or
    the staged checkpoint may not match the intended runtime contract

Location-control arguments:
- `--gem5-ckp-dir <path>`
  - optional
  - overrides the default gem5 checkpoint root
  - default is:
    - `<mounting-folder>/checkpoints/<experiment-name>`
  - use this only when you want the converted checkpoints to live somewhere
    other than the normal experiment-aligned checkpoint root

- `--base <path-to-qcow2>`
  - optional
  - overrides the base disk image used during conversion
  - default is the image resolved from the experiment context
  - override this only when the experiment context does not point at the right
    qcow2 image or when you are intentionally converting against a different
    disk baseline

VM connectivity arguments:
- `--ssh-host`
  - default: `127.0.0.1`
  - host used for the guest SSH path during conversion
- `--ssh-user`
  - default: `qflex`
  - guest user for the conversion-side SSH path
- `--monitor-base`
  - default: `45454`
  - base port for QEMU monitor access
- `--qmp-base`
  - default: `4444`
  - base port for QMP
- `--ssh-base`
  - default: `2222`
  - base port for guest SSH forwarding

Use the connectivity arguments only when the default local port layout or
guest-user assumption does not match your environment.

Rewrite-control argument:
- `--overwrite`
  - default: disabled
  - forces regeneration of the checkpoint-ready layer even if an apparently
    ready conversion already exists
  - use this when:
    - you intentionally changed the conversion inputs
    - you want to discard a prior converted checkpoint
    - you suspect the existing converted state is stale
  - without `--overwrite`, `convert-single` is allowed to return early when the
    checkpoint is already ready for the requested contract

Typical use:

```bash
xargs -a ./qflex.args -- ./qflex qpoints convert-single \
  --snapshot snapshot_0 \
  --ruby-protocol moesi_cmp_directory \
  --sim-config /home/dev/qflex_git/QPoints/configs/timing_ruby_moesi_ws_flexus_mesh_ref_8c.args
```

Important note:
- users do not need to pre-run this when using gem5 `run_sample`
- the gem5 `run_sample` path can call `convert_single` automatically
- `convert-single` is most useful for:
  - one-off debugging
  - validating conversion for a single snapshot
  - preparing a checkpoint before a manual `run-gem5`

### 8. `qflex qpoints convert-multi`

Purpose:
- convert a snapshot range in parallel

This command is not recommended for normal use at the moment.

Unlike `convert-single`, this command does not consume the shared
`ExperimentContext`. It expects many parameters directly on the invocation,
because it operates more like a low-level batch utility than a normal
top-level workflow command.

Current status:
- treat this as a stale utility, not as the preferred conversion path
- current testing shows that on HDD-backed setups, the non-sequential disk
  writes from several concurrent checkpoint conversions can make total
  conversion time significantly worse than the serial `convert-single` path
- the supported and recommended path is:
  - `convert-single` for one-off work
  - gem5 `run_sample`, which drives per-snapshot `convert_single`
    serially

Future work:
- `convert-multi` should be revisited only after further development and
  testing establish that parallel conversion is actually beneficial on the
  storage configurations we care about

### 9. `qflex qpoints run-gem5`

Purpose:
- run gem5 on one converted checkpoint

Main modes:
- instruction-bounded runs with `--inst`
- cycle-window runs with:
  - `--warmup-cycles`
  - `--measurement-cycles`

Protocol options:
- `--timing-ruby` for the MESI timing-Ruby path
- `--timing-ruby-moesi` for the MOESI timing-Ruby path

Current guidance:
- `--timing-ruby-moesi` is the current intended path for BXKraken-origin
  checkpoints
- this path matches the project's non-inclusive shared-cache direction more
  closely than the older MESI path
- `--timing-ruby` remains useful for legacy comparisons, bring-up, or focused
  diagnostics, but it is not the preferred protocol for the current
  checkpoint-conversion workflow

Why this matters:
- the selected protocol determines which gem5 coherence model is used during
  timing simulation
- conversion staging and readiness checks are protocol-specific
- the MOESI path is the one tied to the current sliced-restore and LLC
  slice-count contract

Typical use:

```bash
xargs -a ./qflex.args -- ./qflex qpoints run-gem5 \
  --snapshot snapshot_0 \
  --warmup-cycles 200000 \
  --measurement-cycles 1000000 \
  --timing-ruby-moesi \
  --sim-config /home/dev/qflex_git/QPoints/configs/timing_ruby_moesi_ws_flexus_mesh_ref_8c.args
```

### 10. `qflex run_sample`

Purpose:
- select a snapshot range, restore each selected checkpoint,
  simulate it as a sampling unit, and emit an aggregate report

Why this is the primary user-facing timing command:
- from a user’s point of view, the goal is to run a sample
- `run_sample` is the consolidated command for that goal
- for gem5, it already owns the intended workflow
- for Flexus, the older partitioned path still exists, but `run_sample` now
  consolidates the normal user-facing timing flow under one command

Legacy note:
- `partition`, `run_partition`, and `result` are still present
- they reflect the older Flexus-oriented path
- they should be treated as legacy workflow commands from a user-facing point
  of view
- future work may retire that legacy path and keep `run_sample` as the primary
  timing command for both engines

Supported timing engines:
- `--timing-engine gem5`
- `--timing-engine flexus`

What `run_sample` consumes from the shared experiment context:
- `--experiment-name`
  - used for result naming and output placement
- `--mounting-folder`
  - used to resolve experiment and default checkpoint locations
- `--core-count`
  - used by both gem5 and Flexus timing paths
- `--memory-gb`
  - used by the gem5 path
- `--bootloader`
  - used by the gem5 path
- `--root-device`
  - used by the gem5 path
- the experiment-local image path
  - used by the gem5 path when it needs conversion

Required direct arguments:
- `--first <snapshot_X>`
  - required in practice
  - first snapshot in the range
- `--last <snapshot_Y>`
  - required in practice
  - last snapshot in the range
- use exact QEMU snapshot names such as `snapshot_0`, `snapshot_1`

Engine-selection argument:
- `--timing-engine {gem5|flexus}`
  - selects which timing backend will consume the snapshot range
  - current guidance:
    - use `gem5` for the new gem5 timing path
    - use `flexus` only when you intentionally want the current Flexus timing
      backend

Window-control arguments:
- `--warmup-cycles <N>`
  - detailed warmup window in CPU cycles
  - must be non-negative
- `--measurement-cycles <N>`
  - measurement window in CPU cycles
  - must be positive
- these apply to both engines in the current `run_sample` contract

Gem5-only protocol arguments:
- `--timing-ruby`
  - selects the MESI timing-Ruby path
- `--timing-ruby-moesi`
  - selects the MOESI timing-Ruby path
- exactly one of these is required for gem5 `run_sample`
- neither is accepted on the Flexus path

Gem5-only restore/control arguments:
- `--sim-config <path>`
  - optional in syntax, but important in practice
  - passes the gem5 config-args file used for conversion and runtime contract
    resolution
  - gem5-only
- `--gem5-ckp-dir <path>`
  - optional
  - overrides the default gem5 checkpoint root
  - default is:
    - `<mounting-folder>/checkpoints/<experiment-name>`
- `--cache-hierarchy-restore/--no-cache-hierarchy-restore`
  - enables or disables gem5 LLC/L1 cache-hierarchy restore flags
  - gem5-only
- `--cleanup-conversion-artifacts/--no-cleanup-conversion-artifacts`
  - controls whether derived gem5 artifacts are deleted after each successful
    per-snapshot run
  - gem5-only

For gem5, current requirements are:
- `--timing-ruby` or `--timing-ruby-moesi`
- `--measurement-cycles > 0`

For Flexus, current constraints are:
- `--sim-config` is not accepted
- `--no-cleanup-conversion-artifacts` is not accepted
- the gem5-only Ruby protocol flags are not accepted

Important current gem5 behavior:
- per-snapshot `convert_single` is called automatically
- each converted checkpoint is then simulated as its sampling unit
- derived gem5 artifacts can be cleaned after each successful run
- final IPC/uIPC aggregation is written after the range finishes

When to override the gem5-specific options:
- override `--sim-config` when you need a particular gem5 machine/model
  contract instead of the default protocol-specific config
- override `--gem5-ckp-dir` when the checkpoint root should not live under the
  normal experiment-aligned default
- disable cache-hierarchy restore only for focused diagnostics
- disable cleanup only when you want to retain `.gem` and converted checkpoint
  artifacts for reuse or inspection

Typical gem5 use:

```bash
xargs -a ./qflex.args -- ./qflex run_sample \
  --timing-engine gem5 \
  --first snapshot_0 \
  --last snapshot_1 \
  --warmup-cycles 200000 \
  --measurement-cycles 1000000 \
  --timing-ruby-moesi \
  --cleanup-conversion-artifacts \
  --sim-config /home/dev/qflex_git/QPoints/configs/timing_ruby_moesi_ws_flexus_mesh_ref_8c.args
```

Typical Flexus use:

```bash
xargs -a ./qflex.args -- ./qflex run_sample \
  --timing-engine flexus \
  --first snapshot_0 \
  --last snapshot_0 \
  --warmup-cycles 200000 \
  --measurement-cycles 1000000
```

## Artifact model

### Source-side snapshot artifacts

Functional warming creates source-side checkpoint state under the experiment
`run/` directory. Typical artifacts include:

- `snapshot_X.loc`
- `snapshot_X.state.zstd`
- `snapshot_X.uarch/`
- optional `snapshot_X.gem/`

These represent the checkpoint source state.

### Derived gem5 artifacts

The gem5 conversion path creates:

- `run/snapshot_X.gem/` when `.gem` is emitted or regenerated
- `checkpoints/<experiment>/snapshot_X/` for the gem5 checkpoint root

These are derived artifacts, not the authoritative source lineage.

### Timing outputs

gem5 timing outputs go under:

- `QPoints/sim_outs/<experiment>/<snapshot>/`

Aggregate reporting across the processed sampling units goes under:
- `QPoints/sim_outs/<experiment>/uipc_report.json`

This report aggregates the processed sampling units for the current run.

Tracked validation packages live under:

- `QPoints/validation_records/`

## Cleanup behavior

For gem5 `run_sample`, cleanup of derived conversion artifacts is controlled by:

- `--cleanup-conversion-artifacts`
- `--no-cleanup-conversion-artifacts`

When cleanup is enabled, the current implementation removes:

- `run/snapshot_X.gem/`
- `checkpoints/<experiment>/snapshot_X/`

It does **not** remove:

- the underlying QEMU snapshot
- `snapshot_X.loc`
- `snapshot_X.state.zstd`
- `snapshot_X.uarch/`
- timing outputs in `QPoints/sim_outs/...`

This is intentional. It preserves the source lineage while allowing the derived
gem5 artifacts to be recreated later by `convert_single`.

## Tracing and instrumentation

Tracing flags are command-scoped debugging controls. They should normally be
passed directly on the CLI, not buried in the shared args file.

### Warming-side and BXKraken-side tracing

Useful commands:
- `qflex test-worm`

Relevant options:
- `qflex test-worm --branch-trace`
- `qflex test-worm --tage-decision-trace`
- `qflex test-worm --tage-decision-trace-limit`

When to use them:
- `--branch-trace` for branch-stream debugging on the warming side
- `--tage-decision-trace` when diagnosing predictor behavior on the warming side

`qflex test-worm` is useful when you want a controlled QEMU+B XKraken session
for debugging or instrumentation without generating snapshots.

Note:
- `--collect-gem5-bbl-btb` and `--emit-gem` are not tracing options
- they are workflow/export controls documented in the `initialize` and `fw`
  sections above

### gem5 timing-side tracing

Useful command:
- `qflex qpoints run-gem5`

Relevant options:
- `--branch-trace`
- `--tage-decision-trace`
- `--data-trace`
- `--dump-cache-state`

Outputs evidenced by integration tests include:
- `branch_trace_core_*.log`
- `data_trace_core_*.log`

When to use them:
- `--branch-trace` for control-flow analysis in gem5
- `--tage-decision-trace` for predictor-debugging work
- `--data-trace` for memory-access analysis
- `--dump-cache-state` for Ruby cache debugging

Important constraint:
- `--dump-cache-state` currently requires `--timing-ruby`
- these tracing options are currently exposed on `qflex qpoints run-gem5`, not
  on the higher-level `qflex run_sample` wrapper

## Kernel handling caveat

The boot flow now carries kernel provenance much more carefully than before,
but automatic extraction of the exact matching kernel ELF is still incomplete.

If the lineage requires user action during boot or the automatic capture path
cannot finalize the kernel bundle, the user may still need to provide the
kernel explicitly.

This matters most for:

- gem5 restore correctness
- TLB sidecar generation
- machine-contract validation during conversion and timing runs

## Current guidance

- Use `qflex` as the user entrypoint.
- Keep persistent experiment context in the args file.
- Keep gem5 model/runtime configuration in the sim-config.
- Keep tracing and one-off behavioral controls on the direct command line.
- Treat validation records as the evidence layer for claims that are already
  considered stable.
