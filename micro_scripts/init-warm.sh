pushd .. 

xargs -a ./micro_scripts/qflex.micro.args -- ./qflex initialize --loadvm-name $1

popd
