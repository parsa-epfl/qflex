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
#ifndef FLEXUS_SLICES__CMOB_MESSAGE_HPP_INCLUDED
#define FLEXUS_SLICES__CMOB_MESSAGE_HPP_INCLUDED

#include <iostream>
#include <bitset>
#include <core/boost_extensions/intrusive_ptr.hpp>
#include <core/types.hpp>

#ifdef FLEXUS_CMOBMessage_TYPE_PROVIDED
#error "Only one component may provide the Flexus::SharedTypes::CMOBMessage data type"
#endif
#define FLEXUS_CMOBMessage_TYPE_PROVIDED

namespace Flexus {
namespace SharedTypes {

typedef Flexus::SharedTypes::PhysicalMemoryAddress MemoryAddress;

struct CMOBLine {
  static const int32_t kLineSize = 12;
  MemoryAddress theAddresses[kLineSize];
  std::bitset<kLineSize> theWasHit;
};

inline std::ostream & operator<< (std::ostream & aStream, const CMOBLine & line) {
  for (int32_t i = 0; i < 12; ++i) {
    aStream << std::hex << line.theAddresses[i] << ( line.theWasHit[i] ? "(hit) " : "(nohit) ");
  }
  return aStream;
}

namespace CMOBCommand {
enum CMOBCommand {
  eRead,
  eWrite,
  eInit
};
}
namespace {
char * CMOBCommandStr[] = {
  "eRead",
  "eWrite",
  "eInit"
};
}

struct CMOBMessage : public boost::counted_base {
  CMOBCommand::CMOBCommand theCommand;
  CMOBLine theLine;
  int64_t theCMOBOffset;
  uint32_t theCMOBId;
  int64_t theRequestTag;
};

inline std::ostream & operator<< (std::ostream & aStream, const CMOBMessage & msg) {
  aStream << "CMOB[" << msg.theCMOBId << "] #" << msg.theRequestTag << " " << CMOBCommandStr[msg.theCommand] << " @" << msg.theCMOBOffset << " " << msg.theLine;
  return aStream;
}

} //End SharedTypes
} //End Flexus

#endif //FLEXUS_COMMON_CMOBMESSAGE_HPP_INCLUDED
