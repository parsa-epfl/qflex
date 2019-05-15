#!/bin/sh
# QFlex consists of several software components that are governed by various
# licensing terms, in addition to software that was developed internally.
# Anyone interested in using QFlex needs to fully understand and abide by the
# licenses governing all the software components.

# ### Software developed externally (not by the QFlex group)

#     * [NS-3](https://www.gnu.org/copyleft/gpl.html)
#     * [QEMU](http://wiki.qemu.org/License)
#     * [SimFlex](http://parsa.epfl.ch/simflex/)

# ### Software developed internally (by the QFlex group)
# **QFlex License**

# QFlex
# Copyright (c) 2016, Parallel Systems Architecture Lab, EPFL
# All rights reserved.

# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:

#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright notice,
#       this list of conditions and the following disclaimer in the documentation
#       and/or other materials provided with the distribution.
#     * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
#       nor the names of its contributors may be used to endorse or promote
#       products derived from this software without specific prior written
#       permission.

# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
# EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

set -x
set -e
sudo apt-get update -qq
sudo apt-get install -y build-essential checkinstall wget python-dev \
    software-properties-common pkg-config zip zlib1g-dev unzip curl
# Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential checkinstall git-core libbz2-dev libtool expect bridge-utils uml-utilities
sudo apt-get --no-install-recommends -y build-dep qemu
# Install a compatible version of GCC
sudo apt-get install python-software-properties
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get -y install gcc-${GCC_VERSION} g++-${GCC_VERSION}
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${GCC_VERSION} 20
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-${GCC_VERSION} 20
# Install Boost Library
wget http://sourceforge.net/projects/boost/files/boost/${BOOST_VERSION}/${BOOST}.tar.bz2 -O /tmp/${BOOST}.tar.gz
tar -xf /tmp/${BOOST}.tar.gz
rm -f /tmp/${BOOST}.tar.gz
cd ./${BOOST}/
./bootstrap.sh --prefix=/usr/local --with-libraries=date_time,serialization,regex,iostreams
./b2 -j4
sudo ./b2 install
cd ..
rm -rf ./${BOOST}/
# Fetch all the submodules (QEMU, images and etc.)
git submodule update --init --recursive
# Get images
cd images
git lfs pull --exclude "debian-memcached/*"
cd ..
# Build Qemu
cd ./qemu
export CFLAGS="-fPIC"
if [ "$TARGET" = "KeenKraken-arm" ]; then
    ./configure --target-list=aarch64-softmmu --enable-flexus --disable-werror --disable-tpm
else
    ./configure --target-list=sparc64-softmmu --enable-flexus --disable-werror --disable-tpm
fi
make -j4
# Build Qemu with Flexus
cd ../flexus
make $TARGET
make stat-manager
