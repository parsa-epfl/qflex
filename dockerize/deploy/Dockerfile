# This script helps avoid starting docker container with root privelages
FROM ubuntu:16.04 AS GOSU

COPY entrypoint.sh /usr/local/bin/entrypoint.sh 

RUN chmod 755 /usr/local/bin/entrypoint.sh && \
    apt-get update && apt-get -y --no-install-recommends install \
    ca-certificates \
    curl && \
    gpg --keyserver ha.pool.sks-keyservers.net --recv-keys B42F6819007F00F88E364FD4036A9C25BF357DD4 && \
    curl -o /usr/local/bin/gosu -SL "https://github.com/tianon/gosu/releases/download/1.4/gosu-$(dpkg --print-architecture)" \
    && curl -o /usr/local/bin/gosu.asc -SL "https://github.com/tianon/gosu/releases/download/1.4/gosu-$(dpkg --print-architecture).asc" \
    && gpg --verify /usr/local/bin/gosu.asc \
    && rm /usr/local/bin/gosu.asc \
    && chmod +x /usr/local/bin/gosu

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]

#######################################################################################################

# Development image
FROM GOSU

# Author
MAINTAINER Abhinav Aggarwal <abhinavagg96@gmail.com>

ENV GCC_VERSION "5"
ENV BOOST "boost_1_59_0"
ENV BOOST_VERSION "1.59.0"
ENV CFLAGS "-fPIC"
ADD https://github.com/git-lfs/git-lfs/releases/download/v2.3.1/git-lfs-linux-amd64-2.3.1.tar.gz /tmp/git-lfs-linux-amd64-2.3.1.tgz

# Need to sed for the dep-sources
RUN sed -i.bak 's/#\s*deb-src\(.*xenial-security\suniverse\)/deb-src\1/g' /etc/apt/sources.list && sed -i.bak 's/#\s*deb-src\(.*xenial-security\smain.*\)/deb-src\1/g' /etc/apt/sources.list && \
# Update app repository list and install packages
apt-get update -qq && apt-get install -y apt-utils vim ctags build-essential checkinstall wget python-dev software-properties-common pkg-config zip zlib1g-dev unzip libbz2-dev libtool python-software-properties git-core && apt-get --no-install-recommends -y build-dep qemu && \
# Install git-lfs
cd /tmp && tar xzf git-lfs-linux-amd64-2.3.1.tgz && cd git-lfs-2.3.1 && ./install.sh && git lfs install && \
# Install gcc-5
add-apt-repository -y ppa:ubuntu-toolchain-r/test && apt-get install -y gcc-${GCC_VERSION} g++-${GCC_VERSION} && \
# Compile Boost 1.59.0
wget http://sourceforge.net/projects/boost/files/boost/${BOOST_VERSION}/${BOOST}.tar.bz2 -O /tmp/${BOOST}.tar.bz2 && \
cd /tmp && tar xjf ${BOOST:-boost_1_59_0}.tar.bz2 && \
cd ./${BOOST:-boost_1_59_0} && \
./bootstrap.sh --prefix=/usr/local && ./b2 -j8 && ./b2 install && \
# Add library path for boost ( and any other local library installed, which good practice dictates to always install in /usr/local/lib )
if [ ! -d "/etc/ld.so.conf.d" ]; then mkdir /etc/ld.so.conf.d; fi && \
echo "/usr/local/lib" > /etc/ld.so.conf.d/local_libs.conf && ldconfig && \
# Get Qflex code
git clone --branch dev https://github.com/parsa-epfl/qflex.git /home/usr/qflex && \
cd /home/usr/qflex && \
sed -i.bak 's#git@github.com:#https:\/\/github.com\/#g' .gitmodules && \
git submodule update --init --recursive && \
# Install qemu
cd /home/usr/qflex/qemu && \
./configure --target-list=aarch64-softmmu --enable-flexus --disable-werror --disable-tpm --enable-extsnap && \
make -j && \
rm -rf /tmp/* /var/lib/apt/lists/*

CMD ["/bin/bash", "-c", "--", "while true; do sleep 30; done;"]
