pushd .. 

xargs -a ./micro_scripts/qflex.micro.args -- ./qflex run-partition --warming-ratio $1 --measurement-ratio $2

popd
