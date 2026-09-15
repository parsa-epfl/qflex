# Harness: compile + 2-node parity run + diff — PLAN (awaiting user approval; nothing run yet)

Goal (user, 2026-09-14): before Phase B, a surface I can drive myself to (1) recompile qemu-pdes +
both forks in-container, (2) stash the resulting binaries under a label, (3) run the 2-node MS chain
from the existing `init_warmed` checkpoint with a chosen binary label, (4) diff two labels' artifacts
so "pre-first-commit vs post-first-commit results literally match" is a machine check, not a read.
Every run still waits for an explicit per-run "go" from the user.

## Facts the design rests on (verified read-only)

- `make test-iterate` is drifted: `tests/container_compiler.py` has no `test_*`, pytest collects
  nothing. The working entry is `python -m tests.container_compiler` (its `__main__`), which runs
  `make qemu-config/qemu-build/parallel-qemu-config/parallel-qemu-build` inside `qflex_test`
  (container_compiler.py:43-74). Container is left running.
- `qemu-saved/`, `parallel-qemu-saved/` exist only in the container layer (wiped by every build and by
  `stop-docker`). Durable outputs are the bind-mounted `qemu/build/qemu-system-aarch64` (timing) and
  `parallel-qemu/build/qemu-system-aarch64` (FW). qemu-pdes/ is bind-mounted too (docker.py:158-160).
- `set_up_folders` copies the two binaries into `<exp>/run/` from hardcoded `./*-saved/build/` paths
  with `cp -u` (config.py:373-387). No override exists. Boot/load/init/fw use `run/qemu-system-aarch64`;
  timing uses `run/vanilla-qemu-system-aarch64` via `../` from `partition_<P>/` — swapping the two files
  in `run/` covers every phase.
- Reference run already on disk: `/mnt/sdb/multi-node-experiments/experiments/media-streaming-node-{0,1}-0`
  (full chain, 500 FW snapshots, 23 partitions, timing.csv, core_info_new.csv). Its `run/` binaries are
  dated 2026-06-22 = image `qflex-worm-debug:3.36.0` (built 06-21) from C-repo HEADs of 06-15 (the four C
  repos have no newer commits); its logs print the HEAD-style `>>>>>>> NET_INIT_PDES CALLED <<<<<<<`
  (new code prints `(link 0)`). So it IS a pre-first-commit run — usable as the reference.
- Both MS qcow2s carry `booted`/`loaded`/`init_warmed`; `run/init_warmed.{state.zstd,mem/,uarch/}`
  exist per node. `init_warmed.mem/` = 33 GB (node0) + 34 GB (node1); `loaded.zstd` = 27 GB (node0).
  `.mem/base` is opened `"rb"` at loadvm (savevm.c:3292,3544); new snapshots write their own names.
- `run/root-multi-node-N.qcow2` is a symlink to the SHARED image in `image_folder`; FW `savevm` writes
  `snapshot_N` tags into it (overwrite). Timing opens it `snapshot=on` (read-only). /mnt/sdb: 124 GB free.
- `./dep exec` does not forward env; `docker exec` runs `bash -c "<command>"`, so `VAR=x ./qflex …`
  inside the command string works. OmegaConf resolves `${oc.env:VAR}` at `to_container(resolve=True)`
  (di_loader.py:158), so one parity YAML can be parameterised by an env var.
- `./qflex duplicate-experiment` (rsync src/ → dst/, hierarchy-checked) is the existing "seed run B
  from run A" surface; it has no exclude filter.
- `clean_up.sh` kills EVERY qemu on the host (`--pid=host`) — the harness must never call it.
- Per-phase MS wall clock (from the reference logs): fw 41 min, partition 0.2 s, ~190 s per timing idx
  (22 idx in partition_0 ≈ 70 min), result 53 s. Chain from init_warmed ≈ 2 h per label.

## Files (labels: `pre` = pre-first-commit build, `latest` = branch head; one shell file per test)

| File | Why |
|---|---|
| `parity-lib-tmp.sh` | shared functions: paths, `switch_repos`, `stash_build`, per-level `seed`, per-level `run` (via `./dep exec`), `diff_labels` (→ `tests/parity_diff.py`), `level_main` |
| `parity-switch-pre-tmp.sh` | checks out the PRE commit set in the 4 C repos (qemu-pdes `6593f5e`, parallel-qemu `c2f32af`, qemu `c2059d44da`, middleware `4b43c82`), `python -m tests.container_compiler` in `qflex_test`, `stash_build pre`. Refuses if any C repo has uncommitted tracked changes |
| `parity-switch-latest-tmp.sh` | same, checking out branch `single-node-pdes` in each C repo → `stash_build latest` |
| `parity-copy-tmp.sh <label>` | plain rsync (progress bar) of BOTH node folders, complete, plus both qcow2s from /mnt/sdb into the label`s /mnt/sdc locations (~169 GB per label). User runs it. Then run levels with `PARITY_SKIP_SEED=1` |
| `parity-idx-tmp.sh` | level idx: `run-idx` (p/i from the YAML's run_idx overlay); diff = that idx's `all.measurement.*.log` |
| `parity-partition-tmp.sh` | level partition: `run-single-partition`; diff = every idx of that partition |
| `parity-partitions-tmp.sh` | level partitions: `run-partition` + `result`; diff = all partitions + `timing.csv` (sorted), `core_info_new.csv`, `REQUIRED_SAMPLE_SIZE` |
| `parity-fw-tmp.sh` | level fw (isolated, parallel-qemu): `fw` from `init_warmed`; diff = snapshot set, `.loc`, `_in_flight.json`, `.uarch/statistics.csv` (ts dropped), `snapshot_*` qcow2 VM clocks |
| `parity-load-tmp.sh` | level load (isolated, parallel-qemu): `load` from `booted`; diff = `loaded` tag VM clock + `loaded_in_flight.json` |
| `parity-fw-all-tmp.sh` | level fw-all: `fw → partition-cleanup → partition → run-partition → result`; diff = fw + partitions |
| `parity-all-tmp.sh` | level all: `boot → load → initialize → fw → partition-cleanup → partition → run-partition → result`; diff = booted/loaded/init_warmed tags + fw + partitions |
| `tests/parity_diff.py` | `--level` selects the checks above; `run/core_info.csv` (input IPNS) checked at every level; all-phantom node skips timing/result; exit 1 on any diff |
| `conf/MS/ms-multi-parity-pre.yaml`, `conf/MS/ms-multi-parity-latest.yaml` | `extends: ms-multi`; relocate mounting/image/binaries folders under `/mnt/sdc/testing/parity/{experiments,images-<label>,builds/<label>}`; experiment names suffixed `-parity-<label>` |
| `commands/config.py` | `binaries_folder` field/param: `set_up_folders` force-copies the label's two binaries into `run/`, asserts they exist |
| `commands/duplicate_experiment.py` + `qflex` | repeatable `--exclude` (rsync passthrough) so each level seeds only its inputs |
| `Makefile` | only the drifted `test-iterate` target fixed (→ `python -m tests.container_compiler`) |

Every level script has the same shape: `./parity-<level>-tmp.sh <pre|latest> <container>` seeds that
label for that level (copies from the /mnt/sdb reference, which is only ever read) and runs it in
the foreground; `./parity-<level>-tmp.sh diff` compares pre vs latest. A full "test the harness at
version X" is therefore: `./parity-switch-X-tmp.sh` → `./parity-<level>-tmp.sh X qflex_test` for the
levels wanted (smallest first: idx → partition → partitions → fw → load → fw-all → all) → after both
labels ran a level, `./parity-<level>-tmp.sh diff`. Each step still waits for the user's per-run go;
`switch_repos` is the ONLY place I run a git checkout, and only on the user's explicit command.

Per-level seed (from the reference, plus per-label qcow2 copies): timing levels keep the
reference's partitioned FW snapshots and drop its `result_*`; fw levels drop `partition_*`/`snapshot_*`;
load drops `loaded.zstd` + `init_warmed*`; all copies nothing but the images.

## The YAMLs track the API — update them as the schema changes (user directive)

The parity configs are part of the change set, not frozen fixtures. Every phase that changes the
public surface must update them in the same step, and `./qflex get-experiment-folder -c <yaml>` +
the dry-run suite must pass on both before any run:

| Phase | `ms-multi-parity-pre.yaml` | `ms-multi-parity-latest.yaml` |
|---|---|---|
| now (A.9) | legacy lists inherited from `ms-multi` | same |
| B1–B3 (fork call sites) | unchanged | unchanged (C API only) |
| C1 (`nics:` DI schema, `commands/nic.py`) | STAYS on the legacy 4 lists (the legacy translation path must keep working — that is the byte-identical guarantee under test) | SWITCHES to explicit `nics:` components (`node0_nic0 → (1,0)`, `node1_nic0 → (0,0)`), `neighbor_node_list`/`latencies_ns_list`/`syncs_list`/`pdes_net_devs` removed from its leaves; per-phase `boot:` latency/sync overrides move onto the NIC components |
| C2+ (per-phase enablement) | unchanged | any new per-phase knob the latest run needs |

Invariant to assert at C1 before running: both YAMLs resolve to the SAME shm names, MAC/PCI order
and `-netdev` clauses (legacy 0↔0 exception) — check with the dry-run printer, not by eye.

## Building each label = a specific commit set in the four C repos (user switches, I never run git writes)

| Repo | pre (pre-first-commit) | latest |
|---|---|---|
| qemu-pdes | `6593f5e` (HEAD; the A.1–A.9 work is the uncommitted tree on top) | branch head after the first commit (+ later ones) |
| parallel-qemu | `c2f32af` | after B1 |
| qemu | `c2059d44da` | after B2 |
| qemu/middleware | `4b43c82` | after B3 |
| qflex (python/YAML) | branch head is fine for both labels — `binaries_folder` and the parity YAMLs are additive; the pre YAML keeps the legacy schema | branch head |

Sequence per label (`parity-switch-<label>-tmp.sh`): the four C repos are checked out at the label's
commits — refused while any has uncommitted tracked changes, so the A.x tree must be committed first → `python -m tests.container_compiler` (in `qflex_test`) →
`stash_build <label>` → switch back — all of it is `parity-switch-<label>-tmp.sh`. `PROVENANCE.txt` records what was built.
Shortcut for pre: the 3.36.0/3.37.0 images were built from exactly these pre commits, so their
`-saved/` binaries are a valid pre stash without any checkout (needs a `./dep`-surface way to copy
them out — not written; only needed if the on-disk reference results are ever re-run).
After each of B1/B2/B3 lands the latest binaries change → rerun `parity-switch-latest-tmp.sh` before the next parity run.

## Status

- [x] H1 files WRITTEN 2026-09-14 (not run, not compiled): Makefile (test-iterate fix only), parity-lib-tmp.sh + one
      parity-<level>-tmp.sh per level + parity-switch-{pre,latest}-tmp.sh (user: separate tmp shell files, not the Makefile), commands/config.py (binaries_folder), commands/
      duplicate_experiment.py + qflex (--exclude), conf/MS/ms-multi-parity-{pre,latest}.yaml,
      tests/parity_diff.py. Two configs (user) instead of one env-parameterised YAML.
- [ ] Dry checks the user can run without side effects: `./qflex get-experiment-folder -c conf/MS/ms-multi-parity-latest.yaml`,
      `make test` (hermetic dry-run suite must stay green — binaries_folder default is empty).
- [ ] First use: `parity-switch-pre-tmp.sh` needs the A.x qemu-pdes tree committed first (dirty-tree guard); then `parity-idx-tmp.sh pre qflex_test`. `latest` only after Phase B (forks must link).
