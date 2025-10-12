pushd .. 

# Initial sample size, for now it's does not affect init warm phase
xargs -a ./micro_scripts/qflex.micro.args -- ./qflex initialize --loadvm-name $1 --sample-size 30

popd
