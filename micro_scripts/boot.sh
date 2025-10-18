pushd .. 

xargs -a ./micro_scripts/qflex.micro.args -- ./qflex boot

# folders "bin", "cfg", "flags", "lib", "run", "scripts", "images" are in the experiment folder, linking them for accesibility during the presentation
for folder in bin cfg flags lib run scripts images; do
    unlink ./micro_scripts/$folder > /dev/null 2>&1
    ln -s $PWD/../experiments/experiment_micro/$folder ./micro_scripts/$folder
done

popd
