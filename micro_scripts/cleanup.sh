pushd .. 

# Initial sample size, for now it's does not affect cleanup phase
xargs -a ./micro_scripts/qflex.micro.args -- ./qflex partition-cleanup --sample-size 30

popd
