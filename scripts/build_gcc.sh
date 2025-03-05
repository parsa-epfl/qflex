#!/usr/bin/env bash
#
# _           _        _ _    ____  ____ ____
#(_)_ __  ___| |_ __ _| | |  / ___|/ ___/ ___|
#| | '_ \/ __| __/ _` | | | | |  _| |  | |
#| | | | \__ \ || (_| | | | | |_| | |__| |___
#|_|_| |_|___/\__\__,_|_|_|  \____|\____\____|
#
#   Bryan Perdrizat
#       This file install the GCC with $version to
#       /opt/qflex/bin/gcc. This should be used only in
#       the case your system has no easy way to download
#       the latest GCC which is kind of needed to build
#       KnottyKraken.
set -e

output_dir="/opt/qflex"
version="13.2.0"

# If the output directory exists let's assume that
# gcc is already installed
if [ -d "$output_dir" ]; then
    exit 0
fi

# Create temp directory
tmp=$(mktemp -d)

# Set current working directory
# to the temp dir
pushd $tmp

# Download
wget https://ftp.mpi-inf.mpg.de/mirrors/gnu/mirror/gcc.gnu.org/pub/gcc/releases/gcc-${version}/gcc-${version}.tar.xz
# Extract
tar -xvf gcc-${version}.tar.xz
# Configure
./gcc-${version}/configure --host=x86_64-linux-gnu --target=x86_64-linux-gnu \
                            --enable-languages=c,c++ \
                            --disable-multilib --disable-boostrap \
                            --enable-lto --prefix=/opt/qflex/ -enable-threads=posix --disable-libssp \
                            --disable-libstdcxx-pch --disable-werror --libdir=/opt/qflex/lib \
                            --libexecdir=/opt/qflex/lib --mandir=/opt/qflex/share/man --infodir=/opt/qflex/share/info
# Compile
make -j12
# Install
sudo make install

popd

exit 0
