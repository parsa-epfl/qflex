<h1 align="center">QFlex</h1>
<p align="center">
  <i>State-of-the-art modeling tools for the computer architecture community.<br/>The QFlex project targets quick, accurate, and flexible simulation of computer systems</i>
  <br/>
  <img width="600" alt="QFlex" src="docs/readme-assets/qflex_logo.png"/>
  <br/>
  <b><a href="https://github.com/parsa-epfl/qflex/tree/main/docs">Documentation</a></b> | <b><a href="https://github.com/parsa-epfl/qflex">GitHub</a></b> | <b><a href="https://qflex.epfl.ch">Website</a></b>
  <br/><br/>
</p>

## [🎯 Features](#features)

* 🕰 **Timing-First**. 70 KIPS cycle-accurate simulation.
* 🗂️ **Components-based**. Create custom components.
* ✨ **Free**. QFlex is completely free and open source.


## [🚀 Getting started](#getting-started)

More information regarding the build system and other subsystem of
QFlex can be found in the `docs`.

### 1. Tools

The following packages should be installed to build QFlex

- [GNU Compiler](https://gcc.gnu.org/) >= 13.1
- [cmake](https://cmake.org)
- [conan](https://conan.io)
- [meson](https://mesonbuild.com)

### 2. Clone repositories
```sh
git clone --recursive https://github.com/parsa-epfl/qflex
```

### 3. Build QEMU

```sh
./build cq
```

### 4. Build Flexus
```sh
 conan profile detect #One time only
./build semikraken
./build knottykraken
```

### 5. Create symlinks
```sh
ln -s qemu/build/aarch64-softmmu/qemu-system-aarch64 qemu-aarch64
```

### 6. Add Images

[Download a simple image](https://github.com/parsa-epfl/qflex/releases/latest/) and place it the
repository root location.

```sh
wget https://github.com/parsa-epfl/qflex/releases/latest/download/images.tar.xz
tar -xvf images.tar.xz
```

The repository tree under images folder should look like this.

```
images/
├── busybox
└── Busybox
    ├── Image
    ├── rootfs.ext4
    └── rootfs.qcow2
```

### 7. Run
```sh
./runq images/bb # run a bare QEMU emulation
./runq images/bb-timing # run knottykraken release version
```
The filesystem now contains basic tools provided by `/bin/busybox` like `ls`, `cd`, etc.

## [🏆 Contributors](#contributors)

<p align="center">
    <a href="https://github.com/parsa-epfl/qflex/graphs/contributors">
      <img src="https://contrib.rocks/image?repo=parsa-epfl/qflex" />
    </a>
</p>

Made with [contrib.rocks](https://contrib.rocks).

## [📄 License](#license)

This software is an open-sourced software licensed under the following license:

```text
***Software developed externally (not by the QFlex group)***

QFlex consists of several software components that are governed by various
licensing terms, in addition to software that was developed internally.
Anyone interested in using QFlex needs to fully understand and abide by the
licenses governing all the software components.

  * [QEMU] (https://wiki.qemu.org/License)
  * [Boost] (https://www.boost.org/users/license.html)
  * [Conan] (https://github.com/conan-io/conan/blob/develop2/LICENSE.md)

**QFlex License**

QFlex
Copyright (c) 2025, Parallel Systems Architecture Lab, EPFL
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
   nor the names of its contributors may be used to endorse or promote
   products derived from this software without specific prior written
   permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```
