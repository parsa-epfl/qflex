#!/usr/bin/env python3

# collect the experiment results.

import glob
import json
import os
import csv

MAX_CORES = 256  # Maximum expected cores

# Top-down CPI-stack buckets aggregated from uarch-TB:<class>:Bkd:<reason> lines.
# TB values are proportional stall attribution (NOT cycles) — normalize to fractions downstream.
TB_BUCKETS = [
    "tb_frontend", "tb_branch", "tb_backend", "tb_mem_local", "tb_mem_remote",
    "tb_sbdrain", "tb_spin", "tb_interrupt", "tb_other",
]


def tb_bucket(reason: str) -> str:
    """Map a TB Bkd:<reason> tail to one top-down bucket (precedence matters)."""
    if reason.startswith("Spin"):
        return "tb_spin"
    if "Interrupt" in reason:
        return "tb_interrupt"
    if reason.startswith("Branch"):
        return "tb_branch"
    if "SBDrain" in reason or reason.startswith("Store:SBFull"):
        return "tb_sbdrain"
    if reason.startswith("EmptyROB"):
        return "tb_frontend"
    if reason.startswith("Dataflow") or reason == "Busy":
        return "tb_backend"
    if reason.startswith(("Load", "Store", "Atomic", "SideEffect:Load",
                          "SideEffect:Store", "SideEffect:Atomic", "Forwarded")):
        return "tb_mem_remote" if ("Remote" in reason or "PeerL" in reason) else "tb_mem_local"
    return "tb_other"


def parse_core_idx(line):
    """Extract core index from a log line."""
    core_idx_str = line.split()[0].split("-")[0]
    if core_idx_str == "000":
        return 0
    else:
        return int(core_idx_str.lstrip("0"))


def parse_asid_json(asid_path, core_id):
    """Parse .asid.json file and return ASID for given core.

    Returns ASID value, or -1 if file doesn't exist, can't be parsed,
    or the core is not found in the file.
    """
    if not os.path.exists(asid_path):
        return -1

    try:
        with open(asid_path, "r") as f:
            data = json.load(f)

        if "cores" not in data:
            return -1

        for core_info in data["cores"]:
            if core_info.get("core_id") == core_id:
                asid = core_info.get("asid")
                if asid is None or not isinstance(asid, int) or asid < 0:
                    return -1
                return asid
    except Exception:
        return -1

    return -1


def parse_csv_file(csv_path, actual_core_count):
    """Parse CSV file and extract required statistics per core.

    Returns a dictionary with core_id as key and statistics as values.
    """
    csv_stats = {
        "bx_instruction": [-1] * MAX_CORES,
        "bx_instruction_access": [-1] * MAX_CORES,
        "bx_data_access": [-1] * MAX_CORES,
        "bx_private_icache_miss": [-1] * MAX_CORES,
        "bx_private_dcache_miss": [-1] * MAX_CORES,
        "bx_shared_cache_miss": [-1] * MAX_CORES,
        "bx_branch_count": [-1] * MAX_CORES,
        "bx_bp_miss": [-1] * MAX_CORES,
        "bx_tlb_miss": [-1] * MAX_CORES,
        "virtio_blk_read": [-1] * MAX_CORES,
        "virtio_blk_write": [-1] * MAX_CORES,
        "virtio_complete": [-1] * MAX_CORES,
        "bx_drain_pipeline": [-1] * MAX_CORES,
        "bx_drain_store_buffer": [-1] * MAX_CORES,
        "bx_read_noc_hop": [-1] * MAX_CORES,
        "bx_write_noc_hop": [-1] * MAX_CORES,
        "bx_instruction_u": [-1] * MAX_CORES,
        "bx_instruction_k": [-1] * MAX_CORES,
        "bx_private_dcache_miss_load": [-1] * MAX_CORES,
        "bx_private_dcache_miss_store": [-1] * MAX_CORES,
        "bx_private_dcache_miss_ptw": [-1] * MAX_CORES,
        "bx_ifetch_noc_hop": [-1] * MAX_CORES,
        "bx_shared_cache_miss_write": [-1] * MAX_CORES,
        "bx_shared_cache_miss_ifetch": [-1] * MAX_CORES,
        "bx_shared_cache_miss_read": [-1] * MAX_CORES,
    }

    if not os.path.exists(csv_path):
        # Return empty stats if CSV doesn't exist
        return csv_stats

    try:
        with open(csv_path, "r") as f:
            reader = csv.DictReader(f)
            for row in reader:
                core_id = int(row.get("core_id", -1))
                if core_id < 0 or core_id >= actual_core_count:
                    continue

                # Extract total counts (not user or kernel specific)
                if "Instruction" in row:
                    csv_stats["bx_instruction"][core_id] = int(row["Instruction"])
                if "InstructionAccess" in row:
                    csv_stats["bx_instruction_access"][core_id] = int(
                        row["InstructionAccess"]
                    )
                if "DataAccess" in row:
                    csv_stats["bx_data_access"][core_id] = int(row["DataAccess"])
                if "PrivateICacheMiss" in row:
                    csv_stats["bx_private_icache_miss"][core_id] = int(
                        row["PrivateICacheMiss"]
                    )
                if "PrivateDCacheMiss" in row:
                    csv_stats["bx_private_dcache_miss"][core_id] = int(
                        row["PrivateDCacheMiss"]
                    )
                if "SharedCacheMiss" in row:
                    csv_stats["bx_shared_cache_miss"][core_id] = int(
                        row["SharedCacheMiss"]
                    )
                if "BranchCount" in row:
                    csv_stats["bx_branch_count"][core_id] = int(row["BranchCount"])
                if "BPMiss" in row:
                    csv_stats["bx_bp_miss"][core_id] = int(row["BPMiss"])
                if "TLBMiss" in row:
                    csv_stats["bx_tlb_miss"][core_id] = int(row["TLBMiss"])
                if "VirtIOBlkRead" in row:
                    csv_stats["virtio_blk_read"][core_id] = int(row["VirtIOBlkRead"])
                if "VirtIOBlkWrite" in row:
                    csv_stats["virtio_blk_write"][core_id] = int(row["VirtIOBlkWrite"])
                if "VirtIOComplete" in row:
                    csv_stats["virtio_complete"][core_id] = int(row["VirtIOComplete"])

                if "DrainStoreBuffer" in row:
                    csv_stats["bx_drain_store_buffer"][core_id] = int(
                        row["DrainStoreBuffer"]
                    )

                if "DrainPipeline" in row:
                    csv_stats["bx_drain_pipeline"][core_id] = int(row["DrainPipeline"])

                if "ReadHopCount" in row:
                    csv_stats["bx_read_noc_hop"][core_id] = int(row["ReadHopCount"])

                if "WriteHopCount" in row:
                    csv_stats["bx_write_noc_hop"][core_id] = int(row["WriteHopCount"])

                if "Instruction:u" in row:
                    csv_stats["bx_instruction_u"][core_id] = int(row["Instruction:u"])

                if "Instruction:k" in row:
                    csv_stats["bx_instruction_k"][core_id] = int(row["Instruction:k"])

                if "PrivateDCacheMissDueToLoad" in row:
                    csv_stats["bx_private_dcache_miss_load"][core_id] = int(
                        row["PrivateDCacheMissDueToLoad"]
                    )

                if "PrivateDCacheMissDueToStore" in row:
                    csv_stats["bx_private_dcache_miss_store"][core_id] = int(
                        row["PrivateDCacheMissDueToStore"]
                    )

                if "PrivateDCacheMissDueToPTW" in row:
                    csv_stats["bx_private_dcache_miss_ptw"][core_id] = int(
                        row["PrivateDCacheMissDueToPTW"]
                    )

                if "InstructionFetchHopCount" in row:
                    csv_stats["bx_ifetch_noc_hop"][core_id] = int(
                        row["InstructionFetchHopCount"]
                    )

                if "SharedCacheMissDueToDataWrite" in row:
                    csv_stats["bx_shared_cache_miss_write"][core_id] = int(
                        row["SharedCacheMissDueToDataWrite"]
                    )

                if "SharedCacheMissDueToInstructionFetch" in row:
                    csv_stats["bx_shared_cache_miss_ifetch"][core_id] = int(
                        row["SharedCacheMissDueToInstructionFetch"]
                    )

                if "SharedCacheMissDueToDataRead" in row:
                    csv_stats["bx_shared_cache_miss_read"][core_id] = int(
                        row["SharedCacheMissDueToDataRead"]
                    )

    except Exception as e:
        print(f"Warning: Error parsing {csv_path}: {e}")

    return csv_stats


def get_measurement_points(folder_name: str):
    """Discover all measurement points in a folder by scanning .log filenames."""
    points = []
    for log_file in glob.glob(f"{folder_name}/all.measurement.*.log"):
        basename = os.path.basename(log_file)
        # Strip prefix and suffix: all.measurement.0000100000.log -> 0000100000
        point_str = basename.replace("all.measurement.", "").replace(".log", "")
        if point_str == "end":
            continue
        try:
            points.append(int(point_str))
        except ValueError:
            print(f"Warning: Could not parse point from filename {basename}")
    return sorted(set(points))


def parse_one_result(folder_name: str):
    result = []
    for point in get_measurement_points(folder_name):
        # Initialize all statistics with -1 to detect actual core count
        warm_instruction = [-1] * MAX_CORES
        warm_instruction_u = [-1] * MAX_CORES

        itlb_misses = [-1] * MAX_CORES
        dtlb_misses = [-1] * MAX_CORES
        stlb_misses = [-1] * MAX_CORES

        btb_misses = [-1] * MAX_CORES
        tage_misses = [-1] * MAX_CORES

        l1i_misses = [-1] * MAX_CORES
        l1d_misses = [-1] * MAX_CORES

        llc_read_misses = [-1] * MAX_CORES
        llc_write_misses = [-1] * MAX_CORES

        halted_cycles = [-1] * MAX_CORES

        core_cycles = [-1] * MAX_CORES  # New: extract cycles from log

        # Additive diagnostic instrumentation (log-only; existing columns unchanged).
        spin_cycles = [-1] * MAX_CORES
        wfi_cycles = [-1] * MAX_CORES
        spins = [-1] * MAX_CORES
        commits_nonspin_system = [-1] * MAX_CORES
        commits_spin_user = [-1] * MAX_CORES
        commits_spin_system = [-1] * MAX_CORES
        nic_sent = [-1] * MAX_CORES
        nic_recv = [-1] * MAX_CORES
        maf = [-1.0] * MAX_CORES
        tb = {b: [0] * MAX_CORES for b in TB_BUCKETS}

        log_file = f"{folder_name}/all.measurement.{point:010}.log"
        with open(log_file) as f:
            for line in f:
                if "-uarch-Commits" in line and not ":" in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    warm_instruction[core_idx] = int(line.split()[1])

                if "-uarch-Commits:NonSpin:User" in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    warm_instruction_u[core_idx] = int(line.split()[1])

                if "mmu-itlb_misses" in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    itlb_misses[core_idx] = int(line.split()[1])

                if "mmu-dtlb_misses" in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    dtlb_misses[core_idx] = int(line.split()[1])

                if "mmu-stlb_misses" in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    stlb_misses[core_idx] = int(line.split()[1])

                if "-fag-mispredict:BTB " in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    btb_misses[core_idx] = int(line.split()[1])

                if "-fag-mispredict:TAGE " in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    tage_misses[core_idx] = int(line.split()[1])

                if "ufetch-Misses " in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    l1i_misses[core_idx] = int(line.split()[1])

                if "L1d-Misses  " in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    l1d_misses[core_idx] = int(line.split()[1])

                if "L2-ReadMissMemory" in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    llc_read_misses[core_idx] = int(line.split()[1])

                if "L2-WriteMissMemory" in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    llc_write_misses[core_idx] = int(line.split()[1])

                # TODO: Flexus emits no -uarch-HaltedCycles (real stat is -uarch-WFICycles),
                # so halted_cycles is always 0. Not fixed here: result_new.py's default IPNS uses
                # (cycles - halted_cycles), so correcting the name would shift every reported IPNS.
                if "-uarch-HaltedCycles" in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    halted_cycles[core_idx] = int(line.split()[1])

                # Extract core cycles (looking for "-uarch-Cycles " with space)
                if "-uarch-Cycles " in line and "-uarch-Cycles:" not in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    core_cycles[core_idx] = int(line.split()[1])

                # --- Additive diagnostic scrapes (log-only) ---
                if "-uarch-SpinCycles " in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    spin_cycles[core_idx] = int(line.split()[1])

                if "-uarch-WFICycles " in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    wfi_cycles[core_idx] = int(line.split()[1])

                if "-uarch-Spins " in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    spins[core_idx] = int(line.split()[1])

                if "-uarch-Commits:NonSpin:System" in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    commits_nonspin_system[core_idx] = int(line.split()[1])

                if "-uarch-Commits:Spin:User" in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    commits_spin_user[core_idx] = int(line.split()[1])

                if "-uarch-Commits:Spin:System" in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    commits_spin_system[core_idx] = int(line.split()[1])

                if "-nic-MsgsSent " in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    nic_sent[core_idx] = int(line.split()[1])

                if "-nic-MsgsReceived " in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    nic_recv[core_idx] = int(line.split()[1])

                if "-L1d-MAFAvg:Total " in line:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    val = line.split()[1]
                    # MAFAvg can be "{nan}" when a core had no L1d reads — record 0 there.
                    maf[core_idx] = float(val) if val[0].isdigit() else 0.0

                if "-uarch-TB:" in line and ":Bkd:" in line and len(line.split()) >= 2:
                    core_idx = parse_core_idx(line)
                    if core_idx >= MAX_CORES:
                        continue
                    reason = line.split()[0].split(":Bkd:", 1)[1]
                    tb[tb_bucket(reason)][core_idx] += int(line.split()[1])

        # Determine actual core count from log file data
        actual_core_count = 0
        for i in range(MAX_CORES):
            if warm_instruction[i] != -1:
                actual_core_count = i + 1

        if actual_core_count == 0:
            # No data found in this log file
            continue

        # Parse corresponding CSV file
        csv_file = f"{folder_name}/all.measurement.{point:010}.csv"
        csv_stats = parse_csv_file(csv_file, actual_core_count)

        # Parse corresponding ASID JSON file
        asid_file = f"{folder_name}/all.measurement.{point:010}.asid.json"

        for core_id in range(actual_core_count):
            if warm_instruction[core_id] == -1:
                continue

            asid = parse_asid_json(asid_file, core_id)

            result.append(
                [
                    folder_name.split("_")[-1].split(".")[0],
                    core_id,
                    asid,
                    point,
                    warm_instruction[core_id],
                    warm_instruction_u[core_id]
                    if warm_instruction_u[core_id] != -1
                    else 0,
                    itlb_misses[core_id] if itlb_misses[core_id] != -1 else 0,
                    dtlb_misses[core_id] if dtlb_misses[core_id] != -1 else 0,
                    stlb_misses[core_id] if stlb_misses[core_id] != -1 else 0,
                    btb_misses[core_id] if btb_misses[core_id] != -1 else 0,
                    tage_misses[core_id] if tage_misses[core_id] != -1 else 0,
                    l1i_misses[core_id] if l1i_misses[core_id] != -1 else 0,
                    l1d_misses[core_id] if l1d_misses[core_id] != -1 else 0,
                    (llc_read_misses[core_id] if llc_read_misses[core_id] != -1 else 0)
                    + (
                        llc_write_misses[core_id]
                        if llc_write_misses[core_id] != -1
                        else 0
                    ),
                    halted_cycles[core_id] if halted_cycles[core_id] != -1 else 0,
                    core_cycles[core_id] if core_cycles[core_id] != -1 else 0,
                    # CSV statistics (use 0 if not available)
                    csv_stats["virtio_blk_read"][core_id]
                    if csv_stats["virtio_blk_read"][core_id] != -1
                    else 0,
                    csv_stats["virtio_blk_write"][core_id]
                    if csv_stats["virtio_blk_write"][core_id] != -1
                    else 0,
                    csv_stats["virtio_complete"][core_id]
                    if csv_stats["virtio_complete"][core_id] != -1
                    else 0,
                    csv_stats["bx_instruction"][core_id]
                    if csv_stats["bx_instruction"][core_id] != -1
                    else 0,
                    csv_stats["bx_instruction_access"][core_id]
                    if csv_stats["bx_instruction_access"][core_id] != -1
                    else 0,
                    csv_stats["bx_data_access"][core_id]
                    if csv_stats["bx_data_access"][core_id] != -1
                    else 0,
                    csv_stats["bx_private_icache_miss"][core_id]
                    if csv_stats["bx_private_icache_miss"][core_id] != -1
                    else 0,
                    csv_stats["bx_private_dcache_miss"][core_id]
                    if csv_stats["bx_private_dcache_miss"][core_id] != -1
                    else 0,
                    csv_stats["bx_shared_cache_miss"][core_id]
                    if csv_stats["bx_shared_cache_miss"][core_id] != -1
                    else 0,
                    csv_stats["bx_branch_count"][core_id]
                    if csv_stats["bx_branch_count"][core_id] != -1
                    else 0,
                    csv_stats["bx_bp_miss"][core_id]
                    if csv_stats["bx_bp_miss"][core_id] != -1
                    else 0,
                    csv_stats["bx_tlb_miss"][core_id]
                    if csv_stats["bx_tlb_miss"][core_id] != -1
                    else 0,
                    csv_stats["bx_drain_pipeline"][core_id]
                    if csv_stats["bx_drain_pipeline"][core_id] != -1
                    else 0,
                    csv_stats["bx_drain_store_buffer"][core_id]
                    if csv_stats["bx_drain_store_buffer"][core_id] != -1
                    else 0,
                    csv_stats["bx_read_noc_hop"][core_id]
                    if csv_stats["bx_read_noc_hop"][core_id] != -1
                    else 0,
                    csv_stats["bx_write_noc_hop"][core_id]
                    if csv_stats["bx_write_noc_hop"][core_id] != -1
                    else 0,
                    csv_stats["bx_instruction_u"][core_id]
                    if csv_stats["bx_instruction_u"][core_id] != -1
                    else 0,
                    csv_stats["bx_instruction_k"][core_id]
                    if csv_stats["bx_instruction_k"][core_id] != -1
                    else 0,
                    csv_stats["bx_private_dcache_miss_load"][core_id]
                    if csv_stats["bx_private_dcache_miss_load"][core_id] != -1
                    else 0,
                    csv_stats["bx_private_dcache_miss_store"][core_id]
                    if csv_stats["bx_private_dcache_miss_store"][core_id] != -1
                    else 0,
                    csv_stats["bx_private_dcache_miss_ptw"][core_id]
                    if csv_stats["bx_private_dcache_miss_ptw"][core_id] != -1
                    else 0,
                    (csv_stats["bx_private_dcache_miss_load"][core_id] if csv_stats["bx_private_dcache_miss_load"][core_id] != -1 else 0)
                    + (csv_stats["bx_private_dcache_miss_ptw"][core_id] if csv_stats["bx_private_dcache_miss_ptw"][core_id] != -1 else 0),
                    csv_stats["bx_ifetch_noc_hop"][core_id]
                    if csv_stats["bx_ifetch_noc_hop"][core_id] != -1
                    else 0,
                    csv_stats["bx_shared_cache_miss_write"][core_id]
                    if csv_stats["bx_shared_cache_miss_write"][core_id] != -1
                    else 0,
                    csv_stats["bx_shared_cache_miss_ifetch"][core_id]
                    if csv_stats["bx_shared_cache_miss_ifetch"][core_id] != -1
                    else 0,
                    csv_stats["bx_shared_cache_miss_read"][core_id]
                    if csv_stats["bx_shared_cache_miss_read"][core_id] != -1
                    else 0,
                    # --- Additive diagnostic columns (log-only) ---
                    spin_cycles[core_id] if spin_cycles[core_id] != -1 else 0,
                    wfi_cycles[core_id] if wfi_cycles[core_id] != -1 else 0,
                    spins[core_id] if spins[core_id] != -1 else 0,
                    commits_nonspin_system[core_id]
                    if commits_nonspin_system[core_id] != -1
                    else 0,
                    commits_spin_user[core_id] if commits_spin_user[core_id] != -1 else 0,
                    commits_spin_system[core_id]
                    if commits_spin_system[core_id] != -1
                    else 0,
                    nic_sent[core_id] if nic_sent[core_id] != -1 else 0,
                    nic_recv[core_id] if nic_recv[core_id] != -1 else 0,
                    maf[core_id] if maf[core_id] != -1 else 0,
                    *(tb[b][core_id] for b in TB_BUCKETS),
                ]
            )
    return result


# find all result_* folders recursively.
folders = []
for root, dirs, files in os.walk("."):
    for dir in dirs:
        if dir.startswith("result_"):
            folders.append(os.path.join(root, dir))

# Parse all results and merge all results into one list.
all_results = []
for folder in folders:
    print(f"Processing {folder}", end="\r")
    all_results.extend(parse_one_result(folder))

# Write the result to a csv file.
with open("timing.csv", "w") as f:
    f.write(
        "snapshot_id,core,asid,sys_cycles,instruction,instruction:u,itlb_miss,dtlb_miss,stlb_miss,btb_miss,tage_miss,l1i_miss,l1d_miss,l2_miss,halted_cycles,core_cycles,virtio_blk_read,virtio_blk_write,virtio_complete,bx_instruction,bx_instruction_access,bx_data_access,bx_private_icache_miss,bx_private_dcache_miss,bx_shared_cache_miss,bx_branch_count,bx_bp_miss,bx_tlb_miss,bx_drain_pipeline,bx_drain_store_buffer,bx_read_noc_hop,bx_write_noc_hop,bx_instruction_u,bx_instruction_k,bx_private_dcache_miss_load,bx_private_dcache_miss_store,bx_private_dcache_miss_ptw,bx_private_dcache_miss_load_ptw,bx_ifetch_noc_hop,bx_shared_cache_miss_write,bx_shared_cache_miss_ifetch,bx_shared_cache_miss_read,spin_cycles,wfi_cycles,spins,commits_nonspin_system,commits_spin_user,commits_spin_system,nic_sent,nic_recv,maf,"
        + ",".join(TB_BUCKETS)
        + "\n"
    )
    for result in all_results:
        f.write(",".join([str(x) for x in result]) + "\n")
