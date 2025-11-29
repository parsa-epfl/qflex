
cloud-localds seed.img user-data
qemu-img convert -f raw -O qcow2 seed.img seed.qcow2


cloud-localds set-up.img set-up-env
qemu-img convert -f raw -O qcow2 set-up.img set-up.qcow2
