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
RUN apt install -y --no-install-recommends grep
# TODO once mirrors are back change back to gcc-14
RUN apt install -y --no-install-recommends gcc-13 g++-13
RUN apt install -y --no-install-recommends libpixman-1-dev
# System Python is only used to build QEMU (its configure bootstraps a venv and needs pip's distlib).
# Everything else Python is managed by uv below.
RUN apt install -y --no-install-recommends python3 python3-venv python3-pip python3-setuptools python3-wheel   
RUN apt install -y --no-install-recommends zstd
RUN apt install -y --no-install-recommends vim
RUN apt install -y --no-install-recommends tmux
RUN apt install -y --no-install-recommends htop

# uv manages all other Python tooling: conan, and the locked environment of the qflex CLI
# (pyproject.toml / uv.lock), using a uv-managed interpreter (.python-version).
COPY --from=ghcr.io/astral-sh/uv:0.7.19 /uv /uvx /usr/local/bin/
ENV UV_PYTHON_INSTALL_DIR=/opt/uv/python \
    UV_TOOL_DIR=/opt/uv/tools \
    UV_TOOL_BIN_DIR=/usr/local/bin \
    UV_PROJECT_ENVIRONMENT=/opt/qflex-venv
RUN uv tool install conan

# Install Rust system-wide. Needed to build bxdb in this image, and reused by
# the WormCache image (which no longer installs its own toolchain).
ENV RUSTUP_HOME=/home/dev/rust/rustup
ENV CARGO_HOME=/home/dev/rust/cargo
ENV PATH=/home/dev/rust/cargo/bin:${PATH}

RUN curl https://sh.rustup.rs -sSf | sh -s -- -y --no-modify-path

RUN echo 'export RUSTUP_HOME=/home/dev/rust/rustup' >> /etc/bash.bashrc && \
    echo 'export CARGO_HOME=/home/dev/rust/cargo' >> /etc/bash.bashrc && \
    echo 'export PATH=/home/dev/rust/cargo/bin:$PATH' >> /etc/bash.bashrc

# `just` runs the shared build recipes (see ./justfile).
ARG JUST_VERSION=1.58.0
RUN curl -fsSL "https://github.com/casey/just/releases/download/${JUST_VERSION}/just-${JUST_VERSION}-x86_64-unknown-linux-musl.tar.gz" \
    | tar -xz -C /usr/local/bin just

# TODO everything before this, should be in another base image 
# Copy local dir to container
WORKDIR /home/dev/qflex
COPY --link --exclude=fw-qemu --exclude=timing-qemu --exclude=./commands --exclude=./qflex --exclude=WormCache . /home/dev/qflex

# Build bxdb (Rust) before the QEMUs. The recipes in ./justfile build bxdb
# first and point both QEMUs at it.
RUN just bxdb

# Build QFlex

# TODO this needs to be removed, but we first need to remove the unused libs
ENV CFLAGS="$CFLAGS -Wno-error"

# TODO add debug mode back in, as right now the mode is not used
ARG MODE=release

WORKDIR /home/dev/qflex


# TODO add a check later to make sure qflex folder it self is never mounted, as we need the binaries, or change where they are craeted
# TODO address the two qemu versions
RUN --mount=type=bind,source=./timing-qemu,target=/home/dev/qflex/timing-qemu,rw PROFILE=${MODE} just kraken && \
    cp -r out/kraken /home/dev/qflex/kraken_out && \
    conan cache clean -v && \
    conan remove -c "*" && \
    PROFILE=${MODE} just timing-qemu && \
    rm -rf out && \
    mkdir timing-qemu-saved && \
    cp -r /home/dev/qflex/timing-qemu/pc-bios /home/dev/qflex/timing-qemu-saved/pc-bios && \
    cp -r /home/dev/qflex/timing-qemu/build /home/dev/qflex/timing-qemu-saved/build

RUN --mount=type=bind,source=./fw-qemu,target=/home/dev/qflex/fw-qemu,rw just fw-qemu && \
    mkdir /home/dev/qflex/fw-qemu-saved && \
    cp -r /home/dev/qflex/fw-qemu/pc-bios /home/dev/qflex/fw-qemu-saved/pc-bios && \
    cp -r /home/dev/qflex/fw-qemu/build /home/dev/qflex/fw-qemu-saved/build




WORKDIR /home/dev/qflex
# Post-build file link
RUN ln -s /home/dev/qflex/fw-qemu-saved/build/aarch64-softmmu/qemu-system-aarch64 /home/dev/qflex/qemu-aarch64
RUN ln -s /home/dev/qflex/fw-qemu-saved/build/qemu-img /home/dev/qflex/qemu-img

# Install the CLI's locked dependencies. The venv comes first on PATH, so `./qflex` and `python`
# use it directly; `uv run ./qflex` also works and reuses the same venv.
RUN uv sync --frozen
ENV PATH=/opt/qflex-venv/bin:${PATH}
COPY  ./commands /home/dev/qflex/commands
COPY ./typer_inputs /home/dev/qflex/typer_inputs
COPY ./qflex /home/dev/qflex

COPY ./QEMU_EFI.fd /home/dev/qflex/QEMU_EFI.fd

# TODO this is hardcoded as typer doesn't have a way to generate completions from within docker build, as long as tool is called qflex this is ok
RUN cat /home/dev/qflex/completion_docker.txt >> /root/.bashrc

CMD ["bash"]
