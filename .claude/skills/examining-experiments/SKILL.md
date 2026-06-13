---
name: examining-experiments
description: Use ONLY when debugging the FINAL, overall result of a COMPLETED qflex run — i.e. the run has finished (all phases through `result`) and you're trying to explain the numbers, decide if the run is trustworthy, or compare two finished runs. Covers reading the timing result (`RunResultCommand.log`: Average U-IPC, CV, required-vs-current sample size, censored zero/NaN windows), distinguishing a real server stall from a result-script window/censoring artifact (per-dump `Commits:NonSpin:User` trace in `all.measurement.*.log`), the decisive memory-pressure / idle scan (`WFICycles>0`, `Idle:Commits>0`, kernel share, MPKI, CPI-stack), the host-side resource check (`free -h; df -h /dev/shm; vmstat 1 3` + RSS-vs-`-m`-ceiling lazy-allocation reading of the qemu processes), and the single-node-vs-multi-node parity comparison (what the CLI derives — cores/cache/memory/controllers — and which of those should be *doubled* vs *equal* in single-node, with the why). TRIGGER when the user says "why is the U-IPC …", "is this run correct", "compare single vs multi results", "did this run hit memory pressure", "the result looks wrong", or asks to examine/audit a finished experiment's output. SKIP entirely for: live monitoring of a still-running phase, writing/authoring experiments or YAML/expects, the executor/dispatch internals, or anything before the run has produced a `result`. This skill is a read-only forensic checklist — it changes no configs.
---

# Examining a finished qflex experiment (post-run result forensics)

Always, Even if you make assumptions from this skill file, double check with user again.

Read-only checklist for explaining/validating the **final** result of a completed run, or
comparing two finished runs. Everything here is `Read`/`grep`/`ps` — never edit configs or kill
processes from this skill. All findings below were validated against real runs.

## 0. Locate the experiment folder — never hand-write the path

```bash
./qflex get-experiment-folder -c <yaml> | grep '^/'
```
Pure path computation (runs the YAML through the loader). Prints the group folder then each
sub-experiment's per-node folder. Per-phase artifacts live under `<exp>/`:
`Boot|Load|InitWarm|FunctionalWarming|RunPartition|RunSinglePartitionCommand|RunResultCommand.{log,err}`,
plus Path-A `expect_log.txt` / `qemu_serial.log` / `qemu_monitor.log`, and per-(partition,idx)
timing output under `<exp>/run/partition_<P>/result_<idx>/all.measurement.*.log`.

## 1. Read the final result — `RunResultCommand.log`

The headline table:
```
║ Average U-IPC            ║ <mean> ║   ← user-mode IPC = -uarch-Commits:NonSpin:User / cycles
║ Coefficient of Variation ║ <cv>   ║
║ Current Sample Size      ║ <n>    ║   ← snapshots that survived censoring (NOT always == sample_size)
║ Required Sample Size     ║ <req>  ║   ← if > current, the CI isn't tight enough
```
- **`Current Sample Size` < `sample_size`** means windows were dropped. `result.py` censors
  zero/NaN U-IPC windows (`OMMIT_ZERO=True`) — every dropped one prints `Snapshot N: Invalid IPC
  value 0.0 (NaN or zero)`. `result_new.py` keeps zeros (drops NaN only). **For cross-run
  comparisons use `result_new.py`'s numbers**, and be aware the censored mean is inflated relative
  to a keep-zeros mean.
- **U-IPC = `-uarch-Commits:NonSpin:User`** (user-mode, spin-filtered commits) over a fixed-cycle
  measurement window. Plain IPC = `-uarch-Commits` (incl. kernel+spin).
- Quick mean of the per-snapshot prints:
  ```bash
  grep -oE "IPC data across cores: \[[0-9.]+\]" RunResultCommand.log | grep -oE "[0-9]+\.[0-9]+" \
    | awk '{s+=$1;n++} END{if(n)printf "mean=%.4f n=%d\n",s/n,n}'
  ```

## 2. Is a "zero" window a real stall, or a result-script artifact?

A censored "Invalid IPC value 0.0" window does **NOT** mean the server did nothing. The window is
`[index, index+measure_units)` of per-interval deltas; a snapshot can do all its work in the
*warming* prefix and little in the *measurement* segment (the H2 effect), yielding ~0 there while
the run committed hundreds of thousands of user instructions overall.

To decide, trace the per-dump cumulative user commits of the suspect snapshot (note: the printed
"Snapshot N" index may not equal the `result_N/` folder — the aggregator re-orders; check a few):
```bash
f=$(find <exp>/run -type d -name result_<idx>|head -1)
for c in 0000100000 0000200000 0000300000 0000400000 0000500000; do
  v=$(grep -h "uarch-Commits:NonSpin:User" "$f/all.measurement.$c.log"|grep -oE "[0-9]+$"|head -1)
  echo "cyc $c: $v"
done
```
Monotonically growing commits ⇒ the server was working; the "zero" is a window-placement/censoring
artifact, not a stall. Flat/zero across all dumps ⇒ genuinely idle that snapshot.

## 3. The decisive memory-pressure / idle scan

Memory pressure (swap/reclaim) or an idle/under-driven server shows up in the Flexus counters, not
just guest logs. Scan **all** snapshots:
```bash
tot=0;wfi=0;idle=0
for f in $(find <exp>/run -type f -name all.measurement.end.log); do
  tot=$((tot+1))
  w=$(grep -h uarch-WFICycles "$f"|grep -oE "[0-9]+$"|head -1)
  i=$(grep -h "uarch-TB:Idle:Commits" "$f"|grep -oE "[0-9]+$"|head -1)
  [ "${w:-0}" -gt 0 ] 2>/dev/null && wfi=$((wfi+1))
  [ "${i:-0}" -gt 0 ] 2>/dev/null && idle=$((idle+1))
done
echo "snapshots=$tot WFI>0=$wfi Idle>0=$idle"
```
Interpretation:
- **`WFICycles > 0`** anywhere = the CPU entered wait-for-interrupt (idle-waiting). All-zero =
  the server never idle-waits.
- **`Idle:Commits > 0`** = the kernel idle task ran ⇒ **nothing runnable** ⇒ *under-driven / waiting
  for client requests*, the OPPOSITE of memory reclaim (reclaim keeps the CPU BUSY in kernel).
- **Kernel share** = `TB:System:Commits / Commits` per snapshot. Modest (3–13%) is normal; a
  sustained spike *with* zero idle and stalls would point at kernel-bound work (reclaim/IO).
- The per-unit **Diagnostics** table in `RunResultCommand.log` summarises it: `idle%`, `spin% (β)`,
  `useful%`, and a CPI-stack (`frontend/branch/backend/mem_local/mem_remote/sbdrain/spin/...`),
  plus `L1D-MPKI`/`L2-MPKI`/`MLP`. Memory-bound shows as high `mem_local`/`mem_remote` CPI and high
  MPKI; idle shows as `idle% > 0`.

Verdict pattern that means **no memory pressure**: `WFI>0=0`, modest kernel share, low MPKI,
`idle%`=0 on working windows. (Caveat: runs don't capture guest `free`/`meminfo`; this is the
µarch fingerprint, not a direct RAM readout — for certainty add a `free -m`/`vmstat` dump to the
load/FW expect.)

## 4. Host-side memory / shm / swap during (or just after) a run

```bash
free -h; df -h /dev/shm; vmstat 1 3
```
- **`free -h`** → `Mem: used` = active RAM; `available` = real headroom; `Swap: used` = swapped out.
- **`df -h /dev/shm`** → the docker `--shm-size` tmpfs; `Used` is PDES rings (`/dev/shm/pdes_*`) +
  container shm. (Container's own view: `docker exec <c> df -h /dev/shm`.)
- **`vmstat 1 3`** → swap **activity**: watch `si`/`so` (KB/s). Nonzero `so` under load = actually
  swapping (the real pressure signal). `swpd` is the static total, not activity.

Inspect the qemu processes (dev container is `--pid=host`, so host `ps` sees them — **read-only
`ps` only; NEVER blanket-`pkill` qemu, it would hit other users' runs**):
```bash
ps -eo pid,rss,vsz,etime,args | grep "[q]emu-system"
```
- **`-m 32768` is a CEILING, not a reservation.** It maps 32 GB of *virtual* address space (**VSZ
  ≈ 35 GB**), but physical RAM is backed lazily on first touch. So actual host cost = **RSS**.
- Two `-m 32G` guests do NOT cost 64 GB unless both have *touched* all of it. RSS << VSZ is normal
  and healthy. (Real example: WS single RSS 17.4 GB after loading its 14 GB Solr index — which is
  exactly why `memory_gb: 16` was marginal and 32 GB is the right call.)
- RSS-0 / VSZ-0 lines in the grep are the `gdb` wrappers or your own grep, not guests.
- `%CPU ≈ 100` per guest is expected for `is_parallel: false` (single-thread RR/TCG), not a hang;
  cross-check `etime` + fresh log mtimes to confirm progress.

## 5. Single-node vs multi-node parity — what the CLI derives, what should match

Single-node hosts BOTH server + client in one machine (`doubled_vcpu`); multi-node is two machines
(node-0 server, node-1 phantom client). For a fair comparison the **server's** µarch must be
identical; only the client being remote (wire latency) should differ. Per-machine derived values
(sources: `commands/config.py` `create_simulation_context` + `get_ipns_per_core`; `commands/qemu.py`
`get_qemu_base_args`/`quantum_args`; semikraken width-halving in
`flexus/core/components/ComponentManager.cpp`):

| Quantity | Single | Multi n0 (server) | Multi n1 (client) | single == Σ(multi)? |
|---|---|---|---|---|
| `-smp` vCPUs | 2 | 1 | 1 | ✅ should be DOUBLED |
| core_info.csv ipns rows | server + client | server | client | ✅ exact union |
| detail-modeled cores | 1 (semikraken halves 2→1) | 1 (knotty) | 0 (phantom) | ✅ server modeled once |
| memory `-m` | should be Σ | per-machine | per-machine | ⚠️ should be DOUBLED (else server starved) |
| L2 total / slices | 1 slice | 1 slice | 0 | ✅ EQUAL — do NOT double |
| directory_set | 512·core_count | same | — | ✅ equal |
| mem-controllers | 1 (shared by 2 vCPUs) | 1 (dedicated) | 1 | ⚠️ shared vs dedicated |

Rules of thumb:
- **Cores: doubled is correct.** Verify: single-node `core_info.csv` is exactly the union of the
  two multi-node nodes' `core_info.csv` (`cat <exp>/run/core_info.csv`).
- **Cache: EQUAL is correct — never double it.** `l2_set` scales with `core_count` (not `-smp`),
  and semikraken halves the system width back to 1, so the single-node server's L2 == the
  multi-node server's L2. The client is phantom in BOTH (no cache traffic), so there's nothing to
  add. Doubling single-node L2 would unfairly inflate its U-IPC. Verify live: each timing
  checkpoint has the same slice count — `ls <exp>/run/partition_0/result_0/*-L2-cache-slice.json`.
- **Memory: should be the SUM of the two machines.** `-m` is fixed per machine, so single-node
  (hosting server+client) on the per-machine value starves the server vs the multi-node server's
  dedicated RAM. Bump single-node `memory_gb` to ~2× (see §4 for confirming RSS vs ceiling).
- **Different simulator single vs multi is intended:** single-node doubled_vcpu → `libsemikraken`
  (phantom client in-machine); multi node-0 → `libknottykraken`; multi node-1 phantom →
  `libphantomkraken`. Not a bug.

## 6. Smell-test the phase logs (benign vs real)

`grep` each phase's `*.log`/`*.err` + `expect_log.txt` + `qemu_serial.log`:
- **Real problems:** `out of memory|oom-kill|cannot allocate` (guest OOM), `segfault|Aborted|core
  dumped`, `Unable to set parameter|no parameter named` (a flexus.set silently defaulted — the
  EstimatedIPC class), missing end markers (`savevm log 7`, `Generate N snapshots`), empty
  where-it-shouldn't-be files.
- **Benign noise (do NOT raise as failures):** Solr/ZooKeeper startup transients (`Connection
  refused`/`SessionExpired` during cloud bring-up), logger *class-name* dumps in a JSON stack-trace
  listing (`"ERROR"`, `RetryExec`, `ErrorHandler` are class names being printed, not errors
  firing), `error: no suitable video mode found` (EFI graphics), `Activating swap devices … [ ok ]`
  (Alpine enabling the swap partition at boot ≠ swapping).
- **Workload success markers (presence = the workload actually did useful work):** web-search
  `NodeDataChanged` (server) + `Ramp up completed` (client); web-serving `Successfully browsed
  Elgg` (real success — `Ramp up completed` alone is just a TIMER, not proof). Zero success lines
  + an error wall = broken workload even if rc=0.

## 7. What this skill deliberately does NOT do

No config edits, no process kills, no re-runs. It explains a finished result. If the examination
turns up a config inequity (e.g. memory not doubled) or a code bug (a silent flexus default),
surface it to the user with file:line + the evidence — don't fix it from here.
