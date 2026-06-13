# MULTI_NODE.md — QFlex multi-node and PDES

This document explains how QFlex simulates multiple machines together: the PDES wire, WWT quantum sync, where virtual time comes from, how sampling units stay aligned across nodes, distributed snapshots, and what each submodule contributes. This is the **comprehensive** doc; each submodule has a smaller `MULTI_NODE.md` focused on its own role and how it leans on the others.

For the four-phase pipeline (emulation → FW → sampling-unit selection → timing) and the sampling vocabulary (population / sample / sampling unit) see [CLAUDE.md](CLAUDE.md). This doc assumes you already know that vocabulary.

## Big picture

Multi-node QFlex = **N independent QEMU processes**, one per simulated machine, all running on the same host, glued together host-side by **PDES (Parallel Discrete-Event Simulation)** via POSIX shared-memory rings under `/dev/shm/pdes_*`. Inter-machine guest packets never touch a real OS network stack — they travel through shm.

- **Node 0 is the master.** `is_master_node()` ⇔ `node_number == 0`. Single-node uses `node_number == -1`.
- **Master must boot first.** It clears stale `/dev/shm/pdes*` files at startup; non-master nodes do not. If a non-master starts first, you silently reuse a previous run's state. This is now enforced in Python via per-leaf sentinel files plus the `wait_for_nodes: list[int]` field — see [Python-side dispatch](#python-side-dispatch-sub_experiments--mpprocess) below.
- The same `net/pdes-*.c` source files exist in **both** [parallel-qemu/](parallel-qemu/) and [qemu/](qemu/). Multi-node works in both functional warming (parallel-qemu) and timing simulation (qemu); only one fork is in play at a time, depending on phase.
- Each node also gets its own copy of the qcow2 disk image, suffixed `-node<N>` ([commands/config.py:185](commands/config.py#L185) `copy_image_for_node`).

## Phase coverage

Multi-node is on for the entire execution of every phase that runs guests. In practice **what dominates is the timing-phase sampling-unit window** — that's where the most cross-node coordination happens, because Flexus and PDES interact tightly there.

## Per-node config

`ExperimentContext` ([commands/config.py:121-135](commands/config.py#L121)) holds the multi-node config as **four parallel lists, all the same length**, ordered consistently:

| Field | Type | Meaning |
|---|---|---|
| `node_number` | `int` | This node's id. `0` = master, `-1` = single-node. |
| `neighbor_node_list` | `list[int]` | The `node_number`s this node has a link to. |
| `latencies_ns_list` | `list[int]` | One-way wire latency to each neighbour, ns. |
| `syncs_list` | `list[str]` | `'true'` / `'false'` strings (NOT bools). Whether to require WWT sync messages on this link. |
| `pdes_net_devs` | `list[str]` | Guest-facing NIC model per link: `'e1000'` or `'virtio-net-pci'`. |

Length-asserts and a "must set `node_number` if any list is non-empty" assert live in `create_experiment_context` ([commands/config.py:519-527](commands/config.py#L519)).

`syncs_list` controls **WWT message-level sync** on a link, not sampling-unit alignment — see [Sampling-unit alignment](#sampling-unit-alignment) below.

## Shared-memory naming

Per neighbour, two unidirectional rings (send + receive). Names come from `get_shm_names()` and `setup_nic_args()` ([commands/config.py:155-163, 351-396](commands/config.py#L155)):

```
pdes_<from>_to_<to>[partition_<P>][idx_<I>_]
```

The `partition_<P>` and `idx_<I>` suffixes appear when running parallelised partitions of the timing phase — they keep concurrent sampling-unit runs from clobbering each other's rings. After a crash, [clean_up.sh](clean_up.sh) wipes `/dev/shm/pdes*` and kills lingering qemu processes. Use it before retrying.

## Wire format

[parallel-qemu/include/net/pdes-communicator.h](parallel-qemu/include/net/pdes-communicator.h):

```c
#define MAX_MSG_SIZE 2048
#define RING_SIZE    8192

#define MSG_TYPE_NORMAL          0
#define MSG_TYPE_SYNC            1
#define DRAIN_START              3
#define DRAIN_END                4
#define CHECKPOINT_INIT_STEP     5
#define END_OF_EMULATION         6

typedef struct {
    uint64_t ts_ns;          /* virtual-time timestamp */
    uint32_t len;
    uint8_t  data[MAX_MSG_SIZE];
    uint8_t  type;
} Message;
```

Ring depth = 8192 entries, max payload = 2048 bytes. `ts_ns` is in **virtual time** (ns) — that's what makes this discrete-event simulation rather than wall-clock messaging.

## Engine + WWT (Wisconsin Wind Tunnel)

Two layers, both singletons per QEMU process:

- **`PDESEngine`** ([parallel-qemu/include/net/pdes-engine.h](parallel-qemu/include/net/pdes-engine.h), [parallel-qemu/net/pdes-engine.c](parallel-qemu/net/pdes-engine.c)) — owns the comm rings, virtual-time bookkeeping, drain-on-savevm, and a 5 µs `QEMU_CLOCK_HOST` poll timer that pulls messages off the recv ring.
- **`PDESWWT`** ([parallel-qemu/net/pdes-wwt.c](parallel-qemu/net/pdes-wwt.c)) — wraps the engine and adds Wisconsin Wind Tunnel quantum sync.

WWT in one paragraph: the simulator advances virtual time in **quanta**. The MNQ (multi-node quantum) on a link is the link's wire latency: `wwt->quantum_ns = latencyns` (qemu-pdes/net/pdes-wwt.c). Within a quantum, a node may freely send packets timestamped within that window; at each quantum boundary, neighbours exchange `MSG_TYPE_SYNC` messages so no node runs ahead and produces a packet that should already have been delivered to a neighbour. `current_quantum_round` and the `sync_counts` hash table track the protocol. `quantum_size` (the YAML/CLI knob) is the **PWQ** (intra-machine) quantum, surfaced to QEMU by `quantum_args()` in [commands/qemu.py](commands/qemu.py):

- **boot/load/FW (parallel-qemu)**: `-icount shift=0,align=off,sleep=off,q=<quantum_size>,check_period=<quantum_size × check_period_quantum_coeff>` (or `-quantum size=...` when `is_parallel`)
- **Timing (qemu)**: bare `-icount shift=0,align=off,sleep=off` — no quantum/check_period; virtual time comes from the Flexus tick, and the MNQ is `latencyns` as above. `quantum_size` never reaches the timing phase.

`latencyns` (per-link) adds wire latency to outgoing message timestamps. `first_sync_virtual_time` is computed once at engine create time to reconcile checkpoints that have different virtual-time origins (e.g. when each node was independently checkpointed during FW and resumed for timing).

## Virtual-time source — the headline

PDES asks "what time is it?" at every send/recv. The answer comes from a **different submodule depending on the phase**:

| Phase | Binary | Virtual-time source | Mechanism |
|---|---|---|---|
| 1–3 (emulation, FW, sample selection) | [parallel-qemu](parallel-qemu/) | `-icount shift=…,q=…` | QEMU's instruction count translated to ns by the icount machinery. |
| 4 (timing) | [qemu](qemu/) + [middleware](qemu/middleware/) + [flexus](flexus/) | **Flexus cycle count** via the middleware `tick` | `flexus->tick()` callback updates QEMU's notion of virtual time on every cycle; PDES reads that. |

This is the single most important thing to know about multi-node QFlex: **in the timing phase, Flexus is the clock**. Any change to how Flexus drives `tick` (see Flexus's `features/multi-node` branch — recent work on tick-based cycle counting) is by definition a multi-node change.

## Pause is bidirectional

When WWT decides this node has run past a sync boundary, the middleware can pause **both** QEMU's main loop **and** Flexus. That's why `pause`, `resume`, and `is_paused` are part of `FLEXUS_API_t` ([qemu/middleware/libqflex/libqflex-legacy-api.h](qemu/middleware/libqflex/libqflex-legacy-api.h)):

- Without Flexus honouring pause, the timing model would race ahead while the engine waits for a neighbour, and virtual time would be wrong by the time the neighbour caught up.
- Likewise, the middleware exposes `PauseStatusCallBack` to the engine ([parallel-qemu/include/net/pdes-engine.h:33](parallel-qemu/include/net/pdes-engine.h#L33), `is_waiting_for_quanta`) so PDES knows when a peer (and thereby this node's tick path) is currently blocked.

This was the core of the recent work on the `feature/multi-node` branch in both [qemu/](qemu/) and [qemu/middleware/](qemu/middleware/).

## Send / receive flow

Guest TX (this node sends a packet to a neighbour):

```
guest NIC driver
  → pdes_net_receive   (parallel-qemu/net/pdes-netdev.c:21)
  → wwt_send           (parallel-qemu/net/pdes-wwt.c:143)
  → pdes_comm_send     (parallel-qemu/net/pdes-communicator.c)
  → /dev/shm/pdes_<self>_to_<neighbour>
```

Guest RX (a neighbour's packet arrives):

```
PDESEngine 5 µs poll timer
  → pdes_comm_recv     (drains shm recv ring)
  → wwt_recivied_callback   (parallel-qemu/net/pdes-wwt.c:156)
  → pdes_recv_callback (parallel-qemu/net/pdes-netdev.c:29)
  → qemu_send_packet
  → guest NIC
```

Same code path, same source files, in both QEMU forks.

## Distributed snapshot

[parallel-qemu/net/pdes-checkpoint.c](parallel-qemu/net/pdes-checkpoint.c) and [parallel-qemu/include/net/pdes-checkpoint.h](parallel-qemu/include/net/pdes-checkpoint.h):

- Every send schedules a delivery via `pdes_inflight_add`; every successful deliver removes it.
- On `savevm`, the engine drains across all nodes (`DRAIN_START` / `DRAIN_END` messages), serialises the in-flight array to JSON, and persists the per-node CPU+memory state separately via [qemu/middleware/snapvm-external/](qemu/middleware/snapvm-external/).
- On `loadvm`, `pdes_inflight_restore_and_schedule` reschedules the previously in-flight messages so the run resumes in a coherent state.
- `END_OF_EMULATION` is the corresponding clean-shutdown protocol — nodes notify each other before exiting.

The checkpoint takes effect at **a quantum-aligned virtual time** — see next section.

## Sampling-unit alignment

Each node has its own sampling unit, but **virtual time is global**, so the sampling units across nodes line up. That alignment comes from the **quantum boundary**, not from `syncs_list`:

- `syncs_list = 'true' | 'false'` per link controls whether WWT *message-level* sync (the `MSG_TYPE_SYNC` exchange) is required on that link.
- Sampling-unit start/end always happens at a quantum boundary, regardless of `syncs_list`.

That's also why WormCacheQFlex doesn't have to know about multi-node: when it fires `savevm_cb`, it just signals "snapshot now" and yields to PDES, which lands the actual checkpoint at the next quantum-aligned virtual time. See [WormCacheQFlex/MULTI_NODE.md](WormCacheQFlex/MULTI_NODE.md).

## Per-submodule responsibilities (the "hub" view)

One paragraph each. Each submodule has its own `MULTI_NODE.md` for the full story.

- **[parallel-qemu/](parallel-qemu/)** ([MULTI_NODE.md](parallel-qemu/MULTI_NODE.md)) — owns the canonical `net/pdes-*.c` source. Provides virtual time during phases 1–3 via icount. Loads [WormCacheQFlex](WormCacheQFlex/) as a `-plugin`. The fork that the `multi` Typer command launches.
- **[qemu/](qemu/)** ([MULTI_NODE.md](qemu/MULTI_NODE.md)) — carries an in-sync copy of the same PDES code. Used in phase 4 (timing). Provides virtual time via Flexus cycle count surfaced by the middleware tick — **not** icount.
- **[qemu/middleware/](qemu/middleware/)** ([MULTI_NODE.md](qemu/middleware/MULTI_NODE.md)) — owns the pause handshake (`pause`/`resume`/`is_paused` in `FLEXUS_API_t`) and the virtual-time path (Flexus's `tick` callback into QEMU). `snapvm-external` writes the per-node CPU+memory state during distributed snapshots.
- **[flexus/](flexus/)** ([MULTI_NODE.md](flexus/MULTI_NODE.md)) — **is the clock** in the timing phase. Implements pause/resume/is_paused so PDES (via the middleware) can stall it on a sync boundary. Cycle count from `tick` is what becomes virtual time for PDES.
- **[WormCacheQFlex/](WormCacheQFlex/)** ([MULTI_NODE.md](WormCacheQFlex/MULTI_NODE.md)) — multi-node-naive. The only multi-node-relevant moment is signalling "snapshot now" and yielding to PDES; PDES picks the quantum-aligned virtual time the snapshot lands at. NUMA in WormCache code is intra-chip, not multi-node.

## Python-side dispatch: `sub_experiments` + `mp.Process`

A single `./qflex <phase> -c tests/realrun/multi.yaml` now runs every node in one invocation. The YAML's unnamed `experiment_context` (the group node) carries `_deps_.sub_experiments` listing the per-node leaves; the DI graph builds them all up front. At execute time, `Executor._execute_group` ([commands/executer.py](commands/executer.py)) spawns one `multiprocessing.Process` per sub-experiment whose target sets `executor.experiment_context = sub` and re-enters `executor.execute()`. Each child process re-uses the parent executor (pickled via `mp.Process`) — no executor cloning, no kwargs stashing, no `ParallelExecutor` (that class was removed; this is the universal mechanism for any axis of parallelism).

Per-leaf ordering uses Python-side sentinels under `<group_folder>/.sentinels/`. Each leaf's `_execute_leaf`:

1. Polls `wait_for_nodes` upstream `.started` sentinels (`_wait_for_sentinels`).
2. Calls `experiment_context.prepare_for_execution()` (folder setup + nic args; replaces the side effects that used to live in `create_experiment_context`).
3. Touches its own `<phase>_part<P>_idx<I>_node<N>.started` sentinel.
4. Runs the bash via `subprocess.run`.
5. Touches `.done` on success.

The `_part<P>_idx<I>` suffixes appear when `partition_number` / `idx` are set on the context — so for `run-partition`, two nodes coordinate per-(partition, idx) leaf pair, not per-phase. `wait_for_nodes` is a generic primitive: master sets `[]`, node 1 sets `[0]`, node 3 can set `[2]`, etc. — chained orderings without code changes.

Both `RunPartitionCommand` (per-partition fan-out) and `RunSinglePartitionCommand` (per-idx sequential) now route through this same machinery. The full tree at `./qflex run-partition -c <multi.yaml>` is `top group → node ctx (per-partition fan-out) → partition ctx (per-idx sequential) → idx ctx (RunIdxCommand leaf bash)`.

See [tests/realrun/multi.yaml](tests/realrun/multi.yaml) for the canonical YAML, [tests/](tests/) (one `test_<phase>.py` per pipeline phase, all using the shared `assert_two_node_master_first` helper from [tests/conftest.py](tests/conftest.py)) for the assertions about ordering, and the `executor` and `multi-node` skills for the implementation details.

## Operational gotchas

- **Master must boot first.** Otherwise non-master nodes will silently reuse stale shm rings from a previous run. Enforced in Python now via the `wait_for_nodes` sentinel mechanism above; the master's leaf touches its `.started` sentinel before any waiter can proceed.
- **`--shm-size=128g` and `--pid=host`** on the parent's docker run are required for multi-node. Smaller shm = silent breakage.
- **After a crash, run `./clean_up.sh`** before retrying — it removes `/dev/shm/pdes*` and kills lingering qemu processes.
- **One `./qflex` invocation per phase, all nodes.** Use a multi-node YAML (see [tests/realrun/multi.yaml](tests/realrun/multi.yaml)) and the executor fans out automatically. The legacy `./qflex multi` ([commands/multinode.py](commands/multinode.py)) is a single-node `gdb --args ./qemu-system-aarch64 ...` wrapper — kept for one-off debugging, but the dispatch path is the recommended way.
- **e1000 vs virtio-net-pci.** virtio-net-pci is cheaper — less I/O work simulated/emulated. Pick e1000 for higher fidelity, virtio for speed.
- **Flag types are strings.** `syncs_list` entries are `'true'` / `'false'` strings, not Python bools.
- **MAC addresses are derived from `node_number`.** [commands/config.py:373](commands/config.py#L373): `mac=52:54:00:aa:bb:<node_number*10 + i>` — guarantees uniqueness across nodes for up to 10 neighbours per node.
- **Telnet monitor port:** if `use_telnet_monitor` is on, the port defaults to `55558 + node_number` so multiple nodes don't collide. For Path A (interaction_script on boot/load), a separate **serial** telnet port defaults to `55600 + node_number` — see the `boot-load-interactive` skill for the rationale (monitor-on-telnet for savevm; serial-on-telnet for the guest console; never multiplexed via Ctrl-A C when an `expect` script is in the loop).
- **`savevm` over the monitor is master-only under multi-node.** PDES `savevm` triggers a `DRAIN_START` / `DRAIN_END` coordination across every node and persists per-node CPU+memory state as part of the distributed snapshot. Node 0 sends the command; non-master nodes use a *different* `interaction_script` whose job is to keep their QEMU alive (so they can participate in the drain) until the master finishes — typically by polling a sentinel file the master writes after `savevm` returns. Wiring the same `interaction_script` to every leaf when a snapshot is involved is a bug. See a `tests/realrun/` savevm-create fixture for the canonical split (`boot_create_and_savevm_master.exp` for node 0, `boot_create_and_wait.exp` for the rest) and the `boot-load-interactive` skill for more.

## See also

- [parallel-qemu/MULTI_NODE.md](parallel-qemu/MULTI_NODE.md) — owner of the canonical PDES code; FW-phase clock source.
- [qemu/MULTI_NODE.md](qemu/MULTI_NODE.md) — timing-phase QEMU; in-sync PDES copy; honours pause from PDES.
- [qemu/middleware/MULTI_NODE.md](qemu/middleware/MULTI_NODE.md) — the pause handshake and virtual-time path between QEMU and Flexus.
- [flexus/MULTI_NODE.md](flexus/MULTI_NODE.md) — the cycle-count clock that drives PDES in the timing phase.
- [WormCacheQFlex/MULTI_NODE.md](WormCacheQFlex/MULTI_NODE.md) — yields to PDES on snapshot; otherwise multi-node-naive.
