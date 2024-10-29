# syntax=docker/dockerfile:1.4

# Pooria Poorsarvi Tehrani: TODO change the base to something way lighter
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive


ARG DEBUG_MODE=release

# Update the package list and install prerequisites
# Pooria Poorsarvi Tehrani: TODO might not need all of this for installation
RUN apt update && apt upgrade -y && apt install -y software-properties-common build-essential

# Pooria Poorsarvi Tehrani: TODO maybe move to even a newer version of python
# Add the deadsnakes PPA for Python 3.10
RUN add-apt-repository ppa:deadsnakes/ppa -y
RUN add-apt-repository ppa:ubuntu-toolchain-r/test -y

RUN apt update && apt install -y --no-install-recommends wget \
                    curl                \
                    git                 \
                    gdb
                    RUN \
                    libcapstone-dev     \
                    libslirp-dev        \
                    libglib2.0-dev      \
                    ninja-build         \
                    cmake               \
                    meson               \
                    gcc-14 g++-14       \
                    python3.10 python3.10-venv python3.10-dev python3.10-distutils python3-apt \
                    && rm -rf /var/lib/apt/lists/*                                             \
                    && rm -rf /usr/lib/python3/dist-packages/distro*

RUN curl -sS https://bootstrap.pypa.io/get-pip.py | python3.10 \
    && python3.10 -m pip install --upgrade pip         \
                                        setuptools  \
                                        wheel       \
                                        conan       \
    && python3.10 -m pip cache purge

# So weird that alternatives can mess up distro
RUN update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.10 1 \
    && update-alternatives --install /usr/bin/python python /usr/bin/python3.10 1

ENV CC=/usr/bin/gcc-14
ENV CXX=/usr/bin/g++-14

COPY . /qflex-src
WORKDIR /qflex-src

RUN conan profile detect \
    && conan build flexus -pr flexus/target/_profile/${DEBUG_MODE} --name=keenkraken -of /qflex/out -b missing \
    && conan build flexus -pr flexus/target/_profile/${DEBUG_MODE} --name=knottykraken -of /qflex/out -b missing \
    && conan export-pkg flexus -pr flexus/target/_profile/${DEBUG_MODE} --name=keenkraken -of /qflex/out \
    && conan export-pkg flexus -pr flexus/target/_profile/${DEBUG_MODE} --name=knottykraken -of /qflex/out \
    && conan cache clean -v \
    && conan remove -c "*"  \
    && ./build cq ${DEBUG_MODE} \
    && cp -v qemu/build/aarch64-softmmu/qemu-system-aarch64 /qflex/qemu-aarch64

WORKDIR /qflex
COPY /qflex-src/runq /qflex
RUN rm -rf /qflex/out/*kraken && rm -rf /qflex-src

CMD ["bash"]
