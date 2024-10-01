# syntax=docker/dockerfile:1.4

# Pooria Poorsarvi Tehrani: TODO change the base to something way lighter
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive


ARG DEBUG_MODE=

# Update the package list and install prerequisites
# Pooria Poorsarvi Tehrani: TODO might not need all of this for installation
RUN apt-get update && apt-get install -y \
    software-properties-common \
    build-essential \
    libssl-dev \
    zlib1g-dev \
    libbz2-dev \
    libreadline-dev \
    libsqlite3-dev \
    wget \
    curl \
    llvm \
    libncurses5-dev \
    libncursesw5-dev \
    xz-utils \
    tk-dev \
    libffi-dev \
    liblzma-dev \
    lzma \
    lzma-dev \
    python3-apt

# Pooria Poorsarvi Tehrani: TODO maybe move to even a newer version of python
# Add the deadsnakes PPA for Python 3.10
RUN add-apt-repository ppa:deadsnakes/ppa -y

RUN apt-get update && apt-get install -y python3.10 python3.10-venv python3.10-dev python3.10-distutils
RUN curl -sS https://bootstrap.pypa.io/get-pip.py | python3.10




RUN add-apt-repository ppa:ubuntu-toolchain-r/test -y

RUN python3.10 -m pip install --upgrade pip setuptools wheel

# So weird that alternatives can mess up distro
RUN update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.10 1
RUN update-alternatives --install /usr/bin/python python /usr/bin/python3.10 1

RUN apt-get update -y
# Pooria Poorsarvi Tehrani: TODO check if we should move to 14
RUN apt-get install gcc-14 g++-14 -y
ENV CC=/usr/bin/gcc-14
ENV CXX=/usr/bin/g++-14

RUN apt-get install cmake -y
RUN rm -rf /usr/lib/python3/dist-packages/distro*


RUN pip install conan
RUN pip install meson

RUN apt-get install libglib2.0-dev -y
RUN apt install ninja-build -y

RUN conan profile detect


RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y git && \ 
    apt-get install -y gdb && \
    apt-get install -y libcapstone-dev

RUN pip install capstone


WORKDIR /qflex

RUN wget https://github.com/parsa-epfl/qflex/releases/download/2024.08-next/images.tar.xz
RUN tar -xvf images.tar.xz

COPY --link . /qflex

RUN  rm -f qemu-aarch64


RUN ./build cq ${DEBUG_MODE}

RUN ./build keenkraken
# Pooria Poorsarvi Tehrani: TODO knottykraken has not been tested with this
RUN ./build knottykraken ${DEBUG_MODE}

RUN ln -s qemu/build/aarch64-softmmu/qemu-system-aarch64 qemu-aarch64





CMD ["sh"]