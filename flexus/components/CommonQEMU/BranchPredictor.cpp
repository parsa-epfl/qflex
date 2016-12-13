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
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <list>
#include <iostream>
#include <iomanip>
#include <fstream>

#include <core/boost_extensions/intrusive_ptr.hpp>
#include <core/boost_extensions/padded_string_cast.hpp>
#include <boost/throw_exception.hpp>
#include <boost/none.hpp>

#include <boost/dynamic_bitset.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
using namespace boost::multi_index;

#include <core/target.hpp>
#include <core/debug/debug.hpp>
#include <core/types.hpp>
#include <components/uFetch/uFetchTypes.hpp>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/tracking.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/export.hpp>

#include <core/stats.hpp>
namespace Stat = Flexus::Stat;

#include <components/CommonQEMU/BranchPredictor.hpp>

#define DBG_DefineCategories BPred
#define DBG_SetDefaultOps AddCat(BPred)
#include DBG_Control()

namespace Flexus {
namespace SharedTypes {

struct BTBEntry {
  VirtualMemoryAddress thePC;
  mutable eBranchType theBranchType;
  mutable VirtualMemoryAddress theTarget;
  BTBEntry( VirtualMemoryAddress aPC, eBranchType aType, VirtualMemoryAddress aTarget)
    : thePC( aPC )
    , theBranchType( aType )
    , theTarget( aTarget) {
  }
  BTBEntry( ) {}
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const uint32_t version) {
    ar & thePC;
    ar & theBranchType;
    ar & theTarget;
  }

};

struct by_baddr {};
typedef multi_index_container
< BTBEntry
, indexed_by
< sequenced<>
, ordered_unique
< tag<by_baddr>
, member< BTBEntry, VirtualMemoryAddress, &BTBEntry::thePC>
>
>
>
btb_set_t;

template<class Archive>
void save(Archive & ar, const btb_set_t & t, uint32_t version) {
  std::list<BTBEntry> btb;
  std::copy( t.begin(), t.end(), std::back_inserter(btb));
  // the hackish const is necessary to satisfy boost 1.33.1
  ar << (const std::list<BTBEntry>)btb;
}
template<class Archive>
void load(Archive & ar, btb_set_t & t, uint32_t version) {
  std::list<BTBEntry> btb;
  ar >> btb;
  std::copy( btb.begin(), btb.end(), std::back_inserter(t));
}

} //SharedTypes
} //Flexus

namespace boost {
namespace serialization {
template<class Archive>
inline void serialize( Archive & ar, Flexus::SharedTypes::btb_set_t & t, const uint32_t file_version ) {
  split_free(ar, t, file_version);
}
}
}

namespace Flexus {
namespace SharedTypes {

int64_t log2(int64_t aVal) {
  DBG_Assert(aVal != 0);
  --aVal;
  uint32_t ii = 0;
  while (aVal > 0) {
    ii++;
    aVal >>= 1;
  }
  return ii;
}

eDirection moreTaken( eDirection aDirection ) {
  if (aDirection == kStronglyTaken) {
    return kStronglyTaken;
  }
  return eDirection(aDirection - 1);
}

eDirection moreNotTaken( eDirection aDirection ) {
  if (aDirection == kStronglyNotTaken) {
    return kStronglyNotTaken;
  }
  return eDirection(aDirection + 1);
}

eDirection apply( eDirection aDirection, eDirection aState) {
  if (aDirection <= kTaken) {
    return moreTaken(aState);
  } else {
    return moreNotTaken(aState);
  }
}

eDirection reverse(eDirection direction) {
  if (direction >= kNotTaken) {
    direction = kTaken;
  } else {
    direction = kNotTaken;
  }
  return direction;
}

struct gShare {
  std::vector< eDirection > thePatternTable;
  uint32_t theShiftReg;
  int32_t theShiftRegSize;
  uint32_t thePatternMask;

private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const uint32_t version) {
    uint32_t shift_reg = theShiftReg;
    ar & thePatternTable;
    ar & theShiftRegSize;
    ar & thePatternMask;
    ar & shift_reg;
    theShiftReg = (theShiftReg & theShiftRegSize );
  }
  gShare( ) {}

public:
  gShare( int32_t aSize )
    : theShiftRegSize(aSize) {
    // Set the history pattern mask to 2^bits-1
    thePatternMask = ( 1 << aSize ) - 1;

    reset();
  }

  void reset() {
    thePatternTable.clear();
    theShiftReg = 0;
    eDirection direction = kNotTaken;
    for (int32_t i = 0; i < (1 << theShiftRegSize ); ++i) {
      thePatternTable.push_back(direction);
      direction = reverse(direction);
    }
  }

  //This hashing function was stolen from SimpleScalar.  It is quite
  //arbitrary.
  int32_t index( VirtualMemoryAddress anAddress) {
    return (( theShiftReg ) ^ (anAddress >> 2)) & thePatternMask;
  }

  int32_t index( VirtualMemoryAddress anAddress, uint32_t aShiftReg) {
    return (( aShiftReg ) ^ (anAddress >> 2)) & thePatternMask;
  }

  void shiftIn( eDirection aDirection ) {
    theShiftReg = ( theShiftReg << 1 ) & thePatternMask;
    if ( aDirection >= kNotTaken ) {
      theShiftReg |= 1;
    }
  }

  void setShiftReg(uint32_t aPreviousShiftRegState) {
    theShiftReg = aPreviousShiftRegState & thePatternMask;
  }

  uint32_t shiftReg() const {
    return theShiftReg;
  }

  eDirection direction( VirtualMemoryAddress anAddress) {
    return thePatternTable[index(anAddress)];
  }

  eDirection direction ( VirtualMemoryAddress anAddress, uint32_t aShiftRegState ) {
    return thePatternTable[index(anAddress, aShiftRegState)];
  }

  void update(VirtualMemoryAddress anAddress, eDirection aDirection) {
    thePatternTable[index(anAddress)] = aDirection;
  }

  void update(VirtualMemoryAddress anAddress, uint32_t aShiftRegState, eDirection aDirection) {
    thePatternTable[index(anAddress, aShiftRegState)] = aDirection;
  }

};

struct Bimodal {
  std::vector< eDirection > theTable;
  int32_t theSize;
  uint64_t theIndexMask;

private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const uint32_t version) {
    ar & theTable;
    ar & theSize;
    ar & theIndexMask;
  }
  Bimodal( ) {}

public:
  Bimodal( int32_t aSize )
    : theSize(aSize) {
    //aBTBSize must be a power of 2
    DBG_Assert( ((aSize - 1) & (aSize)) == 0);
    theIndexMask = aSize - 1;

    reset();
  }

  void reset() {
    theTable.clear();
    eDirection direction = kNotTaken;
    for (int32_t i = 0; i < theSize; ++i) {
      theTable.push_back(direction);
      direction = reverse(direction);
    }
  }

  //This hashing function was stolen from SimpleScalar.  It is quite
  //arbitrary.
  int32_t index( VirtualMemoryAddress anAddress) {
    return (( anAddress >> 19) ^ (anAddress >> 2)) & theIndexMask;
  }

  eDirection direction( VirtualMemoryAddress anAddress) {
    return theTable[index(anAddress)];
  }

  void update(VirtualMemoryAddress anAddress, eDirection aDirection) {
    theTable[index(anAddress)] = aDirection;
  }
};

struct BTB {
  std::vector< btb_set_t > theBTB;
  uint32_t theBTBSets;
  uint32_t theBTBAssoc;
  uint64_t theIndexMask;

private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const uint32_t version) {
    ar & theBTB;
    ar & theBTBSets;
    ar & theBTBAssoc;
    ar & theIndexMask;
  }
  BTB() {}

public:
  BTB( int32_t aBTBSets, int32_t aBTBAssoc )
    : theBTBSets(aBTBSets)
    , theBTBAssoc(aBTBAssoc) {
    //aBTBSize must be a power of 2
    DBG_Assert( ((aBTBSets - 1) & (aBTBSets)) == 0);
    theBTB.resize(aBTBSets);
    theIndexMask = aBTBSets - 1;
  }

  int32_t index( VirtualMemoryAddress anAddress) {
    // Shift address by 2, since we assume word-aligned PCs
    return (anAddress >> 2) & theIndexMask;
  }

  bool contains(VirtualMemoryAddress anAddress) {
    return (theBTB[index(anAddress)].get<by_baddr>().count(anAddress) > 0);
  }

  eBranchType type(VirtualMemoryAddress anAddress) {
    int32_t idx = index(anAddress);
    btb_set_t::index<by_baddr>::type::iterator iter = theBTB[idx].get<by_baddr>().find(anAddress);
    if (iter == theBTB[idx].get<by_baddr>().end()) {
      return kNonBranch;
    }
    return iter->theBranchType;
  }

  boost::optional<VirtualMemoryAddress> target(VirtualMemoryAddress anAddress) {
    int32_t idx = index(anAddress);
    btb_set_t::index<by_baddr>::type::iterator iter = theBTB[idx].get<by_baddr>().find(anAddress);
    if (iter == theBTB[idx].get<by_baddr>().end() ) {
      return boost::none;
    }
    return iter->theTarget;
  }

  bool update( VirtualMemoryAddress aPC, eBranchType aType, VirtualMemoryAddress aTarget) {
    int32_t idx = index(aPC);
    btb_set_t::index<by_baddr>::type::iterator iter = theBTB[idx].get<by_baddr>().find(aPC);
    if (iter != theBTB[idx].get<by_baddr>().end()) {
      if (aType == kNonBranch) {
        theBTB[idx].get<by_baddr>().erase(iter);
      } else {
        iter->theBranchType = aType;
        if (aTarget) {
          DBG_(Verb, ( << "BTB setting target for " << aPC << " to " << aTarget ) );
          iter->theTarget = aTarget;
        }
        theBTB[idx].relocate( theBTB[idx].end(), theBTB[idx].project<0>(iter));
      }
      return false; //not a new entry
    } else if (aType != kNonBranch) {
      if (theBTB[idx].size() >= theBTBAssoc) {
        theBTB[idx].pop_front();
      }
      DBG_(Verb, ( << "BTB adding new branch for " << aPC << " to " << aTarget ) );
      theBTB[idx].push_back( BTBEntry( aPC, aType, aTarget) );
      return true; //new entry
    }
    return false; //not a new entry

  }

  bool update( BranchFeedback const & aFeedback) {
    return update(aFeedback.thePC, aFeedback.theActualType, aFeedback.theActualTarget);
  }

};

struct CombiningImpl : public BranchPredictor {

  std::string theName;
  uint32_t theIndex;
  uint32_t theSerial;
  BTB theBTB;
  Bimodal theBimodal;
  Bimodal theMeta;
  gShare theGShare;

  Stat::StatCounter theBranches;          //Retired Branches
  Stat::StatCounter theBranches_Unconditional;
  Stat::StatCounter theBranches_Conditional;
  Stat::StatCounter theBranches_Call;
  Stat::StatCounter theBranches_Return;

  Stat::StatCounter thePredictions;
  Stat::StatCounter thePredictions_Bimodal;
  Stat::StatCounter thePredictions_GShare;
  Stat::StatCounter thePredictions_Unconditional;

  Stat::StatCounter theCorrect;
  Stat::StatCounter theCorrect_Bimodal;
  Stat::StatCounter theCorrect_GShare;
  Stat::StatCounter theCorrect_Unconditional;

  Stat::StatCounter theMispredict;
  Stat::StatCounter theMispredict_NewBranch;
  Stat::StatCounter theMispredict_Direction;
  Stat::StatCounter theMispredict_Meta;
  Stat::StatCounter theMispredict_MetaGShare;
  Stat::StatCounter theMispredict_MetaBimod;
  Stat::StatCounter theMispredict_Target;

  CombiningImpl( std::string const & aName, uint32_t anIndex )
    : theName(aName)
    , theIndex(anIndex)
    , theSerial(0)
    , theBTB( 2048, 16 )
    , theBimodal( 16384 )
    , theMeta( 16384 )
    , theGShare( 13 )
    , theBranches                      ( aName + "-branches" )
    , theBranches_Unconditional        ( aName + "-branches:unconditional" )
    , theBranches_Conditional          ( aName + "-branches:conditional" )
    , theBranches_Call                 ( aName + "-branches:call" )
    , theBranches_Return               ( aName + "-branches:return" )
    , thePredictions                   ( aName + "-predictions" )
    , thePredictions_Bimodal           ( aName + "-predictions:bimodal" )
    , thePredictions_GShare            ( aName + "-predictions:gshare" )
    , thePredictions_Unconditional     ( aName + "-predictions:unconditional" )
    , theCorrect                       ( aName + "-correct" )
    , theCorrect_Bimodal               ( aName + "-correct:bimodal" )
    , theCorrect_GShare                ( aName + "-correct:gshare" )
    , theCorrect_Unconditional         ( aName + "-correct:unconditional" )
    , theMispredict                    ( aName + "-mispredict" )
    , theMispredict_NewBranch          ( aName + "-mispredict:new" )
    , theMispredict_Direction          ( aName + "-mispredict:direction" )
    , theMispredict_Meta               ( aName + "-mispredict:meta" )
    , theMispredict_MetaGShare         ( aName + "-mispredict:meta:chose_gshare" )
    , theMispredict_MetaBimod          ( aName + "-mispredict:meta:chose_bimod" )
    , theMispredict_Target             ( aName + "-mispredict:target" )
  {}

  bool isBranch( VirtualMemoryAddress anAddress) {
    return theBTB.contains(anAddress);
  }

  VirtualMemoryAddress predictConditional( FetchAddr & aFetch ) {
    eDirection bimodal = theBimodal.direction( aFetch.theAddress );
    eDirection gshare = theGShare.direction( aFetch.theAddress );
    eDirection meta = theMeta.direction( aFetch.theAddress );
    eDirection prediction;

    // Decide which predictor to believe
    if (meta >= kNotTaken) {
      ++thePredictions;
      ++thePredictions_GShare;
      prediction = gshare;
    } else {
      ++thePredictions;
      ++thePredictions_Bimodal;
      prediction = bimodal;
    }

    //Record the predictor state/predictions
    aFetch.theBPState->thePrediction = prediction;
    aFetch.theBPState->theBimodalPrediction = bimodal;
    aFetch.theBPState->theMetaPrediction = meta;
    aFetch.theBPState->theGSharePrediction = gshare;
    aFetch.theBPState->theGShareShiftReg = theGShare.shiftReg();

    // Speculatively update the shift register with the prediction from the
    // gshare predictor.
    // TODO: evaluate whether it's better to shift in the
    // overall prediction or just the gshare part.
    theGShare.shiftIn ( gshare );

    DBG_ (Verb, ( << theIndex << "-BPRED-COND:  " << aFetch.theAddress << " BIMOD " << bimodal << " GSHARE " << gshare << " META " << meta << " OVERALL " << prediction ) );

    if (prediction <= kTaken && theBTB.target( aFetch.theAddress ) ) {
      return * theBTB.target(aFetch.theAddress );
    } else {
      return VirtualMemoryAddress(0);
    }
  }

  VirtualMemoryAddress predict( FetchAddr & aFetch ) {
    aFetch.theBPState = boost::intrusive_ptr<BPredState>(new BPredState() );
    aFetch.theBPState->thePredictedType = theBTB.type( aFetch.theAddress);
    aFetch.theBPState->theSerial = theSerial++;
    aFetch.theBPState->thePredictedTarget = VirtualMemoryAddress(0);
    aFetch.theBPState->thePrediction = kStronglyTaken;
    aFetch.theBPState->theBimodalPrediction = kStronglyTaken;
    aFetch.theBPState->theMetaPrediction = kStronglyTaken;
    aFetch.theBPState->theGSharePrediction = kStronglyTaken;
    aFetch.theBPState->theGShareShiftReg = 0;

    switch ( aFetch.theBPState->thePredictedType ) {
      case kNonBranch:
        aFetch.theBPState->thePredictedTarget = VirtualMemoryAddress(0);
        break;
      case kConditional:
        aFetch.theBPState->thePredictedTarget = predictConditional(aFetch);
        break;
      case kUnconditional:
        ++thePredictions;
        ++thePredictions_Unconditional;
        if (theBTB.target(aFetch.theAddress)) {
          aFetch.theBPState->thePredictedTarget = *theBTB.target(aFetch.theAddress);
        } else {
          aFetch.theBPState->thePredictedTarget = VirtualMemoryAddress(0);
        }
        aFetch.theBPState->theGShareShiftReg = theGShare.shiftReg();
        break;
      case kCall:
        ++thePredictions;
        ++thePredictions_Unconditional;
        if (theBTB.target(aFetch.theAddress)) {
          aFetch.theBPState->thePredictedTarget = *theBTB.target(aFetch.theAddress);
        } else {
          aFetch.theBPState->thePredictedTarget = VirtualMemoryAddress(0);
        }
        aFetch.theBPState->theGShareShiftReg = theGShare.shiftReg();
        //Need to push address onto retstack
        break;
      case kReturn:
        //Need to pop retstack
        break;
      default:
        aFetch.theBPState->thePredictedTarget = VirtualMemoryAddress(0);
        break;
    }

    if ( aFetch.theBPState->thePredictedType != kNonBranch ) {
      DBG_( Verb, ( << theIndex << "-BPRED-PREDICT: PC \t" << aFetch.theAddress
                    << " serial " << aFetch.theBPState->theSerial
                    << " Target \t" << aFetch.theBPState->thePredictedTarget
                    << "\tType " << aFetch.theBPState->thePredictedType ) );
    }

    return aFetch.theBPState->thePredictedTarget;
  }

  void stats(BranchFeedback const & aFeedback) {
    if (! aFeedback.theBPState || aFeedback.theActualType != aFeedback.theBPState-> thePredictedType) {
      ++theMispredict;
      ++theMispredict_NewBranch;
      DBG_( Verb, ( << "BPRED-RESOLVE Mispredict New-Branch " << aFeedback.theActualType << " @" << aFeedback.thePC << " " << aFeedback.theActualDirection << " to " << aFeedback.theActualTarget ) );
    } else {
      if ( aFeedback.theActualType  == kConditional) {
        if (( aFeedback.theBPState->thePrediction >= kNotTaken ) && ( aFeedback.theActualDirection >= kNotTaken )) {
          ++theCorrect;
          if (aFeedback.theBPState->theMetaPrediction >= kNotTaken) {
            DBG_( Verb, ( << "BPRED-RESOLVE Correct (GShare) " << aFeedback.theBPState->thePrediction << " " << aFeedback.theActualType << " @" << aFeedback.thePC << " TAKEN to " << aFeedback.theActualTarget ) );
            ++theCorrect_GShare;
          } else {
            DBG_( Verb, ( << "BPRED-RESOLVE Correct (Bimodal) " << aFeedback.theBPState->thePrediction << " " << aFeedback.theActualType << " @" << aFeedback.thePC << " TAKEN to " << aFeedback.theActualTarget  ) );
            ++theCorrect_Bimodal;
          }
        } else if (( aFeedback.theBPState->thePrediction <= kTaken ) && ( aFeedback.theActualDirection <= kTaken ) ) {
          if (aFeedback.theActualTarget == aFeedback.theBPState->thePredictedTarget) {
            ++theCorrect;
            if (aFeedback.theBPState->theMetaPrediction >= kNotTaken) {
              DBG_( Verb, ( << "BPRED-RESOLVE Correct (GShare) " << aFeedback.theBPState->thePrediction << " " << aFeedback.theActualType << " @" << aFeedback.thePC << " NOT TAKEN" ) );
              ++theCorrect_GShare;
            } else {
              DBG_( Verb, ( << "BPRED-RESOLVE Correct (Bimodal) " << aFeedback.theBPState->thePrediction << " " << aFeedback.theActualType << " @" << aFeedback.thePC << " NOT TAKEN" ) );
              ++theCorrect_Bimodal;
            }
          } else {
            DBG_( Verb, ( << "BPRED-RESOLVE Mispredict (Target) " << aFeedback.theBPState->thePrediction << " " << aFeedback.theActualType << " @" << aFeedback.thePC << " TAKEN to " << aFeedback.theActualTarget << " predicted target " << aFeedback.theBPState->thePredictedTarget ) );
            ++theMispredict;
            ++theMispredict_Target;
          }

        } else {
          ++theMispredict;
          if (( aFeedback.theBPState->theBimodalPrediction >= kNotTaken ) == ( aFeedback.theBPState->theGSharePrediction >= kNotTaken )) {
            DBG_( Verb, ( << "BPRED-RESOLVE Mispredict (Direction) " << aFeedback.theBPState->thePrediction << " " << aFeedback.theActualType << " @" << aFeedback.thePC << " " << aFeedback.theActualDirection << " to " << aFeedback.theActualTarget ) );
            ++theMispredict_Direction;
          } else {
            ++theMispredict_Meta;
            if ( (aFeedback.theBPState->thePrediction >= kNotTaken) == (aFeedback.theBPState->theGSharePrediction >= kNotTaken) ) {
              DBG_( Verb, ( << "BPRED-RESOLVE Mispredict (Meta:Gshare) " << aFeedback.theBPState->thePrediction << " " << aFeedback.theActualType << " @" << aFeedback.thePC << " " << aFeedback.theActualDirection << " to " << aFeedback.theActualTarget  ) );
              ++theMispredict_MetaGShare;
            } else {
              DBG_( Verb, ( << "BPRED-RESOLVE Mispredict (Meta:Bimod) " << aFeedback.theBPState->thePrediction << " " << aFeedback.theActualType << " @" << aFeedback.thePC << " " << aFeedback.theActualDirection << " to " << aFeedback.theActualTarget  ) );
              ++theMispredict_MetaBimod;
            }
          }
        }
      } else {
        //Unconditinal
        if (aFeedback.theActualTarget == aFeedback.theBPState->thePredictedTarget) {
          DBG_( Verb, ( << "BPRED-RESOLVE Correct (Unconditional) " << aFeedback.theActualType << " @" << aFeedback.thePC << " to " << aFeedback.theActualTarget  ) );
          ++theCorrect;
          ++theCorrect_Unconditional;
        } else {
          DBG_( Verb, ( << "BPRED-RESOLVE Mispredict (Uncond-Target) " << aFeedback.theActualType << " @" << aFeedback.thePC << " to " << aFeedback.theActualTarget << " predicted target " << aFeedback.theBPState->thePredictedTarget ) );
          ++theMispredict;
          ++theMispredict_Target;
        }
      }
    }

    switch (aFeedback.theActualType) {
      case kConditional:
        theBranches++;
        theBranches_Conditional++;
        break;
      case kUnconditional:
        theBranches++;
        theBranches_Unconditional++;
        break;
      case kCall:
        theBranches++;
        theBranches_Call++;
        break;
      case kReturn:
        theBranches++;
        theBranches_Return++;
        break;
      default:
        break;
    }
  }

  void feedback( BranchFeedback const & aFeedback) {
    stats(aFeedback);
    bool is_new = theBTB.update(aFeedback);

    if (aFeedback.theActualType == kConditional) {

      if (is_new) {
        theBimodal.update(aFeedback.thePC, aFeedback.theActualDirection);
        theGShare.shiftIn( aFeedback.theActualDirection );
      } else if ( aFeedback.theBPState ) {

        //Restore shift register and shift in the actual prediction
        if ( (aFeedback.theBPState->theGSharePrediction >= kNotTaken) !=
             (aFeedback.theActualDirection >= kNotTaken ) ) {
          theGShare.setShiftReg( aFeedback.theBPState->theGShareShiftReg );
          theGShare.shiftIn( aFeedback.theActualDirection );
        }

        //Update 2-bit counters
        //Get current counter values, being careful to use the shift register from before
        eDirection bimodal = theBimodal.direction( aFeedback.thePC );
        eDirection gshare = theGShare.direction ( aFeedback.thePC, aFeedback.theBPState->theGShareShiftReg );
        eDirection meta = theMeta.direction( aFeedback.thePC );

        //Modify the tables, being careful to use the shift registers used to originally make the prediction
        theBimodal.update(aFeedback.thePC, apply(aFeedback.theActualDirection, bimodal) );
        theGShare.update(aFeedback.thePC, aFeedback.theBPState->theGShareShiftReg, apply(aFeedback.theActualDirection, gshare ));

        if ( ( gshare >= kNotTaken ) != ( bimodal >= kNotTaken ) ) {
          //Need to update meta
          if ( (aFeedback.theActualDirection >= kNotTaken) == (gshare >= kNotTaken) ) {
            //More gshare
            theMeta.update(aFeedback.thePC, moreNotTaken( meta ) );
          } else {
            //More bimodal
            theMeta.update(aFeedback.thePC, moreTaken( meta ) );
          }
        }
      } //end !is_new
    } else if (aFeedback.theBPState && aFeedback.theBPState->thePredictedTarget != aFeedback.theActualTarget) {
      if (! is_new) {
        //Unconditional branch which missed its target.  Restore shift register to the time of the branch
        theGShare.setShiftReg( aFeedback.theBPState->theGShareShiftReg );
      } else {
        //New unconditional branch.  We do not know the shift register as of the time of the branch.
      }
    }

    if ( aFeedback.theBPState ) {
      DBG_(Verb, ( << theIndex << "-BPRED-FEEDBACK: PC \t" << aFeedback.thePC
                   << " serial " << aFeedback.theBPState->theSerial
                   << " Target \t" << aFeedback.theActualTarget
                   << "\tType " << aFeedback.theActualType << " dir " << aFeedback.theActualDirection
                   << " pred " << aFeedback.theBPState->thePrediction ) );
    } else {
      DBG_(Verb, ( << theIndex << "-BPRED-FEEDBACK: PC \t" << aFeedback.thePC
                   << " Target \t" << aFeedback.theActualTarget
                   << "\tType " << aFeedback.theActualType << " dir " << aFeedback.theActualDirection ) );
    }

    DBG_(Verb, ( << "Leaving feedback.") );
  }

  void saveStateBinary(std::string const & aDirName) const {
    std::string fname( aDirName);
    fname += "/bpred-" + boost::padded_string_cast < 2, '0' > (theIndex);
    std::ofstream ofs(fname.c_str(), std::ios::binary);
    boost::archive::binary_oarchive oa(ofs);

    oa << theBTB;
    oa << theBimodal;
    oa << theMeta;
    oa << theGShare;

    // close archive
    ofs.close();
  }

  void saveState(std::string const & aDirName) const {
    std::string fname( aDirName);
    fname += "/bpredtxt-" + boost::padded_string_cast < 2, '0' > (theIndex);
    std::ofstream ofs(fname.c_str());
    boost::archive::text_oarchive oa(ofs);

    oa << theBTB;
    oa << theBimodal;
    oa << theMeta;
    oa << theGShare;

    // close archive
    ofs.close();
  }

  void loadStateBinary(std::string const & aDirName) {
    std::string fname( aDirName);
    fname += "/bpred-" + boost::padded_string_cast < 2, '0' > (theIndex);
    std::ifstream ifs(fname.c_str(), std::ios::binary);
    if (ifs.good()) {
      try {
        boost::archive::binary_iarchive ia(ifs);

        ia >> theBTB;
        ia >> theBimodal;
        ia >> theMeta;
        ia >> theGShare;
        DBG_( Dev, ( << theName << " loaded branch predictor.  BTB size: " << theBTB.theBTBSets << " by " << theBTB.theBTBAssoc << " Bimodal size: " << theBimodal.theSize << " Meta size: " << theMeta.theSize << " Gshare size: " << theGShare.theShiftRegSize ) );

        ifs.close();
      } catch (...) {
        DBG_Assert( false, ( << "Unable to load bpred state" ) );
      }
    } else {
      DBG_(Dev, ( << "Unable to load bpred state " << fname << ". Using default state." ) );
    }
  }

  void loadState(std::string const & aDirName) {
    std::string fname( aDirName);
    fname += "/bpredtxt-" + boost::padded_string_cast < 2, '0' > (theIndex);
    std::ifstream ifs(fname.c_str());
    if (ifs.good()) {
      boost::archive::text_iarchive ia(ifs);

      ia >> theBTB;
      ia >> theBimodal;
      ia >> theMeta;
      ia >> theGShare;
      DBG_( Dev, ( << theName << " loaded branch predictor.  BTB size: " << theBTB.theBTBSets << " by " << theBTB.theBTBAssoc << " Bimodal size: " << theBimodal.theSize << " Meta size: " << theMeta.theSize << " Gshare size: " << theGShare.theShiftRegSize ) );

      ifs.close();
    } else {
      loadStateBinary(aDirName);
    }
  }

};

struct FastCombiningImpl : public FastBranchPredictor {

  std::string theName;
  uint32_t theIndex;
  uint32_t theSerial;
  BTB theBTB;
  Bimodal theBimodal;
  Bimodal theMeta;
  gShare theGShare;

  Stat::StatCounter theBranches;          //Retired Branches
  Stat::StatCounter theBranches_Unconditional;
  Stat::StatCounter theBranches_Conditional;
  Stat::StatCounter theBranches_Call;
  Stat::StatCounter theBranches_Return;

  Stat::StatCounter thePredictions;
  Stat::StatCounter thePredictions_Bimodal;
  Stat::StatCounter thePredictions_GShare;
  Stat::StatCounter thePredictions_Unconditional;

  Stat::StatCounter theCorrect;
  Stat::StatCounter theCorrect_Bimodal;
  Stat::StatCounter theCorrect_GShare;
  Stat::StatCounter theCorrect_Unconditional;

  Stat::StatCounter theMispredict;
  Stat::StatCounter theMispredict_NewBranch;
  Stat::StatCounter theMispredict_Direction;
  Stat::StatCounter theMispredict_Meta;
  Stat::StatCounter theMispredict_MetaGShare;
  Stat::StatCounter theMispredict_MetaBimod;
  Stat::StatCounter theMispredict_Target;

  FastCombiningImpl( std::string const & aName, uint32_t anIndex )
    : theName(aName)
    , theIndex(anIndex)
    , theSerial(0)
    , theBTB( 1024, 16 )
    , theBimodal( 32768 )
    , theMeta( 8192 )
    , theGShare( 13 )
    , theBranches                      ( aName + "-branches" )
    , theBranches_Unconditional        ( aName + "-branches:unconditional" )
    , theBranches_Conditional          ( aName + "-branches:conditional" )
    , theBranches_Call                 ( aName + "-branches:call" )
    , theBranches_Return               ( aName + "-branches:return" )
    , thePredictions                   ( aName + "-predictions" )
    , thePredictions_Bimodal           ( aName + "-predictions:bimodal" )
    , thePredictions_GShare            ( aName + "-predictions:gshare" )
    , thePredictions_Unconditional     ( aName + "-predictions:unconditional" )
    , theCorrect                       ( aName + "-correct" )
    , theCorrect_Bimodal               ( aName + "-correct:bimodal" )
    , theCorrect_GShare                ( aName + "-correct:gshare" )
    , theCorrect_Unconditional         ( aName + "-correct:unconditional" )
    , theMispredict                    ( aName + "-mispredict" )
    , theMispredict_NewBranch          ( aName + "-mispredict:new" )
    , theMispredict_Direction          ( aName + "-mispredict:direction" )
    , theMispredict_Meta               ( aName + "-mispredict:meta" )
    , theMispredict_MetaGShare         ( aName + "-mispredict:meta:chose_gshare" )
    , theMispredict_MetaBimod          ( aName + "-mispredict:meta:chose_bimod" )
    , theMispredict_Target             ( aName + "-mispredict:target" )
  {}

  VirtualMemoryAddress predictConditional(  VirtualMemoryAddress anAddress, BPredState & aBPState ) {
    eDirection bimodal = theBimodal.direction( anAddress );
    eDirection gshare = theGShare.direction( anAddress );
    eDirection meta = theMeta.direction( anAddress );
    eDirection prediction;

    // Decide which predictor to believe
    if (meta >= kNotTaken) {
      ++thePredictions;
      ++thePredictions_GShare;
      prediction = gshare;
    } else {
      ++thePredictions;
      ++thePredictions_Bimodal;
      prediction = bimodal;
    }

    //Record the predictor state/predictions
    aBPState.thePrediction = prediction;
    aBPState.theBimodalPrediction = bimodal;
    aBPState.theMetaPrediction = meta;
    aBPState.theGSharePrediction = gshare;
    aBPState.theGShareShiftReg = theGShare.shiftReg();

    // Speculatively update the shift register with the prediction from the
    // gshare predictor.
    // TODO: evaluate whether it's better to shift in the
    // overall prediction or just the gshare part.
    theGShare.shiftIn ( gshare );

    DBG_ (Verb, ( << theIndex << "-BPRED-COND:  " << anAddress << " BIMOD " << bimodal << " GSHARE " << gshare << " META " << meta << " OVERALL " << prediction ) );

    if (prediction <= kTaken && theBTB.target( anAddress ) ) {
      return * theBTB.target(anAddress );
    } else {
      return VirtualMemoryAddress(0);
    }
  }

  void predict( VirtualMemoryAddress anAddress, BPredState & aBPState ) {
    aBPState.thePredictedType = theBTB.type(anAddress);
    aBPState.theSerial = theSerial++;
    aBPState.thePredictedTarget = VirtualMemoryAddress(0);
    aBPState.thePrediction = kStronglyTaken;
    aBPState.theBimodalPrediction = kStronglyTaken;
    aBPState.theMetaPrediction = kStronglyTaken;
    aBPState.theGSharePrediction = kStronglyTaken;
    aBPState.theGShareShiftReg = 0;

    switch ( aBPState.thePredictedType ) {
      case kNonBranch:
        aBPState.thePredictedTarget = VirtualMemoryAddress(0);
        break;
      case kConditional:
        aBPState.thePredictedTarget = predictConditional(VirtualMemoryAddress(anAddress), aBPState);
        break;
      case kUnconditional:
        ++thePredictions;
        ++thePredictions_Unconditional;
        if (theBTB.target(anAddress)) {
          aBPState.thePredictedTarget = *theBTB.target(anAddress);
        } else {
          aBPState.thePredictedTarget = VirtualMemoryAddress(0);
        }
        aBPState.theGShareShiftReg = theGShare.shiftReg();
        break;
      case kCall:
        ++thePredictions;
        ++thePredictions_Unconditional;
        if (theBTB.target(anAddress)) {
          aBPState.thePredictedTarget = *theBTB.target(anAddress);
        } else {
          aBPState.thePredictedTarget = VirtualMemoryAddress(0);
        }
        aBPState.theGShareShiftReg = theGShare.shiftReg();
        //Need to push address onto retstack
        break;
      case kReturn:
        //Need to pop retstack
        break;
      default:
        aBPState.thePredictedTarget = VirtualMemoryAddress(0);
        break;
    }

    if ( aBPState.thePredictedType != kNonBranch ) {
      DBG_( Verb, ( << theIndex << "-BPRED-PREDICT: PC \t" << anAddress
                    << " serial " << aBPState.theSerial
                    << " Target \t" << aBPState.thePredictedTarget
                    << "\tType " << aBPState.thePredictedType ) );
    }
  }

  void stats(uint32_t anAddress,  eBranchType anActualType, eDirection anActualDirection, uint32_t anActualTarget, BPredState & aBPState) {
    if (anActualType != aBPState.thePredictedType) {
      ++theMispredict;
      ++theMispredict_NewBranch;
      DBG_( Verb, ( << "BPRED-RESOLVE Mispredict New-Branch " << anActualType << " @" << anAddress << " " << anActualDirection << " to " << anActualTarget ) );
    } else {
      if ( anActualType  == kConditional) {
        if (( aBPState.thePrediction >= kNotTaken ) && ( anActualDirection >= kNotTaken )) {
          ++theCorrect;
          if (aBPState.theMetaPrediction >= kNotTaken) {
            DBG_( Verb, ( << "BPRED-RESOLVE Correct (GShare) " << aBPState.thePrediction << " " << anActualType << " @" << anAddress << " TAKEN to " << anActualTarget ) );
            ++theCorrect_GShare;
          } else {
            DBG_( Verb, ( << "BPRED-RESOLVE Correct (Bimodal) " << aBPState.thePrediction << " " << anActualType << " @" << anAddress << " TAKEN to " << anActualTarget  ) );
            ++theCorrect_Bimodal;
          }
        } else if (( aBPState.thePrediction <= kTaken ) && ( anActualDirection <= kTaken ) ) {
          if (anActualTarget == static_cast<uint32_t>(aBPState.thePredictedTarget)) {
            ++theCorrect;
            if (aBPState.theMetaPrediction >= kNotTaken) {
              DBG_( Verb, ( << "BPRED-RESOLVE Correct (GShare) " << aBPState.thePrediction << " " << anActualType << " @" << anAddress << " NOT TAKEN" ) );
              ++theCorrect_GShare;
            } else {
              DBG_( Verb, ( << "BPRED-RESOLVE Correct (Bimodal) " << aBPState.thePrediction << " " << anActualType << " @" << anAddress << " NOT TAKEN" ) );
              ++theCorrect_Bimodal;
            }
          } else {
            DBG_( Verb, ( << "BPRED-RESOLVE Mispredict (Target) " << aBPState.thePrediction << " " << anActualType << " @" << anAddress << " TAKEN to " << anActualTarget << " predicted target " << aBPState.thePredictedTarget ) );
            ++theMispredict;
            ++theMispredict_Target;
          }

        } else {
          ++theMispredict;
          if (( aBPState.theBimodalPrediction >= kNotTaken ) == ( aBPState.theGSharePrediction >= kNotTaken )) {
            DBG_( Verb, ( << "BPRED-RESOLVE Mispredict (Direction) " << aBPState.thePrediction << " " << anActualType << " @" << anAddress << " " << anActualDirection << " to " << anActualTarget ) );
            ++theMispredict_Direction;
          } else {
            ++theMispredict_Meta;
            if ( (aBPState.thePrediction >= kNotTaken) == (aBPState.theGSharePrediction >= kNotTaken) ) {
              DBG_( Verb, ( << "BPRED-RESOLVE Mispredict (Meta:Gshare) " << aBPState.thePrediction << " " << anActualType << " @" << anAddress << " " << anActualDirection << " to " << anActualTarget  ) );
              ++theMispredict_MetaGShare;
            } else {
              DBG_( Verb, ( << "BPRED-RESOLVE Mispredict (Meta:Bimod) " << aBPState.thePrediction << " " << anActualType << " @" << anAddress << " " << anActualDirection << " to " << anActualTarget  ) );
              ++theMispredict_MetaBimod;
            }
          }
        }
      } else {
        //Unconditinal
        if (anActualTarget == static_cast<uint32_t>(aBPState.thePredictedTarget)) {
          DBG_( Verb, ( << "BPRED-RESOLVE Correct (Unconditional) " << anActualType << " @" << anAddress << " to " << anActualTarget  ) );
          ++theCorrect;
          ++theCorrect_Unconditional;
        } else {
          DBG_( Verb, ( << "BPRED-RESOLVE Mispredict (Uncond-Target) " << anActualType << " @" << anAddress << " to " << anActualTarget << " predicted target " << aBPState.thePredictedTarget ) );
          ++theMispredict;
          ++theMispredict_Target;
        }
      }
    }

    switch (anActualType) {
      case kConditional:
        theBranches++;
        theBranches_Conditional++;
        break;
      case kUnconditional:
        theBranches++;
        theBranches_Unconditional++;
        break;
      case kCall:
        theBranches++;
        theBranches_Call++;
        break;
      case kReturn:
        theBranches++;
        theBranches_Return++;
        break;
      default:
        break;
    }
  }

  void feedback( VirtualMemoryAddress anAddress,  eBranchType anActualType, eDirection anActualDirection, VirtualMemoryAddress anActualTarget, BPredState & aBPState) {
    stats(anAddress, anActualType, anActualDirection, anActualTarget, aBPState);
    bool is_new = theBTB.update(anAddress, anActualType, anActualTarget);

    if (anActualType == kConditional) {

      if (is_new) {
        theBimodal.update(anAddress, anActualDirection);
        theGShare.shiftIn( anActualDirection );
      } else if ( aBPState.thePredictedType != kNonBranch ) {

        //Restore shift register and shift in the actual prediction
        if ( (aBPState.theGSharePrediction >= kNotTaken) !=
             (anActualDirection >= kNotTaken ) ) {
          theGShare.setShiftReg( aBPState.theGShareShiftReg );
          theGShare.shiftIn( anActualDirection );
        }

        //Update 2-bit counters
        //Get current counter values, being careful to use the shift register from before
        eDirection bimodal = theBimodal.direction( anAddress );
        eDirection gshare = theGShare.direction ( anAddress, aBPState.theGShareShiftReg );
        eDirection meta = theMeta.direction( anAddress );

        //Modify the tables, being careful to use the shift registers used to originally make the prediction
        theBimodal.update(anAddress, apply(anActualDirection, bimodal) );
        theGShare.update(anAddress, aBPState.theGShareShiftReg, apply(anActualDirection, gshare ));

        if ( ( gshare >= kNotTaken ) != ( bimodal >= kNotTaken ) ) {
          //Need to update meta
          if ( (anActualDirection >= kNotTaken) == (gshare >= kNotTaken) ) {
            //More gshare
            theMeta.update(anAddress, moreNotTaken( meta ) );
          } else {
            //More bimodal
            theMeta.update(anAddress, moreTaken( meta ) );
          }
        }
      } //end !is_new
    } else if (aBPState.thePredictedType != kNonBranch && aBPState.thePredictedTarget != anActualTarget) {
      if (! is_new) {
        //Unconditional branch which missed its target.  Restore shift register to the time of the branch
        theGShare.setShiftReg( aBPState.theGShareShiftReg );
      } else {
        //New unconditional branch.  We do not know the shift register as of the time of the branch.
      }
    }

    DBG_(Verb, ( << theIndex << "-BPRED-FEEDBACK: PC \t" << anAddress
                 << " serial " << aBPState.theSerial
                 << " Target \t" << anActualTarget
                 << "\tType " << anActualType << " dir " << anActualDirection
                 << " pred " << aBPState.thePrediction ) );

    DBG_(Verb, ( << "Leaving feedback.") );
  }

  void saveStateBinary(std::string const & aDirName) const {
    std::string fname( aDirName);
    fname += "/bpred-" + boost::padded_string_cast < 2, '0' > (theIndex);
    std::ofstream ofs(fname.c_str(), std::ios::binary);
    boost::archive::binary_oarchive oa(ofs);

    oa << theBTB;
    oa << theBimodal;
    oa << theMeta;
    oa << theGShare;

    // close archive
    ofs.close();
  }

  void saveState(std::string const & aDirName) const {
    std::string fname( aDirName);
    fname += "/bpredtxt-" + boost::padded_string_cast < 2, '0' > (theIndex);
    std::ofstream ofs(fname.c_str());
    boost::archive::text_oarchive oa(ofs);

    oa << theBTB;
    oa << theBimodal;
    oa << theMeta;
    oa << theGShare;

    // close archive
    ofs.close();
  }

  void loadState(std::string const & aDirName) {
    std::string fname( aDirName);
    fname += "/bpredtxt-" + boost::padded_string_cast < 2, '0' > (theIndex);
    std::ifstream ifs(fname.c_str());
    if (ifs.good()) {
      boost::archive::text_iarchive ia(ifs);

      ia >> theBTB;
      ia >> theBimodal;
      ia >> theMeta;
      ia >> theGShare;
      DBG_( Dev, ( << theName << " loaded branch predictor.  BTB size: " << theBTB.theBTBSets << " by " << theBTB.theBTBAssoc << " Bimodal size: " << theBimodal.theSize << " Meta size: " << theMeta.theSize << " Gshare size: " << theGShare.theShiftRegSize ) );

      ifs.close();
    } else {
      loadStateBinary(aDirName);
      DBG_(Dev, ( << "Unable to load bpred state " << fname << ". Using default state." ) );
    }
  }

  void loadStateBinary(std::string const & aDirName) {
    std::string fname( aDirName);
    fname += "/bpred-" + boost::padded_string_cast < 2, '0' > (theIndex);
    std::ifstream ifs(fname.c_str(), std::ios::binary);
    if (ifs.good()) {
      try {
        boost::archive::binary_iarchive ia(ifs);

        ia >> theBTB;
        ia >> theBimodal;
        ia >> theMeta;
        ia >> theGShare;
        DBG_( Dev, ( << theName << " loaded branch predictor.  BTB size: " << theBTB.theBTBSets << " by " << theBTB.theBTBAssoc << " Bimodal size: " << theBimodal.theSize << " Meta size: " << theMeta.theSize << " Gshare size: " << theGShare.theShiftRegSize ) );

        ifs.close();
      } catch (...) {
        DBG_Assert( false, ( << "Unable to load bpred state" ) );
      }
    } else {
      DBG_(Dev, ( << "Unable to load bpred state " << fname << ". Using default state." ) );
    }
  }

};

BranchPredictor * BranchPredictor::combining(std::string const & aName, uint32_t anIndex) {
  return new CombiningImpl(aName, anIndex);
}

FastBranchPredictor * FastBranchPredictor::combining(std::string const & aName, uint32_t anIndex) {
  return new FastCombiningImpl(aName, anIndex);
}

std::ostream & operator << (std::ostream & anOstream, eBranchType aType) {
  char const * types[] = {
    "NonBranch"
    , "Conditional"
    , "Unconditional"
    , "Call"
    , "Return"
  };
  if (aType >= kLastBranchType) {
    anOstream << "InvalidBranchType(" << static_cast<int>(aType) << ")";
  } else {
    anOstream << types[aType];
  }
  return anOstream;
}

std::ostream & operator << (std::ostream & anOstream, eDirection aDir) {
  char const * dir[] = {
    "StronglyTaken"
    , "Taken"
    , "NotTaken"
    , "StronglyNotTaken"
  };
  if (aDir > kStronglyNotTaken) {
    anOstream << "InvalidBranchType(" << static_cast<int>(aDir) << ")";
  } else {
    anOstream << dir[aDir];
  }
  return anOstream;
}

} //SharedTypes
} //Flexus
