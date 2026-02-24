pushd ..
 
COMMON_ARGS=$(cat ./multi-node-web-search/qflex.multinode-web.args)

NODE_ARGS=$(cat ./multi-node-web-search/qflex.node.1.args)

echo ${COMMON_ARGS} ${NODE_ARGS} | xargs -o ./qflex partition --syncs "true" --partition-count 1 --latancies-ns "10000"

popd