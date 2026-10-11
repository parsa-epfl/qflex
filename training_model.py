#!/usr/bin/env python3
"""
training_model.py - Train IPC model using non-negative least squares on collected statistics.

This script reads the output of collect.py, computes differences between sampling intervals,
splits data into training and test sets, and fits a non-negative least squares model without intercept.
All coefficients are constrained to be >= 0.

Model: NonIdleCycles = P * X^T
where:
  - NonIdleCycles = (core_cycles - halted_cycles) / FREQ  (in ns)
  - X = FEATURE_NAMES = [bx_private_icache_miss, bx_private_dcache_miss, bx_shared_cache_miss,
         bx_bp_miss, bx_drain_store_buffer, bx_instruction]
  - P = vector of 6 non-negative coefficients (no offset)

The trained cores are written to core_info_ipc_model_new.csv in the 9-column format
consumed by fw-qemu (model_type=ipc-model, ipns=1.0, one *_coeff column per feature).
"""

import argparse
import csv
import json
import os
import random
import shutil
import sys
from datetime import datetime
import numpy as np
import plotille
from scipy.optimize import nnls, lsq_linear
from scipy.cluster.vq import kmeans2, whiten
from typing import Any, List, Tuple, Dict
from rich.console import Console
from rich.table import Table
from rich.panel import Panel
from rich.text import Text
from rich import box
from rich.align import Align
from rich.columns import Columns

FREQ = 2

# Features used for training - modify this list to include/exclude features
FEATURE_NAMES = [
    "bx_private_icache_miss",
    "bx_private_dcache_miss",
    "bx_shared_cache_miss",
    "bx_bp_miss",
    "bx_drain_store_buffer",
    "bx_instruction"
]

# New format columns derived from FEATURE_NAMES so they always stay in sync
NEW_FORMAT_COLUMNS = ["host_core_idx", "model_type", "ipns"] + [
    f"{name}_coeff" for name in FEATURE_NAMES
]

console = Console()


def parse_existing_core_info(file_path: str) -> Dict[int, Dict]:
    """
    Parse and validate existing core_info.csv file.

    Returns dict mapping host_core_idx -> row_data
    Exits with error if validation fails.
    """
    if not os.path.exists(file_path):
        console.print(f"[red]Error: core_info.csv file not found: {file_path}[/red]")
        sys.exit(1)

    try:
        with open(file_path, "r", newline="") as f:
            reader = csv.DictReader(f)
            fieldnames = reader.fieldnames or []

            # Validate header
            if fieldnames != NEW_FORMAT_COLUMNS:
                console.print(f"[red]Error: Invalid header in {file_path}[/red]")
                console.print(f"[red]Expected: {NEW_FORMAT_COLUMNS}[/red]")
                console.print(f"[red]Found: {fieldnames}[/red]")
                sys.exit(1)

            core_data = {}
            for row_idx, row in enumerate(reader, 1):
                try:
                    # Validate host_core_idx is integer (but use row index as target core index)
                    host_core_idx = int(row["host_core_idx"])

                    # Use row index (0-based) as target core index for grouping
                    target_core_idx = row_idx - 1

                    # Check for duplicates (shouldn't happen with enumerate, but defensive)
                    if target_core_idx in core_data:
                        console.print(
                            f"[red]Error: Duplicate target core index {target_core_idx} at row {row_idx}[/red]"
                        )
                        sys.exit(1)

                    # Validate model_type
                    model_type = row["model_type"]
                    if model_type not in ["constant", "ipc-model"]:
                        console.print(
                            f"[red]Error: Invalid model_type '{model_type}' at row {row_idx}[/red]"
                        )
                        console.print(f"[red]Expected: 'constant' or 'ipc-model'[/red]")
                        sys.exit(1)

                    # Validate ipns is numeric
                    try:
                        float(row["ipns"])
                    except ValueError:
                        console.print(
                            f"[red]Error: Invalid ipns value '{row['ipns']}' at row {row_idx}[/red]"
                        )
                        sys.exit(1)

                    # Validate all coefficient columns are numeric
                    for col in NEW_FORMAT_COLUMNS[3:]:  # Skip first 3 columns
                        try:
                            float(row[col])
                        except ValueError:
                            console.print(
                                f"[red]Error: Invalid value '{row[col]}' in column {col} at row {row_idx}[/red]"
                            )
                            sys.exit(1)

                    core_data[target_core_idx] = row

                except ValueError as e:
                    console.print(
                        f"[red]Error: Invalid data at row {row_idx}: {e}[/red]"
                    )
                    sys.exit(1)

            return core_data

    except csv.Error as e:
        console.print(f"[red]Error: CSV parsing error in {file_path}: {e}[/red]")
        sys.exit(1)
    except Exception as e:
        console.print(f"[red]Error: Failed to read {file_path}: {e}[/red]")
        sys.exit(1)


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


def load_data(csv_path: str) -> List[Dict]:
    """Load data from collect.py output CSV."""
    data = []
    with open(csv_path, "r") as f:
        reader = csv.DictReader(f)

        # Validate all required columns are present before processing
        required_columns = set(FEATURE_NAMES) | {
            "snapshot_id", "core", "sys_cycles", "halted_cycles", "core_cycles",
        }
        csv_columns = set(reader.fieldnames or [])
        missing = required_columns - csv_columns
        if missing:
            console.print(
                f"[red]Error: CSV is missing required columns: {sorted(missing)}[/red]"
            )
            sys.exit(1)

        for row in reader:
            data.append(
                {
                    "snapshot_id": row["snapshot_id"],
                    "core": int(row["core"]),
                    "sys_cycles": int(row["sys_cycles"]),
                    "instruction": int(row["instruction"]),
                    "instruction_u": int(row["instruction:u"]),
                    "itlb_miss": int(row["itlb_miss"]),
                    "dtlb_miss": int(row["dtlb_miss"]),
                    "stlb_miss": int(row["stlb_miss"]),
                    "btb_miss": int(row["btb_miss"]),
                    "tage_miss": int(row["tage_miss"]),
                    "l1i_miss": int(row["l1i_miss"]),
                    "l1d_miss": int(row["l1d_miss"]),
                    "l2_miss": int(row["l2_miss"]),
                    "halted_cycles": int(row["halted_cycles"]),
                    "core_cycles": int(row["core_cycles"]),
                    "bx_instruction": int(row["bx_instruction"]),
                    **{feature: int(row[feature]) for feature in FEATURE_NAMES},
                }
            )
    return data


def compute_differences(
    data: List[Dict], interval_index: int, core_ids: list[int] | None = None
) -> List[Dict]:
    """
    Compute differences between consecutive sampling intervals for each core.

    interval_index: 0 means return first interval as-is, 1 means return diff between
                   second and first interval, 2 means diff between third and second, etc.
    core_ids: Optional list of core IDs to filter. If None, all cores are included.
    Returns list of diff samples for the specified interval.
    """
    # Filter data by core_ids if specified
    if core_ids is not None:
        data = [row for row in data if row["core"] in core_ids]

    # Keep only rows at regular 100000-cycle measurement points.
    # Boundary rows inserted at ASID-change events land at non-multiples of 100000
    # and must be excluded: they have valid ASIDs but represent partial intervals,
    # so including them would cause positional indexing in compute_differences() to
    # pick up partial-interval rows instead of the intended full measurement points.
    data = [row for row in data if row["sys_cycles"] % 100000 == 0]

    # Group data by snapshot_id and core
    grouped = {}
    for row in data:
        key = (row["snapshot_id"], row["core"])
        if key not in grouped:
            grouped[key] = []
        grouped[key].append(row)

    # Sort each group by sys_cycles
    for key in grouped:
        grouped[key].sort(key=lambda x: x["sys_cycles"])

    # Compute differences for the specified interval
    diffs = []
    for key, group in grouped.items():
        if interval_index == 0:
            # Return first interval as-is (no diff)
            if len(group) < 1:
                continue
            curr = group[0]
            diff = {
                "snapshot_id": curr["snapshot_id"],
                "core": curr["core"],
                "sys_cycles": curr["sys_cycles"],
                "interval": interval_index,
                # NonIdleCycles = (core_cycles - halted_cycles) / FREQ (convert to ns)
                "non_idle_cycles": (curr["core_cycles"] - curr["halted_cycles"]) / FREQ,
                # Features (bx_* stats) - driven by FEATURE_NAMES
                "bx_instruction": curr["bx_instruction"],
                **{feature: curr[feature] for feature in FEATURE_NAMES},
            }
            diffs.append(diff)
        else:
            # Return diff between interval_index and interval_index-1
            if len(group) < interval_index + 1:
                continue
            prev = group[interval_index - 1]
            curr = group[interval_index]

            diff = {
                "snapshot_id": curr["snapshot_id"],
                "core": curr["core"],
                "sys_cycles": curr["sys_cycles"],
                "interval": interval_index,
                # NonIdleCycles = (core_cycles - halted_cycles) / FREQ (convert to ns)
                "non_idle_cycles": (
                    (curr["core_cycles"] - curr["halted_cycles"])
                    - (prev["core_cycles"] - prev["halted_cycles"])
                )
                / FREQ,
                # Features (bx_* stats) - driven by FEATURE_NAMES
                "bx_instruction": curr["bx_instruction"] - prev["bx_instruction"],
                **{feature: curr[feature] - prev[feature] for feature in FEATURE_NAMES},
            }

            if diff["non_idle_cycles"] == 0:
                continue

            diffs.append(diff)

    return diffs


def split_data(
    diffs: List[Dict], test_ratio: float, random_seed: int = 42
) -> Tuple[List[Dict], List[Dict]]:
    """Split data into training and test sets randomly."""
    random.seed(random_seed)
    shuffled = diffs.copy()
    random.shuffle(shuffled)

    test_size = int(len(shuffled) * test_ratio)
    test_set = shuffled[:test_size]
    train_set = shuffled[test_size:]

    return train_set, test_set


def extract_features(diffs: List[Dict]) -> Tuple[np.ndarray, np.ndarray]:
    """
    Extract feature matrix X and target vector y from diff data.

    X: (n_samples, n_features) matrix of bx_* features, where n_features
       is determined by the global FEATURE_NAMES list.
    y: (n_samples,) vector of non_idle_cycles
    """
    n_samples = len(diffs)
    n_features = len(FEATURE_NAMES)
    X = np.zeros((n_samples, n_features))
    y = np.zeros(n_samples)

    for i, diff in enumerate(diffs):
        y[i] = diff["non_idle_cycles"]
        for j, feature in enumerate(FEATURE_NAMES):
            X[i, j] = diff[feature]

    return X, y


NOC_FEATURES = {"bx_read_noc_hop", "bx_write_noc_hop", "bx_ifetch_noc_hop"}


def fit_nnls_no_intercept(
    X: np.ndarray,
    y: np.ndarray,
    min_instruction_coeff: float = 0.0,
    no_noc: bool = False,
) -> np.ndarray:
    """
    Fit non-negative least squares model without intercept.

    Model: y = X @ P, subject to P >= 0 (and P[bx_instruction] >= min_instruction_coeff)
    Uses scipy.optimize.nnls when min_instruction_coeff == 0, otherwise falls back
    to scipy.optimize.lsq_linear with per-variable lower bounds.  When no_noc is True,
    NOC feature columns are excluded from the fit entirely and their coefficients are
    set to zero in the returned vector.
    """
    if no_noc:
        keep = [i for i, name in enumerate(FEATURE_NAMES) if name not in NOC_FEATURES]
        X_fit = X[:, keep]
        keep_names = [FEATURE_NAMES[i] for i in keep]
    else:
        keep = list(range(len(FEATURE_NAMES)))
        X_fit = X
        keep_names = FEATURE_NAMES

    if min_instruction_coeff > 0.0:
        n = X_fit.shape[1]
        lb = np.zeros(n)
        for idx, name in enumerate(keep_names):
            if name == "bx_instruction":
                lb[idx] = min_instruction_coeff
        result = lsq_linear(X_fit, y, bounds=(lb, np.full(n, np.inf)))
        P_fit = result.x
    else:
        P_fit, _ = nnls(X_fit, y)

    P = np.zeros(len(FEATURE_NAMES))
    for i, orig_idx in enumerate(keep):
        P[orig_idx] = P_fit[i]
    # Round to 3 decimal places and ensure non-negativity
    return np.maximum(np.round(P, 3), 0)


def fit_constant_ipc_model(
    instructions: np.ndarray, non_idle_cycles: np.ndarray
) -> float:
    """
    Fit constant IPC model.

    Computes the average ratio of non_idle_cycles / instruction from training data.
    This represents the constant IPC (cycles per instruction) parameter.

    Returns: constant_ipc (cycles per instruction)
    """
    # Avoid division by zero
    mask = instructions > 0
    if np.sum(mask) == 0:
        return 0.0
    return float(np.mean(non_idle_cycles[mask] / instructions[mask]))


def predict_constant_ipc(instructions: np.ndarray, constant_ipc: float) -> np.ndarray:
    """
    Predict non-idle cycles using constant IPC model.

    predicted_non_idle_cycles = constant_ipc * instruction
    """
    return constant_ipc * instructions


def extract_ipc_features(diffs: List[Dict]) -> Tuple[np.ndarray, np.ndarray]:
    """
    Extract instruction count and non_idle_cycles for constant IPC model.

    Returns:
        instructions: (n_samples,) vector of bx_instruction counts
        non_idle_cycles: (n_samples,) vector of non_idle_cycles
    """
    n_samples = len(diffs)
    instructions = np.zeros(n_samples)
    non_idle_cycles = np.zeros(n_samples)

    for i, diff in enumerate(diffs):
        instructions[i] = diff["bx_instruction"]
        non_idle_cycles[i] = diff["non_idle_cycles"]

    return instructions, non_idle_cycles


def evaluate_model(X: np.ndarray, y: np.ndarray, P: np.ndarray, weights: np.ndarray | None = None) -> Dict:
    """Evaluate model performance on test set."""
    y_pred = X @ P

    # Residuals (errors)
    residuals = y - y_pred

    # Mean Absolute Error
    mae = np.mean(np.abs(residuals))

    # Mean Error (without absolute - shows bias)
    me = np.mean(residuals)

    # Mean Squared Error
    mse = np.mean(residuals**2)

    # Root Mean Squared Error
    rmse = np.sqrt(mse)

    mask = y != 0

    # Mean Absolute Percentage Error (avoid division by zero)
    if np.sum(mask) > 0:
        mape = np.mean(np.abs(residuals[mask] / y[mask])) * 100
    else:
        mape = float("inf")

    # Mean Percentage Error (signed)
    if np.sum(mask) > 0:
        mpe = np.mean((y_pred[mask] - y[mask]) / y[mask]) * 100
    else:
        mpe = float("inf")

    # Weighted MAPE and MPE (instruction-count weights)
    if weights is not None and np.sum(weights[mask]) > 0:
        w = weights[mask]
        w_norm = w / w.sum()
        weighted_mape = float(np.sum(w_norm * np.abs(residuals[mask] / y[mask])) * 100)
        weighted_mpe = float(np.sum(w_norm * (y_pred[mask] - y[mask]) / y[mask]) * 100)
    else:
        weighted_mape = mape
        weighted_mpe = mpe

    return {
        "mae": mae,
        "me": me,
        "mse": mse,
        "rmse": rmse,
        "mape": mape,
        "mpe": mpe,
        "weighted_mape": weighted_mape,
        "weighted_mpe": weighted_mpe,
        "residuals": residuals,
    }


def create_metrics_table(metrics: Dict, title: str, color: str = "cyan") -> Table:
    """Create a rich table for displaying model metrics."""
    table = Table(title=f"[bold]{title}[/bold]", box=box.ROUNDED, border_style=color)
    table.add_column("Metric", style="cyan", no_wrap=True)
    table.add_column("Value", justify="right", style="green")

    table.add_row("Mean Absolute Error (MAE)", f"{metrics['mae']:>12.2f}")
    table.add_row("Mean Error (ME)", f"{metrics['me']:>12.2f}")
    table.add_row("Mean Squared Error (MSE)", f"{metrics['mse']:>12.2f}")
    table.add_row("Root Mean Squared Error (RMSE)", f"{metrics['rmse']:>12.2f}")
    table.add_row("Mean Abs Percentage Error (MAPE)", f"{metrics['mape']:>11.2f}%")
    table.add_row("Mean Percentage Error (MPE)",       f"{metrics['mpe']:>11.2f}%")
    table.add_row("Weighted MAPE (instr-count)",       f"{metrics['weighted_mape']:>11.2f}%")
    table.add_row("Weighted MPE  (instr-count)",       f"{metrics['weighted_mpe']:>11.2f}%")

    return table


def plot_residual_distribution(
    residuals: np.ndarray, title: str = "Residual Distribution"
) -> None:
    """
    Plot a distribution of residuals using plotille.
    """
    if len(residuals) == 0:
        return

    fig = plotille.Figure()
    fig.width = 80
    fig.height = 20
    fig.color_mode = "byte"
    fig.x_label = "Residual (cycles)"
    fig.y_label = "Frequency"

    # Set x-axis limits based on residual range
    min_res = np.min(residuals)
    max_res = np.max(residuals)
    margin = (max_res - min_res) * 0.1 if max_res != min_res else 1
    fig.set_x_limits(min_=min_res - margin, max_=max_res + margin)

    # Plot histogram
    fig.histogram(residuals.tolist(), bins=20, lc=100)

    console.print(f"\n[bold cyan]{title}[/bold cyan]")
    print(fig.show())

    # Print some statistics using rich
    stats_table = Table(box=box.SIMPLE, show_header=False)
    stats_table.add_column("Stat", style="cyan")
    stats_table.add_column("Value", style="yellow")
    stats_table.add_row("Mean residual", f"{np.mean(residuals):.2f}")
    stats_table.add_row("Median residual", f"{np.median(residuals):.2f}")
    stats_table.add_row("Std residual", f"{np.std(residuals):.2f}")
    stats_table.add_row("Min residual", f"{np.min(residuals):.2f}")
    stats_table.add_row("Max residual", f"{np.max(residuals):.2f}")
    console.print(stats_table)


def evaluate_constant_ipc_model(
    instructions: np.ndarray, non_idle_cycles: np.ndarray, constant_ipc: float
) -> Dict:
    """Evaluate constant IPC model performance."""
    y_pred = predict_constant_ipc(instructions, constant_ipc)

    # Residuals (errors)
    residuals = non_idle_cycles - y_pred

    # Mean Absolute Error
    mae = np.mean(np.abs(residuals))

    # Mean Error (without absolute - shows bias)
    me = np.mean(residuals)

    # Mean Squared Error
    mse = np.mean(residuals**2)

    # Root Mean Squared Error
    rmse = np.sqrt(mse)

    # Mean Absolute Percentage Error (avoid division by zero)
    mask = non_idle_cycles != 0
    if np.sum(mask) > 0:
        mape = np.mean(np.abs(residuals[mask] / non_idle_cycles[mask])) * 100
    else:
        mape = float("inf")

    return {
        "mae": mae,
        "me": me,
        "mse": mse,
        "rmse": rmse,
        "mape": mape,
        "residuals": residuals,
    }


def generate_core_info_csv(
    output_path: str,
    core_groups: list[list[int]] | None,
    group_coefficients: list[np.ndarray],
    all_cores: list[int],
    original_core_data: Dict[int, Dict] | None = None,
    trained_cores: set[int] | None = None,
) -> None:
    """
    Generate core_info.csv in ipc-model format with trained coefficients.

    Args:
        output_path: Path to output CSV file
        core_groups: List of core groups (each group is a list of core IDs)
        group_coefficients: List of coefficient arrays, one per group
        all_cores: List of all core IDs that have training data
        original_core_data: Original core_info.csv data (host_core_idx -> row_data), or None
        trained_cores: Set of core IDs that have training data and should be updated
    """
    import csv

    # Build mapping from core_id to coefficient array for trained cores
    core_to_coeffs = {}

    if core_groups:
        # Map each core to its group's coefficients
        for group_idx, cores in enumerate(core_groups):
            coeffs = group_coefficients[group_idx]
            for core_id in cores:
                if core_id in all_cores:
                    core_to_coeffs[core_id] = coeffs
    else:
        # Single model for all cores
        coeffs = group_coefficients[0]
        for core_id in all_cores:
            core_to_coeffs[core_id] = coeffs

    # Prepare output data
    output_rows = []
    output_coeff_columns = [f"{name}_coeff" for name in FEATURE_NAMES]

    if original_core_data is not None:
        # Update trained cores with new coefficients
        # Iterate with target_core_idx (dict key) to match trained_cores
        for target_core_idx, row in original_core_data.items():
            if (
                trained_cores
                and target_core_idx in trained_cores
                and target_core_idx in core_to_coeffs
            ):
                coeffs = core_to_coeffs[target_core_idx]
                row["model_type"] = "ipc-model"
                row["ipns"] = "1.0"
                for j, feature in enumerate(FEATURE_NAMES):
                    row[f"{feature}_coeff"] = f"{coeffs[j]:.6f}"
            output_rows.append(row)
    else:
        # Generate fresh file with only trained cores
        for core_id in sorted(all_cores):
            coeffs = core_to_coeffs.get(core_id, group_coefficients[0])
            row = {
                "host_core_idx": str(core_id),
                "model_type": "ipc-model",
                "ipns": "1.0",
            }
            for j, feature in enumerate(FEATURE_NAMES):
                row[f"{feature}_coeff"] = f"{coeffs[j]:.6f}"
            output_rows.append(row)

    # Ensure output directory exists
    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)

    # Build output columns: fixed columns + dynamic coefficient columns
    output_columns = ["host_core_idx", "model_type", "ipns"] + output_coeff_columns

    # Write CSV with column ordering
    with open(output_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=output_columns, lineterminator="\n")
        writer.writeheader()
        writer.writerows(output_rows)

    console.print(f"[bold green]Generated core_info.csv at: {output_path}[/bold green]")
    if original_core_data:
        console.print(
            f"[green]Preserved {len(original_core_data)} cores, updated {len(trained_cores) if trained_cores else 0} cores[/green]"
        )


def print_top_error_samples(
    test_set: list[dict],
    y_pred: np.ndarray,
    group_name: str,
    top_n: int = 10,
) -> None:
    """
    Print the top_n sampling units with the largest absolute prediction error.

    Displays snapshot_id, core, sys_cycles, interval, actual non_idle_cycles,
    predicted value, and signed error to aid manual log inspection.
    The invariant always holds: predicted + error = actual.
    """
    if len(test_set) == 0:
        return

    y_actual = np.array([d["non_idle_cycles"] for d in test_set])
    errors = (
        y_actual - y_pred
    )  # signed: positive = under-predicted, negative = over-predicted
    abs_errors = np.abs(errors)
    top_indices = np.argsort(abs_errors)[::-1][:top_n]

    table = Table(
        title=f"[bold]Top {top_n} Highest-Error Samples - {group_name}[/bold]",
        box=box.ROUNDED,
        border_style="red",
    )
    table.add_column("Rank", justify="right", style="dim")
    table.add_column("snapshot_id", style="cyan")
    table.add_column("core", justify="right")
    table.add_column("sys_cycles", justify="right")
    table.add_column("interval", justify="right")
    table.add_column("actual (ns)", justify="right", style="green")
    table.add_column("predicted (ns)", justify="right", style="yellow")
    table.add_column("error (ns)\nactual-predicted", justify="right", style="red")

    for rank, idx in enumerate(top_indices, 1):
        d = test_set[idx]
        table.add_row(
            str(rank),
            str(d["snapshot_id"]),
            str(d["core"]),
            str(d["sys_cycles"]),
            str(d["interval"]),
            f"{d['non_idle_cycles']:.2f}",
            f"{y_pred[idx]:.2f}",
            f"{errors[idx]:+.2f}",
        )

    console.print(table)


def instruction_weights(samples: list[dict]) -> np.ndarray:
    """Compute per-sample instruction-count weights (bx_instruction)."""
    return np.array([s["bx_instruction"] for s in samples], dtype=float)


def train_and_evaluate_group(
    diffs: list[dict],
    test_ratio: float,
    random_seed: int,
    group_name: str,
    min_instruction_coeff: float = 0.0,
    no_noc: bool = False,
) -> tuple[np.ndarray, dict, dict, np.ndarray, np.ndarray, list[dict]] | None:
    """
    Train NNLS model for a group and evaluate on train/test sets.

    Returns None if there is not enough data, otherwise:
        P: Trained coefficients
        train_metrics: Training metrics
        test_metrics: Test metrics
        X_test: Test feature matrix
        y_test: Test target vector
        test_set: Raw test dicts (for error inspection)
    """
    if len(diffs) == 0:
        console.print(f"[red]Error: No data for {group_name}[/red]")
        return None

    train_set, test_set = split_data(diffs, test_ratio, random_seed)

    console.print(
        f"[cyan]{group_name}: {len(diffs)} sampling units (aggregated across cores) ({len(train_set)} train, {len(test_set)} test)[/cyan]"
    )

    if len(train_set) == 0 or len(test_set) == 0:
        console.print(f"[red]Error: One of the splits is empty for {group_name}[/red]")
        return None

    X_train, y_train = extract_features(train_set)
    X_test, y_test = extract_features(test_set)
    w_train = instruction_weights(train_set)
    w_test = instruction_weights(test_set)

    P = fit_nnls_no_intercept(X_train, y_train, min_instruction_coeff, no_noc)

    train_metrics = evaluate_model(X_train, y_train, P, weights=w_train)
    test_metrics = evaluate_model(X_test, y_test, P, weights=w_test)

    return (
        P,
        train_metrics,
        test_metrics,
        X_test,
        y_test,
        test_set,
    )


def main():
    parser = argparse.ArgumentParser(
        description="Train IPC model using OLS on collected statistics."
    )
    parser.add_argument(
        "csv_file", type=str, help="Path to the CSV file output by collect.py"
    )
    parser.add_argument(
        "interval_index",
        type=int,
        help="Index of the sampling interval to use (0 = first interval, 1 = diff between second and first, etc.)",
    )
    parser.add_argument(
        "--test-ratio",
        type=float,
        default=0.5,
        help="Ratio of data to use for testing (default: 0.5)",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="Random seed for reproducibility (default: 42)",
    )
    parser.add_argument(
        "--output",
        type=str,
        default="model_coefficients.json",
        help="Output file for model coefficients in JSON format (default: model_coefficients.json)",
    )
    parser.add_argument(
        "-g",
        "--core-groups",
        type=str,
        default=None,
        help='Core groups specification (e.g., "0-7,8-15" or "0,1,2,3-5"). If not specified, all cores are trained as a single group.',
    )
    parser.add_argument(
        "--core-info-output",
        type=str,
        default=None,
        help="Output file for core_info.csv (default: same directory as output, named core_info.csv)",
    )
    parser.add_argument(
        "-c",
        "--core-info",
        type=str,
        default=None,
        help="Path to existing core_info.csv file. If provided, only cores present in training data will be updated, others will be preserved.",
    )
    parser.add_argument(
        "--min-instruction-coeff",
        type=float,
        default=0.05,
        help="Minimum value for the bx_instruction coefficient (default: 0.05, i.e. at most 20 "
        "instructions per nanosecond). Set to 0.0 to disable and use standard NNLS. "
        "Uses lsq_linear instead of nnls when non-zero.",
    )
    parser.add_argument(
        "--no-noc",
        action="store_true",
        default=False,
        help="Force bx_read_noc_hop, bx_write_noc_hop, and bx_ifetch_noc_hop coefficients to zero.",
    )
    parser.add_argument(
        "--min-non-idle-cycles",
        type=float,
        default=0.0,
        help="Minimum non_idle_cycles threshold (in nanoseconds) for filtering samples after "
        "compute_differences. Samples with non_idle_cycles below this value are discarded. "
        "Default: 0.0 (disabled, no filtering).",
    )
    parser.add_argument(
        "--error-top-n",
        type=int,
        default=10,
        help="Number of top error samples to display in the report (default: 10)",
    )

    args = parser.parse_args()

    # Validate arguments
    if args.test_ratio < 0 or args.test_ratio > 1:
        parser.error("--test-ratio must be between 0 and 1")

    if args.interval_index < 0:
        parser.error("interval_index must be non-negative")

    # Parse core groups
    core_groups = None
    if args.core_groups:
        try:
            core_groups = parse_core_groups(args.core_groups)
            console.print(f"[green]Parsed core groups: {core_groups}[/green]")
        except Exception as e:
            console.print(f"[red]Error parsing core groups: {e}[/red]")
            return 1

    # Determine core_info output path
    if args.core_info_output:
        core_info_path = args.core_info_output
    else:
        # Default to same directory as output file, named "core_info.csv"
        output_dir = os.path.dirname(args.output) or "."
        core_info_path = os.path.join(output_dir, "core_info_ipc_model_new.csv")

    console.print(f"\n[bold green]Loading data from {args.csv_file}...[/bold green]")
    data = load_data(args.csv_file)
    console.print(f"[green]Loaded {len(data)} total records[/green]")

    # Get all unique cores from data
    all_cores = sorted(set(row["core"] for row in data))
    console.print(f"[green]Found {len(all_cores)} unique cores: {all_cores}[/green]")

    # Parse existing core_info.csv if provided
    original_core_data = None
    if args.core_info:
        console.print(
            f"\n[bold green]Reading existing core_info.csv from {args.core_info}...[/bold green]"
        )
        original_core_data = parse_existing_core_info(args.core_info)
        console.print(
            f"[green]Loaded {len(original_core_data)} cores from existing file[/green]"
        )

        # Validate all trained cores exist in original
        trained_core_set = set(all_cores)
        original_core_set = set(original_core_data.keys())
        missing_cores = trained_core_set - original_core_set

        if missing_cores:
            console.print(
                f"[red]Error: Cores in training data not found in original core_info.csv: {sorted(missing_cores)}[/red]"
            )
            console.print(
                f"[red]Cannot proceed - all training cores must exist in the original file[/red]"
            )
            sys.exit(1)

        # Create backup if output file exists
        if os.path.exists(core_info_path):
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            backup_path = f"{core_info_path}.{timestamp}"
            try:
                shutil.copy2(core_info_path, backup_path)
                console.print(
                    f"[yellow]Backed up existing file to: {backup_path}[/yellow]"
                )
            except Exception as e:
                console.print(f"[red]Warning: Could not create backup: {e}[/red]")

    # FEATURE_NAMES at module level controls which features are used for training
    # Store group results for aggregated statistics
    group_results: list[dict[str, Any]] = []
    group_coefficients: list[np.ndarray] = []
    all_test_X = []
    all_test_y = []
    all_test_predictions = []
    all_test_weights = []

    if core_groups:
        # Train one model per group
        console.print(
            f"\n[bold cyan]Training models for {len(core_groups)} core groups...[/bold cyan]"
        )

        for group_idx, cores in enumerate(core_groups):
            group_name = f"Group {group_idx + 1} (cores {cores})"
            console.print(f"\n[bold yellow]=== {group_name} ===[/bold yellow]")

            # Filter valid cores
            valid_cores = [c for c in cores if c in all_cores]
            if not valid_cores:
                console.print(f"[red]Warning: No valid cores in {group_name}[/red]")
                continue

            # Compute differences for this group
            diffs = compute_differences(data, args.interval_index, valid_cores)

            # Filter low non-idle-cycle samples
            if args.min_non_idle_cycles > 0.0:
                before = len(diffs)
                diffs = [
                    d for d in diffs if d["non_idle_cycles"] >= args.min_non_idle_cycles
                ]
                after = len(diffs)
                if before != after:
                    console.print(
                        f"[yellow]Filtered {before - after} samples with non_idle_cycles < "
                        f"{args.min_non_idle_cycles:.1f} ns ({after} remaining)[/yellow]"
                    )

            # Train and evaluate
            result = train_and_evaluate_group(
                diffs,
                args.test_ratio,
                args.seed,
                group_name,
                args.min_instruction_coeff,
                args.no_noc,
            )

            if result is None:
                continue

            (
                P,
                train_metrics,
                test_metrics,
                X_test,
                y_test,
                test_set,
            ) = result
            group_coefficients.append(P)

            # Store for aggregated statistics
            if X_test is not None and y_test is not None:
                all_test_X.append(X_test)
                all_test_y.append(y_test)
                all_test_predictions.append(X_test @ P)
                all_test_weights.append(instruction_weights(test_set))

            # Display coefficients
            coef_table = Table(
                title=f"[bold]NNLS Model Coefficients - {group_name}[/bold]",
                box=box.ROUNDED,
            )
            coef_table.add_column("Feature", style="cyan", no_wrap=True)
            coef_table.add_column("Coefficient", justify="right", style="green")
            for name, coeff in zip(FEATURE_NAMES, P):
                coef_table.add_row(name, f"{coeff:>15.6f}")
            console.print(coef_table)

            # Display metrics
            if train_metrics and test_metrics:
                console.print("\n")
                train_table = create_metrics_table(
                    train_metrics, f"NNLS Model - {group_name} Training", "blue"
                )
                test_table = create_metrics_table(
                    test_metrics, f"NNLS Model - {group_name} Test", "green"
                )
                console.print(Columns([train_table, test_table]))

                group_results.append(
                    {
                        "group": group_name,
                        "key": f"group_{group_idx}",
                        "cores": valid_cores,
                        "asid": None,
                        "sample_count": len(diffs),
                        "coefficients": P,
                        "train_metrics": train_metrics,
                        "test_metrics": test_metrics,
                    }
                )

            # Display top error samples for debugging
            if test_set is not None and X_test is not None:
                print_top_error_samples(
                    test_set, X_test @ P, group_name, args.error_top_n
                )
    else:
        # Single model for all cores
        console.print("\n[bold cyan]Training single model for all cores...[/bold cyan]")

        diffs = compute_differences(data, args.interval_index)
        console.print(
            f"[cyan]Generated {len(diffs)} sampling units (aggregated across cores)[/cyan]"
        )

        # Filter low non-idle-cycle samples
        if args.min_non_idle_cycles > 0.0:
            before = len(diffs)
            diffs = [
                d for d in diffs if d["non_idle_cycles"] >= args.min_non_idle_cycles
            ]
            after = len(diffs)
            if before != after:
                console.print(
                    f"[yellow]Filtered {before - after} samples with non_idle_cycles < "
                    f"{args.min_non_idle_cycles:.1f} ns ({after} remaining)[/yellow]"
                )

        result = train_and_evaluate_group(
            diffs,
            args.test_ratio,
            args.seed,
            "All Cores",
            args.min_instruction_coeff,
            args.no_noc,
        )

        if result is None:
            return 1

        (
            P,
            train_metrics,
            test_metrics,
            X_test,
            y_test,
            test_set,
        ) = result

        group_coefficients.append(P)

        # Store for aggregated statistics
        if X_test is not None and y_test is not None:
            all_test_X.append(X_test)
            all_test_y.append(y_test)
            all_test_predictions.append(X_test @ P)

        # Display coefficients
        coef_table = Table(
            title="[bold]NNLS Model Coefficients[/bold]", box=box.ROUNDED
        )
        coef_table.add_column("Feature", style="cyan", no_wrap=True)
        coef_table.add_column("Coefficient", justify="right", style="green")
        for name, coeff in zip(FEATURE_NAMES, P):
            coef_table.add_row(name, f"{coeff:>15.6f}")
        console.print(coef_table)

        # Display metrics
        if train_metrics and test_metrics:
            console.print("\n")
            train_table = create_metrics_table(
                train_metrics, "NNLS Model - Training Set", "blue"
            )
            test_table = create_metrics_table(
                test_metrics, "NNLS Model - Test Set", "green"
            )
            console.print(Columns([train_table, test_table]))

            group_results.append(
                {
                    "group": "All Cores",
                    "key": "all_cores",
                    "cores": sorted(all_cores) if all_cores else None,
                    "asid": None,
                    "sample_count": len(diffs),
                    "coefficients": P,
                    "train_metrics": train_metrics,
                    "test_metrics": test_metrics,
                }
            )

        # Display top error samples for debugging
        if test_set is not None and X_test is not None:
            print_top_error_samples(test_set, X_test @ P, "All Cores", args.error_top_n)

    # Compute aggregated statistics
    if all_test_X and all_test_y and all_test_predictions:
        console.print(
            "\n[bold cyan]=== Aggregated Statistics (All Groups) ===[/bold cyan]"
        )

        combined_X = np.vstack(all_test_X)
        combined_y = np.concatenate(all_test_y)
        combined_pred = np.concatenate(all_test_predictions)
        combined_weights = np.concatenate(all_test_weights) if all_test_weights else None

        residuals = combined_y - combined_pred
        mask = combined_y != 0
        mape = (
            np.mean(np.abs(residuals[mask] / combined_y[mask])) * 100
            if np.sum(mask) > 0 else float("inf")
        )
        mpe = (
            np.mean((combined_pred[mask] - combined_y[mask]) / combined_y[mask]) * 100
            if np.sum(mask) > 0 else float("inf")
        )
        if combined_weights is not None and np.sum(combined_weights[mask]) > 0:
            w = combined_weights[mask]
            w_norm = w / w.sum()
            weighted_mape = float(np.sum(w_norm * np.abs(residuals[mask] / combined_y[mask])) * 100)
            weighted_mpe = float(np.sum(w_norm * (combined_pred[mask] - combined_y[mask]) / combined_y[mask]) * 100)
        else:
            weighted_mape = mape
            weighted_mpe = mpe
        aggregated_metrics = {
            "mae": np.mean(np.abs(residuals)),
            "me": np.mean(residuals),
            "mse": np.mean(residuals**2),
            "rmse": np.sqrt(np.mean(residuals**2)),
            "mape": mape,
            "mpe": mpe,
            "weighted_mape": weighted_mape,
            "weighted_mpe": weighted_mpe,
            "residuals": residuals,
        }

        agg_table = create_metrics_table(
            aggregated_metrics, "Aggregated NNLS Model - Test Set", "magenta"
        )
        console.print(agg_table)

    # Generate core_info.csv
    if group_coefficients:
        trained_cores = set(all_cores) if all_cores else None
        generate_core_info_csv(
            core_info_path,
            core_groups,
            group_coefficients,
            all_cores,
            original_core_data,
            trained_cores,
        )

    # Save coefficients to JSON file
    json_data = {
        "format_version": 1,
        "source": "training_model",
        "freq_ghz": FREQ,
        "FEATURE_NAMES": FEATURE_NAMES,
        "interval_index": args.interval_index,
        "random_seed": args.seed,
        "test_ratio": args.test_ratio,
        "min_non_idle_cycles": args.min_non_idle_cycles,
        "models": [
            {
                "key": r["key"],
                "asid": r["asid"],
                "cores": r["cores"],
                "sample_count": r["sample_count"],
                "coefficients": {
                    name: float(coeff)
                    for name, coeff in zip(FEATURE_NAMES, r["coefficients"])
                },
                "train_metrics": {
                    k: float(np.asarray(v).flat[0])
                    for k, v in r["train_metrics"].items()
                },
                "test_metrics": {
                    k: float(np.asarray(v).flat[0])
                    for k, v in r["test_metrics"].items()
                },
            }
            for r in group_results
        ],
    }
    with open(args.output, "w") as f:
        json.dump(json_data, f, indent=2)
    console.print(f"[bold green]Model saved to: {args.output}[/bold green]")

    return 0


if __name__ == "__main__":
    exit(main())
