pushd ..

docker run -it --entrypoint /bin/bash \
    -v $PWD/../experiments/:home/dev/experiments \
    -v $PWD/../root.qcow2:home/dev/root.qcow2 \
    -v $PWD/:home/dev/qflex \
    -w /home/dev/qflex/micro_scripts \
    --security-opt seccomp=unconfined \
    --cap-add SYS_PTRACE \
    ghcr.io/parsa-epfl/qflex:qflex-worm-release-3.2.1


popd

 