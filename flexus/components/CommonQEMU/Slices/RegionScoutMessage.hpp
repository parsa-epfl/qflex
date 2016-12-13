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
#ifndef FLEXUS_SLICES__REGIONSCOUTMESSAGE_HPP_INCLUDED
#define FLEXUS_SLICES__REGIONSCOUTMESSAGE_HPP_INCLUDED

#include <core/boost_extensions/intrusive_ptr.hpp>

#include <core/types.hpp>

#include <list>

namespace Flexus {
namespace SharedTypes {

class RegionScoutMessage : public boost::counted_base {
  typedef PhysicalMemoryAddress MemoryAddress;
public:
  enum RegionScoutMessageType {
    eRegionProbe,
    eRegionStateProbe,

    eRegionMissReply,
    eRegionHitReply,

    eRegionIsShared,
    eRegionNonShared,

    eRegionGlobalMiss,
    eRegionPartialMiss,

    eRegionProbeOwner,
    eRegionOwnerReply,
    eRegionSetOwner,

    eRegionEvict,

    eBlockProbe,
    eBlockScoutProbe,
    eBlockMissReply,
    eBlockHitReply,

    eSetTagProbe,
    eRVASetTagProbe,
    eSetTagReply,
    eRVASetTagReply

  };

private:
  RegionScoutMessageType theType;
  MemoryAddress theRegion;
  int32_t theOwner;
  int32_t theCount;
  uint32_t theBlocks;
  bool theShared;

  std::list<MemoryAddress> theTagList;

public:
  const RegionScoutMessageType type() const {
    return theType;
  }
  void  setType(const RegionScoutMessageType & type) {
    theType = type;
  }

  const MemoryAddress region() const {
    return theRegion;
  }
  void  setRegion(const MemoryAddress & region) {
    theRegion = region;
  }

  const int32_t owner() const {
    return theOwner;
  }
  void  setOwner(const int32_t & owner) {
    theOwner = owner;
  }

  const bool shared() const {
    return theShared;
  }
  void  setShared(const bool & shared) {
    theShared = shared;
  }

  const int32_t count() const {
    return theCount;
  }
  void  setCount(const int32_t & count) {
    theCount = count;
  }

  const uint32_t blocks() const {
    return theBlocks;
  }
  void  setBlocks(const uint32_t & blocks) {
    theBlocks = blocks;
  }

  void addTag(const MemoryAddress & addr) {
    theTagList.push_back(addr);
  }
  std::list<MemoryAddress>& getTags() {
    return theTagList;
  }

  explicit RegionScoutMessage(RegionScoutMessageType aType)
    : theType(aType)
    , theRegion(0)
    , theOwner(-1)
    , theBlocks(0)
    , theShared(true)
  {}

  explicit RegionScoutMessage(RegionScoutMessageType aType, const MemoryAddress & aRegion)
    : theType(aType)
    , theRegion(aRegion)
    , theOwner(-1)
    , theBlocks(0)
    , theShared(true)
  {}

  explicit RegionScoutMessage(RegionScoutMessageType aType, const MemoryAddress & aRegion, int32_t anOwner)
    : theType(aType)
    , theRegion(aRegion)
    , theOwner(anOwner)
    , theBlocks(0)
  {}

  friend std::ostream & operator << (std::ostream & s, RegionScoutMessage const & aMsg);
};

std::ostream & operator << (std::ostream & s, RegionScoutMessage::RegionScoutMessageType const & aType);

} //SharedTypes
} //Scaffold

#endif  // FLEXUS_SLICES__REGIONSCOUTMESSAGE_HPP_INCLUDED
