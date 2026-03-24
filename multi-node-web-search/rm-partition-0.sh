COMMON_ARGS=$(cat ./qflex.multinode-web.args)
NODE_ARGS=$(cat ./multi-node-web-search/qflex.node.0.args)

pushd ../..

echo ${COMMON_ARGS} {NODE_ARGS} | xargs -o ./qflex partition-cleanup --syncs "false"

popd