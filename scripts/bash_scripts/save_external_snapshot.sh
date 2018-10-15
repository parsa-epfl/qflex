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

TIMEOUT="20"
SNAP_NAME=$1
INSTANCE_ID=$2

/usr/bin/expect <<EOD
set timeout $TIMEOUT
spawn nc -U /tmp/qmp-sock$INSTANCE_ID

expect {
  timeout { send_user "\nFailed to connect to qmp-sock\n"; exit 1 }
  eof { send_user "\nnc failure for qmp-sock\n"; exit 1 }
  "*QMP*"
}

send "{ \"execute\": \"qmp_capabilities\" }\r"

expect {
  timeout { send_user "\nFailed to execute qmp_capabilities\n"; exit 1 }
  eof { send_user "\nnc failure for qmp-sock\n"; exit 1 }
  "*return*"
}

send "{ \"execute\": \"human-monitor-command\", \"arguments\": { \"command-line\": \"savevm-ext $SNAP_NAME\" } }\r"

expect {
  timeout { send_user "\n Failed to Save Snapshot"; exit 1 }
  eof { send_user "\nnc failure for qmp-sock\n"; exit 1 }
  "*STOP*"
}

sleep 15

expect eof
EOD


