# Fix access to internet
sudo ip link set enp0s1 up
sudo dhclient enp0s1

# Install docker requirements
sudo apt update -y
sudo apt install -y ca-certificates curl
sudo install -m 0755 -d /etc/apt/keyrings
sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc

sudo tee /etc/apt/sources.list.d/docker.sources <<EOF
Types: deb
URIs: https://download.docker.com/linux/ubuntu
Suites: $(. /etc/os-release && echo "${UBUNTU_CODENAME:-$VERSION_CODENAME}")
Components: stable
Signed-By: /etc/apt/keyrings/docker.asc
EOF

sudo apt update




# Install docker
sudo apt install docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

# Test docker installation
sudo docker run hello-world



# general fixes for NIC stuff
su
apk add iproute2
su qflex

# Server NIC fixes
ip link show
ip link set enp0s2 down
ip link set enp0s2 address 52:54:00:12:34:56
ip link set enp0s2 up
ip addr add 192.168.100.1/24 dev enp0s2
ip addr show enp0s2

# Test sever:
nc -l 5555


# Client NIC fixes
ip link show
ip link set enp0s2 up
ip addr add 192.168.100.2/24 dev enp0s2

# Test client:
ping -c 2 192.168.100.1
echo 'Hello from VM2!' | nc 192.168.100.1 5555

# Alpine:
# Potentially for host (docker) if not connecting to the internet:
cp /etc/resolv.conf /tmp/resolv.conf sed 's/nameserver 10.90.36.3/# nameserver 10.90.36.3/' /tmp/resolv.conf > /etc/resolv.conf


# connect to internet:
su
ip link set eth1 up 
udhcpc -i eth1
apk add iproute2

# Server NIC fixes
su
ip link show
ip link set eth0 down
ip link set eth0 address 52:54:00:12:34:56
ip link set eth0 up
ip addr add 192.168.100.1/24 dev eth0
ip addr show eth0
su qflex

# Test server:
nc -l -p 5555


# Client NIC fixes
su
ip link show
ip link set eth0 up
ip addr add 192.168.100.2/24 dev eth0
su qflex

# Test client:
ping 192.168.100.1 -i 0.05 -c 10000000
echo 'Hello from VM2!' | nc 192.168.100.1 5555