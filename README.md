<h1 align="center">QFlex</h1>
<p align="center">
  <i>State-of-the-art modeling tools for the computer architecture community.<br/>The QFlex project targets quick, accurate, and flexible simulation of computer systems</i>
  <br/>
  <img width="600" alt="QFlex" src="docs/readme-assets/qflex_logo.svg"/>
  <br/>
  <b><a href="https://github.com/parsa-epfl/qflex">GitHub</a></b> | <b><a href="https://qflex.epfl.ch">Website</a></b>
  <br/><br/>
</p>

## Overview

This repository is the top-level entrypoint for the qflex workflow.

In the current project organization, users should normally drive the system
through the `qflex` CLI rather than through lower-level QPoints helpers or
legacy scripts. The maintained user-facing documentation for the gem5
conversion and timing flow lives under:

- [docs/gem5_conversion/project_progress.md](docs/gem5_conversion/project_progress.md)
- [docs/gem5_conversion/qflex_cli_guide.md](docs/gem5_conversion/qflex_cli_guide.md)

These two documents are the current starting point for:

- what this project phase delivered
- what remains for future work
- how to use the current qflex CLI workflow from boot through timing runs

## Current direction

The current project direction is:

- qflex as the primary user-facing entrypoint
- BXKraken-based functional warming and statistical sampling on the source side
- Flexus and gem5 as timing-engine options under the same broader workflow
- validation records tracked in `QPoints/validation_records`

For the gem5 path specifically, the maintained workflow is documented in:

- [docs/gem5_conversion/qflex_cli_guide.md](docs/gem5_conversion/qflex_cli_guide.md)

For the project-level status and validated scope, use:

- [docs/gem5_conversion/project_progress.md](docs/gem5_conversion/project_progress.md)

## Basic setup

### 1. Clone repositories

```sh
git clone --recursive git@github.com:parsa-epfl/qflex.git
```

### 2. Install Python requirements

```sh
cd qflex
pip install -r requirements.txt
```

### 3. Inspect the CLI surface

```sh
./qflex --help
```

Container bring-up and Docker organization are still in flux. The current
container surface exists, but it is not yet the clean long-term contract for
the project. See:

- [docs/gem5_conversion/project_progress.md](docs/gem5_conversion/project_progress.md)


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
