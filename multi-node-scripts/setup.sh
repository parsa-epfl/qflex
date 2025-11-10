# TODO Bring this into python to be taken care of per node?
ip link add name br0 type bridge
ip link set br0 up
ip tuntap add dev tap0 mode tap user $(whoami)
ip tuntap add dev tap1 mode tap user $(whoami)
ip link set tap0 master br0
ip link set tap1 master br0
ip link set tap0 up
ip link set tap1 up


ip link show br0
bridge link show br0
