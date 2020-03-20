# QFlex #

QFlex is an instrumentation framework with several tools for different use cases. We support trace-based simulation to quickly instrument existing QEMU images, timing models to simulate multi-core CPU microarchitectures in detail and an FPGA-accelerating mode which enables high-performing instrumented code. We based our framework on QEMU, a widely-used machine emulator, which is able to boot any machine and execute unmodified applications and operating systems. 

More information is available on the QFlex [website][qfw].

[![QFlex](http://qflex.epfl.ch/wp-content/uploads/2018/09/QFlex.png)](http://qflex.epfl.ch/)

# Licensing #

QFlex's software components are all available as open-source software. All of the software components are governed by 
their own licensing terms. Researchers interested in using QFlex are required to fully understand and abide by the 
licensing terms of the various components. For more information, please refer to the [license page][qfl].

# Running QFlex #

Instructions on how to run an example benchmark on matrix multiplication with QFlex are available [here][qfd]. Along with QFlex, we provide a 64-bit ARM version image of Ubuntu 16 [here][qfi]. This way, QFlex users can easily start running required workloads and perform a microarchitectural study of the aforementioned benchmark.

QFlex is and will always will be a work in progress, and at this stage, we are able to perform full-system simulation of a single server node (for 64-bit ARM). We are working on many other features including multi-node simulation.

# Support #

We encourage QFlex users to use GitHub issues for requests for enhancements, questions or bug fixes.

[qfw]: http://qflex.epfl.ch/
[qfl]: http://qflex.epfl.ch/license/
[qfd]: http://qflex.epfl.ch/download/
[qfi]: https://github.com/parsa-epfl/images/tree/arm
