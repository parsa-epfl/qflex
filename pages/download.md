---
layout: page
title: Download
sidebar: "true"
---

QFlex is a full-system cycle-accurate simulation framework of multi-node computer systems, which is composed of QEMU, Flexus, and NS-3. QEMU is a widely-used machine emulator, which is able to boot any machine and execute unmodified applications and operating systems. Flexus is a cycle-accurate tool, which simulates modern CPUs with varying core types, network-on-chip topologies, and cache organizations; and various DRAM-based memory systems. NS-3 is a popular network simulator that glues all the simulated server nodes together with different network integration characteristics. 

QFlex is still a work in progress, and at this stage, we provide limited functionality. Currently, QFlex is able perform full-system trace-based simulation of a single server node (for 64-bit ARM). Hence, no timing models are available yet.

QFlex's source code is available on our [GitHub repository]({{ site.github.repo }}).

## How to Build QFlex ##
-------------------------

Before compiling QFlex, make sure you have the required prerequisites installed. The process of installing the prerequisites for an Ubuntu distribution is described below.

- Installing the basic dependencies:

```bash
$ sudo apt-get update -qq
$ sudo apt-get install -y build-essential checkinstall wget sudo \
                          python-dev software-properties-common \
                          pkg-config zip zlib1g-dev unzip libbz2-dev \
                          libtool python-software-properties git-core

$ sudo apt-get --no-install-recommends -y build-dep qemu
```

- We use `git-lfs` to store and share QEMU images.
  Please refer to [this page](https://help.github.com/articles/installing-git-large-file-storage/#platform-linux) to install `git-lfs`.

- Installing a compatible version of `gcc`, _5_:

  **Note:** Following this process will replace your system's `gcc` with `gcc 5`.

```bash
$ export GCC_VERSION="5"
$ sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
$ sudo apt-get update
$ sudo apt-get -y install gcc-${GCC_VERSION} g++-${GCC_VERSION}
```

- Setting the recently installed version of `gcc` as your system's
default version:

```bash
$ sudo update-alternatives --install /usr/bin/gcc gcc \
                                     /usr/bin/gcc-${GCC_VERSION} 20
$ sudo update-alternatives --install /usr/bin/g++ g++ \
                                   /usr/bin/g++-${GCC_VERSION} 20
$ sudo update-alternatives --config gcc
$ sudo update-alternatives --config g++
```

- Installing a compatible version of the `boost` library, _1.59.0_ :

```bash
$ export BOOST="boost_1_59_0"
$ export BOOST_VERSION="1.59.0"
$ wget http://sourceforge.net/projects/boost/files/boost/${BOOST_VERSION}/${BOOST}.tar.bz2 -O /tmp/${BOOST}.tar.gz
$ tar -xf /tmp/${BOOST}.tar.gz
$ cd ./${BOOST}/
$ ./bootstrap.sh --prefix=/usr/local
$ ./b2 -j8
$ sudo ./b2 install
```
- Now use `git` to clone the QFlex repository, and update all its submodules
(depending on you network connection, it may take a while for this to complete):

```bash
$ cd ..
$ git clone --branch v1.0 git@github.com:ParsaLab/qflex.git
$ cd qflex
$ git submodule update --init --recursive
$ cd images
$ git lfs fetch
$ git lfs pull
```
   **Note:** Please note that the submodules are defined in SSH format, and GitHub needs your SSH key for authentication.
   You can find more information [here](https://help.github.com/articles/generating-an-ssh-key/).


- Please note that QFlex uses our modified version of QEMU at its core,
and it has already been downloaded as one of the submodules.
Here is how to build QEMU:

```bash
$ cd ../qemu
$ export CFLAGS="-fPIC"
$ ./configure --target-list=aarch64-softmmu \
              --enable-flexus --disable-werror --disable-tpm
$ make -j
```

- As the final step before compiling QFlex, we need to set the paths
for `GCC_BINARY`, `GCC_PATH`, `BOOST_PATH`, and `BOOST_BINARIES` in
`makefile.defs` to the installed dependencies.

- Now that we have everything set, we can build QFlex. 

  **Note:** For now, only the 64-bit ARM trace-based simulator is supported. The simulator features a two-level cache hierarchy, with a private per-core first-level and a shared second-level. 

```bash
$ make "QEMUCMP.L2Shared.Trace-arm"
$ make stat-manager
```

## How to Use QFlex ##
-------------------------

You can run QFlex with the aid of the scripts present in the `scripts` folder. The scripts to aid both in the deployment of simple single-instance or complex multi-instance QFlex jobs with various network configurations. For complex deployments, please refer to each script usage command, by calling the script with the `-h` flag.

Before running QFlex, you must have the following prerequisites:

- A QEMU image with an ARM 64-bit OS installed (we have tested Debian 8 and Ubuntu Server 12.04)
- Kernel and initrd images, normally extracted from the OS image.


We offer two QEMU images with Debian 8 preinstalled in this repository:

- `images/debian-blank`: This image has a clean Debian 8 instalation.
- `images/debian-memcached`: This image has Memcached installed.

Both images user name and password are:

`cloudsuite:cloudsuite`.

We also offer the Linux kernel images, extracted from the same Debian 8 builds, located in the `images/kernel` folder of this repository.

### Deploying a single node QFlex job ###

For simple deployments, `cd` into the `scripts` folder and copy the `user_example.cfg` to a new file named `user.cfg`. In the new file, configure the environment variables `QEMU_PATH`, `KERNEL_PATH`, `KERNEL`, `INITRD`, `FLEXUS_REPO`, `FLEXUS_PATH`, `IMG_0`, `ADD_TO_PATH` and `ADD_TO_LD_LIBRARY_PATH`. Depending on your QFlex installation, the `ADD_TO_PATH` and `ADD_TO_LD_LIBRARY_PATH` variables may be empty.

Once you have configured the `user.cfg` script, you can run QFlex. We strongly recommend that you boot QEMU without Flexus. Booting a machine with Flexus attached is not recommended as it greatly slows down the execution. Hence, the simulation workflow will be as follows: (1) Boot a machine without Flexus, (2) install and tune your application, (3) take a snapshot when the application reaches the desired point to start simulating, and (4) start QEMU with Flexus from the aforesaid snapshot. 

You can boot QEMU without Flexus by using the following command (from the `scripts` folder):

```bash
$ ./run_instance.sh
```

This command will boot your image. You should see the guest boot information on your terminal. Unless you are running an image with network and SSH support, this will be your interface with the guest. With QEMU running, you can take a snapshot with the following commands:

```
# On the guest terminal
Ctrl^a + C # Calls QEMU monitor
(qemu) $ savevm snapshot_name
(qemu) $ quit
```

After that, you can start QEMU from the snapshot, with Flexus attached, by using the following command:

```bash
$ ./run_instance.sh -tr -lo=snapshot_name
```

### Example: Running Memcached with QFlex ###

In this section, we will simulate a Memcache server with QFlex. We will use the provided Memcached image. The Memcached client and server will run on the same QEMU instance; client and server will communicate through the loopback network interface. Configure your `user.cfg` file, pointing it to the memcached image and configuring a boot with at least 2 cores and 4GB of memory.

The [benchmark](http://cloudsuite.ch/pages/benchmarks/datacaching/) is composed of the following components:

- `/usr/local/bin/memcached`: The Memcached server binary.
- `/home/cloudsuite/memcached/memcached_client`: The Memcached client and configuration files.

In order to run the Memcached benchmark, we recommend using a terminal multiplexer software, such as `screen`. Furthermore, as we are using a single QEMU instance for the client and the server, you should pin the client and server to different groups of cores with `taskset` . You can install `screen` using `apt-get` from the guest OS (Debian 8). `taskset` is already installed.

  **Note:** The `screen` control command `Ctrl^a + C` conflicts with the QEMU monitor's command. To get around this, create a `.screenrc` file under your home directory with `escape ^Xx` in it. This step allows using `screen` with `Ctrl^x` instead of `Ctrl^a`

The following commands assume a QEMU instance with 4GB of memory and and at least 2 cores. We require at least one core for the server and one for the client. We will start a Memcached server of 1GB, since we must have at least 4 times the memory of the Memcached server to safely run the server and the client in the same machine. We also assume you have two terminals in the guest OS, one for the server and one for the client.

Boot the QEMU instance, without Flexus attached, and start a `screen` terminal. On this first terminal, we are going to start the Memcached server and pin it to core 1, with the following command:

```bash
$ taskset 0x2 memcached -t 1 -m 1024 -n 550
```

Now you must start the client. Since the client and server have to run concurrently, start another terminal. The Memcached client must perform several tasks: generate the dataset, warmup the server, and tune and run the benchmark. The provided dataset `twitter_data/twitter_dataset_unscaled` is around 300MB. We will scale the dataset by a factor of 3 to generate a dataset of around 1GB, with the following command:

```bash
$ cd /home/cloudsuite/memcached/memcached_client
$ taskset 0x4 ./loader -a ../twitter_dataset/twitter_dataset_unscaled \
              -o ../twitter_dataset/twitter_dataset_3x -s docker_servers.txt -w 1 -S 3 -D 1024 -j -T 1
```

(`w` - number of client threads which has to be divisible to the number of servers, `S` - scaling factor, `D` - target server memory, `T` - statistics interval, `s` - server configuration file, `j` - an indicator that the server should be warmed up).

After a few minutes, you should see the following message: `You are warmed up, sir`. This message indicates that the dataset has been created.

Now, warm up the server with the following command:

```bash
$ cd /home/cloudsuite/memcached/memcached_client
$ taskset 0x4 ./loader -a ../twitter_dataset/twitter_dataset_3x \
              -s docker_servers.txt -w 1 -S 1 -D 1024 -j -T 1
```

After a few minutes, you should see the following message: `You are warmed up, sir`. This message indicates that the server is warmed up. The next step is to tune the server, which you can do with the following command:

```bash
$ cd /home/cloudsuite/memcached/memcached_client
$ taskset 0x4 ./loader -a ../twitter_dataset/twitter_dataset_3x \
              -s docker_servers.txt -g 0.8 -T 1 -c 200 -w 1 -e -r rps
```

In the command above, `rps` indicates the number of requests per second to be issued by the client. You must vary the value of `rps` until the client reports a QoS that is acceptable for your tests (the magnitude reported for QoS is milliseconds). Once you find the correct value of `rps`, you must run the client with the same command above. Wait for the client response latency to stabilize, and take a snapshot of the running benchmark with the following commands:

```
# On the guest terminal
Ctrl^a + C # Calls QEMU monitor
(qemu) $ savevm warmedup
(qemu) $ quit
```

After that, you can start QFlex in trace-mode simulation with the following command:

```bash
$ ./run_instance.sh -tr -lo=warmedup
```

Note that the micro-architectural parameters for the simulation are defined in the `scripts/config/user_postload` configuration file.

After the simulation, a `stats_db.out.gz` file is created with all the statistics of the simulated run. You can check the statistics by running the `stat-manager` binary (which we compiled before):

```bash
$ stat-manager print all
```

### Automation of QFlex ###

The process of booting a single- or multi-instance QEMU configuration, installing software, taking snapshots, and running QFlex can be automated. This automation is done by the `run_system.sh` script in the `scripts` folder. You should understand the entire process before using that script, since some debugging might be required. Please refer to the documentation in the scripts folder for information on how to use the scripts.
