# Preparation

## Install packages

The following packages should be installed to build QEMU and FLEXUS

- GNU Compiler (gcc/g++) (with C++14 support) [Which GNU Compiler support which standard](https://gcc.gnu.org/projects/cxx-status.html)
- cmake
- meson
- ninja (maybe as a dependency of meson)
- boost
- GNU Lib C (min 2.35) _Run `ldd --version` to know about_
- GNOME Lib C (min 2.72)

No requirements for old/outdated Linux distributions. Just use latest ones.

## Clone repositories

```sh
git clone -b bryan/rework https://github.com/parsa-epfl/qflex
git submodule update --init
rm -rf ./qemu/middleware
git clone -b feat/qemu-8.2 https://github.com/parsa-epfl/libqflex qemu/middleware
```

## Building `prod` or `dev` version
Both QEMU and Flexus can be build both for production or developement.
The developement version enable the maximum debug compilation for GCC, and
add the address sanitizer (ASAN) with the flag `-fsanitize=address`, while also intoducing the frame pointer with `-fno-omit-frame-pointer`.

To do so, use the debug flag (yes|no) when appropriate.

```sh
./build [receipt] [(yes|no)]
```

## Building QEMU

```sh
./build cq
```

## Building FLEXUS
```sh
./build KeenKraken
./build KnottyKraken
```

## Create symlinks
```sh
ln -s qemu/build/aarch64-softmmu/qemu-system-aarch64 qemu-aarch64
```

## Add Image

[Download a simple image](https://github.com/parsa-epfl/qflex/releases/download/2024.03/images.tar.xz) and place it the
repository root location.

```sh
wget https://github.com/parsa-epfl/qflex/releases/download/2024.03/images.tar.xz
tar -xvf images.tar.xz
```

The repository should look like this.

```

images/
├── busybox
└── Busybox
    ├── Image
    ├── rootfs.ext4
    └── rootfs.qcow2
```

# Run

## Normal QEMU

```sh
./runq images/busybox
```

The filesystem now contains basic tools provided by `/bin/busybox` like `ls`, `cd`, etc. It also contains a `/bin/media` file that is the workload that I am currently running.

When you think the simulation proceeds to the point you want, hit Ctrl-A + Ctrl-C to enter the QEMU monitor, and type the following commands:
```sh
stop # stops simulation first
savevm-external SNAPSHOT_NAME
```

After that you can type `cont` to continue the simulation or `q` to quit.

You can modify any command line arguments passed to QEMU in the `emu` file, which just writes those arguments in a prettier format.

## Trace simulation

```sh
./runq +trace
```

The simulation log is saved in `debug.log` instead of being displayed in the terminal (the latter is broken). You can change parameters passed to FLEXUS in the `trace.cfg` file.

<!-- ## Timing simulation

```sh
./runq +timing
```

You can change parameters passed to FLEXUS in the `timing.cfg` file. -->

# Notes

These repos are updated frequently, so please also pull them frequently.