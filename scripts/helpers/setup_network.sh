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
#       This is the script to create Bridges and setup the TAPs.                #
#                                                                               #
#################################################################################

# Define Base Mac Address
BASE_MAC="52:54:00:00"

# Function to setup a Tap instance
setup_tap() {
    if [ "$2" = 0 ]; then
        TAP_NAME="tap_qemu_$1"
        if [ "$1" -lt 10 ]; then
            MAC_ADDRESS="$BASE_MAC:10:0$1"
        else
            MAC_ADDRESS="$BASE_MAC:10:$1"
        fi
    else
        TAP_NAME="tap_ns3_$1"
        if [ "$1" -lt 10 ]; then
            MAC_ADDRESS="$BASE_MAC:20:0$1"
        else
            MAC_ADDRESS="$BASE_MAC:20:$1"
        fi
    fi
    echo "Setting up Tap $TAP_NAME"
    sudo tunctl -t "$TAP_NAME" -u `whoami`
    echo "[ OK ] Tap Created "
    sudo ifconfig "$TAP_NAME" hw ether "$MAC_ADDRESS"
    echo "[ OK ] MAC Address defined as $MAC_ADDRESS "
    sudo ifconfig "$TAP_NAME" promisc up
    echo "[ OK ] Tap Up "
}

# Function to set up a Bridge
# setup_bridge bridge_name tap_left tap_right ip mask mac
setup_bridge() {
    BRIDGE_NAME="qemubr_$1"
    TAP1_NAME="tap_qemu_$1"
    MASK=255.255.255.0
    IP_ADDRESS="192.168.2.$1"
    if [ "$1" -lt 10 ]; then
        MAC_ADDRESS="$BASE_MAC:01:0$1"
    else
        MAC_ADDRESS="$BASE_MAC:01:$1"
    fi
    if [ "$2" = 0 ]; then
        TAP2_NAME="tap_qemu_1"
    else
        TAP2_NAME="tap_ns3_$1"
    fi
    echo "Setting up bridge $BRIDGE_NAME with Taps $TAP1_NAME and $TAP2_NAME"
    sudo brctl addbr "$BRIDGE_NAME"
    echo "[ OK ] Bridge Created "
    sudo brctl addif "$BRIDGE_NAME" "$TAP1_NAME" "$TAP2_NAME"
    echo "[ OK ] Taps Added "
    sudo ifconfig "$BRIDGE_NAME" hw ether "$MAC_ADDRESS"
    echo "[ OK ] MAC Address defined as $MAC_ADDRESS"
    sudo ifconfig "$BRIDGE_NAME" "$IP_ADDRESS" netmask "$MASK" up
    echo "[ OK ] Bridge Up with IP Address $IP_ADDRESS"
}

# Parse the dynamic options
for i in "$@"
do
    case $i in
        -num=*|--number=*)
        NUMBER="${i#*=}"
        NS3="TRUE"
        shift
        ;;
        --no_ns3)
        NS3="FALSE"
        shift
        ;;
        *)
        echo "$0 : What do you mean by $i ?"
        usage
        exit
        ;;
    esac
done

echo -e "\n*** Removing Existing Taps and Bridges"
source $DIR/helpers/remove_network.sh

echo -e "*** Creating New Taps and Bridges"

if [ "$NS3" = "TRUE" ]; then
    # Setup Taps and Bridges
    i=0
    while [ "$i" -lt "$NUMBER" ]
    do
        # Setup QEMU Taps
        setup_tap "$i" 0 >> $LOG

        # Setup NS3 Taps
        setup_tap "$i" 1 >> $LOG

        # Setup Bridges
        setup_bridge "$i" 1 >> $LOG

        i=$[$i+1]
    done

else
    # Setup Taps and Bridges
    # Setup QEMU Tap 1
    setup_tap 0 0 >> $LOG

    # Setup QEMU Tap 2
    setup_tap 1 0 >> $LOG

    # Setup Bridge
    setup_bridge 0 0 >> $LOG
fi

#sudo service network restart
sudo /etc/init.d/networking restart

# Time is required for the Taps and bridges to be set up
echo Linking Taps and Bridges...
echo -e "\n *** Linking Taps and Bridges ***" >> $LOG
sleep 5
