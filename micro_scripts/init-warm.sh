pushd .. 


if [ $# -eq 2 ] && [ "$2" = "--skip-generate-cfg" ]; then
    xargs -a ./micro_scripts/qflex.micro.args -- ./qflex initialize --loadvm-name $1 $2
else
    xargs -a ./micro_scripts/qflex.micro.args -- ./qflex initialize --loadvm-name $1
fi

popd
