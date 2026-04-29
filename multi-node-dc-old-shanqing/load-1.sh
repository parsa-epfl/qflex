COMMON_ARGS=$(cat ./qflex.multinode-web.args)

NODE_ARGS=$(cat ./qflex.node.1.args)

pushd .. 

echo ${COMMON_ARGS} ${NODE_ARGS} | xargs -o ./qflex load --syncs "true" --loadvm-name $2 --latancies-ns $1

popd