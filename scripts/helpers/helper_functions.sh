#!/bin/bash
# QFlex consists of several software components that are governed by various
# licensing terms, in addition to software that was developed internally.
# Anyone interested in using QFlex needs to fully understand and abide by the
# licenses governing all the software components.
# 
##### Software developed externally (not by the QFlex group)
# 
#     * [NS-3](https://www.gnu.org/copyleft/gpl.html)
#     * [QEMU](http://wiki.qemu.org/License)
#     * [SimFlex] (http://parsa.epfl.ch/simflex/)
# 
##### Software developed internally (by the QFlex group)
# **QFlex License**
# 
# QFlex
# Copyright &copy; 2016, Parallel Systems Architecture Lab, EPFL
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright notice,
#       this list of conditions and the following disclaimer in the documentation
#       and/or other materials provided with the distribution.
#     * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
#       nor the names of its contributors may be used to endorse or promote
#       products derived from this software without specific prior written
#       permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
# EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.#

#################################################################################
#                                                                               #
#       This is the script used to declare helper functions.                    #
#      	which will be used in the QEMU run and configure scripts.               #
#                                                                               #
#################################################################################

# Font Colors
PURPLE='\033[0;35m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

ERROR_PRINT=${RED}ERROR:${NC}

# Function to set a value to a variable
set_variable(){
    declare PARA="$1"
    declare PARA_VALUE=${!PARA}
    declare PASSED_VALUE="$2"
    if [ ! -z $PARA_VALUE ]; then
        printf "[  ${PURPLE}VARIABLE  ${NC}] $PARA = ${PARA_VALUE}\n"
    else
        if [ ! -z $PASSED_VALUE ]; then
            eval $PARA=$PASSED_VALUE
            printf "[  ${PURPLE}VARIABLE  ${NC}] $PARA = ${PASSED_VALUE}\n"
        else
            printf "[  ${YELLOW}WARNING  ${NC}] $PARA set to null value\n"
        fi
    fi
}

# Functions for Disk Configuration
Disk_Config() {
    echo Disk configured for $1
    IMG="IMG_$1"
    DISK_CONFIG="-global virtio-blk-device.scsi=off\
    -device virtio-scsi-device,id=scsi
    -drive file=${!IMG},id=rootimg,cache=unsafe,if=none 
    -device scsi-hd,drive=rootimg"
}

# Functions for Network Configuration
Network_User() {
    NETWORK="$1"
    if [ "$NETWORK" = "FALSE" ]; then
        NETWORK_CONFIG=""
        return 0
    fi

    MAC="52:54:00:00:00:00"
    echo Network Configured for NETWORK_USER
    NETWORK_CONFIG="-netdev user,id=net1,hostfwd=tcp::2220-:22 \
    -device virtio-net-device,mac=${MAC},netdev=net1"
}

Network_Tap() {
    NETWORK="$1"
    if [ "$NETWORK" = "FALSE" ]; then
        NETWORK_CONFIG=""
        return 0
    fi

    TAP_NAME="tap_qemu_$2"
    MAC_USER_NIC="52:54:00:00:02:1$2"
    MAC_TAP_NIC="52:54:00:00:02:2$2"
    SSH_PORT="222$2"
    USER_NAME="$3"
    echo Network Configured for NETWORK_TAP$2
    NETWORK_CONFIG="-netdev user,id=net1,hostfwd=tcp::${SSH_PORT}-:22 \
    -device virtio-net-device,mac=${MAC_USER_NIC},netdev=net1
    -netdev tap,id=net0,ifname=${TAP_NAME},script=no,downscript=no \
    -device virtio-net-device,mac=${MAC_TAP_NIC},netdev=net0 \
    -runas ${USER_NAME}"
}

