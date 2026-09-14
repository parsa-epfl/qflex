#!/usr/bin/env bash
# Shared functions for the parity harness (temporary). Sourced by parity-<level>-tmp.sh and
# parity-switch-{pre,latest}-tmp.sh. Labels: pre | latest. Levels: idx | partition | partitions | fw | load | fw-all | all
set -euo pipefail

PARITY_ROOT=${PARITY_ROOT:-/mnt/sdc/testing/parity}
PARITY_REF_YAML=${PARITY_REF_YAML:-conf/MS/ms-multi.yaml}
PARITY_REF_IMAGES=${PARITY_REF_IMAGES:-/mnt/sdb/multi-node-experiments/image-1c/ms}
C_REPOS=(qemu-pdes parallel-qemu qemu qemu/middleware)
# The pre-first-commit set (= what built the on-disk reference run and the 3.36/3.37 images).
PRE_COMMITS=(6593f5e c2f32af c2059d44da 4b43c82)
LATEST_BRANCH=single-node-pdes

yaml() { echo "conf/MS/ms-multi-parity-$1.yaml"; }
capture() { for n in 0 1; do qemu-img snapshot -l "$PARITY_ROOT/images-$1/root-multi-node-$n.qcow2"; done > "$PARITY_ROOT/images-$1/snapshots_$2.txt"; }
dup() { local label=$1; shift; ./qflex duplicate-experiment -c "$(yaml "$label")" --target-yaml "$PARITY_REF_YAML" "$@"; }

# Put the four C repos at a label's commit set. Refuses on uncommitted tracked changes so nothing is lost.
switch_repos() {
    local i r target
    for r in "${C_REPOS[@]}"; do
        [[ -z $(git -C "$r" status --short -uno) ]] || { echo "$r has uncommitted changes — commit or stash first" >&2; exit 1; }
    done
    for i in "${!C_REPOS[@]}"; do
        r=${C_REPOS[$i]}; target=$LATEST_BRANCH; [[ $1 == pre ]] && target=${PRE_COMMITS[$i]}
        git -C "$r" checkout "$target"
        echo "$r -> $(git -C "$r" log --oneline -1)"
    done
}

# Stash the bind-mounted build outputs (the container-only *-saved/ trees are wiped by every build).
stash_build() {
    local dst=$PARITY_ROOT/builds/$1
    mkdir -p "$dst"
    cp -f parallel-qemu/build/qemu-system-aarch64 "$dst/qemu-system-aarch64"
    cp -f qemu/build/qemu-system-aarch64 "$dst/vanilla-qemu-system-aarch64"
    { date -Is; for r in "${C_REPOS[@]}"; do
        echo "$r $(git -C "$r" rev-parse HEAD) dirty_files=$(git -C "$r" status --short -uno | wc -l)"; done; } > "$dst/PROVENANCE.txt"
}

# Seed a label from the reference run (source is only read) with exactly the inputs the level needs,
# never its outputs. `results` = import the reference's diffable artifacts without running (no qcow2 copy).
seed() {
    local label=$1 level=$2
    local fresh=(--exclude '*.log' --exclude '*.err' --exclude log --exclude err --exclude .sentinels --exclude fp_gen_speed
                 --exclude 'core_info_*.csv' --exclude timing.csv --exclude core_info_new.csv --exclude '*_SAMPLE_SIZE')
    mkdir -p "$PARITY_ROOT/images-$label"
    case $level in
        results)
            dup "$label" --exclude init_warmed.mem --exclude loaded.zstd --exclude 'booted*' --exclude lib --exclude fp_gen_speed
            for n in 0 1; do qemu-img snapshot -l "$PARITY_REF_IMAGES/root-multi-node-$n.qcow2"; done > "$PARITY_ROOT/images-$label/snapshots_post.txt"
            return ;;
        idx|partition|partitions)   # timing only: keep the reference's partitioned FW snapshots, drop its timing results
            dup "$label" "${fresh[@]}" --exclude 'result_*' --exclude loaded.zstd --exclude 'booted*' ;;
        fw|fw-all)                  # from init_warmed: fresh FW output
            dup "$label" "${fresh[@]}" --exclude 'partition_*' --exclude 'snapshot_*' --exclude run_partitions.sh --exclude loaded.zstd --exclude 'booted*' ;;
        load)                       # from booted: fresh load output
            dup "$label" "${fresh[@]}" --exclude 'partition_*' --exclude 'snapshot_*' --exclude run_partitions.sh --exclude loaded.zstd --exclude 'init_warmed*' ;;
        all)                        # boot creates the folders itself; only the images are needed
            ;;
        *) echo "unknown level: $level" >&2; exit 2 ;;
    esac
    rsync -ah --info=progress2 "$PARITY_REF_IMAGES"/root-multi-node-{0,1}.qcow2 "$PARITY_ROOT/images-$label/"
}

# Foreground chain for the level; every phase logs into its experiment folder. Timing-only levels
# must not touch partition-cleanup/partition (they would wipe the seeded snapshots).
run() {
    local label=$1 container=$2 level=$3 phases y; y=$(yaml "$label")
    case $level in
        idx)        phases="run-idx" ;;
        partition)  phases="run-single-partition" ;;
        partitions) phases="run-partition result" ;;
        fw)         phases="fw" ;;
        load)       phases="load" ;;
        fw-all)     phases="fw partition-cleanup partition run-partition result" ;;
        all)        phases="boot load initialize fw partition-cleanup partition run-partition result" ;;
        *) echo "unknown level: $level" >&2; exit 2 ;;
    esac
    capture "$label" pre
    for phase in $phases; do
        ./dep exec --container-name "$container" --command "./qflex $phase -c $y"
    done
    capture "$label" post
}

diff_labels() { python -m tests.parity_diff --level "$1" --ref "$(yaml pre)" --cand "$(yaml latest)"; }

# Entry point shared by every parity-<level>-tmp.sh: `<label> <container>` seeds + runs, `diff` compares.
level_main() {
    local level=$1; shift
    if [[ ${1:-} == diff ]]; then diff_labels "$level"
    elif [[ $# -eq 2 ]]; then seed "$1" "$level"; run "$1" "$2" "$level"
    else echo "usage: $0 <pre|latest> <container>  |  $0 diff" >&2; exit 2; fi
}
