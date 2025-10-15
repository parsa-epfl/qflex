#!/usr/bin/env python3

import pandas as pd
import numpy as np
import argparse
import math
import sys
import os
import shutil
from datetime import datetime
from pathlib import Path
import plotext as plt
from rich.console import Console
from rich.table import Table
from rich.panel import Panel
from rich.text import Text
from rich import box
from rich.align import Align

# Global constants
INTERVAL = 100000
END = 300000
CORE_COUNT = 64

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
    for group_str in core_groups_str.split(','):
        group_str = group_str.strip()
        if '-' in group_str:
            # Range specification like "0-7"
            start, end = map(int, group_str.split('-'))
            cores = list(range(start, end + 1))
        else:
            # Single core specification like "5"
            cores = [int(group_str)]
        groups.append(cores)
    
    return groups

def parse_direct_measurements(result_folders: list[str], diff_distance: int = 1) -> tuple[np.ndarray, np.ndarray]:
    """
    Parse measurement files directly without creating a CSV file.
    Based on collect.py logic but returns instruction and instruction_u arrays directly.
    
    Args:
        result_folders: List of result_* folder paths
        diff_distance: Distance for reducing cycles
    
    Returns:
        Tuple of (instructions, instructions_u) arrays with shape [snapshots, cycles, cores]
    """
    all_data = []
    
    for folder_name in result_folders:
        snapshot_id = folder_name.split("_")[-1].split(".")[0]
        
        for point in range(INTERVAL, END+1, INTERVAL):
            warm_instruction = [0] * CORE_COUNT
            warm_instruction_u = [0] * CORE_COUNT

            measurement_file = f"{folder_name}/all.measurement.{point:010}.log"
            if not os.path.exists(measurement_file):
                console.print(f"[red]Error: {measurement_file} does not exist[/red]")
                sys.exit(-1)

            with open(measurement_file) as f:
                for line in f:
                    if "-uarch-Commits" in line and not ":" in line:
                        # Extract core index, handling leading zeros
                        core_idx_str = line.split()[0].split("-")[0]
                        if core_idx_str == "000":
                            core_idx = 0
                        else:
                            core_idx = int(core_idx_str.lstrip("0"))

                        if core_idx >= CORE_COUNT:
                            continue

                        warm_instruction[core_idx] = int(line.split()[1])

                    if "-uarch-Commits:NonSpin:User" in line:
                        # Extract core index, handling leading zeros
                        core_idx_str = line.split()[0].split("-")[0]
                        if core_idx_str == "000":
                            core_idx = 0
                        else:
                            core_idx = int(core_idx_str.lstrip("0"))

                        if core_idx >= CORE_COUNT:
                            continue

                        warm_instruction_u[core_idx] = int(line.split()[1])

            # Store data for each core
            for core_id in range(CORE_COUNT):
                if warm_instruction[core_id] == 0:
                    continue
                
                all_data.append({
                    'snapshot_id': snapshot_id,
                    'core': core_id,
                    'cycle': point,
                    'instruction': warm_instruction[core_id],
                    'instruction_u': warm_instruction_u[core_id]
                })
    
    if not all_data:
        console.print("[red]Error: No valid measurement data found[/red]")
        sys.exit(-1)
    
    # Convert to DataFrame for easier processing
    df = pd.DataFrame(all_data)
    
    # Get unique values and create mappings
    snapshots = sorted(df["snapshot_id"].unique())
    cycles = sorted(df["cycle"].unique())
    cores = sorted(df[df["instruction"] != 0]["core"].unique())
    
    console.print(f"[green]Snapshots: {len(snapshots)}[/green]")
    console.print(f"[green]Core Count: {len(cores)}[/green]")
    console.print(f"[green]Cycle Count: {len(cycles)}[/green]")
    
    # Create mapping dictionaries for efficient lookup
    snapshot_to_idx = {snapshot: idx for idx, snapshot in enumerate(snapshots)}
    cycle_to_idx = {cycle: idx for idx, cycle in enumerate(cycles)}
    core_to_idx = {core: idx for idx, core in enumerate(cores)}
    
    # Initialize arrays
    instructions = np.zeros((len(snapshots), len(cycles), len(cores)))
    instructions_u = np.zeros((len(snapshots), len(cycles), len(cores)))
    
    # Process each row directly
    for _, row in df.iterrows():
        snapshot_id = row["snapshot_id"]
        cycle = row["cycle"]
        core = row["core"]
        instruction = row["instruction"]
        instruction_u = row["instruction_u"]
    
        # Skip rows with zero instructions or cores not in our valid cores list
        if instruction == 0 or core not in core_to_idx:
            continue
            
        # Get array indices
        snapshot_idx = snapshot_to_idx[snapshot_id]
        cycle_idx = cycle_to_idx[cycle]
        core_idx = core_to_idx[core]
        
        # Add values to arrays
        instructions[snapshot_idx, cycle_idx, core_idx] = instruction
        instructions_u[snapshot_idx, cycle_idx, core_idx] = instruction_u
    
    # Reduce cycles by diff_distance
    instructions = instructions[:, ::diff_distance, :]
    instructions_u = instructions_u[:, ::diff_distance, :]
    
    # Calculate differences
    instructions = np.diff(instructions, axis=1, prepend=np.zeros((instructions.shape[0], 1, instructions.shape[2])))
    instructions_u = np.diff(instructions_u, axis=1, prepend=np.zeros((instructions_u.shape[0], 1, instructions_u.shape[2])))
    
    return instructions, instructions_u

def read_core_info(core_info_path: str):
    """
    Read the core_info.csv and handle cases with or without affinity_core_idx.
    Returns a list of dictionaries with core information.
    """
    if not os.path.exists(core_info_path):
        console.print(f"[red]Error: core_info.csv not found at {core_info_path}[/red]")
        sys.exit(1)
    
    df = pd.read_csv(core_info_path)
    
    # Check if affinity_core_idx column exists
    has_affinity = 'affinity_core_idx' in df.columns
    
    core_info = []
    for _, row in df.iterrows():
        core_data = {'ipns': row['ipns']}
        if has_affinity:
            core_data['affinity_core_idx'] = row['affinity_core_idx']
        core_info.append(core_data)
    
    return core_info, has_affinity

def calculate_z_score(confidence: float) -> float:
    """
    Calculate Z-score for given confidence level.
    """
    # Common confidence levels to Z-scores mapping
    z_scores = {
        90.0: 1.645,
        95.0: 1.96,
        99.0: 2.576,
        99.9: 3.291
    }
    
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


def plot_u_ipc_distribution(result_folders: list[str], sampling_unit_size: int, index: int, plot_enabled: bool = True) -> dict:
    """
    Plot U-IPC distribution and return statistics for the specified sampling unit.
    
    Args:
        result_folders: List of result_* folder paths
        sampling_unit_size: Size of sampling unit in terms of INTERVAL
        index: Index of the sampling unit to analyze
        plot_enabled: Whether to display the plot
    
    Returns:
        Dictionary with U-IPC statistics
    """
    # Get instruction count data
    instruction_data, instruction_data_u = parse_direct_measurements(result_folders, sampling_unit_size)
    
    # Check if the index is valid
    if index >= instruction_data.shape[1]:
        console.print(f"[red]Error: Index {index} is out of bounds. Maximum index is {instruction_data.shape[1] - 1}[/red]")
        sys.exit(1)
    
    # Extract data for the specified interval index
    interval_instruction_data_u = instruction_data_u[:, index, :]  # Shape: [snapshots, cores]
    
    # Calculate IPC for each snapshot and core
    interval_ipc_data_u = interval_instruction_data_u / (INTERVAL * sampling_unit_size)
    
    # Calculate U-IPC statistics for sample size validation
    snapshot_total_u_ipc = np.sum(interval_ipc_data_u, axis=1)  # Sum across cores for each snapshot
    valid_u_ipc_data = [x for x in snapshot_total_u_ipc if not math.isnan(x) and x > 0]
    
    if len(valid_u_ipc_data) > 0:
        average_u_ipc = np.mean(valid_u_ipc_data)
        std_dev = np.std(valid_u_ipc_data)
        coefficient_of_variation = std_dev / average_u_ipc if average_u_ipc > 0 else float('inf')
        
        # Calculate required sample size for 95% confidence and 5% error (defaults)
        z_score = calculate_z_score(95.0)
        required_sample_size = (z_score * coefficient_of_variation / 0.05) ** 2
        current_sample_size = len(valid_u_ipc_data)
        is_sample_size_enough = current_sample_size >= required_sample_size
        
        # Plot distribution using plotext if enabled
        if plot_enabled:
            console.print("\n[bold cyan]U-IPC Distribution:[/bold cyan]")
            plt.clf()
            plt.plotsize(width=60, height=15)
            plt.hist(valid_u_ipc_data, bins=20)
            plt.title("Distribution of U-IPC Values")
            plt.xlabel("U-IPC")
            plt.ylabel("Frequency")
            plt.show()
        
        return {
            'valid_u_ipc_data': valid_u_ipc_data,
            'average_u_ipc': average_u_ipc,
            'coefficient_of_variation': coefficient_of_variation,
            'required_sample_size': required_sample_size,
            'current_sample_size': current_sample_size,
            'is_sample_size_enough': is_sample_size_enough
        }
    else:
        return {
            'valid_u_ipc_data': [],
            'average_u_ipc': 0,
            'coefficient_of_variation': float('inf'),
            'required_sample_size': float('inf'),
            'current_sample_size': 0,
            'is_sample_size_enough': False
        }


def generate_new_core_info(result_folders: list[str], old_core_info_path: str, sampling_unit_size: int, index: int, 
                          confidence: float | None = None, acceptable_sampling_error: float | None = None, output_path: str = "./core_info_new.csv"):
    """
    Generate new core_info.csv with updated IPC values from direct measurement data analysis.
    """
    # Get instruction count data
    instruction_data, instruction_data_u = parse_direct_measurements(result_folders, sampling_unit_size)
    
    # Check if the index is valid
    if index >= instruction_data.shape[1]:
        console.print(f"[red]Error: Index {index} is out of bounds. Maximum index is {instruction_data.shape[1] - 1}[/red]")
        sys.exit(1)
    
    # Extract data for the specified interval index
    interval_instruction_data = instruction_data[:, index, :]  # Shape: [snapshots, cores]
    interval_instruction_data_u = instruction_data_u[:, index, :]  # Shape: [snapshots, cores]
    
    # Calculate IPC for each snapshot and core
    interval_ipc_data = interval_instruction_data / (INTERVAL * sampling_unit_size)
    interval_ipc_data_u = interval_instruction_data_u / (INTERVAL * sampling_unit_size)
    
    # Average across snapshots for each core
    core_average_ipc = np.mean(interval_ipc_data, axis=0)  # Shape: [cores]
    
    # Filter out cores with zero or NaN IPC
    valid_core_ipc = {}
    for core_idx, ipc in enumerate(core_average_ipc):
        if not math.isnan(ipc) and ipc > 0:
            valid_core_ipc[core_idx] = ipc
    
    if len(valid_core_ipc) == 0:
        console.print("[red]Error: No valid per-core IPC data found for the specified sampling unit.[/red]")
        sys.exit(1)
    
    # Read old core_info to get structure
    old_core_info, has_affinity = read_core_info(old_core_info_path)
    
    # Prepare data for new core_info.csv and comparison
    new_core_data = []
    comparison_data = []
    
    cores_with_timing_data = 0
    cores_inherited = 0
    
    for i, old_core in enumerate(old_core_info):
        old_ipns = old_core['ipns']
        
        # Use per-core IPC if available, otherwise inherit from old core_info.csv
        if i in valid_core_ipc:
            # Multiply by 2 and round to 2 decimal places as per requirement
            new_ipns = round(2 * valid_core_ipc[i], 2)
            cores_with_timing_data += 1
            updated = True
        else:
            # If no timing data for this core, inherit from old core_info.csv
            new_ipns = old_ipns
            cores_inherited += 1
            updated = False
        
        # Add to new core data
        core_data = {'ipns': f"{new_ipns:.2f}"}
        if has_affinity:
            core_data['affinity_core_idx'] = str(int(old_core['affinity_core_idx']))
        new_core_data.append(core_data)
        
        # Store comparison data
        comparison_data.append({
            'core': i,
            'old_ipns': f"{old_ipns:.2f}",
            'new_ipns': f"{new_ipns:.2f}",
            'difference': f"{(new_ipns - old_ipns):.2f}",
            'updated': updated
        })
    
    # Create DataFrame and save to CSV
    new_df = pd.DataFrame(new_core_data)
    new_df.to_csv(output_path, index=False)
    
    # Get U-IPC statistics from the separate plotting function (without actually plotting)
    u_ipc_stats = plot_u_ipc_distribution(result_folders, sampling_unit_size, index, plot_enabled=False)
    
    # Set default values for optional parameters
    if confidence is None:
        confidence = 95.0
    if acceptable_sampling_error is None:
        acceptable_sampling_error = 0.05
    
    # Recalculate required sample size with user-specified confidence and error
    if u_ipc_stats['current_sample_size'] > 0:
        z_score = calculate_z_score(confidence)
        required_sample_size = (z_score * u_ipc_stats['coefficient_of_variation'] / acceptable_sampling_error) ** 2
        is_sample_size_enough = u_ipc_stats['current_sample_size'] >= required_sample_size
    else:
        required_sample_size = float('inf')
        is_sample_size_enough = False
    
    return {
        'comparison_data': comparison_data,
        'cores_with_timing_data': cores_with_timing_data,
        'cores_inherited': cores_inherited,
        'average_u_ipc': u_ipc_stats['average_u_ipc'],
        'coefficient_of_variation': u_ipc_stats['coefficient_of_variation'],
        'required_sample_size': required_sample_size,
        'current_sample_size': u_ipc_stats['current_sample_size'],
        'is_sample_size_enough': is_sample_size_enough,
        'confidence': confidence,
        'acceptable_sampling_error': acceptable_sampling_error
    }


def display_results_table(results: dict):
    """
    Display a formatted table showing the results using Rich library.
    """
    # Core comparison table
    table = Table(title="IPNS Comparison Table", box=box.ROUNDED)
    table.add_column("Core", justify="center", style="cyan")
    table.add_column("Old IPNS", justify="center", style="magenta")
    table.add_column("New IPNS", justify="center", style="green")
    table.add_column("Difference", justify="center", style="yellow")
    table.add_column("Updated", justify="center", style="blue")
    
    for data in results['comparison_data']:
        updated_str = "✓" if data['updated'] else "✗"
        style = "green" if data['updated'] else "red"
        table.add_row(
            str(data['core']), 
            data['old_ipns'], 
            data['new_ipns'], 
            data['difference'], 
            Text(updated_str, style=style)
        )
    
    console.print(table)
    
    # Summary statistics
    summary_table = Table(title="Summary Statistics", box=box.DOUBLE)
    summary_table.add_column("Metric", style="cyan")
    summary_table.add_column("Value", style="green")
    
    summary_table.add_row("Average U-IPC", f"{results['average_u_ipc']:.4f}")
    summary_table.add_row("Coefficient of Variation", f"{results['coefficient_of_variation']:.4f}")
    summary_table.add_row("Current Sample Size", str(results['current_sample_size']))
    summary_table.add_row("Required Sample Size", f"{results['required_sample_size']:.1f}")
    summary_table.add_row("Sample Size Adequate", 
                         Text("✓ Yes", style="green") if results['is_sample_size_enough'] 
                         else Text("✗ No", style="red"))
    summary_table.add_row("Confidence Level", f"{results['confidence']}%")
    summary_table.add_row("Acceptable Error", f"{results['acceptable_sampling_error']*100}%")
    
    console.print(summary_table)
    
    if not results['is_sample_size_enough']:
        needed_samples = results['required_sample_size'] - results['current_sample_size']
        warning_panel = Panel(
            f"[red]Need {needed_samples:.1f} more sample units for adequate sample size[/red]",
            title="[bold red]Warning[/bold red]",
            border_style="red"
        )
        console.print(warning_panel)


def analyze_sampling_unit(result_folders: list[str], sampling_unit_size: int, index: int, core_groups: list[list[int]] | None = None):
    """
    Analyze a specific sampling unit from direct measurement data.
    """
    # Get instruction count data
    instruction_data, instruction_data_u = parse_direct_measurements(result_folders, sampling_unit_size)
    
    console.print(f"[bold cyan]Analysis Configuration:[/bold cyan]")
    console.print(f"[green]Result folders: {len(result_folders)}[/green]")
    console.print(f"[green]Sampling unit size: {sampling_unit_size} INTERVAL(s)[/green]")
    console.print(f"[green]INTERVAL value: {INTERVAL}[/green]")
    console.print(f"[green]Analyzing sampling unit at index: {index}[/green]")
    
    # Check if the index is valid
    if index >= instruction_data_u.shape[1]:
        console.print(f"[red]Error: Index {index} is out of bounds. Maximum index is {instruction_data_u.shape[1] - 1}[/red]")
        sys.exit(1)
    
    # Extract data for the specified interval index
    interval_instruction_u_data = instruction_data_u[:, index, :]  # Shape: [snapshots, cores]
    
    # Calculate IPC for each snapshot and core
    interval_ipc_u_data = interval_instruction_u_data / (INTERVAL * sampling_unit_size)
    
    # Analyze core groups if specified, otherwise analyze all cores as one group
    if core_groups:
        console.print(f"\n[bold cyan]Analyzing {len(core_groups)} core groups separately:[/bold cyan]")
        for group_idx, core_group in enumerate(core_groups):
            console.print(f"\n[yellow]Core Group {group_idx + 1}: cores {core_group}[/yellow]")
            
            # Validate that all cores in the group exist in the data
            max_core_idx = interval_ipc_u_data.shape[1] - 1
            valid_cores = [core for core in core_group if 0 <= core <= max_core_idx]
            invalid_cores = [core for core in core_group if core not in valid_cores]
            
            if invalid_cores:
                console.print(f"[red]Warning: cores {invalid_cores} not found in data (max core index: {max_core_idx})[/red]")
            
            if not valid_cores:
                console.print("[red]Error: No valid cores found in this group.[/red]")
                continue
            
            # Aggregate across specified cores for each snapshot
            group_ipc_data = interval_ipc_u_data[:, valid_cores]  # Shape: [snapshots, group_cores]
            snapshot_group_ipc = np.sum(group_ipc_data, axis=1)  # Shape: [snapshots]
            
            # Filter out NaN and zero values
            valid_data = [x for x in snapshot_group_ipc if not math.isnan(x) and x != 0]
            
            if len(valid_data) == 0:
                console.print("[red]Error: No valid data found for this core group.[/red]")
                continue
            
            # Calculate statistics for this group
            average_ipc = np.mean(valid_data)
            std_dev = np.std(valid_data)
            coefficient_of_variation = std_dev / average_ipc
            
            # Calculate required sample size for 95% confidence and 5% error
            required_sample_size = (1.96 * coefficient_of_variation / 0.05) ** 2
            current_sample_size = len(valid_data)
            is_sample_size_enough = current_sample_size >= required_sample_size
            
            # Report results for this group in a table
            group_table = Table(title=f"Core Group {group_idx + 1} Statistics", box=box.ROUNDED)
            group_table.add_column("Metric", style="cyan")
            group_table.add_column("Value", style="green")
            
            group_table.add_row("Valid cores used", str(valid_cores))
            group_table.add_row("Valid snapshots", str(current_sample_size))
            group_table.add_row("Average IPC", f"{average_ipc:.4f}")
            group_table.add_row("Standard deviation", f"{std_dev:.4f}")
            group_table.add_row("Coefficient of variation", f"{coefficient_of_variation:.4f}")
            group_table.add_row("Required sample size", f"{required_sample_size:.1f}")
            group_table.add_row("Sample size adequate", 
                               Text("✓ Yes", style="green") if is_sample_size_enough 
                               else Text("✗ No", style="red"))
            
            console.print(group_table)
            
            if not is_sample_size_enough:
                needed = required_sample_size - current_sample_size
                console.print(f"[red]Need {needed:.1f} more sample units for adequate sample size[/red]")
    else:
        # Original behavior: aggregate across all cores
        console.print("\n[bold cyan]Analyzing all cores as a single group:[/bold cyan]")
        
        # Aggregate across cores for each snapshot to get total IPC per snapshot
        snapshot_total_ipc = np.sum(interval_ipc_u_data, axis=1)  # Shape: [snapshots]
        
        # Filter out NaN and zero values
        valid_data = [x for x in snapshot_total_ipc if not math.isnan(x) and x != 0]
        
        if len(valid_data) == 0:
            console.print("[red]Error: No valid data found for the specified sampling unit.[/red]")
            sys.exit(1)
        
        # Calculate statistics
        average_ipc = np.mean(valid_data)
        std_dev = np.std(valid_data)
        coefficient_of_variation = std_dev / average_ipc
        
        # Calculate required sample size for 95% confidence and 5% error
        required_sample_size = (1.96 * coefficient_of_variation / 0.05) ** 2
        current_sample_size = len(valid_data)
        is_sample_size_enough = current_sample_size >= required_sample_size
        
        # Report results in a table
        all_cores_table = Table(title="All Cores Statistics", box=box.ROUNDED)
        all_cores_table.add_column("Metric", style="cyan")
        all_cores_table.add_column("Value", style="green")
        
        all_cores_table.add_row("Valid snapshots", str(current_sample_size))
        all_cores_table.add_row("Average IPC", f"{average_ipc:.4f}")
        all_cores_table.add_row("Standard deviation", f"{std_dev:.4f}")
        all_cores_table.add_row("Coefficient of variation", f"{coefficient_of_variation:.4f}")
        all_cores_table.add_row("Required sample size", f"{required_sample_size:.1f}")
        all_cores_table.add_row("Sample size adequate", 
                               Text("✓ Yes", style="green") if is_sample_size_enough 
                               else Text("✗ No", style="red"))
        
        console.print(all_cores_table)
        
        if not is_sample_size_enough:
            needed = required_sample_size - current_sample_size
            console.print(f"[red]Need {needed:.1f} more sample units for adequate sample size[/red]")
    
    # Additional per-core statistics
    # console.print(f"\n[bold cyan]Per-core IPC statistics:[/bold cyan]")
    # core_average_ipc = np.mean(interval_ipc_u_data, axis=0)  # Average across snapshots for each core
    
    # per_core_table = Table(title="Per-Core IPC", box=box.SIMPLE)
    # per_core_table.add_column("Core", justify="center", style="cyan")
    # per_core_table.add_column("Average IPC", justify="center", style="green")
    
    # for core_idx, avg_ipc in enumerate(core_average_ipc):
    #     if not math.isnan(avg_ipc) and avg_ipc > 0:
    #         per_core_table.add_row(str(core_idx), f"{avg_ipc:.4f}")
    #     else:
    #         per_core_table.add_row(str(core_idx), Text("No valid data", style="red"))
    
    # console.print(per_core_table)


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
        """
    )
    
    parser.add_argument(
        "--core-info-path",
        default="./run/core_info.csv",
        help="Path to existing core_info.csv file. If exists, will automatically generate core_info_new.csv (default: ./run/core_info.csv)"
    )
    parser.add_argument(
        "-u", "--unit-size",
        type=int,
        default=1,
        help="Size of sampling unit in terms of INTERVAL (default: 1)"
    )
    parser.add_argument(
        "-i", "--index",
        type=int,
        default=2,
        help="Index of the sampling unit to analyze (in terms of number of INTERVAL)"
    )
    parser.add_argument(
        '-g', '--core-groups',
        help='Specify core groups to analyze separately (e.g., "0-7,8-15" or "0,1,2,3-5"). If not specified, all cores are analyzed as one group.'
    )
    parser.add_argument(
        '--confidence',
        type=float,
        help="Confidence level for sample size calculation (optional, default: 95.0)"
    )
    parser.add_argument(
        '--error',
        type=float,
        help="Acceptable sampling error (optional, default: 0.05)"
    )
    parser.add_argument(
        '--plot',
        action='store_true',
        default=True,
        help="Plot U-IPC distribution using plotext (default: True)"
    )
    parser.add_argument(
        '--no-plot',
        dest='plot',
        action='store_false',
        help="Disable plotting of U-IPC distribution"
    )
    
    args = parser.parse_args()

    console.print(Panel.fit("[bold green]Measurement Data Analysis Tool[/bold green]", border_style="green"))

    # Find all result_* folders recursively
    result_folders = []
    for root, dirs, files in os.walk("./run"):
        for dir_name in dirs:
            if dir_name.startswith("result_"):
                result_folders.append(os.path.join(root, dir_name))

    if not result_folders:
        console.print("[red]Error: No result_* folders found in current directory[/red]")
        sys.exit(1)

    console.print(f"[green]Found {len(result_folders)} result folders[/green]")

    # Parse core groups if provided
    core_groups = None
    if args.core_groups:
        try:
            core_groups = parse_core_groups(args.core_groups)
            console.print(f"[green]Parsed core groups: {core_groups}[/green]")
        except Exception as e:
            console.print(f"[red]Error parsing core groups '{args.core_groups}': {e}[/red]")
            sys.exit(1)

    # Analyze the sampling unit
    analyze_sampling_unit(result_folders, args.unit_size, args.index, core_groups)

    # Always plot U-IPC distribution regardless of core_info.csv generation
    plot_u_ipc_distribution(result_folders, args.unit_size, args.index, args.plot)

    # Always generate new core_info.csv if core_info_path exists
    if os.path.exists(args.core_info_path):
        console.print(f"\n[bold cyan]Generating new core_info.csv...[/bold cyan]")
        
        results = generate_new_core_info(
            result_folders,
            args.core_info_path,
            args.unit_size,
            args.index,
            args.confidence,
            args.error,
            "./core_info_new.csv"
        )
        
        # Display results
        display_results_table(results)
        
        console.print(f"\n[bold green]Generated new core_info.csv at ./core_info_new.csv[/bold green]")
        console.print(f"[green]Sampling unit size: {args.unit_size}, Index: {args.index}[/green]")
    else:
        console.print(f"\n[yellow]Core info file not found at {args.core_info_path}, skipping core_info_new.csv generation[/yellow]")

if __name__ == "__main__":
    main()

