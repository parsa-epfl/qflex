# Recovering an Alpine Kernel ELF with `vmlinux-to-elf`

This document covers one scope only:

- after `qflex boot`, if the boot workflow did not already capture a ready
  `vmlinux` ELF, how to recover one from the kernel bundle produced by `boot`

It is written for:

- host: `iccluster118`
- container: `cb8*`
- repo: `/home/dev/qflex_git`

## What `boot` is expected to leave behind

The bounded `boot` workflow creates an experiment kernel bundle under:

- `/mnt/sdb/aansari/experiments/<experiment>/kernel/`

For an Alpine guest, that bundle is expected to contain at least:

- `vmlinuz-*`
- `System.map-*`
- `config-*`

Those files are the input to the recovery process below.

This document does not cover:

- rebuilding Alpine kernels from source
- using `aports`
- validating a recovered ELF against a timing checkpoint

## Toolchain prerequisites

The recovery process runs on the host and needs:

- `git`
- `python3`
- `python3 -m venv`
- network access to clone `vmlinux-to-elf`
- network access for `pip install .` to resolve Python dependencies

## Inputs

The minimum required input is:

- the `vmlinuz-*` file from the kernel bundle produced by `boot`

Useful companion files:

- `System.map-*`
- `config-*`

For the current web-search example on this machine, the boot-step kernel bundle
contains:

- `/mnt/sdb/aansari/experiments/ws-image-fresh-8c/kernel/vmlinuz-virt`
- `/mnt/sdb/aansari/experiments/ws-image-fresh-8c/kernel/System.map-virt`
- `/mnt/sdb/aansari/experiments/ws-image-fresh-8c/kernel/config-virt`

## Output

The output is a recovered kernel ELF, for example:

- `/tmp/qflex-kernel-recovery/vmlinux-recovered-from-vmlinuz.elf`

## Step-by-step procedure

### 1. Prepare a disposable host workspace

```bash
mkdir -p /tmp/qflex-kernel-recovery
cd /tmp/qflex-kernel-recovery
```

### 2. Clone `vmlinux-to-elf`

```bash
git clone https://github.com/marin-m/vmlinux-to-elf.git /tmp/vmlinux-to-elf
```

### 3. Create a virtualenv and install the tool

```bash
cd /tmp/vmlinux-to-elf
python3 -m venv .venv
. .venv/bin/activate
pip install --upgrade pip
pip install .
```

Sanity check:

```bash
vmlinux-to-elf --help
```

### 4. Run the recovery on the `vmlinuz-*` captured by `boot`

Example using the current web-search kernel bundle:

```bash
cd /tmp/vmlinux-to-elf
. .venv/bin/activate

vmlinux-to-elf \
  /mnt/sdb/aansari/experiments/ws-image-fresh-8c/kernel/vmlinuz-virt \
  /tmp/qflex-kernel-recovery/vmlinux-recovered-from-vmlinuz.elf
```

For another experiment, replace the input path with the `vmlinuz-*` file from:

- `/mnt/sdb/aansari/experiments/<experiment>/kernel/`

What the tool should report:

- successful decompression of the compressed kernel image
- detection of the kernel release string
- detection of `aarch64`
- successful writing of the output ELF

## Basic verification

Check the recovered file type:

```bash
file /tmp/qflex-kernel-recovery/vmlinux-recovered-from-vmlinuz.elf
```

Expected shape:

- `ELF 64-bit`
- `ARM aarch64`
- `statically linked`

Check the embedded release string:

```bash
strings /tmp/qflex-kernel-recovery/vmlinux-recovered-from-vmlinuz.elf \
  | grep -m 1 'Linux version'
```

For the current web-search example on this machine, the expected version string
starts with:

```text
Linux version 6.1.34-3-virt ...
```

## Example: store the recovered ELF next to the image

If you accept the recovered ELF as the reference kernel for an image, store it
next to that image. For the current web-search image:

```bash
cp /tmp/qflex-kernel-recovery/vmlinux-recovered-from-vmlinuz.elf \
  /mnt/sdb/aansari/ws-image/web-search-fresh-8c/vmlinux-6.1.34-3-virt.elf
```

## Summary

For an Alpine guest in this project:

1. run `qflex boot`
2. go to the kernel bundle produced by `boot`
3. take the captured `vmlinuz-*`
4. recover an ELF with `vmlinux-to-elf`
5. verify the architecture and release string

That is the documented extraction process.
