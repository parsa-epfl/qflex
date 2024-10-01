# syntax=docker/dockerfile:1.4

# Pooria Poorsarvi Tehrani: TODO change the base to something way lighter
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive


ARG DEBUG_MODE=

# Update the package list and install prerequisites
# Pooria Poorsarvi Tehrani: TODO might not need all of this for installation
RUN apt-get update && apt-get install -y 
RUN apt-get install -y  software-properties-common 
RUN apt-get install -y build-essential
RUN apt-get install -y libssl-dev 
RUN apt-get install -y zlib1g-dev 
RUN apt-get install -y libbz2-dev 
RUN apt-get install -y libreadline-dev 
RUN apt-get install -y libsqlite3-dev 
RUN apt-get install -y wget 
RUN apt-get install -y curl 
RUN apt-get install -y llvm 
RUN apt-get install -y libncurses5-dev 
RUN apt-get install -y libncursesw5-dev 
RUN apt-get install -y xz-utils 
RUN apt-get install -y tk-dev 
RUN apt-get install -y libffi-dev 
RUN apt-get install -y liblzma-dev 
RUN apt-get install -y lzma 
RUN apt-get install -y lzma-dev 
RUN apt-get install -y python3-apt

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


RUN apt-get upgrade -y 
RUN apt-get install -y git  
RUN apt-get install -y gdb 
RUN apt-get install -y libcapstone-dev
RUN apt-get install -y slirp
RUN apt-get install -y libslirp0 
RUN apt-get install -y libslirp-dev

RUN pip install capstone


WORKDIR /qflex

RUN wget https://github.com/parsa-epfl/qflex/releases/latest/download/images.tar.xz
RUN tar -xvf images.tar.xz

COPY --link . /qflex

RUN  rm -f qemu-aarch64


RUN ./build cq ${DEBUG_MODE}

RUN ./build keenkraken ${DEBUG_MODE}
# Pooria Poorsarvi Tehrani: TODO knottykraken has not been tested with this
RUN ./build knottykraken ${DEBUG_MODE}

RUN ln -s qemu/build/aarch64-softmmu/qemu-system-aarch64 qemu-aarch64





CMD ["sh"]