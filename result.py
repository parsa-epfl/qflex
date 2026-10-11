#!/usr/bin/env python3
"""
Analyze the sampling error of the detailed simulation.

Reads the timing.csv produced by collect.py, takes one sampling unit of every snapshot, and
for each core group reports the U-IPC statistics and the sample size needed to meet the
sampling error bound. The suggested sample size for the next iteration is written to
./NEXT_SAMPLE_SIZE.

Exit code: 0 if every group meets the bound, -1 (255) if not, 1 on invalid input.
"""

import argparse
import math
import sys

import numpy as np
import pandas as pd
import plotille
from rich import box
from rich.console import Console
from rich.panel import Panel
from rich.table import Table
from rich.text import Text

INTERVAL = 100000
FREQ_GHZ = 2

# Unknown confidence levels fall back to 95%.
Z_SCORES = {68.0: 1.0, 80.0: 1.28, 85.0: 1.44, 90.0: 1.645, 95.0: 1.96, 98.0: 2.33, 99.0: 2.576, 99.9: 3.291}

console = Console()


def parse_core_groups(core_groups_str: str) -> list[list[int]]:
    """Parse e.g. "0-7,8-15" or "0,1,2,3-5" into a list of core lists."""
    groups = []
    for group_str in core_groups_str.split(","):
        if "-" in group_str:
            start, end = map(int, group_str.split("-"))
            groups.append(list(range(start, end + 1)))
        else:
            groups.append([int(group_str)])
    return groups


def load_sampling_unit(csv_path: str, unit_size: int, index: int) -> tuple[np.ndarray, np.ndarray]:
    """
    Returns the user instructions and halted cycles of sampling unit `index` of every snapshot,
    both of shape [snapshot, core]. A sampling unit spans `unit_size` measurement intervals.
    """
    df = pd.read_csv(csv_path, usecols=["snapshot_id", "core", "sys_cycles", "instruction:u", "halted_cycles"])
    # ASID-change boundary rows land at non-multiples of INTERVAL; keep the regular measurement points.
    df = df[df["sys_cycles"] % INTERVAL == 0]
    if df.empty:
        console.print("[red]Error: No regular measurement rows found in timing.csv after filtering[/red]")
        sys.exit(1)

    snapshot_idx, snapshot_ids = pd.factorize(df["snapshot_id"])
    point_idx = (df["sys_cycles"] // INTERVAL - 1).to_numpy()
    core_idx = df["core"].to_numpy()
    shape = (len(snapshot_ids), point_idx.max() + 1, core_idx.max() + 1)
    console.print(f"[green]Snapshots: {shape[0]}, cores: {shape[2]}, measurement points: {shape[1]}[/green]")

    def unit_delta(column: str) -> np.ndarray:
        # Counters are cumulative: sample every unit_size-th point, then take per-unit differences.
        cumulative = np.zeros(shape)
        cumulative[snapshot_idx, point_idx, core_idx] = df[column].to_numpy()
        cumulative = cumulative[:, ::unit_size, :]
        if index >= cumulative.shape[1]:
            console.print(f"[red]Error: Index {index} is out of bounds. Maximum index is {cumulative.shape[1] - 1}[/red]")
            sys.exit(1)
        return cumulative[:, index, :] - (cumulative[:, index - 1, :] if index > 0 else 0)

    return unit_delta("instruction:u"), unit_delta("halted_cycles")


def weighted_harmonic_ipc(instructions_u: np.ndarray, halted_cycles: np.ndarray, total_cycles: float, cores: list[int]) -> float:
    """
    Sum over cores of the harmonic mean of the non-idle IPC across snapshots, where each snapshot
    is weighted by its non-idle share and the idle share counts as an IPC of 1.
    """
    total = 0.0
    for core in cores:
        ipcs, ipc_weights, idle_weight = [], [], 0.0
        for instructions, halted in zip(instructions_u[:, core], halted_cycles[:, core]):
            if halted >= total_cycles:
                idle_weight += 1.0
            else:
                ipcs.append(instructions / (total_cycles - halted))
                ipc_weights.append((total_cycles - halted) / total_cycles)
                idle_weight += halted / total_cycles
        total_weight = sum(ipc_weights) + idle_weight
        ipc_weights = [w / total_weight for w in ipc_weights]
        denominator = idle_weight / total_weight
        for ipc, weight in zip(ipcs, ipc_weights):
            if ipc != 0:
                denominator += weight / ipc
        if denominator != 0:
            total += sum(ipc_weights) / denominator
    return total


def plot_distribution(values: np.ndarray, title: str) -> None:
    console.print(f"\n[bold cyan]{title}:[/bold cyan]")
    fig = plotille.Figure()
    fig.width = 80
    fig.height = 20
    fig.color_mode = "byte"
    fig.x_label = "U-IPC"
    fig.y_label = "Frequency"
    fig.set_x_limits(min_=0, max_=max(values) * 1.1)
    fig.histogram(values, bins=20, lc=100)
    print("Distribution of U-IPC Values")
    print(fig.show())


def next_prime(n: int) -> int:
    candidate = max(n, 2)
    while any(candidate % i == 0 for i in range(2, int(candidate**0.5) + 1)):
        candidate += 1
    return candidate


def main():
    parser = argparse.ArgumentParser(
        description="Analyze the sampling error of the detailed simulation from timing.csv",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python result.py --unit-size 2 --index 1
  python result.py --core-groups "0-7,8-15" --confidence 99 --error 0.02 --no-plot
        """,
    )
    parser.add_argument("--timing-csv", default="./timing.csv", help="Path to timing.csv produced by collect.py (default: ./timing.csv)")
    parser.add_argument("-u", "--unit-size", type=int, default=1, help="Size of sampling unit in terms of INTERVAL (default: 1)")
    parser.add_argument("-i", "--index", type=int, default=2, help="Index of the sampling unit to analyze (default: 2)")
    parser.add_argument("-g", "--core-groups", help='Core groups to analyze separately (e.g., "0-7,8-15" or "0,1,2,3-5"). If not specified, all cores are analyzed as one group.')
    parser.add_argument("--confidence", type=float, default=95.0, help="Confidence level for sample size calculation (default: 95.0)")
    parser.add_argument("--error", type=float, default=0.05, help="Acceptable sampling error (default: 0.05)")
    parser.add_argument("--plot", action=argparse.BooleanOptionalAction, default=True, help="Plot the U-IPC distribution (default: True)")
    args = parser.parse_args()

    console.print(Panel.fit("[bold green]Measurement Data Analysis Tool[/bold green]", border_style="green"))
    console.print(f"[cyan]Parsing measurement data from {args.timing_csv}...[/cyan]")
    instructions_u, halted_cycles = load_sampling_unit(args.timing_csv, args.unit_size, args.index)
    console.print(f"[green]Analyzing sampling unit {args.index} ({args.unit_size} x {INTERVAL} cycles)[/green]")

    total_cycles = INTERVAL * args.unit_size * FREQ_GHZ
    u_ipc = instructions_u / total_cycles  # [snapshot, core]
    if (u_ipc < 0).any():
        console.print(f"[red]Error: Found negative U-IPC values. Please check the data for sampling unit index {args.index}.[/red]")
        sys.exit(1)

    num_cores = u_ipc.shape[1]
    z_score = Z_SCORES.get(args.confidence, 1.96)
    if args.core_groups:
        groups = parse_core_groups(args.core_groups)
        console.print(f"\n[bold cyan]Analyzing {len(groups)} core groups separately:[/bold cyan]")
    else:
        groups = [list(range(num_cores))]
        console.print("\n[bold cyan]Analyzing all cores as a single group:[/bold cyan]")

    group_results = []
    for group_idx, group in enumerate(groups):
        if args.core_groups:
            console.print(f"\n[yellow]Core Group {group_idx + 1}: cores {group}[/yellow]")
        cores = [core for core in group if 0 <= core < num_cores]
        invalid_cores = [core for core in group if core not in cores]
        if invalid_cores:
            console.print(f"[red]Warning: cores {invalid_cores} not found in data (max core index: {num_cores - 1})[/red]")
        if not cores:
            console.print("[red]Error: No valid cores found in this group.[/red]")
            continue

        # Each snapshot is one sample: the U-IPC summed over the cores of the group.
        samples = u_ipc[:, cores].sum(axis=1)
        average = np.mean(samples)
        std_dev = np.std(samples)
        coefficient_of_variation = std_dev / average
        required_sample_size = (z_score * coefficient_of_variation / args.error) ** 2
        is_sample_size_enough = len(samples) >= required_sample_size
        group_results.append((required_sample_size, len(samples), is_sample_size_enough))

        table = Table(title=f"Core Group {group_idx + 1} Statistics" if args.core_groups else "All Cores Statistics", box=box.ROUNDED)
        table.add_column("Metric", style="cyan")
        table.add_column("Value", style="green")
        if args.core_groups:
            table.add_row("Valid cores used", str(cores))
        table.add_row("Valid snapshots", str(len(samples)))
        table.add_row("Average U-IPC", f"{average:.4f}")
        table.add_row("Weighted Harmonic IPC", f"{weighted_harmonic_ipc(instructions_u, halted_cycles, total_cycles, cores):.4f}")
        table.add_row("Standard deviation", f"{std_dev:.4f}")
        table.add_row("Coefficient of variation", f"{coefficient_of_variation:.4f}")
        table.add_row("Required sample size", f"{required_sample_size:.1f}")
        table.add_row("Sample size adequate", Text("✓ Yes", style="green") if is_sample_size_enough else Text("✗ No", style="red"))
        console.print(table)
        if not is_sample_size_enough:
            console.print(f"[red]Need {required_sample_size - len(samples):.1f} more sample units for adequate sample size[/red]")
        if args.plot and args.core_groups:
            plot_distribution(samples, f"U-IPC Distribution - Core Group {group_idx + 1}")

    all_bounds_satisfied = all(enough for _, _, enough in group_results)
    if group_results:
        # Never shrink the sample size, to avoid oscillating between iterations.
        effective_size = max(max(required for required, _, _ in group_results), max(current for _, current, _ in group_results))
        next_sample_size = next_prime(math.ceil(effective_size))
        with open("./NEXT_SAMPLE_SIZE", "w") as f:
            f.write(str(next_sample_size))
        console.print(f"[green]Next sample size written to ./NEXT_SAMPLE_SIZE: {next_sample_size}[/green]")

    if args.plot:
        console.print("\n[bold cyan]=== Overall U-IPC Distribution (All Cores) ===[/bold cyan]")
        plot_distribution(u_ipc.sum(axis=1), "U-IPC Distribution")

    if not all_bounds_satisfied:
        console.print("[red]Error: Not all groups satisfy the sampling error bound. Exiting with -1.[/red]")
        sys.exit(-1)


if __name__ == "__main__":
    main()
