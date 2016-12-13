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
#include <iostream>

#include <core/types.hpp>

#include <components/CommonQEMU/Slices/MemOp.hpp>

namespace Flexus {
namespace SharedTypes {

std::ostream & operator <<( std::ostream & anOstream, eOperation op) {
  const char * map_tables[] = {
    "Load"
    , "AtomicPreload"
    , "RMW"
    , "CAS"
    , "StorePrefetch"
    , "Store"
    , "Invalidate"
    , "Downgrade"
    , "Probe"
    , "Return"
    , "LoadReply"
    , "AtomicPreloadReply"
    , "StoreReply"
    , "StorePrefetchReply"
    , "RMWReply"
    , "CASReply"
    , "DowngradeAck"
    , "InvAck"
    , "ProbeAck"
    , "ReturnReply"
    , "MEMBARMarker"
    , "LastOperation"
  };
  if (op >= kLastOperation) {
    anOstream << "Invalid Operation(" << static_cast<int>(op) << ")";
  } else {
    anOstream << map_tables[op];
  }
  return anOstream;
}

std::ostream & operator << ( std::ostream & anOstream, MemOp const & aMemOp) {
  anOstream
      << aMemOp.theOperation
      << "(" << aMemOp.theSize << ") "
      << aMemOp.theVAddr
      << "[" << aMemOp.theASI << "] "
      << aMemOp.thePAddr
      << " pc:" << aMemOp.thePC
      ;
  if (aMemOp.theReverseEndian) {
    anOstream << " {reverse-endian}";
  }
  if (aMemOp.theNonCacheable) {
    anOstream << " {non-cacheable}";
  }
  if (aMemOp.theSideEffect) {
    anOstream << " {side-effect}";
  }
  if (aMemOp.theNAW) {
    anOstream << " {non-allocate-write}";
  }
  anOstream << " =" << aMemOp.theValue;
  return anOstream;
}

} //SharedTypes
} //Flexus
