
# New change for docker image: TODO add it to main docker image
pushd ..
experiment_name=multi_web_full_experiment_8_0
cat > /etc/resolv.conf <<EOF
nameserver 10.90.36.4
nameserver 10.95.34.209
nameserver 10.90.53.15
search iccluster.epfl.ch intranet.epfl.ch epfl.ch xaas.epfl.ch
EOF

rm -rf /mnt/sdb/pooria-multi-web/experiments/${experiment_name}/lib/WormCacheQFlex/ && rm -f /mnt/sdb/pooria-multi-web/experiments/${experiment_name}/lib/libworm_cache.so && ls /mnt/sdb/pooria-multi-web/experiments/${experiment_name}/lib



apt-get update -y
apt-get install -y iputils-ping

make parallel-qemu-config
make parallel-qemu-build

make qemu-config MODE=release
make qemu-build MODE=release

make flexus-config MODE=release
make flexus-build MODE=release


rm -f /mnt/sdb/pooria-multi-web/experiments/${experiment_name}/lib/*.so  && rm -f /mnt/sdb/pooria-multi-web/experiments/${experiment_name}/run/vanilla-qemu-system-aarch64

popd