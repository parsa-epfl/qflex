#!/usr/bin/env python3

import pandas as pd
import numpy as np
import argparse
import math
import sys
import os
import shutil
from datetime import datetime

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

def parse_file_per_core(file_name: str, diff_distance: int = 1) -> tuple[np.ndarray, np.ndarray, int]:
    df = pd.read_csv(file_name)
    
    # Get unique values and create mappings
    snapshots = sorted(df["snapshot_id"].unique())
    cycles = sorted(df["cycle"].unique())
    cores = sorted(df[df["instruction"] != 0]["core"].unique())
    
    print(f"Snapshots: {len(snapshots)}")
    print(f"Core Count: {len(cores)}")
    print(f"Cycle Count: {len(cycles)}")
    
    INTERVAL = df["cycle"].min()
    print(f"INTERVAL value: {INTERVAL}")
    
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
        instruction_u = row["instruction:u"]
    
        # Skip rows with zero instructions or cores not in our valid cores list
        if instruction == 0 or core not in core_to_idx:
            continue
            
        # Get array indices
        snapshot_idx = snapshot_to_idx[snapshot_id]
        cycle_idx = cycle_to_idx[cycle]
        core_idx = core_to_idx[core]
        
        # Add values to arrays (sum in case of multiple entries)
        instructions[snapshot_idx, cycle_idx, core_idx] = instruction
        instructions_u[snapshot_idx, cycle_idx, core_idx] = instruction_u
    
    # Reduce cycles by diff_distance
    instructions = instructions[:, ::diff_distance, :]
    instructions_u = instructions_u[:, ::diff_distance, :]
    
    # Calculate differences
    instructions = np.diff(instructions, axis=1, prepend=np.zeros((instructions.shape[0], 1, instructions.shape[2])))
    instructions_u = np.diff(instructions_u, axis=1, prepend=np.zeros((instructions_u.shape[0], 1, instructions_u.shape[2])))
    
    return instructions, instructions_u, INTERVAL

def read_old_core_info(old_core_info_path):
    """
    Read the old core_info.csv to get ipns and affinity_core_idx values
    Returns a list of (ipns, affinity_core_idx) tuples in order
    """
    df = pd.read_csv(old_core_info_path)
    
    # Convert to list of tuples
    core_info = [(row['ipns'], row['affinity_core_idx']) for _, row in df.iterrows()]
    
    return core_info


def generate_new_core_info(timing_csv_path, old_core_info_path, sampling_unit_size, index, output_path=None):
    """
    Generate new core_info.csv with updated IPC values from timing data using per-core sampling analysis
    Returns comparison data for display
    """
    # Generate timestamp for backups
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    
    # Get instruction count data and INTERVAL
    instruction_data, _, INTERVAL = parse_file_per_core(timing_csv_path, sampling_unit_size)
    
    # Check if the index is valid
    if index >= instruction_data.shape[1]:
        print(f"Error: Index {index} is out of bounds. Maximum index is {instruction_data.shape[1] - 1}")
        sys.exit(1)
    
    # Extract data for the specified interval index
    # instruction_data shape: [snapshots, intervals, cores]
    interval_instruction_data = instruction_data[:, index, :]  # Shape: [snapshots, cores]
    
    # Calculate IPC for each snapshot and core
    interval_ipc_data = interval_instruction_data / (INTERVAL * sampling_unit_size)
    
    # Average across snapshots for each core
    core_average_ipc = np.mean(interval_ipc_data, axis=0)  # Shape: [cores]
    
    # Filter out cores with zero or NaN IPC
    valid_core_ipc = {}
    for core_idx, ipc in enumerate(core_average_ipc):
        if not math.isnan(ipc) and ipc > 0:
            valid_core_ipc[core_idx] = ipc
    
    if len(valid_core_ipc) == 0:
        print("Error: No valid per-core IPC data found for the specified sampling unit.")
        sys.exit(1)
    
    # Read old core_info to get affinity mappings
    old_core_info = read_old_core_info(old_core_info_path)
    
    # Determine output path
    if output_path is None:
        output_path = old_core_info_path
        # Create timestamped backup of old core_info.csv
        backup_core_info_path = f"{old_core_info_path}.{timestamp}.backup"
        shutil.copy2(old_core_info_path, backup_core_info_path)
        print(f"Backed up old core_info.csv to {backup_core_info_path}")
    
    # Create timestamped backup of timing.csv
    backup_timing_path = f"{timing_csv_path}.{timestamp}.backup"
    shutil.copy2(timing_csv_path, backup_timing_path)
    print(f"Backed up timing.csv to {backup_timing_path}")
    
    # Prepare data for new core_info.csv and comparison
    new_core_data = []
    comparison_data = []
    
    cores_with_timing_data = 0
    cores_inherited = 0
    
    for i, (old_ipns, affinity_core_idx) in enumerate(old_core_info):
        # Use per-core IPC if available, otherwise inherit from old core_info.csv
        if i in valid_core_ipc:
            # Round to 2 decimal places
            new_ipc = round(2*valid_core_ipc[i], 2)
            cores_with_timing_data += 1
            updated = True
        else:
            # If no timing data for this core, inherit from old core_info.csv
            new_ipc = old_ipns
            cores_inherited += 1
            updated = False
        
        # Add to new core data
        new_core_data.append({
            'ipns': f"{new_ipc:.2f}",
            'affinity_core_idx': int(affinity_core_idx)
        })
        
        # Store comparison data
        comparison_data.append({
            'core': i,
            'old_ipc': f"{old_ipns:.2f}",
            'new_ipc': f"{new_ipc:.2f}",
            'difference': f"{(new_ipc - old_ipns):.2f}",
            'updated': updated
        })
    
    # Create DataFrame and save to CSV
    new_df = pd.DataFrame(new_core_data)
    new_df.to_csv(output_path, index=False)
    
    print(f"Generated new core_info.csv at {output_path}")
    print(f"Updated IPC values for {cores_with_timing_data} cores from per-core timing data")
    print(f"Inherited IPC values for {cores_inherited} cores from old core_info.csv")
    print(f"Sampling unit size: {sampling_unit_size}, Index: {index}")
    
    return comparison_data


def display_ipc_comparison_table(comparison_data):
    """
    Display a formatted table showing the IPC comparison
    """
    print("\nIPC Comparison Table:")
    print("=" * 60)
    print(f"{'Core':<6} {'Old IPC':<10} {'New IPC':<10} {'Difference':<12} {'Updated':<8}")
    print("-" * 60)
    
    for data in comparison_data:
        updated_str = "Yes" if data['updated'] else "No"
        print(f"{data['core']:<6} {data['old_ipc']:<10} {data['new_ipc']:<10} "
              f"{data['difference']:<12} {updated_str:<8}")
    
    # Summary statistics
    updated_cores = sum(1 for d in comparison_data if d['updated'])
    total_cores = len(comparison_data)
    avg_old_ipc = np.mean([float(d['old_ipc']) for d in comparison_data])
    avg_new_ipc = np.mean([float(d['new_ipc']) for d in comparison_data])
    
    print("-" * 60)
    print(f"Summary: {updated_cores}/{total_cores} cores updated")
    print(f"Average old IPC: {avg_old_ipc:.2f}")
    print(f"Average new IPC: {avg_new_ipc:.2f}")
    print(f"Overall difference: {(avg_new_ipc - avg_old_ipc):.2f}")
    print("=" * 60)


def analyze_sampling_unit(csv_file: str, sampling_unit_size: int, index: int, core_groups: list[list[int]] | None = None):
    """
    Analyze a specific sampling unit from the CSV file.
    
    Args:
        csv_file: Path to the CSV file
        sampling_unit_size: Size of sampling unit in terms of INTERVAL
        index: Index of the sampling unit to analyze (in terms of number of INTERVAL)
        core_groups: List of core groups to analyze separately (optional)
    """
    # Get instruction count data and INTERVAL
    instruction_data, instruction_data_u, INTERVAL = parse_file_per_core(csv_file, sampling_unit_size)
    
    print(f"CSV file: {csv_file}")
    print(f"Sampling unit size: {sampling_unit_size} INTERVAL(s)")
    print(f"INTERVAL value: {INTERVAL}")
    print(f"Analyzing sampling unit at index: {index}")
    print("-" * 50)
    
    # Check if the index is valid
    if index >= instruction_data_u.shape[1]:
        print(f"Error: Index {index} is out of bounds. Maximum index is {instruction_data_u.shape[1] - 1}")
        sys.exit(1)
    
    # Extract data for the specified interval index
    # instruction_data shape: [snapshots, intervals, cores]
    interval_instruction_u_data = instruction_data_u[:, index, :]  # Shape: [snapshots, cores]
    
    # Calculate IPC for each snapshot and core
    interval_ipc_u_data = interval_instruction_u_data / (INTERVAL * sampling_unit_size)
    
    # Analyze core groups if specified, otherwise analyze all cores as one group
    if core_groups:
        print(f"Analyzing {len(core_groups)} core groups separately:")
        for group_idx, core_group in enumerate(core_groups):
            print(f"\nCore Group {group_idx + 1}: cores {core_group}")
            print("-" * 40)
            
            # Validate that all cores in the group exist in the data
            max_core_idx = interval_ipc_u_data.shape[1] - 1
            valid_cores = [core for core in core_group if 0 <= core <= max_core_idx]
            invalid_cores = [core for core in core_group if core not in valid_cores]
            
            if invalid_cores:
                print(f"Warning: cores {invalid_cores} not found in data (max core index: {max_core_idx})")
            
            if not valid_cores:
                print("Error: No valid cores found in this group.")
                continue
            
            # Aggregate across specified cores for each snapshot
            group_ipc_data = interval_ipc_u_data[:, valid_cores]  # Shape: [snapshots, group_cores]
            snapshot_group_ipc = np.sum(group_ipc_data, axis=1)  # Shape: [snapshots]
            
            # Filter out NaN and zero values
            valid_data = [x for x in snapshot_group_ipc if not math.isnan(x) and x != 0]
            
            if len(valid_data) == 0:
                print("Error: No valid data found for this core group.")
                continue
            
            # Calculate statistics for this group
            average_ipc = np.mean(valid_data)
            std_dev = np.std(valid_data)
            coefficient_of_variation = std_dev / average_ipc
            
            # Calculate required sample size for 95% confidence and 5% error
            required_sample_size = (1.96 * coefficient_of_variation / 0.05) ** 2
            current_sample_size = len(valid_data)
            is_sample_size_enough = current_sample_size >= required_sample_size
            
            # Report results for this group
            print(f"Valid cores used: {valid_cores}")
            print(f"Number of valid snapshots: {current_sample_size}")
            print(f"Average IPC (sum across group cores): {average_ipc:.4f}")
            print(f"Standard deviation: {std_dev:.4f}")
            print(f"Coefficient of variation: {coefficient_of_variation:.4f}")
            print(f"Required sample size (95% confidence, 5% error): {required_sample_size:.1f}")
            print(f"Sample size adequate: {'Yes' if is_sample_size_enough else 'No'}")
            
            if not is_sample_size_enough:
                print(f"Need {required_sample_size - current_sample_size:.1f} more samples for adequate sample size")
    else:
        # Original behavior: aggregate across all cores
        print("Analyzing all cores as a single group:")
        print("-" * 40)
        
        # Aggregate across cores for each snapshot to get total IPC per snapshot
        snapshot_total_ipc = np.sum(interval_ipc_u_data, axis=1)  # Shape: [snapshots]
        
        # Filter out NaN and zero values
        valid_data = [x for x in snapshot_total_ipc if not math.isnan(x) and x != 0]
        
        if len(valid_data) == 0:
            print("Error: No valid data found for the specified sampling unit.")
            sys.exit(1)
        
        # Calculate statistics
        average_ipc = np.mean(valid_data)
        std_dev = np.std(valid_data)
        coefficient_of_variation = std_dev / average_ipc
        
        # Calculate required sample size for 95% confidence and 5% error
        # Formula: n = (Z * CV / E)^2, where Z=1.96 for 95% confidence, E=0.05 for 5% error
        required_sample_size = (1.96 * coefficient_of_variation / 0.05) ** 2
        current_sample_size = len(valid_data)
        is_sample_size_enough = current_sample_size >= required_sample_size
        
        # Report results
        print(f"Number of valid snapshots: {current_sample_size}")
        print(f"Average IPC (aggregated across cores): {average_ipc:.4f}")
        print(f"Standard deviation: {std_dev:.4f}")
        print(f"Coefficient of variation: {coefficient_of_variation:.4f}")
        print(f"Required sample size (95% confidence, 5% error): {required_sample_size:.1f}")
        print(f"Sample size adequate: {'Yes' if is_sample_size_enough else 'No'}")
        
        if not is_sample_size_enough:
            print(f"Need {required_sample_size - current_sample_size:.1f} more samples for adequate sample size")
    
    # Additional per-core statistics
    print(f"\nPer-core IPC statistics:")
    print("-" * 30)
    core_average_ipc = np.mean(interval_ipc_u_data, axis=0)  # Average across snapshots for each core
    for core_idx, avg_ipc in enumerate(core_average_ipc):
        if not math.isnan(avg_ipc) and avg_ipc > 0:
            print(f"Core {core_idx}: {avg_ipc:.4f}")
        else:
            print(f"Core {core_idx}: No valid data")


def main():
    parser = argparse.ArgumentParser(
        description="Analyze sampling results and generate core_info.csv from timing data",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Analyze timing data only
  python analyze_sampling_result.py timing.csv --unit-size 2 --index 1
  
  # Analyze timing data with core groups
  python analyze_sampling_result.py timing.csv --unit-size 2 --index 1 --core-groups "0-7,8-15"
  
  # Analyze timing data AND generate new core_info.csv
  python analyze_sampling_result.py timing.csv --core-info old_core_info.csv --unit-size 2 --index 1
  
  # Analyze timing data AND generate new core_info.csv with custom output path
  python analyze_sampling_result.py timing.csv --core-info old_core_info.csv --output new_core_info.csv --unit-size 2 --index 1
        """
    )
    
    parser.add_argument(
        'timing_csv',
        help="Path to timing.csv file"
    )
    parser.add_argument(
        '--core-info',
        help="Path to existing core_info.csv file (optional - if provided, will generate new core_info.csv)"
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
        required=True,
        help="Index of the sampling unit to analyze (in terms of number of INTERVAL)"
    )
    parser.add_argument(
        '-o', '--output',
        help="Output path for new core_info.csv (default: replace old file with backup). Only used if --core-info is provided."
    )
    parser.add_argument(
        '-g', '--core-groups',
        help='Specify core groups to analyze separately (e.g., "0-7,8-15" or "0,1,2,3-5"). If not specified, all cores are analyzed as one group.'
    )
    
    args = parser.parse_args()

    # Parse core groups if provided
    core_groups = None
    if args.core_groups:
        try:
            core_groups = parse_core_groups(args.core_groups)
            print(f"Parsed core groups: {core_groups}")
        except Exception as e:
            print(f"Error parsing core groups '{args.core_groups}': {e}")
            sys.exit(1)

    # Analyze the sampling unit.
    analyze_sampling_unit(args.timing_csv, args.unit_size, args.index, core_groups)

    # If core_info is provided, generate new core_info.csv
    if args.core_info:
        if not os.path.exists(args.core_info):
            print(f"Error: old core_info.csv file not found: {args.core_info}")
            sys.exit(1)
        
        # Generate new core_info.csv with updated IPC values
        comparison_data = generate_new_core_info(
            args.timing_csv, 
            args.core_info, 
            args.unit_size, 
            args.index, 
            args.output
        )
        
        # Display IPC comparison table
        display_ipc_comparison_table(comparison_data)

if __name__ == "__main__":
    main()

