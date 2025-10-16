pushd ..

parent=$(realpath $PWD/..)
docker run -it --entrypoint /bin/bash \
    -v $parent/experiments/:home/dev/experiments \
    -v $parent/root.qcow2:home/dev/root.qcow2 \
    -v $parent/qflex:home/dev/qflex \
    -w /home/dev/qflex/micro_scripts \
    --security-opt seccomp=unconfined \
    --cap-add SYS_PTRACE \
    ghcr.io/parsa-epfl/qflex:qflex-worm-release-3.2.1


popd

 