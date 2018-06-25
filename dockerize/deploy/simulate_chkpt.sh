#! /bin/bash
# Place this file in image path
IMAGE_DIR=$1
CHKPT=$2

cd /home/usr/qflex/images/"$IMAGE_DIR"/"$CHKPT"
echo 1 > preload_system_width
/home/usr/qflex/qemu/aarch64-softmmu/qemu-system-aarch64 -machine virt -cpu cortex-a57 -smp 1 -m 2000 -kernel /home/usr/qflex/images/{$IMAGE_DIR}/vmlinuz-4.4.0-83-generic -initrd /home/usr/qflex/images/{$IMAGE_DIR}/initrd.img-4.4.0-83-generic -append 'console=ttyAMA0 root=/dev/sda2' -global virtio-blk-device.scsi=off -device virtio-scsi-device,id=scsi -drive file=/home/usr/qflex/images/{$IMAGE_DIR}/ubuntu-16.04-lts-blank.qcow2,id=rootimg,cache=unsafe,if=none -device scsi-hd,drive=rootimg -rtc driftfix=slew -nographic -exton -loadext "$CHKPT" -simulatefor 10000000 -startsimulation -simpath "/home/usr/qflex/flexus.so"
rm preload_system_width
