COMMON_ARGS=$(cat ./qflex.multinode-web.args)

NODE1_ARGS=$(cat ./qflex.node.1.args)

pushd .. 

echo ${COMMON_ARGS} ${NODE1_ARGS} | xargs -o ./qflex unpartition --syncs "true"  --latancies-ns "1000000"

popd