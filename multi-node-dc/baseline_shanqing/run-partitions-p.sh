COMMON_ARGS=$(cat ./qflex.multinode-web.args)

pushd ../..

echo ${COMMON_ARGS} | xargs -o ./qflex run-single-partition --warming-ratio 2 --measurement-ratio 1 --syncs "false" --partition-number $1

popd
