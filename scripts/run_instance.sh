#!/bin/bash -xv
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
#       This is the script used to run the QEMU instance.                       #
#                                                                               #
#################################################################################

# Font colors
RED='\033[0;31m'
NC='\033[0m' # No Color

# Getting the Absolute Path to the script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Default Run time Variables
USER_FILE="user.cfg"
NETWORK="TRUE"
LOAD_OPTION=""
DEP_FILE=""
TYPE="single"
RUN_FOLDER=`pwd`
QUANTUM=0

# This function is called on in ERROR state
usage() {
    echo -e "\nUsage: $0 "
    echo -e "use -mult (--multiple) option to set up multiple instances (default: single instance)"
    echo -e "use -lo=snapshot, where snapshot is name of snapshot (default: boot)"
    echo -e "To use with Flexus trace add the -tr option"
    echo -e "To use single instance without user network use --no_net option"
    echo -e "To run multiple instances without NS3 use --no_ns3 opton"
    echo -e "To kill the qemu instances after the automated run add the --kill option"
    echo -e "You can use the -sf option to manually set the SIMULATE_TIME for Flexus (default $SIMULATE_TIME)"
    echo -e "Use the -uf option to specify the user file. e.g. -uf=user1.cfg to use user1.cfg (default: user.cfg)"
    echo -e "To name your log directory use the -exp=\"name\" or --experiment=\"name\". (default name: run)"
    echo -e "Use the -ow option to overwrite in an existing log directory"
    echo -e "Use the -sn option to take a snapshot with specified name"
    echo -e "Use the -set_quantum option to set a limit for the number of instructions executed per turn by each cpu."
    echo -e "Use the -h option for help"
}

# Parse the dynamic options
for i in "$@"
do
    case $i in
        -mult|--multiple)
        TYPE="multiple"
        shift
        ;;
        -lo=*|--load_option=*)
        LOAD_OPTION="${i#*=}"
        shift
        ;;
        -df=*|--dep_file=*)
        DEP_FILE="${i#*=}"
        shift
        ;;
        -num=*|--run_number=*)
        RUN_NUMBER="${i#*=}"
        shift
        ;;
        -tr|--trace)
        TRACE="TRUE"
        shift
        ;;
        --no_net)
        NETWORK="FALSE"
        shift
        ;;
        --no_ns3)
        shift
        ;;
        --kill)
        shift
        ;;
        -ow|--overwrite)
        shift
        ;;
        -exp=*|--experiment=*)
        shift
        ;;
        -sf=*|--simulatefor=*)
        SIMULATE_TIME="${i#*=}"
        shift
        ;;
        -uf=*|--user_file=*)
        USER_FILE="${i#*=}"
        shift
        ;;
        -sn=*|--snapshot=*)
        SNAPSHOT_NAME="${i#*=}"
        shift
        ;;
        -rs|--remove_snapshot)
        shift
        ;;
        -sq=*|--set_quantum=*)
        QUANTUM="${i#*=}"
        shift
        ;;
        -h|--help)
        usage
        exit
        shift
        ;;
        *)
        echo "$0 : what do you mean by $i ?"
        usage
        exit
        ;;
    esac
done

# Calling my configuration files
if [ ! -f $DIR/${USER_FILE} ]; then
    printf "$0:$LINENO: $ERROR_PRINT $DIR/$USER_FILE does not exist!\n"
    exit 1
else
    source $DIR/$USER_FILE
fi

# Configures Disk and Network ( RUN_CFG is set for server and client to run in sudo mode, kept null for single instance )
if [ "${TYPE}" = "single" ]; then
    Disk_Config 0
    Network_User $NETWORK
    echo "Running Single Instance"

elif [ "${TYPE}" = "multiple" ]; then
    RUN_CFG="sudo PATH=$ADD_TO_PATH:$PATH LD_LIBRARY_PATH=$ADD_TO_LD_LIBRARY_PATH:$LD_LIBRARY_PATH"
    Disk_Config $RUN_NUMBER
    Network_Tap $NETWORK $RUN_NUMBER $USER_NAME
    echo "Running Multiple Instance Server"
else
    usage
    exit 1
fi

# Evaluates Load Option
# FIXME: to be updated when the new snapshot mechanism is ready
if [ -z "${LOAD_OPTION}" ]; then
    LOAD_OPTS=""
else
    LOAD_OPTS="${LOAD_OPTION} ${DEP_FILE}"
fi

# Evaluates set quantum Option
if [ "${QUANTUM}" = "0" ]; then
    QUANTUM_OPT="-set_quantum 1024"
else
    QUANTUM_OPT="-set_quantum ${QUANTUM}"
fi

# Checks for Trace
if [ "${TRACE}" = "TRUE" ]; then
    if [ -f $RUN_FOLDER/preload_system_width ]; then
        printf "$0:$LINENO: $ERROR_PRINT File $RUN_FOLDER/preload_system_width exists!\n"
        exit 1
    fi

    echo Configuring the number of simulate cores: $QEMU_CORE_NUM in QEMU, $FLEXUS_CORE_NUM in Flexus.
    if [ "$QEMU_CORE_NUM" -lt "$FLEXUS_CORE_NUM" ]; then
        printf "$0:$LINENO: $ERROR_PRINT The number of the cores simulated cannot be greater "
        printf "than the number of cores emulated in QEMU.\n"
        exit 1
    fi
    echo Creating a configuration file for Flexus.
    echo $FLEXUS_CORE_NUM >> $RUN_FOLDER/preload_system_width

    if [ ! -f $RUN_FOLDER/user_postload ]; then
        echo -e "\n$0:$LINENO: WARNING: No user_postload file found in the run folder: $RUN_FOLDER"
        echo Using the default one from the repo...
        cp $FLEXUS_REPO/scripts/config/user_postload $RUN_FOLDER
    fi

    echo "Using Trace, simulating for $SIMULATE_TIME"
    ICOUNT_CONFIG=" -icount shift=2,sleep=off"
    echo "Using icount= $ICOUNT_CONFIG"
    if [ ! -f $FLEXUS_PATH ]; then
        printf "$0:$LINENO:$ERROR_PRINT $FLEXUS_PATH does not exist!\n"
        exit 1
    fi

    FLEXUS="-simpath $FLEXUS_PATH  -startsimulation -simulatefor $SIMULATE_TIME"
fi

# Evaluates QMP Option
if [ ! -z "${SNAPSHOT_NAME}" ]; then
    QMP="-qmp unix:/tmp/qmp-sock,server,nowait"
else
    QMP=""
fi

echo -e "\n---Starting QEMU---\n" 

set -x

# Commands to start QEMU instance
if [ ! -z "${LOAD_OPTION}" ]; then
    $RUN_CFG $QEMU_PATH/qemu-system-aarch64 \
        -machine virt \
        -cpu cortex-a57 \
        -smp $QEMU_CORE_NUM \
        -m $MEM \
        -kernel ${KERNEL_PATH}/${KERNEL} \
        -append "console=ttyAMA0 root=/dev/sda2" \
        -initrd ${KERNEL_PATH}/${INITRD} \
        -nographic \
        -rtc driftfix=slew \
        $NETWORK_CONFIG \
        $DISK_CONFIG \
        -loadvm "$LOAD_OPTS" \
        $ICOUNT_CONFIG \
        $FLEXUS \
        $QUANTUM_OPT \
        $QMP
else # FIXME: code replication to be removed when the new snapshot mechanism is ready 
     # (testing: loadvm and " marks interpretation)
    $RUN_CFG $QEMU_PATH/qemu-system-aarch64 \
        -machine virt \
        -cpu cortex-a57 \
        -smp $QEMU_CORE_NUM \
        -m $MEM \
        -kernel ${KERNEL_PATH}/${KERNEL} \
        -append "console=ttyAMA0 root=/dev/sda2" \
        -initrd ${KERNEL_PATH}/${INITRD} \
        -nographic \
        -rtc driftfix=slew \
        $NETWORK_CONFIG \
        $DISK_CONFIG \
        $ICOUNT_CONFIG \
        $FLEXUS \
        $QUANTUM_OPT \
        $QMP
fi

if [ "${TRACE}" = "TRUE" ]; then
    set +x
    echo Cleaning...
    rm $RUN_FOLDER/preload_system_width
    echo Done!
fi
