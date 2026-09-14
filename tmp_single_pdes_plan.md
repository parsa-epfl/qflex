# Multi-NIC PDES: single-node self-loop + N NICs per node

> Resumable working plan. Update the Progress section at every step. If a session dies, resume from here.

## Progress

(Tick convention: [x] ONLY when the user says a step is finished. [~] = code written, awaiting user review/approval.)

STANDING RULES (apply to every phase):
- Review metric = DISTANCE FROM HEAD, not code volume — deletions are diff lines too. Never delete
  dead code / comments / unused-but-HEAD-existing API during a refactor; the user does cleanups
  separately, later. Match HEAD's imperfections deliberately unless an approved semantic change
  forces otherwise. New-in-branch one-liner wrappers ARE fair game to trim (pure additions).
- One approved step at a time; HARD STOP after it. Keep this file and tmp_table.md updated at every step.
- The Progress log below is APPEND-ONLY: LATER entries supersede earlier ones (A.5's
  self_loop_partner design and A.6's per-link vt_base are DEAD — see A.7/A.8). For the CURRENT
  end state read `qemu-pdes/include/net/pdes-engine.h` (structs + API) and tmp_table.md TABLE 2
  (refreshed to the executed end state); the original plan prose further down is kept verbatim
  and loses to the Progress log wherever they conflict.

- [~] Phase A — qemu-pdes core CODE WRITTEN (A1 container / A2 node-level decisions / A3 checkpoint / A4 self-loop hazards)
- [~] Phase A.2 — state-ownership audit CODE WRITTEN: node-level facts moved onto PDESNode (in header,
  `pdes_node_get()` accessor): vt_base(+set), self_ready (qatomic; phantom seeds at register),
  master_init, fw_exit_after_checkpoint, checkpoint spec {needs, name, format, round, round_committed}.
  Deleted from PDESEngine: self_ready/master_init/fw_exit/needs/name/format/checkpoint_quantum_round;
  added link_idx (O(1) pdes_link_index) + ckpt_holding (link parked at committed round; self-clears
  latches + skip_boundry on rewake). pdes_node_paused = paused_links counter (qatomic fetch_inc/dec).
  Round now committed AT ARM on the master via pdes_node_commit_round() (current round, +1 if any
  master link already sent this round's sync — old single-link semantics preserved); peer FW arm stays
  uncommitted placeholder; adopt = one node write guarded by round_committed (+equal-round assert on
  duplicates). Announce/READY/CLEANUP skip self-loop links. create_checkpoint_bh reads node spec;
  loops remaining (15): register scans, node aggregates (may_stop/peers_done/init_ready/all-holding/
  commit_round), node events over per-link resources (drain/restore/destroy_strategy/ckp_done latch/
  ckp_init latch), poll_all_links liveness. Awaiting user compile + 2-node regression + approval.
- [~] Phase A.3 — SoC sweep over ALL changes (user directive). Ownership map now:
  `PDESCommunicator` (per connection): the two SPSC rings + fds. `PDESEngine` (per connection):
  ring NAMES, latencyns, recv chain, poll timer, first-sync gate, paused, link role (master side),
  link_idx/self_loop, per-link protocol latches/counters. `PDESWWT` (per connection, sync layer):
  quantum_ns, rounds, sync_counts, timers, should_sync. `PDESNode` (per process): links list,
  paused count, vt_base, self_ready, master_init, fw_exit, checkpoint spec, terminate latch, bh.
  Removed duplications: engine->first_sync_virtual_time (== node vt_base; soft-seeded at first
  register, finalized+asserted at setup_wwt), wwt->latencyns (== engine->latencyns),
  engine->needs_sync (== wwt->should_sync). Deleted dead write-only state: sync/setup/drain/
  checkpoint_initiate timers, pause_status_cb/opaque (+PauseStatusCallBack typedef),
  base_diff/base_time_diff/caclulated_time_diff, first_sync_time (+dead branch in
  pdes_engine_send), number_of_neighbors{,_finished}/has_finished (+dead arithmetic).
  pdes_engine_create shrank to (shm_send, shm_recv, latencyns, cb, opaque, master, phantom).
- [~] Phase A.4 — TWO-STRUCT RESTRUCTURE (user directive: "two structs, one is the engine,
  responsible globally for causality, not per link; one node/link responsible per connection").
  `PDESEngine` is now the ONE global causality engine per process (former PDESNode + link list):
  links GPtrArray, paused_links, vt_base, self_ready, master_init, fw_exit, checkpoint spec/round,
  checkpoint_bh, terminated. `PDESLink` is ONE struct per connection (former per-link
  PDESEngine + PDESWWT merged): comm + shm names + latency + recv chain + first-sync gate + paused
  + master/phantom (link role, process-uniform asserted) + quantum/rounds/sync_counts/timers +
  per-link latches/counters. File split: pdes-engine.c = global engine + registry + engine
  decisions + drain/defer/restore/terminate; pdes-wwt.c = the whole link implementation
  (create/poll/send/sync barrier/checkpoint hold; pause/play + process_message now static there).
  API renames: pdes_node_* → pdes_engine_*; pdes_engine_wwt_create+pdes_engine_create →
  pdes_link_create; pdes_engine_poll → pdes_link_poll; wwt_send → pdes_link_send_packet;
  pdes_engine_destroy → pdes_engine_terminate; can_stop(engine) → pdes_engine_can_stop();
  set_checkpoint_values_for_master → pdes_engine_arm_master_checkpoint; pdes_drain(name, format)
  (engine arg dropped); pdes_inflight_restore_and_schedule compat wrapper DELETED
  (→ pdes_engine_restore_inflight); get_universal_virtual_time(void). PDESRecvCallback typedef +
  two-level recv chain gone. Header now = exact cross-file surface (decl↔def audited both ways).
  Phase-B conversion list (fork call sites, UPDATED names): icount/dynamic_barrier/rr →
  pdes_engine_paused(); savevm → pdes_savevm_defer / pdes_drain(name,fmt) /
  pdes_engine_restore_inflight / validate_checkpoint; pf_api → finish_initiate_checkpoint(void),
  pdes_engine_fw_complete(), pdes_link_count(); libqflex → pdes_engine_can_stop(), destroy_strategy().
- [~] Phase A.5 — per-instance creation chain verified + poll-all removed (user review): creation is
  netdev → pdes_link_create → pdes_comm_create, all per-link instances driven by their own QEMU
  timers. `pdes_poll_all_links` DELETED. Communicator is pure SPSC transport again: `pdes_comm_send`
  returns -EAGAIN on full (no upward calls, no engine include); the retry policy lives on the link
  (`pdes_link_send`), which drains only its stored `self_loop_partner` (the one ring peer this
  blocked thread must service itself; set at register pairing, replaces the `self_loop` bool).
  The barrier spin polls only its own link (partner's sync is already in our recv ring before any
  spin — both quanta_syncs run in the same virtual-timer pass).
- [~] Phase A.6 — vt_base back PER LINK (user correction): the time base is the frame a link agrees
  with ITS peer at the first-sync handshake — per link because nodes (especially N>2) start at
  different times. `vt_base`/`vt_base_set` live on PDESLink (soft-seeded at create, finalized at
  that link's setup — no cross-link assert at setup); `get_universal_virtual_time(PDESLink*)`.
  Frame alignment across links is asserted ONLY where comparability is required: the multi-link
  checkpoint arm (pdes_savevm_defer: equal quanta AND equal vt_base), since the committed round is
  shared engine state that every link must interpret identically.
- [~] Phase A.7 — three structs, three files EXECUTED per approved tables (see tmp_table.md):
  `PDESEngine` (global: link registry, machine flags master/phantom checked in at registration with
  uniformity asserts + plugin TODO, self_ready/master_init/fw_exit, checkpoint spec, terminate latch,
  counter-free pause/play — per-link `paused` flag, rel/acq, single writer, Flexus edge by checking
  all links); `PDESWWT` (singleton strategy, initialized once at first netdev reg; owns `should_sync`
  checked in + asserted uniform); NEW `PDESLink` + NEW `net/pdes-link.c` (connection: own comm/rings,
  names, partner, latency, first-sync gate + per-link vt_base, delivery cb, poll timer, CTRL
  handshake state, per-link sync state) with `pdes_link_create`, the link registry, and
  `get_universal_virtual_time`. Layer rule (user): functions live in the file of the layer they are
  named for — `pdes_engine_send`/`pdes_engine_poll`/`process_message` stay in pdes-engine.c,
  `wwt_send` stays in pdes-wwt.c (all operating on the PDESLink they are given). Constructors kept
  as functions (user): `pdes_engine_create(master, phantom)` + `pdes_wwt_create(sync)` — once-init
  with equality asserts on later netdev registrations; `pdes_link_register(link)` is pure registry. `pdes_engine_set_master_init` wrapper removed (callers write the public `master_init` field directly).
  Diff-bloat sweep (user): `PDESWWT` is now FULLY PRIVATE to pdes-wwt.c (struct + static instance +
  static `pdes_wwt_create`; `pdes_wwt_get` deleted — nothing outside the strategy file uses it, so
  it vanished from the header entirely); one-liner `pdes_link_index` deleted (callers read the
  public `link_idx` field). DIFF-LENS SWEEP (user: reduce diff, not code): PDESWWT typedef+struct RESTORED to the header at
  its HEAD location (contents = the new strategy fields); HEAD-extern wwt functions restored extern
  with header decls (setup_wwt/send_sync/finish_quantum/is_waiting_for_quanta/quanta_sync,
  PDESLink param) and the added forward-decl block dropped; dead HEAD code restored adapted
  (finish_quantum, get_quantum_time_local, schedule_poll, initiate_checkpoint_master, time_test,
  get_number_of_inflight_messages, pdes_inflight_get_all, extern load_json/array_free +
  InflightMessageArray typedef back in checkpoint.h); trailing-newline-only header edits reverted.
  Undeclared-non-static set now matches HEAD exactly (user cleans later).
  vt_base FINAL PLACEMENT (user): it is a MACHINE variable — peers' virtual times differ per link
  but we only ever use OUR OWN virtual time — so `vt_base`/`vt_base_set` live on PDESEngine
  (soft-seeded at pdes_engine_create, finalized at the first link's setup_wwt, later setups assert
  equality). `get_universal_virtual_time(void)` moved back to pdes-utility.c (its HEAD home). The
  cross-link vt_base assert in pdes_savevm_defer is gone (single base makes it moot).
  Kept-with-reason: `pdes_engine_is_master/paused` (approved, many
  callers), `pdes_engine_clear/schedule_checkpoint_bh` (real logic, cross-file),
  `pdes_savevm_defer`/`pdes_engine_fw_complete`/`pdes_engine_restore_inflight` (designated Phase-B
  fork entry points so forks never poke engine fields).
  11 one-line TX/RX stack-position comments across netdev/link/wwt/comm/utility. Function names all
  preserved. Fork meson one-liners for pdes-link.c deferred to each fork's Phase-B gate.
  A.7 review fix (user): checkpoint handshake FACTS are per machine — `ckp_done` (post-save) and
  `ckp_init` (peer warmed) moved onto PDESEngine (replacing per-link pending_ckp_done/init and
  their set-all loops in create_checkpoint_bh / finish_initiate_checkpoint); links keep only the
  `ckp_done_sent`/`ckp_init_sent` announce-once latches, mirroring `ready_sent`. Each control link
  self-serves the announcement in its own send_sync.
  A.7 hygiene rule (user): original dead-code comments / commented-out blocks / vestigial lines are
  RESTORED and stay — no cleanup mixed into the refactor; the user does cleanup themselves later.
  Awaiting user review/compile.
- [~] Phase A.8 — self_loop_partner DELETED; uniform control plane (user ruling: "one machine, 2 nics,
  2 links, connected to each other" — a local pair is two ordinary peered links, nothing loops to
  itself, no link is treated differently). Three changes:
  (1) UNIFORM EMISSION: READY/CKP_DONE/CKP_INIT are announced on EVERY link by EVERY machine (the
  peer-only + self-loop-exclusion conditions in send_sync are gone); recipients already count them
  only if master, so 2-node semantics are unchanged (master's extra bits are ignored by peers —
  wire ctrl bytes differ from HEAD, semantics don't). To make the counts tally: create_checkpoint_bh
  sets `ckp_done` on every machine post-save (master additionally self_ready, as before), and the
  master also checks in `ckp_init` wherever it sets master_init (finish_initiate_checkpoint +
  validate_checkpoint). CKP_REQUEST stays master-only (FW self-announce guard) but now rides all
  links — a duplicate on a local pair hits the equal-round adopt assert. CLEANUP stays master-only.
  (2) AGGREGATES over ALL links: may_stop (master = AND cleanup_sent over all links; vacuous
  has_control/self_ready fallback deleted), init_peers_ready, peers_done — no exclusions; a local
  pair's endpoints genuinely report to each other over the wire (counts N remote + 2 local = N+2).
  (3) DRAIN: pdes_engine_send's -EAGAIN retry polls ALL local links inline (QEMU HOST timers are
  main-loop-dispatched, and the retry blocks the main loop — the clock type picks the deadline, not
  the executor; a remote reader drains itself). `self_loop_partner` field + register pairing loop
  DELETED — registration keeps only the one-owner-per-ring uniqueness assert.
  Also fixed: leftover `pdes_engine_terminate()` call in create_checkpoint_bh → `pdes_engine_destroy()`.
  Awaiting user compile + 2-node regression.
- [~] Phase A.9 — HEAD-parity audit (2-node/1-link trace of every path vs HEAD 6593f5e) found two
  regressions, both fixed (user approved): (1) `sync_checkpoint_check` gated the hold on
  `round_committed` (arm) where HEAD gated on `notified_neighbors` (announce) — with sync=false a
  master arming between quanta_sync(R) and wwt_sync_check(R) held+saved at R without ever sending
  CTRL_CKP_REQUEST, so the peer never checkpointed; master now also requires `notified_neighbors`.
  (2) HEAD reset `notified_neighbors` at every arm; the branch cleared it only on rewake, so a re-arm
  before the rewake timer skipped the announce — new static `pdes_engine_reset_link_latches()`
  (notified_neighbors + ckpt_holding, all links) called at both arm sites
  (`set_checkpoint_values_for_master`, `pdes_savevm_defer`). Everything else traced HEAD-equivalent
  in virtual time (pause/play edge, commit-at-arm == commit-at-announce under BQL, adopt latch,
  in-flight file for link 0, exit handshake, net cleanup after bdrv_close_all); only wire ctrl bits
  and log text differ. NEXT (user directive): build a Claude-runnable compile + 2-node parity
  harness BEFORE Phase B.
- [~] Harness H1 (user directive before Phase B; design decided 2026-09-14, files WRITTEN, nothing run):
  `make test-iterate` fixed (→ `python -m tests.container_compiler`). Per user: ONE SHELL FILE PER TEST —
  `parity-<level>-tmp.sh <pre|latest> <container>` / `... diff` for levels idx, partition, partitions,
  fw, load, fw-all, all (shared `parity-lib-tmp.sh`), plus `parity-switch-{pre,latest}-tmp.sh` that
  check out that version's commit set in the 4 C repos, recompile in qflex_test and stash the binaries
  under the label (dirty-tree guard). `tests/parity_diff.py --level` diffs per level. New `binaries_folder` field on
  ExperimentContext (cp -f override of run/ binaries, asserts the files exist). Two configs
  `conf/MS/ms-multi-parity-{pre,latest}.yaml` extend ms-multi; everything lives under
  /mnt/sdc/testing/parity/. Reference = existing /mnt/sdb MS run (results-only seed). Details +
  status in harness-temp.md. NEXT: user reviews files; user commits the A.x qemu-pdes tree (switch scripts refuse
  a dirty repo); then on command: `parity-switch-pre-tmp.sh` → `parity-idx-tmp.sh pre qflex_test` (smallest
  level first); `latest` only after Phase B.
- [ ] Gate: user approves A.1–A.9 + says done → Phase B (I do NOT start any other submodule without explicit user approval)

### Phase A — what landed (2026-09-01) — HISTORICAL, superseded in places

> First-pass record kept for archaeology. The `PDESNode`/`pdes_node_*` API, self-loop
> auto-detection at register, per-link self_ready/pending_ckp_* described below are ALL GONE —
> A.4–A.8 above replaced them (three structs, HEAD function names, machine facts on PDESEngine,
> uniform control plane, no self_loop_partner). Do not code against this section.

All in `qemu-pdes/`, made fully clean on its own (user directive): `get_singleton_engine` /
`get_singleton_wwt_engine` are DELETED, dead code removed (`initiate_checkpoint_master`,
`pause_bh`/`checkpoint_bh`/`waiting_for_quanta` fields), `finish_initiate_checkpoint` is now `(void)`
node-level, and the init-warm handshake aggregates across control links (`pdes_node_init_peers_ready`,
`pdes_node_set_master_init`, idempotent `set_checkpoint_values_for_master`). `pdes_node_is_master()`
reads link 0's master flag, which `pdes_link_register` now ASSERTS is process-uniform.
**Consequence: the forks do NOT compile until Phase B converts their ~18 call sites** (icount.c,
dynamic_barrier.c, tcg-accel-ops-rr.c, pf_api.c, savevm.c ×2, libqflex.c) — accepted state per user.
Still compatible: `pdes_drain`, `can_stop(engine)`, `pdes_inflight_restore_and_schedule`,
`pdes_inflight_count`, `get_json_file_name`, `validate_checkpoint`, `create_checkpoint_bh`.

- `pdes-engine.c/.h`: 3 singletons → one static `PDESNode` container (`links` GPtrArray + `paused_links`
  + node `checkpoint_bh` + `terminated` latch). New: `pdes_link_count/at/index/register`,
  `pdes_poll_all_links`, `pdes_node_paused` (OR), `pdes_node_may_stop`/`pdes_node_can_stop` (exit gate),
  `pdes_node_peers_done` (CLEANUP release across control links — replaces per-link
  `number_of_neighbors=1` threshold), `pdes_savevm_defer`, `pdes_node_restore_inflight`,
  `pdes_node_fw_complete`, `pdes_node_schedule/clear_checkpoint_bh`. Engines store shm ring names;
  self-loop links auto-detected at register by cross-matched names; ALL former singleton guards
  (engine assert, wwt assert, communicator exit-on-2nd-create) are GENERALIZED, not deleted:
  `pdes_link_check_unique(send, recv)` asserts no other link of this process owns the same send or
  recv ring (one writer / one reader per SPSC ring), called at the top of pdes_engine_wwt_create
  (BEFORE any ring mmap — old fail timing preserved) and again at pdes_link_register. The two
  Flexus-API asserts in pause/play were never removed, only moved inside the edge-trigger blocks.
  Flexus pause/resume edge-triggers
  on node 0↔1 paused-link count. `pdes_engine_destroy` terminate-once. `set_checkpoint_values_for_master`
  arms all links.
- `pdes-wwt.c`: singleton removed; `wwt_sync_check(PDESWWT*)`, `sync_checkpoint_check(PDESWWT*)`,
  `get_quantum_time_universal/local(PDESWWT*, …)`. setup_wwt asserts shared virtual-time base across
  links. Master CLEANUP gated on `pdes_node_peers_done()`. Exit spin uses `pdes_node_may_stop()` +
  polls all links. Checkpoint adopt (CTRL_CKP_REQUEST) propagates arm to ALL of the node's links;
  node-level bh fires once when every link sits at its committed round.
- `pdes-communicator.c`: singleton guard + exit-on-second-create deleted; ring-full wait polls all links.
- `pdes-checkpoint.c/.h`: in-flight entries carry owner link; per-link count/save/restore;
  `get_json_file_name_link` (link 0 keeps legacy filename); `create_checkpoint_bh` = one save per node,
  clears flags on all links, per-link self_ready/pending_ckp_done. `pdes_inflight_remove` matches owner.
- `pdes-netdev.c`: cleanup passes `s->engine->engine` (fixes pre-existing PDESWWT*/PDESEngine* type
  confusion); teardown once via node latch.
- Multi-link checkpoint requires equal link quanta — asserted at arm (defer) and adopt.
- Fork savevm defer still arms only link 0 until Phase B — correct while Python emits 1 netdev.
- [ ] Parity YAMLs track the API (user directive): `conf/MS/ms-multi-parity-latest.yaml` switches to the
  `nics:` schema at C1 (pre YAML stays on the legacy lists — that path IS what is under test); both
  must dry-run to identical `-netdev`/shm names before any run. Each B-phase changes the latest
  binaries → rerun `parity-switch-latest-tmp.sh`. Commit table + scripts in harness-temp.md
  (`parity-switch-{pre,latest}-tmp.sh` are the only git checkouts I run, on explicit command only).
- [ ] B1 parallel-qemu call sites
- [ ] B2 qemu call sites
- [ ] B3 qemu/middleware call sites
- [ ] C1 qflex Python schema groundwork (commands/nic.py, config.py, executer.py, dry-run tests) — INCLUDES rewriting `conf/MS/ms-multi-parity-latest.yaml` to `nics:` and asserting it dry-runs identical to the pre YAML
- [ ] C2+ per-phase enablement: boot → load → fw → init-warm → run-idx → run-single-partition → run-partition (new conf/MS self-loop YAML + expects)
- [ ] D docs (CLAUDE.md, MULTI_NODE.md, fork docs stale-singleton corrections)

## Affected repos and files

All five affected repos confirmed on branch `single-node-pdes` (main, qemu-pdes, parallel-qemu, qemu, qemu/middleware). **flexus** and **WormCacheQFlex** are untouched.

### qflex (main repo) — config schema + orchestration

| File | Why it changes | Stage |
|---|---|---|
| `commands/nic.py` (NEW file) | `NicLink` model lives in its own file — `config.py` is big enough as is | C1 |
| `commands/config.py` | `nics` factory param (per-NIC endpoint declaration); legacy parallel-lists → `nics` translation; `get_shm_names` must encode both endpoints so self-loop send/recv don't collide; `setup_nic_args` iterates `nics` and sets per-link master + stale-shm cleanup; validation gains endpoint symmetry + master-adjacency invariant (kills the 2-node-only shape) | C1 |
| `commands/executer.py` | `_kill_peer_qemus` pgreps `shm-send=/<my-recv>` — on a self-loop that string is our own cmdline, so it would SIGKILL our own qemu; must skip self-loop links | C1 |
| `tests/` (new files + conftest fixtures) | Prove existing configs are byte-identical (untouched dry-run suite) and cover new shapes: naming, normalization, self-loop cmdline, validation failures | C1–C2 |
| `conf/MS/ms-selfloop.yaml` (NEW; no existing conf touched) + new expects under `conf/MS/expects/` | Self-loop experiment config extending `ms.yaml`; guest-side client isolation (defeat local-route shortcut) + NIC IRQ affinity | C2 |
| `CLAUDE.md`, `MULTI_NODE.md`, `tmp_single_pdes_plan.md` | Document the `nics:` schema, legacy translation, global-master model; resumable plan file kept updated every step | D (plan file: every step) |

### qemu-pdes (shared C submodule) — the bulk of the work

| File | Why it changes | Stage |
|---|---|---|
| `net/pdes-communicator.c` | `singleton_comm` hard-exits on a 2nd instance — must go (struct is already instance-based); ring-full send wait must poll local engines or a self-loop deadlocks (only drainer is this same thread) | 2, 5 |
| `net/pdes-engine.c` + `include/net/pdes-engine.h` | `singleton_engine` assert blocks the 2nd netdev — becomes a registry; node-level aggregates added here (`pdes_node_paused/active/can_stop`) because pause/exit are per-process decisions, not per-link; Flexus pause/resume must edge-trigger on the node's 0↔1 paused-count | 2–3 |
| `net/pdes-wwt.c` | `singleton_wwt_engine` assert — same registry conversion; internal users (`wwt_sync_check`, `sync_checkpoint_check`, quantum-time helpers) must take a `PDESWWT*` instead of the singleton; terminate-once gate becomes node-level (else first link to finish `exit(0)`s the process); 2nd+ engine must align quantum boundaries with the 1st (sync=true deadlock guard); spin loop must poll other local engines | 2–3, 5 |
| `net/pdes-checkpoint.c` + `include/net/pdes-checkpoint.h` | In-flight message list and `"<name>_in_flight.json"` are process-global — two links would collide; becomes per-link (link 0 keeps the legacy filename); checkpoint execution must fire once per node when ALL links reach the committed round | 4 |
| `net/pdes-netdev.c` | `pdes_net_cleanup` destroys "the" engine — with N netdevs, destroy-once at node level | 5 |

### parallel-qemu (fork — mechanical call-site edits only)

| File | Why it changes | Stage |
|---|---|---|
| `softmmu/icount.c:333` | icount warp gate reads the singleton's `paused` — must ask the node aggregate (any link paused) | 3 |
| `util/dynamic_barrier.c:167,207` | PWQ pause gate, same singleton read → `pdes_node_paused()` | 3 |
| `accel/tcg/tcg-accel-ops-rr.c:327` | RR-mode pause gate, same | 3 |
| `plugins/pf_api.c:240-263` | FW plugin exit/checkpoint hooks reach for the singleton engine → node-level equivalents | 3 |
| `migration/savevm.c` (~6 sites) | savevm defer + loadvm in-flight restore talk to the singleton → `pdes_savevm_defer()` / per-link restore loop | 4 |

### qemu (timing fork — mechanical call-site edits only)

| File | Why it changes | Stage |
|---|---|---|
| `softmmu/icount.c:343` | same warp-gate conversion as parallel-qemu | 3 |
| `migration/savevm.c` (~6 sites) | same savevm/loadvm conversion as parallel-qemu | 4 |

### qemu/middleware (nested submodule)

| File | Why it changes | Stage |
|---|---|---|
| `libqflex/libqflex.c:318,391` | Flexus exit bridge calls the singleton's `can_stop`/destroy — must aggregate over all links (`pdes_node_can_stop()`, destroy-all) | 3 |

## Context

Goal: run the multi-node machinery with **1 node** — the node gets 2+ PDES NICs (e.g. 2 NICs for its 2 cores, each NIC pinned to a core) whose traffic crosses the simulated PDES wire (latency + optional WWT sync) instead of kernel loopback. This isolates the network/OS effect when comparing 1-node vs 2-node experiments. The generalization must be robust: N cores → N NICs, N links per node, extensible.

**Explicit goal: kill the 2-node-only hardcodes.** Today's stack is structurally a pair (per-link `number_of_neighbors=1`, sync-release threshold of 1, pair-shaped READY/CLEANUP counters, every YAML a 2-leaf topology). After this change, any N-node topology is describable and simulatable, with one validated constraint (user decision: "master-adjacent"): **every node must have at least one direct link to the global master** — the control plane (checkpoint announce, READY/CLEANUP exit) rides only master-links; all other links (self-loops AND non-master inter-node links, e.g. the 1↔2 edge of a ring) are data-plane-only (NORMAL + WWT sync, no CTRL coordination). Chains like 0—1—2 without a 0↔2 link are rejected at validation; relay/flooding is out of scope.

> **UPDATED by A.8 (uniform control plane):** CTRL bits are no longer excluded from any link.
> Every machine emits READY/CKP_INIT/CKP_DONE on EVERY link; recipients count them only if they
> are the master, so non-master links carry ignored bits rather than none. CKP_REQUEST and
> CLEANUP remain master-emitted (on all of the master's links). "Data-plane-only" survives only
> as an effect (no control *decision* rides a non-master link), not as an exclusion in code.
> Master-adjacency stays required: a peer's exit gate is cleanup_received, and only a master
> link ever delivers CLEANUP.

Two shapes must work (user: "both shapes"):
- **(a) Self-loop**: 1 QEMU process, node is its own neighbor; NIC A's send ring = NIC B's recv ring.
- **(b) Multi-NIC per node**: per-link identity so a node can have several links to peers.

All phases (boot/load/fw on parallel-qemu/icount; timing on qemu+flexus where Flexus tick is the clock). Both `sync=false` and `sync=true` must work on self-loops (user answer). Existing 2-node configs must keep working **byte-identically** (shm names, pgrep strings, in-flight filenames).

### Working agreement (user-mandated)

- **Approval gates, one unit at a time**: for each stage I present the concrete list of changes and get explicit approval BEFORE touching the next unit. Order: (1) `qemu-pdes` core change → (2) consuming submodules one at a time in dep order (`parallel-qemu` → `qemu` → `qemu/middleware`) → (3) qflex Python/CLI, one simulation phase at a time (boot, load, fw, …) → (4) documentation.
- **Resumable plan file**: first execution step creates `tmp_single_pdes_plan.md` in the repo root mirroring this whole plan; it is updated at every step (progress ticks, decisions) so a disconnected session can resume from it.
- **No existing conf is modified.** Exactly one new YAML: an MS single-node self-loop conf extending `conf/MS/ms.yaml`, overriding only what's needed.
- **`NicLink` lives in its own new Python file** (e.g. `commands/nic.py`) — `commands/config.py` is big enough as is.
- **C design stays simple**: keep a singleton — but the singleton becomes a container holding all engine instances; wherever code made a direct singleton call, iterate over the instances. No new abstraction layer.

### User decisions (from planning Q&A)
- YAML shape: **NICs as named DI components** injected via `_deps_.nics` (same machinery as `sub_experiments`) — each NIC explicitly declares its remote endpoint (node AND NIC index on that node). The four legacy parallel lists are translated internally. Explicit connectivity replaces any implicit pairing convention.
- Coordination model: **one global master per topology** (node 0; in single-node, that node). NOT per-link masters. The self-loop master must not wait on a fake peer handshake with itself — "generalized and optimized".
- Guest-side client isolation (netns vs macvlan): **left open**, decided during impl in the expect scripts (pure guest Linux commands).
- NIC→core pinning: guest-side (IRQ `smp_affinity` + existing `server_cores`/`client_cores` cpusets). No simulator-level binding — delivery is `qemu_send_packet` on the main loop, no vCPU affinity exists anywhere, and Flexus doesn't model the NIC.

## Current state (verified by exploration)

- **C**: `qemu-pdes/` is a shared git submodule compiled into BOTH forks (`{parallel-qemu,qemu}/net/meson.build:68-73`, `-iquote` at each `meson.build:510`). Single physical copy; only integration call sites differ per fork.
- **Blocker**: three process-global singletons abort on a 2nd `-netdev pdes`: `singleton_engine` (`pdes-engine.c:19,48,103`), `singleton_wwt_engine` (`pdes-wwt.c:34,116,164`), `singleton_comm` (`pdes-communicator.c:11,88-92`, `exit(EXIT_FAILURE)`).
- Already per-NIC-correct: `PDESNetState{nc, PDESWWT*}` (`pdes-netdev.c:9-12`), `pdes_net_receive`/`pdes_recv_callback` use their own engine/nc. Per-instance already: `sync_counts` (round-keyed, fine per-link), `current_quantum_round`, `quantum_ns=latencyns`, `number_of_neighbors=1` (becomes an invariant: each link object has exactly one peer endpoint).
- Link identity on the wire = the shm ring-name pair only (`NetdevPdesOptions`: shm-send/shm-recv/latencyns/sync/master/*phantom — no node id; `Message` has no source id). DRAIN_START/END are gone; all coordination rides SYNC's `CTRL_*` byte (CKP_REQUEST/INIT/READY/CLEANUP/CKP_FINAL/CKP_DONE).
- Cross-cutting state that is per-process-global today and must aggregate at node level: pause flag (`engine->paused`, read by icount warp gate + PWQ/RR gates), savevm defer (`needs_to_checkpoint`, `checkpoint_name`), exit handshake (`can_stop`, `ready_peers`, `cleanup_sent/received`), in-flight list `pending_messages` + `"<name>_in_flight.json"` filename (not link-keyed → collides), terminate-once (`wwt_sync_check:528-543` calls `flexus_api.stop()` / `pdes_engine_destroy`+`exit(0)`).
- **Python**: only `commands/config.py` and `commands/executer.py` touch neighbor lists. `get_shm_names` (`config.py:205-216`) is a pure function of (node, neighbor) → self-loop send/recv names collide (`pdes_0_to_0` both directions). `setup_nic_args` (`config.py:456-514`) already indexes per-link (`netdev=net{i}`, PCI `0x10+i`, MAC `node*10+i`); internet NAT NIC at `0x10+neighbor_count`. `_kill_peer_qemus` (`executer.py:102-130`) pgreps `shm-send=/<my-recv>` → would SIGKILL itself on a self-loop. Templates/jinja/flexus emission, sentinels, ports, image copies: zero neighbor/NIC dependence (keyed by node_number).
- **Guest**: wire IPs are expect-script conventions (`192.168.100.1/.2` on eth0; NAT on eth1). No NIC IRQ affinity mechanism exists anywhere (new, guest-side only).

## Config schema: NICs as named DI components

NICs are first-class DI components (same machinery as `sub_experiments`: named components + `_deps_` `list[T]` wiring). New `NicLink` Pydantic model in a NEW file `commands/nic.py` (`config.py` is big enough as is); `create_experiment_context` gains `nics: Optional[List[NicLink]] = None` (Optional → `_OMIT` when unwired, so legacy configs bind nothing new):

```yaml
components:
  node0_nic0:
    _target_: commands.nic.NicLink
    _name_: node0_nic0
    _base_: _nic_defaults              # latency_ns/sync/net_dev live once
    neighbor: 0
    neighbor_nic: 1
  node0_nic1:
    _target_: commands.nic.NicLink
    _name_: node0_nic1
    _base_: _nic_defaults
    neighbor: 0
    neighbor_nic: 0
  experiment_context_node_0:
    _target_: commands.config.create_experiment_context
    node_number: 0
    _deps_:
      nics: [{name: node0_nic0}, {name: node0_nic1}]
```

- A NIC's identity is `(node_number, nic_index)` where nic_index = position in the `_deps_.nics` list; its remote endpoint is `(neighbor, neighbor_nic)`. No implicit pairing convention anywhere.
- **DRY wins from DI**: a `_nic_defaults` base block holds the common latency/sync/net_dev once; phase overlays override a single NIC component directly (e.g. `boot: { components: { node0_nic0: { latency_ns: 1000000, sync: "false" } } }` replaces today's per-phase `latencies_ns_list` rewrite).
- **Legacy translation**: the four parallel lists remain accepted; when `nics` is unwired, the factory normalizes them into `NicLink`s (entry i → `neighbor=neighbor_node_list[i], neighbor_nic=0`). Explicit `nics` + non-empty legacy lists → error. All downstream code reads only `context.nics`.
- CLI path: legacy parallel-list flags stay the CLI surface; `nics` is YAML/DI-only (`data_class_wrap` must skip non-primitive params when rendering flags — small wrapper allowance if Typer chokes on `Optional[List[NicLink]]`).
- Group-level cross-validation in the group factory (it sees all sub_experiments): node a NIC i pointing at (b, j) requires node b NIC j pointing back at (a, i); both endpoints must agree on latency/sync/net_dev; self-loop endpoints mirror within the node; **master-adjacency**: every node has ≥1 direct link to the global master (non-master links are allowed but data-plane-only).

### Shm-name derivation (backward-compatible)

Send ring of NIC i on node a toward (b, j): `pdes_{a}n{i}_to_{b}n{j}`; recv: `pdes_{b}n{j}_to_{a}n{i}` — plus the existing group prefix and `part_`/`idx_` suffixes, unchanged.

**Legacy exception**: when `a != b` and `i == j == 0` (every existing config), names stay `pdes_{a}_to_{b}` / `pdes_{b}_to_{a}` — **byte-identical to today**, so `_kill_peer_qemus` pgrep strings and any on-disk expectations are untouched. A given (a,b) pair can have only one 0↔0 link, so the rule is unambiguous.

Self-loop falls out naturally: NIC 0 sends on `pdes_0n0_to_0n1`, receives on `pdes_0n1_to_0n0`; NIC 1 the mirror image — cross-wired by construction.

C-side link id = registry creation order (= `-netdev` cmdline order = `nics` list index). No QAPI change.

## Stages

Order per working agreement: **A) qemu-pdes core → B) consuming submodules one at a time → C) qflex Python one phase at a time → D) docs.** Each stage leaves the tree compiling/working, and each gets user approval of its concrete change list before the next begins. The user rebuilds/tests C changes (per standing preference, Claude does not run builds or real-run tests without asking).

### Phase A — qemu-pdes core (approval gate 1)

The singleton is NOT removed — it becomes a **container**: one static singleton struct holding the list of all engine instances (+ the node-level state that is per-process, not per-link). Wherever code made a direct singleton call, it iterates the container's instances. `get_singleton_engine()`/`get_singleton_wwt_engine()` remain temporarily as "instance 0" compat accessors so both forks compile untouched until Phase B.

**A1 — container conversion (zero behavior change for 1 link)**
- `net/pdes-communicator.c:11,88-92`: drop `singleton_comm` + exit-on-second-create (struct already instance-based).
- `net/pdes-engine.c` / `net/pdes-wwt.c` / `include/net/pdes-engine.h`: creation appends to the container instead of asserting NULL; compat accessors return instance 0; wwt-internal users (`wwt_sync_check:691`, `sync_checkpoint_check:477`, `get_quantum_time_universal/local:467-472`) take their `PDESWWT*` explicitly.

**A2 — node-level decisions iterate the container**
- Pause: `pdes_node_paused()` = OR over instances (guest pauses if ANY link waits, resumes when ALL released); Flexus `pause()`/`resume()` fire only on the node's 0→1 / 1→0 transitions.
- Exit: termination gate in `wwt_sync_check:528-543` = AND over **control-plane links** (links touching the global master: master side `cleanup_sent`, peer side `cleanup_received`); data-plane-only links (self-loops AND non-master inter-node links) count as done, and once the node's exit is granted it stops requiring syncs on them so a peer tearing down its end can't wedge the barrier. A pure self-loop node stops the moment it's ready — waits for no one. `flexus_api.stop()` / destroy+`exit(0)` fires exactly once per process. `can_stop` (pdes-engine.c:361-371) generalizes the same way.
- Master: node is master iff it hosts the global master role (node 0 semantics; the netdev `master=` flag marks its control-plane links). On data-plane-only inter-node links, `master=true` goes to the lower node number — it only picks the WWT setup leader, no control-plane meaning.

**A3 — checkpoint machinery**
- Node-level (on the container): `needs_to_checkpoint`, `checkpoint_name`, `checkpoint_format`, `fw_exit_after_checkpoint`, bh-dedupe (`pdes-wwt.c:491-498`) — one `create_checkpoint_bh` per node, firing when ALL links reach their committed round (self-loop pair shares the clock → trivially aligned). Per-link stays per-engine: `checkpoint_quantum_round`, `notified_neighbors`, `pending_ckp_init/done`, `ckp_done_peers`. `CTRL_CKP_REQUEST` is announced on control-plane (master) links only; a node arms its data-plane-only links' rounds locally from the adopted announcement — rounds differ per link (quantum = latency), so the common commit point is a **virtual time**, each link converting it to its own round. All rings drain, including self-loop and data-plane-only ones.
- In-flight per link: owner engine on `ScheduledMessage`/`MessageReceiveContext`; `pdes_inflight_add` takes a `PDESWWT*`; JSON filename: link 0 keeps `"<name>_in_flight.json"` (backward compat), link k≥1 → `"<name>_in_flight_l<k>.json"`; `pdes_drain` iterates all links; `validate_checkpoint` (`pdes-checkpoint.c:283-333`) asks node-level master.
- New `bool pdes_savevm_defer(name, format)` + per-link restore entry points, so Phase B's savevm.c edits are one-liners.

**A4 — multi-instance runtime correctness (self-loop hazards)**
1. Boundary alignment: 2nd+ instance arms its `setup_timer` off the first's base; assert equal `first_sync_virtual_time` at `setup_wwt` (sync=true deadlock guard — syncs are sent from main-loop timer context, so a paused guest clock doesn't block the exchange).
2. Ring-full send escape (`pdes_comm_send` wait loop, `pdes-communicator.c:202-207`): poll all container instances between `usleep(1)` iterations — a self-loop's only drainer is this same thread.
3. `wwt_sync_check` spin (`pdes-wwt.c:520-551`): poll the other instances inside the wait loop so a self-loop peer's sync already in shm is consumed regardless of host-timer ordering.
4. `pdes_net_cleanup` (`pdes-netdev.c:14-19`): destroy-once per process across N netdevs.

Skew/drift asserts (`pdes-wwt.c:349-354,417-420,578-584`) stay untouched — trivially satisfied on a shared clock; they serve as the correctness oracle in testing.

Test after Phase A: user runs the 2-node regression (boot→fw + one timing partition). One link → container degenerates to today's behavior exactly.

### Phase B — consuming submodules, one at a time, dep order (approval gates 2–4)

Each converts its `get_singleton_*()` call sites to the engine-level API; approval before each submodule.

**CURRENT API for the conversions** (the `pdes_node_*` names in older text are dead; this list
matches `include/net/pdes-engine.h` as of A.8 — re-verify against actual fork call sites at each
B gate): pause gates → `pdes_engine_paused()`; savevm/loadvm → `pdes_savevm_defer(name, format)`,
`pdes_drain(name, format)`, `pdes_engine_restore_inflight(name)`, `validate_checkpoint(&name)`;
FW plugin hooks → `finish_initiate_checkpoint(void)`, `pdes_engine_fw_complete(void)`,
`pdes_link_count()`; Flexus exit bridge → `can_stop(void)`, `destroy_strategy(void)` /
`pdes_engine_destroy(void)`. PLUS each fork's `net/meson.build` gains the one-liner for
`../../qemu-pdes/net/pdes-link.c` (NEW file; forks don't compile without it).

**B1 `parallel-qemu`**: warp gate `softmmu/icount.c:333`; PWQ gates `util/dynamic_barrier.c:167,207` + RR gate `accel/tcg/tcg-accel-ops-rr.c:327`; FW plugin hooks `plugins/pf_api.c:240-263`; `migration/savevm.c` (~6 sites); `net/meson.build` one-liner.
**B2 `qemu`**: `softmmu/icount.c:343` + `migration/savevm.c` (~6 sites) + `net/meson.build` one-liner — same conversions.
**B3 `qemu/middleware`**: `libqflex/libqflex.c:318,391` → `can_stop(void)` / `destroy_strategy(void)`.

Test after each: rebuild + 2-node regression for that fork's phases (parallel-qemu → boot/fw; qemu+middleware → one timing partition).

### Phase C — qflex Python/CLI, one simulation phase at a time (approval gates 5+)

**C1 — shared schema groundwork (one approval gate; all commands share `ExperimentContext`)**
- New file `commands/nic.py`: `NicLink` Pydantic model (`neighbor`, `neighbor_nic`, `latency_ns`, `sync`, `net_dev`) — NOT in `config.py` (big enough as is).
- `commands/config.py`: `nics: Optional[List[NicLink]]` factory param (DI-injectable per the schema above); normalize the four legacy parallel lists into `nics` when unwired (error if both given); downstream reads only `nics`. `get_shm_names` (`:205-216`): endpoint-based derivation with the legacy 0↔0 exception. `setup_nic_args` (`:456-514`): iterate `nics`; `master=` = `is_master_node()` for inter-node links, self-loop pairs lower-nic-index endpoint `master=true`; self-loop process always cleans its pair's stale rings; MAC/PCI/netdev-id reindex over `nics`, no scheme change. Validation (`:679-689` + group factory): endpoint symmetry, per-link endpoint agreement (latency/sync/net_dev), master-adjacency invariant (every node ≥1 direct link to node 0). Verify `data_class_wrap` skips the non-primitive `nics` param on the CLI surface (legacy list flags remain the CLI path).
- `commands/executer.py:102-130` `_kill_peer_qemus`: iterate `nics`, skip self-loop links (no peer process; pgrep would match ourselves).
- `SimulationCommand._assert_syncs_true` unchanged: self-loops follow the 2-node convention (sync=false in boot, sync=true in simulation phases).
- Tests (dry-run, `make test`): existing suite passes with **zero assertion edits** (byte-identical proof via legacy translation); new unit tests on normalization + `get_shm_names`; new dry-run tests for self-loop cmdline (two cross-wired `-netdev pdes` clauses, no peer-kill) and validation failures.

**C2+ — per-simulation-phase enablement, one at a time (approval gate each), smallest first: boot → load → fw → init-warm → timing (run-idx → run-single-partition → run-partition)**
- One new conf only: `conf/MS/ms-selfloop.yaml` (name TBD) with `extends: ms` — overrides only `node_number`, the NIC components, and per-phase interaction scripts. **No existing conf is touched.**
- New self-loop expect(s) under `conf/MS/expects/` as each phase needs them: server on eth0 = `192.168.100.1` (core 0 via existing `server_cores`); client forced onto eth1 = `192.168.100.2` with the local-route short-circuit defeated (netns vs docker-macvlan vs policy routing — left open, prototyped in the boot expect); NIC IRQ `smp_affinity`: eth0→core 0, eth1→core 1 (guest-side only). Per-workload memory: MS binaries at `./hammar/target/release/{warm,hammar}`; taskset conventions carried through.
- Per phase: wire + dry-run test first, then the user runs the real test for that phase before the next phase starts (test-order preference: smallest first).

### Phase D — documentation (approval gate final)

- `CLAUDE.md` / `MULTI_NODE.md` (root): `nics:` DI schema, legacy translation, self-loop semantics, global-master model, shm naming spec.
- `qemu-pdes`-related notes in the fork docs where the singleton is described (parallel-qemu/qemu `MULTI_NODE.md`s mention the singleton model and the stale "two in-sync copies" framing — correct both).
- Update `tmp_single_pdes_plan.md` ticks throughout; final state records what landed per repo/branch.

## What does NOT change (scope fence)

- `Message` wire format, `MSG_TYPE_*`, `CTRL_*` protocol, sync payload — no source id needed (identity = ring pair).
- QAPI schema (`qapi/net.json`, both forks).
- PWQ machinery internals, Flexus, all Jinja/flexus templates (zero NIC dependence confirmed).
- Skew/drift logic; per-link `number_of_neighbors = 1` (hardcode → invariant).
- Sentinels, telnet ports, per-node image copy, `wait_for_nodes` — keyed by node_number; a self-loop is one node, one leaf, no `sub_experiments`.
- All existing shm names, in-flight filenames, `_kill_peer_qemus` pgrep strings for current configs (byte-identical guarantee, proven by the untouched dry-run suite).
- Control-plane relay/flooding for topologies where a node has NO direct master link (chains, deep trees): rejected in validation, out of scope. (Non-master links themselves ARE supported — data-plane-only.)

## Verification

1. Phase A/B (C changes): user rebuilds (`make test-iterate` on request / image rebuild) and runs the 2-node regression after each approval unit; the self-loop ladder runs once Phase C enables it: boot ping (latency visible in RTT) → fw sync=true (drift asserts as oracle) → savevm/loadvm with traffic in flight (both in-flight JSONs) → unchanged 2-node regression. Builds and real runs are the user's call; I ask before triggering any.
2. Phase C: `make test` (dry-run, hermetic) after each gate — existing multi-node tests must pass unedited; new tests cover the new shapes. Real-run per phase, smallest first, user-approved each time.
3. Post-run smell test per CLAUDE.md: walk experiment-folder logs (Boot/Load/FW logs, expect_log.txt, drift-assert absence, in-flight JSON counts at savevm).

## Open implementation details (decided in-flight, small)

- Exact home of node-level fields on the singleton container struct — pick during Phase A (minimal-change ethos).
- `flexus_api.pause/resume` nesting semantics — refcounting in qemu-pdes makes us safe either way; confirm with user if Flexus-side behavior looks surprising in testing.
- Guest client isolation mechanism (netns / macvlan / policy routing) — prototyped in the Phase C boot expect.
- `conf/MS/ms-selfloop.yaml` final name — user's call at Phase C.
