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
#       This is the script used to configure a QEMU instance                    #
#       running in multiple instance mode.                                      #
#                                                                               #
#################################################################################

HOST="localhost"
PASSWORD="cloudsuite"
PORT=222$1
USER="cloudsuite"
TIMEOUT="10"
ID=$1

/usr/bin/expect <<EOD
set timeout $TIMEOUT
spawn ssh $PASSWORD@$HOST -p$PORT -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no

expect {
  timeout { send_user "\nFailed to get password prompt\n"; exit 1 }
  eof { send_user "\nSSH failure for $HOST\n"; exit 1 }
  "*assword:*"
}

send "$PASSWORD\r"

expect {
  timeout { send_user "\nLogin failed. Password incorrect.\n"; exit 1}
  "*\$ "
}

send_user "\nPassword is correct\n"
send "ip addr\r"

expect {
  timeout { send_user "\nLogin failed. Password incorrect.\n"; exit 1}
  "*\$ "
}

send_user "\nPassword is correct\n"
send "sudo ip address add 192.168.2.10$1/24 brd + dev eth0\r"

expect {
  timeout { send_user "\nFailed to get password prompt\n"; exit 1 }
  eof { send_user "\nSSH failure for $HOST\n"; exit 1 }
  "*cloudsuite:*"
}

send "$PASSWORD\r"

expect {
  timeout { send_user "\nError Running command file.\n"; exit 1}
  "*\$ "
}

send "ip addr\r"

expect eof
EOD
