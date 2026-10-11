#!/usr/bin/env python3
"""
training_by_asid.py - Train IPC model per ASID using non-negative least squares.

This script reads the timing.csv produced by collect.py and trains non-negative
least squares models for each unique ASID.

Flow:
1. Load timing.csv and filter to the given cycle range
2. For each (snapshot_id, core) group, build moment records by accumulating
   delta bx_* metrics across consecutive same-ASID intervals
3. Train NNLS model per ASID (minimum 30 samples)
4. Export coefficients and metrics to CSV
"""

import argparse
import json
import os
import random
import sys
from typing import Dict, List, Tuple, Optional
import numpy as np
import pandas as pd
from scipy.optimize import nnls, lsq_linear
from rich.console import Console
from rich.table import Table
from rich import box

console = Console()

FREQ = 2  # GHz - frequency for converting cycles to nanoseconds
MIN_SAMPLES = 30  # Minimum samples per ASID for training

# Feature names (must match CSV column names)
FEATURE_NAMES = [
    "bx_private_icache_miss",
    "bx_private_dcache_miss",
    "bx_shared_cache_miss",
    "bx_bp_miss",
    "bx_drain_store_buffer",
    "bx_instruction"
]


def load_and_build_records(
    csv_path: str, cycle_start: int, cycle_end: int, min_non_idle: float = 0.0
) -> List[Dict]:
    """
    Load timing.csv and build moment records grouped by (snapshot_id, core, asid).

    For each (snapshot_id, core) group, iterates over consecutive row pairs.
    Delta bx_* metrics are accumulated while the ASID stays the same. When the
    ASID changes (or at the end of the group), a record is finalised with the
    accumulated deltas and the non_idle_cycles computed from delta core/halted cycles.

    Records whose non_idle_cycles < min_non_idle are discarded. This filters out
    very short moments (e.g. ASID-change boundaries at the edge of the cycle range)
    that carry too little signal and can degrade regression quality.

    Returns: List of records with keys:
        snapshot_idx, asid, core_id, non_idle_cycles, <FEATURE_NAMES>
    """
    df = pd.read_csv(csv_path)

    # Validate all required columns are present before processing
    required_columns = set(FEATURE_NAMES) | {
        "snapshot_id", "core", "sys_cycles", "asid", "core_cycles", "halted_cycles",
    }
    missing = required_columns - set(df.columns)
    if missing:
        console.print(
            f"[red]Error: CSV is missing required columns: {sorted(missing)}[/red]"
        )
        sys.exit(1)

    # Filter to cycle range
    df = df[(df["sys_cycles"] >= cycle_start) & (df["sys_cycles"] <= cycle_end)]

    # Skip rows where asid is unknown
    df = df[df["asid"] != -1]

    # Sort for sequential processing within each (snapshot_id, core) group
    df = df.sort_values(["snapshot_id", "core", "sys_cycles"])

    all_records = []

    for (snapshot_id, core_id), group in df.groupby(
        ["snapshot_id", "core"], sort=False
    ):
        rows = group.reset_index(drop=True)

        if len(rows) < 2:
            continue

        current_asid = None
        current_metrics = None
        current_core_cycles_start = 0
        current_halted_cycles_start = 0

        for i in range(len(rows) - 1):
            row_start = rows.iloc[i]
            row_end = rows.iloc[i + 1]

            asid_start = row_start["asid"]
            asid_end = row_end["asid"]

            # Initialise on first iteration
            if i == 0:
                current_asid = asid_start
                current_metrics = {k: 0 for k in FEATURE_NAMES}
                current_core_cycles_start = row_start["core_cycles"]
                current_halted_cycles_start = row_start["halted_cycles"]

            # Accumulate delta bx_* metrics for this interval into the current ASID's bucket
            for feature in FEATURE_NAMES:
                current_metrics[feature] += int(row_end[feature]) - int(
                    row_start[feature]
                )

            # Check if ASID changes at the end of this interval
            if asid_end != current_asid:
                # Finalise the current moment
                delta_core = int(row_end["core_cycles"]) - int(
                    current_core_cycles_start
                )
                delta_halted = int(row_end["halted_cycles"]) - int(
                    current_halted_cycles_start
                )
                non_idle = (delta_core - delta_halted) / FREQ

                if non_idle >= min_non_idle:
                    record = {
                        "snapshot_idx": snapshot_id,
                        "asid": current_asid,
                        "core_id": core_id,
                        "non_idle_cycles": non_idle,
                    }
                    record.update(current_metrics)
                    all_records.append(record)

                # Start a new moment for the new ASID
                current_asid = asid_end
                current_metrics = {k: 0 for k in FEATURE_NAMES}
                current_core_cycles_start = row_end["core_cycles"]
                current_halted_cycles_start = row_end["halted_cycles"]

        # Finalise the last moment for this (snapshot_id, core)
        if current_metrics is not None:
            last_row = rows.iloc[-1]
            delta_core = int(last_row["core_cycles"]) - int(current_core_cycles_start)
            delta_halted = int(last_row["halted_cycles"]) - int(
                current_halted_cycles_start
            )
            non_idle = (delta_core - delta_halted) / FREQ

            if non_idle >= min_non_idle:
                record = {
                    "snapshot_idx": snapshot_id,
                    "asid": current_asid,
                    "core_id": core_id,
                    "non_idle_cycles": non_idle,
                }
                record.update(current_metrics)
                all_records.append(record)

    return all_records


def extract_features_and_target(records: List[Dict]) -> Tuple[np.ndarray, np.ndarray]:
    """
    Extract feature matrix X and target vector y from records.

    X: (n_samples, 9) matrix of bx_* features
    y: (n_samples,) vector of non_idle_cycles
    """
    n_samples = len(records)
    X = np.zeros((n_samples, len(FEATURE_NAMES)))
    y = np.zeros(n_samples)

    for i, record in enumerate(records):
        y[i] = record["non_idle_cycles"]
        for j, feature in enumerate(FEATURE_NAMES):
            X[i, j] = record[feature]

    return X, y


def split_data(
    records: List[Dict], test_ratio: float, random_seed: int
) -> Tuple[List[Dict], List[Dict]]:
    """Split records into training and test sets randomly."""
    random.seed(random_seed)
    shuffled = records.copy()
    random.shuffle(shuffled)

    test_size = int(len(shuffled) * test_ratio)
    test_set = shuffled[:test_size]
    train_set = shuffled[test_size:]

    return train_set, test_set


NOC_FEATURES = {"bx_read_noc_hop", "bx_write_noc_hop", "bx_ifetch_noc_hop"}


def fit_nnls_model(
    X: np.ndarray,
    y: np.ndarray,
    min_instruction_coeff: float = 0.0,
    no_noc: bool = False,
) -> np.ndarray:
    """
    Fit non-negative least squares model.

    Model: y = X @ P, subject to P >= 0 (and P[bx_instruction] >= min_instruction_coeff)
    Uses nnls when min_instruction_coeff == 0, otherwise uses lsq_linear with
    per-variable lower bounds.  When no_noc is True, NOC feature columns are excluded
    from the fit entirely and their coefficients are set to zero in the returned vector.
    Returns coefficients rounded to 3 decimal places.
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
    return np.maximum(np.round(P, 3), 0)


def instruction_weights(samples: list[dict]) -> np.ndarray:
    """Compute per-sample instruction-count weights (bx_instruction)."""
    return np.array([s["bx_instruction"] for s in samples], dtype=float)


def evaluate_model(X: np.ndarray, y: np.ndarray, P: np.ndarray, weights: np.ndarray | None = None) -> Dict:
    """Evaluate model performance."""
    y_pred = X @ P
    residuals = y - y_pred

    mae = np.mean(np.abs(residuals))
    me = np.mean(residuals)
    mse = np.mean(residuals**2)
    rmse = np.sqrt(mse)

    mask = y != 0
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
    }


def train_asid_model(
    records: List[Dict],
    test_ratio: float,
    random_seed: int,
    min_samples: int,
    min_instruction_coeff: float = 0.0,
    no_noc: bool = False,
) -> Optional[Tuple[np.ndarray, Dict, Dict]]:
    """
    Train NNLS model for a single ASID.

    Returns: (coefficients, train_metrics, test_metrics) or None if insufficient samples
    """
    if len(records) < min_samples:
        return None

    train_set, test_set = split_data(records, test_ratio, random_seed)

    if len(train_set) == 0 or len(test_set) == 0:
        return None

    X_train, y_train = extract_features_and_target(train_set)
    X_test, y_test = extract_features_and_target(test_set)
    w_train = instruction_weights(train_set)
    w_test = instruction_weights(test_set)

    P = fit_nnls_model(X_train, y_train, min_instruction_coeff, no_noc)

    train_metrics = evaluate_model(X_train, y_train, P, weights=w_train)
    test_metrics = evaluate_model(X_test, y_test, P, weights=w_test)

    return P, train_metrics, test_metrics


def main():
    parser = argparse.ArgumentParser(
        description="Train IPC models per ASID using non-negative least squares."
    )
    parser.add_argument(
        "csv_file", type=str, help="Path to timing.csv produced by collect.py"
    )
    parser.add_argument(
        "--cycle-range",
        type=str,
        required=True,
        help='Cycle range in format "START-END" (e.g., "100000-300000")',
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
        "--min-non-idle",
        type=float,
        default=0.0,
        help="Minimum non_idle_cycles for a moment record to be included in training. "
        "Filters out very short moments (e.g. ASID-change boundaries) that carry "
        "little signal. Use e.g. 50000 to match a half-interval threshold. "
        "(default: 0.0, i.e. only discard zero-activity moments)",
    )
    parser.add_argument(
        "--min-samples",
        type=int,
        default=MIN_SAMPLES,
        help=f"Minimum samples per ASID for training (default: {MIN_SAMPLES})",
    )
    parser.add_argument(
        "--output",
        type=str,
        default="asid_models.json",
        help="Output JSON file path (default: asid_models.json)",
    )
    parser.add_argument(
        "--output-csv",
        type=str,
        default="asid_info.csv",
        help="Output CSV file path with per-ASID coefficients (default: asid_info.csv)",
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

    args = parser.parse_args()

    # Validate arguments
    try:
        cycle_start, cycle_end = map(int, args.cycle_range.split("-"))
    except ValueError:
        parser.error(
            "--cycle-range must be in format 'START-END' (e.g., '100000-300000')"
        )

    if cycle_start >= cycle_end:
        parser.error("Cycle range START must be less than END")

    if args.test_ratio < 0 or args.test_ratio > 1:
        parser.error("--test-ratio must be between 0 and 1")

    # Load timing.csv and build moment records
    console.print(f"[bold green]Loading {args.csv_file}...[/bold green]")
    if args.min_non_idle > 0:
        console.print(
            f"[dim]Filtering records with non_idle_cycles < {args.min_non_idle}[/dim]"
        )
    all_records = load_and_build_records(
        args.csv_file, cycle_start, cycle_end, args.min_non_idle
    )

    console.print(f"[green]Collected {len(all_records)} records total[/green]")

    if not all_records:
        console.print(
            "[red]Error: No records collected. Check cycle range and file availability.[/red]"
        )
        sys.exit(1)

    # Index records by ASID
    asid_records: Dict[int, List[Dict]] = {}
    for record in all_records:
        asid = record["asid"]
        if asid not in asid_records:
            asid_records[asid] = []
        asid_records[asid].append(record)

    console.print(f"[green]Found {len(asid_records)} unique ASIDs[/green]")

    # Train models per ASID
    results = []
    skipped_asids = []

    for asid in sorted(asid_records.keys()):
        records = asid_records[asid]

        if len(records) < args.min_samples:
            skipped_asids.append((asid, len(records)))
            continue

        result = train_asid_model(
            records,
            args.test_ratio,
            args.seed,
            args.min_samples,
            args.min_instruction_coeff,
            args.no_noc,
        )

        if result is None:
            skipped_asids.append((asid, len(records)))
            continue

        P, train_metrics, test_metrics = result

        results.append(
            {
                "asid": asid,
                "coefficients": P,
                "train_metrics": train_metrics,
                "test_metrics": test_metrics,
                "sample_count": len(records),
            }
        )

    # Report skipped ASIDs
    if skipped_asids:
        console.print(
            f"\n[yellow]Skipped {len(skipped_asids)} ASIDs with insufficient samples:[/yellow]"
        )
        for asid, count in skipped_asids:
            console.print(f"  ASID {asid}: {count} samples (min: {args.min_samples})")

    # Display results
    if results:
        console.print(
            f"\n[bold green]Trained {len(results)} models successfully[/bold green]"
        )

        for result in results:
            asid = result["asid"]

            table = Table(
                title=f"ASID {asid} (n={result['sample_count']})", box=box.ROUNDED
            )
            table.add_column("Feature", style="cyan")
            table.add_column("Coefficient", justify="right", style="green")

            for feature, coeff in zip(FEATURE_NAMES, result["coefficients"]):
                table.add_row(feature, f"{coeff:.3f}")

            console.print(table)

            metrics_table = Table(box=box.SIMPLE)
            metrics_table.add_column("Set", style="cyan")
            metrics_table.add_column("MAE", justify="right")
            metrics_table.add_column("RMSE", justify="right")
            metrics_table.add_column("MAPE", justify="right")
            metrics_table.add_column("MPE", justify="right")
            metrics_table.add_column("wMAPE", justify="right")
            metrics_table.add_column("wMPE", justify="right")

            metrics_table.add_row(
                "Train",
                f"{result['train_metrics']['mae']:.2f}",
                f"{result['train_metrics']['rmse']:.2f}",
                f"{result['train_metrics']['mape']:.2f}%",
                f"{result['train_metrics']['mpe']:.2f}%",
                f"{result['train_metrics']['weighted_mape']:.2f}%",
                f"{result['train_metrics']['weighted_mpe']:.2f}%",
            )
            metrics_table.add_row(
                "Test",
                f"{result['test_metrics']['mae']:.2f}",
                f"{result['test_metrics']['rmse']:.2f}",
                f"{result['test_metrics']['mape']:.2f}%",
                f"{result['test_metrics']['mpe']:.2f}%",
                f"{result['test_metrics']['weighted_mape']:.2f}%",
                f"{result['test_metrics']['weighted_mpe']:.2f}%",
            )

            console.print(metrics_table)
            console.print()
    else:
        console.print(
            "[red]Error: No models trained. All ASIDs had insufficient samples.[/red]"
        )
        sys.exit(1)

    # Save models to JSON file
    cycle_start_val, cycle_end_val = map(int, args.cycle_range.split("-"))
    json_data = {
        "format_version": 1,
        "source": "training_by_asid",
        "freq_ghz": FREQ,
        "feature_names": FEATURE_NAMES,
        "cycle_range_start": cycle_start_val,
        "cycle_range_end": cycle_end_val,
        "models": [
            {
                "key": f"asid_{r['asid']}",
                "asid": int(r["asid"]),
                "cores": None,
                "sample_count": r["sample_count"],
                "coefficients": {
                    name: float(coeff)
                    for name, coeff in zip(FEATURE_NAMES, r["coefficients"])
                },
                "train_metrics": {k: float(v) for k, v in r["train_metrics"].items()},
                "test_metrics": {k: float(v) for k, v in r["test_metrics"].items()},
            }
            for r in results
        ],
    }
    with open(args.output, "w") as f:
        json.dump(json_data, f, indent=2)
    console.print(f"[bold green]Model saved to: {args.output}[/bold green]")

    # Save coefficients as CSV
    coeff_columns = [f"{name}_coeff" for name in FEATURE_NAMES]
    csv_rows = []
    for model in json_data["models"]:
        row = {"asid": model["asid"]}
        for name in FEATURE_NAMES:
            row[f"{name}_coeff"] = model["coefficients"][name]
        csv_rows.append(row)
    csv_df = pd.DataFrame(csv_rows, columns=["asid"] + coeff_columns)
    csv_df.to_csv(args.output_csv, index=False)
    console.print(f"[bold green]Coefficients saved to: {args.output_csv}[/bold green]")

    return 0


if __name__ == "__main__":
    exit(main())
