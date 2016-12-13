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
#ifndef FLEXUS_SLICES__MRCMESSAGE_HPP_INCLUDED
#define FLEXUS_SLICES__MRCMESSAGE_HPP_INCLUDED

#include <core/boost_extensions/intrusive_ptr.hpp>

#include <core/types.hpp>

namespace Flexus {
namespace SharedTypes {

class MRCMessage : public boost::counted_base {

public:
  enum MRCMessageType {
    //Sent by Consort to inform MRCReflector that it has added a block to
    //its buffer
    kAppend,

    //Sent by MRCReflector to request a stream on behalf of a node which
    //experienced a read miss
    kRequestStream

  };

private:
  MRCMessageType theType;
  uint32_t theNode;
  PhysicalMemoryAddress theAddress;
  int64_t theLocation;
  int64_t  theTag;

public:
  const MRCMessageType type() const {
    return theType;
  }

  uint32_t node() const {
    return theNode;
  }

  PhysicalMemoryAddress address() const {
    return theAddress;
  }

  int64_t location() const {
    return theLocation;
  }

  int64_t  tag() const {
    return theTag;
  }

  MRCMessage(MRCMessageType aType, uint32_t aNode, PhysicalMemoryAddress anAddress, int64_t aLocation, int64_t  aTag = -1)
    : theType(aType)
    , theNode(aNode)
    , theAddress(anAddress)
    , theLocation(aLocation)
    , theTag(aTag)
  {}

  friend std::ostream & operator << (std::ostream & s, MRCMessage const & aMsg);
};

} //SharedTypes
} //Scaffold

#endif  // FLEXUS_SLICES__MRCMESSAGE_HPP_INCLUDED
