"""
Parity diff between two labels of the same experiment (read-only, host-side) — NOT a pytest test.

    python -m tests.parity_diff --level <idx|partition|partitions|fw|load|fw-all|all> --ref <yaml> --cand <yaml>

Per node, the level selects which outputs must match byte-for-byte:
  timing  — `run/partition_<P>/result_<i>/all.measurement.*.log` (one idx, one partition, or all)
  result  — `timing.csv` (sorted), `core_info_new.csv`, `REQUIRED_SAMPLE_SIZE`
  fw      — snapshot name set, `snapshot_*.loc`, `snapshot_*_in_flight.json`, `.uarch/statistics.csv`
            minus its host-timestamp column, and the `snapshot_*` qcow2 tag VM-clocks
  tag     — a named checkpoint (booted / loaded / init_warmed): qcow2 VM-clock + `<name>_in_flight.json`
`run/core_info.csv` (the input IPNS) is checked at every level. Exit 1 on any difference.
"""

import argparse
import filecmp
import glob
import os
import re
import sys

from tests.conftest import REPO_ROOT
from dep_injection.builder import build_experiment_context

VM_CLOCK = re.compile(r"\d\d:\d\d:\d\d\.\d+")
LEVELS = {
    "idx":        ["timing_one"],
    "partition":  ["timing_partition"],
    "partitions": ["timing_all", "result"],
    "fw":         ["fw"],
    "load":       ["tag:loaded"],
    "fw-all":     ["fw", "timing_all", "result"],
    "all":        ["tag:booted", "tag:loaded", "tag:init_warmed", "fw", "timing_all", "result"],
}


def leaves(yaml_path: str, cmd_name: str) -> dict:
    ctx = build_experiment_context(os.path.join(REPO_ROOT, yaml_path), cmd_name=cmd_name)
    return {s.node_number: s for s in ctx.sub_experiments}


def by_name(root: str, pattern: str) -> dict:
    return {os.path.basename(p): p for p in glob.glob(f"{root}/run/**/{pattern}", recursive=True)}


def stats_rows(path: str) -> list:
    with open(path) as f:
        return [",".join(line.split(",")[1:]) for line in f]   # drop the `ts` column


def tag_clocks(image_folder: str) -> dict:
    path, clocks = f"{image_folder}/snapshots_post.txt", {}
    if not os.path.exists(path):
        return clocks
    with open(path) as f:
        for line in f:
            parts = line.split()
            if len(parts) > 2:
                clocks[parts[1]] = next((t for t in parts if VM_CLOCK.fullmatch(t)), None)
    return clocks


def compare_sets(label: str, a: dict, b: dict, diffs: list, content: bool):
    missing = sorted(set(a) ^ set(b))
    bad = [n for n in sorted(set(a) & set(b)) if not filecmp.cmp(a[n], b[n], shallow=False)] if content else []
    if missing:
        diffs.append(f"{label}: name set differs ({len(a)} vs {len(b)}): {missing[:5]}")
    if bad:
        diffs.append(f"{label}: {len(bad)} files differ in content, e.g. {bad[:5]}")
    print(f"  {label}: {len(a)} vs {len(b)} {'DIFF' if missing or bad else 'ok'}")


def check_timing(r: str, c: str, diffs: list, partition=None, idx=None):
    part = f"partition_{partition}" if partition is not None else "partition_*"
    res = f"result_{idx}" if idx is not None else "result_*"
    rl = {os.path.relpath(p, r): p for p in glob.glob(f"{r}/run/{part}/{res}/all.measurement.*.log")}
    cl = {os.path.relpath(p, c): p for p in glob.glob(f"{c}/run/{part}/{res}/all.measurement.*.log")}
    compare_sets(f"timing {part}/{res} all.measurement.*.log", rl, cl, diffs, content=True)


def check_result(r: str, c: str, diffs: list):
    for name in ("core_info_new.csv", "REQUIRED_SAMPLE_SIZE"):
        compare_sets(f"result {name}", {name: f"{r}/{name}"}, {name: f"{c}/{name}"}, diffs, content=True)
    with open(f"{r}/timing.csv") as fr, open(f"{c}/timing.csv") as fc:
        same = sorted(fr) == sorted(fc)
    print(f"  result timing.csv (sorted): {'ok' if same else 'DIFF'}")
    if not same:
        diffs.append("timing.csv rows differ")


def check_fw(ref, cand, diffs: list):
    r, c = ref.get_experiment_folder_address(), cand.get_experiment_folder_address()
    compare_sets("fw snapshot_*.state.zstd", by_name(r, "snapshot_*.state.zstd"), by_name(c, "snapshot_*.state.zstd"), diffs, content=False)
    compare_sets("fw snapshot_*.loc", by_name(r, "snapshot_*.loc"), by_name(c, "snapshot_*.loc"), diffs, content=True)
    compare_sets("fw snapshot_*_in_flight.json", by_name(r, "snapshot_*_in_flight.json"), by_name(c, "snapshot_*_in_flight.json"), diffs, content=True)
    ru, cu = by_name(r, "snapshot_*.uarch"), by_name(c, "snapshot_*.uarch")
    bad = [n for n in sorted(set(ru) & set(cu)) if stats_rows(f"{ru[n]}/statistics.csv") != stats_rows(f"{cu[n]}/statistics.csv")]
    print(f"  fw .uarch/statistics.csv (ts dropped): {len(bad)} differ")
    if bad:
        diffs.append(f"uarch statistics differ for {bad[:5]}")
    rc = {t: v for t, v in tag_clocks(ref.image_folder).items() if t.startswith("snapshot_")}
    cc = {t: v for t, v in tag_clocks(cand.image_folder).items() if t.startswith("snapshot_")}
    bad = sorted(t for t in set(rc) & set(cc) if rc[t] != cc[t])
    print(f"  fw qcow2 snapshot_* VM clocks: {len(rc)} vs {len(cc)} tags, {len(bad)} differ")
    if bad or set(rc) != set(cc):
        diffs.append(f"qcow2 snapshot_* VM clocks differ for {bad[:5]} (tag sets {len(rc)} vs {len(cc)})")


def check_tag(ref, cand, name: str, diffs: list):
    rc, cc = tag_clocks(ref.image_folder).get(name), tag_clocks(cand.image_folder).get(name)
    print(f"  tag {name} VM clock: {rc} vs {cc} {'ok' if rc == cc and rc else 'DIFF'}")
    if rc != cc or not rc:
        diffs.append(f"tag {name}: VM clock {rc} vs {cc}")
    r, c = ref.get_experiment_folder_address(), cand.get_experiment_folder_address()
    compare_sets(f"tag {name}_in_flight.json", by_name(r, f"{name}_in_flight.json"), by_name(c, f"{name}_in_flight.json"), diffs, content=True)
    if name == "init_warmed":
        rs, cs = f"{r}/run/init_warmed.uarch/statistics.csv", f"{c}/run/init_warmed.uarch/statistics.csv"
        same = stats_rows(rs) == stats_rows(cs)
        print(f"  tag init_warmed .uarch/statistics.csv (ts dropped): {'ok' if same else 'DIFF'}")
        if not same:
            diffs.append("init_warmed uarch statistics differ")


def compare_node(ref, cand, checks: list) -> list:
    diffs = []
    r, c = ref.get_experiment_folder_address(), cand.get_experiment_folder_address()
    for chk in checks:
        if chk == "timing_one":
            check_timing(r, c, diffs, partition=cand.partition_number, idx=cand.idx)
        elif chk == "timing_partition":
            check_timing(r, c, diffs, partition=cand.partition_number)
        elif chk == "timing_all":
            check_timing(r, c, diffs)
        elif chk == "result":
            check_result(r, c, diffs)
        elif chk == "fw":
            check_fw(ref, cand, diffs)
        elif chk.startswith("tag:"):
            check_tag(ref, cand, chk[4:], diffs)
    if not filecmp.cmp(f"{r}/run/core_info.csv", f"{c}/run/core_info.csv", shallow=False):
        diffs.append("run/core_info.csv (input IPNS) differs — runs are not comparable")
    return diffs


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--level", required=True, choices=sorted(LEVELS))
    ap.add_argument("--ref", required=True)
    ap.add_argument("--cand", required=True)
    args = ap.parse_args()
    # run_idx / run_single_partition phase overlays carry partition_number / idx for the one-idx levels.
    cmd_name = {"idx": "run_idx", "partition": "run_single_partition"}.get(args.level, "result")
    ref, cand = leaves(args.ref, cmd_name), leaves(args.cand, cmd_name)
    assert set(ref) == set(cand), f"node sets differ: {sorted(ref)} vs {sorted(cand)}"
    all_diffs = []
    for n in sorted(ref):
        if cand[n].all_phantom_cores and any(c.startswith("timing") or c == "result" for c in LEVELS[args.level]):
            print(f"node {n}: all-phantom, timing/result checks skipped")
        print(f"node {n}: {ref[n].get_experiment_folder_address()}  vs  {cand[n].get_experiment_folder_address()}")
        checks = [c for c in LEVELS[args.level]
                  if not (cand[n].all_phantom_cores and (c.startswith("timing") or c == "result"))]
        all_diffs += [f"node {n}: {x}" for x in compare_node(ref[n], cand[n], checks)]
    print(f"\nPARITY[{args.level}]: " + ("MATCH" if not all_diffs else "DIFF"))
    for d in all_diffs:
        print("  " + d)
    return 1 if all_diffs else 0


if __name__ == "__main__":
    sys.exit(main())
