COMMON_ARGS=$(cat ./qflex.multinode-web.args)

pushd ../..

echo ${COMMON_ARGS} | xargs -o ./qflex fw --syncs "false" --loadvm-name init_warmed --sample-size 600

popd