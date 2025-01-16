RUNQ
====

This document shows all about how to run any of the uArch simulators (KeenKraken & KnottyKraken).

## Config File
QFlex uses two differents type configuration files.

The first is a custom file version of a QEMU invokation arguments. The file type can have any name and any extension.

The second, `trace.cfg` and `timing.cfg` are used to configure KeenKraken or KnottyKraken. They are written in a specific structure only parsable by Flexus. The file type can have any name and any extension.

### QEMU Config

The config file is pure text, and shall follow the following standard.
- Top level options should be written without any dash (-) nor any whitespaces prepended
- 2nd level options needs to be indented with the same amount of whitespace along the file.

All key-value arguments whether top or second level, needs a space between their key and their valu.


```txt

nographic

machine
    virt
    gic-version     max
    virtualization  off
    secure          off

cpu
    max
    pauth           off

parallel
    none
m
    4G

rtc
    clock           vm
```

The config contains three variables defined by `runQ`.
- ROOT: Give the location where the `runQ` executable is located
- SMP: The number of core to use for the next run, default=1
- EXTRA: Arbitry command passed by the command line, usually used to add uArch simulator specific arguments

```txt

kernel
    ${ROOT}/Busybox/Image
smp
    ${SMP}

drive
    file            ${ROOT}/Busybox/rootfs.qcow2
    format          qcow2
    if              virtio

libqflex
    mode            trace
    lib-path        ${ROOT}/../out/keenkraken/Debug/libkeenkraken.so
    cfg-path        ${ROOT}/../trace.cfg
    debug           vverb
    ${EXTRA}
```

### uArch Config
`trace.cfg` and `timing.cfg` files contains variables definition that will overload the default ones. The best way to change the configuration is to rely on the file themselves and their documentation.

## runQ
This utility is written in pure Python. There is no requirement on the python verison; 3.11.9 is version known to work.

Before using `runQ` you may want to make the file executable using `chmod +x runq`. `runQ` have an information page accessible using `./runq -h`.

### Basic

`runQ` must take a configuration file as input.

```bash
./runq my_dir/my_config
```

The number of cores can be changed easily using `--smp`.

```bash
./runq my_dir/my_config --smp 64
```

### Debug

`runQ` contains debug options:
- `-d`,`--dry` : Print QEMU start command instead of executing it
- `-b`,`--binary` : Set a custom binary to uses instead of looking in the current working directory
- `--gdb` : Start a GDB instance an execute the command in it

### Simulator

The extra flags `-e`, or `--extra` can be used to add arguments to an existing configuration file. The argument will be set in place of the ${EXTRA} placeholder.

```txt
libqflex
    mode            trace
    lib-path        ${ROOT}/../out/keenkraken/Debug/libkeenkraken.so
    cfg-path        ${ROOT}/../trace.cfg
    debug           vverb
    ${EXTRA}
```

## Docker
In order to get parsa/qflex, build the docker image locally, for more information please refer to the [BUILD-FLEXUS](./01-BUILD-FLEXUS.md) document.
You can directly enter the docker container to work and run your simulation

```bash
docker run -ti --entrypoint bash --rm --volume <images>:/qflex/<images> parsa/qflex-[release|debug]
```

You can also run everything directly from docker by running the following:

```bash
docker run -ti --rm --volume <images>:/qflex/<images> parsa/qflex-[release|debug] [runq command|ex: ./runq images/bb]
```










