#!/bin/sh
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

set -x
set -e

# Test qFlex
if [ "$TARGET" != "KeenKraken-arm" ]; then
    exit 0
fi

cd $HOME/build/parsa-epfl/qflex/tests/
cp ../scripts/user_example.cfg ../scripts/user.cfg

sed -i "s/USER_NAME username/USER_NAME $USER/" ../scripts/user.cfg
sed -i "s/QEMU_CORE_NUM 4/QEMU_CORE_NUM 1/" ../scripts/user.cfg
sed -i "s/FLEXUS_CORE_NUM 4/FLEXUS_CORE_NUM 1/" ../scripts/user.cfg
sed -i "s/MEM 4096/MEM 512/" ../scripts/user.cfg
path_to_kernel=$(printf '%s\n' "$HOME/build/parsa-epfl/qflex/images/kernel" | sed 's/[[\.*^$/]/\\&/g')
sed -i "s/\/path\/to\/qemu\/image\/kernel/$path_to_kernel/g" ../scripts/user.cfg
path_to_qemu=$(printf '%s\n' "$HOME/build/parsa-epfl/qflex/qemu" | sed 's/[[\.*^$/]/\\&/g')
sed -i "s/\/path\/to\/qemu/$path_to_qemu/g" ../scripts/user.cfg
path_to_qflex=$(printf '%s\n' "$HOME/build/parsa-epfl/qflex" | sed 's/[[\.*^$/]/\\&/g')
sed -i "s/\/path\/to\/qflex/$path_to_qflex/g" ../scripts/user.cfg
path_to_image=$(printf '%s\n' "$HOME/build/parsa-epfl/qflex/images/debian-blank/debian.qcow2" | sed 's/[[\.*^$/]/\\&/g')
sed -i "s/\/path\/to\/server\/image\/.qcow2\/or\/.img/$path_to_image/g" ../scripts/user.cfg
sed -i "s/\/path\/to\/client\/image\/.qcow2\/or\/.img/$path_to_image/g" ../scripts/user.cfg
path_to_sim=$(printf '%s\n' "$HOME/build/parsa-epfl/qflex/flexus/simulators/KeenKraken/libflexus_KeenKraken_arm_iface_gcc.so" | sed 's/[[\.*^$/]/\\&/g')
sed -i "s/\/path\/to\/simulator\/.so/$path_to_sim/g" ../scripts/user.cfg

sudo ./test_travis.sh
