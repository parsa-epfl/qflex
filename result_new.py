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
INTERVAL = 100000  # overridden by --interval (the experiment's stat_interval_cycles)
MEASURE_UNITS = 1  # overridden by --measure-units (the experiment's measurement_ratio)
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


def _window(arr: np.ndarray, index: int) -> np.ndarray:
    """Sum the per-interval deltas over the measurement window [index, index+MEASURE_UNITS).
    arr is [snapshots, sampling_unit_idx, cores]; returns [snapshots, cores]."""
    return arr[:, index:index + MEASURE_UNITS, :].sum(axis=1)


def _window_cycles(sampling_unit_size: int) -> int:
    """Total cycles in the measurement window (the IPC denominator)."""
    return INTERVAL * sampling_unit_size * MEASURE_UNITS


def _check_window_bounds(n_units: int, index: int) -> None:
    if index + MEASURE_UNITS > n_units:
        console.print(
            f"[red]Error: window [{index}, {index + MEASURE_UNITS}) is out of bounds — "
            f"only {n_units} sampling units exist[/red]"
        )
        sys.exit(1)


class MeasurementData:
    """Container for parsed measurement data to avoid repeated parsing."""

    def __init__(
        self,
        instructions: np.ndarray,
        instructions_u: np.ndarray,
        halted_cycles: np.ndarray,
        result_folders: list[str],
        sampling_unit_size: int,
        wfi_cycles: np.ndarray | None = None,
    ):
        self.instructions = (
            instructions  # instructions[snapshot_idx, sampling_unit_idx, core]
        )
        self.instructions_u = (
            instructions_u  # instructions_u[snapshot_idx, sampling_unit_idx, core]
        )
        # LEGACY idle source: the `halted_cycles` csv column is the dead HaltedCycles counter (~0);
        # it does NOT reflect real idle. Kept for back-compat / the existing (legacy) numbers.
        self.halted_cycles = (
            halted_cycles  # halted_cycles[snapshot_idx, sampling_unit_idx, core]
        )
        # ACCURATE idle source: the `wfi_cycles` column (the ++theWFI counter) — real WFI/idle cycles.
        # None when loading an older npz that lacks it (dual-report then falls back to legacy only).
        self.wfi_cycles = wfi_cycles
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
        wfi_cycles=measurement_data.wfi_cycles
        if measurement_data.wfi_cycles is not None
        else np.zeros_like(measurement_data.halted_cycles),
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
        wfi_cycles=data["wfi_cycles"] if "wfi_cycles" in data.files else None,
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
    # ACCURATE idle source: parse the separate `wfi_cycles` column (real WFI/idle — the ++theWFI
    # counter). Guarded for older timing.csv lacking it (then wfi stays None → legacy-only reporting).
    if "wfi_cycles" in df.columns:
        wfi_cycles_abs = np.zeros((num_snapshots, num_raw_points, num_cores))
        wfi_cycles_abs[snap_idx, su_idx, core_idx] = df["wfi_cycles"].to_numpy()
        wfi_cycles_abs = wfi_cycles_abs[:, ::sampling_unit_size, :]
        wfi_cycles_delta = np.diff(wfi_cycles_abs, axis=1, prepend=np.zeros(prepend_shape))
    else:
        wfi_cycles_delta = None

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
        wfi_cycles=wfi_cycles_delta,
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
    measurement_data: MeasurementData, index: int, core_ids: list[int] | None = None,
    idle_source: str = "halted",
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
    # idle_source: "halted" = LEGACY dead `halted_cycles` column (~0 idle → answer ≈ normal IPC);
    # "wfi" = ACCURATE real idle (the ++theWFI `wfi_cycles` counter). Falls back to legacy if wfi absent.
    if idle_source == "wfi" and measurement_data.wfi_cycles is not None:
        halted_cycles_data = measurement_data.wfi_cycles
    else:
        halted_cycles_data = measurement_data.halted_cycles
    sampling_unit_size = measurement_data.sampling_unit_size

    if index + MEASURE_UNITS > instruction_data_u.shape[1]:
        return 0.0

    total_cycles = _window_cycles(sampling_unit_size)  # * FREQ_GHZ  (see TODO at FREQ_GHZ)
    num_snapshots = instruction_data_u.shape[0]

    if core_ids is None:
        core_ids = list(range(instruction_data_u.shape[2]))

    total_harmonic_sum = 0.0

    window_halted = _window(halted_cycles_data, index)
    window_instr_u = _window(instruction_data_u, index)
    for core_id in core_ids:
        if core_id >= instruction_data_u.shape[2]:
            continue

        ipcs = []
        ipc_weights = []
        idle_weight = 0.0

        for snapshot_idx in range(num_snapshots):
            halted_cycles = window_halted[snapshot_idx, core_id]
            instructions_u = window_instr_u[snapshot_idx, core_id]

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

    _check_window_bounds(instruction_data.shape[1], index)

    # Sum the measurement window's deltas: [snapshots, cores]
    interval_instruction_data_u = _window(instruction_data_u, index)

    # Calculate IPC for each snapshot and core
    interval_ipc_data_u = interval_instruction_data_u / _window_cycles(sampling_unit_size)

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
    # Keep-zeros series (existing behavior, preserved): per-snapshot U-IPC summed across cores.
    valid_u_ipc_data = [x for x in snapshot_total_u_ipc]
    # ADD: busy-window (drop-zeros) series — mirrors result.py's OMMIT_ZERO (drop NaN and <= 0).
    busy_u_ipc_data = [x for x in snapshot_total_u_ipc if (not math.isnan(x)) and x > 0]
    n_dropped = len(snapshot_total_u_ipc) - len(busy_u_ipc_data)
    if n_dropped > 0:
        console.print(
            f"[yellow]Note: {n_dropped}/{len(snapshot_total_u_ipc)} snapshots have NaN or zero U-IPC "
            f"(idle / no user commit) — excluded from the busy-window (drop-zeros) U-IPC below.[/yellow]"
        )

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

        # ADD: busy-window (drop-zeros) stats — the busy-cycle U-IPC (what result.py reports).
        if len(busy_u_ipc_data) > 0:
            busy_average_u_ipc = float(np.mean(busy_u_ipc_data))
            busy_cv = (
                float(np.std(busy_u_ipc_data)) / busy_average_u_ipc
                if busy_average_u_ipc > 0 else float("inf")
            )
            busy_required_sample_size = (z_score * busy_cv / acceptable_sampling_error) ** 2
            busy_current_sample_size = len(busy_u_ipc_data)
        else:
            busy_average_u_ipc, busy_cv = 0.0, float("inf")
            busy_required_sample_size, busy_current_sample_size = float("inf"), 0

        # Plot the original (keep-zeros) distribution UNCHANGED, then ADD a second plot for the
        # busy/drop-zeros distribution. Two plots total; the first is exactly as before.
        if plot_enabled:
            title = plot_title if plot_title else "U-IPC Distribution"
            _plot_single_distribution(valid_u_ipc_data, title, plot_enabled)
            _plot_single_distribution(busy_u_ipc_data, title + " (busy / drop-zeros)", plot_enabled)

        return {
            "valid_u_ipc_data": valid_u_ipc_data,
            "average_u_ipc": average_u_ipc,
            "coefficient_of_variation": coefficient_of_variation,
            "required_sample_size": required_sample_size,
            "current_sample_size": current_sample_size,
            "is_sample_size_enough": is_sample_size_enough,
            "busy_average_u_ipc": busy_average_u_ipc,
            "busy_coefficient_of_variation": busy_cv,
            "busy_required_sample_size": busy_required_sample_size,
            "busy_current_sample_size": busy_current_sample_size,
            "n_dropped": n_dropped,
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

    _check_window_bounds(instruction_data.shape[1], index)

    interval_instruction_data = _window(instruction_data, index)
    interval_instruction_data_u = _window(instruction_data_u, index)
    interval_halted_cycles_data = _window(halted_cycles_data, index)

    valid_core_ipc = {}
    valid_core_ipc_non_halted = {}

    # Calculate the IPC for each core across snapshots.
    for core_id in range(interval_instruction_data.shape[1]):
        total_cycles = 0
        total_halted_cycles = 0
        total_instructions = 0
        for snapshot_idx in range(interval_instruction_data.shape[0]):
            total_instructions += interval_instruction_data[snapshot_idx, core_id]
            total_cycles += _window_cycles(sampling_unit_size)  # * FREQ_GHZ  (see TODO at FREQ_GHZ)
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
        "busy_average_u_ipc": u_ipc_stats.get("busy_average_u_ipc"),
        "busy_required_sample_size": u_ipc_stats.get("busy_required_sample_size"),
        "busy_current_sample_size": u_ipc_stats.get("busy_current_sample_size"),
        "n_dropped": u_ipc_stats.get("n_dropped", 0),
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

    # ADD — SEPARATE second table (the keep-zeros "Summary Statistics" above is unchanged):
    # busy-window / drop-zeros U-IPC = busy-cycle efficiency over snapshots that did user work
    # (idle/zero/NaN snapshots excluded), matching result.py's OMMIT_ZERO.
    if results.get("busy_average_u_ipc") is not None:
        busy_table = Table(title="Summary Statistics (busy / drop-zeros U-IPC)", box=box.DOUBLE)
        busy_table.add_column("Metric", style="cyan")
        busy_table.add_column("Value", style="green")
        busy_table.add_row("Average U-IPC (busy)", f"{results['busy_average_u_ipc']:.4f}")
        busy_table.add_row("Busy sample size", str(results["busy_current_sample_size"]))
        busy_table.add_row("Dropped (idle / zero / NaN)", str(results.get("n_dropped", 0)))
        busy_table.add_row(
            "Required Sample Size (busy)", f"{results['busy_required_sample_size']:.1f}"
        )
        console.print(busy_table)

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

    _check_window_bounds(instruction_data_u.shape[1], index)

    # Sum the measurement window's deltas: [snapshots, cores]
    interval_instruction_u_data = _window(instruction_data_u, index)

    # Calculate IPC for each snapshot and core
    interval_ipc_u_data = interval_instruction_u_data / _window_cycles(sampling_unit_size)

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
            valid_data = [x for x in snapshot_group_u_ipc]
            unvalid_count = len(snapshot_group_u_ipc) - len(valid_data)
            if unvalid_count > 0:
                console.print(
                    f"[yellow]Warning: Dropped {unvalid_count} snapshots with NaN or zero U-IPC (idle or invalid data) for this group[/yellow]"
                )

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
            # ADD accurate-idle variant (wfi_cycles) next to the legacy one (halted_cycles ~0 idle).
            whi_wfi = calculate_weighted_harmonic_average(
                measurement_data, index, valid_cores, idle_source="wfi"
            )
            group_table.add_row("Weighted Harmonic IPC (wfi, accurate)", f"{whi_wfi:.4f}")
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

            # ADD second per-group table for the busy / drop-zeros distribution (pairs with 2nd plot).
            g_busy = [x for x in snapshot_group_u_ipc if (not math.isnan(x)) and x > 0]
            if g_busy:
                gb_avg = float(np.mean(g_busy))
                gb_std = float(np.std(g_busy))
                gb_cv = gb_std / gb_avg if gb_avg > 0 else float("inf")
                gb_req = (z_score * gb_cv / acceptable_sampling_error) ** 2
                gbt = Table(
                    title=f"Core Group {group_idx + 1} Statistics (busy / drop-zeros)", box=box.ROUNDED
                )
                gbt.add_column("Metric", style="cyan")
                gbt.add_column("Value", style="green")
                gbt.add_row("Busy snapshots", str(len(g_busy)))
                gbt.add_row(
                    "Dropped (idle / zero / NaN)", str(len(snapshot_group_u_ipc) - len(g_busy))
                )
                gbt.add_row("Average U-IPC (busy)", f"{gb_avg:.4f}")
                gbt.add_row("Coefficient of variation", f"{gb_cv:.4f}")
                gbt.add_row("Required sample size (busy)", f"{gb_req:.1f}")
                console.print(gbt)

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
        valid_data = [x for x in snapshot_total_u_ipc]

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
        # ADD accurate-idle variant (wfi_cycles) next to the legacy one (halted_cycles ~0 idle).
        whi_wfi_all = calculate_weighted_harmonic_average(
            measurement_data, index, idle_source="wfi"
        )
        all_cores_table.add_row("Weighted Harmonic IPC (wfi, accurate)", f"{whi_wfi_all:.4f}")
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
        console.print(
            "[dim]Weighted Harmonic IPC shown two ways: legacy uses the dead halted_cycles column "
            "(~0 idle → ≈ normal IPC); '(wfi, accurate)' uses the real wfi_cycles idle counter — they "
            "diverge once the core idles (e.g. single-node WS, ~8% idle).[/dim]"
        )

        # ADD second all-cores table for the busy / drop-zeros distribution — pairs with the second
        # (busy) plot. The table above (keep-zeros) is unchanged.
        busy_data = [x for x in snapshot_total_u_ipc if (not math.isnan(x)) and x > 0]
        if busy_data:
            b_avg = float(np.mean(busy_data))
            b_std = float(np.std(busy_data))
            b_cv = b_std / b_avg if b_avg > 0 else float("inf")
            b_req = (z_score * b_cv / acceptable_sampling_error) ** 2
            busy_all_table = Table(title="All Cores Statistics (busy / drop-zeros)", box=box.ROUNDED)
            busy_all_table.add_column("Metric", style="cyan")
            busy_all_table.add_column("Value", style="green")
            busy_all_table.add_row("Busy snapshots", str(len(busy_data)))
            busy_all_table.add_row(
                "Dropped (idle / zero / NaN)", str(len(snapshot_total_u_ipc) - len(busy_data))
            )
            busy_all_table.add_row("Average U-IPC (busy)", f"{b_avg:.4f}")
            busy_all_table.add_row("Standard deviation", f"{b_std:.4f}")
            busy_all_table.add_row("Coefficient of variation", f"{b_cv:.4f}")
            busy_all_table.add_row("Required sample size (busy)", f"{b_req:.1f}")
            console.print(busy_all_table)

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

    # Cycle-denominated memory + cross-node columns. Optional so an older timing.csv still
    # gets the base diagnostics; the memory-latency / L2-split tables just don't render.
    optional = [
        "mem_offchip_req_count", "mem_offchip_req_latency", "mem_offchip_retire_stalls",
        "mem_onchip_req_count", "mem_onchip_req_latency", "mem_onchip_retire_stalls",
        "l2_miss_peer", "l2_miss_memory",
        "exc_entries_system", "exc_entries_idle", "exc_entries_trap",
        "exc_entries_user", "resync_interrupt",
    ]
    present_optional = [c for c in optional if c in df.columns]

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

    deltas = {c: delta(c) for c in cumulative + present_optional}
    maf_arr = strided("maf")
    n_units = deltas["instruction"].shape[1]
    if index + MEASURE_UNITS > n_units:
        console.print(
            f"[yellow]Diagnostics skipped: window [{index}, {index + MEASURE_UNITS}) out of bounds (only {n_units} units)[/yellow]"
        )
        return None

    sl = slice(index, index + MEASURE_UNITS)
    # Per-snapshot user commits in the window (summed across cores): how many snapshots did zero
    # user work — the "server idle/waiting" signal that result.py's OMMIT_ZERO would censor.
    per_snap_user = deltas["instruction:u"][:, sl, :].sum(axis=(1, 2))
    per_snap_cyc = deltas["core_cycles"][:, sl, :].sum(axis=(1, 2))
    per_snap_wfi = deltas["wfi_cycles"][:, sl, :].sum(axis=(1, 2))
    # Snapshot census: of N snapshots, how many add no user instructions and why. NaN = no cycles
    # advanced (undefined U-IPC); zero = cycles ran but 0 user commits; of the zeros, how many were
    # idle (wfi_cycles>0, i.e. the core was halted — needs the ++theWFI fix to be nonzero).
    n_snap = int(per_snap_user.shape[0])
    n_nan = int((per_snap_cyc == 0).sum())
    n_zero = int(((per_snap_user == 0) & (per_snap_cyc > 0)).sum())
    n_zero_idle = int(((per_snap_user == 0) & (per_snap_cyc > 0) & (per_snap_wfi > 0)).sum())
    n_contrib = int((per_snap_user > 0).sum())
    zero_info = (n_snap, int((per_snap_user == 0).sum()))  # kept for back-compat (n_snap, n_zero_total)
    census = {"total": n_snap, "contributing": n_contrib, "zero": n_zero,
              "zero_idle_wfi": n_zero_idle, "nan": n_nan}

    totals, maf_by_core = {}, {}
    for core in range(n_core):
        tot_instr = float(deltas["instruction"][:, sl, core].sum())
        if tot_instr <= 0:
            continue  # inactive core for this unit
        totals[core] = {c: float(deltas[c][:, sl, core].sum()) for c in cumulative + present_optional}
        vals = maf_arr[:, sl, core].ravel()
        vals = vals[vals > 0]
        maf_by_core[core] = float(vals.mean()) if len(vals) else 0.0
    return totals, maf_by_core, zero_info, census


def report_diagnostics(
    csv_path: str, index: int, sampling_unit_size: int,
    core_groups: list[list[int]] | None = None,
):
    """Print per-core diagnostic tables for the all-three-shrink investigation."""
    result = _diag_window_totals(csv_path, index, sampling_unit_size)
    if result is None:
        return
    totals, maf_by_core, zero_info, census = result
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
                "idle%", "kern%(insn)", "useful%", "spin-IPC"]:
        ta.add_column(col, justify="center")

    def ipc_row(label, t):
        cyc = t["core_cycles"] or 1
        ipc = t["instruction"] / cyc
        uipc_u = t["instruction:u"] / cyc
        uipc_ns = (t["instruction:u"] + t["commits_nonspin_system"]) / cyc
        beta = t["spin_cycles"] / cyc
        idle = t["wfi_cycles"] / cyc
        kern = t["commits_nonspin_system"] / (t["instruction"] or 1)
        spin_instr = t["commits_spin_user"] + t["commits_spin_system"]
        spin_ipc = spin_instr / t["spin_cycles"] if t["spin_cycles"] > 0 else 0.0
        ta.add_row(label, f"{ipc:.4f}", f"{uipc_u:.4f}", f"{uipc_ns:.4f}",
                   f"{ipc - uipc_u:.4f}", f"{100 * beta:.3f}", f"{100 * idle:.3f}",
                   f"{100 * kern:.3f}", f"{100 * (1 - beta - idle):.3f}", f"{spin_ipc:.4f}")

    for c in cores:
        ipc_row(str(c), totals[c])
    ipc_row("ALL", agg(all_keys))
    console.print(ta)

    # Snapshot census: of N snapshots, how many add NO user instructions this window, and why.
    # These are exactly what the busy/drop-zeros U-IPC (and result.py's OMMIT_ZERO) excludes.
    tcz = Table(title="Diagnostics — snapshot census (why snapshots add no user instructions)", box=box.ROUNDED)
    for col in ["category", "snapshots", "% of total"]:
        tcz.add_column(col, justify="center")
    N = census["total"] or 1
    tcz.add_row("contributing (U-IPC > 0)", str(census["contributing"]), f"{100*census['contributing']/N:.1f}")
    tcz.add_row("zero U-IPC (cycles ran, 0 user commits)", str(census["zero"]), f"{100*census['zero']/N:.1f}")
    tcz.add_row("  └ of which idle/WFI (wfi_cycles>0)", str(census["zero_idle_wfi"]), f"{100*census['zero_idle_wfi']/N:.1f}")
    tcz.add_row("NaN U-IPC (no cycles advanced)", str(census["nan"]), f"{100*census['nan']/N:.1f}")
    tcz.add_row("TOTAL snapshots", str(census["total"]), "100.0")
    console.print(tcz)
    console.print(
        "[dim]contributing + zero + NaN = total; 'idle/WFI' is the subset of the zeros where the core "
        "was halted (WFI) — needs the ++theWFI fix to be nonzero. These are the censored windows.[/dim]"
    )

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

    # Table B2: literal memory latency + cross-node cache split. Only if collect.py emitted the
    # cycle-denominated columns (older timing.csv → skipped, base tables unaffected).
    if "mem_offchip_req_count" in next(iter(totals.values())):
        tb2 = Table(title="Diagnostics — memory latency & cross-node (literal)", box=box.ROUNDED)
        for col in ["Core", "off-chip avg lat (cyc)", "off-chip reqs", "off-chip stall%",
                    "on-chip avg lat (cyc)", "L2-Peer-MPKI", "L2-Mem-MPKI"]:
            tb2.add_column(col, justify="center")

        def mem_row(label, t):
            instr = t["instruction"] or 1
            cyc = t["core_cycles"] or 1
            off_cnt, on_cnt = t["mem_offchip_req_count"], t["mem_onchip_req_count"]
            off_avg = t["mem_offchip_req_latency"] / off_cnt if off_cnt > 0 else 0.0
            on_avg = t["mem_onchip_req_latency"] / on_cnt if on_cnt > 0 else 0.0
            tb2.add_row(label, f"{off_avg:.1f}", f"{int(off_cnt)}",
                        f"{100 * t['mem_offchip_retire_stalls'] / cyc:.3f}", f"{on_avg:.1f}",
                        f"{1000 * t['l2_miss_peer'] / instr:.3f}",
                        f"{1000 * t['l2_miss_memory'] / instr:.3f}")

        for c in cores:
            mem_row(str(c), totals[c])
        mem_row("ALL", agg(all_keys))
        console.print(tb2)
        console.print("[dim]off-chip avg lat = req_latency/req_count (cyc) — rises if multi-node "
                      "coherence/memory is slower; L2-Peer = cross-node cache-to-cache, L2-Mem = local DRAM.[/dim]")

    # Table B3: exception/interrupt ENTRIES by privilege context + async-IRQ resyncs. Count of taken
    # exceptions/IRQs (one Flexus "Exception" insn per entry). exc:Trap dominates and LUMPS sync
    # syscalls/faults with async IRQs (can't isolate interrupts). Resync:IRQ = interrupt-caused
    # resyncs (0 ≠ "no interrupts"). nic_recv/sent here are the Flexus on-chip COHERENCE-network msg
    # counts (MultiNic→MemoryNetwork), ∝ cache misses — NOT guest NIC packets / device RX IRQs.
    if "exc_entries_trap" in next(iter(totals.values())):
        te = Table(title="Diagnostics — exception/interrupt entries (by context; exc:Trap lumps syscalls+IRQs; nic=coherence msgs)",
                   box=box.ROUNDED)
        for col in ["Core", "exc:User", "exc:System", "exc:Idle", "exc:Trap", "exc:total",
                    "Resync:IRQ", "nic_recv", "nic_sent"]:
            te.add_column(col, justify="center")

        def exc_row(label, t):
            u = t.get("exc_entries_user", 0)
            s, i, tr = t["exc_entries_system"], t["exc_entries_idle"], t["exc_entries_trap"]
            te.add_row(label, f"{int(u)}", f"{int(s)}", f"{int(i)}", f"{int(tr)}",
                       f"{int(u + s + i + tr)}", f"{int(t.get('resync_interrupt', 0))}",
                       f"{int(t['nic_recv'])}", f"{int(t['nic_sent'])}")

        for c in cores:
            exc_row(str(c), totals[c])
        exc_row("ALL", agg(all_keys))
        console.print(te)
        console.print("[dim]= # exceptions/IRQs/faults taken (by where taken). exc:Trap lumps sync "
                      "syscalls/faults with async IRQs. Resync:IRQ = interrupt-caused resyncs (0 ≠ no "
                      "interrupts). nic_recv/sent = on-chip COHERENCE traffic (∝ cache misses), NOT guest "
                      "NIC RX. Per-IRQ-type (timer/IPI/NIC) needs /proc/interrupts or a Flexus counter (see TODO).[/dim]")

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
    console.print("[dim]CPI-stack is RELATIVE attribution (not literal cycles); "
                  "mem_remote = cross-node (Remote/PeerL) stalls — the multi-node signal.[/dim]")

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


def report_per_core_commits(index, sampling_unit_size, run_glob="run/partition_*/result_*"):
    """Per-idx, per-existing-core committed-instruction counts over the measurement window.
    Generic: discovers real cores (NNN-uarch-Commits) and phantom cores (Phantom-N-CommitCount)
    straight from the logs, so it works for single-node (server core + phantom client) and
    multi-node (real cores) alike. Additive/read-only — never affects the U-IPC numbers above.
    Window = cum(end_dump) - cum(start_dump), units [index, index+MEASURE_UNITS)."""
    import glob, re
    start = index * INTERVAL
    end = (index + MEASURE_UNITS) * INTERVAL

    def read(path):
        if not os.path.exists(path):
            return None
        out = {}
        for line in open(path):
            parts = line.split()
            if len(parts) < 2:
                continue
            m = re.match(r"(\d+)-uarch-Commits$", parts[0])
            if m:
                out[f"core{int(m.group(1))}"] = int(parts[1]); continue
            m = re.match(r"Phantom-(\d+)-CommitCount$", parts[0])
            if m:
                out[f"phantom{int(m.group(1))}"] = int(parts[1])
        return out

    dirs = glob.glob(run_glob)
    if not dirs:
        return
    rows = []
    for d in sorted(dirs, key=lambda p: int(os.path.basename(p).split("_")[-1])):
        idx = int(os.path.basename(d).split("_")[-1])
        a = read(f"{d}/all.measurement.{start:010}.log")
        b = read(f"{d}/all.measurement.{end:010}.log")
        if a is None or b is None:
            continue
        cores = set(a) | set(b)
        rows.append((idx, {c: b.get(c, 0) - a.get(c, 0) for c in cores}))
    if not rows:
        return

    labels = sorted({c for _, d in rows for c in d},
                    key=lambda s: (s.startswith("phantom"), int(re.sub(r"\D", "", s) or 0)))
    console.print(f"\n[bold cyan]=== Per-idx commits by core (window units "
                  f"[{index},{index + MEASURE_UNITS}); real cores + phantom) ===[/bold cyan]")
    for idx, d in rows:
        console.print(f"idx {idx}: " + "  ".join(f"{c}={d.get(c, 0)}" for c in labels))
    # Compact summary across idxs: how often each core is active, and (when ≥2 cores exist) how
    # often more than one is active in the same window — the mutual-exclusion sanity check.
    THR = 1000
    active = {c: sum(1 for _, d in rows if d.get(c, 0) > THR) for c in labels}
    multi = sum(1 for _, d in rows if sum(1 for c in labels if d.get(c, 0) > THR) >= 2)
    console.print(f"[dim]active windows (>{THR} commits) per core: {active}; "
                  f"windows with ≥2 cores active simultaneously: {multi}/{len(rows)}[/dim]")


def report_pdes_wire(run_glob: str = "run/partition_*/log") -> None:
    """Additive: cross-node data moved over the PDES wire (MULTI-NODE only). Sums the `[PDES-WIRE]`
    lines the timing qemu prints at engine teardown (one per idx run) from the partition logs. Single-
    node has no PDES wire (client↔server is loopback, invisible to the sim) → prints a note. Skipped
    silently if no logs / no lines (pre-instrumentation runs)."""
    import glob, re
    files = glob.glob(run_glob) + glob.glob("run/partition_*/err")
    pat = re.compile(
        r"\[PDES-WIRE\] data_bytes_sent=(\d+) data_msgs_sent=(\d+) "
        r"data_bytes_recv=(\d+) data_msgs_recv=(\d+)"
    )
    bs = ms = br = mr = runs = 0
    for f in files:
        try:
            with open(f) as fh:
                for line in fh:
                    m = pat.search(line)
                    if m:
                        bs += int(m.group(1)); ms += int(m.group(2))
                        br += int(m.group(3)); mr += int(m.group(4)); runs += 1
        except OSError:
            continue
    if runs == 0:
        console.print(
            "[yellow]Cross-node data moved (PDES wire): none found. Expected on single-node "
            "(client↔server is loopback — no PDES wire). For multi-node, create it with:\n"
            "    ./qflex generate-test-communication -c <this experiment's yaml> --duration-seconds 1\n"
            "(--duration-seconds is GUEST seconds; the guest /proc/net/dev table above compares single vs multi regardless).[/yellow]"
        )
        return
    t = Table(title="Diagnostics — cross-node data moved (PDES wire, NORMAL guest packets)", box=box.ROUNDED)
    for col in ["direction", "bytes", "chunks (msgs)", "avg bytes/chunk"]:
        t.add_column(col, justify="center")
    t.add_row("sent", str(bs), str(ms), f"{bs/ms:.1f}" if ms else "-")
    t.add_row("recv", str(br), str(mr), f"{br/mr:.1f}" if mr else "-")
    console.print(t)
    console.print(
        f"[dim]summed over {runs} [PDES-WIRE] line(s). MULTI-NODE only — single-node's loopback "
        f"client↔server isn't visible sim-side; for the single-vs-multi comparison use /proc/net/dev.[/dim]"
    )


def _parse_netdev(path: str):
    """{iface: (rx_bytes, rx_pkts, tx_bytes, tx_pkts)} from a captured /proc/net/dev dump (ignores the
    typed-echo / marker / prompt lines — only iface rows with 16 trailing numbers match)."""
    import re
    out = {}
    try:
        for line in open(path):
            m = re.match(r"\s*([A-Za-z0-9_.@-]+):\s+((?:\d+\s+){15}\d+)", line)
            if m:
                nums = [int(x) for x in m.group(2).split()]
                if len(nums) >= 16:  # rx: bytes packets errs...(8) ; tx: bytes packets...(8)
                    out[m.group(1)] = (nums[0], nums[1], nums[8], nums[9])
    except OSError:
        return None
    return out or None


def report_netdev() -> None:
    """Additive: guest /proc/net/dev byte + packet (chunk) DELTA per interface over the
    generate-test-communication window. single -> `lo` (localhost loopback); multi -> `lo` + `eth0`
    (cross-node wire). Skipped (with how-to-create) if the capture is absent."""
    before = _parse_netdev("netdev_before.txt")
    after = _parse_netdev("netdev_after.txt")
    if not before or not after:
        console.print(
            "[yellow]No data-movement capture (netdev_*.txt) found. Create it with:\n"
            "    ./qflex generate-test-communication -c <this experiment's yaml> --duration-seconds 1\n"
            "(--duration-seconds is GUEST seconds; resumes `loaded`, captures /proc/net/dev; no savevm, qcow untouched).[/yellow]"
        )
        return
    t = Table(title="Diagnostics — data moved (guest /proc/net/dev delta over the capture window)", box=box.ROUNDED)
    for col in ["iface", "rx bytes", "rx pkts (chunks)", "tx bytes", "tx pkts (chunks)", "avg rx B/pkt", "avg tx B/pkt"]:
        t.add_column(col, justify="center")
    shown = 0
    for iface in sorted(set(before) & set(after)):
        rb = after[iface][0] - before[iface][0]
        rp = after[iface][1] - before[iface][1]
        tb = after[iface][2] - before[iface][2]
        tp = after[iface][3] - before[iface][3]
        if rb == 0 and tb == 0 and rp == 0 and tp == 0:
            continue
        t.add_row(iface, str(rb), str(rp), str(tb), str(tp),
                  f"{rb/rp:.1f}" if rp else "-", f"{tb/tp:.1f}" if tp else "-")
        shown += 1
    if shown:
        console.print(t)
    console.print(
        "[dim]lo = localhost (single-node client↔server loopback); eth0 = cross-node wire (multi-node). "
        "pkts = chunks. Compare single (lo) vs multi (lo + eth0).[/dim]"
    )


def report_interrupts() -> None:
    """Additive: /proc/interrupts DELTA over the capture window, by IRQ — the per-type breakdown
    (timer / reschedule-IPI / NIC RX) that the Flexus lumped exc:Trap counter can't give. Skipped if
    the capture is absent (the netdev warning already says how to create it)."""
    import re
    def parse(path):
        out = {}
        try:
            for line in open(path):
                m = re.match(r"\s*([A-Za-z0-9_-]+):\s+((?:\d+\s+)*\d+)\s*(\S.*)?$", line)
                if m:
                    total = sum(int(x) for x in m.group(2).split())
                    out[m.group(1)] = (total, (m.group(3) or "").strip())
        except OSError:
            return None
        return out or None
    before = parse("interrupts_before.txt")
    after = parse("interrupts_after.txt")
    if not before or not after:
        return
    rows = []
    for irq in set(before) & set(after):
        d = after[irq][0] - before[irq][0]
        if d > 0:
            rows.append((d, irq, after[irq][1]))
    if not rows:
        return
    t = Table(title="Diagnostics — interrupts taken (/proc/interrupts delta, by IRQ)", box=box.ROUNDED)
    for col in ["irq", "name", "count delta"]:
        t.add_column(col, justify="center")
    for d, irq, name in sorted(rows, reverse=True)[:25]:
        t.add_row(irq, name[:44], str(d))
    console.print(t)
    console.print(
        "[dim]Guest IRQ counts — timer (arch_timer), reschedule IPI, NIC RX (virtio/eth), etc. "
        "This is the per-IRQ-type breakdown the Flexus exc:Trap counter lumps together.[/dim]"
    )


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
        "--interval",
        type=int,
        default=100000,
        help="Stats-dump interval in cycles; the experiment's stat_interval_cycles (default: 100000)",
    )
    parser.add_argument(
        "--measure-units",
        type=int,
        default=1,
        help="Number of consecutive intervals aggregated as the measurement window, starting at --index; the experiment's measurement_ratio (default: 1)",
    )
    parser.add_argument(
        "--no-exit-on-fail",
        action="store_true",
        help="Do not sys.exit(-1) when sampling-error bounds are unmet. Used by the statistical-sample loop, where an unmet bound is the normal signal to take another iteration.",
    )
    args = parser.parse_args()

    global FREQ_GHZ, INTERVAL, MEASURE_UNITS
    FREQ_GHZ = args.freq_ghz
    INTERVAL = args.interval
    MEASURE_UNITS = args.measure_units

    console.print(
        Panel.fit(
            "[bold green]Measurement Data Analysis Tool[/bold green]",
            border_style="green",
        )
    )
    console.print(
        f"[green]Window: interval={INTERVAL} cycles, measurement = units "
        f"[{args.index}, {args.index + MEASURE_UNITS}) = {INTERVAL * MEASURE_UNITS} cycles[/green]"
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
    report_per_core_commits(args.index, args.unit_size)
    report_pdes_wire()
    report_netdev()
    report_interrupts()

    # Exit with -1 if sampling error bounds were not satisfied
    if not all_bounds_satisfied and not args.no_exit_on_fail:
        console.print(
            "[red]Error: Not all groups satisfy the sampling error bound. Exiting with -1.[/red]"
        )
        sys.exit(-1)


if __name__ == "__main__":
    main()
