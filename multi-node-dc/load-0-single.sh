
COMMON_ARGS=$(cat ./qflex.multinode-web.args)

NODE_ARGS=$(cat ./qflex.node.0.args)

pushd .. 

echo ${COMMON_ARGS} ${NODE_ARGS} | xargs -o ./qflex load --syncs "false" --loadvm-name $2 --latancies-ns $1

popd