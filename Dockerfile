# syntax=docker/dockerfile:1.17-labs

# Single all-in-one image (flexus/kraken + qemu + parallel-qemu + WormCacheQFlex + QPoints).
# Component stages below only COPY/bind-mount their own sources so changing one component
# (e.g. QPoints) does not invalidate the cached layers of the others. Independent stages are
# scheduled in parallel by BuildKit automatically.

# ARG must be global (before the first FROM) and re-declared in each stage that uses it,
# since ARG values (unlike ENV) do not carry over to stages built FROM another stage.
ARG MODE=release

# TODO changed docker version due to mirrors being down, change back to latest when possible
FROM ubuntu:22.04 AS toolchain

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
RUN apt install -y --no-install-recommends python3 python3-venv python3-pip python3-setuptools python3-wheel
RUN apt install -y --no-install-recommends zstd
RUN apt install -y --no-install-recommends vim
RUN apt install -y --no-install-recommends tmux
RUN apt install -y --no-install-recommends htop

# --break-system-package for ubuntu 24.04
RUN pip install conan && pip cache purge

# Upgrade pip so QPoints/setup.sh's `--break-system-packages` flag is recognized (added in pip 23.0).
RUN python3 -m pip install --upgrade pip

# Newer pip no longer vendors distlib; qemu's mkvenv.py needs a real install of it to build meson's venv.
RUN python3 -m pip install "distlib>=0.3.6"

# QPoints/gem5 deps: installed in the shared toolchain so (a) gem5.opt has its shared libs at
# runtime (the final image is `FROM toolchain`, and only the QPoints tree is copied from
# qpoints-build, not /usr/lib), and (b) QPoints/setup.sh's apt/pip lines are effectively no-ops.
RUN apt install -y --no-install-recommends bzip2 
RUN apt install -y --no-install-recommends libboost-all-dev 
RUN apt install -y --no-install-recommends libprotobuf-dev 
RUN apt install -y --no-install-recommends libsqlite3-dev 
RUN apt install -y --no-install-recommends m4 
RUN apt install -y --no-install-recommends netcat-openbsd 
RUN apt install -y --no-install-recommends pkg-config 
RUN apt install -y --no-install-recommends sshpass 
RUN apt install -y --no-install-recommends protobuf-compiler 
RUN apt install -y --no-install-recommends gdb-multiarch 
RUN apt install -y --no-install-recommends python3-jinja2 
RUN apt install -y --no-install-recommends python3-dev 
RUN apt install -y --no-install-recommends python3-six 
RUN apt install -y --no-install-recommends qemu-utils 
RUN apt install -y --no-install-recommends zlib1g-dev
# JSON library used in Gem5. Not sure why the error did not occur before, but apparently it is needed now.
RUN apt install -y --no-install-recommends nlohmann-json3-dev 
RUN python3 -m pip install "scons==3.1.2" gdown

# Rust/cargo toolchain (needed at runtime: WormCacheQFlex is compiled per-experiment, not here)
ENV RUSTUP_HOME=/home/dev/rust/rustup
ENV CARGO_HOME=/home/dev/rust/cargo
ENV PATH=/home/dev/rust/cargo/bin:${PATH}
RUN curl https://sh.rustup.rs -sSf | sh -s -- -y --no-modify-path
RUN echo 'export RUSTUP_HOME=/home/dev/rust/rustup' >> /etc/bash.bashrc && \
    echo 'export CARGO_HOME=/home/dev/rust/cargo' >> /etc/bash.bashrc && \
    echo 'export PATH=/home/dev/rust/cargo/bin:$PATH' >> /etc/bash.bashrc

WORKDIR /home/dev/qflex

# scons/gem5 build scripts invoke `python` (not `python3`); needed by qpoints-build.
RUN ln -s /usr/bin/python3 /usr/bin/python

# TODO this needs to be removed, but we first need to remove the unused libs
ENV CFLAGS="$CFLAGS -Wno-error"


# ---- flexus/kraken + qemu w/ libqflex (bind-mounted, only kraken_out + qemu-saved are persisted) ----
FROM toolchain AS flexus-qemu-build
ARG MODE
COPY --link build build-multiple-kraken_vanilla.py /home/dev/qflex/
RUN --mount=type=bind,source=./flexus,target=/home/dev/qflex/flexus,rw \
    --mount=type=bind,source=./qemu,target=/home/dev/qflex/qemu,rw \
    conan profile detect --force && \
    conan build flexus -pr flexus/target/_profile/${MODE} --name=knottykraken -of /home/dev/qflex/out -b missing && \
    conan build flexus -pr flexus/target/_profile/${MODE} --name=semikraken -of /home/dev/qflex/out -b missing && \
    conan export-pkg flexus -pr flexus/target/_profile/${MODE} --name=knottykraken -of /home/dev/qflex/out && \
    conan export-pkg flexus -pr flexus/target/_profile/${MODE} --name=semikraken -of /home/dev/qflex/out && \
    conan cache clean -v && \
    conan remove -c "*" && \
    ./build cq ${MODE} && \
    python3 build-multiple-kraken_vanilla.py && \
    mkdir /home/dev/qflex/kraken_out && \
    cp -r out/lib/Release /home/dev/qflex/kraken_out && \
    rm -rf out && \
    mkdir /home/dev/qflex/qemu-saved && \
    cp -r /home/dev/qflex/qemu/pc-bios /home/dev/qflex/qemu-saved/pc-bios && \
    cp -r /home/dev/qflex/qemu/build /home/dev/qflex/qemu-saved/build


# ---- parallel-qemu (bind-mounted, only pc-bios+build are persisted as parallel-qemu-saved) ----
FROM toolchain AS parallel-qemu-build
RUN --mount=type=bind,source=./parallel-qemu,target=/home/dev/qflex/parallel-qemu,rw \
    cd parallel-qemu && \
    ./configure --target-list=aarch64-softmmu --disable-gtk --enable-capstone && \
    ninja -C build && \
    mkdir /home/dev/qflex/parallel-qemu-saved && \
    cp -r /home/dev/qflex/parallel-qemu/pc-bios /home/dev/qflex/parallel-qemu-saved/pc-bios && \
    cp -r /home/dev/qflex/parallel-qemu/build /home/dev/qflex/parallel-qemu-saved/build


# ---- QPoints + gem5 (COPY, not bind-mount: runtime needs the full tree, not just one binary) ----
FROM toolchain AS qpoints-build
WORKDIR /home/dev/qflex
# Copy only what setup.sh needs first, to keep the (slow) gem5 build cached when other QPoints files change
COPY --link ./QPoints/setup.sh /home/dev/qflex/QPoints/setup.sh
COPY --link ./QPoints/bin /home/dev/qflex/QPoints/bin
COPY --link ./QPoints/gem5 /home/dev/qflex/QPoints/gem5
RUN bash /home/dev/qflex/QPoints/setup.sh
COPY --link ./QPoints /home/dev/qflex/QPoints


# ---- final assembly ----
FROM toolchain AS runtime
WORKDIR /home/dev/qflex

COPY --from=flexus-qemu-build /home/dev/qflex/kraken_out /home/dev/qflex/kraken_out
COPY --from=flexus-qemu-build /home/dev/qflex/qemu-saved /home/dev/qflex/qemu-saved
COPY --from=parallel-qemu-build /home/dev/qflex/parallel-qemu-saved /home/dev/qflex/parallel-qemu-saved
COPY --from=qpoints-build /home/dev/qflex/QPoints /home/dev/qflex/QPoints
COPY --link ./WormCacheQFlex /home/dev/qflex/WormCacheQFlex

# Post-build file links
RUN ln -s /home/dev/qflex/parallel-qemu-saved/build/aarch64-softmmu/qemu-system-aarch64 /home/dev/qflex/qemu-aarch64
RUN ln -s /home/dev/qflex/parallel-qemu-saved/build/qemu-img /home/dev/qflex/qemu-img

# Copy the remaining runtime files after builds to avoid invalidating build cache.
# flexus/qemu/parallel-qemu are bind-mount-only above (not persisted): mount them from the
# host at `docker run` time (see DockerStarter) if you need to rebuild them in-container.
COPY --link --exclude=flexus --exclude=qemu --exclude=parallel-qemu --exclude=QPoints --exclude=WormCacheQFlex --exclude=.venv . /home/dev/qflex

RUN pip install -r requirements.txt

# TODO this is hardcoded as typer doesn't have a way to generate completions from within docker build, as long as tool is called qflex this is ok
RUN cat /home/dev/qflex/completion_docker.txt >> /root/.bashrc

CMD ["bash"]
