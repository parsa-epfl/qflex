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
#ifndef FLEXUS_XACT_TIME_BREAKDOWN_INCLUDED
#define FLEXUS_XACT_TIME_BREAKDOWN_INCLUDED

#include <core/target.hpp>
#include <core/types.hpp>
#include <core/flexus.hpp>
#include <core/stats.hpp>

#include <algorithm>

namespace nXactTimeBreakdown {

namespace {
const char * kStallNames[] = {
  "Unknown"
  , "Busy"

  , "Dataflow"
  , "DataflowBubble"

  , "Spin:Busy"
  , "Spin:Stall"
  , "Spin:SBNonEmpty"

  , "Idle:Busy"
  , "Idle:Stall"

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
  , "Load:L2:Instructions"
  , "Load:L2:Data:Private"
  , "Load:L2:Data:Shared:RO"
  , "Load:L2:Data:Shared:RW"
  , "Load:L2:Coherence"
  , "Load:L3"
  , "Load:L3:Instructions"
  , "Load:L3:Data:Private"
  , "Load:L3:Data:Shared:RO"
  , "Load:L3:Data:Shared:RW"
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
  , "Store:L2:Instructions"
  , "Store:L2:Data:Private"
  , "Store:L2:Data:Shared:RO"
  , "Store:L2:Data:Shared:RW"
  , "Store:L2:Coherence"
  , "Store:L3"
  , "Store:L3:Instructions"
  , "Store:L3:Data:Private"
  , "Store:L3:Data:Shared:RO"
  , "Store:L3:Data:Shared:RW"
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
  , "Store:SBFull:L2:Data:Private"
  , "Store:SBFull:L2:Data:Shared"
  , "Store:SBFull:L2:Coherence"
  , "Store:SBFull:L2:Unknown"
  , "Store:SBFull:L3:Data:Private"
  , "Store:SBFull:L3:Data:Shared"
  , "Store:SBFull:L3:Coherence"
  , "Store:SBFull:L3:Unknown"
  , "Store:SBFull:Mem"
  , "Store:SBFull:PeerL1Cache"
  , "Store:SBFull:PeerL2Cache"
  , "Store:SBFull:Other"

  //Atomic
  , "Atomic:Forwarded"
  , "Atomic:L1"
  , "Atomic:L2"
  , "Atomic:L2:Instructions"
  , "Atomic:L2:Data:Private"
  , "Atomic:L2:Data:Shared:RO"
  , "Atomic:L2:Data:Shared:RW"
  , "Atomic:L2:Coherence"
  , "Atomic:L3"
  , "Atomic:L3:Instructions"
  , "Atomic:L3:Data:Private"
  , "Atomic:L3:Data:Shared:RO"
  , "Atomic:L3:Data:Shared:RW"
  , "Atomic:L3:Coherence"
  , "Atomic:Directory"
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
  , "SideEffect:Speculating"

  , "MMUAccess"

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
  , "EmptyROB:Interrupt"

  , "Branch"

  , "SyncPipe"

  , "FailedSpeculation"
  , "SyncWhileSpeculating"
  , "TSOBReplay"


};
}

namespace Stat = Flexus::Stat;
using Flexus::Core::theFlexus;

enum eCycleClass {
  kUnknown
  , kBusy

  //Dataflow dependancy preventing commit
  , kDataflow
  , kDataflowBubble

  //Spin
  , kSpin_Busy
  , kSpin_Stall
  , kSpin_SBNonEmpty

  , kIdle_Busy
  , kIdle_Stall

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
  , kLoad_L2_Instruction
  , kLoad_L2_Data_Private
  , kLoad_L2_Data_Shared_RO
  , kLoad_L2_Data_Shared_RW
  , kLoad_L2_Coherence
  , kLoad_L3
  , kLoad_L3_Instruction
  , kLoad_L3_Data_Private
  , kLoad_L3_Data_Shared_RO
  , kLoad_L3_Data_Shared_RW
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
  , kStore_L2_Instruction
  , kStore_L2_Data_Private
  , kStore_L2_Data_Shared_RO
  , kStore_L2_Data_Shared_RW
  , kStore_L2_Coherence
  , kStore_L3
  , kStore_L3_Instruction
  , kStore_L3_Data_Private
  , kStore_L3_Data_Shared_RO
  , kStore_L3_Data_Shared_RW
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
  , kStore_BufferFull_L2_Data_Private
  , kStore_BufferFull_L2_Data_Shared
  , kStore_BufferFull_L2_Coherence
  , kStore_BufferFull_L2_Unknown
  , kStore_BufferFull_L3_Data_Private
  , kStore_BufferFull_L3_Data_Shared
  , kStore_BufferFull_L3_Coherence
  , kStore_BufferFull_L3_Unknown
  , kStore_BufferFull_Mem
  , kStore_BufferFull_PeerL1Cache
  , kStore_BufferFull_PeerL2Cache
  , kStore_BufferFull_Other

  //Atomic
  , kAtomic_Forwarded
  , kAtomic_L1
  , kAtomic_L2
  , kAtomic_L2_Instruction
  , kAtomic_L2_Data_Private
  , kAtomic_L2_Data_Shared_RO
  , kAtomic_L2_Data_Shared_RW
  , kAtomic_L2_Coherence
  , kAtomic_L3
  , kAtomic_L3_Instruction
  , kAtomic_L3_Data_Private
  , kAtomic_L3_Data_Shared_RO
  , kAtomic_L3_Data_Shared_RW
  , kAtomic_L3_Coherence
  , kAtomic_Directory
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
  , kSideEffect_Speculating

  , kMMUAccess

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
  , kEmptyROB_Interrupt

  //Branch
  , kBranch

  //SyncPipe
  , kSyncPipe

  //FailedSpeculation
  , kFailedSpeculation
  , kSyncWhileSpeculating
  , kTSOBReplay


  , kLastStallClass
};

enum eDiscardCause {
  ePreserve,
  eDiscard_Resync,
  eDiscard_FailedSpeculation
};

struct TimeXact {
  uint32_t theTimeClass;
  enum Type {
    eSpinRetire,
    eBusyRetire,
    eStall
  } theType;
  uint32_t theCount;
  eCycleClass theCycleClass;
  uint64_t theSequence;
};

struct TimeBreakdown {
  std::string theSource;
  uint64_t theLastAccountedCycle;
  uint64_t theLastEnqueueSequence;
  int32_t theLastTimeClass;
  Stat::StatCounter theSkippedCycles;

  uint64_t tmpStallCycles; //for IStall stats
  uint64_t tmpStoreStallCycles; //for IStall stats

  struct TimeClass {
    std::string theClassName;

    Stat::StatCounter theCycles;
    Stat::StatCounter theCommits;
    Stat::StatCounter theCommits_Busy;
    Stat::StatCounter theCommits_Spin;
    Stat::StatCounter theDiscards_Resync;
    Stat::StatCounter theDiscards_Resync_Busy;
    Stat::StatCounter theDiscards_Resync_Spin;
    Stat::StatCounter theDiscards_FailedSpec;
    Stat::StatCounter theDiscards_FailedSpec_Busy;
    Stat::StatCounter theDiscards_FailedSpec_Spin;
    std::vector<Stat::StatCounter *> theStallAccumulators;

    TimeClass(std::string const & aSource, std::string const & aClass)
      : theClassName(aClass)
      , theCycles(aSource + ":" + aClass + ":AccountedCycles")
      , theCommits(aSource + ":" + aClass + ":Commits")
      , theCommits_Busy(aSource + ":" + aClass + ":Commits:Busy")
      , theCommits_Spin(aSource + ":" + aClass + ":Commits:Spin")
      , theDiscards_Resync(aSource + ":" + aClass + ":Discards:Resync")
      , theDiscards_Resync_Busy(aSource + ":" + aClass + ":Discards:Resync:Busy")
      , theDiscards_Resync_Spin(aSource + ":" + aClass + ":Discards:Resync:Spin")
      , theDiscards_FailedSpec(aSource + ":" + aClass + ":Discards:FailedSpec")
      , theDiscards_FailedSpec_Busy(aSource + ":" + aClass + ":Discards:FailedSpec:Busy")
      , theDiscards_FailedSpec_Spin(aSource + ":" + aClass + ":Discards:FailedSpec:Spin") {
      theStallAccumulators.resize(kLastStallClass);
      for (int32_t i = 0; i < kLastStallClass; ++i) {
        theStallAccumulators[i] = new Stat::StatCounter(aSource + ":" + aClass + ":Bkd:" + kStallNames[i]);
      }
    }

    void applyTransaction( TimeXact const & aTransaction ) {
      switch (aTransaction.theType) {
        case TimeXact::eStall:
          theCycles += aTransaction.theCount;
          (*theStallAccumulators[aTransaction.theCycleClass]) += aTransaction.theCount;
          break;
        case TimeXact::eSpinRetire:
          theCommits  += aTransaction.theCount;
          theCommits_Spin += aTransaction.theCount;
          break;
        case TimeXact::eBusyRetire:
          theCommits  += aTransaction.theCount;
          theCommits_Busy += aTransaction.theCount;
          break;
      }
    }

    void applyTransaction( TimeXact const & aTransaction, eCycleClass anAlternateClass, eDiscardCause aDiscardCommits = ePreserve ) {
      switch (aTransaction.theType) {
        case TimeXact::eStall:
          theCycles += aTransaction.theCount;
          if (aTransaction.theCycleClass == kIdle_Busy || aTransaction.theCycleClass == kIdle_Stall) {
            (*theStallAccumulators[aTransaction.theCycleClass]) += aTransaction.theCount;
          } else {
            (*theStallAccumulators[anAlternateClass]) += aTransaction.theCount;
          }
          break;
        case TimeXact::eSpinRetire:
          switch (aDiscardCommits) {
            case ePreserve:
              theCommits  += aTransaction.theCount;
              theCommits_Spin += aTransaction.theCount;
              break;
            case eDiscard_Resync:
              theDiscards_Resync  += aTransaction.theCount;
              theDiscards_Resync_Spin += aTransaction.theCount;
              break;
            case eDiscard_FailedSpeculation:
              theDiscards_FailedSpec  += aTransaction.theCount;
              theDiscards_FailedSpec_Spin += aTransaction.theCount;
              break;
          }
          break;
        case TimeXact::eBusyRetire:
          switch (aDiscardCommits) {
            case ePreserve:
              theCommits  += aTransaction.theCount;
              theCommits_Busy += aTransaction.theCount;
              break;
            case eDiscard_Resync:
              theDiscards_Resync  += aTransaction.theCount;
              theDiscards_Resync_Busy += aTransaction.theCount;
              break;
            case eDiscard_FailedSpeculation:
              theDiscards_FailedSpec  += aTransaction.theCount;
              theDiscards_FailedSpec_Busy += aTransaction.theCount;
              break;
          }
          break;
      }
    }

  };

  std::vector<TimeClass *> theClasses;
  std::list<TimeXact> theTransactionQueue;

  TimeBreakdown(std::string const & aSource)
    : theSource(aSource)
    , theSkippedCycles( aSource + ":SkippedCycles" ) {
    theLastAccountedCycle = 0;
    theLastEnqueueSequence = 0;
    theLastTimeClass = 0;

    tmpStallCycles = 0; //for IStall stats
    tmpStoreStallCycles = 0; //for IStall stats
  }

  void skipCycle() {
    //Advance theLastAccountedCycle by one to "skip" a cycle from the next
    //accounting - do not create a transaction for the cycle
    theLastAccountedCycle++;
    theSkippedCycles++;
  }

  uint32_t addClass(std::string const & aClass) {
    theClasses.push_back( new TimeClass(theSource, aClass) );
    return theClasses.size() - 1;
  }

  void enqueueTransaction( TimeXact const & aTransaction ) {
    if (! theTransactionQueue.empty()) {
      DBG_Assert( aTransaction.theSequence >= theTransactionQueue.back().theSequence );
    }
    theTransactionQueue.push_back( aTransaction );
  }

  void applyTransactions( uint64_t aStopSequenceNumber) {
    while (! theTransactionQueue.empty() && theTransactionQueue.front().theSequence <= aStopSequenceNumber ) {
      theClasses[theTransactionQueue.front().theTimeClass]->applyTransaction(theTransactionQueue.front());
      theTransactionQueue.pop_front();
    }
  }

  void modifyAndApplyTransactions( uint64_t aStopSequenceNumber, eCycleClass anAlternateClass, eDiscardCause aDiscardCommits = ePreserve ) {
    while (! theTransactionQueue.empty() && theTransactionQueue.front().theSequence <= aStopSequenceNumber ) {
      theClasses[theTransactionQueue.front().theTimeClass]->applyTransaction(theTransactionQueue.front(), anAlternateClass, aDiscardCommits );
      theTransactionQueue.pop_front();
    }
  }
  void modifyAndApplyTransactionsBackwards( uint64_t aStopSequenceNumber, eCycleClass anAlternateClass, eDiscardCause aDiscardCommits = ePreserve ) {
    while (! theTransactionQueue.empty() && theTransactionQueue.back().theSequence >= aStopSequenceNumber ) {
      theClasses[theTransactionQueue.back().theTimeClass]->applyTransaction(theTransactionQueue.back(), anAlternateClass, aDiscardCommits );
      theTransactionQueue.pop_back();
    }
  }
  void modifyAndApplyAllTransactions( eCycleClass anAlternateClass, eDiscardCause aDiscardCommits = ePreserve ) {
    while (! theTransactionQueue.empty() ) {
      theClasses[theTransactionQueue.front().theTimeClass]->applyTransaction(theTransactionQueue.front(), anAlternateClass, aDiscardCommits );
      theTransactionQueue.pop_front();
    }
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

  int32_t accumulateStoreCyclesTmp( void ) {
    uint64_t cycles = theFlexus->cycleCount() - theLastAccountedCycle;
    theLastAccountedCycle = theFlexus->cycleCount();
    tmpStoreStallCycles += cycles;
    return cycles;
  }
  int32_t commitAccumulatedStoreCycles(eCycleClass aCycleClass) {
    uint64_t retCycles = stall(aCycleClass, tmpStoreStallCycles);
    tmpStoreStallCycles = 0;
    return retCycles;
  }
  int32_t commitAccumulatedStoreCycles(eCycleClass aCycleClass, uint64_t aCycles) {
    return stall(aCycleClass, aCycles);
  }

  int32_t accumulateCyclesTmp( void ) {
    uint64_t cycles = theFlexus->cycleCount() - theLastAccountedCycle;
    theLastAccountedCycle = theFlexus->cycleCount();
    tmpStallCycles += cycles;
    return cycles;
  }
  int32_t commitAccumulatedCycles(eCycleClass aCycleClass) {
    uint64_t retCycles = stall(aCycleClass, tmpStallCycles);
    tmpStallCycles = 0;
    return retCycles;
  }
  int32_t commitAccumulatedCycles(eCycleClass aCycleClass, uint64_t aCycles) {
    return stall(aCycleClass, aCycles);
  }

  //Enqueue stall cycles
  int32_t stall(eCycleClass aCycleClass) {
    uint64_t cycles = theFlexus->cycleCount() - theLastAccountedCycle;
    theLastAccountedCycle = theFlexus->cycleCount();
    stall(aCycleClass, cycles);
    return cycles;
  }
  int32_t stall( eCycleClass aCycleClass, uint64_t aNumCycles) {
    TimeXact xact;
    xact.theType = TimeXact::eStall;
    xact.theTimeClass = theLastTimeClass;
    xact.theCount = aNumCycles;
    xact.theCycleClass = aCycleClass;
    xact.theSequence = theLastEnqueueSequence;
    enqueueTransaction(xact);
    return aNumCycles;
  }

  int32_t retire(eCycleClass aPrecedingStallClass, uint64_t anInsnSequence, int32_t aTimeClass, bool isSpin) {
    bool count_retire_cycle = false;
    int32_t stall_cycles = 0;
    theLastTimeClass = aTimeClass;
    if (theLastAccountedCycle != theFlexus->cycleCount()) {
      count_retire_cycle = true;
      stall_cycles = theFlexus->cycleCount() - theLastAccountedCycle - 1;
      if (stall_cycles > 0) {
        stall( aPrecedingStallClass, stall_cycles );
      }
    }
    instruction( anInsnSequence, isSpin);
    if (count_retire_cycle) {
      if (aPrecedingStallClass == kIdle_Stall) {
        stall(kIdle_Busy, 1);
      } else {
        if (isSpin) {
          stall(kSpin_Busy, 1);
        } else {
          stall(kBusy, 1);
        }
      }
    }
    theLastAccountedCycle = theFlexus->cycleCount();
    return stall_cycles;
  }

  void instruction( uint64_t anInsnSequence, bool isSpin) {
    TimeXact xact;
    if (isSpin) {
      xact.theType = TimeXact::eSpinRetire;
    } else {
      xact.theType = TimeXact::eBusyRetire;
    }
    xact.theTimeClass = theLastTimeClass;
    xact.theCount = 1;
    xact.theCycleClass = kUnknown;
    xact.theSequence = anInsnSequence;
    theLastEnqueueSequence = anInsnSequence;
    enqueueTransaction(xact);
  }

};

} //End nXactTimeBreakdown

#endif //FLEXUS_XACT_TIME_BREAKDOWN_INCLUDED
