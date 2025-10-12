pushd .. 

# Initial sample size, for now it's does not affect loading phase
xargs -a ./micro_scripts/qflex.micro.args -- ./qflex load --loadvm-name $1 --sample-size 30

popd
