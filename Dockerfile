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
RUN apt install -y --no-install-recommends net-tools
RUN apt install --yes cloud-image-utils qemu-system-x86
RUN apt install -y --no-install-recommends net-tools
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
RUN apt install -y --no-install-recommends grep
# TODO once mirrors are back change back to gcc-14
RUN apt install -y --no-install-recommends gcc-13 g++-13
RUN apt install -y --no-install-recommends libpixman-1-dev
RUN apt install -y --no-install-recommends python3 python3-venv python3-pip python3-setuptools python3-wheel   
RUN apt install -y --no-install-recommends zstd
RUN apt install -y --no-install-recommends vim
RUN apt install -y --no-install-recommends tmux
RUN apt install -y --no-install-recommends htop
RUN apt-get install -y expect telnet

RUN apt-get update && apt-get install -y \
    iproute2 \
    iputils-ping \
    && rm -rf /var/lib/apt/lists/*

# perf + flame-graph toolchain. linux-tools-$(uname -r) is host-kernel-pinned and
# can't be installed at build time; the shim below makes `perf` resolve to whatever
# version-suffixed binary linux-tools-generic ships, which is good enough for the
# `perf record -F 99 -g` flow that feeds inferno. If exact-host-kernel matching is
# needed, install linux-tools-$(uname -r) from inside the running container.
RUN apt-get update && apt-get install -y --no-install-recommends \
    linux-tools-common \
    linux-tools-generic \
    linux-cloud-tools-generic \
    && rm -rf /var/lib/apt/lists/* \
    && ln -sf $(ls /usr/lib/linux-tools/*/perf | tail -n1) /usr/local/bin/perf

# Rust toolchain (system-wide) + inferno for collapsing perf samples and rendering
# flame graphs. Lives in the base so all variants — including non-worm — can profile.
ENV RUSTUP_HOME=/home/dev/rust/rustup
ENV CARGO_HOME=/home/dev/rust/cargo
ENV PATH=/home/dev/rust/cargo/bin:${PATH}
RUN curl https://sh.rustup.rs -sSf | sh -s -- -y --no-modify-path
RUN echo 'export RUSTUP_HOME=/home/dev/rust/rustup' >> /etc/bash.bashrc && \
    echo 'export CARGO_HOME=/home/dev/rust/cargo' >> /etc/bash.bashrc && \
    echo 'export PATH=/home/dev/rust/cargo/bin:$PATH' >> /etc/bash.bashrc
RUN cargo install inferno

# --break-system-package for ubuntu 24.04
RUN pip install conan && pip cache purge

# TODO everything before this, should be in another base image 
# Copy local dir to container
WORKDIR /home/dev/qflex
COPY --link --exclude=parallel-qemu --exclude=qemu --exclude=./commands --exclude=./qflex --exclude=WormCacheQFlex . /home/dev/qflex

# Build QFlex

# TODO this needs to be removed, but we first need to remove the unused libs
ENV CFLAGS="$CFLAGS -Wno-error"

# TODO add debug mode back in, as right now the mode is not used
ARG MODE=release

WORKDIR /home/dev/qflex



COPY ./requirements.txt /home/dev/qflex/requirements.txt
RUN pip install -r requirements.txt
COPY  ./commands /home/dev/qflex/commands
COPY ./typer_inputs /home/dev/qflex/typer_inputs
COPY ./qflex /home/dev/qflex
RUN ln -s /usr/bin/python3 /usr/bin/python

COPY ./QEMU_EFI.fd /home/dev/qflex/QEMU_EFI.fd

# TODO this is hardcoded as typer doesn't have a way to generate completions from within docker build, as long as tool is called qflex this is ok
RUN cat /home/dev/qflex/completion_docker.txt >> /root/.bashrc

CMD ["bash"]