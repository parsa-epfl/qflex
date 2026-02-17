pushd .. 
COMMON_ARGS=$(cat ./multi-node-web-search/qflex.multinode-web.args)

NODE_ARGS=$(cat ./multi-node-web-search/qflex.node.0.args)

echo ${COMMON_ARGS} ${NODE_ARGS} | xargs -o ./qflex boot --syncs "false" --latancies-ns $1

popd