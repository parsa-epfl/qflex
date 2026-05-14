# CORE_CLOCKS.md — PWQ (intra-machine / across-cores) barrier and clocks in parallel-qemu

> **Scope — read this first. QFlex has TWO different barriers; this doc covers ONE of them.**
>
> | Term | Scope | What it synchronises | Where it lives | Mechanism |
> |---|---|---|---|---|
> | **PWQ** — Parallel Warming Quantum (this doc) | intra-machine | the **cores / vCPUs inside one full-system node** — i.e. all guest CPUs of a single simulated client/server machine running in one QEMU process | [`parallel-qemu/util/dynamic_barrier.c`](parallel-qemu/util/dynamic_barrier.c) + [`parallel-qemu/accel/tcg/tcg-accel-ops-quantum.c`](parallel-qemu/accel/tcg/tcg-accel-ops-quantum.c) (parallel/MTTCG); [`parallel-qemu/accel/tcg/tcg-accel-ops-rr.c`](parallel-qemu/accel/tcg/tcg-accel-ops-rr.c) (sequential/RR) | `dynamic_barrier_polling_t quantum_barrier` — a single in-process polling barrier; per-vCPU `quantum_budget` exhaustion triggers `EXCP_QUANTUM` → wait |
> | **MNQ** — Multi-Node Quantum | across full-system machines | separate **full-system nodes** — i.e. the distinct simulated client/server machines, each in its own QEMU process, communicating via `/dev/shm/pdes_*` rings | [`qemu-pdes/net/pdes-wwt.c`](qemu-pdes/net/pdes-wwt.c) (`quanta_sync`, `wwt_sync_check`, `pdes_pause`, `pdes_play`) + [`qemu-pdes/net/pdes-engine.c`](qemu-pdes/net/pdes-engine.c) | WWT (Wisconsin Wind Tunnel) quantum-boundary sync: every node's quantum timer fires at the same virtual-time boundary, exchanges `MSG_TYPE_SYNC` over shm, pauses until peer acks, then resumes together |
>
> These two barriers operate at **different scopes** and use **different code**. Don't conflate them:
> - "core X is stuck at the barrier" → almost always **PWQ** (this doc, §2).
> - "nodes are drifting / messages from the past / `quanta_sync` hangs / `pdes_pause` not actually stopping vCPUs" → **MNQ** (in [`qemu-pdes/`](qemu-pdes/)) — for that, see [MULTI_NODE.md](MULTI_NODE.md) and the `multi-node` skill.
>
> Naming note: "PWQ" stands for *Parallel Warming Quantum* — historical, since the design originated in the functional-warming phase. The same PWQ barrier is active in every phase where `parallel-qemu` runs guest code (boot, init-warm, FW, sample-selection). "Node" in MNQ means a *full-system node* (one simulated machine), not a NUMA node inside one machine.
>
> The two barriers couple at exactly one place: the PWQ barrier-leader writes `QEMU_CLOCK_VIRTUAL` (§4); the MNQ layer reads that clock when stamping outgoing messages and gating quantum-boundary sync. The MNQ `engine->paused` is *supposed* to gate PWQ clock advance, but at the time of writing that hand-off is not implemented cooperatively on the PWQ side — see [`multi-node-fixs-temp.md`](multi-node-fixs-temp.md) if it still exists, or the `multi-node` skill for the MNQ side.

This doc explains how `parallel-qemu` enforces PWQ — the quantum *between cores within a single QEMU process* — how that differs between parallel (MTTCG) and non-parallel (RR) modes, how virtual time is sourced in each, and where MNQ reads from it. Read this when you're debugging anything time-related INSIDE ONE MACHINE: missing PWQ barrier releases, drifting per-core timestamps, stuck cores, savevm that won't fire, or weird per-core vtime in checkpoints.

The cross-machine story (PDES wire, WWT/MNQ sync between QEMU processes, distributed snapshots) lives in [MULTI_NODE.md](MULTI_NODE.md). This doc is intentionally about what happens *inside one QEMU process*; the only PDES coupling discussed here is (a) where the PWQ clock is read by MNQ (§4) and (b) where the PWQ barrier triggers the cross-machine drain (§2.4).

For WormCacheQFlex's near-zero role in all of this see §6.

---

## TL;DR

- **Two disjoint code paths.** Parallel uses [`parallel-qemu/accel/tcg/tcg-accel-ops-quantum.c`](parallel-qemu/accel/tcg/tcg-accel-ops-quantum.c) + the polling barrier in [`parallel-qemu/util/dynamic_barrier.c`](parallel-qemu/util/dynamic_barrier.c). Non-parallel uses [`parallel-qemu/accel/tcg/tcg-accel-ops-rr.c`](parallel-qemu/accel/tcg/tcg-accel-ops-rr.c) + upstream icount. No barrier in RR mode — single OS thread, nothing to sync.
- **Mutually exclusive at the macro level.** `quantum_enabled()` is `(quantum_size != 0)` in [`parallel-qemu/include/sysemu/quantum.h`](parallel-qemu/include/sysemu/quantum.h). Parallel mode asserts `!icount_enabled()` at thread start.
- **CLI picks the mode** in [`commands/qemu.py`](commands/qemu.py) `quantum_args()` — `-quantum size=N,check_period=M` (parallel) vs `-icount shift=0,…,q=N,check_period=M` (non-parallel). `is_parallel` is the toggle.
- **`QEMU_CLOCK_VIRTUAL` is the single interface; the writer is what flips.** In parallel mode the barrier leader advances it by exactly `quantum_size` on each release; in non-parallel it's upstream icount per executed instruction.
- **WormCacheQFlex does NOT enforce the quantum.** It only *registers* `pf_periodic_check_cb`; QEMU itself invokes the callback. WormCache is the policy ("should we snapshot now?"), the barrier/RR loop is the mechanism.

---

## 1. Mode selection on the CLI

[`commands/qemu.py:158-170`](commands/qemu.py) `quantum_args()`:

```python
if self.simulation_context.is_parallel:
    qemu_args = f' -quantum size={quantum_size},check_period={int(quantum_size * coeff)} '
else:
    qemu_args = f' -icount shift=0,align=off,sleep=off,q={quantum_size},check_period={int(quantum_size * coeff)} '
```

| Mode | CLI flag | Backend |
|---|---|---|
| Parallel (MTTCG) | `-quantum size=N,check_period=M` | QFlex-added quantum machinery |
| Non-parallel (RR) | `-icount shift=0,…,q=N,check_period=M` | Upstream icount |

`check_period_quantum_coeff` defaults to **53** ([`commands/config.py`](commands/config.py)) — i.e. WormCache is polled every 53 quanta by default. See §4 for what that means.

The timing fork in [`qemu/`](qemu/) overrides `quantum_args()` to always emit `-icount shift=0,…` — quantum-style sync there is Flexus-driven, not parallel-qemu-driven, so the polling barrier and `ip100ns` machinery described here are **parallel-qemu only**.

---

## 2. Parallel (MTTCG) mode — the polling barrier

### 2.1 Single global barrier, dynamic threshold = number of vCPUs

[`parallel-qemu/accel/tcg/tcg-accel-ops-quantum.c:49`](parallel-qemu/accel/tcg/tcg-accel-ops-quantum.c) declares one global:

```c
dynamic_barrier_polling_t quantum_barrier;
```

`quantum_initialize_barrier()` (line 488) calls `dynamic_barrier_polling_init(&quantum_barrier, 0)` — threshold starts at zero. Each vCPU thread joins via `dynamic_barrier_polling_increase_by_1` at line 242 on start and `decrease_by_1` at line 450 on exit. Dynamic on purpose — vCPU hot-plug / unplug adjusts the threshold instead of needing a reinit.

### 2.2 Barrier struct (the wait protocol)

[`parallel-qemu/include/qemu/dynamic_barrier.h:37-59`](parallel-qemu/include/qemu/dynamic_barrier.h) + impl in [`parallel-qemu/util/dynamic_barrier.c`](parallel-qemu/util/dynamic_barrier.c):

- **Ticket lock** (`next_ticket` / `now_serving`): FIFO over the *arrival* protocol. Held only to decide "am I the last thread to arrive?" — not held during waiting.
- **`threshold`**: registered vCPU count.
- **`count`**: threads currently waiting in this generation.
- **`return_value.one_64`**: a single 64-bit atomic packing `barrier_result_t { stop_request:32, generation:32 }`. This is the **broadcast channel** — every waiter spin-polls this word.
- **`histogram[128]`**: one `time_histogram_t` per core, dumped to `quantum_histogram_<cpu_index>.log` on thread exit. Useful post-mortem for load imbalance / debugging long waits.

`dynamic_barrier_polling_wait` ([`parallel-qemu/util/dynamic_barrier.c:169-409`](parallel-qemu/util/dynamic_barrier.c)):

1. Acquire ticket. If you are the **last thread** to arrive (`count == threshold - 1`):
   - Call `increase_quantum_time()` ([`parallel-qemu/softmmu/cpu-timers.c:173`](parallel-qemu/softmmu/cpu-timers.c)) — `timers_state.quantum_set_time += quantum_size`. **This is the only place the global virtual clock advances in parallel mode.**
   - Run `pf_periodic_check_cb` if `current_cycle >= next_check_threshold` (WormCache's "snapshot now?" hook). If it returns true, set `broadcast_stop_request = 1` and `qemu_notify_event()`.
   - If `!runstate_is_running()`, set `stop_request = 2`.
   - Atomically store `{stop_request, generation+1}` to `return_value.one_64` — this releases everyone.
2. Otherwise spin-poll `return_value.one_64`, exit once the generation has advanced. Every ~1M spins, peek at `runstate_is_running()` so a paused VM doesn't spin forever.

### 2.3 Per-vCPU budget — how a core runs out of quantum

[`parallel-qemu/accel/tcg/tcg-accel-ops-quantum.c:240-310`](parallel-qemu/accel/tcg/tcg-accel-ops-quantum.c):

```c
cpu->quantum_generation = dynamic_barrier_polling_increase_by_1(&quantum_barrier);
cpu->quantum_budget = (quantum_size * cpu->ip100ns) / 100;
```

`ip100ns` is per-core IPC (instructions per 100 ns) loaded from `core_info.csv` at startup. Different cores can run at different speeds — see §5.

**Inside the TB**: the translator injects a budget decrement at the top of every translation block (helper `gen_helper_check_and_deduce_quantum` in [`parallel-qemu/accel/tcg/helper-quantum.c`](parallel-qemu/accel/tcg/helper-quantum.c), injected from [`parallel-qemu/accel/tcg/translator.c`](parallel-qemu/accel/tcg/translator.c)). When the budget goes ≤ 0, the helper sets `cpu->quantum_budget_depleted = 1`, the TB exits via `exitreq_label`, and `tcg_cpus_exec()` returns with `EXCP_QUANTUM`.

The thread then waits and replenishes (line 296):

```c
cpu->quantum_budget += (quantum_size * cpu->ip100ns) / 100;
cpu->quantum_generation += 1;
…
cpu_virtual_time[cpu->cpu_index].vts += quantum_size;   // line 432, post-idle path
```

So per-core virtual time `cpu_virtual_time[].vts` advances by exactly `quantum_size` on each barrier crossing, in lockstep with `timers_state.quantum_set_time`.

### 2.4 "Pause on cores" is just `stop_request`

There is **no separate pause API at the barrier level.** Three signals share the same return-value word:

| `stop_request` | Meaning | Set by |
|---|---|---|
| 0 | Normal — next quantum, keep going | — |
| 1 | Snapshot / pause — finish this quantum, drop out of TCG loop | Leader thread when `pf_periodic_check_cb` returns true |
| 2 | Machine not running — bail mid-loop | Leader (sees `!runstate_is_running()`) or any waiter (1M-spin runstate check) |

The PDES distributed-snapshot drain ([`parallel-qemu/net/pdes-checkpoint.c`](parallel-qemu/net/pdes-checkpoint.c)) hooks into the `stop_request=1` path: once all cores have released the barrier and dropped out of the TCG loop, the main loop calls `pdes_drain()` before `savevm` lands. Per-node pause and inter-node drain are coordinated by the same machinery — the barrier itself doesn't need to know PDES exists.

### 2.5 The three wait blocks (don't be fooled — they're different)

`mttcg_cpu_thread_fn` calls `dynamic_barrier_polling_wait` from **three sites**. They look near-identical but each has load-bearing differences:

| # | Lines | Trigger | Loop condition | Continuation |
|---|---|---|---|---|
| A. Post-TB-exit | 276-321 | `tcg_cpus_exec()` returned with `cpu->quantum_budget_depleted` ⇒ `r == EXCP_QUANTUM` | `while (quantum_budget <= 0)` | `goto continue_to_run` re-enters `tcg_cpus_exec()` without re-taking iothread. **Hot path.** |
| B. Pre-atomic-op | 333-374 | TB exit was `EXCP_ATOMIC` — about to call `cpu_exec_step_atomic()` | `while (quantum_budget <= quantum_for_deduction)` (NOT 0) | Guarantees enough budget upfront so the atomic step never crosses a quantum boundary mid-op |
| C. Post-idle catch-up | 386-439 | `qemu_wait_io_event()` returned (vCPU was idle); the idle wait itself depleted the budget | `do … while (quantum_budget <= 0)` | The **only** block that increments `cpu_virtual_time[].vts` directly (line 432); also the only block that can early-detach via `quantum_allow_interrupt_wakeup_inside` (off by default under qflex) |

So the three really are different: A is the normal flow, B is atomic-op pre-flight reservation, C is post-idle reconciliation. The duplication is intentional — each block's exit conditions differ enough that a shared helper would force `if` ladders that obscure the protocol.

---

## 3. Non-parallel (RR / concurrent) mode

[`parallel-qemu/accel/tcg/tcg-accel-ops-rr.c`](parallel-qemu/accel/tcg/tcg-accel-ops-rr.c) — a single host thread round-robin schedules all vCPUs:

- **No `dynamic_barrier_polling_t`.** There is nothing to synchronize between.
- `-icount shift=0,…,q=N` — `q=` value lands in `icount_switch_period` ([`parallel-qemu/softmmu/icount.c`](parallel-qemu/softmmu/icount.c)) and is used as the **per-vCPU budget** in the round-robin scheduler (line 321).
- **Yield is implicit.** Once `cpu_budget` hits zero, the loop moves to the next vCPU. A kick timer (lines 75-92) periodically pre-empts so a stuck vCPU can't starve siblings.
- **Per-core `cpu_virtual_time[].vts` still maintained** for plugin/PDES bookkeeping, but advances *proportionally* to instructions actually executed:

  ```c
  cpu_virtual_time[cpu->cpu_index].vts += cpu->quantum_required * 10000 / cpu->ip100ns;   // line 348
  ```

  This is not the canonical clock in this mode — upstream icount is. The per-core `vts` exists so plugins see consistent per-core time across both modes.
- **Snapshot pacing** still works: lines 330-336 periodically invoke `pf_periodic_check_cb`. The RR loop will yield at the end of the current vCPU's budget and savevm proceeds. No barrier wait — there's nothing parallel to stall.

---

## 4. Virtual time — same `QEMU_CLOCK_VIRTUAL`, two different writers

The single source of truth in QEMU is `qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL)`. In parallel-qemu it can return one of two things ([`parallel-qemu/softmmu/cpu-timers.c`](parallel-qemu/softmmu/cpu-timers.c) gates on `quantum_enabled()` / `icount_enabled()`):

| Mode | Returned virtual time | Where advanced |
|---|---|---|
| Parallel (`-quantum`) | `timers_state.virtual_clock_snapshot + timers_state.quantum_set_time` | `increase_quantum_time()` ([line 173](parallel-qemu/softmmu/cpu-timers.c)), called by the leader thread at the barrier. Step = `quantum_size`. |
| Non-parallel (`-icount`) | Upstream icount: `qemu_icount << icount_time_shift` | Whenever `tcg_cpus_exec()` runs instructions in the RR loop. With `shift=0`, 1 ns per instruction at IPC=1. |

PDES reads through `QEMU_CLOCK_VIRTUAL` ([`parallel-qemu/net/pdes-utility.c`](parallel-qemu/net/pdes-utility.c) `get_universal_virtual_time()`), so PDES is mode-agnostic — the parallel/non-parallel distinction is invisible to it.

### `quantum_size` vs `check_period`

Two different cadences, both controlled by the `-quantum` flag (or `-icount` in RR mode):

- **`quantum_size`** (ns): the *atomic unit of inter-thread virtual time advance*. Every barrier release moves the global clock by exactly this. Per-core budget = `(quantum_size * ip100ns) / 100` instructions. Bound: `assert(quantum_size < 0x7fffffff)`.
- **`check_period`** (alias `quantum_check_threshold`): must be an integer multiple of `quantum_size` (asserted at [`parallel-qemu/softmmu/quantum.c:19`](parallel-qemu/softmmu/quantum.c)). Controls **how often `pf_periodic_check_cb` is asked "snapshot?"** — NOT how often the barrier is hit.

Inside the barrier ([`parallel-qemu/util/dynamic_barrier.c:206-221`](parallel-qemu/util/dynamic_barrier.c)) the leader maintains a `current_cycle` running counter (incremented by `quantum_size` per release) and `next_check_threshold` (advances by `quantum_check_threshold` per callback fire). The callback fires at `current_cycle = N * check_period` for N = 1, 2, … In RR mode the same logic uses `icount_checking_period` instead — same protocol, different driver.

---

## 5. Per-core IPC (`ip100ns`) and `core_info.csv`

What makes the simulator able to model heterogeneous cores running at different speeds while all hitting the same global virtual-time quantum.

### File contract

[`parallel-qemu/accel/tcg/tcg-accel-ops-quantum.c:58-116`](parallel-qemu/accel/tcg/tcg-accel-ops-quantum.c) `quantum_initialize_core_info_table`:

- Path: `-quantum ipns_file=…`; default `core_info.csv` (resolved in the cwd, which is `<exp>/run/`).
- Two header shapes:
  - `ipns` → one column, one row per core; affinity defaults to identity.
  - `ipns,affinity_core_idx` → second column overrides host-CPU pinning via `pthread_setaffinity_np`.
- Stored as `ip100ns = (uint64_t)(ipns * 100)` — integer instructions per 100 ns, so the per-core budget math `(quantum_size * ip100ns) / 100` is a single 64-bit multiply.
- **If `quantum_enabled()` and the file is missing, QEMU exits 1.** RR mode is forgiving — missing file leaves all cores at `ip100ns = 0`.

### How qflex generates it

[`commands/config.py`](commands/config.py) `get_ipns_csv()` writes `<exp>/cfg/core_info.csv` and symlinks it as `<exp>/run/core_info.csv`. **Does not overwrite if present** — hand-tuning is OK; stale-on-YAML-change is the failure mode (delete the CSV to regenerate).

Rows come from `get_ipns_per_core()`:

```python
ipns_primary   = round(primary_ipc   * machine_freq_ghz, 2)
ipns_secondary = round(secondary_ipc * machine_freq_ghz, 2)
ipns_phantom   = round(phantom_ipc   * machine_freq_ghz / min_ipc * primary_ipc, 2)
```

IPC (instructions per cycle) × GHz (cycles per ns) = instructions per ns = `ipns`. Three IPC dials (primary / secondary / phantom) for the three roles: workload cores, co-tenant cores under `is_consolidated`, doubled-vCPU client cores.

### How a vCPU picks it up

[`parallel-qemu/accel/tcg/tcg-accel-ops-quantum.c:204`](parallel-qemu/accel/tcg/tcg-accel-ops-quantum.c) at the top of `mttcg_cpu_thread_fn`:

```c
cpu->ip100ns = core_info_table[cpu->cpu_index].ip100ns;
…
cpu->quantum_budget = (quantum_size * cpu->ip100ns) / 100;
```

Worked example: `quantum_size=1_000_000` ns, core 0 with `ip100ns=150` (1.5 IPC × 1.0 GHz), core 1 with `ip100ns=50`. Per quantum: core 0 = 1.5M instructions, core 1 = 0.5M instructions. Both reach the barrier at their own pace; the slow one will spin waiting for the fast one. At release, both advance `vts` by exactly `quantum_size` — per-core virtual time stays in lockstep even though per-core instruction count diverges. **This is the formal definition of "running cores at different IPC" in the simulator.**

---

## 6. WormCacheQFlex is NOT part of the quantum machinery

Verified by grepping the entire [WormCacheQFlex/](WormCacheQFlex/) tree:

- Zero hits for `barrier`, `dynamic_barrier`, `pdes`, `wwt`, `sync_count`, or any inter-vCPU synchronization primitive.
- Only `quantum` reference is `quantum_checking_callback` in [`WormCacheQFlex/src/chronic/snapshot.rs`](WormCacheQFlex/src/chronic/snapshot.rs) — *registered* with QEMU via `qemu_plugin_register_periodic_check_cb`. **QEMU invokes it from the barrier leader (parallel) or RR loop (non-parallel)**; the plugin does not poll or enforce anything.
- Per-vCPU `vcpu_tb_trans` callbacks instrument TBs (update cache/TLB/BP state) and return immediately. No locking that would gate other vCPUs.
- `savevm_cb` only serialises state; it does not pause cores. Pause / drain is QEMU's + PDES's responsibility.

`WormCacheQFlex/MULTI_NODE.md` says it directly: *"WormCache picks the moment to ask; PDES picks the moment it lands."* The same applies intra-node — WormCache doesn't know how the per-core quantum is enforced either.

---

## 7. Debug checklist — common time-related failure modes

When a time / quantum / sync symptom shows up, walk this list in order:

1. **Mode confusion.** Confirm which file is even running: `is_parallel=True` → parallel mode (`tcg-accel-ops-quantum.c` + barrier). `is_parallel=False` → RR mode (`tcg-accel-ops-rr.c`, no barrier). Symptoms of mode confusion: `g_assert(!icount_enabled())` panic at thread start (means parallel-mode CLI received with icount enabled somehow); per-core budget of 0 (means parallel-mode CLI but `core_info.csv` not found / all cores got `ip100ns=0`).
2. **Threshold mismatch.** Parallel mode: barrier threshold must equal the number of vCPU threads that registered via `increase_by_1`. Symptoms: cores spin forever at the barrier (last-thread never arrives because the threshold is wrong). Inspect via the histogram dump (`quantum_histogram_<i>.log`) at thread exit, or printf the threshold from `quantum_barrier`.
3. **`stop_request` propagation.** Cores stuck in `dynamic_barrier_polling_wait`: check `runstate_is_running()` and the 1M-spin runstate poll. If runstate looks fine but waiters never release, the leader thread either never arrived (threshold issue) or crashed mid-`increase_quantum_time()`.
4. **`check_period` constraint.** [`parallel-qemu/softmmu/quantum.c:19`](parallel-qemu/softmmu/quantum.c) asserts `check_period >= quantum_size && check_period % quantum_size == 0`. If the CLI is computing `check_period = quantum_size * 53.0` (default coeff), that's already an integer multiple — but tweaking `check_period_quantum_coeff` to a non-integer is a likely abort cause.
5. **Snapshot not firing.** `pf_periodic_check_cb` returning `true` is what sets `stop_request=1`. If snapshots don't happen: (a) is WormCache loaded? (b) is its mode `warm` or `ff`? (c) is `check_period` so large that the next threshold hasn't been crossed yet? (d) is the callback actually firing — add a printf and rebuild.
6. **Multi-node drift.** Multi-node sync relies on every node using the **same `quantum_size`**. PDES message timestamps are pulled from `QEMU_CLOCK_VIRTUAL` which advances by `quantum_size` per release; if two nodes have different `quantum_size`, they'll desync deterministically. Inspect: `grep -- '-quantum' <Boot.log>` or `<Load.log>` on each node.
7. **Per-core vtime ≠ global vtime.** In parallel mode they should be equal (modulo the post-idle catch-up window in wait-block C). If they diverge by more than one quantum on a core, that core is probably stuck inside an atomic op or an interrupt path that bypassed the barrier — look for log lines from wait-block B or C.
8. **Atomic op stalling.** If a vCPU hangs at a barrier wait with `quantum_for_deduction` set (wait-block B), the atomic op needs more budget than the per-core quantum can supply in one shot. Bump `quantum_size` or check the atomic op for correctness.
9. **Idle-induced budget loss.** Wait-block C zeros leftover budget after returning from `qemu_wait_io_event`. If a core looks like it "skipped" a quantum, this is by design — the idle period broke the per-core-vtime ↔ quantum invariant and the protocol resyncs by waiting for the next barrier release.
10. **CSV stale-or-missing.** If you regenerate the YAML but the experiment folder already has `cfg/core_info.csv`, `get_ipns_csv()` won't overwrite it. Delete the CSV (and its `run/core_info.csv` symlink) to force regeneration.

---

## 8. Critical files (cite-and-navigate)

Parallel-qemu side (the QFlex-added stack):

- [`parallel-qemu/include/qemu/dynamic_barrier.h`](parallel-qemu/include/qemu/dynamic_barrier.h) — barrier struct + API
- [`parallel-qemu/util/dynamic_barrier.c`](parallel-qemu/util/dynamic_barrier.c) — polling wait, ticket lock, stop_request semantics, histogram per core
- [`parallel-qemu/accel/tcg/tcg-accel-ops-quantum.c`](parallel-qemu/accel/tcg/tcg-accel-ops-quantum.c) — MTTCG thread fn, three wait blocks, per-core budget, `ip100ns` load
- [`parallel-qemu/accel/tcg/helper-quantum.c`](parallel-qemu/accel/tcg/helper-quantum.c) — TB-injected budget decrement
- [`parallel-qemu/accel/tcg/translator.c`](parallel-qemu/accel/tcg/translator.c) — injects the helper
- [`parallel-qemu/accel/tcg/cpu-exec.c`](parallel-qemu/accel/tcg/cpu-exec.c) — `EXCP_QUANTUM` exception path
- [`parallel-qemu/softmmu/quantum.c`](parallel-qemu/softmmu/quantum.c) + [`parallel-qemu/include/sysemu/quantum.h`](parallel-qemu/include/sysemu/quantum.h) — `-quantum` parsing, `quantum_enabled()` macro
- [`parallel-qemu/softmmu/cpu-timers.c`](parallel-qemu/softmmu/cpu-timers.c) — `increase_quantum_time()`, `QEMU_CLOCK_VIRTUAL` mode switching
- [`parallel-qemu/include/qemu/plugin-pf.h`](parallel-qemu/include/qemu/plugin-pf.h) — `cpu_virtual_time[]` per-core array
- [`parallel-qemu/include/qemu/histogram.h`](parallel-qemu/include/qemu/histogram.h) + [`parallel-qemu/util/histogram.c`](parallel-qemu/util/histogram.c) — per-core wait histograms

Non-parallel side (upstream icount + RR scheduler):

- [`parallel-qemu/accel/tcg/tcg-accel-ops-rr.c`](parallel-qemu/accel/tcg/tcg-accel-ops-rr.c) — round-robin loop, `icount_switch_period` budget
- [`parallel-qemu/softmmu/icount.c`](parallel-qemu/softmmu/icount.c) — icount machinery

Mode selection + IPC source (qflex side):

- [`commands/qemu.py`](commands/qemu.py) `quantum_args()` — picks `-quantum` vs `-icount` from `is_parallel`
- [`commands/config.py`](commands/config.py) `get_ipns_csv()` + `get_ipns_per_core()` — writes `core_info.csv`

PDES coupling (only at the edges):

- [`parallel-qemu/net/pdes-engine.c`](parallel-qemu/net/pdes-engine.c) / [`parallel-qemu/net/pdes-utility.c`](parallel-qemu/net/pdes-utility.c) — `get_universal_virtual_time()` reads `QEMU_CLOCK_VIRTUAL` (mode-agnostic)
- [`parallel-qemu/net/pdes-checkpoint.c`](parallel-qemu/net/pdes-checkpoint.c) — distributed drain triggered after barrier releases on `stop_request=1`

WormCache (only to confirm non-involvement):

- [`WormCacheQFlex/src/chronic/snapshot.rs`](WormCacheQFlex/src/chronic/snapshot.rs) — registers `quantum_checking_callback`; everything else is observer state

---

## See also

- [MULTI_NODE.md](MULTI_NODE.md) — cross-node PDES wire, WWT, distributed snapshots. The per-process clock described here is what PDES messages timestamp against.
- [parallel-qemu/CLAUDE.md](parallel-qemu/CLAUDE.md) — broader context on the fast QEMU fork.
- [WormCacheQFlex/CLAUDE.md](WormCacheQFlex/CLAUDE.md) — the plugin that registers `pf_periodic_check_cb`.
