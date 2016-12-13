// DO-NOT-REMOVE begin-copyright-block 
//QFlex consists of several software components that are governed by various
//licensing terms, in addition to software that was developed internally.
//Anyone interested in using QFlex needs to fully understand and abide by the
//licenses governing all the software components.
//
//### Software developed externally (not by the QFlex group)
//
//    * [NS-3](https://www.gnu.org/copyleft/gpl.html)
//    * [QEMU](http://wiki.qemu.org/License) 
//    * [SimFlex] (http://parsa.epfl.ch/simflex/)
//
//Software developed internally (by the QFlex group)
//**QFlex License**
//
//QFlex
//Copyright (c) 2016, Parallel Systems Architecture Lab, EPFL
//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without modification,
//are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright notice,
//      this list of conditions and the following disclaimer in the documentation
//      and/or other materials provided with the distribution.
//    * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
//      nor the names of its contributors may be used to endorse or promote
//      products derived from this software without specific prior written
//      permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
//ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
//EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
//GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
//HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
//THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// DO-NOT-REMOVE end-copyright-block   
#include <core/simulator_layout.hpp>

#include <components/CommonQEMU/Slices/MemoryMessage.hpp>

#define FLEXUS_BEGIN_COMPONENT DecoupledFeeder
#include FLEXUS_BEGIN_COMPONENT_DECLARATION()

COMPONENT_PARAMETERS(
  //FIXME Not sure what this does-- 
  //PARAMETER( QemuQuantum, int64_t, "CPU quantum size in simics", "simics_quantum", 100 )
  PARAMETER( SystemTickFrequency, double, "CPU System tick frequency. 0.0 leaves frequency unchanged", "stick", 0.0 )
  PARAMETER( HousekeepingPeriod, int64_t, "Simics cycles between housekeeping events", "housekeeping_period", 1000 )
  PARAMETER( TrackIFetch, bool, "Track and report instruction fetches", "ifetch", false )
  PARAMETER( CMPWidth, int, "Number of cores per CMP chip (0 = sys width)", "CMPwidth", 1 )

  //FIXME Trying to see if it works without WhiteBox
  //PARAMETER( WhiteBoxDebug, bool, "WhiteBox debugging on/off", "whitebox_debug", false )
  //PARAMETER( WhiteBoxPeriod, int, "WhiteBox period", "whitebox_debug_period", 10000 )
  PARAMETER( SendNonAllocatingStores, bool, "Send NonAllocatingStores on/off", "send_non_allocating_stores", false )
);

typedef std::pair< uint64_t, uint32_t> ulong_pair;
typedef std::pair< uint64_t, std::pair< uint32_t, uint32_t> > pc_type_annul_triplet;

COMPONENT_INTERFACE(
  DYNAMIC_PORT_ARRAY( PushOutput, MemoryMessage, ToL1D )
  DYNAMIC_PORT_ARRAY( PushOutput, MemoryMessage, ToL1I )
  //DYNAMIC_PORT_ARRAY( PushOutput, ulong_pair, ToBPred )
  DYNAMIC_PORT_ARRAY( PushOutput, pc_type_annul_triplet, ToBPred )
  DYNAMIC_PORT_ARRAY( PushOutput, MemoryMessage, ToNAW )

  PORT( PushOutput, MemoryMessage, ToDMA )
);

#include FLEXUS_END_COMPONENT_DECLARATION()
#define FLEXUS_END_COMPONENT DecoupledFeeder
