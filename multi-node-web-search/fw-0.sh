pushd .. 

COMMON_ARGS=$(cat ./multi-node-web-search/qflex.multinode-web.args)

NODE_ARGS=$(cat ./multi-node-web-search/qflex.node.0.args)

echo ${COMMON_ARGS} ${NODE_ARGS} | xargs -o ./qflex fw --syncs "true" --loadvm-name init_warmed --sample-size 601 --latancies-ns "100000"

popd