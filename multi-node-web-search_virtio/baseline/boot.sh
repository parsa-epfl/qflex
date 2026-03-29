COMMON_ARGS=$(cat ./qflex.multinode-web.args)

pushd ../..



echo ${COMMON_ARGS} | xargs -o ./qflex boot --syncs "false"

popd