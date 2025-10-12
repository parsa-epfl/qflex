pushd .. 

# Initial sample size, for now it's does not affect boot
xargs -a ./micro_scripts/qflex.args -- ./qflex boot --sample-size 30

popd
