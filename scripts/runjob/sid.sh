home=/home/sgupta/qflex
export CFLAGS="-fPIC"
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

cd $home/flexus
make -j8 KnottyKraken-arm

cd $home/qemu
./configure --target-list=aarch64-softmmu --enable-flexus --enable-extsnap --python=/usr/bin/python2.7
make -j

#QEMU
./qemu-system-aarch64 --machine virt -cpu cortex-a57 -smp 1 -m 2000
    -global virtio-blk-device.scsi=off -device virtio-scsi-device,id=scsi
    -rtc driftfix=slew -nographic -drive if=none,file=$home/images/ubuntu16/ubuntu.qcow2,id=hd0
    -pflash $home/images/ubuntu16/flash0.img -pflash $home/images/ubuntu16/flash1.img
    -device scsi-hd,drive=hd0 -device virtio-scsi-device -netdev user,id=net1,hostfwd=tcp::2220-:22
    -device virtio-net-device,mac=52:54:00:00:02:12,netdev=net1 -exton -accel tcg,thread=single

-device virtio-scsi-device -netdev user,id=net1,hostfwd=tcp::2220-:22 -device virtio-net-device,mac=52:54:00:00:02:12,netdev=net1

#TRACE
./qemu-system-aarch64 --machine virt -cpu cortex-a57 -smp 1 -m 2000 -global virtio-blk-device.scsi=off
-device virtio-scsi-device,id=scsi -rtc driftfix=slew -nographic -drive
if=none,file=$home/images/ubuntu16/ubuntu.qcow2,id=hd0 -pflash $home/images/ubuntu16/flash0.img
-pflash $home/images/ubuntu16/flash1.img -loadext spec_mcf -device scsi-hd,drive=hd0 -device
virtio-scsi-device -netdev user,id=net1,hostfwd=tcp::2220-:22
-device virtio-net-device,mac=52:54:00:00:02:12,netdev=net1 -exton -accel tcg,thread=single
-flexus mode=trace,length=20000,simulator=$home/flexus/simulators/KeenKraken/libflexus_KeenKraken_arm_iface_gcc.so,config=$home/scripts/config/user_postload,debug=dev


#TIMING
./qemu-system-aarch64 --machine virt -cpu cortex-a57 -smp 1 -m 2000 -global virtio-blk-device.scsi=off
-device virtio-scsi-device,id=scsi -rtc driftfix=slew -nographic -drive
if=none,file=$home/images/ubuntu16/ubuntu.qcow2,id=hd0 -pflash $home/images/ubuntu16/flash0.img
-pflash $home/images/ubuntu16/flash1.img -loadext spec_bzip2 -device scsi-hd,drive=hd0 -device
virtio-scsi-device -netdev user,id=net1,hostfwd=tcp::2220-:22
-device virtio-net-device,mac=52:54:00:00:02:12,netdev=net1 -exton -accel tcg,thread=single
-flexus mode=timing,length=20000,simulator=$home/flexus/simulators/KnottyKraken/libflexus_KnottyKraken_arm_iface_gcc.so,config=$home/scripts/config/user_postload,debug=dev
-singlestep -d nochain,in_asm |& tee /dev/shm/sid_output

/qflex/qflex/qemu/aarch64-softmmu/qemu-system-aarch64 --machine virt -cpu cortex-a57 -smp 1 -m 2000 -global virtio-blk-device.scsi=off -device virtio-scsi-device,id=scsi -rtc clock=vm -nographic -drive if=none,file=/qflex/qflex/images/ubuntu16/ubuntu.qcow2,id=hd0 -pflash /qflex/qflex/images/ubuntu16/flash0.img -pflash /qflex/qflex/images/ubuntu16/flash1.img -device scsi-hd,drive=hd0 -device virtio-scsi-device -netdev user,id=net1,hostfwd=tcp::2220-:22 -device virtio-net-device,mac=52:54:00:00:02:12,netdev=net1

#script emulation
/qflex/qflex/qemu/aarch64-softmmu/qemu-system-aarch64
-global virtio-blk-device.scsi=off -device virtio-scsi-device,id=scsi
-drive file=/qflex/qflex/images/ubuntu16/ubuntu.qcow2,id=hd0,if=none
-device scsi-hd,drive=hd0 -pflash /qflex/qflex/images/ubuntu16/flash0.img
-pflash /qflex/qflex/images/ubuntu16/flash1.img -smp 1 -m 2000 -machine virt -cpu cortex-a57
-name i0 -nographic -exton -accel tcg,thread=single -rtc driftfix=slew

#script trace
/qflex/qflex/qemu/aarch64-softmmu/qemu-system-aarch64
-global virtio-blk-device.scsi=off -device virtio-scsi-device,id=scsi
-drive file=/qflex/qflex/images/ubuntu16/ubuntu.qcow2,id=hd0,if=none
-device scsi-hd,drive=hd0 -pflash /qflex/qflex/images/ubuntu16/flash0.img
-pflash /qflex/qflex/images/ubuntu16/flash1.img -smp 1 -m 2000 -machine virt -cpu cortex-a57
-loadext boot -name i0 -nographic -exton -accel tcg,thread=single -rtc driftfix=slew

#script timing
/qflex/qflex/qemu/aarch64-softmmu/qemu-system-aarch64
-global virtio-blk-device.scsi=off -device virtio-scsi-device,id=scsi
-drive file=/qflex/qflex/images/ubuntu16/ubuntu.qcow2,id=hd0,if=none
-device scsi-hd,drive=hd0 -pflash /qflex/qflex/images/ubuntu16/flash0.img
-pflash /qflex/qflex/images/ubuntu16/flash1.img -smp 1 -m 2000 -machine virt -cpu cortex-a57
-flexus mode=timing,length=100k,simulator=/qflex/qflex/flexus/simulators/KnottyKraken/libflexus_KnottyKraken_arm_iface_gcc.so,config=/qflex/qflex/scripts/my_config/user_postload
-singlestep -d nochain -d in_asm -name i0 -nographic -exton -accel tcg,thread=single -rtc driftfix=slew






