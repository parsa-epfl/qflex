
pushd ..
MOUNTING_FOLDER=$1
IMAGE_FOLDER="$1/images"

xargs -a ./issue_59/qflex.args -- ./qflex boot --mounting-folder ${MOUNTING_FOLDER} --image-folder ${IMAGE_FOLDER}

popd