


COMMON_ARGS=$(cat ../args/qflex.multinode-web.args)

NODE_ARGS0=$(cat ../args/qflex.node.0.args)
NODE_ARGS1=$(cat ../args/qflex.node.1.args)



tmux select-window -t 0
pushd ../../../


echo ${COMMON_ARGS} ${NODE_ARGS0} | xargs -o ./qflex boot --syncs "false"


tmux select-window -t 1
pushd ../../../

echo ${COMMON_ARGS} ${NODE_ARGS1} | xargs -o ./qflex boot --syncs "false"

popd

tmux select-window -t 0
popd