pushd .. 
COMMON_ARGS=$(cat ./multi-node-web-search/qflex.multinode-web.args)

NODE_ARGS=$(cat ./multi-node-web-search/qflex.node.0.args)

echo ${COMMON_ARGS} ${NODE_ARGS} | xargs -o ./qflex load --syncs "true" --loadvm-name boot-pooria

popd