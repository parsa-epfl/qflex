pushd ..

parent=$(realpath $PWD/..)
docker run -it --entrypoint /bin/bash \
    -v $parent/experiments/:/home/dev/experiments \
    -v $parent/root.qcow2:/home/dev/root.qcow2 \
    -v $parent/qflex/micro_scripts:/home/dev/qflex/micro_scripts \
    -v $parent/exp02:/home/dev/exp02 \
    -v $parent/exp01:/home/dev/exp01 \
    -w /home/dev/qflex/micro_scripts \
    --security-opt seccomp=unconfined \
    --cap-add SYS_PTRACE \
    ghcr.io/parsa-epfl/qflex:qflex-worm-release-3.2.1


popd

 