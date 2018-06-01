# Builds QFlex Timing.
FROM ubuntu:16.04

# Author
MAINTAINER Mark Sutherland <mark.sutherland@epfl.ch>

# Update app repository list and install packages
RUN apt-get update -qq && apt-get install -y apt-utils sudo

# Need to sed for the dep-sources
RUN sudo sed -i.bak 's/#\s*deb-src\(.*xenial-security\suniverse\)/deb-src\1/g' /etc/apt/sources.list && sudo sed -i.bak 's/#\s*deb-src\(.*xenial-security\smain.*\)/deb-src\1/g' /etc/apt/sources.list

# Update app repository list and install packages
RUN sudo apt-get update -qq && \
    apt-get install -y build-essential checkinstall wget python-dev software-properties-common pkg-config zip zlib1g-dev unzip libbz2-dev libtool python-software-properties git-core vim ctags && \
    apt-get --no-install-recommends -y build-dep qemu

# Install git-lfs
ADD https://github.com/git-lfs/git-lfs/releases/download/v2.3.1/git-lfs-linux-amd64-2.3.1.tar.gz /root/git-lfs-linux-amd64-2.3.1.tgz
RUN cd /root && tar xzf /root/git-lfs-linux-amd64-2.3.1.tgz && cd git-lfs-2.3.1 && ./install.sh && git lfs install

# Install gcc-5 and set alternatives
ENV GCC_VERSION "5"
RUN add-apt-repository -y ppa:ubuntu-toolchain-r/test && apt-get update && apt-get -y install gcc-${GCC_VERSION} g++-${GCC_VERSION}

# Uncomment the following if GCC-5 is not automatically getting installed (it was for me)
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${GCC_VERSION} 20 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-${GCC_VERSION} 20 && \
    update-alternatives --config gcc && \
    update-alternatives --config g++

# Compile Boost 1.59.0
ENV BOOST "boost_1_59_0"
ENV BOOST_VERSION "1.59.0"
RUN wget http://sourceforge.net/projects/boost/files/boost/${BOOST_VERSION}/${BOOST}.tar.bz2 -O /tmp/${BOOST}.tar.bz2
RUN tar xjf /tmp/${BOOST:-boost_1_59_0}.tar.bz2
WORKDIR ./${BOOST:-boost_1_59_0}
RUN ./bootstrap.sh --prefix=/usr/local && ./b2 -j8 && ./b2 install
WORKDIR /

# Get Qflex Code
RUN git clone --branch dev https://github.com/parsa-epfl/qflex.git && \
    cd qflex && \
    sed -i.bak 's#git@github.com:#https:\/\/github.com\/#g' .gitmodules && \
    git submodule update --init --recursive && \
    cd flexus && git checkout arm-core && \
    cd ../qemu && git checkout timing && \
    cd ../libqflex && git checkout timing

#git lfs in its own layer
RUN curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.deb.sh | sudo bash && \
    cd qflex/images && \
    git lfs install && \
    git checkout install-docker-update && \
    git lfs fetch && \
    git lfs pull

#Build QEMU and Flexus
ENV CFLAGS "-fPIC"
RUN cd qflex/qemu && \
    ./configure --target-list=aarch64-softmmu --enable-flexus --disable-werror --disable-tpm && \
    make -j && \
    cd ../flexus && \
    make -j "KnottyKraken" && \
    make stat-manager

ENTRYPOINT /bin/bash
