
# New change for docker image: TODO add it to main docker image
pushd ..
experiment_name=multi_web_full_experiment_14_1

rm -rf /mnt/sdb/pooria-multi-web/experiments/${experiment_name}/lib/WormCacheQFlex/ && rm -f /mnt/sdb/pooria-multi-web/experiments/${experiment_name}/lib/libworm_cache.so && ls /mnt/sdb/pooria-multi-web/experiments/${experiment_name}/lib


rm -f /mnt/sdb/pooria-multi-web/experiments/${experiment_name}/lib/*.so  && rm -f /mnt/sdb/pooria-multi-web/experiments/${experiment_name}/run/vanilla-qemu-system-aarch64

popd