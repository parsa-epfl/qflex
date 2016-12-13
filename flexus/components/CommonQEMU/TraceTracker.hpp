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
#ifndef _TRACE_TRACKER_HPP_
#define _TRACE_TRACKER_HPP_

#include <map>

#include <core/boost_extensions/intrusive_ptr.hpp>
#include <memory>
#include <boost/optional.hpp>
#include <tuple>
#include <core/target.hpp>
#include <core/types.hpp>
#include <core/stats.hpp>

#include <components/CommonQEMU/Slices/TransactionTracker.hpp>

#include DBG_Control()

namespace nTraceTracker {


typedef uint32_t address_t;


class TraceTracker {


public:
  void access      (int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t addr, address_t pc, bool prefetched, bool write, bool miss, bool priv, uint64_t ltime);
  void commit      (int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t addr, address_t pc, uint64_t ltime);
  void store       (int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t addr, address_t pc, bool miss, bool priv, uint64_t ltime);
  void prefetch    (int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t block);
  void fill        (int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t block, Flexus::SharedTypes::tFillLevel fillLevel, bool isFetch, bool isWrite);
  void prefetchFill(int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t block, Flexus::SharedTypes::tFillLevel fillLevel);
  void prefetchHit (int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t block, bool isWrite);
  void prefetchRedundant(int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t block);
  void insert      (int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t block);
  void eviction    (int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t block, bool drop);
  void invalidation(int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t block);
  void invalidAck  (int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t block);
  void invalidTagCreate (int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t block);
  void invalidTagRefill (int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t block);
  void invalidTagReplace(int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t block);

  void accessLoad  (int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t block, uint32_t offset, int32_t size);
  void accessStore (int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t block, uint32_t offset, int32_t size);
  void accessFetch (int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t block, uint32_t offset, int32_t size);
  void accessAtomic(int32_t aNode, Flexus::SharedTypes::tFillLevel cache, address_t block, uint32_t offset, int32_t size);

  TraceTracker();
  void initialize();
  void finalize();


};

} // namespace nTraceTracker

extern nTraceTracker::TraceTracker theTraceTracker;

#endif
