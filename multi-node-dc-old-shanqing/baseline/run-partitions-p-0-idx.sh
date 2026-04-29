COMMON_ARGS=$(cat ./qflex.multinode-web.args)

pushd ../..

echo ${COMMON_ARGS} | xargs -o ./qflex run-idx --warming-ratio 2 --measurement-ratio 1 --syncs "false" --partition-number 0 --idx $1

popd
