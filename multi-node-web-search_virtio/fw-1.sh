COMMON_ARGS=$(cat ./qflex.multinode-web.args)

NODE_ARGS=$(cat ./qflex.node.1.args)


pushd .. 

echo ${COMMON_ARGS} ${NODE_ARGS} | xargs -o ./qflex fw --syncs "true" --loadvm-name init_warmed --sample-size 601 --latancies-ns "100000"

popd