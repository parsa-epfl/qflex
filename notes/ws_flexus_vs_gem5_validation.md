# Web-Search Flexus vs gem5 Validation Notes

## Scope

This note records the validation process for the new phase focused on
comparing Flexus IPC against gem5 IPC for:

- experiment: `ws-image-fresh-8c`
- snapshot: `snapshot_0`

The immediate question for this phase is:

- are Flexus and gem5 in the same ballpark on this snapshot?

This note is intended to be updated step by step as the validation proceeds.

## Current comparison contract

### Reference files

Top-level experiment/checkpoint context:

- `args/ws_image_fresh_8c_flexus_compare.qflex.args`

gem5 simulated-machine reference:

- `QPoints/configs/timing_ruby_moesi_ws_flexus_ref_8c.args`

Flexus simulated-machine reference:

- `/mnt/sdb/aansari/experiments/ws-image-fresh-8c/cfg/flexus_configuration.json`
- `/mnt/sdb/aansari/experiments/ws-image-fresh-8c/cfg/timing.cfg`

### Standard comparison window

Use the same window for both engines:

- snapshot: `snapshot_0`
- cores: `8`
- warmup: `200000` cycles
- measurement: `1000000` cycles

Rationale:

- this window is already validated on the gem5 side for this lineage
- Flexus requires 100k-aligned windows
- it is more meaningful than the smaller earlier bring-up windows

## Ownership model

### Experiment/checkpoint context

Owned by:

- `args/ws_image_fresh_8c_flexus_compare.qflex.args`

This file defines stable context such as:

- experiment identity
- checkpoint root context
- core count
- memory size
- image / bootloader / root device

### Simulated machine

Owned separately per engine.

gem5:

- `QPoints/configs/timing_ruby_moesi_ws_flexus_ref_8c.args`

Flexus:

- `/mnt/sdb/aansari/experiments/ws-image-fresh-8c/cfg/flexus_configuration.json`
- `/mnt/sdb/aansari/experiments/ws-image-fresh-8c/cfg/timing.cfg`

This split is intentional:

- CLI/run parameters control the experiment execution
- engine-specific config files control the simulated machine

## What is aligned today

The current gem5 reference sim-config is aligned to the recorded Flexus
machine as closely as the current path allows on:

- 8 cores
- L1I: `64kB`, 8-way
- L1D: `64kB`, 8-way
- LLC slice size: `1MB`
- LLC associativity: `16`
- LLC slice count: `8`
- directory slice count: `8`
- BTB: `4096 sets x 4 ways` -> `16384 entries`, `4 ways`
- ITLB: `64`
- DTLB: `64`
- large ASID enabled
- memory channels / ranks
- restored BTB / TAGE / TLB state
- major frontend/core knobs with direct gem5 counterparts:
  - fetch/decode/rename/dispatch/issue/wb widths
  - commit width
  - fetch queue size
  - ROB size
  - store queue size

## Known non-equivalences

These are currently accepted mismatches and must be called out in any result
discussion:

1. Flexus LLC slice homing uses 4KB group interleaving; current gem5 Ruby
   still cannot express that homing rule faithfully.

So the comparison is:

- closer than the generic gem5 path
- still not fully structurally equivalent

This is acceptable for the initial “same ballpark or not” question, but not
for any stronger equivalence claim.

## Phase target refinement

Before running the main Flexus-vs-gem5 IPC comparison, this phase will try to
reduce two structural mismatches that are feasible to address quickly:

1. topology mismatch
   - target: move the gem5 reference from Crossbar toward a mesh topology
2. LLC slicing mismatch
   - target: use a sliced gem5 LLC / directory configuration that matches the
     recorded Flexus slice counts

This phase will **not** attempt to fix LLC home assignment / slice homing.

That means:

- we do want gem5 to use a mesh-like network if the current path supports it
- we do want gem5 to use an 8-slice LLC / 8-slice directory structure
- we do **not** expect gem5 in this phase to match the Flexus 4KB
  group-interleaved LLC home assignment rule

So the intended comparison target for this phase is:

- topology: closer
- slice count: closer
- home-assignment semantics: still knowingly different

Any interpretation of the final IPC gap must preserve that caveat.

## Initial gem5 sliced-LLC smoke result

A short MOESI gem5 smoke was run on `snapshot_0` with the current 8-slice
reference sim-config. Two cases were checked:

1. sliced runtime with cache-hierarchy restore enabled
2. sliced runtime with cache-hierarchy restore disabled

### What passed

The control run with cache-hierarchy restore disabled completed successfully.
That establishes that the current gem5 path can:

- restore and execute `snapshot_0`
- use `MOESI_CMP_directory`
- use `8` LLC slices
- use `8` directory slices
- use `1MB` per LLC slice

So the intended runtime shape is viable:

- LLC: `8 x 1MB = 8MB` total
- directories: `8` slices

### What failed

The same smoke with LLC/L1 cache-hierarchy restore enabled failed immediately
at gem5 bring-up with:

- `fatal: --restore-llc-state currently supports only --num-l2caches=1; got 8 L2 caches.`

### Current conclusion

At this point the missing piece is not sliced MOESI execution itself.
The missing piece is multi-slice LLC warm restore support.

So for this phase, the state is:

- sliced gem5 runtime: yes
- sliced gem5 with checkpoint restore: yes
- sliced gem5 with LLC/L1 cache-hierarchy warm restore: no

That means any near-term gem5 vs Flexus comparison can proceed either:

1. without cache-hierarchy restore, or
2. after implementing multi-slice LLC restore support

The first path is suitable for a ballpark runtime comparison. The second path
is required for a closer warm-state comparison.

## Follow-up tooling note

Current `fw` behavior always emits the full warm snapshot bundle, including
`*.gem`, alongside the other snapshot artifacts.

That is acceptable for this phase, but it is heavier than necessary when the
user only wants Flexus-side functional warming and does not plan to run gem5
timing simulation.

We should add a future `fw` mode that skips `*.gem` emission when gem5 timing
artifacts are not needed.

## Functional warming used for BTB regeneration

To regenerate `snapshot_0` with gem5 BTB export enabled, the following command
was used:

```bash
/home/dev/qflex_git/qflex fw \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c_flexus_compare.qflex.args \
  --loadvm-name init_warmed \
  --sample-size 1 \
  --collect-gem5-bbl-btb
```

### Relevant args/config used by that run

The `fw` run used:

- args file:
  - `args/ws_image_fresh_8c_flexus_compare.qflex.args`
- load VM:
  - `init_warmed`
- sample size:
  - `1`
- gem5 BTB export:
  - enabled via `--collect-gem5-bbl-btb`

Relevant contents of the args file for this run:

- `--core-count 8`
- `--llc-size-per-tile-mb 1`
- `--parallel`
- `--network user`
- `--memory-gb 32`
- `--workload-name web-search`
- `--population-seconds 0.1`
- `--experiment-name ws-image-fresh-8c`
- `--image-name root.qcow2`
- `--image-folder /mnt/sdb/aansari/ws-image/web-search-fresh-8c`
- `--bootloader /home/dev/qflex_git/QPoints/bin/m5/binaries/boot_v2_qemu_virt.arm64`
- `--root-device /dev/vda`
- `--mounting-folder /mnt/sdb/aansari`
- `--check-period-quantum-coeff 53.0`
- `--use-image-directly`

### Output verified after regeneration

The run regenerated:

- `/mnt/sdb/aansari/experiments/ws-image-fresh-8c/run/snapshot_0.gem`
- `/mnt/sdb/aansari/experiments/ws-image-fresh-8c/run/snapshot_0.loc`
- `/mnt/sdb/aansari/experiments/ws-image-fresh-8c/run/snapshot_0.state.zstd`
- `/mnt/sdb/aansari/experiments/ws-image-fresh-8c/run/snapshot_0.uarch`

Most importantly, the new fetch-side source file:

- `/mnt/sdb/aansari/experiments/ws-image-fresh-8c/run/snapshot_0.uarch/fetch.json.zstd`

now contains:

- `restore_export.bbl_btb`

per core, which is the BTB export view expected by the gem5-side converter.

## Conversion of regenerated snapshot_0

After regenerating `snapshot_0`, the following conversion command was used:

```bash
/home/dev/qflex_git/qflex qpoints convert-single \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c_flexus_compare.qflex.args \
  --snapshot snapshot_0 \
  --overwrite
```

### Result

The conversion completed successfully and rebuilt:

- `/mnt/sdb/aansari/checkpoints/ws-image-fresh-8c/snapshot_0`
- `/mnt/sdb/aansari/checkpoints/ws-image-fresh-8c/snapshot_0/m5.cpt`
- `/mnt/sdb/aansari/checkpoints/ws-image-fresh-8c/snapshot_0/gem5_uarch/moesi_cmp_directory`

Observed completion summary:

- `convert-single completed in 174s`

### Emitted BTB / TAGE restore artifacts

The regenerated MOESI protocol directory now contains BTB restore files:

- `btb_restore_addrs.core0.txt`
- `btb_restore_addrs.core1.txt`
- `btb_restore_addrs.core2.txt`
- `btb_restore_addrs.core3.txt`
- `btb_restore_addrs.core4.txt`
- `btb_restore_addrs.core5.txt`
- `btb_restore_addrs.core6.txt`
- `btb_restore_addrs.core7.txt`

and TAGE restore files:

- `tage_restore_state.core0.json`
- `tage_restore_state.core1.json`
- `tage_restore_state.core2.json`
- `tage_restore_state.core3.json`
- `tage_restore_state.core4.json`
- `tage_restore_state.core5.json`
- `tage_restore_state.core6.json`
- `tage_restore_state.core7.json`

The MOESI manifest confirms that BTB restore is no longer empty:

- `restorable_branch_candidates: 111142`
- `line_count: 111142`

So the regenerated `snapshot_0` fixed the original source-side BTB export
problem, and conversion now stages both BTB and TAGE restore artifacts for the
MOESI checkpoint path.

## Fundamental restore layout fix

### Contract

Protocol-agnostic warm-state artifacts must live at top-level:

- `snapshot_0/gem5_uarch/btb_restore_candidates.json`
- `snapshot_0/gem5_uarch/btb_restore_addrs.coreN.txt`
- `snapshot_0/gem5_uarch/tage_restore_candidates.json`
- `snapshot_0/gem5_uarch/tage_restore_state.coreN.json`
- `snapshot_0/gem5_uarch/mmu-cpuN.cpt`

Only cache/coherence artifacts should remain protocol-scoped:

- `snapshot_0/gem5_uarch/mesi_two_level/...`
- `snapshot_0/gem5_uarch/moesi_cmp_directory/...`

### Root cause

The converter was writing BTB/TAGE restore files into the selected Ruby
protocol directory, while timing-Ruby runtime discovery was correctly looking
for BTB/TAGE at top-level `gem5_uarch/`.

That meant:

- conversion generated BTB/TAGE state
- runtime failed to discover it
- gem5 ran with cold BTB/TAGE even when the checkpoint carried valid restore
  artifacts

### Code fix

Changed:

- `/home/dev/qflex_git/QPoints/scripts/uarch_restore/prepare_gem5_uarch.py`

Decision:

- keep runtime discovery unchanged
- keep protocol-specific LLC/L1/directory files under the Ruby protocol
  subdirectory
- move BTB/TAGE restore file emission to top-level `gem5_uarch/`

No runtime script change was required for this fix.

### Revalidated conversion output

After the converter fix, the rebuilt checkpoint now has:

- `/mnt/sdb/aansari/checkpoints/ws-image-fresh-8c/snapshot_0/gem5_uarch/btb_restore_addrs.core0.txt`
- `/mnt/sdb/aansari/checkpoints/ws-image-fresh-8c/snapshot_0/gem5_uarch/tage_restore_state.core0.json`
- `/mnt/sdb/aansari/checkpoints/ws-image-fresh-8c/snapshot_0/gem5_uarch/mmu-cpu0.cpt`

while protocol-specific cache restore files remain under:

- `/mnt/sdb/aansari/checkpoints/ws-image-fresh-8c/snapshot_0/gem5_uarch/moesi_cmp_directory/`

The MOESI manifest now records BTB/TAGE restore paths at top-level
`gem5_uarch/`, which matches the intended contract.

### Runtime proof

To isolate BTB/TAGE discovery from the separate multi-slice LLC restore
limitation, the following timing-Ruby MOESI smoke was run with cache-hierarchy
restore disabled:

```bash
/home/dev/qflex_git/qflex qpoints run-gem5 \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c_flexus_compare.qflex.args \
  --snapshot snapshot_0 \
  --warmup-cycles 0 \
  --measurement-cycles 100000 \
  --timing-ruby-moesi \
  --sim-config /home/dev/qflex_git/QPoints/configs/timing_ruby_moesi_ws_flexus_ref_8c.args \
  --no-cache-hierarchy-restore
```

gem5 confirmed that BTB and TAGE restore were consumed from top-level
`gem5_uarch/` for all cores. Example runtime messages:

- `system.cpu_cluster.cpus0.branchPred restored 16368 staged BTB entries from /mnt/sdb/aansari/checkpoints/ws-image-fresh-8c/snapshot_0/gem5_uarch/btb_restore_addrs.core0.txt`
- `system.cpu_cluster.cpus0.branchPred.tage restored staged TAGE state from /mnt/sdb/aansari/checkpoints/ws-image-fresh-8c/snapshot_0/gem5_uarch/tage_restore_state.core0.json`

So the restore-consumption issue is fixed at the layout level, not worked
around at runtime.

## Canonical commands

### gem5 reference

```bash
/home/dev/qflex_git/qflex run_sample \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c_flexus_compare.qflex.args \
  --first snapshot_0 \
  --last snapshot_0 \
  --warmup-cycles 200000 \
  --measurement-cycles 1000000 \
  --timing-engine gem5 \
  --timing-ruby-moesi \
  --sim-config /home/dev/qflex_git/QPoints/configs/timing_ruby_moesi_ws_flexus_ref_8c.args
```

### Flexus reference

```bash
/home/dev/qflex_git/qflex run_sample \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c_flexus_compare.qflex.args \
  --first snapshot_0 \
  --last snapshot_0 \
  --warmup-cycles 200000 \
  --measurement-cycles 1000000 \
  --timing-engine flexus
```

## Operational caveat

Both engines currently write canonical reports under:

- `QPoints/sim_outs/ws-image-fresh-8c/`

So running them back to back will overwrite the canonical report path unless
the outputs are copied out between runs.

## Planned validation sequence

1. Run gem5 with the MOESI reference sim-config.
2. Copy gem5 report artifacts to a comparison staging location.
3. Run Flexus with the same args and timing window.
4. Copy Flexus report artifacts to the same staging location.
5. Compare:
   - aggregate IPC
   - average IPC
   - per-core IPC
   - uIPC when available
6. Decide whether the two engines are in the same ballpark before deeper
   diagnosis.


## Lazy `.gem` emission phase

This phase established the intended contract for functional warming:

- `fw` should be able to create a warm snapshot without emitting
  `snapshot_0.gem/`
- later, if gem5 conversion is needed, `convert-single` should materialize the
  missing `.gem` bundle lazily from the saved snapshot

### Contract and implementation work

Two implementation pieces were required.

1. Optional `.gem` emission on the warm path

- `qflex fw` now accepts:
  - `--emit-gem`
- default behavior is now:
  - do **not** emit `.gem` during `fw`
- WormCache was updated so the warm snapshot path explicitly passes the
  `generate_gem5_chkpt` boolean into the parallel-QEMU snapshot API

2. Refresh + rebuild hook for experiment-local WormCache

The first runtime attempt failed for a non-obvious reason:

- `fw` rebuilt the experiment-local WormCache plugin
- but it rebuilt stale source already sitting in:
  - `<experiment>/lib/WormCacheQFlex`
- so the old plugin still emitted `snapshot_0.gem/`

The fix was to make the refresh/build hook explicit and shared.

Current hook behavior:

1. if `refresh_wormcache` is enabled, recopy:
   - repo-root `WormCacheQFlex`
   - into `<experiment>/lib/WormCacheQFlex`
2. copy experiment `cfg/parameter.rs`
3. rebuild there
4. copy back:
   - `libworm_cache.so`
   - `checkpoint_conversion`

This shared hook now applies consistently to:

- `fw`
- `init_warm`
- `test_worm`

### Successful lazy-emission validation

After resetting `snapshot_0` from:

- the qcow2 internal snapshot table
- the experiment `run/` directory
- the checkpoint directory

the following command was run:

```bash
/home/dev/qflex_git/qflex fw   --args-file /home/dev/qflex_git/args/ws_image_fresh_8c_flexus_compare.qflex.args   --refresh-wormcache   --loadvm-name init_warmed   --sample-size 1   --collect-gem5-bbl-btb
```

### Result

The run completed successfully and created:

- `snapshot_0` in the qcow2 image
- `run/snapshot_0.loc`
- `run/snapshot_0.state.zstd`
- `run/snapshot_0.uarch/`

Critically, it did **not** create:

- `run/snapshot_0.gem/`

So the `fw` half of the lazy `.gem` contract is now working as intended.

### Current status at end of this phase

What is now proven:

- functional warming can create `snapshot_0` without pre-emitting `.gem`
- the experiment-local WormCache refresh+rebuild path is now correct and shared

What remains to validate next:

- use the lazy `snapshot_0` lineage for the next gem5/Flexus comparison steps

### Successful lazy conversion validation

With `snapshot_0` present from `fw` and no preexisting `run/snapshot_0.gem/`, the following command was run:

```bash
/home/dev/qflex_git/qflex qpoints convert-single \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c_flexus_compare.qflex.args \
  --snapshot snapshot_0 \
  --overwrite
```

### Result

The lazy conversion contract worked as intended:

1. `convert-single` detected that `run/snapshot_0.gem/` was missing
2. it materialized `snapshot_0.gem/` lazily from the saved snapshot
3. it proceeded into the normal checkpoint conversion path

Observed outputs included:

- `run/snapshot_0.gem/`
- `checkpoints/ws-image-fresh-8c/snapshot_0/snapshot_0.img`
- `checkpoints/ws-image-fresh-8c/snapshot_0/m5.cpt`
- `checkpoints/ws-image-fresh-8c/snapshot_0/machine_config.json`
- `checkpoints/ws-image-fresh-8c/snapshot_0/gem5_uarch/`

The later console output was the usual TLB-apply / VATranslator tail, not a failure of the lazy `.gem` contract itself.

### Current state at end of lazy `.gem` validation

The two-part contract is now validated end to end:

1. `fw` can create `snapshot_0` without pre-emitting `.gem`
2. `convert-single` can backfill `.gem` lazily and continue conversion

This closes the lazy `.gem` validation phase.
