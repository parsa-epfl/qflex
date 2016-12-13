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
#ifndef _OFFCHIP_TRACKER_HPP_
#define _OFFCHIP_TRACKER_HPP_

#include <unordered_map>
#include <boost/dynamic_bitset.hpp>

using namespace Flexus;
using namespace Core;

#define MyLevel Iface

#include DBG_Control()

namespace nTraceTracker {

struct PrefetchStats {
  Stat::StatCounter Requests;
  Stat::StatCounter ReqRead;
  Stat::StatCounter ReqWrite;
  Stat::StatCounter ReqGoodRead;
  Stat::StatCounter ReqGoodWrite;
  Stat::StatCounter ReqWrongPathRead;
  Stat::StatCounter ReqWrongPathWrite;
  Stat::StatCounter ReqWrongPathEvict;
  Stat::StatCounter ReqWrongPathInval;
  Stat::StatCounter ReqSpecSwitch;
  Stat::StatCounter Prefetches;
  Stat::StatCounter PrefHitRead;
  Stat::StatCounter PrefHitWrite;
  Stat::StatCounter PrefHitGoodRead;
  Stat::StatCounter PrefHitGoodWrite;
  Stat::StatCounter PrefHitWrongPathRead;
  Stat::StatCounter PrefHitWrongPathWrite;
  Stat::StatCounter PrefHitWrongPathEvict;
  Stat::StatCounter PrefHitWrongPathInval;
  Stat::StatCounter PrefSpecSwitch;
  Stat::StatCounter PrefOverEvict;
  Stat::StatCounter PrefOverInval;
  Stat::StatCounter RedundantSpec;
  Stat::StatCounter RedundantPresent;

  PrefetchStats(std::string aName)
    : Requests(aName + "-Requests")
    , ReqRead(aName + "-ReqRead")
    , ReqWrite(aName + "-ReqWrite")
    , ReqGoodRead(aName + "-ReqGoodRead")
    , ReqGoodWrite(aName + "-ReqGoodWrite")
    , ReqWrongPathRead(aName + "-ReqWrongPathRead")
    , ReqWrongPathWrite(aName + "-ReqWrongPathWrite")
    , ReqWrongPathEvict(aName + "-ReqWrongPathEvict")
    , ReqWrongPathInval(aName + "-ReqWrongPathInval")
    , ReqSpecSwitch(aName + "-ReqSpecSwitch")
    , Prefetches(aName + "-Prefetches")
    , PrefHitRead(aName + "-PrefHitRead")
    , PrefHitWrite(aName + "-PrefHitWrite")
    , PrefHitGoodRead(aName + "-PrefHitGoodRead")
    , PrefHitGoodWrite(aName + "-PrefHitGoodWrite")
    , PrefHitWrongPathRead(aName + "-PrefHitWrongPathRead")
    , PrefHitWrongPathWrite(aName + "-PrefHitWrongPathWrite")
    , PrefHitWrongPathEvict(aName + "-PrefHitWrongPathEvict")
    , PrefHitWrongPathInval(aName + "-PrefHitWrongPathInval")
    , PrefSpecSwitch(aName + "-PrefSpecSwitch")
    , PrefOverEvict(aName + "-PrefOverEvict")
    , PrefOverInval(aName + "-PrefOverInval")
    , RedundantSpec(aName + "-RedundantSpec")
    , RedundantPresent(aName + "-RedundantPresent")
  {}
};

struct PrefetchEntry {
  enum PrefetchState {
    eSpeculative,
    ePresent,
    ePrefetched,
    ePrefSpec
  };
  PrefetchState state;
  bool presentL1;
  bool presentL2;
  bool read;
  bool write;
  PrefetchStats * stats;
  PrefetchEntry(bool isPrefetch, bool intoBuffer, bool isWrite, PrefetchStats * aStats)
    : state(isPrefetch ? ePrefetched : eSpeculative)
    , presentL1(false)
    , presentL2(!isPrefetch || !intoBuffer)
    , read(isPrefetch ? false : !isWrite)
    , write(isPrefetch ? false : isWrite)
    , stats(aStats) {
    if (isPrefetch) {
      ++stats->Prefetches;
    } else {
      ++stats->Requests;
      if (isWrite) {
        ++stats->ReqWrite;
      } else {
        ++stats->ReqRead;
      }
    }
  }

  void fillL1(bool prefetch) {
    DBG_Assert(!presentL1);
    presentL1 = true;
  }
  void fillL2(bool prefetch) {
    DBG_Assert(!presentL2);
    if (!prefetch) {
      DBG_Assert(state == ePrefSpec, ( << "state = " << state ) );
    } else {
      DBG_Assert(false, ( << "state = " << state ) );
    }
    presentL2 = true;
  }
  void insertL2() {
    DBG_Assert(!presentL2);
    presentL2 = true;
  }
  bool evictL1() {
    DBG_Assert(presentL1);
    presentL1 = false;
    if (!present()) {
      allGone(false);
      return true;
    }
    return false;
  }
  bool evictL2() {
    DBG_Assert(presentL2);
    presentL2 = false;
    if (!present()) {
      allGone(false);
      return true;
    }
    return false;
  }
  bool invalL1() {
    //DBG_Assert(presentL1);
    if (presentL1) {
      presentL1 = false;
      if (!present()) {
        allGone(true);
        return true;
      }
    }
    return false;
  }
  bool invalL2() {
    //DBG_Assert(presentL2);
    if (presentL2) {
      presentL2 = false;
      if (!present()) {
        allGone(true);
        return true;
      }
    }
    return false;
  }
  bool evictPB() {
    DBG_Assert(state == ePrefetched);
    ++stats->PrefOverEvict;
    return true;
  }
  bool invalPB() {
    DBG_Assert(state == ePrefetched);
    ++stats->PrefOverInval;
    return true;
  }
  void prefetchHit(bool isWrite) {
    if (state == ePrefetched) {
      if (isWrite) {
        ++stats->PrefHitWrite;
        write = true;
      } else {
        ++stats->PrefHitRead;
        read = true;
      }
      state = ePrefSpec;
    }
  }
  void redundant() {
    switch (state) {
      case eSpeculative:
        ++stats->RedundantSpec;
        break;
      case ePresent:
        ++stats->RedundantPresent;
        break;
      case ePrefetched:
      case ePrefSpec:
        // duplicate prefetches - don't count here
        break;
      default:
        DBG_Assert(false, ( << "state = " << state ) );
    }
  }
  bool nonSpecAccess(bool isWrite) {
    bool firstUse = false;
    if (!presentL1) {
      // this must be a commit that is satisfied by the store buffer - therefore
      // it should not mark an access to be non-speculative
      return firstUse;
    }
    switch (state) {
      case eSpeculative:
        DBG_Assert(readOrWrite());
        if (write) {
          ++stats->ReqGoodWrite;
        } else {
          ++stats->ReqGoodRead;
        }
        if (isWrite && !write) {
          ++stats->ReqSpecSwitch;
        }
        state = ePresent;
        firstUse = true;
        break;
      case ePresent:
        DBG_Assert(readOrWrite());
        break;
      case ePrefetched:
        // if a prefetched block falls out of L1, then we will not receive
        // a prefetch-hit notification after it comes back into L1
        if (isWrite) {
          ++stats->PrefHitWrite;
          ++stats->PrefHitGoodWrite;
          write = true;
        } else {
          ++stats->PrefHitRead;
          ++stats->PrefHitGoodRead;
          read = true;
        }
        state = ePresent;
        firstUse = true;
        break;
      case ePrefSpec:
        DBG_Assert(readOrWrite());
        if (write) {
          ++stats->PrefHitGoodWrite;
        } else {
          ++stats->PrefHitGoodRead;
        }
        if (isWrite && !write) {
          ++stats->PrefSpecSwitch;
        }
        state = ePresent;
        firstUse = true;
        break;
      default:
        DBG_Assert(false);
    }
    return firstUse;
  }

private:
  bool present() {
    return (presentL1 || presentL2);
  }
  bool readOrWrite() {
    return (read || write);
  }
  void allGone(bool isInval) {
    switch (state) {
      case eSpeculative:
        if (isInval) {
          ++stats->ReqWrongPathInval;
        } else {
          ++stats->ReqWrongPathEvict;
        }
        if (write) {
          ++stats->ReqWrongPathWrite;
        }
        if (read) {
          ++stats->ReqWrongPathRead;
        }
        break;
      case ePresent:
        // normal cache operation - nothing to do
        break;
      case ePrefetched:
        if (isInval) {
          ++stats->PrefOverInval;
        } else {
          ++stats->PrefOverEvict;
        }
        break;
      case ePrefSpec:
        if (isInval) {
          ++stats->PrefHitWrongPathInval;
        } else {
          ++stats->PrefHitWrongPathEvict;
        }
        if (write) {
          ++stats->PrefHitWrongPathWrite;
        }
        if (read) {
          ++stats->PrefHitWrongPathRead;
        }
        break;
      default:
        DBG_Assert(false);
    }
  }
};

typedef std::unordered_map<address_t, PrefetchEntry> PrefetchMap;
typedef PrefetchMap::iterator PrefetchMapIter;

struct PrefetchTracker {
  std::string theName;
  PrefetchMap theCurrentPrefetches;
  PrefetchStats theStats;

  PrefetchTracker(std::string aName)
    : theName(aName)
    , theStats(theName)
  {}
  void fill(address_t block, SharedTypes::tFillLevel cache, bool isWrite) {
    PrefetchMapIter iter = theCurrentPrefetches.find(block);
    if (iter != theCurrentPrefetches.end()) {
      switch (cache) {
        case SharedTypes::eL1:
          iter->second.fillL1(false);
          break;
        case SharedTypes::eL2:
          // this should only happen on a missWritableReply after a write hit
          // to a prefetched block
          iter->second.fillL2(false);
          break;
        default:
          DBG_Assert(false, ( << "cache = " << cache ) );
      }
    } else {
      switch (cache) {
        case SharedTypes::eL1:
          // this is ok - probably an invalidation following a fill up the hierarchy.
          // the fill will have already been marked as wrong-path, so ignore the fill here
          break;
        case SharedTypes::eL2:
          theCurrentPrefetches.insert( std::make_pair(block, PrefetchEntry(false, false, isWrite, &theStats)) );
          break;
        case SharedTypes::ePrefetchBuffer:
          theCurrentPrefetches.insert( std::make_pair(block, PrefetchEntry(true, true, false, &theStats)) );
          break;
        default:
          DBG_Assert(false, ( << "cache = " << cache ) );
      }
    }
  }
  void prefetchFill(address_t block, SharedTypes::tFillLevel cache) {
    PrefetchMapIter iter = theCurrentPrefetches.find(block);
    if (iter != theCurrentPrefetches.end()) {
      switch (cache) {
        case SharedTypes::eL1:
          iter->second.fillL1(true);
          break;
        default:
          DBG_Assert(false, ( << "cache = " << cache ) );
      }
    } else {
      switch (cache) {
        case SharedTypes::eL1:
          // this is ok - probably an invalidation following a fill up the hierarchy.
          // the fill will have already been marked as wrong-path, so ignore the fill here
          break;
        case SharedTypes::eL2:
          theCurrentPrefetches.insert( std::make_pair(block, PrefetchEntry(true, false, false, &theStats)) );
          break;
        default:
          DBG_Assert(false, ( << "cache = " << cache ) );
      }
    }
  }
  void insert(address_t block, SharedTypes::tFillLevel cache) {
    PrefetchMapIter iter = theCurrentPrefetches.find(block);
    if (iter != theCurrentPrefetches.end()) {
      DBG_Assert(cache == SharedTypes::eL2);
      iter->second.insertL2();
    }
  }
  void evict(address_t block, SharedTypes::tFillLevel cache) {
    PrefetchMapIter iter = theCurrentPrefetches.find(block);
    if (iter != theCurrentPrefetches.end()) {
      bool remove = false;
      switch (cache) {
        case SharedTypes::eL1:
          remove = iter->second.evictL1();
          break;
        case SharedTypes::eL2:
          remove = iter->second.evictL2();
          break;
        case SharedTypes::ePrefetchBuffer:
          remove = iter->second.evictPB();
          break;
        case SharedTypes::eCore:
          // ignore "core" duplicate L1 cache simulation
          break;
        default:
          DBG_Assert(false, ( << "cache = " << cache ) );
      }
      if (remove) {
        theCurrentPrefetches.erase(iter);
      }
    }
  }
  void inval(address_t block, SharedTypes::tFillLevel cache) {
    PrefetchMapIter iter = theCurrentPrefetches.find(block);
    if (iter != theCurrentPrefetches.end()) {
      bool remove = false;
      switch (cache) {
        case SharedTypes::eL1:
          remove = iter->second.invalL1();
          break;
        case SharedTypes::eL2:
          remove = iter->second.invalL2();
          break;
        case SharedTypes::ePrefetchBuffer:
          remove = iter->second.invalPB();
          break;
        case SharedTypes::eCore:
          // ignore "core" duplicate L1 cache simulation
          break;
        default:
          DBG_Assert(false, ( << "cache = " << cache ) );
      }
      if (remove) {
        theCurrentPrefetches.erase(iter);
      }
    }
  }
  bool store(address_t block) {
    bool offchip = false;
    PrefetchMapIter iter = theCurrentPrefetches.find(block);
    if (iter != theCurrentPrefetches.end()) {
      offchip = iter->second.nonSpecAccess(true);
    }
    return offchip;
  }
  bool commit(address_t block) {
    bool offchip = false;
    PrefetchMapIter iter = theCurrentPrefetches.find(block);
    if (iter != theCurrentPrefetches.end()) {
      offchip = iter->second.nonSpecAccess(false);
    }
    return offchip;
  }
  void hit(address_t block, bool isWrite) {
    PrefetchMapIter iter = theCurrentPrefetches.find(block);
    if (iter != theCurrentPrefetches.end()) {
      iter->second.prefetchHit(isWrite);
    }
  }
  void redundant(address_t block) {
    PrefetchMapIter iter = theCurrentPrefetches.find(block);
    if (iter != theCurrentPrefetches.end()) {
      iter->second.redundant();
    }
  }
};

typedef boost::dynamic_bitset<> SGvector;
struct SgpTrackStats {
  Stat::StatCounter Generations;
  Stat::StatCounter OffchipMisses;
  Stat::StatCounter SgpHits;
  Stat::StatCounter OffchipNonHead;
  Stat::StatCounter HitsNonHead;
  Stat::StatCounter HitsParallel;
  Stat::StatCounter HitsNonParallel;
  Stat::StatCounter OffchipParallel;
  Stat::StatCounter OffchipNonParallel;
  Stat::StatCounter SparseGens;
  Stat::StatCounter SparseHits;
  Stat::StatInstanceCounter<int64_t> EpochsPerGen;
  Stat::StatInstanceCounter<int64_t> OffchipEpochMembership;
  Stat::StatInstanceCounter<int64_t> HitsEpochMembership;
  Stat::StatInstanceCounter<int64_t> OffchipHeadMembership;
  Stat::StatInstanceCounter<int64_t> HitsHeadMembership;
  Stat::StatCounter DetailNoneGen;
  Stat::StatCounter DetailNoneEpoch;
  Stat::StatCounter DetailCoreOnly;
  Stat::StatCounter DetailSgpOnly;
  Stat::StatCounter DetailBoth;
  Stat::StatCounter DetailSgpOnlyFirst;
  Stat::StatCounter DetailBothFirst;
  Stat::StatCounter DetailNoCore;
  Stat::StatCounter DetailNoCoreFirst;
  Stat::StatInstanceCounter<int64_t> DetailBothDist;

  SgpTrackStats(std::string aName)
    : Generations(aName + "-Generation")
    , OffchipMisses(aName + "-OffchipMisses")
    , SgpHits(aName + "-SgpHits")
    , OffchipNonHead(aName + "-OffchipNonHead")
    , HitsNonHead(aName + "-HitsNonHead")
    , HitsParallel(aName + "-HitsParallel")
    , HitsNonParallel(aName + "-HitsNonParallel")
    , OffchipParallel(aName + "-OffchipParallel")
    , OffchipNonParallel(aName + "-OffchipNonParallel")
    , SparseGens(aName + "-SparseGens")
    , SparseHits(aName + "-SparseHits")
    , EpochsPerGen(aName + "-EpochsPerGen")
    , OffchipEpochMembership(aName + "-OffchipEpochMembership")
    , HitsEpochMembership(aName + "-HitsEpochMembership")
    , OffchipHeadMembership(aName + "-OffchipHeadMembership")
    , HitsHeadMembership(aName + "-HitsHeadMembership")
    , DetailNoneGen(aName + "-DetailNoneGen")
    , DetailNoneEpoch(aName + "-DetailNoneEpoch")
    , DetailCoreOnly(aName + "-DetailCoreOnly")
    , DetailSgpOnly(aName + "-DetailSgpOnly")
    , DetailBoth(aName + "-DetailBoth")
    , DetailSgpOnlyFirst(aName + "-DetailSgpOnlyFirst")
    , DetailBothFirst(aName + "-DetailBothFirst")
    , DetailNoCore(aName + "-DetailNoCore")
    , DetailNoCoreFirst(aName + "-DetailNoCoreFirst")
    , DetailBothDist(aName + "-DetailBothDist")
  {}
};
struct SgpTrackEntry {
  SGvector theSeenAccesses;
  SGvector theOffchipMisses;
  SGvector theSgpPredictions;
  SGvector theSgpHits;
  int32_t theFirstMiss;
  std::deque<SGvector> theParallel;
  SgpTrackEntry(int32_t sgpBlocks, int32_t offset, bool hit)
    : theSeenAccesses(sgpBlocks)
    , theOffchipMisses(sgpBlocks)
    , theSgpPredictions(sgpBlocks)
    , theSgpHits(sgpBlocks)
    , theFirstMiss(offset) {
    theOffchipMisses.set(offset);
    if (hit) {
      theSgpHits.set(offset);
    }
  }
  SgpTrackEntry(int32_t sgpBlocks, SGvector & predictions)
    : theSeenAccesses(sgpBlocks)
    , theOffchipMisses(sgpBlocks)
    , theSgpPredictions(predictions)
    , theSgpHits(sgpBlocks)
    , theFirstMiss(-1)
  {}

  void offchipMiss(int32_t offset) {
    if (theFirstMiss < 0) {
      theFirstMiss = offset;
    }
    theOffchipMisses.set(offset);
  }
  void sgpHit(int32_t offset) {
    if (theFirstMiss < 0) {
      theFirstMiss = offset;
    }
    theOffchipMisses.set(offset);
    theSgpHits.set(offset);
  }
  void sgpPredict(SGvector & predictions) {
    if (!theSgpPredictions.any()) {
      theSgpPredictions = predictions;
    }
  }
  void addParallel(int32_t head, SGvector & parallel, bool disp) {
    if (!theSeenAccesses.test(head)) {
      if (disp) DBG_(Dev, ( << "  already seen: " << theSeenAccesses ) );
      if (disp) DBG_(Dev, ( << "  this par.   : " << parallel ) );
      SGvector newpar(parallel & (~theSeenAccesses));
      if (disp) DBG_(Dev, ( << "  new parallel: " << newpar ) );
      if (newpar.any()) {
        bool push = true;
        if (!theParallel.empty()) {
          if (disp) DBG_(Dev, ( << "  prev epoch  : " << theParallel.back() ) );
          // check if these parallel misses should be attached to the most recent
          SGvector oldpar(parallel & (~newpar));
          if (oldpar.any()) {
            if (oldpar == theParallel.back()) {
              // if the non-new bits are identical to the previous epoch, then attach
              theParallel.back() |= newpar;
              if (disp) DBG_(Dev, ( << "  attach prev : " << theParallel.back() ) );
              push = false;
            } else if (newpar.count() == 1) {
              // if only one new bit, then attach if other bits from this epoch are
              // a subset of the previous epoch
              if (! ((oldpar & (~theParallel.back())).any()) ) {
                theParallel.back() |= newpar;
                if (disp) DBG_(Dev, ( << "  attach prev2: " << theParallel.back() ) );
                push = false;
              }
            }
          }
        }
        if (push) {
          theParallel.push_back(newpar);
        }
        theSeenAccesses |= parallel;
      }
    }
  }
  void endGen(SgpTrackStats & stats, bool disp) {
    if (!theOffchipMisses.any()) {
      return;
    }
    DBG_Assert(theFirstMiss >= 0);
    stats.Generations++;

    if (disp) {
      DBG_(Dev, ( << "  seen acc: " << theSeenAccesses ) );
      DBG_(Dev, ( << "  off-chip: " << theOffchipMisses ) );
      DBG_(Dev, ( << "  sgp pred: " << theSgpPredictions ) );
      DBG_(Dev, ( << "  sgp hits: " << theSgpHits ) );
    }

    // all vectors with more than one bit set represent parallel accesses
    SGvector potential(theOffchipMisses.size());
    std::deque<SGvector>::iterator iter = theParallel.begin();

    // head stats
    if (iter != theParallel.end()) {
      SGvector temp(*iter & theOffchipMisses);
      stats.OffchipHeadMembership << std::make_pair(temp.count(), 1);
      temp = *iter & theSgpHits;
      stats.HitsHeadMembership << std::make_pair(temp.count(), 1);

      temp = (theOffchipMisses & ~(*iter));
      stats.OffchipNonHead += temp.any();
      temp = (theSgpHits & ~(*iter));
      stats.HitsNonHead += temp.any();
    }

    bool trySparse = true;
    bool isSparse = false;
    bool isHit = false;
    int32_t epochs = 0;
    for (; iter != theParallel.end(); ++iter) {
      if (iter->count() > 1) {
        potential |= *iter;
      }
      SGvector temp(*iter & theOffchipMisses);
      if (temp.any()) {
        epochs++;
        if (trySparse) {
          trySparse = false;
          if (temp.count() == 1) {
            isSparse = true;
            if (theSgpHits.any()) {
              isHit = true;
            }
          }
        } else {
          isSparse = false;
          isHit = false;
        }
        stats.OffchipEpochMembership << std::make_pair(temp.count(), 1);
        temp = *iter & theSgpHits;
        stats.HitsEpochMembership << std::make_pair(temp.count(), 1);
      }
    }
    stats.EpochsPerGen << std::make_pair(epochs, 1);
    if (isSparse) {
      stats.SparseGens++;
    }
    if (isHit) {
      stats.SparseHits++;
    }

    SGvector hitsParallel(theSgpHits & potential);
    SGvector hitsNonParallel(theSgpHits & ~potential);
    SGvector offchipParallel(theOffchipMisses & potential);
    SGvector offchipNonParallel(theOffchipMisses & ~potential);

    stats.SgpHits += theSgpHits.count();
    stats.OffchipMisses += theOffchipMisses.count();
    stats.HitsParallel += hitsParallel.count();
    stats.HitsNonParallel += hitsNonParallel.count();
    stats.OffchipParallel += offchipParallel.count();
    stats.OffchipNonParallel += offchipNonParallel.count();

    // walk through all epochs
    int32_t epoch = 0;
    for (iter = theParallel.begin(); iter != theParallel.end(); ++iter) {
      SGvector thisMiss(*iter & theOffchipMisses);
      SGvector thisSgp(thisMiss & theSgpPredictions);
      if (thisSgp.any()) {
        // some hits
        int32_t sgpOnly = 1;  // core cannot get first miss in epoch
        int32_t both = thisSgp.count() - 1;
        int32_t coreOnly = thisMiss.count() - both - 1;
        DBG_Assert( (both >= 0) && (coreOnly >= 0) );
        if (disp) DBG_(Dev, ( << "  detail: (epoch" << epoch << ") sgp hit, both=" << both << " core=" << coreOnly ) );
        if (epoch == 0) {
          stats.DetailSgpOnlyFirst += sgpOnly;
          stats.DetailBothFirst += both;
          if (coreOnly == 0) {
            stats.DetailNoCoreFirst++;
          }
        } else {
          stats.DetailSgpOnly += sgpOnly;
          stats.DetailBoth += both;
          if (coreOnly == 0) {
            stats.DetailNoCore++;
            stats.DetailBothDist << std::make_pair(both, 1);
            if (disp && both == 0) DBG_(Dev, ( << " here" ) );
          }
        }
        stats.DetailCoreOnly += coreOnly;
      } else if (thisMiss.any()) {
        // some misses but no hits
        int32_t coreOnly = thisMiss.count() - 1;
        DBG_Assert(coreOnly >= 0);
        if (disp) DBG_(Dev, ( << "  detail: (epoch" << epoch << ") no hit, core=" << coreOnly ) );
        if (epoch == 0) {
          stats.DetailNoneGen++;
        } else {
          stats.DetailNoneEpoch++;
        }
        stats.DetailCoreOnly += coreOnly;
      }
      epoch++;
    }
  }
};

typedef std::unordered_map<address_t, SgpTrackEntry> SgpTrackMap;
typedef SgpTrackMap::iterator SgpTrackIter;

class SgpTracker {
  std::string theName;
  int32_t theSgpBlocks;
  address_t theGroupMask;
  int32_t theOffsetShift;

  SgpTrackMap theGroups;
  SgpTrackStats theStats;
public:
  SgpTracker(std::string aName, int32_t blockSize, int32_t sgpBlocks)
    : theName(aName)
    , theSgpBlocks(sgpBlocks)
    , theGroupMask(~(blockSize * sgpBlocks - 1))
    , theOffsetShift((int)log2(blockSize))
    , theStats(aName)
  {}
  void offchipMiss(address_t block, bool isWrite) {
    address_t group = makeGroup(block);
    int32_t offset = makeOffset(block);
    SgpTrackIter iter = theGroups.find(group);
    if (iter != theGroups.end()) {
      iter->second.offchipMiss(offset);
    } else {
      theGroups.insert( std::make_pair(group, SgpTrackEntry(theSgpBlocks, offset, false)) );
    }
  }
  void sgpPredict(address_t group, void * aPredictSet) {
    SGvector * vec = (SGvector *)aPredictSet;
    SgpTrackIter iter = theGroups.find(group);
    if (iter != theGroups.end()) {
      iter->second.sgpPredict(*vec);
    } else {
      theGroups.insert( std::make_pair(group, SgpTrackEntry(theSgpBlocks, *vec)) );
    }
  }
  void sgpHit(address_t block, bool isWrite) {
    address_t group = makeGroup(block);
    int32_t offset = makeOffset(block);
    SgpTrackIter iter = theGroups.find(group);
    if (iter != theGroups.end()) {
      iter->second.sgpHit(offset);
    } else {
      theGroups.insert( std::make_pair(group, SgpTrackEntry(theSgpBlocks, offset, true)) );
    }
  }
  void parallelList(address_t block, std::set<uint64_t> & list) {
    address_t group = makeGroup(block);
    SgpTrackIter iter = theGroups.find(group);
    if (iter != theGroups.end()) {
      SGvector parallel(theSgpBlocks);
      std::set<uint64_t>::iterator setIter = list.begin();
      for (; setIter != list.end(); ++setIter) {
        if (group == makeGroup(*setIter)) {
          int32_t offset = makeOffset(*setIter);
          parallel.set(offset);
        }
      }
      int32_t offset = makeOffset(block);
      bool disp = false;
      //if(group == 0x9370000) {
      //disp = true;
      //DBG_(Dev, ( << "Parallel list for group 0x" << std::hex << group ) );
      //}
      iter->second.addParallel(offset, parallel, disp);
    }
  }
  void endGen(address_t block) {
    address_t group = makeGroup(block);
    bool disp = false;
    //if(group == 0x9370000) {
    //disp = true;
    //DBG_(Dev, ( << "End Gen for group 0x" << std::hex << group ) );
    //}
    SgpTrackIter iter = theGroups.find(group);
    if (iter != theGroups.end()) {
      iter->second.endGen(theStats, disp);
      theGroups.erase(iter);
    }
  }
private:
  address_t makeGroup(address_t addr) {
    return (addr & theGroupMask);
  }
  int32_t makeOffset(address_t addr) {
    return ((addr & ~theGroupMask) >> theOffsetShift);
  }
  /*
  address_t makeGroup(uint64_t addr) {
    return (addr & (uint64_t)theGroupMask);
  }
  int32_t makeOffset(address_t addr) {
    return ((addr & ~(uint64_t)theGroupMask) >> theOffsetShift);
  }
  */
};

} // namespace nTraceTracker

#undef MyLevel

#endif
