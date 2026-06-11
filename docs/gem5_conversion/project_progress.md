# gem5 Conversion Project Progress

## Purpose

When this phase of the project started, qflex already supported:

- statistical sampling
- parallel functional warming with BXKraken
- Flexus timing simulation

The missing piece was gem5 timing simulation under the same methodology.

The core requirement was checkpoint conversion. BXKraken-generated checkpoints
had to be converted into a form gem5 could consume, both architecturally and
microarchitecturally.

## Conceptual model

This project uses a few terms in a precise way:

- **sample**: a collection of sampling units that together satisfy the
  statistical sampling methodology for a workload
- **snapshot**: a QEMU snapshot such as `snapshot_0`; this is QEMU VM-state
  terminology
- **checkpoint**: a snapshot plus the recorded microarchitectural state from
  parallel functional warming; this is passive state
- **sampling unit**: a checkpoint when it is actively simulated for a timing
  window that includes detailed warmup and measurement

The gem5 bridge required two conversion layers:

1. **Architectural conversion**
   - disk image
   - memory image
   - register/checkpoint state
2. **Microarchitectural conversion**
   - BTB
   - TAGE
   - L1I
   - L1D
   - LLC
   - directory/private-state restore families
   - L1 ITLB/DTLB

The architectural-conversion story also evolved during the project. The work
started from the older QPoints architectural conversion path, tied to a
particular QEMU/gem5 pairing. Over time, that architectural conversion logic
was replaced by the newer project-owned methodology in the QEMU side of the
stack. That is one reason the current `QPoints` repository name and lineage are
now misleading.

## What is delivered now

### qflex now has a real gem5 path

The user entrypoint is now the `qflex` CLI, not ad hoc QPoints scripts.

The current qflex surface includes:

- experiment lifecycle commands such as `boot`, `load`, `initialize`, and `fw`
- gem5 checkpoint conversion through `qflex qpoints convert-single` and
  `qflex qpoints convert-multi`
- single-checkpoint gem5 timing through `qflex qpoints run-gem5`
- sample-driven timing through `qflex run_sample` for both Flexus and gem5

This is the most important workflow change from the user’s point of view.

At the same time, the older Flexus partition path still exists in the CLI.
The project direction is to treat `run_sample` as the primary user-facing
timing command, while the partition-oriented Flexus flow remains as legacy
surface that may be retired later.

### QPoints acts as a bridge layer

QPoints still matters in the stack, but it no longer represents the primary
user interface.

Its current role is to:

- stage gem5-consumable checkpoints
- prepare gem5-side microarchitectural restore artifacts
- host gem5 integration configs
- host tracked validation records

That role is different from what the repository name suggests. The current
name and lineage are now stale and should be replaced in a future cleanup
phase. The stale naming issue is not only about workflow ownership. It is also
about the fact that the architectural conversion path no longer reflects the
older project identity that the repository name implies.

### Microarchitectural conversion now covers a substantial restore stack

The gem5 path is no longer limited to architectural conversion. The current
stack now supports conversion and restore work across several major
microarchitectural components:

- BTB
- TAGE
- L1I
- L1D
- LLC
- directory/private-state restore families
- L1 ITLB/DTLB

This does not mean every possible checkpoint shape is supported. It does mean
the project can now drive meaningful gem5 timing runs from BXKraken-origin
checkpoints with more than architectural state alone.

### Focused validation microbenchmarks now exist for component-level checks

The project now includes focused validation microbenchmarks under:

- [QPoints/scripts/validation](../../QPoints/scripts/validation)

These benchmarks were designed to test specific parts of the conversion and
restore stack with tighter control than a large full-system workload can offer.
Representative examples include:

- [tlb_resident_rw](../../QPoints/scripts/validation/tlb_resident_rw)
  - targeted TLB restoration checks
- [l1d_resident_rw](../../QPoints/scripts/validation/l1d_resident_rw)
  - private L1-D restoration checks
- [llc_resident_rw](../../QPoints/scripts/validation/llc_resident_rw)
  - shared-cache residency checks
- [l1d_frontend_mix](../../QPoints/scripts/validation/l1d_frontend_mix)
  - combined L1-D, L1-I, BTB, and branch-prediction checks
- [tage_family_8x4](../../QPoints/scripts/validation/tage_family_8x4)
  - branch-family/TAGE-focused validation and offline probing

Their results are tracked through validation packages under:

- [QPoints/validation_records](../../QPoints/validation_records)

Important examples include:

- [canonical_tlb_restore_64](../../QPoints/validation_records/canonical_tlb_restore_64/snapshot_0/TLB_RESTORE_64_REPORT.md)
- [diagnostic_tlb_restore_1024_population1](../../QPoints/validation_records/diagnostic_tlb_restore_1024_population1/snapshot_0/TLB_RESTORE_1024_REPORT.md)
- [reference_l1d_restore_100k](../../QPoints/validation_records/reference_l1d_restore_100k/snapshot_0)
- [reference_l1i_restore_100k](../../QPoints/validation_records/reference_l1i_restore_100k/snapshot_0)

This benchmark-and-record structure matters because it lets the project test
conversion correctness for individual components with purpose-built workloads,
instead of depending only on broad full-system comparisons.

### BTB support now includes the gem5-oriented basic-block view

For frontend restore, the warming side now supports exporting the gem5-oriented
basic-block BTB view. That export is carried into the gem5 conversion flow and
is part of the current restored frontend story.

### TLB conversion was materially reworked during integration

The initial TLB conversion did not simply land and remain unchanged. After the
handoff of the first TLB implementation, the integration pass changed the
conversion path substantially in response to failures exposed by focused
diagnostics.

That work included:

- integrating runtime-gated TLB restore into gem5 execution
- tightening the source-to-sidecar machine contract
- adding focused TLB validation work, especially the dedicated
  `tlb_resident_rw` diagnostic

The strongest tracked TLB reference is:

- [diagnostic_tlb_restore_1024_population1](../../QPoints/validation_records/diagnostic_tlb_restore_1024_population1/snapshot_0/TLB_RESTORE_1024_REPORT.md)

That record matters because it documents both:

- the failure modes that appeared during integration
- the final corrected proof that restored `1024/1024` ITLB/DTLB state can
  reduce the microbenchmark’s restored TLB misses to zero

The final corrected proof depended on two important controls:

- preserving the intended source-side TLB configuration during lineage
  regeneration
- removing the SME mismatch through the `sme=off` source CPU model

### The SME mismatch was identified and turned into a source-model default

One of the most important lessons from the TLB work was that an apparently
"microarchitectural" restore failure can actually come from a machine-model
mismatch at the source/target boundary.

In the TLB diagnostics, the suspicious early post-restore kernel path was
eventually traced to an SME-related mismatch. The source QEMU side exposed
state that the target gem5 fork did not model correctly, which led to an
undefined-instruction path and distorted the restored TLB behavior.

That issue was important enough to become part of the current default source
CPU contract.

Today, the repository default for the QEMU source CPU model is:

- `max,pauth=off,sme=off`

and it is resolved through:

- [qemu_cpu.py](../../commands/qemu_cpu.py)

using the environment-variable override:

- `QFLEX_QEMU_CPU`

So `sme=off` is no longer just a one-off diagnostic workaround. It is part of
the current default organization of the project, while still remaining an
explicit knob that can be changed later if the source/target model contract is
reopened deliberately.

### Kernel provenance and machine-contract handling are stronger

Web-search bring-up exposed a kernel mismatch problem between the source
lineage and the gem5 consumer side.

That led to explicit kernel and machine-contract handling across the workflow:

- boot now records kernel provenance into experiment-local machine metadata
- conversion and timing runs validate that contract
- checkpoint roots carry their own machine manifest

This tightened the workflow substantially. It also exposed an important
remaining limitation: automatic extraction of the matching kernel ELF is still
not fully complete.

### The cache-hierarchy direction moved from MESI bring-up toward non-inclusive MOESI

The project initially used MESI as the practical bring-up path for Ruby cache
restore. That was useful, but it is not the right long-term fidelity target for
BXKraken-origin checkpoints.

The reason is architectural:

- the important source checkpoints come from a non-inclusive shared-cache model
- `MESI_Two_Level` is inclusive

That mismatch matters for private-state restoration because an inclusive target
would force LLC residency that did not exist in the source model, which would
change occupancy and later replacement behavior.

For that reason, the long-term direction shifted toward a non-inclusive Ruby
protocol, with `MOESI_CMP_directory` as the active path for that migration.

### Cache restoration currently normalizes restored shared-cache lines to clean state

The current cache-hierarchy restore path does not try to preserve dirty LLC
state exactly as a target-state invariant in gem5.

Instead, the restored shared-cache lines are normalized into clean gem5
residency and their data is taken from memory. This is intentional. The source
checkpoint is taken at a point where memory is already up to date, so the gem5
side can reconstruct the correct data values by reading memory rather than by
requiring a dirty target LLC state.

This is an important modeling assumption and should be understood as part of
the current restore contract.

### Directory/private-family conversion is explicit, but not complete

Directory-state conversion turned out to be one of the hardest parts of the
gem5 bring-up.

The current MOESI conversion covers several major private coherency families,
but not all possible private-state shapes observed in BXKraken checkpoints.
Unsupported families are tracked explicitly by the conversion flow and reported
through a structured guardrail report rather than being converted
speculatively.

The implemented set was intentionally limited to families whose restore story
could be validated with the available microbenchmarks and workload evidence.
That is a deliberate scope choice, not an omission hidden by the tooling.

### Timing methodology support is usable with cycle-window reporting

The gem5 timing path now supports detailed warmup and measurement windows in
cycles, with measurement-only IPC and uIPC reporting.

This matters because the project goal is not just “run gem5 somehow,” but “run
gem5 under the same sampling methodology in a form that can be compared
and tracked.”

### Lazy `.gem` and serial multi-snapshot gem5 runs are now supported

The workflow now supports a more practical multi-snapshot path:

- `.gem` emission can be deferred during functional warming
- `convert_single` can recreate `.gem` lazily when needed
- gem5 `run_sample` can process a snapshot range serially, converting each
  selected snapshot into a checkpoint and then simulating that checkpoint as a
  sampling unit
- per-snapshot conversion readiness is checked automatically
- derived conversion artifacts can be cleaned after each successful timing run

This is a substantial usability improvement over a manual “preconvert
everything first” flow.

At the same time, the older `convert-multi` path should not be treated as the
recommended conversion mode today. Current testing indicates that, on
HDD-backed setups, parallel conversion can suffer from enough non-sequential
artifact writes to make total conversion time significantly worse than the
serial path. For now, `convert-multi` remains a stale utility and future-work
item rather than the preferred way to prepare checkpoint ranges.

## What is validated

Two validation records are the strongest current references for the latest
project state.

### Flexus vs gem5 comparison point

- [ws_flexus_vs_gem5_mesh_compare_200k_1m](../../QPoints/validation_records/ws_flexus_vs_gem5_mesh_compare_200k_1m/snapshot_0/WS_FLEXUS_VS_GEM5_MESH_COMPARE_200K_1M_REPORT.md)

This records a stable comparison point for web-search on `snapshot_0` with:

- warmup: `200000` cycles
- measurement: `1000000` cycles

It shows that the current gem5 path is operational and comparable enough to
serve as a reference point, while also making clear that gem5 and Flexus do
not yet behave identically.

### Serial multi-snapshot gem5 mechanics

- [ws_gem5_multi_snapshot_serial_200k_1m](../../QPoints/validation_records/ws_gem5_multi_snapshot_serial_200k_1m/snapshots_0_1/WS_GEM5_MULTI_SNAPSHOT_SERIAL_200K_1M_REPORT.md)

This validates the mechanics of the serial gem5 `run_sample` path across
`snapshot_0` and `snapshot_1`, including:

- per-snapshot lazy conversion
- per-snapshot timing of each restored checkpoint as its sampling unit
- optional cleanup of derived conversion artifacts
- final aggregation across the processed snapshot range

This is a mechanics validation, not a full validation of the statistical
methodology over a larger sample set.

## Current direction

The current architecture of the project is:

- **qflex** is the user-facing entrypoint
- **QPoints** is the bridge between BXKraken-origin state and gem5
- **validation records** are the tracked evidence layer for claims we consider
  stable enough to merge and cite

That direction is deliberate. It moves the project away from ad hoc local
flows and toward:

- explicit contracts
- reproducible validation
- command ownership in one CLI surface

## Caveats and future work

### The final branch-integration step is still pending at the project level

The current `gem5-integration/main` branches should be understood carefully.

For **QPoints** and **gem5**, `gem5-integration/main` is already the canonical
branch at the repository level for this project. Those repositories do not need
an additional merge into some other branch just to become canonical for their
own scope.

For the top-level project repositories, the story is different:

- **qflex** still needs a final merge from `gem5-integration/main` into the
  repository's `main` branch
- **WormCacheQFlex** still needs the corresponding final merge from
  `review/ali/wormcache-restore` into the repository's `develop` branch

So the current integration branches are already canonical in some repositories,
but not yet fully propagated to the top-level canonical branches of the whole
project stack.

### QPoints should be renamed and migrated

The current `QPoints` repository name and lineage are stale relative to what
the project now does. The architectural conversion story has evolved, and the
current bridge layer should eventually live in a repository whose name and
history match its present ownership and purpose.

### Automatic kernel ELF extraction is still incomplete

The boot flow now captures and carries kernel provenance much more carefully
than before, but automatic extraction of the matching kernel ELF is not fully
implemented.

In some lineages, especially when boot requires user action, the user must
still supply the kernel explicitly. This is a workflow limitation that the
qflex CLI guide should describe precisely.

### STLB conversion is still missing

The current restore path covers L1 ITLB/DTLB conversion and validation, but not
STLB conversion. Completing the memory-translation story requires STLB support
in a later phase.

### Container bring-up and Dockerfile organization need consolidation

The current Dockerfile and container bring-up story is more complicated than it
should be.

At the moment:

- the Dockerfile surface is split across multiple files and build paths
- container bring-up is harder to understand and maintain than necessary
- the current organization copies repositories into containers in a way that
  breaks the normal Git provenance of the project stack

That last point matters operationally. Inside the container, development should
remain tied to:

- each repository's own Git history
- the parent repository's submodule pointers

Without that, normal development and review tasks become awkward because the
container no longer reflects the real repository relationships cleanly.

Future work should replace the current container organization with a single
canonical Dockerfile, or an equivalently simple canonical container path, that:

- is much easier to understand and maintain
- avoids the current multi-container and multi-Dockerfile complexity where it
  is not actually needed
- preserves the Git provenance of each repository inside the container
- preserves the parent-repository view of submodule pointers so that submodule
  updates remain correct and reviewable

### Initial gem5 takeover activity needs deeper study

The TLB diagnostics showed that the period immediately after gem5 takes over a
checkpoint can matter a great deal.

One important lesson from the TLB work is that early kernel activity during
takeover can perturb restored microarchitectural state before the intended
steady-state user footprint dominates the timing window.

The `1024/1024` TLB diagnostic eventually proved that restored TLB state can be
fully effective on the microbenchmark once the source-side configuration is
preserved and the SME mismatch is removed. But the failed pre-fix run was still
important because it exposed how early exceptional kernel paths can distort the
observed post-restore behavior.

In that case, the suspicious early kernel path was not generic noise. It was
traced to a machine-model mismatch that caused an undefined-instruction path
around SME-related state. The later `sme=off` source-model control removed that
path and allowed the TLB proof run to reach zero restored misses on the
microbenchmark.

This has implications for:

- checkpoint fidelity
- detailed warmup interpretation
- how much trust to place in the first portion of the resumed timing window

### Flexus vs gem5 still needs stronger trust-building tests

The current web-search comparison is useful, but it should not be overclaimed.

It shows two things at once:

- some cores are in a reasonably comparable IPC range between gem5 and Flexus
- other cores are not, and show clear behavioral divergence

That mismatch does not automatically mean gem5 is broken as a timing model. In
a full-system workload,
differences in:

- OS scheduling
- interrupt timing
- network and memory behavior
- cache-home placement

can all change which work each core performs during a fixed window.

So the current gap should be interpreted carefully. It is evidence that the
current full-system comparison is too weak to establish simulation fidelity on
its own, not evidence that gem5 is necessarily malformed.

If gem5 is to be treated with higher confidence relative to Flexus as a golden
timing reference, the project needs stronger tests than “run both simulators
from the same baseline for the same period and compare the result.” The next
step is to design tests that control more of the full-system variability rather
than relying only on one resumed timing window.

### Flexus and gem5 do not have a proven-identical LLC home/bank mapping policy

The current project aligns gem5 and Flexus on several important structural
points:

- same broad statistical-sampling flow
- same checkpoint lineage
- same number of cores
- comparable mesh shape for the current web-search reference

However, the mapping from an address to its effective LLC home/bank is not
established to be identical between the two implementations.

On the gem5 side, sliced restore and runtime validation are tied to gem5’s own
LLC slice-count and address-to-slice interleaving rule. On the Flexus side,
home placement is determined through its own mapping and destination logic.

This may not matter much for many workloads. But for workloads that stress bank
placement or conflict patterns, the difference can become visible and should be
treated as a real modeling caveat.

### Some MOESI private families remain intentionally unsupported

The current conversion tooling does not try to force every observed private
coherency family into a gem5 restore shape.

Instead:

- supported families are converted explicitly
- unsupported families are reported explicitly

If a later workload depends on one of the currently unsupported families, that
should trigger targeted extension work rather than speculative silent handling.
