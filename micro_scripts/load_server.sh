pushd .. 

xargs -a ./micro_scripts/qflex.micro.args -- ./qflex load --loadvm-name $1

popd
