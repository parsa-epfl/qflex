# A.7 step tables — EXECUTED [~ awaiting user review]; A.9 parity fixes applied 2026-09-14; source of truth = tmp_single_pdes_plan.md
# TABLE 2 refreshed to the ACTUAL end state through A.8 (uniform control plane, no
# self_loop_partner, diff-lens restorations). TABLE 1 is the historical A.7 step plan — where the
# two disagree, TABLE 2 + the header file win.

STANDING RULE: review metric = DISTANCE FROM HEAD, not code volume — deletions are diff lines too.
No dead-code/comment cleanup mixed into refactors (user cleans separately); match HEAD's
imperfections unless an approved semantic change forces otherwise; trim only new-in-branch
one-liner additions.

### Phase A.7 — three structs, three files (CURRENT STEP, replanned per user)

Layering (user directive; names of existing functions unchanged):
- **PDESEngine / pdes-engine.c** — the ONE global instance: link registry + process-wide facts +
  pause/play logic (on top of a link, checking all its links; per-link `paused` attribute,
  rel/acq atomics only — no counter, no lock).
- **PDESWWT / pdes-wwt.c** — ONE strategy instance per process, created exactly once at the FIRST
  netdev registration (like the engine). It owns no link information and no per-link state; the
  strategy FUNCTIONS operate on a given link. Today it carries only its once-created identity
  (future strategy-wide knobs land here).
- **PDESLink (NEW struct) / pdes-link.c (NEW file)** — everything that IS the connection: comm
  (each link creates its OWN communicator/rings), shm names, latency,
  first-sync gate + `vt_base` (established when this link and ITS peer connect — per link, never
  global), guest delivery callback, poll timer, paused flag, master/phantom, and the per-connection
  CTRL handshake state (init/ready/cleanup/ckp-done latches + counters), PLUS this link's sync
  state under the strategy (quantum_ns, rounds, sync_counts, barrier/hold timers,
  finished_quantum, notified_neighbors, ckpt_holding, skip_boundry) — per-connection data, so it
  lives on the connection; the WWT functions are what act on it. `should_sync` is strategy-level →
  WWT singleton, checked in once + asserted uniform.
- Every send/recv function gets a ONE-LINE stack-position comment
  (`guest -> net -> netdev -> link -> comm -> shm`, RX mirror).

**Struct field placement (final):**

| Struct | Fields |
|---|---|
| `PDESLink` (NEW) | `comm` (own PDESCommunicator, created in `pdes_link_create`), `shm_send/recv_name`, `link_idx`, `latencyns`, `recv_cb/recv_opaque`, `msg_rec_poll_timer`, `has/sent/received_first_sync`, `vt_base(+set)`, `deferred_normal`, `paused`, CTRL handshake (`init_flag`, `ready_peers`, `cleanup_sent/received`, `ready_sent`, `ckp_init_sent`/`ckp_done_sent` once-latches, `ckp_done_peers`), and per-link sync state operated on by the strategy (`quantum_ns` — genuinely per link via latency, `setup/quantum/sync_check` timers, `sync_counts`, `current_quantum_round`, `finished_quantum`, `notified_neighbors`, `ckpt_holding`, `skip_boundry_check_after_checkpoint`); `should_sync` moves to the WWT singleton (strategy-level, uniformity-asserted) |
| `PDESWWT` | SINGLETON: static instance + static `pdes_wwt_create(sync)` in pdes-wwt.c; the struct itself was RESTORED to the header at its HEAD location (diff-lens rule) with fields {initialized, should_sync}. Created once at the first netdev registration; holds `should_sync` (checked in, later registrations ASSERT equality). Strategy functions take the link they act on. |
| `PDESEngine` | `links` = GPtrArray of `PDESLink*`; `paused_links` counter DELETED; GAINS `master` + `phantom` — machine-level facts, not per-link (user): the per-netdev options are checked in at registration (first link sets them, later links ASSERT equality); phantom check-in seeds `self_ready`; ALSO gains the checkpoint-handshake MACHINE FACTS `ckp_done` (set once post-save) + `ckp_init` (peer warmed) — per user: all checkpointing decisions are per machine; links only keep the announced-on-this-wire once-latch (same pattern as `ready_sent`), so the set-all loops in create_checkpoint_bh/finish_initiate_checkpoint are gone. At every machine/strategy/engine-level check-in site: `// TODO: deliver these once via a separate plugin/config channel instead of repeating them per -netdev` |

**TABLE 1 — what THIS STEP changes (current working tree → proposed):**

| Function | File today | New home | Change |
|---|---|---|---|
| `pdes_pause(void*)` | pdes-engine.c | stays | opaque = PDESLink. Counter-free: `if (qatomic_read(&link->paused)) return;` → `bool first = !pdes_engine_paused();` (edge over OTHER links, before publishing) → `qatomic_store_release(&link->paused, true)` → if `first`, Flexus pause. Single writer (main thread); another link's play can't touch this flag. |
| `pdes_play(void*)` | pdes-engine.c | stays | Mirror: never-paused link is a no-op; `store_release(false)`; `if (!pdes_engine_paused())` Flexus resume (last-link edge by checking all links). |
| `pdes_engine_paused(void)` | pdes-engine.c | stays | OR-scan with `qatomic_load_acquire` over per-link `paused`; `paused_links` field deleted. |
| registry `pdes_link_count/at/index/register/check_unique` | pdes-engine.c | **pdes-link.c** (user: link-related code lives in the link file; the list itself stays engine state, reached via `pdes_engine_get()`) | pure registry now: uniqueness + idx + append (machine-flag check-in moved into `pdes_engine_create`; A.8: no pairing — a local pair is two ordinary peered links) |
| engine aggregates (`may_stop`, `peers_done`, `init_peers_ready`, `commit_round`, drain/defer/restore/bh/fw/finish/can_stop/destroy) | pdes-engine.c | stays | field paths become `link->...` (incl. rounds — sync state is on the link) |
| `pdes_link_create(send, recv, latencyns, cb, opaque, master, phantom)` | — (NEW) | **pdes-link.c** | Allocates `PDESLink`, initializes ONLY link aspects: `pdes_link_check_unique` (pre-mmap), OWN `pdes_comm_create`, names, latency, cb/opaque, gate queue, `vt_base` soft seed, poll timer+arm (master/phantom pass through to registration — they are engine facts). Declared in header. |
| `pdes_engine_wwt_create(...)` | pdes-wwt.c | stays | Entry point (netdev call site keeps the name). Calls the once-constructors `pdes_engine_create(master, phantom)` (KEPT as a function per user — first call builds the engine + checks in machine flags; later calls assert equality) and `pdes_wwt_create(sync)` (same, strategy-level), then adds a link: `pdes_link_create` (link + its sync-state init: `sync_counts`, round=0, `quantum_ns`, `setup_timer`+arm) then registers it. **Returns `PDESLink*`** (the connection handle netdev stores). |
| `wwt_send(PDESLink*, data, len)` | pdes-wwt.c | pdes-wwt.c (stays — user: functions live in the file of the layer they are named for) | Param becomes the link. + stack comment. |
| `pdes_engine_send(PDESLink*, Message*)` | pdes-engine.c | pdes-engine.c (stays) | Param becomes the link; -EAGAIN retry polls ALL local links (A.8: blocked main loop must run the local poll timers' work itself; remote readers drain themselves). + stack comment. |
| `pdes_engine_poll(void*)` | pdes-engine.c | pdes-engine.c (stays) | opaque = PDESLink. + stack comment. |
| `process_message(PDESLink*, Message*)` | pdes-engine.c (static) | pdes-engine.c (stays, static) | Intake + CTRL counters on the link; dispatches to the strategy: `wwt_recivied_callback(link, msg)`. + stack comment. |
| `get_universal_virtual_time(void)` | pdes-utility.c | pdes-utility.c (stays) | vt_base is a MACHINE variable (user final): we only use our own virtual time — `vt_base(+set)` on PDESEngine, seeded at engine create, finalized at first setup_wwt (later setups assert equality). |
| `wwt_recivied_callback`, `setup_wwt`, `send_sync`, `quanta_sync`, `wwt_sync_check`, `sync_checkpoint_check`, `is_waiting_for_quanta`, `get_quantum_time_universal`, gate/bound helpers | pdes-wwt.c | stays | Strategy functions now take `PDESLink*` (the link they act on; per-link sync state lives there); master-role checks read `pdes_engine_is_master()`. + stack comments on `send_sync`/`wwt_recivied_callback`. |
| `pdes_net_receive`, `pdes_recv_callback` (netdev.c), `pdes_comm_send/recv`, `process_message_at_virtual_time` | as-is | as-is | + one-line stack-position comment each; netdev's field becomes `PDESLink *link`. |
| checkpoint.c owner params / `MessageReceiveContext.owner` | as-is | as-is | owner type = `PDESLink*` |

Edits summary: NEW `net/pdes-link.c`; header gains `PDESLink` struct + `pdes_link_create` decl,
`PDESWWT` slims to strategy+`link` ref, `paused_links` deleted; engine.c/wwt.c/checkpoint.c/
utility.c/netdev.c updated per table. Fork meson one-liners for the new file land in each fork's
own Phase-B gate (recorded, NOT done now).

**TABLE 2 — net MR view (last commit → proposed end-state), every function:**

| File @HEAD | Function | Net change vs last commit |
|---|---|---|
| communicator.c | `singleton_comm` global | DELETED (guard generalized to `pdes_link_check_unique`, pre-mmap) |
| communicator.c | `pdes_comm_create` | modified: no singleton guard/exit; ONE instance per link, created by `pdes_link_create` |
| communicator.c | `pdes_comm_send` | modified: full ring returns `-EAGAIN` (retry policy owned by the link layer); + stack comment |
| communicator.c | `pdes_comm_recv` | + stack comment; else unchanged |
| communicator.c | `pdes_comm_init_ring`, `create_message`, `pdes_comm_destroy`, stats fns | unchanged |
| engine.c | `singleton_engine` + `get_singleton_engine` | DELETED → static `PDESEngine` instance + NEW `pdes_engine_get` |
| engine.c | `get_current_virtual_for_normal/destroy_message` | DELETED (dead) |
| engine.c | `pdes_engine_create` | KEPT as function (user): `(master, phantom)` once-init — first call builds engine + checks in machine flags + soft-seeds vt_base + phantom seeds self_ready; later calls assert equality |
| engine.c | `initiate_checkpoint_master`, `schedule_poll` | RESTORED adapted (dead at HEAD too; diff-lens rule — no cleanup mixed into refactor) |
| engine.c | `destroy_strategy` | modified: iterates all links freeing their strategy sync timers |
| engine.c | `pdes_engine_destroy` | modified: `(void)`, terminate-once latch; pending-checkpoint path via engine spec |
| engine.c | `pdes_engine_send` | modified: PDESLink*; -EAGAIN retry polls ALL local links inline (blocked main loop runs the local poll timers' work itself) |
| engine.c | `set_checkpoint_values_for_master` | modified: extern (HEAD parity); arms the ONE engine checkpoint spec (idempotent) + commits round at arm; init_warmed sets fw_exit_after_checkpoint; A.9: resets every link's announce/hold latches at arm (HEAD parity) |
| engine.c | NEW `pdes_engine_reset_link_latches` (static) | A.9: `notified_neighbors`+`ckpt_holding` = false on all links; called at both arm sites (here + `pdes_savevm_defer`) |
| engine.c | `process_message` | modified: `(PDESLink*, Message*)`, per-link CTRL counters, dispatches `wwt_recivied_callback(link, msg)` first; STAYS in engine.c (layer rule); A.8: READY/CKP_DONE counted if self is master, CKP_INIT counted always (fires master arm when master_init && init_peers_ready) |
| engine.c | `pdes_engine_poll` | opaque = PDESLink; STAYS in engine.c (layer rule); + stack comment |
| engine.c | `pdes_pause` / `pdes_play` | modified: engine logic over per-link `paused` (rel/acq, single writer); Flexus edge by checking all links; NO shared counter |
| engine.c | `pdes_drain` | modified: `(name, format)`; drains + persists EVERY link (per-link files) |
| engine.c | `finish_initiate_checkpoint` | modified: `(void)`; engine facts — peer sets `ckp_init`; master sets `master_init` AND `ckp_init` (uniform reporting: its links announce too) |
| engine.c | `can_stop` | modified: `(void)`; sets engine `self_ready`; gate = `pdes_engine_may_stop` |
| engine.c | NEW | `pdes_engine_get`; `pdes_engine_is_master` (reads engine->master)/`paused/may_stop/init_peers_ready/peers_done` (A.8: aggregates over ALL links, no exclusions); `pdes_engine_commit_round` (static); `pdes_savevm_defer`; `pdes_engine_restore_inflight`; `pdes_engine_schedule/clear_checkpoint_bh`; `pdes_engine_fw_complete` (registry lives in link.c; `pdes_link_index`/`pdes_engine_set_master_init` wrappers deleted — callers use the public fields) |
| wwt.c | `singleton_wwt_engine` + `get_singleton_wwt_engine` | DELETED |
| wwt.c | `pdes_engine_wwt_create` | modified: once-only WWT-singleton init on first netdev registration; per call = link add via NEW `pdes_link_create`; returns `PDESLink*` |
| wwt.c | `setup_wwt` | modified: extern (HEAD parity); finalizes the ENGINE's `vt_base` at the FIRST link's setup, later setups assert equality (vt_base is a machine variable — user final) |
| wwt.c | `send_sync` | modified: announce reads engine spec at COMMITTED round (master-only, all links); READY/CKP_INIT/CKP_DONE emitted UNIFORMLY on every link by every machine (recipients count only if master); CLEANUP master-only, release = `pdes_engine_peers_done()` over all links (kills `number_of_neighbors=1` threshold); + stack comment |
| wwt.c | `wwt_send` | param → PDESLink; STAYS in wwt.c (layer rule); + stack comment |
| wwt.c | `wwt_recivied_callback` | modified: opaque = PDESLink (per-link sync state on the link); adopt = ONE engine-spec commit (round_committed guard + equal-round assert on duplicates); deliveries owner-tagged; + stack comment |
| wwt.c | `is_waiting_for_quanta`, `get_quantum_time_universal` | modified: extern (HEAD parity); PDESLink param |
| wwt.c | `finish_quantum`, `get_quantum_time_local`, `time_test` global | RESTORED adapted (dead at HEAD too; diff-lens rule) |
| wwt.c | `sync_checkpoint_check` | modified: engine spec + committed round; per-LINK `ckpt_holding`; ONE engine bh when ALL links hold; link self-clears latches + skip_boundry on rewake; A.9: master additionally requires `notified_neighbors` before holding (HEAD gate; fixes sync=false save-before-announce) |
| wwt.c | `wwt_sync_check` | modified: static; spin polls own link; exit gate = `pdes_engine_may_stop()`; terminate-once |
| wwt.c | `quanta_sync` | modified: extern (HEAD parity); PDESLink param; pause/poll on own link |
| wwt.c | NEW | `wwt_link_at_checkpoint_round` (static) |
| wwt.c | PWQ/drift/skew/gate helpers, `PDES_VLOG` | unchanged (param types only) |
| link.c (NEW FILE) | — | NEW `pdes_link_create` + the link registry (`pdes_link_count/at/register/check_unique`) — link's own code only; A.8: register does uniqueness + idx + append, NO pairing (a local pair is two ordinary peered links); engine-/wwt-named functions stay in their layer's file (user rule); `get_universal_virtual_time` stays in utility.c |
| checkpoint.c | `ScheduledMessage` | +`owner` (PDESLink*) |
| checkpoint.c | `pdes_inflight_add/remove` | +owner param (remove matches owner — no cross-link collisions) |
| checkpoint.c | `pdes_inflight_save_json` | +owner; per-link filter + per-link filename |
| checkpoint.c | `pdes_inflight_restore_and_schedule` | replaced by `_link` variant using the owner's own callback; engine loops links via `pdes_engine_restore_inflight` |
| checkpoint.c | `get_json_file_name` | unchanged (legacy = link 0); NEW `get_json_file_name_link` (`_l<k>`, k≥1) |
| checkpoint.c | `pdes_inflight_load_json`, `pdes_inflight_array_free` | extern with `InflightMessageArray` typedef in checkpoint.h (RESTORED to HEAD shape; diff-lens rule) |
| checkpoint.c | `get_number_of_inflight_messages`, `pdes_inflight_get_all` | RESTORED (dead at HEAD too; diff-lens rule) |
| checkpoint.c | NEW | `pdes_inflight_count_link` |
| checkpoint.c | `validate_checkpoint` | modified: engine-level master/init gates; A.8: master-not-ready branch sets `master_init` AND checks in `ckp_init` (uniform reporting) |
| checkpoint.c | `create_checkpoint_bh` | modified: reads/consumes engine spec; ONE save per process; A.8 post-save (fw_exit): `ckp_done` set on EVERY machine + `self_ready` additionally on master (links announce via their `ckp_done_sent` latches); init_warmed legacy path → `pdes_engine_destroy()` |
| utility.c | `process_message_at_virtual_time` | +owner on the remove call; + stack comment |
| utility.c | `get_universal_virtual_time` | STAYS in utility.c (HEAD home); `(void)`; engine `vt_base` frame (machine variable — user final) |
| utility.c | `sync_count_increment/get` | unchanged |
| netdev.c | `pdes_net_cleanup` | `pdes_engine_destroy()` (also fixes the pre-existing type-confusion bug) |
| netdev.c | `PDESNetState` field / `pdes_net_receive` / `pdes_recv_callback` | field becomes `PDESLink *link`; + stack comments; same `wwt_send`/`pdes_engine_wwt_create` call names |
| engine.h | structs | THREE structs: `PDESEngine` (global: links of PDESLink* + process facts + checkpoint spec; no paused counter), `PDESWWT` (SINGLETON strategy instance, once-created, no per-link state), NEW `PDESLink` (connection: comm/names/latency/gate/delivery/paused/CTRL handshake + this link's sync state under the strategy); `master`+`phantom` are ENGINE fields (machine-level), checked in per netdev at registration. Dead fields from HEAD deleted (`needs_sync`, unused timers, `pause_status_*`, `base_diff/base_time_diff/caclulated_time_diff`, `first_sync_time`, `number_of_neighbors(_finished)`, `has_finished`, dup `latencyns`, `boundry_checkpoint_bh`, `pause_bh`, `checkpoint_bh`, `waiting_for_quanta`) |
| engine.h | decls | cross-file surface + HEAD-parity externs restored (diff-lens: setup_wwt/send_sync/finish_quantum/is_waiting_for_quanta/quanta_sync/schedule_poll with PDESLink params); `PDESRecvCallback`/`PauseStatusCallBack` typedefs deleted; NEW engine/registry/`pdes_link_create` decls; A.8: `self_loop_partner` field GONE |

**HARD STOP after Table 1's edits** — nothing beyond them without explicit user approval.
During execution BOTH markdowns stay current: `tmp_table.md` (these tables) and
`tmp_single_pdes_plan.md` (progress ticks; `[x]` only when the user says finished).