pushd .. 

# Initial sample size, for now it's does not affect run_partitions phase
xargs -a ./micro_scripts/qflex.args -- ./qflex run-partition --sample-size 30 --warming-ratio $1 --measurement-ratio $2

popd
