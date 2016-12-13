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
#ifndef FLEXUS_SLICES__PREFETCHMESSAGE_HPP_INCLUDED
#define FLEXUS_SLICES__PREFETCHMESSAGE_HPP_INCLUDED

#include <core/boost_extensions/intrusive_ptr.hpp>

#include <core/types.hpp>

namespace Flexus {
namespace SharedTypes {

struct PrefetchMessage : public boost::counted_base {
  typedef Flexus::SharedTypes::PhysicalMemoryAddress MemoryAddress;

  // enumerated message type
  enum PrefetchMessageType {
    // Requests made by Prefetch controller
    PrefetchReq,
    // Tells the prefetch buffer to perform a prefetch.
    DiscardReq,
    // Tells the prefetch buffer to discard a line, it if is present.
    WatchReq,
    // Tells the prefetch buffer to monitor a cache line, and report if
    // any requests arrive for it.

    // Responses/Notifications from Prefetch Buffer
    PrefetchComplete,
    // Indicates that a prefetch operation has been completed.
    PrefetchRedundant,
    // Indicates that a prefetch operation was rejected by the memory
    // heirarchy.

    LineHit,
    // This indicates that a hit has occurred on a prefetched line
    LineHit_Partial,
    // This indicates that a hit has occurred on a prefetched line
    LineReplaced,
    // This indicates that a line was removed by replacement
    LineRemoved,
    // This indicates that a line was removed for some reason other
    // than replacement.

    WatchPresent,
    // This indicates that a watched line was found when the cache heirarchy
    // was probed
    WatchRedundant,
    // This indicates that a watched line was found in the prefetch buffer
    WatchRequested,
    // This indicates that a watched line was found when the cache heirarchy
    // was probed
    WatchRemoved,
    // This indicates that a watched line is no longer tracked because of a
    // snoop or a write to the line
    WatchReplaced
    // This indicates that a watched line was dropped from the watch list
    // because of a replacement
  };

  const PrefetchMessageType type() const {
    return theType;
  }
  const MemoryAddress address() const {
    return theAddress;
  }

  PrefetchMessageType & type() {
    return theType;
  }
  MemoryAddress & address() {
    return theAddress;
  }

  const int64_t streamID() const {
    return theStreamID;
  }
  int64_t & streamID() {
    return theStreamID;
  }

  explicit PrefetchMessage(PrefetchMessageType aType, MemoryAddress anAddress, int64_t aStreamId = 0)
    : theType(aType)
    , theAddress(anAddress)
    , theStreamID(aStreamId)
  {}

  friend std::ostream & operator << (std::ostream & s, PrefetchMessage const & aMsg);

private:
  PrefetchMessageType theType;
  MemoryAddress theAddress;
  int64_t theStreamID;
};

} //SharedTypes
} //Scaffold

#endif  // FLEXUS_SLICES__PREFETCHMESSAGE_HPP_INCLUDED
