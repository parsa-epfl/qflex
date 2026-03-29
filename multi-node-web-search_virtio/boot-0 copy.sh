pushd .. 
COMMON_ARGS=$(cat ./multi-node-web-search/qflex.multinode-web.args)

NODE_ARGS=$(cat ./multi-node-web-search/qflex.node.no.args)

echo ${COMMON_ARGS} ${NODE_ARGS} | xargs -o ./qflex boot

popd