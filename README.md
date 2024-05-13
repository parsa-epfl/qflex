# Quick Getting Started

More information regarding the build system and other subsystem of
QFlex can be found in the `docs`.

## Install packages

The following packages should be installed to build QEMU and FLEXUS

- GNU Compiler (gcc/g++) 13 (with C++14 support) [Which GNU Compiler support which standard](https://gcc.gnu.org/projects/cxx-status.html)
- [cmake](https://cmake.org)
- [conan](https://conan.io)
- [meson](https://mesonbuild.com)
- GNU Lib C (min 2.35) _`ldd --version` to check_
- GNOME Lib C (min 2.72)

No requirements for old/outdated Linux distributions. Just use latest ones.

## Clone repositories

```sh
git clone -b bryan/rework https://github.com/parsa-epfl/qflex
git submodule update --init
rm -rf ./qemu/middleware
git clone -b feat/qemu-8.2 https://github.com/parsa-epfl/libqflex qemu/middleware
```

## Building QEMU

```sh
./build cq
```

## Building Flexus
```sh
 conan profile detect #One time only
./build keenkraken
./build knottykraken
```

## Create symlinks
```sh
ln -s qemu/build/aarch64-softmmu/qemu-system-aarch64 qemu-aarch64
```

## Add Images

[Download a simple image](https://github.com/parsa-epfl/qflex/releases/download/2024.05/images.tar.xz) and place it the
repository root location.

```sh
wget https://github.com/parsa-epfl/qflex/releases/download/2024.05/images.tar.xz
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

## Run
```sh
./runq images/bb-trace # run keenkraken release version
./runq images/bb-timing-dev # run knottykraken debug version
```
The filesystem now contains basic tools provided by `/bin/busybox` like `ls`, `cd`, etc.

## Notes
These repos are updated frequently, so please also pull them frequently.