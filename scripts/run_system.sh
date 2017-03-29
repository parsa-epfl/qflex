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
#       This is the script used to run and log a complete QEMU                  #
#       automated procedure.                                                    #
#                                                                               #
#################################################################################

# Getting the Absolute Path to the script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Default Run time Variables
LOG_NAME="run"
TYPE="single"
USER_FILE="user.cfg"
NETWORK="TRUE"
NS3="TRUE"
MULTIPLE_INSTANCE_NUMBER=2

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
    echo -e "Use the -h option for help"
}

check_invoke_script() {
    if [ -e "$1" ]; then
        if [ ! -z "$2" ]; then
            source "$1" "$2"
        else
            source "$1"
        fi
    else
        printf "$0:$LINENO:$ERROR_PRINT File not found $1"
        exit 1
    fi
}

check_ssh() {
    # Loop to wait for successful SSH
    FAILURE=1
    while [ "$FAILURE" != 0 ]
    do
        sleep 5
        check_invoke_script $DIR/helpers/ssh_test.sh $1 >> $LOG_QEMU
        if [ "$?" = 0 ]; then
            FAILURE=0
            continue
        fi

        FAILURE=$[$FAILURE+1]
        
        if [ "$FAILURE" = 40 ]; then
            printf "$0:$LINENO:$ERROR_PRINT QEMU Runtime ERROR cannot SSH"
            exit 1
        fi
    done
}

check_run_instance() {
    if [ -e "$DIR/run_instance.sh" ]; then
        # Check for valid user configuration file
        if [ ! -f $DIR/${USER_FILE} ]; then
            printf "$0:$LINENO: $ERROR_PRINT $DIR/$USER_FILE does not exist!\n"
            exit 1
        fi

        # Create a log folder for the instance to hold Flexus logs      
        mkdir $DIR/$LOG_NAME/Qemu_$1

        # Switch to Logging Directory for Flexus
        pushd $DIR/$LOG_NAME/Qemu_$1 >> /dev/null
        bash $RUN >> $LOG_QEMU 2>&1 &
        popd >> /dev/null
    else
        printf "$0:$LINENO:$ERROR_PRINT File not found $DIR/run_instance.sh"
        exit 1
    fi
}

check_error() {
    if [ "$1" = "0" ]; then
        echo "$2"
    else
        echo "$3"
    fi
}

PARA=$@
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
        -tr|--trace)
        TRACE="TRUE"
        shift
        ;;
        --no_net)
        NETWORK="FALSE"
        shift
        ;;
        --no_ns3)
        NS3="FALSE"
        shift
        ;;
        --kill)
        KILL_QEMU="TRUE"
        shift
        ;;
        -ow|--overwrite)
        OVERWRITE="TRUE"
        shift
        ;;
        -exp=*|--experiment=*)
        EXPERIMENT="${i#*=}"
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
        REMOVE_SNAPSHOT="TRUE"
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
        exit 1
        ;;
    esac
done

echo -e "\n *** Running run_system.sh *** "

# Configure Log files
if [ ! -z $EXPERIMENT ]; then
    LOG_NAME=$EXPERIMENT
fi

LOG="$DIR/$LOG_NAME/logs"

if [ -d $DIR/$LOG_NAME ]; then
    if [ "$OVERWRITE" = "TRUE" ]; then
        rm -r $DIR/$LOG_NAME/
    else
        printf "$0:$LINENO:$ERROR_PRINT : Log files currently exist in $LOG_NAME, exiting"
        exit 1
    fi
fi

mkdir $DIR/$LOG_NAME
echo Creating folder $DIR/$LOG_NAME

# Check for previous QEMU processes
PROCESSES=`ps aux | grep "machine virt" | grep qemu`
if [ ! -z "$PROCESSES" ]; then
    printf "$0:$LINENO:$ERROR_PRINT There are QEMU processes already running. Exiting\n"
    exit 1
fi

if [ "$TYPE" = "single" ]; then 
    LOG_QEMU="$DIR/$LOG_NAME/Qemu_0/logs"
    RUN="$DIR/run_instance.sh $PARA"
    echo -e "\nRunning Single Instance Mode : Port 2220\n$RUN\nBooting... Please wait\n"
    echo -e "\n *** Running Single Instance Mode : Port 2220 ***\n" >> $LOG
    check_run_instance 0
    if [ "$TRACE" != "TRUE" ]; then
        check_ssh 0
        echo -e "\n\n *** SSH to remote server success ***\n" >> $LOG

        # After successful SSH, execute all the commands specified for "single" instance on remote host
        echo -e " *** Running Commands on remote host ***\n" >> $LOG
        echo -e "Running Commands"
        check_invoke_script $DIR/helpers/ssh_commands.sh 0 >> $LOG
        echo Finished Commands

        # Take snapshot of current state 
        if [ ! -z "$SNAPSHOT_NAME" ]; then
            if [ ! -z "$REMOVE_SNAPSHOT" ]; then
                echo -e " *** Removing Snapshot ***\n" >> $LOG
                echo -e "\nRemoving Snapshot $SNAPSHOT_NAME"
                # FIXME: to be updated when the new snapshot mechanism is ready (delvm testing)
                cp $DEP_FILE $DIR/$LOG_NAME/Qemu_0/
                check_invoke_script $DIR/helpers/remove_snapshot.sh $SNAPSHOT_NAME >> $LOG
                check_error $? "Snapshot Removed" "Error Removing Snapshot"
            else
                echo -e " *** Taking Snapshot ***\n" >> $LOG
                echo -e "\nTaking Snapshot $SNAPSHOT_NAME"
                check_invoke_script $DIR/helpers/save_snapshot.sh $SNAPSHOT_NAME >> $LOG
                check_error $? "Snapshot Saved" "Error Saving Snapshot"
            fi
        fi
    fi

else
    if [ "$NS3" = "FALSE" ]; then
        echo -e "\n *** Creating Taps and Bridges ***\n" >> $LOG
        check_invoke_script $DIR/helpers/setup_network.sh --no_ns3
        echo -e "\n *** Taps and Bridges created successfully ***\n" >> $LOG
        echo -e "Taps and Bridges linked successfully"
    else
        echo -e "\n *** Creating Taps and Bridges with NS3 ***\n" >> $LOG
        check_invoke_script $DIR/helpers/setup_network.sh "-num=$MULTIPLE_INSTANCE_NUMBER"
        echo -e "\n *** Taps and Bridges linked successfully ***\n" >> $LOG
        echo -e "Taps and Bridges linked successfully"
        echo -e " *** Starting NS3 ***" >> $LOG
        echo -e "\nStarting NS3"

        # Switch to NS3 directory and run NS3 script
        if [ -e "$NS3_PATH/scratch/ns3_2nodes" ]; then
            pushd $NS3_PATH >> /dev/null
            sudo ./waf --run scratch/ns3_2nodes >> $LOG &
            popd >> /dev/null
        else
            printf "$0:$LINENO:$ERROR_PRINT File not found $NS3_PATH/scratch/ns3_2nodes"
            exit 1
        fi
    fi
    i=0
    while [ $i -lt $MULTIPLE_INSTANCE_NUMBER ]
    do
        LOG_QEMU="$DIR/$LOG_NAME/Qemu_$i/logs"
        RUN="$DIR/run_instance.sh $PARA -num=$i"
        echo -e "\nRunning Multiple Instance Mode : Port 222$i\n$RUN\nBooting... Please wait\n"
        echo -e "\n *** Running Multiple Instance Mode : Port 222$i ***\n" >> $LOG
        check_run_instance $i
        check_ssh $i
        echo -e "\n\n *** SSH to remote server $i success ***\n" >> $LOG

        # Configure network settings of Remote Host
        echo -e "\n *** Configuring Remote Server $i ***\n" >> $LOG
        echo Configuring Remote Server $i
        check_invoke_script $DIR/helpers/ssh_config.sh "$i" >> $LOG
        echo -e "\n\n *** Configuration Finished ***\n" >> $LOG
        echo Configuration Finished

        # After successful SSH and setup, execute all the commands specified for "single" instance on remote host
        echo -e " *** Running Commands $i on remote host ***\n" >> $LOG
        echo -e "\nRunning Commands $i"
        check_invoke_script $DIR/helpers/ssh_commands.sh $i >> $LOG
        echo "Finished Commands $i"

        # Take snapshot of current state 
        if [ ! -z "$SNAPSHOT_NAME" ]; then
            if [ ! -z "$REMOVE_SNAPSHOT" ]; then
                echo -e " *** Removing Snapshot ***\n" >> $LOG
                echo -e "\nRemoving Snapshot $SNAPSHOT_NAME"
                check_invoke_script $DIR/helpers/remove_snapshot.sh $SNAPSHOT_NAME >> $LOG
                check_error $? "Snapshot Removed" "Error Removing Snapshot"
            else
                echo -e " *** Taking Snapshot ***\n" >> $LOG
                echo -e "\nTaking Snapshot $SNAPSHOT_NAME"
                check_invoke_script $DIR/helpers/save_snapshot.sh $SNAPSHOT_NAME >> $LOG
                check_error $? "Snapshot Saved" "Error Saving Snapshot"
            fi 
        fi
        i=$[$i+1]
    done
fi

echo -e "\n *** All commands executed ***" >> $LOG

if [ "$KILL_QEMU" = "TRUE" ]; then
    # Command to end the QEMU instance and NS3
    echo -e "\n *** Killing all QEMU instances ***\n" >> $LOG
    echo -e "\nKilling all QEMU instances"
    PROCESS_NUMBER=`ps aux | grep -e "machine virt" -e "ns3_2nodes" | awk '{print$2}'`
    sudo kill -9 $PROCESS_NUMBER >> $LOG
else
    echo -e "\nTo enter Interactive mode on Remote Server: ssh cloudsuite@localhost -p\"Port Number\""
fi

