---
name: multi-node
description: QFlex multi-node simulation AND its control plane — the PDES wire, WWT quantum sync, virtual-time sourcing (icount in FW, Flexus tick in timing), distributed checkpoints, and the simulation-exit handshake. Read this whenever you touch how multiple full-system QEMU nodes coordinate: the sync/barrier, /dev/shm/pdes_*, neighbor nodes, master node, sampling-unit alignment, pause/resume across QEMU and Flexus, AND specifically how checkpoints are taken across nodes or how a node decides to stop/clean up (the control plane that rides the sync message). Trigger on: multi-node, PDES, WWT, quantum/barrier sync, checkpoint coordination, savevm across nodes, can_stop / exit / terminate / cleanup, or any edit to qemu-pdes/net/*.
---

Important manual note from author to be kept in the future unless removed by permission. Remember that right now the engine is the one processing CTRL messages, but WWT is the one writing them at the end of quantum due to SOLID rules. Useful to remember where to look. Needs to be made more generalizable when expanding beyond WWT like synchronous operations.

For the cross-cutting topology overview (PDES rings, WWT quanta, virtual-time sourcing per phase,
per-node shm naming, distributed-snapshot intent, per-submodule roles) read
[../../../MULTI_NODE.md](../../../MULTI_NODE.md).

> **The control plane below is the current source of truth.** `MULTI_NODE.md` (and the per-submodule
> `MULTI_NODE.md` files) still describe the OLD per-message control protocol — `DRAIN_START`/`DRAIN_END`,
> `CHECKPOINT_INIT_STEP`, `END_OF_EMULATION`, `INTENT/PERMISSION` — which **no longer exist**. All
> coordination now rides the SYNC message via `CTRL_*` bits. Trust this file for sync/checkpoint/exit.

## One idea: all control rides the SYNC (barrier) message

There are only two wire message types (`qemu-pdes/include/net/pdes-communicator.h`):
`MSG_TYPE_NORMAL` (guest packets) and `MSG_TYPE_SYNC` (the per-quantum WWT barrier message). Every
coordination signal is a bit in the SYNC payload, so it is delivered and acted on **at the quantum
barrier** (when `sync=true`) or **immediately via the 5µs poll** (when `sync=false`) — never as a
free-floating message that can race the barrier.

SYNC payload: `[round:uint64][ctrl:uint8]` and, only when `CTRL_CKP_REQUEST` is set,
`[format:SnapshotFormat][name:"QPDES<snapshot>\0"]`. `ctrl` bits:

| bit | name | direction | meaning |
|---|---|---|---|
| 0x01 | `CTRL_CKP_REQUEST` | master→peer | checkpoint at THIS sync's round; name+format follow |
| 0x02 | `CTRL_CKP_INIT`    | peer→master | this peer is warmed and ready to checkpoint (init_warmed) |
| 0x04 | `CTRL_READY`       | peer→master | this peer's Flexus is ready to stop |
| 0x08 | `CTRL_CLEANUP`     | master→peer | everyone is ready — terminate |

`send_sync` (`qemu-pdes/net/pdes-wwt.c`) builds the SYNC each quantum and ORs in whatever bits apply;
`process_message` (engine.c) / `wwt_recivied_callback` (wwt.c) act on them on receipt. A SYNC is
emitted **every quantum even when `sync=false`** so control can still flow (TODO in `quanta_sync`: emit
only when carrying a CTRL_* bit while off). `sync_count` is only incremented when `should_sync`.

## Checkpoints (master-coordinated, same quantum on every node)

- A snapshot is requested by `save_snapshot()` (`migration/savevm.c`, both forks). In multi-node it does
  NOT save immediately: it sets `needs_to_checkpoint`, `checkpoint_name`, `notified_neighbors=false`,
  `checkpoint_quantum_round`, and returns false (defer to the barrier). The FW trigger is the WormCache
  plugin → `qemu_plugin_savevm` on **both** nodes; init_warmed is `qemu_plugin_notify_fully_warmed` →
  `finish_initiate_checkpoint`.
- **Only the master announces.** `send_sync` sets `CTRL_CKP_REQUEST` (with a `QPDES`-prefixed name +
  format) when `engine->master && needs_to_checkpoint && !notified_neighbors`, commits
  `checkpoint_quantum_round = round of that sync`, and latches `notified_neighbors`. The `engine->master`
  gate is essential: in FW the peer's own `save_snapshot` also set `needs_to_checkpoint`, so without it
  the peer would self-announce, latch `notified_neighbors`, and then refuse to adopt the master's
  request — keeping its own non-`QPDES` name, which `validate_checkpoint` then skips.
- **Peer adopts** in `wwt_recivied_callback`: on a SYNC with `CTRL_CKP_REQUEST` (and `!notified_neighbors`)
  it sets `needs_to_checkpoint`, `notified_neighbors`, `checkpoint_quantum_round = msg_round`, and
  `checkpoint_name = the QPDES name`. It can't cross the announced round without consuming that very
  sync, so it always learns the round in time and aligns to the master's round.
- Both schedule `create_checkpoint_bh` in `sync_checkpoint_check` when
  `needs_to_checkpoint && notified_neighbors && current_round == checkpoint_quantum_round`.
  `sync_checkpoint_check` holds the quantum (returns true, reschedules) until the bh saves — progression
  pauses until the checkpoint is done. Same agreed round ⇒ same universal time (`R*quantum_ns`) ⇒ a
  coherent distributed snapshot.
- `validate_checkpoint` (pdes-checkpoint.c): a non-master only saves when the name is `QPDES`-prefixed
  (it strips the prefix); that prefix IS the "this is a master-coordinated save" signal — don't remove it.
- `set_checkpoint_values_for_master(const char *name)` is name-parameterized (works for any
  master-initiated checkpoint). `CTRL_CKP_INIT` (`pending_ckp_init`, set in the peer's
  `finish_initiate_checkpoint`) is the init_warmed "I'm warmed" readiness signal.
- init_warmed is terminal for the master: `create_checkpoint_bh` does `pdes_engine_destroy` (→ exit) when
  the name is init_warmed and `engine->master`.
- **Vestigial:** `pdes_drain` + the whole in-flight machinery (`pdes_inflight_*` in pdes-checkpoint.c) is
  dead — `pdes_inflight_add` is disabled and loadvm restore is commented, so it only writes empty JSON.
  It carries a TODO-to-remove (kept for now per the owner). The old `DRAIN_START/END` types are gone.

## Exit / termination (timing phase) — READY → CLEANUP

Replaces the old 3-phase INTENT/PERMISSION/END handshake, which deadlocked: a node could terminate
before flushing a queued END, leaving the peer waiting forever at a quantum. The rule now: **a node
terminates only on a signal it owns or already holds**, so it never waits on a message another node
might fail to send before exiting.

- `can_stop(engine)` (QEMU_API; Flexus polls it at its stopcycle): sets `self_ready` (atomic) and
  returns `master ? cleanup_sent : cleanup_received`. Never blocks.
- **Peer:** `send_sync` emits `CTRL_READY` once when `self_ready`. Peer terminates when
  `cleanup_received`. It keeps sending its per-round syncs until then.
- **Master:** `send_sync` emits `CTRL_CLEANUP` once when `self_ready && ready_peers >= number_of_neighbors`,
  and sets `cleanup_sent` **only after `pdes_engine_send` has put CLEANUP in shm** (so the master, whose
  `can_stop` runs on the Flexus thread, can't exit before the peer can see CLEANUP). Master terminates
  when `cleanup_sent`.
- `process_message`: `CTRL_READY` → `ready_peers++` (master); `CTRL_CLEANUP` → `cleanup_received` (peer).
- Termination path: Flexus `terminateSimulation` (writes `all.measurement.end.log`) → `qemu_api.stop` =
  `libqflex_stop` → exit. `libqflex_stop` just tears down (`destroy_strategy` + `qmp_stop` + `qmp_quit`)
  — no defer, because Flexus only calls it after `can_stop()` is already true. `wwt_sync_check` has a
  main-thread fallback that terminates a node paused at the barrier (when `master?cleanup_sent:cleanup_received`),
  since a paused Flexus can't poll `can_stop`.
- Cross-thread flags (`self_ready`, `ready_peers`, `cleanup_sent`, `cleanup_received`) use `qatomic`
  because `can_stop` (Flexus thread) and `send_sync`/`process_message` (main thread) both touch them.

## Single-node

The whole control plane is multi-node only. With no neighbors `setup_nic_args` emits no `-netdev pdes`,
so `net_init_pdes` never runs and `get_singleton_engine()==NULL`: `save_snapshot` skips the PDES defer
and saves directly, `libqflex_can_stop` returns true immediately, and `send_sync`/`can_stop` are never
reached. Any change to the gates above must preserve this (don't gate single-node behavior on
`engine->master`, which is false for `node_number=-1`).

## Files

`qemu-pdes/net/{pdes-engine,pdes-wwt,pdes-checkpoint}.c` + `include/net/pdes-*.h` (the single shared PDES
source compiled into both `parallel-qemu` and `qemu` via their `net/meson.build`); `migration/savevm.c`
in each fork (the `save_snapshot` defer + `pdes_drain` call); `qemu/middleware/libqflex/libqflex.c`
(`libqflex_can_stop`, `libqflex_stop`). Edit the shared `qemu-pdes/` copy once; it reaches both forks.
