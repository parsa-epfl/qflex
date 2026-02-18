pushd .. 
COMMON_ARGS=$(cat ./multi-node-web-search/qflex.multinode-web.args)

NODE_ARGS=$(cat ./multi-node-web-search/qflex.node.0.args)

echo ${COMMON_ARGS} ${NODE_ARGS} | xargs -o ./qflex partition-cleanup --syncs "true" --latancies-ns "1000000"

popd