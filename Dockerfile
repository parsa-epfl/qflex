# syntax=docker/dockerfile:1.17-labs

# TODO changed docker version due to mirrors being down, change back to latest when possible
# First Stage - Build environement
FROM ubuntu:22.04 AS build

ENV DEBIAN_FRONTEND=noninteractive
# TODO once mirrors are back change back to gcc-14
ENV CC=/usr/bin/gcc-13
ENV CXX=/usr/bin/g++-13


# Update the package list and install prerequisites
RUN apt update -y 
RUN apt upgrade -y 
RUN apt-get update --fix-missing -y 
RUN apt install -y --no-install-recommends software-properties-common
RUN apt install -y --no-install-recommends build-essential
RUN apt install -y --no-install-recommends gnupg
RUN apt install -y --no-install-recommends ca-certificates
RUN add-apt-repository ppa:ubuntu-toolchain-r/test -y

# Need to update after installing the previous dependencies
RUN apt-get update -y
RUN apt install -y --no-install-recommends wget 
RUN apt install -y --no-install-recommends curl
RUN apt install -y --no-install-recommends git                 
RUN apt install -y --no-install-recommends gdb                 
RUN apt install -y --no-install-recommends libcapstone-dev
RUN apt install -y --no-install-recommends libzstd-dev
RUN apt install -y --no-install-recommends libslirp-dev
RUN apt install -y --no-install-recommends libglib2.0-dev
RUN apt install -y --no-install-recommends ninja-build
RUN apt install -y --no-install-recommends cmake
RUN apt install -y --no-install-recommends meson
# TODO once mirrors are back change back to gcc-14
RUN apt install -y --no-install-recommends gcc-13 g++-13
RUN apt install -y --no-install-recommends libpixman-1-dev
RUN apt install -y --no-install-recommends python3 python3-venv python3-pip python3-setuptools python3-wheel   

# --break-system-package for ubuntu 24.04
RUN pip install conan && pip cache purge

# TODO everything before this, should be in another base image 
# Copy local dir to container
COPY --link --exclude=p-qemu --exclude=qemu --exclude=./commands --exclude=./qflex --exclude=WormCache . /qflex
WORKDIR /qflex

# Build QFlex

# TODO this needs to be removed, but we first need to remove the unused libs
ENV CFLAGS="$CFLAGS -Wno-error"

# TODO add debug mode back in, as right now the mode is not used
ARG MODE=release

WORKDIR /qflex

# TODO address the two qemu versions
RUN --mount=type=bind,source=./qemu,target=/qflex/qemu,rw conan profile detect --force && \
    conan build flexus -pr flexus/target/_profile/${MODE} --name=knottykraken -of /qflex/out -b missing && \
    conan build flexus -pr flexus/target/_profile/${MODE} --name=semikraken -of /qflex/out -b missing && \
    conan export-pkg flexus -pr flexus/target/_profile/${MODE} --name=knottykraken -of /qflex/out && \
    conan export-pkg flexus -pr flexus/target/_profile/${MODE} --name=semikraken -of /qflex/out && \
    conan cache clean -v && \
    conan remove -c "*" && \
    ./build cq ${MODE} && \
    mv /qflex/qemu/build /qflex/qemu_build
    
RUN --mount=type=bind,source=./p-qemu,target=/qflex/p-qemu,rw cd p-qemu && \
    ./configure --target-list=aarch64-softmmu --disable-gtk --enable-capstone && \
    ninja -C build && \
    mv /qflex/p-qemu/build /qflex/p-qemu_build




WORKDIR /qflex
# Post-build file link
RUN ln -s /qflex/p-qemu_build/aarch64-softmmu/qemu-system-aarch64 /qflex/qemu-aarch64
RUN ln -s /qflex/p-qemu_build/qemu-img /qflex/qemu-img

RUN pip install -r requirements.txt
COPY  ./commands /qflex/commands 
COPY ./qflex /qflex
RUN ln -s /usr/bin/python3 /usr/bin/python

# TODO this is hardcoded as typer doesn't have a way to generate completions from within docker build, as long as tool is called qflex this is ok
RUN cat /qflex/completion_docker.txt >> /root/.bashrc

CMD ["bash"]
