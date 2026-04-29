COMMON_ARGS=$(cat ./qflex.multinode-web.args)

NODE_ARGS=$(cat ./qflex.node.1.args)


pushd .. 

echo ${COMMON_ARGS} ${NODE_ARGS} | xargs -o ./qflex run-partition --warming-ratio 2 --measurement-ratio 1 --syncs "true" --latancies-ns "100000"

popd
