# QFlex #

QFlex is a full-system cycle-accurate computer architecture simulator of multi-node computer systems. QFlex is a multi-layer software stack composed of QEMU, Flexus, and NS-3. QEMU is a widely-used machine emulator, which allows to boot any machine and execute unmodified applications and operating systems. Flexus is a cycle-accurate modeling tool of complete servers, which enables modeling modern CPUs, with various core types, network-on-chip topologies, and cache organizations; and various DRAM-based memory systems. NS-3 is a popular network simulator that allows to glue all the simulated server nodes together with different network integration characteristics. 

More information is available in the QFlex [website][qfw].

[![QFlex](https://parsalab.github.io/qflex/public/images/QFlex.png)](http://parsalab.github.io/qflex/)

# Licensing #

QFlex's software components are all available as open-source software. All of the software components are governed by 
their own licensing terms. Researchers interested in using QFlex are required to fully understand and abide by the 
licensing terms of the various components. For more information, please refer to the [license page][qfl].

# Running QFlex #

Instructions on how to run QFlex are available [here][qfd]. Along with QFlex, we provide an image of one of the CloudSuite benchmarks, [Data Caching][csdc], running on Debian 8 for 64-bit ARM. This way, QFlex users can easily perform a microarchitectural study of the aforesaid benchmark.

QFlex is still a work in progress, and at this stage, we provide limited functionality. Currently, QFlex is able perform full-system trace-based simulation of a single server node. Hence, no timing models are available yet.

# Support #

We encourage QFlex users to use GitHub issues for requests for enhacements or bug fixes.

[qfw]: http://parsalab.github.io/qflex/
[qfl]: http://parsalab.github.io/qflex//pages/license/
[qfd]: http://parsalab.github.io/qflex/pages/download/
[csdc]: http://cloudsuite.ch/datacaching/


