<h1 align="center">QFlex</h1>
<p align="center">
  <i>State-of-the-art modeling tools for the computer architecture community.<br/>The QFlex project targets quick, accurate, and flexible simulation of computer systems</i>
  <br/>
  <img width="600" alt="QFlex" src="docs/readme-assets/qflex_logo.svg"/>
  <br/>
  <b><a href="https://parsa-epfl.github.io/qflex/quickstart/">Documentation</a></b> | <b><a href="https://github.com/parsa-epfl/qflex">GitHub</a></b> | <b><a href="https://qflex.epfl.ch">Website</a></b>
  <br/><br/>
</p>

## [🎯 Features](#features)

* 🚀 **Fast**. First class support for statistical sampling. Near-GIPS full-system server simulation.
* 🕰 **Timing-First**. 70 KIPS cycle-accurate simulation.
* 🗂️ **Components-based**. Create custom components.
* ✨ **Free**. QFlex is completely free and open source.


## [🚀 Getting started](#getting-started)

More information regarding the build system and other subsystem of
QFlex can be found in the [documentation](https://parsa-epfl.github.io/qflex/quickstart/).

### 1. Tools

The following tools should be installed to start QFlex

- [Python, pip](https://www.python.org/) >= 3.9
- [docker](https://www.docker.com/)

### 2. Clone repositories
```sh
git clone --recursive git@github.com:parsa-epfl/qflex.git
```

### 3. Build the python requirements
```sh
cd qflex
pip install -r requirements.txt
```


### 4. Start the docker image

```sh
./dep --start-docker --worm
```

### 5. Show the help from QFlex cli inside the docker image
```sh
./qflex --help
```

### 6. Follow the quick start guide
Follow the quick start on our documentation page: [Quick Start Guide](https://parsa-epfl.github.io/qflex/quickstart/). The guide will help you start your workload and study it using QFlex. It will also help you understand how statistical sampling helps with both accuracy and speed and how to use it within QFlex.


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
