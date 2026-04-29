COMMON_ARGS=$(cat ./qflex.multinode-web.args)

NODE0_ARGS=$(cat ./qflex.node.0.args)

pushd .. 

echo ${COMMON_ARGS} ${NODE0_ARGS} | xargs -o ./qflex unpartition --syncs "true" --latancies-ns "1000000"

popd