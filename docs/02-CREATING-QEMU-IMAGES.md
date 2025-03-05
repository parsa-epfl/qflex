# Creating OS image

A VM (Virtual Machine) image is a snapshot of a virtual machine's state, comprising its operating system, software, configurations, and data. It encapsulates the disk storage and storage properties. VM images are used to deploy and replicate virtual machines efficiently, simplifying the process of creating new instances with identical setups. They enable portability across different virtualization platforms and facilitate backup and recovery operations. VM images can be stored in various formats, such as VMDK, VHD, or QCOW2, tailored to specific virtualization solutions like QEMU.

## How to build an image

You can either:
1. download a pre-baked image
2. install an image from an ISO
3. build a bare GNU/Linux kernel

## Pre-Backed image

Using a pre backed image is the simplest way to get started using a VM.
First, choose an image among the following provider.

### Provider
* Debian [arm64-virt](https://people.debian.org/~gio/dqib/)

All the next pre-baked image require some modification before being used. The following will set the root password for your image.
```bash
 virt-customize -v -a [image name] --root-password password:[new root password]

```
* Ubuntu [Cloud Image](https://cloud-images.ubuntu.com/)
* Arch [Repo](https://gitlab.archlinux.org/archlinux/arch-boxes/), [Packages](https://geo.mirror.pkgbuild.com/images/)
* Fedora [aarch64](https://fedoraproject.org/cloud/download)
* CentOS [Packages](https://cloud.centos.org/centos/)
* CirrOS [...disk.img](http://download.cirros-cloud.net/)

### How to run
```bash
qemu-system-aarch64 -machine virt -m 2G -cpu max -smp 1 \
-bios [path to QEMU build dir]/pc-bios/edk2-aarch64-code.fd \
-drive file=[pre-baked file],if=virtio,format=qcow2
```

## Build manually

*VERY useful links*
* [Install Alpine Linux in QEMU](https://wiki.alpinelinux.org/wiki/QEMU)
* [Running an aarch64 image in qemu](https://openqa-bites.github.io/posts/2022/2022-04-13-qemu-aarch64/)

***

Building the image manually is similar to setting up an OS on a physical machine. It's a matter of inserting the bootable CD-ROM or USB key.

### x86\_64

To create an Ubuntu image for QEMU, first, download the Ubuntu ISO from the official website. Then, use QEMU's disk image creation tool to allocate storage space.

```bash
qemu-img create -f qcow2 ubuntu.qcow2 40G

```

Next, initialize a disk image file using the desired format, like QCOW2. Launch a QEMU virtual machine with the Ubuntu ISO as a bootable CD-ROM.

```bash
qemu-system-x86_64 -m 4096 -drive file=ubuntu.qcow2 -cdrom [Ubuntu ISO file]
```

Follow the Ubuntu installation prompts, selecting disk space allocation and configuration options. Complete the installation process, shutting down the virtual machine once finished. [For reference](https://realtechtalk.com/QEMU_KVM_How_To_Manually_Create_Basic_Virtual_Machine_VM-2138-articles)

***

### aarch64

To create an Ubuntu image for QEMU targeting aarch64 (arm64), start by obtaining the Ubuntu arm64 ISO. Then, allocate storage space using QEMU's disk image creation tool.

```bash
qemu-img create -f qcow2 ubuntu.qcow2 40G

```
Proceed to create a UEFI firmware image for arm64 architecture using QEMU's provided tools.

```bash
truncate -s 64m efi.img
truncate -s 64m varstore.img
dd if=/usr/share/qemu-efi-aarch64/QEMU_EFI.fd of=efi.img conv=notrunc
```
Launch a QEMU virtual machine with the UEFI firmware and Ubuntu ISO attached.

```bash
qemu-system-aarch64 -nographic -machine virt,gic-version=max -m 2G -cpu max -smp 2 \
-drive file=efi.img,if=pflash,format=raw \
-drive file=varstore.img,if=pflash,format=raw \
-drive file=ubuntu.qcow2,if=virtio,format=qcow2 \
-cdrom [Ubuntu ISO file]
```

Follow Ubuntu's installation prompts, configuring disk space and system settings. Complete the installation and shut down the VM.
[For reference](https://futurewei-cloud.github.io/ARM-Datacenter/qemu/how-to-launch-aarch64-vm/)


## Bare GNU/Linux Kernel
[TODO]

