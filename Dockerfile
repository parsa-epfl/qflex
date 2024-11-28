# syntax=docker/dockerfile:1.4

# First Stage - Build environement
FROM ubuntu:24.04 as build

ENV DEBIAN_FRONTEND=noninteractive

ARG MODE=release

# Update the package list and install prerequisites
# Pooria Poorsarvi Tehrani: TODO might not need all of this for installation
RUN apt update && apt upgrade -y && apt install -y software-properties-common build-essential

# Pooria Poorsarvi Tehrani: TODO maybe move to even a newer version of python
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
                    python3 python3-venv python3-pip python3-setuptools python3-wheel   \
                    && rm -rf /var/lib/apt/lists/*                              \
                    && rm -rf /usr/lib/python3/dist-packages/distro*

RUN pip install --break-system-package conan && pip cache purge

COPY --link . /qflex-src
WORKDIR /qflex-src

ENV CC=/usr/bin/gcc-14
ENV CXX=/usr/bin/g++-14

RUN conan profile detect \
    && mkdir /qflex

RUN conan build flexus -pr flexus/target/_profile/${MODE} --name=keenkraken -of /qflex/out -b missing \
    && conan build flexus -pr flexus/target/_profile/${MODE} --name=knottykraken -of /qflex/out -b missing

RUN conan export-pkg flexus -pr flexus/target/_profile/${MODE} --name=keenkraken -of /qflex/out \
    && conan export-pkg flexus -pr flexus/target/_profile/${MODE} --name=knottykraken -of /qflex/out

RUN conan cache clean -v \
    && conan remove -c "*"

RUN ./build cq ${MODE} \
    && cp -rv qemu/build/ /qflex/ \
    && ln -s /qflex/qemu/build/aarch64-softmmu/qemu-system-aarch64 /qflex/qemu-aarch64

WORKDIR /qflex
RUN cp /qflex-src/runq /qflex/
RUN rm -rf /qflex/out/*kraken && rm -rf /qflex-src

# Second Stage - Run environement
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive

COPY --from=build /qflex /qflex
RUN apt update && apt install -y --no-install-recommends libcapstone4 libslirp0 zstd python3

WORKDIR /qflex
CMD ["bash"]
