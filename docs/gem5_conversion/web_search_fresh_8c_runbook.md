# Web-Search Fresh 8C Runbook

This is a machine-specific walkthrough for running the existing web-search
image on this machine with the `qflex` CLI from `boot` through `run_sample`.

This is not a general CLI document. It is customized for:

- host: `iccluster118`
- container: `cb8*`
- repo: `/home/dev/qflex_git`
- args file: `/home/dev/qflex_git/args/ws_image_fresh_8c.qflex.args`
- comparison args file:
  `/home/dev/qflex_git/args/ws_image_fresh_8c_flexus_compare.qflex.args`
- image folder: `/mnt/sdb/aansari/ws-image/web-search-fresh-8c`
- experiment folder: `/mnt/sdb/aansari/experiments/ws-image-fresh-8c`
- checkpoint folder: `/mnt/sdb/aansari/checkpoints/ws-image-fresh-8c`
- image file: `root.qcow2`
- reference kernel folder:
  `/mnt/sdb/aansari/ws-image/web-search-fresh-8c`
- reference kernel ELF:
  `/mnt/sdb/aansari/ws-image/web-search-fresh-8c/vmlinux-6.1.34-3-virt.elf`

All commands below assume:

```bash
cd /home/dev/qflex_git
```

## 1. Boot the image and create the `boot` snapshot

This is the bounded `boot` flow. It boots the VM, captures kernel provenance,
creates the `boot` snapshot, and shuts the VM down.

```bash
./qflex boot --args-file /home/dev/qflex_git/args/ws_image_fresh_8c.qflex.args
```

Expected result:

- internal QEMU snapshot `boot`
- experiment folder created under:
  `/mnt/sdb/aansari/experiments/ws-image-fresh-8c`

## 2. If `boot` ends with manual kernel action, adopt the reference kernel ELF

The bounded `boot` workflow can end with a kernel status that requires manual
user action if the matching guest-side `vmlinux` ELF is not found automatically.

For this machine and this web-search image, the reference kernel ELF now lives
next to the qcow2 image here:

- `/mnt/sdb/aansari/ws-image/web-search-fresh-8c`

Use that reference ELF to mark the experiment kernel bundle ready:

```bash
./qflex kernel adopt \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c.qflex.args \
  --kernel /mnt/sdb/aansari/ws-image/web-search-fresh-8c/vmlinux-6.1.34-3-virt.elf
```

Use this step only when `boot` reports that manual kernel capture/adoption is
required. If `boot` already leaves the kernel status in `ready`, skip this
step.

## 3. Load the `boot` snapshot and drive the workload to the desired state

This starts a live QEMU session from `boot`.

```bash
./qflex load \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c.qflex.args \
  --loadvm-name boot
```

Inside that live session, drive the web-search workload to the state you want
to use as the baseline for initialization.

When you are ready, create a new internal snapshot named `loaded` from the QEMU
monitor and then quit the VM:

```text
savevm loaded
quit
```

This runbook uses `loaded` as the post-load baseline snapshot name.

## 4. Initialize long-history state and create `init_warmed`

This stage warms the long-history BXKraken-side state and creates the
`init_warmed` snapshot. BBL-BTB collection is enabled so the later gem5
lineage keeps frontend export continuity.

```bash
./qflex initialize \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c.qflex.args \
  --loadvm-name loaded \
  --collect-gem5-bbl-btb
```

Expected result:

- internal QEMU snapshot `init_warmed`

## 5. Run functional warming and generate the snapshot lineage

This stage resumes from `init_warmed` and drops the sampled snapshots.

The command below uses:

- `--sample-size 100` to create `snapshot_0` through `snapshot_99`
- `--collect-gem5-bbl-btb` to continue the BBL-BTB export lineage

This runbook intentionally does **not** use `--emit-gem` during `fw`.
Instead, the `.gem` bundle is created lazily during `run_sample` and then
cleaned up after each successful gem5 run. This keeps the functional-warming
phase lighter and avoids storing a full `.gem` tree for every snapshot when the
timing phase can regenerate the derived gem5 artifacts on demand.

```bash
./qflex fw \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c.qflex.args \
  --loadvm-name init_warmed \
  --sample-size 100 \
  --collect-gem5-bbl-btb
```

Expected result:

- internal QEMU snapshots:
  - `snapshot_0`
  - `snapshot_1`
  - ...
  - `snapshot_99`
- run-directory artifacts under:
  `/mnt/sdb/aansari/experiments/ws-image-fresh-8c/run/`

If you want a different lineage length, change `--sample-size` and adjust the
`run_sample` range accordingly.

## 6. Run the full snapshot range with Flexus

This runs the entire generated lineage from the first snapshot to the last
snapshot using the Flexus timing engine.

```bash
./qflex run_sample \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c_flexus_compare.qflex.args \
  --first snapshot_0 \
  --last snapshot_99 \
  --warmup-cycles 200000 \
  --measurement-cycles 1000000 \
  --timing-engine flexus
```

Results are written under:

- `/home/dev/qflex_git/QPoints/sim_outs/ws-image-fresh-8c/`

Important files:

- `uipc_report.json`
- `snapshot_N/uipc_summary.json`
- `snapshot_N/all.measurement.end.log`

## 7. Run the full snapshot range with gem5

This runs the same snapshot range with the gem5 MOESI timing path.

This path uses lazy conversion by default:

- if `snapshot_N.gem/` and the converted gem5 checkpoint do not already exist,
  `run_sample` creates them for the current snapshot
- after a successful run, the default cleanup behavior removes those derived
  gem5 conversion artifacts again

That is why the `fw` step above does not emit `.gem` bundles up front.

```bash
./qflex run_sample \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c_flexus_compare.qflex.args \
  --first snapshot_0 \
  --last snapshot_99 \
  --warmup-cycles 200000 \
  --measurement-cycles 1000000 \
  --timing-engine gem5 \
  --timing-ruby-moesi \
  --sim-config /home/dev/qflex_git/QPoints/configs/timing_ruby_moesi_ws_flexus_mesh_ref_8c.args
```

Results are written under:

- `/home/dev/qflex_git/QPoints/sim_outs/ws-image-fresh-8c/`

Important files:

- `uipc_report.json`
- `snapshot_N/uipc_summary.json`
- `snapshot_N/stats.txt`

## 8. Preserve Flexus and gem5 outputs separately

Both timing engines currently write canonical reports under the same top-level
folder:

- `/home/dev/qflex_git/QPoints/sim_outs/ws-image-fresh-8c/`

So if you run both engines back to back, the second run will overwrite the
first run's report tree.

Use this sequence instead:

1. run one timing engine
2. rename its result folder immediately
3. run the second timing engine
4. rename its result folder immediately

For example, if you run Flexus first:

```bash
mv /home/dev/qflex_git/QPoints/sim_outs/ws-image-fresh-8c \
   /home/dev/qflex_git/QPoints/sim_outs/ws-image-fresh-8c_flexus_200k_1m
```

Then after the gem5 run:

```bash
mv /home/dev/qflex_git/QPoints/sim_outs/ws-image-fresh-8c \
   /home/dev/qflex_git/QPoints/sim_outs/ws-image-fresh-8c_gem5_200k_1m
```

At the end, the preserved result folders are:

- `/home/dev/qflex_git/QPoints/sim_outs/ws-image-fresh-8c_flexus_200k_1m`
- `/home/dev/qflex_git/QPoints/sim_outs/ws-image-fresh-8c_gem5_200k_1m`

## 9. Minimal command list

If you only want the command sequence without the surrounding notes, it is:

```bash
cd /home/dev/qflex_git

./qflex boot \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c.qflex.args

./qflex kernel adopt \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c.qflex.args \
  --kernel /mnt/sdb/aansari/ws-image/web-search-fresh-8c/vmlinux-6.1.34-3-virt.elf

./qflex load \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c.qflex.args \
  --loadvm-name boot

./qflex initialize \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c.qflex.args \
  --loadvm-name loaded \
  --collect-gem5-bbl-btb

./qflex fw \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c.qflex.args \
  --loadvm-name init_warmed \
  --sample-size 100 \
  --collect-gem5-bbl-btb

./qflex run_sample \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c_flexus_compare.qflex.args \
  --first snapshot_0 \
  --last snapshot_99 \
  --warmup-cycles 200000 \
  --measurement-cycles 1000000 \
  --timing-engine flexus

mv /home/dev/qflex_git/QPoints/sim_outs/ws-image-fresh-8c \
   /home/dev/qflex_git/QPoints/sim_outs/ws-image-fresh-8c_flexus_200k_1m

./qflex run_sample \
  --args-file /home/dev/qflex_git/args/ws_image_fresh_8c_flexus_compare.qflex.args \
  --first snapshot_0 \
  --last snapshot_99 \
  --warmup-cycles 200000 \
  --measurement-cycles 1000000 \
  --timing-engine gem5 \
  --timing-ruby-moesi \
  --sim-config /home/dev/qflex_git/QPoints/configs/timing_ruby_moesi_ws_flexus_mesh_ref_8c.args

mv /home/dev/qflex_git/QPoints/sim_outs/ws-image-fresh-8c \
   /home/dev/qflex_git/QPoints/sim_outs/ws-image-fresh-8c_gem5_200k_1m
```
