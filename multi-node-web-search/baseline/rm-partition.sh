COMMON_ARGS=$(cat ./qflex.multinode-web.args)

pushd ../..

echo ${COMMON_ARGS} | xargs -o ./qflex partition-cleanup --syncs "false"

popd