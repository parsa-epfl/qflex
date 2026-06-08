#!/usr/bin/env python3

import pandas as pd
import numpy as np
import argparse
import math
import sys
import os
from pathlib import Path
import plotille
from rich.console import Console
from rich.table import Table
from rich.panel import Panel
from rich.text import Text
from rich import box
from rich.align import Align

# Global constants
INTERVAL = 100000
FREQ_GHZ = 2.0  # GHz; overridden by --freq-ghz (the experiment's machine_freq_ghz)
# TODO: the `* FREQ_GHZ` factor was commented out of the IPC denominators below so this
# script matches result.py (treats INTERVAL as cycles, not ns). Was making every value
# half. Check later why it was there — i.e. whether sys_cycles/INTERVAL is really ns.

console = Console()


def parse_core_groups(core_groups_str: str) -> list[list[int]]:
    """
    Parse core groups specification like "0-7,8-15" into list of core lists.

    Args:
        core_groups_str: String specifying core groups, e.g., "0-7,8-15" or "0,1,2,3-5"

    Returns:
        List of lists, where each inner list contains core indices for that group
    """
    if not core_groups_str:
        return []

    groups = []
    for group_str in core_groups_str.split(","):
        group_str = group_str.strip()
        if "-" in group_str:
            # Range specification like "0-7"
            start, end = map(int, group_str.split("-"))
            cores = list(range(start, end + 1))
        else:
            # Single core specification like "5"
            cores = [int(group_str)]
        groups.append(cores)

    return groups


class MeasurementData:
    """Container for parsed measurement data to avoid repeated parsing."""

    def __init__(
        self,
        instructions: np.ndarray,
        instructions_u: np.ndarray,
        halted_cycles: np.ndarray,
        result_folders: list[str],
        sampling_unit_size: int,
    ):
        self.instructions = (
            instructions  # instructions[snapshot_idx, sampling_unit_idx, core]
        )
        self.instructions_u = (
            instructions_u  # instructions_u[snapshot_idx, sampling_unit_idx, core]
        )
        self.halted_cycles = (
            halted_cycles  # halted_cycles[snapshot_idx, sampling_unit_idx, core]
        )
        self.result_folders = result_folders
        self.sampling_unit_size = sampling_unit_size


def save_measurement_data(measurement_data: MeasurementData, file_path: str) -> None:
    """
    Save measurement data to an NPZ file for faster future loading.

    Args:
        measurement_data: MeasurementData container to save
        file_path: Path to the output NPZ file
    """
    np.savez(
        file_path,
        instructions=measurement_data.instructions,
        instructions_u=measurement_data.instructions_u,
        halted_cycles=measurement_data.halted_cycles,
        result_folders=measurement_data.result_folders,
        sampling_unit_size=measurement_data.sampling_unit_size,
    )
    console.print(f"[green]Measurement data saved to {file_path}[/green]")


def load_measurement_data(file_path: str) -> MeasurementData:
    """
    Load measurement data from an NPZ file.

    Args:
        file_path: Path to the NPZ file to load

    Returns:
        MeasurementData container with loaded data
    """
    if not os.path.exists(file_path):
        console.print(f"[red]Error: NPZ file {file_path} does not exist[/red]")
        sys.exit(1)

    data = np.load(file_path, allow_pickle=True)

    result_folders = data["result_folders"].tolist()
    sampling_unit_size = int(data["sampling_unit_size"])

    measurement_data = MeasurementData(
        instructions=data["instructions"],
        instructions_u=data["instructions_u"],
        halted_cycles=data["halted_cycles"],
        result_folders=result_folders,
        sampling_unit_size=sampling_unit_size,
    )

    console.print(f"[green]Loaded measurement data from {file_path}[/green]")
    console.print(f"[green]Snapshots: {measurement_data.instructions.shape[0]}[/green]")
    console.print(
        f"[green]Sampling units: {measurement_data.instructions.shape[1]}[/green]"
    )
    console.print(f"[green]Cores: {measurement_data.instructions.shape[2]}[/green]")

    return measurement_data


def parse_measurements_from_csv(
    csv_path: str, sampling_unit_size: int = 1
) -> MeasurementData:
    """
    Build a MeasurementData container from a timing.csv file produced by collect.py.

    Args:
        csv_path: Path to timing.csv
        sampling_unit_size: Size of each sampling unit in multiples of INTERVAL

    Returns:
        MeasurementData with 3D arrays of shape [snapshot_idx, sampling_unit_idx, core]:
            instructions    — delta instruction counts per sampling unit
            instructions_u  — delta user-mode instruction counts per sampling unit
            halted_cycles   — delta halted cycles per sampling unit
    """
    if not os.path.exists(csv_path):
        console.print(f"[red]Error: {csv_path} does not exist[/red]")
        sys.exit(1)

    df = pd.read_csv(csv_path)

    # Keep only regular measurement points (multiples of INTERVAL).
    # ASID-change boundary rows land at non-multiples and have valid ASIDs,
    # so they cannot be filtered by asid alone.
    df = df[df["sys_cycles"] % INTERVAL == 0].copy()

    if df.empty:
        console.print(
            "[red]Error: No regular measurement rows found in timing.csv after filtering[/red]"
        )
        sys.exit(1)

    # Re-map snapshot_id to dense 0-based sequential indices.
    # Folder names like result_3, result_7, ... produce non-contiguous IDs
    # that cannot be used directly as numpy array indices.
    unique_snapshot_ids = sorted(df["snapshot_id"].unique())
    snapshot_id_to_idx = {sid: idx for idx, sid in enumerate(unique_snapshot_ids)}
    df["snapshot_idx"] = df["snapshot_id"].map(snapshot_id_to_idx)

    # Convert sys_cycles to 0-based sampling_unit_idx:
    #   sys_cycles=100000 → 0,  200000 → 1,  300000 → 2, …
    df["sampling_unit_idx"] = df["sys_cycles"] // INTERVAL - 1

    # Derive dimensions from data — no hardcoded END or CORE_COUNT.
    num_snapshots = len(unique_snapshot_ids)
    num_raw_points = int(df["sampling_unit_idx"].max()) + 1
    num_cores = int(df["core"].max()) + 1

    console.print(f"[green]Snapshots: {num_snapshots}[/green]")
    console.print(f"[green]Core count: {num_cores}[/green]")
    console.print(
        f"[green]Raw measurement points per snapshot: {num_raw_points}[/green]"
    )

    # Allocate absolute-value 3D arrays and fill via vectorised indexing.
    prepend_shape = (num_snapshots, 1, num_cores)
    instructions_abs = np.zeros((num_snapshots, num_raw_points, num_cores))
    instructions_u_abs = np.zeros((num_snapshots, num_raw_points, num_cores))
    halted_cycles_abs = np.zeros((num_snapshots, num_raw_points, num_cores))

    snap_idx = df["snapshot_idx"].to_numpy(dtype=np.intp)
    su_idx = df["sampling_unit_idx"].to_numpy(dtype=np.intp)
    core_idx = df["core"].to_numpy(dtype=np.intp)

    instructions_abs[snap_idx, su_idx, core_idx] = df["instruction"].to_numpy()
    instructions_u_abs[snap_idx, su_idx, core_idx] = df["instruction:u"].to_numpy()
    halted_cycles_abs[snap_idx, su_idx, core_idx] = df["halted_cycles"].to_numpy()

    # Apply sampling_unit_size striding, then convert absolutes → per-unit deltas.
    instructions_abs = instructions_abs[:, ::sampling_unit_size, :]
    instructions_u_abs = instructions_u_abs[:, ::sampling_unit_size, :]
    halted_cycles_abs = halted_cycles_abs[:, ::sampling_unit_size, :]

    instructions_delta = np.diff(
        instructions_abs, axis=1, prepend=np.zeros(prepend_shape)
    )
    instructions_u_delta = np.diff(
        instructions_u_abs, axis=1, prepend=np.zeros(prepend_shape)
    )
    halted_cycles_delta = np.diff(
        halted_cycles_abs, axis=1, prepend=np.zeros(prepend_shape)
    )

    console.print(
        f"[green]Sampling units after stride ({sampling_unit_size}x): "
        f"{instructions_delta.shape[1]}[/green]"
    )

    # Store snapshot IDs as strings in place of result_folder paths.
    snapshot_labels = [str(sid) for sid in unique_snapshot_ids]

    return MeasurementData(
        instructions_delta,
        instructions_u_delta,
        halted_cycles_delta,
        snapshot_labels,
        sampling_unit_size,
    )


def detect_csv_version(filepath: str) -> str:
    """
    Detect CSV format version by checking column names.

    Returns:
        'new' if new format (has host_core_idx)
        'old' if old format (has affinity_core_idx)
        'unknown' if neither detected
    """
    try:
        df = pd.read_csv(filepath, nrows=0)  # Read only header
        fieldnames = list(df.columns)

        if "host_core_idx" in fieldnames:
            return "new"
        elif "affinity_core_idx" in fieldnames:
            return "old"
        else:
            return "unknown"
    except Exception as e:
        console.print(f"[red]Error reading file: {e}[/red]")
        return "unknown"


def read_core_info(core_info_path: str):
    """
    Read the core_info.csv and handle new format with host_core_idx.
    Returns a list of dictionaries with core information.
    """
    if not os.path.exists(core_info_path):
        console.print(f"[red]Error: core_info.csv not found at {core_info_path}[/red]")
        sys.exit(1)

    # Check CSV version
    version = detect_csv_version(core_info_path)

    if version == "old":
        console.print(
            f"[red]Error: Detected old format core_info.csv (has 'affinity_core_idx' column, expected 'host_core_idx')[/red]"
        )
        console.print(
            f"[yellow]Please run: python migrate_core_info.py {core_info_path} {core_info_path}[/yellow]"
        )
        sys.exit(2)

    if version == "unknown":
        console.print(
            f"[red]Error: Cannot determine CSV format for {core_info_path}[/red]"
        )
        console.print(
            "Expected columns: 'host_core_idx' (new) or 'affinity_core_idx' (old)"
        )
        sys.exit(2)

    df = pd.read_csv(core_info_path)

    # Check if host_core_idx column exists (new format)
    has_host_core = "host_core_idx" in df.columns

    core_info = []
    for _, row in df.iterrows():
        core_data = {"ipns": row["ipns"]}
        if has_host_core:
            core_data["host_core_idx"] = row["host_core_idx"]
        core_info.append(core_data)

    return core_info, has_host_core


def calculate_z_score(confidence: float) -> float:
    """
    Calculate Z-score for given confidence level.
    """
    # Common confidence levels to Z-scores mapping
    z_scores = {90.0: 1.645, 95.0: 1.96, 99.0: 2.576, 99.9: 3.291}

    if confidence in z_scores:
        return z_scores[confidence]
    else:
        # For other confidence levels, use approximation (normal distribution)
        # This is a simplified approximation, for precise values scipy would be needed
        if confidence == 68.0:
            return 1.0
        elif confidence == 80.0:
            return 1.28
        elif confidence == 85.0:
            return 1.44
        elif confidence == 90.0:
            return 1.645
        elif confidence == 95.0:
            return 1.96
        elif confidence == 98.0:
            return 2.33
        elif confidence == 99.0:
            return 2.576
        else:
            # Default to 95% confidence if unknown
            return 1.96


def is_prime(n: int) -> bool:
    """Check if a number is prime."""
    if n < 2:
        return False
    if n == 2:
        return True
    if n % 2 == 0:
        return False
    for i in range(3, int(n**0.5) + 1, 2):
        if n % i == 0:
            return False
    return True


def next_prime(n: int) -> int:
    """Find the next prime number greater than or equal to n."""
    if n <= 2:
        return 2
    candidate = n
    if candidate % 2 == 0:
        candidate += 1
    while not is_prime(candidate):
        candidate += 2
    return candidate


def calculate_next_checkpoint_size(max_required_size: float) -> int:
    """
    Calculate the next checkpoint size as the maximum required sample size
    rounded up to the nearest prime number.

    Args:
        max_required_size: The maximum required sample size across all groups

    Returns:
        The next checkpoint size (prime number >= max_required_size)
    """
    # Round up to nearest integer
    ceil_size = int(math.ceil(max_required_size))
    return next_prime(ceil_size)


def calculate_weighted_harmonic_average(
    measurement_data: MeasurementData, index: int, core_ids: list[int] | None = None
) -> float:
    """
    Calculate the weighted harmonic average across cores for a specific sampling unit.

    For each core, iterates through all snapshots, calculates non-idle IPC where applicable,
    and computes a weighted harmonic average considering both idle and non-idle periods.

    Args:
        measurement_data: MeasurementData container with parsed measurement data
        index: Index of the sampling unit to analyze
        core_ids: Optional list of core IDs to include. If None, all cores are included.

    Returns:
        The sum of weighted harmonic averages across specified cores
    """
    instruction_data_u = measurement_data.instructions_u
    halted_cycles_data = measurement_data.halted_cycles
    sampling_unit_size = measurement_data.sampling_unit_size

    if index >= instruction_data_u.shape[1]:
        return 0.0

    total_cycles = INTERVAL * sampling_unit_size  # * FREQ_GHZ  (see TODO at FREQ_GHZ)
    num_snapshots = instruction_data_u.shape[0]

    if core_ids is None:
        core_ids = list(range(instruction_data_u.shape[2]))

    total_harmonic_sum = 0.0

    for core_id in core_ids:
        if core_id >= instruction_data_u.shape[2]:
            continue

        ipcs = []
        ipc_weights = []
        idle_weight = 0.0

        for snapshot_idx in range(num_snapshots):
            halted_cycles = halted_cycles_data[snapshot_idx, index, core_id]
            instructions_u = instruction_data_u[snapshot_idx, index, core_id]

            if halted_cycles >= total_cycles:
                idle_weight += 1.0
            else:
                non_idle_cycles = total_cycles - halted_cycles
                ipc = instructions_u / non_idle_cycles
                ipcs.append(ipc)
                ipc_weights.append(non_idle_cycles / total_cycles)
                idle_weight += halted_cycles / total_cycles

        if len(ipcs) == 0:
            continue

        total_weight = sum(ipc_weights) + idle_weight
        if total_weight == 0:
            continue

        normalized_ipc_weights = [w / total_weight for w in ipc_weights]
        normalized_idle_weight = idle_weight / total_weight

        numerator = sum(normalized_ipc_weights)
        denominator = normalized_idle_weight
        for i, ipc in enumerate(ipcs):
            if ipc != 0:
                denominator += normalized_ipc_weights[i] / ipc

        if denominator != 0:
            total_harmonic_sum += numerator / denominator

    return total_harmonic_sum


def _plot_single_distribution(
    valid_u_ipc_data: list[float],
    title: str = "U-IPC Distribution",
    plot_enabled: bool = True,
) -> None:
    """
    Plot a single U-IPC distribution using plotille.

    Args:
        valid_u_ipc_data: List of valid U-IPC values to plot
        title: Title for the plot
        plot_enabled: Whether to actually display the plot
    """
    if not plot_enabled or len(valid_u_ipc_data) == 0:
        return

    console.print(f"\n[bold cyan]{title}:[/bold cyan]")
    fig = plotille.Figure()
    fig.width = 80
    fig.height = 20
    fig.color_mode = "byte"
    fig.x_label = "U-IPC"
    fig.y_label = "Frequency"
    fig.set_x_limits(min_=0, max_=max(valid_u_ipc_data) * 1.1)

    fig.histogram(valid_u_ipc_data, bins=20, lc=100)

    print(f"Distribution of U-IPC Values")
    print(fig.show())


def plot_u_ipc_distribution(
    measurement_data: MeasurementData,
    index: int,
    plot_enabled: bool = True,
    confidence: float = 95.0,
    acceptable_sampling_error: float = 0.05,
    core_ids: list[int] | None = None,
    plot_title: str | None = None,
) -> dict:
    """
    Plot U-IPC distribution and return statistics for the specified sampling unit.

    Args:
        measurement_data: Parsed measurement data container
        index: Index of the sampling unit to analyze
        plot_enabled: Whether to display the plot
        confidence: Confidence level for sample size calculation (default: 95.0)
        acceptable_sampling_error: Acceptable sampling error (default: 0.05)
        core_ids: Optional list of core IDs to include. If None, all cores are included.
        plot_title: Optional custom title for the plot. If None, uses default title.

    Returns:
        Dictionary with U-IPC statistics
    """
    instruction_data = measurement_data.instructions
    instruction_data_u = measurement_data.instructions_u
    halted_cycles_data = measurement_data.halted_cycles
    sampling_unit_size = measurement_data.sampling_unit_size

    if instruction_data_u.shape[1] == 0:
        return {
            "valid_u_ipc_data": [],
            "average_u_ipc": 0,
            "coefficient_of_variation": float("inf"),
            "required_sample_size": float("inf"),
            "current_sample_size": 0,
            "is_sample_size_enough": False,
        }

    # Check if the index is valid
    if index >= instruction_data.shape[1]:
        console.print(
            f"[red]Error: Index {index} is out of bounds. Maximum index is {instruction_data.shape[1] - 1}[/red]"
        )
        sys.exit(1)

    # Extract data for the specified interval index
    interval_instruction_data_u = instruction_data_u[
        :, index, :
    ]  # Shape: [snapshots, cores]

    # Calculate IPC for each snapshot and core
    interval_ipc_data_u = interval_instruction_data_u / (
        INTERVAL * sampling_unit_size  # * FREQ_GHZ  (see TODO at FREQ_GHZ)
    )

    # Filter by core_ids if specified
    if core_ids is not None:
        # Validate core IDs
        max_core_idx = interval_ipc_data_u.shape[1] - 1
        valid_cores = [core for core in core_ids if 0 <= core <= max_core_idx]
        if not valid_cores:
            console.print(
                f"[red]Error: No valid cores found in specified core_ids[/red]"
            )
            return {
                "valid_u_ipc_data": [],
                "average_u_ipc": 0,
                "coefficient_of_variation": float("inf"),
                "required_sample_size": float("inf"),
                "current_sample_size": 0,
                "is_sample_size_enough": False,
            }
        # Select only specified cores
        interval_ipc_data_u = interval_ipc_data_u[:, valid_cores]

    # Calculate U-IPC statistics for sample size validation
    snapshot_total_u_ipc = np.sum(
        interval_ipc_data_u, axis=1
    )  # Sum across cores for each snapshot
    # Drop NaN and zero (idle) snapshots — consistent with the other reporting paths.
    valid_u_ipc_data = [x for x in snapshot_total_u_ipc if not math.isnan(x) and x != 0]

    if len(valid_u_ipc_data) > 0:
        # make sure all U-IPC is larger than 0.
        for val in valid_u_ipc_data:
            if val < 0:
                console.print(
                    f"[red]Error: Found U-IPC value {val:.4f} which is not greater than 0. Please check the data for sampling unit index {index}.[/red]"
                )
                sys.exit(1)

        average_u_ipc = np.mean(valid_u_ipc_data)
        std_dev = np.std(valid_u_ipc_data)
        coefficient_of_variation = (
            std_dev / average_u_ipc if average_u_ipc > 0 else float("inf")
        )

        # Calculate required sample size
        z_score = calculate_z_score(confidence)
        required_sample_size = (
            z_score * coefficient_of_variation / acceptable_sampling_error
        ) ** 2
        current_sample_size = len(valid_u_ipc_data)
        is_sample_size_enough = current_sample_size >= required_sample_size

        # Plot distribution using plotille if enabled
        if plot_enabled:
            title = plot_title if plot_title else "U-IPC Distribution"
            _plot_single_distribution(valid_u_ipc_data, title, plot_enabled)

        return {
            "valid_u_ipc_data": valid_u_ipc_data,
            "average_u_ipc": average_u_ipc,
            "coefficient_of_variation": coefficient_of_variation,
            "required_sample_size": required_sample_size,
            "current_sample_size": current_sample_size,
            "is_sample_size_enough": is_sample_size_enough,
        }
    else:
        return {
            "valid_u_ipc_data": [],
            "average_u_ipc": 0,
            "coefficient_of_variation": float("inf"),
            "required_sample_size": float("inf"),
            "current_sample_size": 0,
            "is_sample_size_enough": False,
        }


def generate_new_core_info(
    measurement_data: MeasurementData,
    old_core_info_path: str,
    index: int,
    confidence: float | None = None,
    acceptable_sampling_error: float | None = None,
    output_path: str = "./core_info_new.csv",
    use_normal_ipc: bool = False,
):
    """
    Generate new core_info.csv with updated IPC values from direct measurement data analysis.
    """
    instruction_data = measurement_data.instructions
    instruction_data_u = measurement_data.instructions_u
    halted_cycles_data = measurement_data.halted_cycles
    sampling_unit_size = measurement_data.sampling_unit_size

    if index >= instruction_data.shape[1]:
        console.print(
            f"[red]Error: Index {index} is out of bounds. Maximum index is {instruction_data.shape[1] - 1}[/red]"
        )
        sys.exit(1)

    interval_instruction_data = instruction_data[:, index, :]
    interval_instruction_data_u = instruction_data_u[:, index, :]
    interval_halted_cycles_data = halted_cycles_data[:, index, :]

    valid_core_ipc = {}
    valid_core_ipc_non_halted = {}

    # Calculate the IPC for each core across snapshots.
    for core_id in range(interval_instruction_data.shape[1]):
        total_cycles = 0
        total_halted_cycles = 0
        total_instructions = 0
        for snapshot_idx in range(interval_instruction_data.shape[0]):
            total_instructions += interval_instruction_data[snapshot_idx, core_id]
            total_cycles += INTERVAL * sampling_unit_size  # * FREQ_GHZ  (see TODO at FREQ_GHZ)
            total_halted_cycles += interval_halted_cycles_data[snapshot_idx, core_id]

        valid_core_ipc[core_id] = (
            total_instructions / total_cycles if total_cycles > 0 else 0.0
        )
        non_halted_cycles = total_cycles - total_halted_cycles
        valid_core_ipc_non_halted[core_id] = (
            (total_instructions / non_halted_cycles) if non_halted_cycles > 0 else 0.0
        )

    if len(valid_core_ipc) == 0 and len(valid_core_ipc_non_halted) == 0:
        console.print(
            "[red]Error: No valid per-core IPC data found for the specified sampling unit.[/red]"
        )
        sys.exit(1)

    old_core_info, has_host_core = read_core_info(old_core_info_path)

    new_core_data = []
    comparison_data = []

    cores_with_timing_data = 0
    cores_inherited = 0

    for i, old_core in enumerate(old_core_info):
        old_ipns = old_core["ipns"]

        if use_normal_ipc:
            ipc_to_use = valid_core_ipc.get(i)
        else:
            ipc_to_use = valid_core_ipc_non_halted.get(i)

        if ipc_to_use is not None:
            new_ipns = round(FREQ_GHZ * ipc_to_use, 2)
            cores_with_timing_data += 1
            updated = True
        else:
            new_ipns = old_ipns
            cores_inherited += 1
            updated = False

        non_halted_ipc_val = valid_core_ipc_non_halted.get(i, 0.0)
        normal_ipc_val = valid_core_ipc.get(i, 0.0)

        # Create full new format with all columns
        core_data = {
            "host_core_idx": str(int(old_core["host_core_idx"]))
            if has_host_core
            else str(i),
            "model_type": "constant",
            "ipns": f"{new_ipns:.2f}",
            "bx_private_icache_miss_coeff": "0.0",
            "bx_private_dcache_miss_load_ptw_coeff": "0.0",
            "bx_private_dcache_miss_store_coeff": "0.0",
            "bx_shared_cache_miss_coeff": "0.0",
            "bx_bp_miss_coeff": "0.0",
            "bx_drain_pipeline_coeff": "0.0",
            "bx_drain_store_buffer_coeff": "0.0",
            "bx_read_noc_hop_coeff": "0.0",
            "bx_write_noc_hop_coeff": "0.0",
            "bx_ifetch_noc_hop_coeff": "0.0",
            "bx_instruction_u_coeff": "0.0",
            "bx_instruction_k_coeff": "0.0",
        }
        new_core_data.append(core_data)

        comparison_data.append(
            {
                "core": i,
                "old_ipns": f"{old_ipns:.2f}",
                "normal_ipc": f"{normal_ipc_val:.4f}",
                "non_halted_ipc": f"{non_halted_ipc_val:.4f}",
                "new_ipns": f"{new_ipns:.2f}",
                "difference": f"{(new_ipns - old_ipns):.2f}",
                "updated": updated,
            }
        )

    new_df = pd.DataFrame(new_core_data)
    new_df.to_csv(output_path, index=False)

    u_ipc_stats = plot_u_ipc_distribution(measurement_data, index, plot_enabled=False)

    if confidence is None:
        confidence = 95.0
    if acceptable_sampling_error is None:
        acceptable_sampling_error = 0.05

    if u_ipc_stats["current_sample_size"] > 0:
        z_score = calculate_z_score(confidence)
        required_sample_size = (
            z_score
            * u_ipc_stats["coefficient_of_variation"]
            / acceptable_sampling_error
        ) ** 2
        is_sample_size_enough = (
            u_ipc_stats["current_sample_size"] >= required_sample_size
        )
    else:
        required_sample_size = float("inf")
        is_sample_size_enough = False

    return {
        "comparison_data": comparison_data,
        "cores_with_timing_data": cores_with_timing_data,
        "cores_inherited": cores_inherited,
        "average_u_ipc": u_ipc_stats["average_u_ipc"],
        "coefficient_of_variation": u_ipc_stats["coefficient_of_variation"],
        "required_sample_size": required_sample_size,
        "current_sample_size": u_ipc_stats["current_sample_size"],
        "is_sample_size_enough": is_sample_size_enough,
        "confidence": confidence,
        "acceptable_sampling_error": acceptable_sampling_error,
    }


def display_results_table(results: dict):
    """
    Display a formatted table showing the results using Rich library.
    """
    table = Table(title="IPNS Comparison Table", box=box.ROUNDED)
    table.add_column("Core", justify="center", style="cyan")
    table.add_column("Old IPNS", justify="center", style="magenta")
    table.add_column("Normal IPC", justify="center", style="yellow")
    table.add_column("Non-Halted IPC", justify="center", style="yellow")
    table.add_column("New IPNS", justify="center", style="green")
    table.add_column("Difference", justify="center", style="yellow")
    table.add_column("Updated", justify="center", style="blue")

    for data in results["comparison_data"]:
        updated_str = "✓" if data["updated"] else "✗"
        style = "green" if data["updated"] else "red"
        table.add_row(
            str(data["core"]),
            data["old_ipns"],
            data["normal_ipc"],
            data["non_halted_ipc"],
            data["new_ipns"],
            data["difference"],
            Text(updated_str, style=style),
        )

    console.print(table)

    summary_table = Table(title="Summary Statistics", box=box.DOUBLE)
    summary_table.add_column("Metric", style="cyan")
    summary_table.add_column("Value", style="green")

    summary_table.add_row("Average U-IPC", f"{results['average_u_ipc']:.4f}")
    summary_table.add_row(
        "Coefficient of Variation", f"{results['coefficient_of_variation']:.4f}"
    )
    summary_table.add_row("Current Sample Size", str(results["current_sample_size"]))
    summary_table.add_row(
        "Required Sample Size", f"{results['required_sample_size']:.1f}"
    )
    summary_table.add_row(
        "Sample Size Adequate",
        Text("✓ Yes", style="green")
        if results["is_sample_size_enough"]
        else Text("✗ No", style="red"),
    )
    summary_table.add_row("Confidence Level", f"{results['confidence']}%")
    summary_table.add_row(
        "Acceptable Error", f"{results['acceptable_sampling_error'] * 100}%"
    )

    console.print(summary_table)

    if not results["is_sample_size_enough"]:
        needed_samples = (
            results["required_sample_size"] - results["current_sample_size"]
        )
        warning_panel = Panel(
            f"[red]Need {needed_samples:.1f} more sample units for adequate sample size[/red]",
            title="[bold red]Warning[/bold red]",
            border_style="red",
        )
        console.print(warning_panel)


def analyze_sampling_unit(
    measurement_data: MeasurementData,
    index: int,
    core_groups: list[list[int]] | None = None,
    confidence: float = 95.0,
    acceptable_sampling_error: float = 0.05,
    plot_enabled: bool = True,
):
    """
    Analyze a specific sampling unit from direct measurement data.
    """
    instruction_data = measurement_data.instructions
    instruction_data_u = measurement_data.instructions_u
    halted_cycles_data = measurement_data.halted_cycles
    result_folders = measurement_data.result_folders
    sampling_unit_size = measurement_data.sampling_unit_size

    console.print(f"[bold cyan]Analysis Configuration:[/bold cyan]")
    console.print(f"[green]Snapshots: {len(result_folders)}[/green]")
    console.print(
        f"[green]Sampling unit size: {sampling_unit_size} INTERVAL(s)[/green]"
    )
    console.print(f"[green]INTERVAL value: {INTERVAL}[/green]")
    console.print(f"[green]Analyzing sampling unit at index: {index}[/green]")

    # Check if the index is valid
    if index >= instruction_data_u.shape[1]:
        console.print(
            f"[red]Error: Index {index} is out of bounds. Maximum index is {instruction_data_u.shape[1] - 1}[/red]"
        )
        sys.exit(1)

    # Extract data for the specified interval index
    interval_instruction_u_data = instruction_data_u[
        :, index, :
    ]  # Shape: [snapshots, cores]

    # Calculate IPC for each snapshot and core
    interval_ipc_u_data = interval_instruction_u_data / (
        INTERVAL * sampling_unit_size  # * FREQ_GHZ  (see TODO at FREQ_GHZ)
    )

    # Initialize list to store results for each group
    group_results = []

    # Analyze core groups if specified, otherwise analyze all cores as one group
    if core_groups:
        console.print(
            f"\n[bold cyan]Analyzing {len(core_groups)} core groups separately:[/bold cyan]"
        )
        for group_idx, core_group in enumerate(core_groups):
            console.print(
                f"\n[yellow]Core Group {group_idx + 1}: cores {core_group}[/yellow]"
            )

            # Validate that all cores in the group exist in the data
            max_core_idx = interval_ipc_u_data.shape[1] - 1
            valid_cores = [core for core in core_group if 0 <= core <= max_core_idx]
            invalid_cores = [core for core in core_group if core not in valid_cores]

            if invalid_cores:
                console.print(
                    f"[red]Warning: cores {invalid_cores} not found in data (max core index: {max_core_idx})[/red]"
                )

            if not valid_cores:
                console.print("[red]Error: No valid cores found in this group.[/red]")
                continue

            # Aggregate across specified cores for each snapshot
            group_u_ipc_data = interval_ipc_u_data[
                :, valid_cores
            ]  # Shape: [snapshots, group_cores]
            snapshot_group_u_ipc = np.sum(
                group_u_ipc_data, axis=1
            )  # Shape: [snapshots]

            # Drop NaN and zero (idle) snapshots — consistent with the other reporting paths.
            valid_data = [x for x in snapshot_group_u_ipc if not math.isnan(x) and x != 0]

            if len(valid_data) == 0:
                console.print(
                    "[red]Error: No valid data found for this core group.[/red]"
                )
                continue

            # Calculate statistics for this group
            average_u_ipc = np.mean(valid_data)
            std_dev = np.std(valid_data)
            coefficient_of_variation = std_dev / average_u_ipc

            # Calculate required sample size
            z_score = calculate_z_score(confidence)
            required_sample_size = (
                z_score * coefficient_of_variation / acceptable_sampling_error
            ) ** 2
            current_sample_size = len(valid_data)
            is_sample_size_enough = current_sample_size >= required_sample_size

            # Store result for this group
            group_results.append(
                {
                    "required_sample_size": required_sample_size,
                    "current_sample_size": current_sample_size,
                    "is_sample_size_enough": is_sample_size_enough,
                }
            )

            # Report results for this group in a table
            group_table = Table(
                title=f"Core Group {group_idx + 1} Statistics", box=box.ROUNDED
            )
            group_table.add_column("Metric", style="cyan")
            group_table.add_column("Value", style="green")

            group_table.add_row("Valid cores used", str(valid_cores))
            group_table.add_row("Valid snapshots", str(current_sample_size))
            group_table.add_row("Average U-IPC", f"{average_u_ipc:.4f}")
            weighted_harmonic_ipc = calculate_weighted_harmonic_average(
                measurement_data, index, valid_cores
            )
            group_table.add_row("Weighted Harmonic IPC", f"{weighted_harmonic_ipc:.4f}")
            group_table.add_row("Standard deviation", f"{std_dev:.4f}")
            group_table.add_row(
                "Coefficient of variation", f"{coefficient_of_variation:.4f}"
            )
            group_table.add_row("Required sample size", f"{required_sample_size:.1f}")
            group_table.add_row(
                "Sample size adequate",
                Text("✓ Yes", style="green")
                if is_sample_size_enough
                else Text("✗ No", style="red"),
            )

            console.print(group_table)

            if not is_sample_size_enough:
                needed = required_sample_size - current_sample_size
                console.print(
                    f"[red]Need {needed:.1f} more sample units for adequate sample size[/red]"
                )

            # Plot distribution for this group
            if plot_enabled:
                plot_u_ipc_distribution(
                    measurement_data,
                    index,
                    plot_enabled=True,
                    confidence=confidence,
                    acceptable_sampling_error=acceptable_sampling_error,
                    core_ids=valid_cores,
                    plot_title=f"U-IPC Distribution - Core Group {group_idx + 1}",
                )
    else:
        # Original behavior: aggregate across all cores
        console.print("\n[bold cyan]Analyzing all cores as a single group:[/bold cyan]")

        # Aggregate across cores for each snapshot to get total IPC per snapshot
        snapshot_total_u_ipc = np.sum(interval_ipc_u_data, axis=1)  # Shape: [snapshots]

        # Drop NaN and zero (idle) snapshots — consistent with the other reporting paths.
        valid_data = [x for x in snapshot_total_u_ipc if not math.isnan(x) and x != 0]

        if len(valid_data) == 0:
            console.print(
                "[red]Error: No valid data found for the specified sampling unit.[/red]"
            )
            sys.exit(1)

        # Calculate statistics
        average_u_ipc = np.mean(valid_data)
        std_dev = np.std(valid_data)
        coefficient_of_variation = std_dev / average_u_ipc

        # Calculate required sample size
        z_score = calculate_z_score(confidence)
        required_sample_size = (
            z_score * coefficient_of_variation / acceptable_sampling_error
        ) ** 2
        current_sample_size = len(valid_data)
        is_sample_size_enough = current_sample_size >= required_sample_size

        # Store result for this group
        group_results.append(
            {
                "required_sample_size": required_sample_size,
                "current_sample_size": current_sample_size,
                "is_sample_size_enough": is_sample_size_enough,
            }
        )

        # Report results in a table
        all_cores_table = Table(title="All Cores Statistics", box=box.ROUNDED)
        all_cores_table.add_column("Metric", style="cyan")
        all_cores_table.add_column("Value", style="green")

        all_cores_table.add_row("Valid snapshots", str(current_sample_size))
        all_cores_table.add_row("Average U-IPC", f"{average_u_ipc:.4f}")
        weighted_harmonic_ipc = calculate_weighted_harmonic_average(
            measurement_data, index
        )
        all_cores_table.add_row("Weighted Harmonic IPC", f"{weighted_harmonic_ipc:.4f}")
        all_cores_table.add_row("Standard deviation", f"{std_dev:.4f}")
        all_cores_table.add_row(
            "Coefficient of variation", f"{coefficient_of_variation:.4f}"
        )
        all_cores_table.add_row("Required sample size", f"{required_sample_size:.1f}")
        all_cores_table.add_row(
            "Sample size adequate",
            Text("✓ Yes", style="green")
            if is_sample_size_enough
            else Text("✗ No", style="red"),
        )

        console.print(all_cores_table)

        if not is_sample_size_enough:
            needed = required_sample_size - current_sample_size
            console.print(
                f"[red]Need {needed:.1f} more sample units for adequate sample size[/red]"
            )

    return group_results


# --- Additive diagnostics (always printed) -----------------------------------
# Self-contained: reads timing.csv directly and never touches MeasurementData,
# parse_measurements_from_csv, generate_new_core_info, or any existing output path.
# Non-fatal by design — if the CSV/columns/index are unusable it skips (returns
# None) so the normal analysis, charts, and U-IPC values are never affected.

TB_BUCKETS = [
    "tb_frontend", "tb_branch", "tb_backend", "tb_mem_local", "tb_mem_remote",
    "tb_sbdrain", "tb_spin", "tb_interrupt", "tb_other",
]


def _diag_window_totals(csv_path: str, index: int, sampling_unit_size: int):
    """Per-core totals (summed over snapshots) for sampling unit `index`.

    Cumulative columns are converted to per-window deltas the same way
    parse_measurements_from_csv does (stride by unit size, then np.diff); `maf`
    is an average, so it is taken as the strided point value, not diffed.
    Returns (totals_by_core, maf_by_core) or None if the CSV is unusable.
    """
    if not os.path.exists(csv_path):
        console.print(f"[yellow]Diagnostics skipped: {csv_path} not found.[/yellow]")
        return None
    df = pd.read_csv(csv_path)

    cumulative = [
        "instruction", "instruction:u", "core_cycles", "spin_cycles", "wfi_cycles",
        "spins", "commits_nonspin_system", "commits_spin_user", "commits_spin_system",
        "l1i_miss", "l1d_miss", "l2_miss", "nic_sent", "nic_recv",
    ] + TB_BUCKETS
    missing = [c for c in cumulative + ["maf"] if c not in df.columns]
    if missing:
        console.print(
            f"[yellow]Diagnostics skipped: timing.csv missing columns {missing} — "
            f"re-run collect.py to regenerate with instrumentation.[/yellow]"
        )
        return None

    df = df[df["sys_cycles"] % INTERVAL == 0].copy()
    snaps = sorted(df["snapshot_id"].unique())
    snap_to_idx = {s: i for i, s in enumerate(snaps)}
    df["snapshot_idx"] = df["snapshot_id"].map(snap_to_idx)
    df["su_idx"] = df["sys_cycles"] // INTERVAL - 1
    n_snap = len(snaps)
    n_pts = int(df["su_idx"].max()) + 1
    n_core = int(df["core"].max()) + 1
    si = df["snapshot_idx"].to_numpy(dtype=np.intp)
    pi = df["su_idx"].to_numpy(dtype=np.intp)
    ci = df["core"].to_numpy(dtype=np.intp)

    def delta(col):
        a = np.zeros((n_snap, n_pts, n_core))
        a[si, pi, ci] = df[col].to_numpy()
        a = a[:, ::sampling_unit_size, :]
        return np.diff(a, axis=1, prepend=np.zeros((n_snap, 1, n_core)))

    def strided(col):
        a = np.zeros((n_snap, n_pts, n_core))
        a[si, pi, ci] = df[col].to_numpy()
        return a[:, ::sampling_unit_size, :]

    deltas = {c: delta(c) for c in cumulative}
    maf_arr = strided("maf")
    n_units = deltas["instruction"].shape[1]
    if index >= n_units:
        console.print(
            f"[yellow]Diagnostics skipped: index {index} out of bounds (max {n_units - 1})[/yellow]"
        )
        return None

    totals, maf_by_core = {}, {}
    for core in range(n_core):
        tot_instr = float(deltas["instruction"][:, index, core].sum())
        if tot_instr <= 0:
            continue  # inactive core for this unit
        totals[core] = {c: float(deltas[c][:, index, core].sum()) for c in cumulative}
        vals = maf_arr[:, index, core]
        vals = vals[vals > 0]
        maf_by_core[core] = float(vals.mean()) if len(vals) else 0.0
    return totals, maf_by_core


def report_diagnostics(
    csv_path: str, index: int, sampling_unit_size: int,
    core_groups: list[list[int]] | None = None,
):
    """Print per-core diagnostic tables for the all-three-shrink investigation."""
    result = _diag_window_totals(csv_path, index, sampling_unit_size)
    if result is None:
        return
    totals, maf_by_core = result
    if not totals:
        console.print("[yellow]Diagnostics skipped: no active cores for this sampling unit.[/yellow]")
        return

    cores = sorted(totals)
    if core_groups:
        wanted = {c for g in core_groups for c in g}
        cores = [c for c in cores if c in wanted]

    def agg(keys):
        return {k: sum(totals[c][k] for c in cores) for k in keys}

    all_keys = next(iter(totals.values())).keys()

    # Table A: IPC & cycle accounting
    ta = Table(title=f"Diagnostics — IPC & cycle accounting (unit {index})", box=box.ROUNDED)
    for col in ["Core", "IPC", "U-IPC(user)", "U-IPC(nonspin)", "gap", "spin% (β)",
                "idle%", "useful%", "spin-IPC"]:
        ta.add_column(col, justify="center")

    def ipc_row(label, t):
        cyc = t["core_cycles"] or 1
        ipc = t["instruction"] / cyc
        uipc_u = t["instruction:u"] / cyc
        uipc_ns = (t["instruction:u"] + t["commits_nonspin_system"]) / cyc
        beta = t["spin_cycles"] / cyc
        idle = t["wfi_cycles"] / cyc
        spin_instr = t["commits_spin_user"] + t["commits_spin_system"]
        spin_ipc = spin_instr / t["spin_cycles"] if t["spin_cycles"] > 0 else 0.0
        ta.add_row(label, f"{ipc:.4f}", f"{uipc_u:.4f}", f"{uipc_ns:.4f}",
                   f"{ipc - uipc_u:.4f}", f"{100 * beta:.3f}", f"{100 * idle:.3f}",
                   f"{100 * (1 - beta - idle):.3f}", f"{spin_ipc:.4f}")

    for c in cores:
        ipc_row(str(c), totals[c])
    ipc_row("ALL", agg(all_keys))
    console.print(ta)

    # Table B: memory & communication
    tb_ = Table(title="Diagnostics — memory & communication", box=box.ROUNDED)
    for col in ["Core", "L1I-MPKI", "L1D-MPKI", "L2-MPKI", "MLP(MAF)", "nic_sent", "nic_recv"]:
        tb_.add_column(col, justify="center")
    for c in cores:
        t = totals[c]
        instr = t["instruction"] or 1
        tb_.add_row(str(c), f"{1000 * t['l1i_miss'] / instr:.3f}",
                    f"{1000 * t['l1d_miss'] / instr:.3f}", f"{1000 * t['l2_miss'] / instr:.3f}",
                    f"{maf_by_core.get(c, 0.0):.2f}", f"{int(t['nic_sent'])}",
                    f"{int(t['nic_recv'])}")
    console.print(tb_)

    # Table C: CPI-stack fractions (proportional attribution, not cycles)
    tc = Table(title="Diagnostics — CPI-stack fractions (proportional)", box=box.ROUNDED)
    tc.add_column("Core", justify="center")
    for b in TB_BUCKETS:
        tc.add_column(b.replace("tb_", ""), justify="center")

    def tb_row(label, t):
        tot = sum(t[b] for b in TB_BUCKETS) or 1
        tc.add_row(label, *[f"{t[b] / tot:.3f}" for b in TB_BUCKETS])

    for c in cores:
        tb_row(str(c), totals[c])
    tb_row("ALL", agg(TB_BUCKETS))
    console.print(tc)

    # Invariants (the solid, same-unit ones)
    inv = Table(title="Invariants", box=box.SIMPLE)
    for col in ["Core", "spin+wfi ≤ cyc", "U-IPC ≤ IPC", "cycles", "sim-time (cyc/freq) ns"]:
        inv.add_column(col, justify="center")
    for c in cores:
        t = totals[c]
        cyc = t["core_cycles"]
        ok1 = t["spin_cycles"] + t["wfi_cycles"] <= cyc
        ok2 = t["instruction:u"] <= t["instruction"]
        inv.add_row(str(c),
                    Text("✓", style="green") if ok1 else Text("✗", style="red"),
                    Text("✓", style="green") if ok2 else Text("✗", style="red"),
                    f"{int(cyc)}", f"{cyc / FREQ_GHZ:.0f}")
    console.print(inv)

    console.print(Panel(
        "[yellow]Not available from timing stats (need guest/faban logs or new "
        "PDES instrumentation): measured RTT, requests-completed / p99, cross-node "
        "PDES message conservation, bytes/drops, context-switch/futex, user/kernel "
        "cycle split.[/yellow]",
        title="Out of scope", border_style="yellow",
    ))


def main():
    parser = argparse.ArgumentParser(
        description="Analyze sampling results and generate core_info.csv from direct measurement data",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Analyze measurement data only
  python result.py --unit-size 2 --index 1

  # Analyze measurement data with core groups
  python result.py --unit-size 2 --index 1 --core-groups "0-7,8-15"

  # Analyze with custom confidence and error parameters (auto-generates core_info_new.csv if core_info exists)
  python result.py --unit-size 2 --index 1 --confidence 99 --error 0.02

  # Analyze without plotting distribution
  python result.py --unit-size 2 --index 1 --no-plot

  # Load pre-processed data from NPZ file
  python result.py --npz-file measurement_data.npz --index 1
        """,
    )

    parser.add_argument(
        "-c",
        "--generate-core-info",
        action="store_true",
        help="Generating the new core_info.csv",
    )

    parser.add_argument(
        "--core-info-path",
        default="./run/core_info.csv",
        help="Path to existing core_info.csv file. If exists, will automatically generate core_info_new.csv (default: ./run/core_info.csv)",
    )
    parser.add_argument(
        "-u",
        "--unit-size",
        type=int,
        default=1,
        help="Size of sampling unit in terms of INTERVAL (default: 1)",
    )
    parser.add_argument(
        "-i",
        "--index",
        type=int,
        default=2,
        help="Index of the sampling unit to analyze (in terms of number of INTERVAL)",
    )
    parser.add_argument(
        "-g",
        "--core-groups",
        help='Specify core groups to analyze separately (e.g., "0-7,8-15" or "0,1,2,3-5"). If not specified, all cores are analyzed as one group.',
    )
    parser.add_argument(
        "--confidence",
        type=float,
        help="Confidence level for sample size calculation (optional, default: 95.0)",
    )
    parser.add_argument(
        "--error",
        type=float,
        help="Acceptable sampling error (optional, default: 0.05)",
    )
    parser.add_argument(
        "--plot",
        action="store_true",
        default=True,
        help="Plot U-IPC distribution using plotille (default: True)",
    )
    parser.add_argument(
        "--no-plot",
        dest="plot",
        action="store_false",
        help="Disable plotting of U-IPC distribution",
    )
    parser.add_argument(
        "--use-normal-ipc",
        action="store_true",
        default=False,
        help="Use normal IPC instead of non-halted IPC for IPNS calculation (default: use non-halted IPC)",
    )
    parser.add_argument(
        "--timing-csv",
        type=str,
        default="./timing.csv",
        help="Path to timing.csv produced by collect.py (default: ./timing.csv)",
    )
    parser.add_argument(
        "-n",
        "--npz-file",
        type=str,
        help="Path to NPZ file to load pre-processed data (alternative to parsing timing.csv)",
    )
    parser.add_argument(
        "--save-npz",
        type=str,
        metavar="PATH",
        help="Save parsed measurement data to an NPZ file at the given path (only used when loading from timing.csv)",
    )
    parser.add_argument(
        "--freq-ghz",
        type=float,
        default=2.0,
        help="Machine frequency in GHz (cycles per ns); the experiment's machine_freq_ghz (default: 2.0)",
    )
    parser.add_argument(
        "--no-exit-on-fail",
        action="store_true",
        help="Do not sys.exit(-1) when sampling-error bounds are unmet. Used by the statistical-sample loop, where an unmet bound is the normal signal to take another iteration.",
    )
    args = parser.parse_args()

    global FREQ_GHZ
    FREQ_GHZ = args.freq_ghz

    console.print(
        Panel.fit(
            "[bold green]Measurement Data Analysis Tool[/bold green]",
            border_style="green",
        )
    )

    # Load from NPZ file if specified, otherwise parse from timing.csv
    if args.npz_file:
        measurement_data = load_measurement_data(args.npz_file)
    else:
        console.print(f"[cyan]Parsing measurement data from {args.timing_csv}...[/cyan]")
        measurement_data = parse_measurements_from_csv(args.timing_csv, args.unit_size)

    # Parse core groups if provided
    core_groups = None
    if args.core_groups:
        try:
            core_groups = parse_core_groups(args.core_groups)
            console.print(f"[green]Parsed core groups: {core_groups}[/green]")
        except Exception as e:
            console.print(
                f"[red]Error parsing core groups '{args.core_groups}': {e}[/red]"
            )
            sys.exit(1)

    # Save NPZ — per core group if groups are defined, otherwise the full dataset
    if args.save_npz and not args.npz_file:
        if core_groups:
            base = args.save_npz[:-4] if args.save_npz.endswith(".npz") else args.save_npz
            for i, group in enumerate(core_groups):
                group_data = MeasurementData(
                    instructions=measurement_data.instructions[:, :, group],
                    instructions_u=measurement_data.instructions_u[:, :, group],
                    halted_cycles=measurement_data.halted_cycles[:, :, group],
                    result_folders=measurement_data.result_folders,
                    sampling_unit_size=measurement_data.sampling_unit_size,
                )
                save_measurement_data(group_data, f"{base}_group{i}.npz")
        else:
            save_measurement_data(measurement_data, args.save_npz)

    # Analyze the sampling unit
    confidence = args.confidence if args.confidence else 95.0
    acceptable_sampling_error = args.error if args.error else 0.05
    group_results = analyze_sampling_unit(
        measurement_data,
        args.index,
        core_groups,
        confidence,
        acceptable_sampling_error,
        args.plot,
    )

    # Track whether all bounds are satisfied for final exit decision
    all_bounds_satisfied = True

    # Calculate next sample size from maximum required size across all groups
    if group_results:
        max_required_size = max(r["required_sample_size"] for r in group_results)
        max_current_size = max(r["current_sample_size"] for r in group_results)
        # Ensure next sample size never decreases to avoid oscillation
        effective_size = max(max_required_size, max_current_size)
        next_sample_size = calculate_next_checkpoint_size(effective_size)

        # Raw required size (unrounded ceil) for the statistical-sample loop, which
        # does its own round-to-50 and takes the max across nodes.
        with open("./REQUIRED_SAMPLE_SIZE", "w") as f:
            f.write(str(int(math.ceil(max_required_size))))

        # Write next sample size to file
        with open("./NEXT_SAMPLE_SIZE", "w") as f:
            f.write(str(next_sample_size))
        console.print(
            f"[green]Next sample size written to ./NEXT_SAMPLE_SIZE: {next_sample_size}[/green]"
        )

        # Check if any group failed the sampling error bound
        all_bounds_satisfied = all(r["is_sample_size_enough"] for r in group_results)
        if not all_bounds_satisfied:
            console.print(
                "[red]Warning: Not all groups satisfy the sampling error bound. Will exit with -1 after completing all steps.[/red]"
            )

    # Always plot U-IPC distribution regardless of core_info.csv generation
    # Plot overall distribution (all cores) as final summary
    console.print(
        "\n[bold cyan]=== Overall U-IPC Distribution (All Cores) ===[/bold cyan]"
    )
    plot_u_ipc_distribution(
        measurement_data, args.index, args.plot, confidence, acceptable_sampling_error
    )

    # Always generate new core_info.csv if core_info_path exists and generate_core_info is set
    if os.path.exists(args.core_info_path) and args.generate_core_info:
        console.print(f"\n[bold cyan]Generating new core_info.csv...[/bold cyan]")

        results = generate_new_core_info(
            measurement_data,
            args.core_info_path,
            args.index,
            args.confidence,
            args.error,
            "./core_info_new.csv",
            args.use_normal_ipc,
        )

        # Display results
        display_results_table(results)

        console.print(
            f"\n[bold green]Generated new core_info.csv at ./core_info_new.csv[/bold green]"
        )
        console.print(
            f"[green]Sampling unit size: {args.unit_size}, Index: {args.index}[/green]"
        )
    elif args.generate_core_info:
        console.print(
            f"\n[yellow]Core info file not found at {args.core_info_path}, skipping core_info_new.csv generation[/yellow]"
        )

    # Always print diagnostics (additive; non-fatal — runs AFTER the plot/U-IPC
    # output above and never affects it).
    console.print("\n[bold cyan]=== Diagnostics ===[/bold cyan]")
    report_diagnostics(args.timing_csv, args.index, args.unit_size, core_groups)

    # Exit with -1 if sampling error bounds were not satisfied
    if not all_bounds_satisfied and not args.no_exit_on_fail:
        console.print(
            "[red]Error: Not all groups satisfy the sampling error bound. Exiting with -1.[/red]"
        )
        sys.exit(-1)


if __name__ == "__main__":
    main()
