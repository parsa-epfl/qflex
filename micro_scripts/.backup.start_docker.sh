pushd ..

docker pull ghcr.io/parsa-epfl/qflex:qflex-worm-release-3.2.1

git checkout main
git reset --hard HEAD
git pull


parent=$(realpath $PWD/..)
docker run -it --entrypoint /bin/bash \
    -v $parent/experiments/:/home/dev/experiments \
    -v $parent/root.qcow2:/home/dev/root.qcow2 \
    -v $parent/qflex/micro_scripts:/home/dev/qflex/micro_scripts \
    -v $parent/../exp02:/exp02 \
    -v $parent/../exp01:/exp01 \
    -v $parent/qflex/micro_scripts:/home/dev/qflex/micro_scripts \
    -v $parent/qflex/commands:/home/dev/qflex/commands \
    -v $parent/qflex/qflex:/home/dev/qflex/qflex \
    -w /home/dev/qflex/micro_scripts \
    --security-opt seccomp=unconfined \
    --cap-add SYS_PTRACE \
    ghcr.io/parsa-epfl/qflex:qflex-worm-release-3.2.1 -c "rm -rf /exp01/lib && mkdir -p /exp01/lib && cp /home/dev/qflex/kraken_out/* /exp01/lib && rm -rf /exp01/lib && mkdir -p /exp01/lib && cp /home/dev/qflex/kraken_out/* /exp01/lib  && bash"



popd

 