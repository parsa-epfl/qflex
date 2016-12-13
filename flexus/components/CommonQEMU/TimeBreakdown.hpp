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
#ifndef FLEXUS_TIME_BREAKDOWN_INCLUDED
#define FLEXUS_TIME_BREAKDOWN_INCLUDED

#include <core/target.hpp>
#include <core/types.hpp>
#include <core/flexus.hpp>
#include <core/stats.hpp>

#include <algorithm>

namespace nTimeBreakdown {

namespace {
const char * kStallNames[] = {
  "Unknown"
  , "Busy"

  , "Dataflow"
  , "DataflowBubble"

  , "Spin:Busy"
  , "Spin:Stall"
  , "Spin:SBNonEmpty"

  //WillRaise
  , "WillRaise:Load"
  , "WillRaise:Store"
  , "WillRaise:Atomic"
  , "WillRaise:Branch"
  , "WillRaise:MEMBAR"
  , "WillRaise:Computation"
  , "WillRaise:Synchronizing"
  , "WillRaise:Other"

  , "Load:Forwarded"
  , "Load:L1"
  , "Load:L2"
  , "Load:L2:Coherence"
  , "Load:L3"
  , "Load:L3:Coherence"
  , "Load:PB"
  , "Load:Local:Cold"
  , "Load:Local:Replacement"
  , "Load:Local:DGP"
  , "Load:Local:Coherence"
  , "Load:Remote:Cold"
  , "Load:Remote:Replacement"
  , "Load:Remote:DGP"
  , "Load:Remote:Coherence:Shared"
  , "Load:Remote:Coherence:Modified"
  , "Load:Remote:Coherence:Invalid"
  , "Load:PeerL1:Coherence:Shared"
  , "Load:PeerL1:Coherence:Modified"
  , "Load:PeerL2:Coherence:Shared"
  , "Load:PeerL2:Coherence:Modified"

  , "Store:Unkown"
  , "Store:Forwarded"
  , "Store:L1"
  , "Store:L2"
  , "Store:L2:Coherence"
  , "Store:L3"
  , "Store:L3:Coherence"
  , "Store:PB"
  , "Store:Local:Cold"
  , "Store:Local:Replacement"
  , "Store:Local:DGP"
  , "Store:Local:Coherence"
  , "Store:Remote:Cold"
  , "Store:Remote:Replacement"
  , "Store:Remote:DGP"
  , "Store:Remote:Coherence:Shared"
  , "Store:Remote:Coherence:Modified"
  , "Store:Remote:Coherence:Invalid"
  , "Store:PeerL1:Coherence:Shared"
  , "Store:PeerL1:Coherence:Modified"
  , "Store:PeerL2:Coherence:Shared"
  , "Store:PeerL2:Coherence:Modified"
  , "Store:SBFull"

  //Atomic
  , "Atomic:Forwarded"
  , "Atomic:L1"
  , "Atomic:L2"
  , "Atomic:L2:Coherence"
  , "Atomic:L3"
  , "Atomic:L3:Coherence"
  , "Atomic:PB"
  , "Atomic:Local:Cold"
  , "Atomic:Local:Replacement"
  , "Atomic:Local:DGP"
  , "Atomic:Local:Coherence"
  , "Atomic:Remote:Cold"
  , "Atomic:Remote:Replacement"
  , "Atomic:Remote:DGP"
  , "Atomic:Remote:Coherence:Shared"
  , "Atomic:Remote:Coherence:Modified"
  , "Atomic:Remote:Coherence:Invalid"
  , "Atomic:PeerL1:Coherence:Shared"
  , "Atomic:PeerL1:Coherence:Modified"
  , "Atomic:PeerL2:Coherence:Shared"
  , "Atomic:PeerL2:Coherence:Modified"
  , "Atomic:SBDrain"

  , "SideEffect:Atomic"
  , "SideEffect:Load"
  , "SideEffect:Store"
  , "SideEffect:SBDrain"

  , "MEMBAR"

  , "EmptyROB:Unknown"
  , "EmptyROB:IStall:PeerL1"
  , "EmptyROB:IStall:L2"
  , "EmptyROB:IStall:PeerL2"
  , "EmptyROB:IStall:L3"
  , "EmptyROB:IStall:Mem"
  , "EmptyROB:IStall:Other"
  , "EmptyROB:Mispredict"
  , "EmptyROB:Sync"
  , "EmptyROB:Resync"
  , "EmptyROB:FailedSpeculation"
  , "EmptyROB:Exception"

  , "Branch"

  , "SyncPipe"

  , "FailedSpeculation"
  , "SyncWhileSpeculating"

};
}

namespace Stat = Flexus::Stat;
using Flexus::Core::theFlexus;

enum eCycleClasses {
  kUnknown
  , kBusy

  //Dataflow dependancy preventing commit
  , kDataflow
  , kDataflowBubble

  //Spin
  , kSpin_Busy
  , kSpin_Stall
  , kSpin_SBNonEmpty

  //WillRaise
  , kWillRaise_Load
  , kWillRaise_Store
  , kWillRaise_Atomic
  , kWillRaise_Branch
  , kWillRaise_MEMBAR
  , kWillRaise_Computation
  , kWillRaise_Synchronizing
  , kWillRaise_Other

  //Load
  , kLoad_Forwarded
  , kLoad_L1
  , kLoad_L2
  , kLoad_L2_Coherence
  , kLoad_L3
  , kLoad_L3_Coherence
  , kLoad_PB
  , kLoad_Local_Cold
  , kLoad_Local_Replacement
  , kLoad_Local_DGP
  , kLoad_Local_Coherence
  , kLoad_Remote_Cold
  , kLoad_Remote_Replacement
  , kLoad_Remote_DGP
  , kLoad_Remote_Coherence_Shared
  , kLoad_Remote_Coherence_Modified
  , kLoad_Remote_Coherence_Invalid
  , kLoad_PeerL1Cache_Coherence_Shared
  , kLoad_PeerL1Cache_Coherence_Modified
  , kLoad_PeerL2Cache_Coherence_Shared
  , kLoad_PeerL2Cache_Coherence_Modified

  //Store
  , kStore_Unknown
  , kStore_Forwarded
  , kStore_L1
  , kStore_L2
  , kStore_L2_Coherence
  , kStore_L3
  , kStore_L3_Coherence
  , kStore_PB
  , kStore_Local_Cold
  , kStore_Local_Replacement
  , kStore_Local_DGP
  , kStore_Local_Coherence
  , kStore_Remote_Cold
  , kStore_Remote_Replacement
  , kStore_Remote_DGP
  , kStore_Remote_Coherence_Shared
  , kStore_Remote_Coherence_Modified
  , kStore_Remote_Coherence_Invalid
  , kStore_PeerL1Cache_Coherence_Shared
  , kStore_PeerL1Cache_Coherence_Modified
  , kStore_PeerL2Cache_Coherence_Shared
  , kStore_PeerL2Cache_Coherence_Modified
  , kStore_BufferFull

  //Atomic
  , kAtomic_Forwarded
  , kAtomic_L1
  , kAtomic_L2
  , kAtomic_L2_Coherence
  , kAtomic_L3
  , kAtomic_L3_Coherence
  , kAtomic_PB
  , kAtomic_Local_Cold
  , kAtomic_Local_Replacement
  , kAtomic_Local_DGP
  , kAtomic_Local_Coherence
  , kAtomic_Remote_Cold
  , kAtomic_Remote_Replacement
  , kAtomic_Remote_DGP
  , kAtomic_Remote_Coherence_Shared
  , kAtomic_Remote_Coherence_Modified
  , kAtomic_Remote_Coherence_Invalid
  , kAtomic_PeerL1Cache_Coherence_Shared
  , kAtomic_PeerL1Cache_Coherence_Modified
  , kAtomic_PeerL2Cache_Coherence_Shared
  , kAtomic_PeerL2Cache_Coherence_Modified
  , kAtomic_SBDrain

  //SideEffect Accesses
  , kSideEffect_Atomic
  , kSideEffect_Load
  , kSideEffect_Store
  , kSideEffect_SBDrain

  //MEMBAR
  , kMEMBAR

  //EmptyROB
  , kEmptyROB_Unknown
  , kEmptyROB_IStall_PeerL1
  , kEmptyROB_IStall_L2
  , kEmptyROB_IStall_PeerL2
  , kEmptyROB_IStall_L3
  , kEmptyROB_IStall_Mem
  , kEmptyROB_IStall_Other
  , kEmptyROB_Mispredict
  , kEmptyROB_Sync
  , kEmptyROB_Resync
  , kEmptyROB_FailedSpeculation
  , kEmptyROB_Exception

  //Branch
  , kBranch

  //SyncPipe
  , kSyncPipe

  //FailedSpeculation
  , kFailedSpeculation
  , kSyncWhileSpeculating

  , kLastStallClass
};

struct TimeBreakdown {
  std::string theSource;
  uint64_t theLastAccountedCycle;
  uint32_t theLastClass;
  uint64_t tmpStallCycles; //for IStall stats
  uint64_t tmpStoreStallCycles; //for IStall stats

  struct TimeClass {
    std::string theClassName;

    Stat::StatCounter theCycles;
    int32_t pendCycles;
    Stat::StatCounter theCommits;
    int32_t pendCommits;
    Stat::StatCounter theCommits_Busy;
    int32_t pendCommits_Busy;
    Stat::StatCounter theCommits_Spin;
    int32_t pendCommits_Spin;
    std::vector<Stat::StatCounter *> theStallAccumulators;
    std::vector<int> pendStallAccumulators;
    bool theHasPending;

    TimeClass(std::string const & aSource, std::string const & aClass)
      : theClassName(aClass)
      , theCycles(aSource + ":" + aClass + ":AccountedCycles")
      , pendCycles(0)
      , theCommits(aSource + ":" + aClass + ":Commits")
      , pendCommits(0)
      , theCommits_Busy(aSource + ":" + aClass + ":Commits:Busy")
      , pendCommits_Busy(0)
      , theCommits_Spin(aSource + ":" + aClass + ":Commits:Spin")
      , pendCommits_Spin(0)
      , theHasPending(false) {
      theStallAccumulators.resize(kLastStallClass);
      pendStallAccumulators.resize(kLastStallClass, 0);
      for (int32_t i = 0; i < kLastStallClass; ++i) {
        theStallAccumulators[i] = new Stat::StatCounter(aSource + ":" + aClass + ":Bkd:" + kStallNames[i]);
      }
    }

    void commitPending() {
      if (theHasPending) {
        //accumulation_type::Accumulate pending stall cycles
        std::vector<Stat::StatCounter *>::iterator iter = theStallAccumulators.begin();
        std::vector<Stat::StatCounter *>::iterator end = theStallAccumulators.end();
        std::vector<int>::iterator pend_iter = pendStallAccumulators.begin();
        DBG_Assert( theStallAccumulators.size() == pendStallAccumulators.size());
        DBG_Assert( theStallAccumulators.size() == kLastStallClass);
        while (iter != end) {
          (**iter) += *pend_iter;
          ++iter;
          ++pend_iter;
        }
        theCommits += pendCommits;
        theCommits_Busy += pendCommits_Busy;
        theCommits_Spin += pendCommits_Spin;
        theCycles += pendCycles;
        clearPending();
      }
    }

    void clearPending() {
      std::fill(pendStallAccumulators.begin(), pendStallAccumulators.end(), 0);
      pendCommits = 0;
      pendCommits_Busy = 0;
      pendCommits_Spin = 0;
      pendCycles = 0;
      theHasPending = false;
    }

    void abortPending(eCycleClasses aStallClass) {
      (*theStallAccumulators[aStallClass]) += pendCycles;
      theCycles += pendCycles;
      clearPending();
    }

    void spinSBNonEmpty(int32_t aNumCommits, uint64_t aNumCycles) {
      theCycles += aNumCycles;
      if (aNumCycles > 0) {
        (*theStallAccumulators[kSpin_SBNonEmpty]) += aNumCycles;
      }
      if (aNumCommits > 0) {
        theCommits += aNumCommits;
        theCommits_Spin += aNumCommits;
      }
    }

    void spinSBNonEmpty_pend(int32_t aNumCommits, uint64_t aNumCycles) {
      theHasPending = true;
      pendCycles += aNumCycles;
      if (aNumCycles > 0) {
        pendStallAccumulators[kSpin_SBNonEmpty] += aNumCycles;
      }
      if (aNumCommits > 0) {
        pendCommits += aNumCommits;
        pendCommits_Spin += aNumCommits;
      }
    }

    void spin(int32_t aNumCommits, uint64_t  aBusyCycles, uint64_t  aStallCycles) {
      theCycles += aBusyCycles + aStallCycles;
      if (aBusyCycles > 0) {
        (*theStallAccumulators[kSpin_Busy]) += aBusyCycles;
      }
      if (aStallCycles > 0) {
        (*theStallAccumulators[kSpin_Stall]) += aStallCycles;
      }
      if (aNumCommits > 0) {
        theCommits += aNumCommits;
        theCommits_Spin += aNumCommits;
      }
    }

    void spin_pend(int32_t aNumCommits, uint64_t  aBusyCycles, uint64_t  aStallCycles) {
      theHasPending = true;
      pendCycles += aBusyCycles + aStallCycles;
      if (aBusyCycles > 0) {
        pendStallAccumulators[kSpin_Busy] += aBusyCycles;
      }
      if (aStallCycles > 0) {
        pendStallAccumulators[kSpin_Stall] += aStallCycles;
      }
      if (aNumCommits > 0) {
        pendCommits += aNumCommits;
        pendCommits_Spin += aNumCommits;
      }
    }

    void commit(int32_t aNumCommits, uint64_t aBusyCycles, uint64_t aStallCycles, eCycleClasses aStallClass) {
      theCycles += aBusyCycles + aStallCycles;
      if (aBusyCycles > 0) {
        (*theStallAccumulators[kBusy]) += aBusyCycles;
      }
      if (aStallCycles > 0) {
        (*theStallAccumulators[aStallClass]) += aStallCycles;
      }
      if (aNumCommits > 0) {
        theCommits += aNumCommits;
        theCommits_Busy += aNumCommits;
      }
    }

    void commit_pend(int32_t aNumCommits, uint64_t aBusyCycles, uint64_t aStallCycles, eCycleClasses aStallClass) {
      theHasPending = true;
      pendCycles += aBusyCycles + aStallCycles;
      if (aBusyCycles > 0) {
        pendStallAccumulators[kBusy] += aBusyCycles;
      }
      if (aStallCycles > 0) {
        pendStallAccumulators[aStallClass] += aStallCycles;
      }
      if (aNumCommits > 0) {
        pendCommits += aNumCommits;
        pendCommits_Busy += aNumCommits;
      }
    }

  };

  std::vector<TimeClass *> theClasses;
  bool isSpeculating;

  TimeBreakdown(std::string const & aSource)
    : theSource(aSource)
    , isSpeculating(false) {
    theLastAccountedCycle = 0;
    theLastClass = 0;

    tmpStallCycles = 0; // for IStall stats
    tmpStoreStallCycles = 0; // for IStall stats
  }

  void skipCycle() {
    //Advance theLastAccountedCycle by one to "skip" a cycle from the next
    //accounting
    theLastAccountedCycle++;
  }

  uint32_t addClass(std::string const & aClass) {
    theClasses.push_back( new TimeClass(theSource, aClass) );
    return theClasses.size() - 1;
  }

  void spin(uint32_t aClass, int32_t aNumCommits, bool isSBNonEmpty) {
    DBG_Assert(aClass < theClasses.size());
    if (isSBNonEmpty) {
      uint64_t spin_sbnonempty_cycles = theFlexus->cycleCount() - theLastAccountedCycle;
      if (isSpeculating) {
        theClasses[aClass]->spinSBNonEmpty_pend(aNumCommits, spin_sbnonempty_cycles);
      } else {
        theClasses[aClass]->spinSBNonEmpty(aNumCommits, spin_sbnonempty_cycles);
      }
    }  else {
      uint64_t busy_cycles = (aNumCommits > 0 ? 1 : 0);
      uint64_t stall_cycles = theFlexus->cycleCount() - theLastAccountedCycle - busy_cycles;
      if (theFlexus->cycleCount() == theLastAccountedCycle ) {
        busy_cycles = 0;
        stall_cycles = 0;
      }
      if (isSpeculating) {
        theClasses[aClass]->spin_pend(aNumCommits, busy_cycles, stall_cycles);
      } else {
        theClasses[aClass]->spin(aNumCommits, busy_cycles, stall_cycles);
      }
    }
    theLastAccountedCycle = theFlexus->cycleCount();
    theLastClass = aClass;
  }

  //for IStall and mispredict stats
  uint64_t getTmpStallCycles ( void ) {
    return tmpStallCycles;
  }
  uint64_t subtTmpStallCycles ( uint64_t aCycles ) {
    if (aCycles < tmpStallCycles) {
      tmpStallCycles -= aCycles;
      return aCycles;
    } else {
      return tmpStallCycles;
    }
  }

  void accumulateStoreCyclesTmp( void ) {
    uint64_t stall_cycles = ( (theFlexus->cycleCount() != theLastAccountedCycle) ? (theFlexus->cycleCount() - theLastAccountedCycle) : 0 );
    theLastAccountedCycle = theFlexus->cycleCount();
    tmpStoreStallCycles += stall_cycles;
  }
  void commitAccumulatedStoreCycles(eCycleClasses aCycleClass, uint64_t aCycles) {
    DBG_Assert(theLastClass < theClasses.size());
    if (isSpeculating) {
      theClasses[theLastClass]->commit_pend(0, 0, aCycles, aCycleClass);
    } else {
      theClasses[theLastClass]->commit(0, 0, aCycles, aCycleClass);
    }
  }

  void accumulateCyclesTmp( void ) {
    uint64_t stall_cycles = ( (theFlexus->cycleCount() != theLastAccountedCycle) ? (theFlexus->cycleCount() - theLastAccountedCycle) : 0 );
    theLastAccountedCycle = theFlexus->cycleCount();
    tmpStallCycles += stall_cycles;
  }
  void commitAccumulatedCycles(eCycleClasses aCycleClass, uint64_t aCycles) {
    DBG_Assert(theLastClass < theClasses.size());
    if (isSpeculating) {
      theClasses[theLastClass]->commit_pend(0, 0, aCycles, aCycleClass);
    } else {
      theClasses[theLastClass]->commit(0, 0, aCycles, aCycleClass);
    }
  }

  void commit(uint32_t aClass, eCycleClasses aCycleClass, int32_t aNumCommits) {
    DBG_Assert(aClass < theClasses.size());
    uint64_t busy_cycles = (aNumCommits > 0 ? 1 : 0);
    uint64_t stall_cycles = theFlexus->cycleCount() - theLastAccountedCycle - busy_cycles;
    if (theFlexus->cycleCount() == theLastAccountedCycle ) {
      busy_cycles = 0;
      stall_cycles = 0;
    }
    theLastAccountedCycle = theFlexus->cycleCount();
    if (isSpeculating) {
      theClasses[aClass]->commit_pend(aNumCommits, busy_cycles, stall_cycles, aCycleClass);
    } else {
      theClasses[aClass]->commit(aNumCommits, busy_cycles, stall_cycles, aCycleClass);
    }
    theLastClass = aClass;
  }

  void stall(eCycleClasses aCycleClass) {
    commit(theLastClass, aCycleClass, 0);
  }
  void stall(uint32_t aClass, eCycleClasses aCycleClass) {
    commit(aClass, aCycleClass, 0);
  }

  void startSpeculation() {
    DBG_Assert( ! isSpeculating );
    isSpeculating = true;
  }
  void advanceSpeculation() {
    DBG_Assert( isSpeculating );
    //Commit speculative counters and reset them to zero
    for(auto& aClass: theClasses){
      aClass.commitPending(); 
    }
    // std::for_each
    // ( theClasses.begin()
    //   , theClasses.end()
    //   , ll::bind( &TimeClass::commitPending, ll::var(ll::_1) )
    // );
  }
  void endSpeculation() {
    advanceSpeculation();
    isSpeculating = false;
  }
  void abortSpeculation(eCycleClasses aCycleClass) {
    DBG_Assert( isSpeculating );
    //Replace speculative counters with the specified "failed speculation" category
    //Reset speculative counters
    for(auto& aClass: theClasses){
      aClass.abortPending(aCycleClass);
    }
    // std::for_each
    // ( theClasses.begin()
    //   , theClasses.end()
    //   , ll::bind( &TimeClass::abortPending, ll::var(ll::_1), aCycleClass )
    // );
    isSpeculating = false;
  }

};

} //End nTimeBreakdown

#endif //FLEXUS_TIME_BREAKDOWN_INCLUDED
