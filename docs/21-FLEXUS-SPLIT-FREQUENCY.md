# Flexus Frequency Simulation Feature

## Problem
Qflex currently simulates all system components (core, uncore) at the same frequency.

## Desired Functionality
Enable simulation of components at different frequencies, e.g.:
- Core 1: 2 GHz
- Core 2: 3 GHz
- Uncore: 1.5 GHz

## Terminology
- **Core Components**:
  - Fetch Unit
  - Fetch Address Generator (FAG)
  - Microarchitecture (uArch)
  - Memory Management Unit (MMU)
  - Decoder
  - L1D Cache

- **Uncore Components**:
  - Network Interface Controller (NIC)
  - Network on Chip (NoC)
  - Memory Controllers
  - L2 Cache

- **Drive Frequency**: Number of times a component is driven in one cycle.

## Notes
- The scheduling algorithm processes components sequentially. For multiple instances, it executes them first (e.g., F1 -> F2 -> FAG1 -> FAG2 -> uArch1 -> uArch2).
- Desired scheduling granularity:
  - Core 1 (F1 -> FAG1 -> uArch1)
  - Core 2 (F2 -> FAG2 -> uArch2)
  - Uncore (NIC1 -> N1 -> M1 -> M2 -> L1)
- Core components must have the same number of units (e.g., 4 cores require 4 fetch units, FAGs, and MMUs). MMUs can differ in unit count (e.g., 1 L2, 4 memory controllers).

## List of Changes
- **[Libqflex]** Accept command line arguments for frequency:
  - Format: `-libqflex mode=timing,...,freq=2:1:3`
  - Order: Core 0 frequency, Core 1 frequency, ..., Uncore frequency.
  - Frequencies can be fractional (up to one decimal place), e.g., `freq=1.2,2.5,2.1`.
  - An argument is required for each core/uncore; otherwise, an assertion error occurs.

- **[Flexus]** Use two vectors for components:
  - One for core components
  - One for uncore components

- **[Flexus]** On initialization:
  1. Convert frequency string into drive numbers:
     - Convert fractions to whole numbers using the LCM of denominators (e.g., `1.5:3` -> `3:6`).
  2. Normalize drive frequencies (e.g., `3:6` -> `1:2`).
  3. Store the new list and the maximum frequency.

- **[Flexus]** Implement a new `do_cycle` function:
  - Iterate over core components one by one (only for cores with the same number of instances).
  - Uncore components will continue using the previous function.

- **[Flexus]** Fine-grained scheduling algorithm:
  - Maintain uniformity in scheduling (e.g., avoid patterns like C1->C1->C1->C2->C2->U->U).
  - Aim for patterns like C1->C2->U->C1->C2->U->C2.
