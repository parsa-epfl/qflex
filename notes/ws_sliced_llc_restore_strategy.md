# Web Search Sliced LLC Restore Strategy

## Scope

This note tracks the investigation and planned support strategy for **sliced LLC warm restore** in gem5 for:

- experiment: `ws-image-fresh-8c`
- snapshot: `snapshot_0`
- protocol: `MOESI_CMP_directory`

This is a local engineering note for the current dev-branch phase. It is intended to be the single reference point for the sliced LLC restore effort before code changes begin.

## Current status

We can already do all of the following:

- create `snapshot_0` through `fw`
- convert the snapshot through `convert-single`
- run gem5 timing with MOESI and 8 slices
- restore BTB/TAGE/TLB state

What still fails is **cache-hierarchy warm restore with sliced LLCs**.

## Reproduced failure

A gem5 timing run with MOESI restore enabled fails with:

```text
fatal: --restore-llc-state currently supports only --num-l2caches=1; got 8 L2 caches.
```

This failure is reproduced in:

- `/home/dev/qflex_git/QPoints/gem5/configs/ruby/MOESI_CMP_directory.py`

## Runtime target we want

For the current web-search comparison target, the intended gem5 runtime shape is:

- `8` cores
- `8` LLC slices
- `1MB` per slice
- `8MB` total LLC
- `8` directory slices
- `MOESI_CMP_directory`

This shape is already accepted by gem5 **without LLC warm restore**.

## What the investigation established

### 1. The current restore model is structurally single-slice

The MOESI Python config currently assumes:

- one global LLC restore file
- one restoring L2 controller
- all directory controllers sharing the same restore file

Concretely:

- `discover_llc_restore_file()` returns one file:
  - `llc_restore_addrs.txt`
- only `l2_cntrl0` gets LLC restore enabled
- all directory controllers receive the same global file

So the current failure is not just a cosmetic guard. The whole restore wiring is single-slice today.

### 2. The converter already has enough information to partition by gem5 slice

The converter inputs are currently:

- `llc-0.json.zstd`
- `directory-0.json.zstd`
- `harvard-0.json.zstd`

The generated LLC restore set for `snapshot_0` contains `128068` lines and is already evenly distributed across the 8 gem5 slices when classified by line address:

- slice 0: `16008`
- slice 1: `16005`
- slice 2: `15998`
- slice 3: `16013`
- slice 4: `16001`
- slice 5: `16002`
- slice 6: `16011`
- slice 7: `16030`

So the problem is **not** missing address entropy or missing source information. The problem is that the restore set is emitted and consumed as one global file.

### 3. Directory source metadata does not currently carry explicit slice/home ownership

A sampled directory entry contains fields like:

- `lru_ts`
- `sharers`
- `in_shared_cache`
- `shared`

It does **not** appear to record an explicit slice/home identifier.

This is acceptable for the current phase because gem5 slice assignment can be derived from `line_addr` using gem5's own interleave rule.

It would only become a blocker if we later try to match Flexus home assignment exactly.

### 4. L2 warm restore has no built-in range filter

This is the decisive implementation fact.

The common warm-file reader:

- `AbstractController::startupWarmStateFromFile()`

just streams each address to the controller's warm-restore handler. It does not filter by controller address range.

For MOESI L2 restore:

- `applyWarmLineAddr()` in LLC mode calls `preloadWarmLine()`
- `preloadWarmLine()` allocates a real L2 line and seeds it as shared

The underlying `CacheMemory` code:

- does not check controller `addr_ranges`
- only indexes the address into the cache set structure

Therefore, if the same global restore file were handed to all eight L2 controllers, each L2 would attempt to preload every line. That is incorrect.

**Conclusion:** per-slice restore files are mandatory for any restore stream consumed by an L2 controller.

### 5. Directory controllers are different

The Ruby directory storage is range-aware:

- `DirectoryMemory::isPresent(address)` checks `addr_ranges`

That means a directory controller can tolerate a global restore file because it can ignore out-of-range lines.

This is not true for the L2 cache or the L2-local directory state.

## Classification of MOESI restore artifacts

The following classification is based on the actual protocol consumers.

### Must be slice-partitioned

These files are consumed by L2 controllers and therefore must be partitioned per gem5 slice.

#### LLC-backed files

- `llc_restore_addrs.txt`
- `moesi_private_clean_restore...`
- `moesi_multi_private_clean_restore...`
- `moesi_private_instruction_only_restore...`

Reason:
- these paths allocate or require a real LLC entry in the L2 slice
- a global file would seed the wrong L2 controllers

#### Non-LLC local-directory files

- `moesi_private_owner_restore...`
- `moesi_multi_private_clean_nonllc_restore...`
- `moesi_private_instruction_only_nonllc_restore...`

Reason:
- these do not allocate an LLC line, but they do seed **L2-local directory state**
- that local directory path is also not range-filtered
- a global file would over-seed every L2 slice

### Can remain global initially

These directory-side restore streams can remain global for the first implementation increment:

- directory LLC seeding from the LLC restore file
- directory owner/non-LLC sharer seeding paths

Reason:
- `DirectoryMemory` is range-aware
- directory controllers can safely ignore out-of-range lines

This is an implementation convenience, not a fundamental requirement. We may still move to per-slice directory files later for symmetry.

## Minimal viable implementation strategy

### Phase 1: slice-aware staging in the converter

Update the converter to emit per-slice files for every L2-consumed MOESI restore artifact.

Likely naming shape:

- `llc_restore_addrs.slice0.txt`
- `llc_restore_addrs.slice1.txt`
- ...
- `llc_restore_addrs.slice7.txt`

The same pattern should apply to the other L2-consumed MOESI restore files.

Partition rule:

- derive the gem5 slice from `line_addr`
- use the current gem5 interleave rule for `num_l2caches = 8`

### Phase 2: slice-aware Ruby wiring

Update `MOESI_CMP_directory.py` so that:

- each `l2_cntrl{i}` gets only its matching per-slice restore file
- the single-slice guard is removed only after this wiring exists

### Phase 3: directory-side cleanup

Decide whether to:

- keep directory restore global for now, or
- also move directory restore to per-slice files for symmetry

This is secondary to getting L2 restore correct.

## Explicit non-goal for this phase

This phase is **not** trying to match Flexus LLC home assignment semantics.

We are only trying to support gem5 sliced LLC warm restore under gem5's current interleave rule.

Flexus-style home assignment remains a separate issue.

## Immediate next investigation before code changes

Before editing code, verify file-by-file that each staged MOESI restore artifact can be partitioned using `line_addr` alone.

The expectation is yes, but it should be confirmed once for the full family of files.

## Files already identified as central

Converter side:
- `/home/dev/qflex_git/QPoints/scripts/uarch_restore/prepare_gem5_uarch.py`

MOESI config side:
- `/home/dev/qflex_git/QPoints/gem5/configs/ruby/MOESI_CMP_directory.py`

Protocol side:
- `/home/dev/qflex_git/QPoints/gem5/src/mem/ruby/protocol/MOESI_CMP_directory-L2cache.sm`
- `/home/dev/qflex_git/QPoints/gem5/src/mem/ruby/protocol/MOESI_CMP_directory-dir.sm`

Common warm-file reader:
- `/home/dev/qflex_git/QPoints/gem5/src/mem/ruby/slicc_interface/AbstractController.cc`

Storage behavior:
- `/home/dev/qflex_git/QPoints/gem5/src/mem/ruby/structures/CacheMemory.cc`
- `/home/dev/qflex_git/QPoints/gem5/src/mem/ruby/structures/DirectoryMemory.cc`

## File-format verification

The current staged MOESI restore artifacts are already partitionable using `line_addr` alone.

### Verified on-disk formats

#### Global LLC restore
- `llc_restore_addrs.txt`
- format: one line address per line
- example:
  - `0x7d3380000`

#### Single-private owner
- `moesi_single_private_data_writeable_restore.txt`
- format: `<line_addr> <owner_core>`
- example:
  - `0x100002200 0`

#### Single-private clean
- `moesi_single_private_data_clean_restore.txt`
- format: `<line_addr> <sharer_core>`
- example:
  - `0x100025400 0`

#### Multi-private clean
- `moesi_multi_private_data_clean_restore.txt`
- format: repeated `<line_addr> <sharer_core>` pairs
- example:
  - `0x3a8f75c40 0`
  - `0x3a8f75c40 1`

#### Multi-private clean non-LLC
- `moesi_multi_private_data_clean_nonllc_restore.txt`
- format: `<line_addr> <sharer_core>`
- example:
  - `0x29d62b640 0`

#### Private instruction-only
- `moesi_private_instruction_only_restore.txt`
- format: `<line_addr> <sharer_core>`
- example:
  - `0x105458640 0`

#### Private instruction-only non-LLC
- `moesi_private_instruction_only_nonllc_restore.txt`
- format: `<line_addr> <sharer_core>`
- example:
  - `0x11701cb40 0`

### Candidate JSON files

The candidate JSON files also retain `line_addr` explicitly, which means slice-aware staging can be implemented without changing the source snapshot format or inventing new metadata.

Examples:
- `moesi_single_private_data_writeable_restore_candidates.json`
- `moesi_single_private_data_clean_restore_candidates.json`
- `moesi_multi_private_data_clean_restore_candidates.json`
- `moesi_private_instruction_only_restore_candidates.json`

These include fields such as:
- `line_addr`
- `owner_core`
- `sharer_core`
- `sharer_cores`
- `llc_backed`

### Conclusion from format verification

There is no format blocker.

For the current gem5 sliced-restore effort, all L2-consumed MOESI restore files can be partitioned by gem5 slice using the staged `line_addr` values that already exist today.

## Generality requirement

Any sliced LLC restore support added in this phase must be general, not hardcoded to the current 8-core / 8-slice web-search setup.

### Required properties

1. **Scale with configured slice count**
- The staging and runtime wiring must derive slice count from the active gem5 configuration.
- The implementation must not assume `8` slices or `8` cores.

2. **Avoid hardcoded address partition rules in random call sites**
- The slice-mapping rule should be centralized in one helper/API boundary.
- Converter-side partitioning and runtime-side discovery should both depend on that same logical contract.

3. **Keep slice count separate from home-policy semantics**
- The first implementation may use gem5's current interleave rule.
- But the code structure should not bake in the assumption that the long-term home assignment policy is permanently this rule.
- We want the option to change or parameterize home slicing later without rewriting the whole restore pipeline.

4. **File naming and discovery should be data-driven**
- Per-slice restore artifacts should be named and discovered using slice indices derived from configuration.
- The converter should emit as many slice files as needed for the configured system.
- The Ruby config should wire files to controllers based on controller index, not on a hardcoded list.

### Practical implication for implementation

When code changes begin, the first version may target the current gem5 interleave rule, but it should still be structured as:

- `slice_count` comes from config
- `slice_id(line_addr, slice_count, policy)` comes from a dedicated helper
- restore-file emission loops over slice indices generically
- Ruby wiring loops over controllers generically

That will keep the first working implementation usable for other core/slice counts and leave room for a future home-policy change.

## Concrete implementation plan

This section defines the first implementation contract for sliced LLC warm restore support.

### 1. Slice-mapping contract

The implementation needs one explicit helper that answers:

- `slice_id(line_addr, slice_count, policy) -> int`

#### Ownership

The first implementation should keep this helper on the **converter side** and mirror the same logic in the Ruby config layer only where discovery/wiring needs to know slice indices.

Reason:
- restore-file emission must decide which file a line belongs to
- the Ruby config only needs slice indices to choose the matching file for each controller
- the actual protocol controllers should not own slice-partition policy

#### First policy

The first implementation should use the current gem5 LLC interleave rule for Ruby L2 slices:

- line-based interleave derived from `line_addr`
- `slice_count = num_l2caches`

#### Structure requirement

Even in the first version, the code should be written so the policy can later be replaced or parameterized. In practice:

- no ad hoc `(line_addr >> 6) & 0x7` logic scattered across files
- no direct hardcoding of `8`
- use one named helper for slice mapping

### 2. Per-slice file contract

All L2-consumed MOESI restore artifacts should move to an explicit per-slice naming scheme.

#### Proposed naming

LLC-backed files:
- `llc_restore_addrs.slice{slice}.txt`
- `moesi_single_private_data_writeable_restore.slice{slice}.txt`
- `moesi_single_private_data_clean_restore.slice{slice}.txt`
- `moesi_multi_private_data_clean_restore.slice{slice}.txt`
- `moesi_private_instruction_only_restore.slice{slice}.txt`

Non-LLC local-directory files:
- `moesi_multi_private_data_clean_nonllc_restore.slice{slice}.txt`
- `moesi_private_instruction_only_nonllc_restore.slice{slice}.txt`

#### Compatibility decision

First implementation should prefer a clean contract over a mixed one:
- emit the new per-slice files
- stop treating the old global files as the primary runtime contract for sliced MOESI restore

If backward compatibility is needed temporarily, it should be explicit and limited to:
- single-slice restore still reading the legacy unsliced filename

### 3. Converter-side behavior

Converter changes belong in:
- `/home/dev/qflex_git/QPoints/scripts/uarch_restore/prepare_gem5_uarch.py`

#### Required behavior

1. derive `slice_count` from conversion context
2. partition every L2-consumed MOESI restore stream by slice
3. emit one file per slice index
4. include emitted slice-file inventory in the manifest

#### Manifest expectation

The manifest should record per-slice outputs explicitly so runtime/debugging can tell:
- which files exist
- which slices are empty
- how many records each slice received

That means the manifest should evolve from:
- one path for a restore family
nto:
- mapping or list keyed by slice index

### 4. Runtime discovery contract

Runtime changes belong in:
- `/home/dev/qflex_git/QPoints/gem5/configs/ruby/MOESI_CMP_directory.py`

#### Required behavior

1. discover slice-aware restore files generically from:
- `num_l2caches`
- protocol gem5_uarch directory

2. wire each `l2_cntrl{i}` only to:
- `...slice{i}.txt`

3. remove single-slice restore guards only after slice-aware wiring exists

#### Missing-file behavior

For each restore family:
- if all slice files are missing, warn once and skip that family
- if some slice files exist and others do not, treat missing files as empty restore sets for those slices
- do not fatal just because a specific slice file is empty or absent

This keeps sparse restore sets legal.

### 5. Directory-side first increment

For the first implementation increment:
- keep directory-side restore input global if that is simpler
- rely on `DirectoryMemory` range filtering

This is acceptable because the current blocker is on the L2 side.

Possible later cleanup:
- add per-slice directory files for symmetry and simpler reasoning

### 6. Test plan

#### Unit / converter tests

Update or add tests around:
- slice mapping helper
- per-slice file emission counts
- manifest recording for per-slice artifacts
- no hardcoded 8-slice assumptions

#### Ruby config tests

Add focused tests for:
- per-slice discovery with arbitrary `num_l2caches`
- correct file assignment to `l2_cntrl{i}`
- sparse slice-file handling

#### End-to-end validation

1. regenerate / convert `snapshot_0`
2. run MOESI gem5 smoke with:
- `num_l2caches = 8`
- `restore-llc-state`
3. verify the earlier fatal is gone
4. verify restore files are consumed slice-by-slice

### 7. Definition of done for the first implementation

The first implementation is done when all of the following are true:

1. `convert-single` emits per-slice MOESI L2-consumed restore files
2. `MOESI_CMP_directory.py` wires those files to the matching L2 controllers generically
3. the previous fatal for `--num-l2caches=8` is gone
4. a gem5 MOESI smoke with sliced LLC restore runs past startup restore
5. the implementation does not hardcode the current 8-slice setup

## Immediate next coding step

When coding begins, the first patch should be confined to the converter side:

- add a generic slice-mapping helper
- emit per-slice LLC restore files
- extend to the rest of the L2-consumed MOESI restore family in the same change if practical

Only after that should the Ruby config be updated to consume the new contract.
