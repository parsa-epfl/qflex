
COMMON_ARGS=$(cat ./qflex.multinode-web.args)

NODE_ARGS=$(cat ./qflex.node.0.args)

pushd .. 
echo ${NODE_ARGS} ${COMMON_ARGS}

echo ${COMMON_ARGS} ${NODE_ARGS} | xargs -o ./qflex boot --syncs "false" --latancies-ns $1

popd