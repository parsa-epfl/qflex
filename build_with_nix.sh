#!/usr/bin/env bash

set -e

SIM=${1:-"qemu"}
BUILD_TYPE=${2:-"release"}

FLEXUS_ROOT=$(realpath flexus)
QEMU_ROOT=$(realpath qemu)

case $SIM in
    *kraken)
        pushd $FLEXUS_ROOT

        # if build directory exists, remove it

        if [ -d "build" ]; then
            rm -rf build
        fi
        
        mkdir build
        cd build

        if [ $BUILD_TYPE = "debug" ];
        then
            cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -DSIMULATOR=$SIM -G Ninja ..
        else
            cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Release -DSIMULATOR=$SIM -G Ninja ..
        fi

        ninja
        popd
        ;;
    [cC][Qq]|qemu)
        pushd $QEMU_ROOT

        if [ "$BUILD_TYPE" = "debug" ]; then
            ./configure --target-list=aarch64-softmmu       \
                        --disable-docs                      \
                        --enable-capstone                   \
                        --enable-slirp                      \
                        --enable-snapvm-external            \
                        --enable-libqflex                   \
                        --enable-debug                      \
                        --extra-cflags="-fsanitize=address" \
                        --extra-cflags="-fno-omit-frame-pointer"

        else
            ./configure --target-list=aarch64-softmmu       \
                        --disable-docs                      \
                        --enable-capstone                   \
                        --enable-slirp                      \
                        --enable-snapvm-external            \
                        --enable-libqflex

        fi
        popd
        ninja -C qemu/build
        ;;
    [Qq])
        ninja -C qemu/build
        ;;
    *)
        echo "Never heard of '${SIM}'"
        exit 1
        ;;
esac
