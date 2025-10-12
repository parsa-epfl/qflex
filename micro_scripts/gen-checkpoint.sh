pushd .. 

xargs -a ./micro_scripts/qflex.args -- ./qflex fw --loadvm-name init_warmed --sample-size $1

popd
