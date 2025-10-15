---
title: QFlex — Quick & Flexible Computer Architecture Simulation
---

# QFlex | Quick & Flexible Computer Architecture Simulation

State-of-the-art modeling tools for the computer architecture community.  
The QFlex project targets quick, accurate, and flexible simulation of multi-node computer systems.

![QFlex Overview](logo/qflex_overview_scheme.jpg)

---

## Download QFlex

- QFlex’s source code is hosted on **GitHub**: [parsa-epfl/qflex](https://github.com/parsa-epfl/qflex)  
- QFlex is available both as a **command-line tool (CLI)** and as a **Docker image**, making setup and integration straightforward.  
- For more details on installation, usage, and configuration, visit the [CLI documentation](reference/cli/).

---

## About QFlex

Computer system designers traditionally rely on fast emulation and full-system simulation to instrument designs, develop system software, model hardware components, and measure metrics of interest. In recent years, post-Moore platforms have introduced more heterogeneity (e.g. accelerators), demanding integrated hardware/software co-design.

Effective integration requires open tools to allow fast instrumentation of applications, full-system models for network and storage controllers, and support for multi-node systems. Full-system server instrumentation needs several orders of magnitude in speed to be practical.

QFlex is a family of full-system instrumentation tools built on top of QEMU, currently supporting **ARM** and **RISC-V** ISAs. It includes:

- A **trace-based model** for quickly instrumenting existing QEMU images  
- **Timing models** to simulate multicore CPU microarchitecture  
- Support for multi-node simulation and system stacks

For more details, modules, and citations, see the project documentation and the GitHub repo.

---

## News / Releases

### QFlex v2.0 Released

Released March 20, 2020. QFlex continues to support trace-based simulation and timing models. (See release notes on GitHub.)  

### QFlex v1.0 Released

Released April 1, 2017. QFlex enabled full-system microarchitectural simulation of multicores running unmodified applications. A CloudSuite workload image was provided. ([zenodo.org](https://zenodo.org/records/504368))  

We encourage users to **watch the GitHub repository** and file issues for enhancements or bug fixes.

---

## Related Projects & Links

- **Flexus** (cycle-accurate simulator used in QFlex): [parsa-epfl/flexus](https://github.com/parsa-epfl/flexus)  
- **libqflex** (API between QEMU and Flexus): [parsa-epfl/libqflex](https://github.com/parsa-epfl/libqflex)  
- EPFL’s EcoCloud page on QFlex: [EcoCloud – QFlex](https://ecocloud.epfl.ch/research/sustainable-it-infrastructure/qflex/)  

---

## Citation / Licensing & Acknowledgments

- QFlex is open source under licenses as described in the GitHub repository.  
- The project relies on QEMU, Boost, Conan, and other components, each governed by their own licenses.  

© 2025 EPFL PARSA – 1015 Lausanne, Switzerland – tel. +41 21 693 1395 – All Rights Reserved
