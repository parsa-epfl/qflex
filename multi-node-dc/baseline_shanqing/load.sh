COMMON_ARGS=$(cat ./qflex.multinode-web.args)

pushd ../..

echo ${COMMON_ARGS} | xargs -o ./qflex load --syncs "false" --loadvm-name $1

popd