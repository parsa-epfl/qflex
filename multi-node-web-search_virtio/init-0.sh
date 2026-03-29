
COMMON_ARGS=$(cat ./qflex.multinode-web.args)

NODE_ARGS=$(cat ./qflex.node.0.args)

pushd .. 

echo ${COMMON_ARGS} ${NODE_ARGS} | xargs -o ./qflex initialize --syncs "true" --loadvm-name load-client-virtio --latancies-ns "100000"

popd