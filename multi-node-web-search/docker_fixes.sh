
# New change for docker image: TODO add it to main docker image
pushd ..

cat > /etc/resolv.conf <<EOF
nameserver 10.90.36.4
nameserver 10.95.34.209
nameserver 10.90.53.15
search iccluster.epfl.ch intranet.epfl.ch epfl.ch xaas.epfl.ch
EOF

rm -rf /mnt/sdb/pooria-multi-web/experiments/multi_web_experiment0/lib/WormCacheQFlex/ && rm -f /mnt/sdb/pooria-multi-web/experiments/multi_web_experiment0/lib/libworm_cache.so && ls /mnt/sdb/pooria-multi-web/experiments/multi_web_experiment0/lib
rm -rf /mnt/sdb/pooria-multi-web/experiments/multi_web_experiment1/lib/WormCacheQFlex/ && rm -f /mnt/sdb/pooria-multi-web/experiments/multi_web_experiment1/lib/libworm_cache.so && ls /mnt/sdb/pooria-multi-web/experiments/multi_web_experiment1/lib



apt-get update -y
apt-get install -y iputils-ping

make parallel-qemu-config
make parallel-qemu-build

popd