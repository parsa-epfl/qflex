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
#ifndef _SHARING_TRACKER_HPP_
#define _SHARING_TRACKER_HPP_

#include <core/qemu/configuration_api.hpp>
#include <boost/pool/pool.hpp>

namespace Flexus {
namespace Qemu {
namespace API {
extern "C" {
#define restrict
//#include FLEXUS_SIMICS_API_HEADER(memory)
#undef restrict
}
}
}
}

#include <unordered_map>

using namespace Flexus;
using namespace Core;

#define MyLevel Iface

#include DBG_Control()

namespace nTraceTracker {

enum SharingType {
  eFalseSharing,
  eTrueSharing,
  eSilentStore
};

typedef uint64_t SharingVector;

struct SharingInfo {
private:
  struct flags_t {
    unsigned pendingInvTag: 1;
    unsigned pendingInv: 1;
    unsigned filledAfterInval: 1;
    unsigned presentL1: 1;
    unsigned presentL2: 1;
    unsigned potentialSilentStore: 1;
    unsigned potentialOnlyReadAfterWrite: 1; // temporary
    flags_t()
      : pendingInvTag(true)
      , pendingInv(false)
      , filledAfterInval(false)
      , presentL1(false)
      , presentL2(false)
    {}
  } flags;

  SharingVector updatedSinceInval;
  SharingVector accessedAfterFill;
  SharingVector updatedAfterFill;
  SharingVector valueDiff;
  uint64_t * valueAtInval;

public:
  SharingInfo()
    : flags()
    , valueAtInval(0)
  {}
  void reset(uint64_t * blockValue) {
    flags.pendingInvTag = false;
    flags.pendingInv = false;
    flags.filledAfterInval = false;

    flags.potentialSilentStore = true;
    flags.potentialOnlyReadAfterWrite = true;

    updatedSinceInval = 0;
    accessedAfterFill = 0;
    updatedAfterFill = 0;
    valueAtInval = blockValue;
  }

  void setValueDiff(SharingVector diff) {
    valueDiff = diff;
    valueAtInval = 0;
  }
  uint64_t * getValueAtInval() {
    return valueAtInval;
  }

  SharingType finalize() {
    SharingType res = eFalseSharing;
    SharingVector sharing = updatedSinceInval & accessedAfterFill;
    if (sharing != 0) {
      if (flags.potentialSilentStore) {
        res = eSilentStore;
      } else {
        res = eTrueSharing;
      }
    }
    return res;
  }

  void updated(uint32_t offset, int32_t size, bool otherNode) {
    // work in 8-byte chunks
    offset >>= 3;
    uint32_t final = ((size - 1) >> 3) + offset;
    for (; offset <= final; offset++) {
      if (otherNode) {
        updatedSinceInval |= (1 << offset);
      } else {
        updatedAfterFill |= (1 << offset);
      }
    }
  }
  void accessed(uint32_t offset, int32_t size) {
    // work in 8-byte chunks
    offset >>= 3;
    uint32_t final = ((size - 1) >> 3) + offset;
    for (; offset <= final; offset++) {
      if ( (1 << offset) & valueDiff ) {
        if ( (1 << offset) & (~updatedAfterFill) ) {
          flags.potentialSilentStore = false;
        }
      }

      accessedAfterFill |= (1 << offset);

      if ( (1 << offset) & (~updatedAfterFill) ) {
        flags.potentialOnlyReadAfterWrite = false;
      }
    }
  }

  void dataIn(int32_t level) {
    if (level == 1) {
      flags.presentL1 = true;
    } else if (level == 2) {
      flags.presentL2 = true;
    } else {
      DBG_Assert(false, ( << "marking data present for cache level " << level ) );
    }
  }
  void dataOut(int32_t level) {
    if (level == 1) {
      flags.presentL1 = false;
    } else if (level == 2) {
      flags.presentL2 = false;
    } else {
      DBG_Assert(false, ( << "marking data absent for cache level " << level ) );
    }
  }
  bool dataPresent() {
    return (flags.presentL1 || flags.presentL2);
  }

  void setFilledAfterInval() {
    flags.filledAfterInval = true;
  }
  bool filledAfterInval() {
    return flags.filledAfterInval;
  }

  void setPendingInv() {
    flags.pendingInv = true;
  }
  bool pendingInv() {
    return flags.pendingInv;
  }

  bool potentialOnlyReadAfterWrite() {
    return flags.potentialOnlyReadAfterWrite;
  }

  void setPendingInvTag() {
    flags.pendingInvTag = true;
  }
  bool pendingInvTag() {
    return flags.pendingInvTag;
  }

  bool firstAccessAfterInval() {
    return (valueAtInval != 0);
  }
  bool noAccesses() {
    return (accessedAfterFill == 0);
  }
};

struct IntHash {
  std::size_t operator()(uint32_t key) const {
    key = key >> 6;
    return key;
  }
};

class SharingTracker {
  int32_t theNumNodes;
  int32_t theNumChunks;
  boost::pool<> theBlockValuePool;
  Qemu::API::conf_object_t * theCPU;
  uint64_t * theCurrValue;

  typedef std::unordered_map<address_t, SharingInfo, IntHash> SharingMap;
  std::vector<SharingMap> theInvalidTags;

  Stat::StatCounter statFalseSharing;
  Stat::StatCounter statTrueSharing;
  Stat::StatCounter statSilentStores;
  Stat::StatCounter statInvTagReplace;
  Stat::StatCounter statNoAccesses;
  Stat::StatCounter statOnlyReadAfterWrite;
  Stat::StatCounter statInvTagReplaceData;
  Stat::StatCounter statInvTagReplaceNoData;

public:
  SharingTracker(int32_t numNodes, int64_t blockSize)
    : theNumNodes(numNodes)
    , theNumChunks(blockSize >> 3)
    , theBlockValuePool(blockSize)
    , theCPU(0)
    , theCurrValue(0)
    , statFalseSharing("sys-SharingTracker-FalseSharing")
    , statTrueSharing("sys-SharingTracker-TrueSharing")
    , statSilentStores("sys-SharingTracker-SilentStores")
    , statInvTagReplace("sys-SharingTracker-InvalidTagReplace")
    , statNoAccesses("sys-SharingTracker-OnlyWrites")
    , statOnlyReadAfterWrite("sys-SharingTracker-OnlyReadAfterWrite")
    , statInvTagReplaceData("sys-SharingTracker-InvTagReplaceData")
    , statInvTagReplaceNoData("sys-SharingTracker-InvTagReplaceNoData") {
    theInvalidTags.resize(theNumNodes);

    theCPU = Qemu::API::QEMU_get_cpu_by_index( 0 );
   //No idea about how to do this
  //  if (!theCPU) {
    //  theCPU = Simics::API::SIM_get_object( "server_cpu0" );
   // }
    if (!theCPU) {
      DBG_Assert(false, ( << "Unable to find CPU  conf object" ) );
    }

    theCurrValue = (uint64_t *) theBlockValuePool.malloc();
  }

  void finalize() {
    // do nothing
  }

  void accessLoad(int32_t aNode, address_t block, uint32_t offset, int32_t size) {
    if (block == 0xd170080/*FS*/ || block == 0x1da87240/*SS*/)
      DBG_(MyLevel, ( << "[" << aNode << "] accessLoad 0x" << std::hex << block << "," << offset));

    SharingMap::iterator iter = theInvalidTags[aNode].find(block);
    if (iter != theInvalidTags[aNode].end()) {
      if (iter->second.firstAccessAfterInval()) {
        iter->second.setValueDiff( calcValueDiff(block, iter->second.getValueAtInval()) );
      }
      iter->second.accessed(offset, size);
    }
  }

  void accessStore(int32_t aNode, address_t block, uint32_t offset, int32_t size) {
    if (block == 0xd170080/*FS*/ || block == 0x1da87240/*SS*/)
      DBG_(MyLevel, ( << "[" << aNode << "] accessStore 0x" << std::hex << block << "," << offset));

    int32_t ii;
    for (ii = 0; ii < theNumNodes; ii++) {
      SharingMap::iterator iter = theInvalidTags[ii].find(block);
      if (iter != theInvalidTags[ii].end()) {
        if (ii == aNode) {
          if (iter->second.firstAccessAfterInval()) {
            iter->second.setValueDiff( calcValueDiff(block, iter->second.getValueAtInval()) );
          }
        }
        iter->second.updated(offset, size, (ii != aNode));
      }
    }
  }

  void accessAtomic(int32_t aNode, address_t block, uint32_t offset, int32_t size) {
    if (block == 0xd170080/*FS*/ || block == 0x1da87240/*SS*/)
      DBG_(MyLevel, ( << "[" << aNode << "] accessAtomic 0x" << std::hex << block << "," << offset));

    accessLoad(aNode, block, offset, size);
    accessStore(aNode, block, offset, size);
  }

  void fill(int32_t aNode, int32_t aCacheLevel, address_t block) {
    if (block == 0xd170080/*FS*/ || block == 0x1da87240/*SS*/)
      DBG_(MyLevel, ( << "[" << aNode << ":" << aCacheLevel << "] fill 0x" << std::hex << block));

    SharingMap::iterator iter = theInvalidTags[aNode].find(block);
    if (iter != theInvalidTags[aNode].end()) {
      iter->second.dataIn(aCacheLevel);
      if (aCacheLevel == 2) {
        iter->second.setFilledAfterInval();
      }
    }
  }

  void insert(int32_t aNode, int32_t aCacheLevel, address_t block) {
    if (block == 0xd170080/*FS*/ || block == 0x1da87240/*SS*/)
      DBG_(MyLevel, ( << "[" << aNode << ":" << aCacheLevel << "] insert 0x" << std::hex << block));

    SharingMap::iterator iter = theInvalidTags[aNode].find(block);
    if (iter != theInvalidTags[aNode].end()) {
      iter->second.dataIn(aCacheLevel);
      DBG_Assert(iter->second.filledAfterInval());
    }
  }

  void evict(int32_t aNode, int32_t aCacheLevel, address_t block, bool drop) {
    if (block == 0xd170080/*FS*/ || block == 0x1da87240/*SS*/)
      DBG_(MyLevel, ( << "[" << aNode << ":" << aCacheLevel << "] evict 0x" << std::hex << block ));

    SharingMap::iterator iter = theInvalidTags[aNode].find(block);
    if (iter != theInvalidTags[aNode].end()) {
      iter->second.dataOut(aCacheLevel);
      if (!iter->second.pendingInv()) {
        if (!iter->second.dataPresent()) {
          if (drop || aCacheLevel == 2) {
            doStats(iter->second, block);
            DBG_Assert(!iter->second.pendingInvTag(), ( << "[" << aNode << "]: 0x" << std::hex << block ) );
            theInvalidTags[aNode].erase(freeSharing(iter));
          }
        }
      }
    }
  }

  void invalidate(int32_t aNode, int32_t aCacheLevel, address_t block) {
    if (block == 0xd170080/*FS*/ || block == 0x1da87240/*SS*/)
      DBG_(MyLevel, ( << "[" << aNode << ":" << aCacheLevel << "] invalidate 0x" << std::hex << block ));

    SharingMap::iterator iter = theInvalidTags[aNode].find(block);
    if (iter != theInvalidTags[aNode].end()) {
      iter->second.dataOut(aCacheLevel);
      if (aCacheLevel == 2) {
        DBG_Assert(!iter->second.pendingInv());
        iter->second.setPendingInv();
      }
    }
  }

  void invalidAck(int32_t aNode, address_t block) {
    if (block == 0xd170080/*FS*/ || block == 0x1da87240/*SS*/)
      DBG_(MyLevel, ( << "[" << aNode << "] invalidAck 0x" << std::hex << block ));

    SharingMap::iterator iter = theInvalidTags[aNode].find(block);
    if (iter != theInvalidTags[aNode].end()) {
      DBG_Assert(iter->second.pendingInv(), ( << "[" << aNode << "]: 0x" << std::hex << block ) );
      DBG_Assert(!iter->second.dataPresent(),  ( << "[" << aNode << "]: 0x" << std::hex << block ) );
      if (iter->second.filledAfterInval()) {
        doStats(iter->second, block);
      }
      if (iter->second.pendingInvTag()) {
        iter->second.reset( readBlockValue(block) );
      } else {
        theInvalidTags[aNode].erase(freeSharing(iter));
      }
    }
  }

  void invTagCreate(int32_t aNode, address_t block) {
    if (block == 0xd170080/*FS*/ || block == 0x1da87240/*SS*/)
      DBG_(MyLevel, ( << "[" << aNode << "] tagCreate 0x" << std::hex << block ));

    SharingMap::iterator iter = theInvalidTags[aNode].find(block);
    if (iter != theInvalidTags[aNode].end()) {
      iter->second.setPendingInvTag();
    } else {
      theInvalidTags[aNode].insert( std::make_pair(block, SharingInfo()) );
    }
  }

  void invTagRefill(int32_t aNode, address_t block) {
    if (block == 0xd170080/*FS*/ || block == 0x1da87240/*SS*/)
      DBG_(MyLevel, ( << "[" << aNode << "] tagRefill 0x" << std::hex << block ));
  }

  void invTagReplace(int32_t aNode, address_t block) {
    if (block == 0xd170080/*FS*/ || block == 0x1da87240/*SS*/)
      DBG_(MyLevel, ( << "[" << aNode << "] tagReplace 0x" << std::hex << block ));

    SharingMap::iterator iter = theInvalidTags[aNode].find(block);
    if (iter != theInvalidTags[aNode].end()) {
      if (!iter->second.pendingInv()) {
        if (iter->second.dataPresent()) {
          DBG_Assert(iter->second.filledAfterInval(), ( << "[" << aNode << "]: 0x" << std::hex << block ) );
          statInvTagReplaceData++;
        } else {
          DBG_Assert(!iter->second.filledAfterInval(), ( << "[" << aNode << "]: 0x" << std::hex << block ) );
          DBG_Assert(iter->second.noAccesses(), ( << "[" << aNode << "]: 0x" << std::hex << block ) );
          statInvTagReplaceNoData++;
          theInvalidTags[aNode].erase(freeSharing(iter));
        }
      }
      statInvTagReplace++;
    }
  }

private:
  void readBlockValue(address_t block, uint64_t * array) {
    int32_t ii;
    for (ii = 0; ii < theNumChunks; ii++) {
      array[ii] = Qemu::API::QEMU_read_phys_memory(theCPU, block, 8);
      block += 8;
    }
  }
  uint64_t * readBlockValue(address_t block) {
    uint64_t * array = (uint64_t *) theBlockValuePool.malloc();
    readBlockValue(block, array);
    return array;
  }

  SharingVector calcValueDiff(address_t block, uint64_t * priorValue) {
    DBG_Assert(priorValue, ( << "address 0x" << std::hex << block ) );
    readBlockValue(block, theCurrValue);

    SharingVector valueDiff = 0;
    int32_t ii;
    for (ii = 0; ii < theNumChunks; ii++) {
      if (theCurrValue[ii] != priorValue[ii]) {
        valueDiff |= (1 << ii);
      }
    }

    theBlockValuePool.free(priorValue);

    return valueDiff;
  }

  SharingMap::iterator & freeSharing(SharingMap::iterator & iter) {
    uint64_t * array = iter->second.getValueAtInval();
    if (array) {
      theBlockValuePool.free(array);
    }
    return iter;
  }

  void doStats(SharingInfo & data, address_t block) {
    if (data.noAccesses()) {
      statNoAccesses++;
    }
    if (data.potentialOnlyReadAfterWrite()) {
      statOnlyReadAfterWrite++;
    }

    SharingType sharing = data.finalize();
    switch (sharing) {
      case eFalseSharing:
        //DBG_(Dev, ( << "     FS:" << std::hex << block ) );
        statFalseSharing++;
        break;
      case eTrueSharing:
        //DBG_(Dev, ( << "     TS:" << std::hex << block ) );
        statTrueSharing++;
        break;
      case eSilentStore:
        //DBG_(Dev, ( << "     SS:" << std::hex << block ) );
        statSilentStores++;
        break;
      default:
        DBG_Assert(false);
    }
  }

};

} // namespace nTraceTracker

#undef MyLevel

#endif
