#!/bin/bash -xv
# QFlex consists of several software components that are governed by various
# licensing terms, in addition to software that was developed internally.
# Anyone interested in using QFlex needs to fully understand and abide by the
# licenses governing all the software components.

# ### Software developed externally (not by the QFlex group)

#     * [NS-3](https://www.gnu.org/copyleft/gpl.html)
#     * [QEMU](http://wiki.qemu.org/License)
#     * [SimFlex](http://parsa.epfl.ch/simflex/)

# ### Software developed internally (by the QFlex group)
# **QFlex License**

# QFlex
# Copyright (c) 2016, Parallel Systems Architecture Lab, EPFL
# All rights reserved.

# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:

#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright notice,
#       this list of conditions and the following disclaimer in the documentation
#       and/or other materials provided with the distribution.
#     * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
#       nor the names of its contributors may be used to endorse or promote
#       products derived from this software without specific prior written
#       permission.

# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
# EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

export LD_LIBRARY_PATH=/usr/local/lib/

# Font Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

BASE_DIR=$1
TEST_DIR=$BASE_DIR/tests/test
LOG=$BASE_DIR/test/result.log
mkdir $TEST_DIR
mkdir $TEST_DIR/config


# An attempt to obtain sudo permissions at the start of the test
sudo clear

# Some helper functions
check_status() {
    if [ ! -z "$1" ]; then
        printf "[ ${GREEN} PASSED ${NC} ] $2\n" >> $LOG
        printf "[ ${GREEN} PASSED ${NC} ] $2\n"
    else
        printf "[ ${RED} FAILED ${NC} ] $2\n" >> $LOG
        printf "[ ${RED} FAILED ${NC} ] $2\n"
        exit 1
    fi
}
################################################################################
## Single Instance Boot
# Create result directory
mkdir $TEST_DIR/single_instance
# Generate configuration files
python $TEST_DIR/config_manager.py generate_system -t $TEST_DIR/config -q $BASE_DIR -c 1
# Launch system
python $BASE_DIR/scripts/test_system.py $TEST_DIR/config/system.ini -s test_snapshot -f $TEST_DIR/single_instance/single_instance.log -v DEBUG
# Test results
if [ $? -eq 0 ]; then
    printf "[ ${GREEN} PASSED ${NC} ] Single Instance Boot\n" >> $LOG
    printf "[ ${GREEN} PASSED ${NC} ] Single Instance Boot\n"
else
    printf "[ ${RED} FAILED ${NC} ] Single Instance Boot\n" >> $LOG
    printf "[ ${RED} FAILED ${NC} ] Single Instance Boot\n"
    exit 1
fi

################################################################################
## Single Instance Trace
# Create result directory
mkdir $TEST_DIR/single_instance_trace
# Edit configuration files
python $TEST_DIR/config_manager.py add_trace_simulation -t $TEST_DIR/config/instance0.ini -q $BASE_DIR
# Launch system
python $BASE_DIR/scripts/run_system.py $TEST_DIR/config/system.ini -f $TEST_DIR/single_instance_trace/single_instance_trace.log -v DEBUG
# Test results
sleep 30
$BASE_DIR/flexus/stat-manager/stat-manager print all > $TEST_DIR/single_instance_trace/stats.raw
# check to see if flexus ran for 10000000 instructions
TEST_TRACE=`grep feeder-ICount.*10000000 $TEST_DIR/single_instance_trace/stats.raw`
check_status "$TEST_TRACE" "Single Instance Trace"
r=1
for stat in Fetches IOOps LoadExOps StoreExOps LoadOps StoreOps; do
  t="$(grep feeder-${stat} $DIR/results/trace_run/Qemu_0/stats.raw | tr -s ' ' | cut -d ' ' -f3)"
  r=$((r*t))
done
if [ $r -eq 0 ]; then
  exit 1
fi
################################################################################
## Single Instance Load
# Create result directory
mkdir $TEST_DIR/single_instance
# Generate configuration files
python $TEST_DIR/config_manager.py generate_system -t $TEST_DIR/config -q $BASE_DIR -c 1
python $TEST_DIR/config_manager.py add_snapshot -t $TEST_DIR/config/instance0.ini -s test_snapshot
# Launch system
python $BASE_DIR/scripts/test_system.py $TEST_DIR/config/system.ini -s test_snapshot -f $TEST_DIR/single_instance/single_instance.log -v DEBUG
# Test results
if [ $? -eq 0 ]; then
    printf "[ ${GREEN} PASSED ${NC} ] Single Instance Load\n" >> $LOG
    printf "[ ${GREEN} PASSED ${NC} ] Single Instance Load\n"
else
    printf "[ ${RED} FAILED ${NC} ] Single Instance Load\n" >> $LOG
    printf "[ ${RED} FAILED ${NC} ] Single Instance Load\n"
    exit 1
fi
################################################################################