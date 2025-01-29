# syntax=docker/dockerfile:1.4

# First Stage - Build environement
FROM ubuntu:24.04 as build

ENV DEBIAN_FRONTEND=noninteractive
ENV CC=/usr/bin/gcc-14
ENV CXX=/usr/bin/g++-14

ARG MODE=release

# Update the package list and install prerequisites
RUN apt update && apt upgrade -y && apt install -y software-properties-common build-essential
RUN add-apt-repository ppa:ubuntu-toolchain-r/test -y

RUN apt update && apt install -y --no-install-recommends wget \
                    curl                \
                    git                 \
                    gdb                 \
                    libcapstone-dev     \
                    libzstd-dev         \
                    libslirp-dev        \
                    libglib2.0-dev      \
                    ninja-build         \
                    cmake               \
                    meson               \
                    gcc-14 g++-14       \
                    libpixman-1-dev     \
                    python3 python3-venv python3-pip python3-setuptools python3-wheel   \
                    && rm -rf /var/lib/apt/lists/*                              \
                    && rm -rf /usr/lib/python3/dist-packages/distro*

RUN pip install --break-system-package conan && pip cache purge

# Copy local dir to container
COPY --link . /qflex
WORKDIR /qflex

# Build QFlex
RUN conan profile detect --force

RUN conan build flexus -pr flexus/target/_profile/${MODE} --name=knottykraken -of /qflex/out -b missing
RUN conan build flexus -pr flexus/target/_profile/${MODE} --name=semikraken -of /qflex/out -b missing
RUN conan export-pkg flexus -pr flexus/target/_profile/${MODE} --name=knottykraken -of /qflex/out
RUN conan export-pkg flexus -pr flexus/target/_profile/${MODE} --name=semikraken -of /qflex/out

RUN conan cache clean -v \
    && conan remove -c "*"

RUN ./build cq ${MODE}


# Post-build file link
RUN ln -s /qflex/qemu/build/aarch64-softmmu/qemu-system-aarch64 /qflex/qemu-aarch64

CMD ["bash"]
