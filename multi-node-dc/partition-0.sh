 
COMMON_ARGS=$(cat ./qflex.multinode-web.args)

NODE_ARGS=$(cat ./qflex.node.0.args)

pushd ..

echo ${COMMON_ARGS} ${NODE_ARGS} | xargs -o ./qflex partition --syncs "true" --partition-count 16 --latancies-ns "100000"

popd