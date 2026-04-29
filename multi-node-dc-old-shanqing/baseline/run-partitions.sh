COMMON_ARGS=$(cat ./qflex.multinode-web.args)

pushd ../..

echo ${COMMON_ARGS} | xargs -o ./qflex run-partition --warming-ratio 2 --measurement-ratio 1 --syncs "false"

popd
