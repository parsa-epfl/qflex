// DO-NOT-REMOVE begin-copyright-block 
//
// Redistributions of any form whatsoever must retain and/or include the
// following acknowledgment, notices and disclaimer:
//
// This product includes software developed by Carnegie Mellon University.
//
// Copyright 2012 by Mohammad Alisafaee, Eric Chung, Michael Ferdman, Brian 
// Gold, Jangwoo Kim, Pejman Lotfi-Kamran, Onur Kocberber, Djordje Jevdjic, 
// Jared Smolens, Stephen Somogyi, Evangelos Vlachos, Stavros Volos, Jason 
// Zebchuk, Babak Falsafi, Nikos Hardavellas and Tom Wenisch for the SimFlex 
// Project, Computer Architecture Lab at Carnegie Mellon, Carnegie Mellon University.
//
// For more information, see the SimFlex project website at:
//   http://www.ece.cmu.edu/~simflex
//
// You may not use the name "Carnegie Mellon University" or derivations
// thereof to endorse or promote products derived from this software.
//
// If you modify the software you must place a notice on or within any
// modified version provided or made available to any third party stating
// that you have modified the software.  The notice shall include at least
// your name, address, phone number, email address and the date and purpose
// of the modification.
//
// THE SOFTWARE IS PROVIDED "AS-IS" WITHOUT ANY WARRANTY OF ANY KIND, EITHER
// EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO ANY WARRANTY
// THAT THE SOFTWARE WILL CONFORM TO SPECIFICATIONS OR BE ERROR-FREE AND ANY
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE,
// TITLE, OR NON-INFRINGEMENT.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
// BE LIABLE FOR ANY DAMAGES, INCLUDING BUT NOT LIMITED TO DIRECT, INDIRECT,
// SPECIAL OR CONSEQUENTIAL DAMAGES, ARISING OUT OF, RESULTING FROM, OR IN
// ANY WAY CONNECTED WITH THIS SOFTWARE (WHETHER OR NOT BASED UPON WARRANTY,
// CONTRACT, TORT OR OTHERWISE).
//
// DO-NOT-REMOVE end-copyright-block   
#include <memory>
#include <fstream>
#include <sstream>

#include <unordered_map>

#include "seq_map.hpp"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/member.hpp>

#include <boost/dynamic_bitset.hpp>

#include <components/CommonQEMU/Slices/PrefetchCommand.hpp>
#include <components/DecoupledFeederQEMU/QemuTracer.hpp>

using namespace boost::multi_index;

#define DBG_DefineCategories SpatialPrefetch
#define DBG_SetDefaultOps AddCat(SpatialPrefetch)
#include DBG_Control()

/**** Repet Types ****
 0  PC
 1  PC + block offset
 2  PC + address
 3  block address
 4  PC + word offset
 5  PC + byte offset
 6  PC + rotated block offset
 7  PC + rotated word offset
 8  block offset
 9  group address
*********************/

/**** CPT Types ****
 0  Perfect
 1  By SG
 2  By pattern
 3  By associativity (Chi-style)
*******************/

namespace nSpatialPrefetcher {

const char * RepetTypeStr[] = {"PC", "PCbloff", "PCaddr", "Addr", "PCwoff", "PCbyoff", "PCrot", "PCrotwd", "BLoff", "GRaddr"};
const char * CptTypeStr[] = {"Perfect", "By SG", "By pattern", "By assoc"};

using namespace Flexus;
using namespace Flexus::Core;
using namespace Flexus::SharedTypes;
namespace Stat = Flexus::Stat;

using boost::intrusive_ptr;

struct IntHash {
  std::size_t operator()(uint32_t key) const {
    key = key >> 6;
    return key;
  }
};

typedef boost::dynamic_bitset<> SpatialPattern;
typedef uint32_t PhtIndex;
typedef uint32_t CptIndex;
typedef uint32_t CptTag;
typedef uint32_t BlockAddress;
typedef uint32_t GroupAddress;
typedef uint32_t GroupOffset;
typedef uint32_t GroupIndex;
typedef uint32_t MemoryAddress;

bool operator < (const SpatialPattern & a, const SpatialPattern & b) {
  unsigned ii;
  for (ii = 0; ii < a.size(); ii++) {
    if (a.test(ii) && !b.test(ii)) {
      return true;
    }
    if (!a.test(ii) && b.test(ii)) {
      return false;
    }
  }
  return false;
}

typedef std::unordered_map<GroupAddress, SpatialPattern, IntHash> SpatialGroupUsage;
typedef SpatialGroupUsage::iterator GroupUsageIter;

void savePattern(std::ostream & ofs, SpatialPattern pattern) {
  uint32_t val = 0;
  uint32_t ii = 0;
  while (ii < pattern.size()) {
    if (pattern.test(ii)) {
      val |= (1 << (ii & 0x3));
    }
    ii++;
    if ((ii & 0x3) == 0) {
      ofs << val;
      val = 0;
    }
  }
}
void loadPattern(std::istream & ifs, SpatialPattern & pattern) {
  std::string str;
  ifs >> str;
  //DBG_(Dev, ( << "str=" << str ) );
  uint32_t base = 0;
  uint32_t ii = 0, jj;
  uint32_t temp;
  while (ii < (pattern.size() >> 2) ) {
    std::istringstream charstr(str.substr(ii, 1));
    charstr >> std::hex >> temp;
    //DBG_(Dev, ( << "temp=" << temp ) );
    for (jj = 0; jj < 4; jj++) {
      if (temp & (1 << jj)) {
        pattern.set(base + jj);
        //DBG_(Dev, ( << "setting bit " << std::dec << (base+jj) ) );
      }
    }
    base += 4;
    ii++;
  }
}

int64_t theSpatialGroupCount = 0;

typedef uint64_t RepetIndex;
struct SpatialGroupEntry {
  SpatialPattern pattern;
  SpatialPattern fills;
  SpatialPattern offChipFills;
  SpatialPattern prefetched;
  RepetIndex index;
  RepetIndex last_poke;
  uint64_t logical_time;
  bool finalized;
  int64_t unique_id;
  SpatialGroupEntry(SpatialPattern aPattern, RepetIndex anIndex)
    : pattern(aPattern)
    , fills(aPattern.size())
    , offChipFills(aPattern.size())
    , prefetched(aPattern.size())
    , index(anIndex)
    , last_poke(anIndex)
    , logical_time(0)
    , finalized(false)
    , unique_id( ++theSpatialGroupCount )
  {}
  SpatialGroupEntry(SpatialPattern aPattern, RepetIndex anIndex, SpatialPattern aFills)
    : pattern(aPattern)
    , fills(aFills)
    , offChipFills(aPattern.size())
    , prefetched(aPattern.size())
    , index(anIndex)
    , last_poke(anIndex)
    , logical_time(0)
    , finalized(false)
    , unique_id( ++theSpatialGroupCount )
  {}
  SpatialGroupEntry(SpatialPattern aPattern, SpatialPattern aFills)
    : pattern(aPattern)
    , fills(aFills)
    , offChipFills(aPattern.size())
    , prefetched(aPattern.size())
    , index(0)
    , last_poke(0)
    , logical_time(0)
    , finalized(false)
    , unique_id( ++theSpatialGroupCount )
  {}
  SpatialGroupEntry(std::istream & ifs, uint32_t sgpBlocks)
    : pattern(sgpBlocks)
    , fills(sgpBlocks)
    , offChipFills(sgpBlocks)
    , prefetched(sgpBlocks)
    , logical_time(0)
    , finalized(false)
    , unique_id( ++theSpatialGroupCount ) {
    ifs >> std::hex >> index;
    last_poke = index;
    //DBG_(Dev, ( << "read index=" << std::hex << index ) );
    loadPattern(ifs, pattern);
    loadPattern(ifs, fills);
    loadPattern(ifs, offChipFills);
  }
  bool hasIndex() const {
    return (index != 0);
  }
  friend std::ostream & operator << (std::ostream & ofs, const SpatialGroupEntry & entry) {
    //return ofs << entry.index << " " << entry.pattern << " "
    //           << entry.fills << " " << entry.offChipFills;
    ofs << entry.index << " ";
    savePattern(ofs, entry.pattern);
    ofs << " ";
    savePattern(ofs, entry.fills);
    ofs << " ";
    savePattern(ofs, entry.offChipFills);
    return ofs;
  }
};
typedef flexus_boost_seq_map<GroupAddress, SpatialGroupEntry> SpatialGroupRepet;
typedef SpatialGroupRepet::iterator GroupRepetIter;
typedef SpatialGroupRepet::seq_iter GroupRepetSeq;
typedef flexus_boost_set_assoc<GroupAddress, SpatialGroupEntry> SpatialGroupAssocRepet;
enum RepetReturnType {
  kRepetRetNone,
  kRepetRetNewGen,
  kRepetRetFirstAccess
};

class SpatialHistorySingle {
  SpatialPattern theHistory;
  uint64_t theLastAccess;
public:
  SpatialHistorySingle(SpatialPattern pattern, uint64_t access)
    : theHistory(pattern)
    , theLastAccess(access)
  {}
  SpatialHistorySingle(std::istream & line, uint32_t sgpBlocks)
    : theHistory(sgpBlocks) {
    loadPattern(line, theHistory);
    line >> std::hex >> theLastAccess;
    //DBG_(Dev, ( << "read lastAccess=" << std::hex << theLastAccess ) );
  }
  SpatialPattern extract() const {
    return theHistory;
  }
  void update(SpatialPattern pattern, uint64_t access) {
    theHistory = pattern;
    theLastAccess = access;
  }
  uint64_t lastAccess() const {
    return theLastAccess;
  }
  static std::string type() {
    return "Last";
  }
  friend std::ostream & operator << (std::ostream & ofs, const SpatialHistorySingle & hist) {
    savePattern(ofs, hist.theHistory);
    return ofs << " " << hist.theLastAccess;
  }
};
class SpatialHistoryUnion {
  SpatialPattern theHistory1;
  SpatialPattern theHistory2;
public:
  SpatialHistoryUnion(SpatialPattern pattern)
    : theHistory1(pattern)
    , theHistory2(pattern.size())
  {}
  SpatialPattern extract() const {
    return (theHistory1 | theHistory2);
  }
  void update(SpatialPattern pattern) {
    theHistory2 = theHistory1;
    theHistory1 = pattern;
  }
  static std::string type() {
    return "Union";
  }
};
class SpatialHistoryInter {
  SpatialPattern theHistory1;
  SpatialPattern theHistory2;
public:
  SpatialHistoryInter(SpatialPattern pattern)
    : theHistory1(pattern)
    , theHistory2(pattern.size())
  {}
  SpatialPattern extract() const {
    return (theHistory1 & theHistory2);
  }
  void update(SpatialPattern pattern) {
    theHistory2 = theHistory1;
    theHistory1 = pattern;
  }
  static std::string type() {
    return "Inter";
  }
};
class SpatialHistory2Bit {
  SpatialPattern theMSBs;
  SpatialPattern theLSBs;
public:
  SpatialHistory2Bit(SpatialPattern pattern)
    : theMSBs(pattern)
    , theLSBs(~pattern)
  {}
  SpatialPattern extract() const {
    return theMSBs;
  }
  void update(SpatialPattern pattern) {
    SpatialPattern msb = (pattern & ( theLSBs | theMSBs)) | (~pattern & ( theLSBs & theMSBs));
    SpatialPattern lsb = (pattern & (~theLSBs | theMSBs)) | (~pattern & (~theLSBs & theMSBs));
    theMSBs = msb;
    theLSBs = lsb;
  }
  static std::string type() {
    return "2Bit";
  }
};

typedef SpatialHistorySingle SpatialHistory;
typedef flexus_boost_seq_map<RepetIndex, SpatialHistory> GroupRepetHistory;
typedef GroupRepetHistory::iterator RepetHistoryIter;
typedef flexus_boost_set_assoc<RepetIndex, SpatialHistory> GroupRepetAssocHistory;

typedef flexus_boost_seq_map<uint8_t, uint8_t> OrderingMap; // <BlockNo,SeqNo>
typedef OrderingMap::iterator OrderingIter;
typedef OrderingMap::seq_iter OrderingSeqIter;
struct GroupOrderingEntry {
  OrderingMap theOrder;
  uint8_t theNextSeqNo;
  GroupOrderingEntry()
    : theNextSeqNo(0)
  {}
  GroupOrderingEntry(uint8_t blockNo)
    : theNextSeqNo(1) {
    theOrder.push_back( std::make_pair(blockNo, 0) ); // first seqNo is zero
  }
  void nextBlock(uint8_t blockNo) {
    DBG_Assert(theOrder.find(blockNo) == theOrder.end());
    theOrder.push_back( std::make_pair(blockNo, theNextSeqNo) );
    theNextSeqNo++;
  }
};
typedef std::unordered_map<GroupAddress, GroupOrderingEntry, IntHash> GroupOrderingBuild;
typedef GroupOrderingBuild::iterator OrderingBuildIter;
typedef std::unordered_map<RepetIndex, OrderingMap, IntHash> GroupOrderingHistory;
typedef GroupOrderingHistory::iterator OrderingHistoryIter;

struct CurrentStreamEntry {
  bool theDense;
  OrderingMap theOrder;
  SpatialPattern thePattern;
  uint8_t theBlockNo;
  int16_t theHits;
  GroupOffset theOffset;
  bool theIsRight;
  CurrentStreamEntry(OrderingMap order, uint8_t lastBlock, GroupOffset offset)
    : theDense(false)
    , theOrder(order)
    , theBlockNo(lastBlock)
    , theHits(0)
    , theOffset(offset)
  {}
  CurrentStreamEntry(SpatialPattern pattern, uint8_t nextBlock, GroupOffset offset, bool isRight)
    : theDense(true)
    , thePattern(pattern)
    , theBlockNo(nextBlock)
    , theHits(0)
    , theOffset(offset)
    , theIsRight(isRight)
  {}
  bool isDense() const {
    return theDense;
  }
  OrderingMap & order() {
    return theOrder;
  }
  SpatialPattern & pattern() {
    return thePattern;
  }
  uint8_t & block() {
    return theBlockNo;
  }
  GroupOffset offset() const {
    return theOffset;
  }
  bool isRight() const {
    return theIsRight;
  }
  void hit() {
    theHits++;
  }
  int16_t hits() const {
    return theHits;
  }
  int16_t possible() const {
    if (theDense) {
      return thePattern.count();
    } else {
      return theOrder.size();
    }
  }
};
typedef flexus_boost_seq_map<GroupAddress, CurrentStreamEntry> CurrentStreamTable;
typedef CurrentStreamTable::iterator CurrentStreamIter;
struct DoneStreamEntry {
  bool theDense;
  int16_t thePossible;
  int16_t theHits;
  DoneStreamEntry(bool dense, int16_t possible, int16_t hits)
    : theDense(dense)
    , thePossible(possible)
    , theHits(hits)
  {}
  bool isDense() {
    return theDense;
  }
  int16_t possible() {
    return thePossible;
  }
  void hit() {
    theHits++;
  }
  int16_t hits() {
    return theHits;
  }
};
typedef std::unordered_map<GroupAddress, DoneStreamEntry, IntHash> DoneStreamTable;
typedef DoneStreamTable::iterator DoneStreamIter;

struct GroupTimeEntry {
  bool live;
  uint64_t lastAccess;
  uint64_t lastEvict;
  uint64_t lastBegin;
  uint64_t lastEnd;
  GroupTimeEntry(uint64_t curr)
    : live(true)
    , lastAccess(curr)
    , lastEvict(curr)
    , lastBegin(curr)
    , lastEnd(curr)
  {}
};
typedef std::unordered_map<GroupAddress, GroupTimeEntry, IntHash> SpatialGroupTime;
typedef SpatialGroupTime::iterator GroupTimeIter;

struct ActiveGroupEntry {
  uint32_t start;
  uint32_t recent;
  SpatialPattern outstanding;
  ActiveGroupEntry(uint32_t curr, SpatialPattern initial)
    : start(curr)
    , recent(curr)
    , outstanding(initial)
  {}
};
typedef std::unordered_map<GroupAddress, ActiveGroupEntry, IntHash> SpatialGroupActive;
typedef SpatialGroupActive::iterator ActiveGroupIter;

enum CacheBlockState {
  ePrefetching,
  eFilling,
  ePrefetched,
  ePresent
};
const char * CacheStateStr[] = {"Prefetching", "Filling", "Prefetched", "Present"};
typedef std::unordered_map<BlockAddress, CacheBlockState, IntHash> CacheTable;
typedef CacheTable::iterator CacheIter;
typedef std::pair<CacheIter, bool> CacheInsert;

class SpatialPrefetcher {

public:
  SpatialPrefetcher( std::string statName, std::string altLoadName, int32_t aNode )
    : theName(statName)
    , theAltLoadName(altLoadName)
    , theNodeId(aNode)
    , theNumAccesses(0)
    , theCounter(0)
    , theCptSparseCount(0)
    , theCurrDense(false)
    , theStitchReady(false)
    , statUsageAccess(statName + "-UsageAccess")
    , statUsageFill(statName + "-UsageFill")
    , statUsageOffChip(statName + "-UsageOffChip")
    , statRepetYes(statName + "-RepetYes")
    , statRepetCountYes(statName + "-RepetCountYes")
    , statRepetNo(statName + "-RepetNo")
    , statRepetCountNo(statName + "-RepetCountNo")
    , statRepetHit(statName + "-RepetHit")
    , statRepetTraining(statName + "-RepetTraining")
    , statRepetMispred(statName + "-RepetMispred")
    , statRepetMiss(statName + "-RepetMisses")
    , statRepetFillsYes(statName + "-RepetFills")
    , statRepetFillsNo(statName + "-RepetFillNo")
    , statRepetFillHit(statName + "-RepetFillHit")
    , statRepetFillTraining(statName + "-RepetFillTraining")
    , statRepetFillMispred(statName + "-RepetFillMispred")
    , statRepetCovGen(statName + "-RepetCovGen")
    , statRepetOffChipYes(statName + "-RepetOffChipYes")
    , statRepetOffChipNo(statName + "-RepetOffChipNo")
    , statRepetOffChipHit(statName + "-RepetOffChipHit")
    , statRepetOffChipTraining(statName + "-RepetOffChipTraining")
    , statRepetOffChipMispred(statName + "-RepetOffChipMispred")
    , statRepetAccessFill(statName + "-RepetAccessFill")
    , statRepetOnlyAccess(statName + "-RepetOnlyAccess")
    , statRepetOnlyFill(statName + "-RepetOnlyFill")
    , statRepetUsageAccess(statName + "-RepetzAccessFill")
    , statRepetUsageFill(statName + "-RepetzUsageFill")
    , statRepetUsageOffChip(statName + "-RepetzUsageOffChip")
    , statRepetUsageCov(statName + "-RepetzUsageCov")
    , statRepetSparseNew(statName + "-RepetSparseNew")
    , statRepetSparseRemove(statName + "-RepetSparseRemove")
    , statRepetEndGenNoIndex(statName + "-RepetEndGenNoIndex")
    , statRepetEvictCptNoIndex(statName + "-RepetEvictCptNoIndex")
    , statRepetLateIndex(statName + "-RepetLateIndex")
    , statRepetNextGenNoIndex(statName + "-RepetNextGenNoIndex")
    , statRepetChangeIndex(statName + "-RepetChangeIndex")
    , statRepetTriggerIndex(statName + "-RepetTriggerIndex")
    , statRepetEndGenNoIndexAccesses(statName + "-RepetEndGenNoIndexAccesses")
    , statRepetEndGenNoIndexMisses(statName + "-RepetEndGenNoIndexMisses")
    , statRepetEvictCptNoIndexAccesses(statName + "-RepetEvictCptNoIndexAccesses")
    , statRepetEvictCptNoIndexMisses(statName + "-RepetEvictCptNoIndexMisses")
    , statRepetLateIndexAccesses(statName + "-RepetLateIndexAccesses")
    , statRepetLateIndexMisses(statName + "-RepetLateIndexMisses")
    , statRepetNextGenNoIndexAccesses(statName + "-RepetNextGenNoIndexAccesses")
    , statRepetNextGenNoIndexMisses(statName + "-RepetNextGenNoIndexMisses")
    , statRepetPattern(statName + "-RepetPattern")
    , statRepetPatternDensity(statName + "-RepetPatternDensity")
    , statRepetActualFillHits(statName + "-RepetActualFillHits")
    , statRepetActualHitGens(statName + "-RepetActualHitGens")
    , statParallelGroups(statName + "-ParallelGroups")
    , statOrderDist(statName + "-OrderDist")
    , statOrderBaseHitsDense(statName + "-OrderBaseHitsDense")
    , statOrderBaseHitsSparse(statName + "-OrderBaseHitsSparse")
    , statOrderBaseLenDense(statName + "-OrderBaseLenDense")
    , statOrderBaseLenSparse(statName + "-OrderBaseLenSparse")
    , statOrderDeltasDense(statName + "-OrderDeltasDense")
    , statOrderDeltasSparse(statName + "-OrderDeltasSparse")
    , statOrderRightJumpsDense(statName + "-OrderRightJumpsDense")
    , statOrderRightJumpsSparse(statName + "-OrderRightJumpsSparse")
    , statOrderLeftJumpsDense(statName + "-OrderLeftJumpsDense")
    , statOrderLeftJumpsSparse(statName + "-OrderLeftJumpsSparse")
    , statOrderSameDirJumpsDense(statName + "-OrderSameDirJumpsDense")
    , statOrderSameDirJumpsSparse(statName + "-OrderSameDirJumpsSparse")
    , statOrderDiffDirJumpsDense(statName + "-OrderDiffDirJumpsDense")
    , statOrderDiffDirJumpsSparse(statName + "-OrderDiffDirJumpsSparse")
    , statOrderDenseSeqOrdYesYes(statName + "-OrderDenseSeqOrdYesYes")
    , statOrderDenseSeqOrdYesNo(statName + "-OrderDenseSeqOrdYesNo")
    , statOrderDenseSeqOrdNoYes(statName + "-OrderDenseSeqOrdNoYes")
    , statOrderDenseSeqOrdNoNo(statName + "-OrderDenseSeqOrdNoNo")
    , statOrderSparseSeqOrdYesYes(statName + "-OrderSparseSeqOrdYesYes")
    , statOrderSparseSeqOrdYesNo(statName + "-OrderSparseSeqOrdYesNo")
    , statOrderSparseSeqOrdNoYes(statName + "-OrderSparseSeqOrdNoYes")
    , statOrderSparseSeqOrdNoNo(statName + "-OrderSparseSeqOrdNoNo")
    , statBufFetchMissRead(statName + "-BufFetchReadMisses")
    , statBufFetchMissWrite(statName + "-BufFetchWriteMisses")
    , statBufFetchPrefetch(statName + "-BufFetchPrefetches")
    , statBufFetchDupFetch(statName + "-BufFetchDupFetches")
    , statBufFetchDupStitch(statName + "-BufFetchDupStitch")
    , statBufFetchGoodRead(statName + "-BufFetchGoodReadFetches")
    , statBufFetchGoodWrite(statName + "-BufFetchGoodWriteFetches")
    , statBufFetchGoodReadOS(statName + "-BufFetchGoodReadOS")
    , statBufFetchGoodWriteOS(statName + "-BufFetchGoodWriteOS")
    , statBufFetchAccessRead(statName + "-BufFetchGoodReadAccesses")
    , statBufFetchAccessWrite(statName + "-BufFetchGoodWriteAccesses")
    , statBufFetchInval(statName + "-BufFetchInvalFetches")
    , statBufFetchDiscard(statName + "-BufFetchDiscardFetches")
    , statBufFetchTriggers(statName + "-BufFetchTriggers")
    , statBufFetchGens(statName + "-BufFetchGensWithPrefetch")
    , statBufFetchGenSize(statName + "-BufFetchGenSize")
    , statBufFetchDist(statName + "-BufFetchDistanceToUse")
    , statBufFetchDenseDist(statName + "-BufFetchDenseDistance")
    , statBufFetchSparseDist(statName + "-BufFetchSparseDistance")
    , statStreamNewStream(statName + "-StreamNewStream")
    , statStreamShortStream(statName + "-StreamShortStream")
    , statStreamReplaceStream(statName + "-StreamReplaceStream")
    , statStreamEndStream(statName + "-StreamEndStream")
    , statStreamOrphanHit(statName + "-StreamOrphanHit")
    , statStreamLenDense(statName + "-StreamLenDense")
    , statStreamLenSparse(statName + "-StreamLenSparse")
    , statStreamLenPossibleDense(statName + "-StreamLenPossibleDense")
    , statStreamLenPossibleSparse(statName + "-StreamLenPossibleSparse")
    , statTimeInterAccess(statName + "-TimeInterAccess")
    , statTimeInterDeadAccess(statName + "-TimeInterDeadAccess")
    , statTimeInterEvict(statName + "-TimeInterEvict")
    , statTimeInterLiveEvict(statName + "-TimeInterLiveEvict")
    , statTimeGroupLive(statName + "-TimeGroupLive")
    , statTimeGroupDead(statName + "-TimeGroupDead")
    , statTimeInterBegin(statName + "-TimeInterBegin")
    , statTimeInterEnd(statName + "-TimeInterEnd")
    , statPrefetchYes(statName + "-PrefetchYes")
    , statPrefetchNo(statName + "-PrefetchNo")
    , statPrefetchCount(statName + "-PrefetchCount")
    , statFills(statName + "-Fills")
    , statPrefetches(statName + "-Prefetches")
    , statCorrect(statName + "-GoodPrefetches")
    , statPartial(statName + "-PartialPrefetches")
    , statLate(statName + "-LatePrefetches")
    , statDup(statName + "-DuplicatePrefetches")
    , statMispredictEvicts(statName + "-EvictPrefetches")
    , statMispredictInvals(statName + "-InvalPrefetches")
    , statEvictions(statName + "-Evictions")
    , statInvalidations(statName + "-Invalidations")
    , statInserts(statName + "-Insertions")
  {}

  void init(bool enableUsageStats, bool enableRepetStats, bool enableBufFetch,
            bool enableTimeRepet, bool enablePrefetch, bool enableActive,
            bool enableOrdering, bool enableStreaming,
            int32_t blockSize, int32_t sgpBlocks, int32_t repetType, bool repetFills,
            bool sparseOpt, int32_t phtSize, int32_t phtAssoc, int32_t pcBits,
            int32_t cptType, int32_t cptSize, int32_t cptAssoc, bool cptSparse,
            bool fetchDist, int32_t streamWindow, bool streamDense, bool sendStreams,
            int32_t bufSize, int32_t streamDescs, bool delayedCommits, int32_t cptFilterSize) {
    DBG_(Dev, ( << theName << ": Initializing SGP - spatial group: " << sgpBlocks << " " << blockSize << "B blocks" ) );
    theBlockSize = blockSize;
    theSgpBlocks = sgpBlocks;
    theRepetType = repetType;
    theRepetFills = repetFills;
    theSparseOpt = sparseOpt;
    thePhtSize = phtSize;
    thePhtAssoc = phtAssoc;
    thePcBits = pcBits;
    theCptType = cptType;
    theCptSize = cptSize;
    theCptAssoc = cptAssoc;
    theCptFilterSize = cptFilterSize;
    theCptSparse = cptSparse;
    theFetchDist = fetchDist;
    theStreamWindow = streamWindow;
    theStreamDense = streamDense;
    theSendStreams = sendStreams;
    theBufSize = bufSize;
    theStreamDescs = streamDescs;
    theDelayedCommits = delayedCommits;
    theEnableUsage = enableUsageStats;
    theEnableRepet = enableRepetStats;
    theEnableBufFetch = enableBufFetch;
    theEnableTimeRepet = enableTimeRepet;
    theEnablePrefetch = enablePrefetch;
    theEnableActive = enableActive;
    theEnableOrdering = enableOrdering;
    theEnableStreaming = enableStreaming;
    theEnableStitch = true;

    if (theEnableBufFetch) {
      DBG_Assert(theEnableRepet, ( << "Error: repet must be enabled for local buffer prefetching" ) );
    }

    if (theEnablePrefetch) {
      DBG_Assert(theEnableBufFetch, ( << "Error: buffer prefetching must be enabled for actual prefetching" ) );
    }

    if (theEnableStreaming) {
      DBG_Assert(theEnableBufFetch, ( << "Error: buffer prefetching must be enabled for streaming" ) );
      DBG_Assert(theEnableOrdering, ( << "Error: ordering must be enabled for streaming" ) );
    }

    if (theEnableActive) {
      DBG_Assert(theEnableBufFetch, ( << "Error: buffer prefetching must be enabled for active tracking" ) );
    }

    theBlockOffsetMask = theBlockSize - 1;
    theBlockBits = mylog2(theBlockSize);

    theSgpBits = mylog2(theSgpBlocks);

    theWordBits = mylog2(theBlockSize * theSgpBlocks / 8);
    theByteBits = mylog2(theBlockSize * theSgpBlocks);

    theWordInBlockBits = mylog2(theBlockSize / 8);

    thePCmask = (1ULL << thePcBits) - 1;

    DBG_(Iface, ( << "blockOffsetMask:" << std::hex << theBlockOffsetMask << std::dec
                  << " blockBits:" << theBlockBits << " sgpBits:" << theSgpBits
                  << " wordBits:" << theWordBits << " byteBits:" << theByteBits ) );

    if (theEnableUsage) {
      theMinSgpBits = 1;
      //theMaxSgpBits = mylog2(cacheSize / blockSize);
      theMaxSgpBits = 6;
      theGroupUsages.resize(theMaxSgpBits + 1);

      unsigned ii;
      for (ii = 0; ii <= theMaxSgpBits; ii++) {
        std::string name(theName);
        name += "-" + std::to_string(theBlockSize << ii) + "B";
        name += "-" + std::to_string(theBlockSize) + "B";
        theUsageStats.push_back(new Stat::StatInstanceCounter<int64_t>(name));
      }
    }

    if (theEnableOrdering) {
      DBG_Assert(theEnableRepet);
      DBG_Assert(theSgpBlocks <= 256);  // otherwise too big for uint8_t
      DBG_Assert( (theCptType == 0) || (theCptType == 1) );
    }

    if (theEnableStreaming) {
      DBG_Assert(theStreamWindow > 0);
    }

    if (theBufSize > 0) {
      DBG_Assert(theFetchDist);
    }

    if (theCptSparse) {
      DBG_Assert(!theRepetFills, ( << "theCptSparseCount not updated correctly for patterns with zero bits set" ) );
    }

    if (theEnableRepet) {
      DBG_Assert( (theRepetType >= 0) && (theRepetType < 10) );
      DBG_Assert( (theCptType >= 0) && (theCptType < 4) );

      if (thePhtAssoc > 0) {
        DBG_Assert(thePhtSize > 0);

        uint32_t theUsefulBottomBits;
        switch (theRepetType) {
          case 0:  // PC only
            theUsefulBottomBits = 0;
            break;
          case 1:  // PC + block offset within SG
            theUsefulBottomBits = theSgpBits;
            break;
          case 2:  // PC + address
            theUsefulBottomBits = 0;
            break;
          case 4:  // PC + word offset within SG
            theUsefulBottomBits = theWordBits;
            break;
          case 6:  // PC + rotated block offset
            theUsefulBottomBits = 0;
            break;
          case 7:  // PC + rotated word offset
            theUsefulBottomBits = theWordInBlockBits;
            break;
          default:
            DBG_Assert(false, ( << "unsupported repet type: " << theRepetType ) );
        }

        theRepetAssocHistory.init(thePhtSize, thePhtAssoc, theUsefulBottomBits);
      }

      switch (theCptType) {
        case 0:
          DBG_Assert(theCptSize == 0);
          DBG_Assert(theCptAssoc == 0);
          DBG_Assert(theCptFilterSize == 0);
          break;
        case 1:
          DBG_Assert(theCptAssoc == 0);
          if (theCptFilterSize > 0) {
            DBG_Assert(theCptSparse);
          }
          break;
        case 2:
          DBG_Assert(false, ( << "CPT restrict by pattern not supported yet" ) );
          break;
        case 3:
          DBG_Assert(theCptSize > 0);
          DBG_Assert(theCptAssoc > 0);
          DBG_Assert(!theCptSparse);
          theGroupAssocRepets.init(theCptSize / (theBlockSize * theSgpBlocks), theCptAssoc, 0);
          break;
        default:
          DBG_Assert(false, ( << "unsupported CPT type: " << theCptType ) );
      }

      Stat::StatAnnotation * repetTypeStat = new Stat::StatAnnotation(theName + "-RepetType");
      *repetTypeStat << RepetTypeStr[theRepetType];
      Stat::StatAnnotation * repetHistoryStat = new Stat::StatAnnotation(theName + "-RepetHistory");
      *repetHistoryStat << SpatialHistory::type();
      Stat::StatAnnotation * cptTypeStat = new Stat::StatAnnotation(theName + "-CptType");
      *cptTypeStat << CptTypeStr[theCptType];
    }

    std::string fname( theName );
    fname += "-regions.out";
    //theRegionFile.open(fname.c_str());
  }

  void finalize() {
    /*
    if(theEnableRepet && thePhtAssoc > 0) {
      std::vector<int> & counts = theRepetAssocHistory.get_counts();
      uint32_t ii;
      for(ii = 0; ii < theRepetAssocHistory.sets(); ii++) {
        theRegionFile << counts[ii] << std::endl;
      }
    }
    theRegionFile.close();
    */

    if (theEnableActive) {
      ActiveGenIter begins = theActiveGenBegins.begin();
      ActiveGenIter ends = theActiveGenEnds.begin();

      // consider all outstanding generations to end at the
      // time of their latest hit
      ActiveGroupIter iter = theActiveGroups.begin();
      for (; iter != theActiveGroups.end(); ++iter) {
        finalizeActiveGroup(iter->second);
      }

      int32_t parallel = 0;
      uint32_t curr;
      for (curr = 0; curr < theNumAccesses; curr++) {
        //DBG_(Dev, ( << "curr=" << curr << " parallel=" << parallel ) );
        if (begins != theActiveGenBegins.end()) {
          DBG_Assert(begins->first >= curr, ( << "curr=" << curr << " begins=" << begins->first) );
          //DBG_(Dev, ( << "begins=" << begins->first << " count=" << begins->second ) );
          if (begins->first == curr) {
            parallel += begins->second;
            ++begins;
          }
        }
        if (ends != theActiveGenEnds.end()) {
          DBG_Assert(ends->first >= curr, ( << "curr=" << curr << " ends=" << ends->first) );
          //DBG_(Dev, ( << "ends=" << ends->first << " count=" << ends->second ) );
          if (ends->first == curr) {
            parallel -= ends->second;
            ++ends;
          }
        }
        statParallelGroups << std::make_pair(parallel, 1);
      }

      /*
      // for verification purposes only
      uint32_t trueLast = 0;
      ActiveGenIter iter2 = theActiveGenBegins.end();
      --iter2;
      if(iter2->first > trueLast) {
        trueLast = iter2->first;
      }
      iter2 = theActiveGenEnds.end();
      --iter2;
      if(iter2->first > trueLast) {
        trueLast = iter2->first;
      }
      for(; curr <= trueLast; curr++) {
        if(begins != theActiveGenBegins.end()) {
          DBG_Assert(begins->first >= curr, ( << "curr="<<curr<<" begins="<<begins->first) );
          if(begins->first == curr) {
            parallel += begins->second;
            ++begins;
          }
        }
        if(ends != theActiveGenEnds.end()) {
          DBG_Assert(ends->first >= curr, ( << "curr="<<curr<<" ends="<<ends->first) );
          if(ends->first == curr) {
            parallel -= ends->second;
            ++ends;
          }
        }
      }
      DBG_Assert(parallel == 0, ( << "parallel=" << parallel << " curr=" << curr ) );
      DBG_(Dev, ( << "last=" << last << " trueLast=" << trueLast << " numAccesses=" << theNumAccesses ) );
      */

    }  // end if EnableActive

    RepetHistoryIter iter = theRepetAssocHistory.index_begin();
    while (iter != theRepetAssocHistory.index_end()) {
      statRepetPatternDensity << std::make_pair(countBits(iter->second.extract()), 1);
      theRepetAssocHistory.index_next(iter);
    }

    if (theEnableRepet) {
      return;
      std::cerr << "finalizing this SGP" << std::endl;
      uint32_t ii;
      RepetHistoryIter iter = theRepetHistory.begin();
      for (; iter != theRepetHistory.end(); ++iter) {
        MemoryAddress pc = extractPC(iter->first, theSgpBits);
        GroupOffset offset = extractGroupOffset(iter->first, theSgpBits);
        std::cout << std::hex << pc << "," << std::dec << offset << "  ";
        for (ii = 0; ii < theSgpBlocks; ii++) {
          std::cout << ( (iter->second.extract()).test(ii) ? "1" : "0" );
        }
        std::cout << std::endl;
      }
    }
  }

  void saveState(std::string const & aDirName) {
    std::string fname( aDirName );
    fname += "/" + theName;
    std::ofstream ofs(fname.c_str());

    saveState ( ofs );

    ofs.close();
  }

  void loadState(std::string const & aDirName) {
    std::string fname1 = aDirName + "/" + theName;
    std::string fname2 = aDirName + "/" + theAltLoadName;
    std::string fname3 = fname2 + "L1";

    std::ifstream ifs(fname1.c_str());
    std::string fname = fname1;
    if (! ifs.good()) {
      ifs.clear();
      ifs.open(fname2.c_str());
      fname = fname2;
    }
    if (! ifs.good()) {
      ifs.clear();
      ifs.open(fname3.c_str());
      fname = fname3;
    }

    if (! ifs.good()) {
      if (theEnablePrefetch) {
        DBG_( Dev, ( << " saved checkpoint states " << fname1 << ", " << fname2 << " and " << fname3 << " not found.  Aborting. " )  );
        //DBG_Assert(false);
      } else {
        DBG_( Dev, ( << " saved checkpoint states " << fname1 << ", " << fname2 << " and " << fname3 << " not found.  Resetting to empty SGP. " )  );
      }
    } else {
      DBG_( Dev, ( << " Loading SGP state from " << fname )  );
      ifs >> std::skipws;

      if ( ! loadState( ifs ) ) {
        DBG_ ( Dev, ( << "Error loading checkpoint state from file: " << fname <<
                      ".  Make sure your checkpoints match your current SGP configuration." ) );
        DBG_Assert ( false );
      }
      ifs.close();
    }
  }

private:
  std::string theName;
  std::string theAltLoadName;
  int32_t theNodeId;
  uint32_t theNumAccesses;
  uint64_t theCounter;

  // for spatial group usage
  std::vector<SpatialGroupUsage> theGroupUsages;
  std::vector<Stat::StatInstanceCounter<int64_t> * > theUsageStats;

  // for spatial group repetitiveness
  SpatialGroupRepet theGroupRepets;
  SpatialGroupAssocRepet theGroupAssocRepets;
  GroupRepetHistory theRepetHistory;
  GroupRepetAssocHistory theRepetAssocHistory;

  typedef flexus_boost_seq_map<SpatialPattern, int> RepetPatternTable;
  typedef RepetPatternTable::iterator RepetPatternIter;
  RepetPatternTable theRepetPatterns;

  // for local buffer prefetching
  std::set<MemoryAddress> theFetchedBlocks;
  struct OrderedBlockEntry {
    uint64_t cycles;
    bool dense;
    OrderedBlockEntry(uint64_t curr, bool d)
      : cycles(curr)
      , dense(d)
    {}
  };
  typedef flexus_boost_seq_map<MemoryAddress, OrderedBlockEntry> OrderedBlocksMap;
  typedef OrderedBlocksMap::iterator OrderedBlocksIter;
  OrderedBlocksMap theOrderedBlocks;

  // for group ordering
  GroupOrderingBuild theOrderingBuild;
  GroupOrderingHistory theOrderingHistory;

  // for streaming
  CurrentStreamTable theCurrentStreams;
  DoneStreamTable theDoneStreams;

  // for group time tracking
  SpatialGroupTime theGroupTimes;

  // for active groups tracking
  SpatialGroupActive theActiveGroups;
  typedef std::map<uint32_t, int> ActiveGenTime; // this CANNOT be an unordered_map!
  typedef ActiveGenTime::iterator ActiveGenIter;
  ActiveGenTime theActiveGenBegins;
  ActiveGenTime theActiveGenEnds;

  int32_t theCurrPrefetchGen;
  int64_t theCurrPrefetchGenerationUUID;
  std::deque<MemoryAddress> thePrefetches;
  boost::intrusive_ptr<PrefetchCommand> theCurrPrefetchCommand;
  std::deque< boost::intrusive_ptr<PrefetchCommand> > thePrefetchCommandList;

  std::ofstream theRegionFile;

  uint32_t theBlockSize;
  uint32_t theSgpBlocks;
  uint32_t theRepetType;
  bool theRepetFills;
  bool theSparseOpt;
  uint32_t thePhtSize;
  uint32_t thePhtAssoc;
  uint32_t thePcBits;
  uint32_t theCptType;
  uint32_t theCptSize;
  uint32_t theCptAssoc;
  int64_t theCptFilterSize;
  bool theCptSparse;
  bool theFetchDist;
  int64_t theStreamWindow;
  bool theStreamDense;
  bool theSendStreams;
  uint32_t theBufSize;
  uint32_t theStreamDescs;
  bool theDelayedCommits;

  int64_t theCptSparseCount;
  bool theCurrDense;
  bool theStitchReady;

  uint32_t theBlockOffsetMask;
  uint32_t theBlockBits;
  uint32_t theSgpBits;   // bits necessary to encode block offset within a SG
  uint32_t theWordBits;  // bits necessary to encode word offset within a SG
  uint32_t theByteBits;  // bits necessary to encode byte offset within a SG
  uint32_t theWordInBlockBits;  // bits necessary to encode word offset within a block
  MemoryAddress thePCmask;
  uint32_t theMinSgpBits;
  uint32_t theMaxSgpBits;

  bool theEnableUsage;
  bool theEnableRepet;
  bool theEnableBufFetch;
  bool theEnableTimeRepet;
  bool theEnablePrefetch;
  bool theEnableActive;
  bool theEnableOrdering;
  bool theEnableStreaming;
  bool theEnableStitch;

  Stat::StatInstanceCounter<int64_t> statUsageAccess;
  Stat::StatInstanceCounter<int64_t> statUsageFill;
  Stat::StatInstanceCounter<int64_t> statUsageOffChip;

  Stat::StatCounter statRepetYes;
  Stat::StatCounter statRepetCountYes;
  Stat::StatCounter statRepetNo;
  Stat::StatCounter statRepetCountNo;

  Stat::StatCounter statRepetHit;
  Stat::StatCounter statRepetTraining;
  Stat::StatCounter statRepetMispred;

  Stat::StatCounter statRepetMiss;
  Stat::StatCounter statRepetFillsYes;
  Stat::StatCounter statRepetFillsNo;
  Stat::StatCounter statRepetFillHit;
  Stat::StatCounter statRepetFillTraining;
  Stat::StatCounter statRepetFillMispred;
  Stat::StatCounter statRepetCovGen;

  Stat::StatCounter statRepetOffChipYes;
  Stat::StatCounter statRepetOffChipNo;
  Stat::StatCounter statRepetOffChipHit;
  Stat::StatCounter statRepetOffChipTraining;
  Stat::StatCounter statRepetOffChipMispred;

  Stat::StatCounter statRepetAccessFill;
  Stat::StatCounter statRepetOnlyAccess;
  Stat::StatCounter statRepetOnlyFill;

  Stat::StatInstanceCounter<int64_t> statRepetUsageAccess;
  Stat::StatInstanceCounter<int64_t> statRepetUsageFill;
  Stat::StatInstanceCounter<int64_t> statRepetUsageOffChip;
  Stat::StatInstanceCounter<int64_t> statRepetUsageCov;

  Stat::StatCounter statRepetSparseNew;
  Stat::StatCounter statRepetSparseRemove;

  Stat::StatCounter statRepetEndGenNoIndex;
  Stat::StatCounter statRepetEvictCptNoIndex;
  Stat::StatCounter statRepetLateIndex;
  Stat::StatCounter statRepetNextGenNoIndex;
  Stat::StatCounter statRepetChangeIndex;
  Stat::StatCounter statRepetTriggerIndex;
  Stat::StatInstanceCounter<int64_t> statRepetEndGenNoIndexAccesses;
  Stat::StatInstanceCounter<int64_t> statRepetEndGenNoIndexMisses;
  Stat::StatInstanceCounter<int64_t> statRepetEvictCptNoIndexAccesses;
  Stat::StatInstanceCounter<int64_t> statRepetEvictCptNoIndexMisses;
  Stat::StatInstanceCounter<int64_t> statRepetLateIndexAccesses;
  Stat::StatInstanceCounter<int64_t> statRepetLateIndexMisses;
  Stat::StatInstanceCounter<int64_t> statRepetNextGenNoIndexAccesses;
  Stat::StatInstanceCounter<int64_t> statRepetNextGenNoIndexMisses;

  Stat::StatInstanceCounter<int64_t> statRepetPattern;
  Stat::StatInstanceCounter<int64_t> statRepetPatternDensity;
  Stat::StatInstanceCounter<int64_t> statRepetActualFillHits;
  Stat::StatCounter statRepetActualHitGens;
  Stat::StatInstanceCounter<int64_t> statParallelGroups;
  Stat::StatInstanceCounter<int64_t> statOrderDist;

  Stat::StatCounter statOrderBaseHitsDense;
  Stat::StatCounter statOrderBaseHitsSparse;
  Stat::StatInstanceCounter<int64_t> statOrderBaseLenDense;
  Stat::StatInstanceCounter<int64_t> statOrderBaseLenSparse;
  Stat::StatInstanceCounter<int64_t> statOrderDeltasDense;
  Stat::StatInstanceCounter<int64_t> statOrderDeltasSparse;
  Stat::StatInstanceCounter<int64_t> statOrderRightJumpsDense;
  Stat::StatInstanceCounter<int64_t> statOrderRightJumpsSparse;
  Stat::StatInstanceCounter<int64_t> statOrderLeftJumpsDense;
  Stat::StatInstanceCounter<int64_t> statOrderLeftJumpsSparse;
  Stat::StatInstanceCounter<int64_t> statOrderSameDirJumpsDense;
  Stat::StatInstanceCounter<int64_t> statOrderSameDirJumpsSparse;
  Stat::StatInstanceCounter<int64_t> statOrderDiffDirJumpsDense;
  Stat::StatInstanceCounter<int64_t> statOrderDiffDirJumpsSparse;
  Stat::StatCounter statOrderDenseSeqOrdYesYes;
  Stat::StatCounter statOrderDenseSeqOrdYesNo;
  Stat::StatCounter statOrderDenseSeqOrdNoYes;
  Stat::StatCounter statOrderDenseSeqOrdNoNo;
  Stat::StatCounter statOrderSparseSeqOrdYesYes;
  Stat::StatCounter statOrderSparseSeqOrdYesNo;
  Stat::StatCounter statOrderSparseSeqOrdNoYes;
  Stat::StatCounter statOrderSparseSeqOrdNoNo;

  Stat::StatCounter statBufFetchMissRead;
  Stat::StatCounter statBufFetchMissWrite;
  Stat::StatCounter statBufFetchPrefetch;
  Stat::StatCounter statBufFetchDupFetch;
  Stat::StatCounter statBufFetchDupStitch;
  Stat::StatCounter statBufFetchGoodRead;
  Stat::StatCounter statBufFetchGoodWrite;
  Stat::StatCounter statBufFetchGoodReadOS;
  Stat::StatCounter statBufFetchGoodWriteOS;
  Stat::StatCounter statBufFetchAccessRead;
  Stat::StatCounter statBufFetchAccessWrite;
  Stat::StatCounter statBufFetchInval;
  Stat::StatCounter statBufFetchDiscard;
  Stat::StatCounter statBufFetchTriggers;
  Stat::StatCounter statBufFetchGens;
  Stat::StatInstanceCounter<int64_t> statBufFetchGenSize;
  Stat::StatLog2Histogram statBufFetchDist;
  Stat::StatLog2Histogram statBufFetchDenseDist;
  Stat::StatLog2Histogram statBufFetchSparseDist;

  Stat::StatCounter statStreamNewStream;
  Stat::StatCounter statStreamShortStream;
  Stat::StatCounter statStreamReplaceStream;
  Stat::StatCounter statStreamEndStream;
  Stat::StatCounter statStreamOrphanHit;
  Stat::StatInstanceCounter<int64_t> statStreamLenDense;
  Stat::StatInstanceCounter<int64_t> statStreamLenSparse;
  Stat::StatInstanceCounter<int64_t> statStreamLenPossibleDense;
  Stat::StatInstanceCounter<int64_t> statStreamLenPossibleSparse;

  Stat::StatLog2Histogram statTimeInterAccess;
  Stat::StatLog2Histogram statTimeInterDeadAccess;
  Stat::StatLog2Histogram statTimeInterEvict;
  Stat::StatLog2Histogram statTimeInterLiveEvict;
  Stat::StatLog2Histogram statTimeGroupLive;
  Stat::StatLog2Histogram statTimeGroupDead;
  Stat::StatLog2Histogram statTimeInterBegin;
  Stat::StatLog2Histogram statTimeInterEnd;

  Stat::StatCounter statPrefetchYes;
  Stat::StatCounter statPrefetchNo;
  Stat::StatCounter statPrefetchCount;

  Stat::StatCounter statFills;
  Stat::StatCounter statPrefetches;
  Stat::StatCounter statCorrect;
  Stat::StatCounter statPartial;
  Stat::StatCounter statLate;
  Stat::StatCounter statDup;
  Stat::StatCounter statMispredictEvicts;
  Stat::StatCounter statMispredictInvals;
  Stat::StatCounter statEvictions;
  Stat::StatCounter statInvalidations;
  Stat::StatCounter statInserts;

public:
  void access(MemoryAddress addr, MemoryAddress pc, bool prefetch, bool write, bool miss, bool priv) {
    DBG_(Iface, ( << theName << (prefetch ? " prefetch" : " access") << " for addr " << std::hex << addr
                  << " (" << (write ? "write" : "read") << "/" << (miss ? "miss" : "hit")
                  << "/" << (priv ? "OS" : "user") << ")" ) );

    BlockAddress block = makeBlockAddress(addr);

    // (prefetch && miss) indicates a prefetch request that missed in the cache
    // (i.e., the prefetch actually went out)
    // (prefetch && !miss) indicates use of a prefetched block
    if (!(prefetch && miss)) {
      if (theEnableUsage) doGroupUsage(addr, false);
      if (theEnableRepet) {
        RepetReturnType repet = doGroupRepet(addr, pc, false, miss || prefetch);
        if (miss || prefetch) {
          if (theEnableBufFetch) doBufFetchMiss(block, write, priv);
        }
        if (theEnableStreaming) doStreamAccess(block, write, miss || prefetch, priv, repet);
        if (!theEnableStitch) {
          if (repet == kRepetRetNewGen) {
            if (!theDelayedCommits) {
              if (theEnableBufFetch) doBufFetchGen(addr, pc, 0);
            } else {
              // this enables "and first" with "all misses" in timing
              if (!miss) {
                if (theEnableBufFetch) doRepetCommit(addr, pc, 1);
              }
            }
          } else {
            // this enables "all misses" in the trace world
            if (!theDelayedCommits) {
              if (miss) {
                if (theEnableBufFetch) doBufFetchGen(addr, pc, 0);
              }
            }
          }
        } else {
          if (theStitchReady) {
            doBufFetchGen(addr, pc, 0);
          }
        }
      }
    }

    theNumAccesses++;
  }

  void fill(MemoryAddress block, bool offChip) {
    DBG_(Iface, ( << theName << " fill for addr " << std::hex << block ) );

    DBG_Assert(block == makeBlockAddress(block));
    if (theEnableRepet) doRepetFill(block, offChip);
  }

  void insert(MemoryAddress block) {
    DBG_(Iface, ( << theName << " insert of addr " << std::hex << block ) );
    DBG_Assert(block == makeBlockAddress(block));
  }

  void evict(MemoryAddress block) {
    DBG_(Iface, ( << theName << " evict of addr " << std::hex << block ) );

    DBG_Assert(block == makeBlockAddress(block));
    if (theEnableUsage) doGroupUsage(block, true);
    if (theEnableRepet) doGroupRepet(block, 0, true, false);
  }

  void invalidate(MemoryAddress block) {
    DBG_(Iface, ( << theName << " inval of addr " << std::hex << block ) );

    DBG_Assert(block == makeBlockAddress(block));
    if (theEnableUsage) doGroupUsage(block, true);
    if (theEnableRepet) doGroupRepet(block, 0, true, false);
    if (theEnableBufFetch) doBufFetchInval(block);
  }

  void commit(MemoryAddress addr, MemoryAddress pc, uint64_t ltime) {
    DBG_(Iface, ( << theName << " commit of addr " << std::hex << addr ) );
    if (!theDelayedCommits) {
      return;
    }

    if (theEnableRepet) {
      RepetReturnType repet;
      int64_t seq_no;
      std::tie( repet, seq_no ) = doRepetCommit(addr, pc, ltime);
      if (repet == kRepetRetNewGen) {
        if (!theEnableStitch) {
          if (theEnableBufFetch) doBufFetchGen(addr, pc, seq_no);
        }
      }
    }
  }

  void store(MemoryAddress addr, MemoryAddress pc, bool miss, bool priv, uint64_t ltime) {
    DBG_(Iface, ( << theName << " store of addr " << std::hex << addr ) );
    if (!theDelayedCommits) {
      return;
    }

    access(addr, pc, false, true, miss, priv);
    if (theEnableRepet) {
      RepetReturnType repet;
      int64_t seq_no;
      std::tie( repet, seq_no ) = doRepetCommit(addr, pc, ltime);
      if (repet == kRepetRetNewGen) {
        if (!theEnableStitch) {
          if (theEnableBufFetch) doBufFetchGen(addr, pc, seq_no);
        }
      }
    }
  }

  bool prefetchReady() {
    if (theSendStreams) {
      return( !thePrefetchCommandList.empty() );
    } else {
      return( !thePrefetches.empty() );
    }
  }

  MemoryAddress getPrefetch() {
    DBG_Assert(!theSendStreams);
    MemoryAddress addr = thePrefetches.front();
    thePrefetches.pop_front();
    return addr;
  }

  boost::intrusive_ptr<PrefetchCommand> getPrefetchCommand() {
    DBG_Assert(theSendStreams);
    boost::intrusive_ptr<PrefetchCommand> cmd = thePrefetchCommandList.front();
    thePrefetchCommandList.pop_front();
    return cmd;
  }

private:
  void doGroupUsage(MemoryAddress addr, bool evict) {
    uint32_t bits;
    for (bits = theMinSgpBits; bits <= theMaxSgpBits; bits++) {
      GroupAddress group = makeGroupAddress(addr, bits);
      GroupUsageIter iter = theGroupUsages[bits].find(group);
      if (iter != theGroupUsages[bits].end()) {
        // existing spatial group
        if (evict) {
          SpatialPattern test( iter->second & makeGroupPattern(addr, bits) );
          if (test.any()) {
            // only finalize (i.e. end of a generation) if this block has been
            // accessed in this generation
            finalizeGroupUsage(iter->second, bits);
            theGroupUsages[bits].erase(iter);
          }
        } else {
          iter->second |= makeGroupPattern(addr, bits);
        }
      } else {
        // create a new spatial group if necessary
        if (!evict) {
          theGroupUsages[bits].insert( std::make_pair(group, makeGroupPattern(addr, bits)) );
        }
      }
    }
  }

  void finalizeGroupUsage(SpatialPattern pattern, int32_t groupBits) {
    *theUsageStats[groupBits] << std::make_pair(countBits(pattern), 1);
  }

  RepetReturnType doGroupRepet(MemoryAddress addr, MemoryAddress pc, bool evict, bool miss) {
    RepetReturnType ret = kRepetRetNone;
    theCounter++;
    theStitchReady = false;
    if (theCptType == 3) {
      return doGroupRepet2(addr, pc, evict, miss);
    }
    GroupAddress group = makeGroupAddress(addr, theSgpBits);

    //theRegionFile << std::hex << group << " " << std::dec << makeGroupOffset(addr,theSgpBits) << " " << getCycleCount() << std::endl;

    if (miss) {
      statRepetMiss++;
    }

    if (!evict) {
      GroupRepetIter iter = theGroupRepets.find(group);
      if (iter != theGroupRepets.end()) {
        if (iter->second.finalized) {
          statRepetNextGenNoIndex++;
          statRepetNextGenNoIndexAccesses << std::make_pair(countBits(iter->second.pattern), 1);
          statRepetNextGenNoIndexMisses << std::make_pair(countBits(iter->second.fills), 1);
          if (theEnableOrdering) {
            OrderingBuildIter build = theOrderingBuild.find(group);
            DBG_Assert(build != theOrderingBuild.end());
            theOrderingBuild.erase(build);
          }
          //if((--theRepetPatterns[iter->second.pattern]) == 0) {
          //  theRepetPatterns.erase(iter->second.pattern);
          //}
          if (countBits(iter->second.pattern) <= 1) {
            theCptSparseCount--;
          }
          if (theEnableBufFetch) doBufFetchEndGen(group);
          theGroupRepets.erase(iter);
        }
      }
    }

    bool removedPht = false;
    bool beginend = false;
    int32_t hits = -1;
    GroupRepetIter iter = theGroupRepets.find(group);
    if (iter != theGroupRepets.end()) {
      // existing spatial group
      if (evict) {
        SpatialPattern accessedThisGen( iter->second.pattern & makeGroupPattern(addr, theSgpBits) );
        SpatialPattern filledThisGen( iter->second.fills & makeGroupPattern(addr, theSgpBits) );
        // only finalize (i.e. end of a generation) if this block has been
        // accessed/filled in this generation
        if ( (!theRepetFills && accessedThisGen.any()) || (theRepetFills && filledThisGen.any()) ) {
          DBG_(Trace, ( << theName << " group_" << std::hex << group << " end gen" ) );
          if (!theDelayedCommits || iter->second.hasIndex()) {
            DBG_Assert(!iter->second.finalized);
            beginend = true;
            hits = finalizeGroupRepet(iter->second.index,
                                      iter->second.pattern,
                                      iter->second.fills,
                                      iter->second.offChipFills,
                                      removedPht);
            if (theEnableOrdering) {
              OrderingBuildIter build = theOrderingBuild.find(group);
              DBG_Assert(build != theOrderingBuild.end());
              finalizeGroupOrdering(iter->second.index, build->second.theOrder, hits, removedPht);
              theOrderingBuild.erase(build);
            }
            //if((--theRepetPatterns[iter->second.pattern]) == 0) {
            //  theRepetPatterns.erase(iter->second.pattern);
            //}
            if (countBits(iter->second.pattern) <= 1) {
              theCptSparseCount--;
            }
            if (theEnableBufFetch) doBufFetchEndGen(group);
            theGroupRepets.erase(iter);
          } else {
            iter->second.finalized = true;
          }
        }
      } else {
        theStitchReady = true;
        //if((--theRepetPatterns[iter->second.pattern]) == 0) {
        //  theRepetPatterns.erase(iter->second.pattern);
        //}
        bool repeatAccess = iter->second.pattern.test( makeGroupOffset(addr, theSgpBits) );
        if (!repeatAccess) {
          ret = kRepetRetFirstAccess;
          if (!iter->second.prefetched.test( makeGroupOffset(addr, theSgpBits) )) {
            theStitchReady = true;
          }
          if (countBits(iter->second.pattern) <= 1) {
            theCptSparseCount--;
            if (theCptSparse) {
              // transition from the filter to the non-sparse table
              if ((theGroupRepets.size() - theCptSparseCount) >= theCptSize) {
                // find the oldest non-sparse
                GroupRepetSeq seq = theGroupRepets.beginSeq();
                while (countBits(seq->second.pattern) <= 1) {
                  ++seq;
                  DBG_Assert(seq != theGroupRepets.endSeq());
                }
                if (!theDelayedCommits || seq->second.hasIndex()) {
                  hits = finalizeGroupRepet(seq->second.index,
                                            seq->second.pattern,
                                            seq->second.fills,
                                            seq->second.offChipFills,
                                            removedPht);
                } else {
                  statRepetEvictCptNoIndex++;
                  statRepetEvictCptNoIndexAccesses << std::make_pair(countBits(seq->second.pattern), 1);
                  statRepetEvictCptNoIndexMisses << std::make_pair(countBits(seq->second.fills), 1);
                }
                if (theEnableOrdering) {
                  OrderingBuildIter build = theOrderingBuild.find(seq->first);
                  DBG_Assert(build != theOrderingBuild.end());
                  if (!theDelayedCommits || seq->second.hasIndex()) {
                    finalizeGroupOrdering(seq->second.index, build->second.theOrder, hits, removedPht);
                  }
                  theOrderingBuild.erase(build);
                }
                if (theEnableBufFetch) doBufFetchEndGen(seq->first);
                theGroupRepets.eraseSeq(seq);
              }
            }
          }
          DBG_(Trace, ( << theName << "  group_" << std::hex << group << " set bit " << makeGroupOffset(addr, theSgpBits) << (miss ? " miss" : " hit") ) );
        }
        iter->second.pattern |= makeGroupPattern(addr, theSgpBits);
        if (miss) {
          iter->second.fills |= makeGroupPattern(addr, theSgpBits);
        }
        if (theEnableOrdering) {
          bool append = false;
          if (!repeatAccess) {
            if (!theRepetFills || miss) {
              append = true;
            }
          }
          if (append) {
            OrderingBuildIter build = theOrderingBuild.find(group);
            DBG_Assert(build != theOrderingBuild.end());
            uint8_t offset = makeGroupOffset(addr, theSgpBits);
            build->second.nextBlock(offset);
          }
        }
        //theRepetPatterns[iter->second.pattern]++;
        if (theCptType == 1) {
          if (!theRepetFills || miss) {
            theGroupRepets.move_back(iter);
          }
        }
      }
    } else {
      // create a new spatial group if necessary
      if (!evict) {
        if (!theRepetFills || miss) {
          ret = kRepetRetNewGen;
          theStitchReady = true;
          beginend = true;
          SpatialPattern pattern( makeGroupPattern(addr, theSgpBits) );
          SpatialPattern fills( pattern );
          if (!miss) {
            fills.reset();
          }
          RepetIndex index = makeRepetIndex(addr, pc);
          if (index == 0) {
            DBG_( Dev,  ( << theName << " 0 index addr: " << addr << " pc: " << pc ) );
          }
          DBG_(Trace, ( << theName << " group_" << std::hex << group << " new gen" ) );
          DBG_(Trace, ( << theName << "  group_" << std::hex << group << " set bit " << makeGroupOffset(addr, theSgpBits)  << (miss ? " miss" : " hit") ) );

          if (theEnableOrdering) {
            DBG_Assert(theOrderingBuild.find(group) == theOrderingBuild.end());
            if (!theRepetFills || miss) {
              uint8_t offset = makeGroupOffset(addr, theSgpBits);
              theOrderingBuild.insert( std::make_pair(group, GroupOrderingEntry(offset)) );
            } else {
              theOrderingBuild.insert( std::make_pair(group, GroupOrderingEntry()) );
            }
          }

          if ( (theCptType == 1) && (theCptSize > 0) ) {
            if ( theCptSparse ) {
              if ( (theCptFilterSize > 0) && (theCptSparseCount >= theCptFilterSize) ) {
                // find the oldest sparse
                GroupRepetSeq seq = theGroupRepets.beginSeq();
                while (countBits(seq->second.pattern) > 1) {
                  ++seq;
                  DBG_Assert(seq != theGroupRepets.endSeq());
                }
                if (!theDelayedCommits || seq->second.hasIndex()) {
                  hits = finalizeGroupRepet(seq->second.index,
                                            seq->second.pattern,
                                            seq->second.fills,
                                            seq->second.offChipFills,
                                            removedPht);
                } else {
                  statRepetEvictCptNoIndex++;
                  statRepetEvictCptNoIndexAccesses << std::make_pair(countBits(seq->second.pattern), 1);
                  statRepetEvictCptNoIndexMisses << std::make_pair(countBits(seq->second.fills), 1);
                }
                if (theEnableOrdering) {
                  OrderingBuildIter build = theOrderingBuild.find(seq->first);
                  DBG_Assert(build != theOrderingBuild.end());
                  if (!theDelayedCommits || seq->second.hasIndex()) {
                    finalizeGroupOrdering(seq->second.index, build->second.theOrder, hits, removedPht);
                  }
                  theOrderingBuild.erase(build);
                }
                theCptSparseCount--;
                if (theEnableBufFetch) doBufFetchEndGen(seq->first);
                theGroupRepets.eraseSeq(seq);
              }
            } else {
              if (theGroupRepets.size() >= theCptSize) {
                if (!theDelayedCommits || theGroupRepets.front().hasIndex()) {
                  hits = finalizeGroupRepet(theGroupRepets.front().index,
                                            theGroupRepets.front().pattern,
                                            theGroupRepets.front().fills,
                                            theGroupRepets.front().offChipFills,
                                            removedPht);
                } else {
                  statRepetEvictCptNoIndex++;
                  statRepetEvictCptNoIndexAccesses << std::make_pair(countBits(theGroupRepets.front().pattern), 1);
                  statRepetEvictCptNoIndexMisses << std::make_pair(countBits(theGroupRepets.front().fills), 1);
                }
                if (theEnableOrdering) {
                  OrderingBuildIter build = theOrderingBuild.find(theGroupRepets.front_key());
                  DBG_Assert(build != theOrderingBuild.end());
                  if (!theDelayedCommits || theGroupRepets.front().hasIndex()) {
                    finalizeGroupOrdering(theGroupRepets.front().index, build->second.theOrder, hits, removedPht);
                  }
                  theOrderingBuild.erase(build);
                }
                if (countBits(theGroupRepets.front().pattern) <= 1) {
                  theCptSparseCount--;
                }
                if (theEnableBufFetch) doBufFetchEndGen(theGroupRepets.front_key());
                theGroupRepets.pop_front();
              }
            }
          }
          if (theDelayedCommits) {
            theGroupRepets.insert( std::make_pair(group, SpatialGroupEntry(pattern, fills)) );
          } else {
            theGroupRepets.insert( std::make_pair(group, SpatialGroupEntry(pattern, index, fills)) );
          }
          //theRepetPatterns[pattern]++;
          theCptSparseCount++;
        }
      }
    }

    if (!evict) {
      //statRepetPattern << std::make_pair( theGroupRepets.size(),1 );
      //statRepetPattern << std::make_pair( theRepetPatterns.size(),1 );
    }
    if (theEnableTimeRepet) doGroupTime(addr, pc, evict, beginend);

    return ret;
  }

  std::pair<RepetReturnType, int64_t> doRepetCommit(MemoryAddress addr, MemoryAddress pc, uint64_t aLogicalTime) {
    RepetReturnType ret = kRepetRetNone;
    int64_t group_sequence_no = 0;
    bool removedPht = false;

    GroupAddress group = makeGroupAddress(addr, theSgpBits);
    GroupRepetIter iter = theGroupRepets.find(group);
    if (iter != theGroupRepets.end()) {
      if (! iter->second.hasIndex() ||  iter->second.last_poke != makeRepetIndex(addr, pc) ) {
        // index has not been set - first check that the block corresponding to this
        // instruction was actually accessed this generation
        SpatialPattern accessedThisGen( iter->second.pattern & makeGroupPattern(addr, theSgpBits) );
        if (accessedThisGen.any()) {
          ++statRepetTriggerIndex;

          ret = kRepetRetNewGen;
          group_sequence_no = iter->second.unique_id;
          RepetIndex index = makeRepetIndex(addr, pc);
          iter->second.last_poke = index;
          if (iter->second.finalized) {
            DBG_(Trace, ( << theName << " group_" << std::hex << group << " finalize index" ) );
            ret = kRepetRetNewGen;
            // this generation has already ended
            statRepetEndGenNoIndex++;
            statRepetEndGenNoIndexAccesses << std::make_pair(countBits(iter->second.pattern), 1);
            statRepetEndGenNoIndexMisses << std::make_pair(countBits(iter->second.fills), 1);
            int32_t hits = finalizeGroupRepet(index,
                                              iter->second.pattern,
                                              iter->second.fills,
                                              iter->second.offChipFills,
                                              removedPht);
            if (theEnableOrdering) {
              OrderingBuildIter build = theOrderingBuild.find(group);
              DBG_Assert(build != theOrderingBuild.end());
              finalizeGroupOrdering(index, build->second.theOrder, hits, removedPht);
              theOrderingBuild.erase(build);
            }
            //if((--theRepetPatterns[iter->second.pattern]) == 0) {
            //  theRepetPatterns.erase(iter->second.pattern);
            //}
            if (countBits(iter->second.pattern) <= 1) {
              theCptSparseCount--;
            }
            if (theEnableBufFetch) doBufFetchEndGen(group);
            theGroupRepets.erase(iter);
          } else {
            if (! iter->second.hasIndex() || aLogicalTime < iter->second.logical_time ) {
              DBG_(Trace, ( << theName << " group_" << std::hex << group << " choose index" ) );
              ret = kRepetRetNewGen;
              if (! iter->second.hasIndex() ) {
                ++statRepetChangeIndex;
              }

              // associate this index with the spatial generation
              iter->second.index = index;
              iter->second.logical_time = aLogicalTime;
              statRepetLateIndex++;
              statRepetLateIndexAccesses << std::make_pair(countBits(iter->second.pattern), 1);
              statRepetLateIndexMisses << std::make_pair(countBits(iter->second.fills), 1);
            }
          }
        }
      }
    }

    return std::make_pair(ret, group_sequence_no);
  }

  // This implements Chi-style spatial group generations
  RepetReturnType doGroupRepet2(MemoryAddress addr, MemoryAddress pc, bool evict, bool miss) {
    DBG_Assert(!theDelayedCommits);
    DBG_Assert(!theEnableStitch);
    if (evict) {
      return kRepetRetNone;
    }

    bool beginend = false;
    GroupIndex group = makeGroupIndex(addr, theSgpBits);
    GroupRepetIter iter = theGroupAssocRepets.find(group);
    if (iter != theGroupAssocRepets.end()) {
      // existing spatial group
      iter->second.pattern |= makeGroupPattern(addr, theSgpBits);
      if (miss) {
        iter->second.fills |= makeGroupPattern(addr, theSgpBits);
      }
      if (!theRepetFills || miss) {
        theGroupAssocRepets.move_back(iter);
      }
    } else {
      if (!theRepetFills || miss) {
        // create a new spatial group if necessary
        beginend = true;
        SpatialPattern pattern( makeGroupPattern(addr, theSgpBits) );
        SpatialPattern fills( pattern );
        if (!miss) {
          fills.reset();
        }
        RepetIndex index = makeRepetIndex(addr, pc);

        if ( (theCptAssoc > 0) && (theGroupAssocRepets.size() >= theCptAssoc) ) {
          bool removedPht = false;
          finalizeGroupRepet(theGroupAssocRepets.front().index,
                             theGroupAssocRepets.front().pattern,
                             theGroupAssocRepets.front().fills,
                             theGroupAssocRepets.front().offChipFills,
                             removedPht);
          if (theEnableBufFetch) doBufFetchEndGen(theGroupAssocRepets.front_key());
          theGroupAssocRepets.pop_front();
        }
        theGroupAssocRepets.insert( std::make_pair(group, SpatialGroupEntry(pattern, index, fills)) );
      }
    }

    //statRepetPattern << std::make_pair( theGroupAssocRepets.size(),1 ); //doesn't make sense here
    //statRepetPattern << std::make_pair( theRepetPatterns.size(),1 );
    if (theEnableTimeRepet) doGroupTime(addr, pc, evict, beginend);

    if (beginend) {
      return kRepetRetNewGen;
    }
    return kRepetRetNone;
  }

  // This is supposed to limit the number of currently-building patterns.
  // Not complete/tested.
  /*
  void doGroupRepet3(MemoryAddress addr, MemoryAddress pc, bool evict) {
    bool beginend = false;
    GroupAddress group = makeGroupAddress(addr, theSgpBits);
    GroupRepetIter iter = theGroupRepets.find(group);
    if(iter != theGroupRepets.end()) {
      // existing spatial group
      if(evict) {
        RepetPatternIter iter2 = theRepetPatterns.find(iter->second.pattern);
        if(iter2 != theRepetPatterns.end()) {
          SpatialPattern test( iter->second.pattern & makeGroupPattern(addr, theSgpBits) );
          if(test.any()) {
            beginend = true;
            // only finalize (i.e. end of a generation) if this block has been
            // accessed in this generation
            finalizeGroupRepet(iter->second.index, iter->second.pattern, iter->second.fills);
            if(--(iter2->second) == 0) {
              theRepetPatterns.erase(iter2);
            }
            if(theEnableBufFetch) doBufFetchEndGen(group);
            theGroupRepets.erase(iter);
          }
        }
        else {
          // do something here ??? (because the pattern has been evicted, all
          // knowledge of the spatial may also be gone
        }
      } else {
        SpatialPattern next( iter->second.pattern | makeGroupPattern(addr, theSgpBits) );
        if(next != iter->second.pattern) {
          // first decrement the ref count of the old pattern
          RepetPatternIter iter2 = theRepetPatterns.find(iter->second.pattern);
          if(iter2 != theRepetPatterns.end()) {
            DBG_Assert(iter2->second > 0);
            if(--(iter2->second) == 0) {
              theRepetPatterns.erase(iter2);
            }
          }
          else {
            // do something here ??? (because the pattern has been evicted, all
            // knowledge of the spatial group may also be gone
          }
          // now increment (or insert) the new pattern
          iter2 = theRepetPatterns.find(next);
          if(iter2 != theRepetPatterns.end()) {
            (iter2->second)++;
            theRepetPatterns.move_back(iter2);
          }
          else {
            if( (theCptSize > 0) && (theRepetPatterns.size() >= theCptSize) ) {
              // do something here ???
              theRepetPatterns.pop_front();
            }
            theRepetPatterns.insert( std::make_pair(next,1) );
          }
          iter->second.pattern = next;
        }
      }
    }
    else {
      // create a new spatial group if necessary
      if(!evict) {
        beginend = true;
        if(theEnableBufFetch) doBufFetchGen(addr,pc);
        SpatialPattern pattern( makeGroupPattern(addr, theSgpBits) );
        RepetIndex index = makeRepetIndex(addr, pc);

        RepetPatternIter iter2 = theRepetPatterns.find(pattern);
        if(iter2 != theRepetPatterns.end()) {
          (iter2->second)++;
          theRepetPatterns.move_back(iter2);
        }
        else {
          if( (theCptSize > 0) && (theRepetPatterns.size() >= theCptSize) ) {
            // do something here ???
            theRepetPatterns.pop_front();
          }
          theRepetPatterns.insert( std::make_pair(pattern,1) );
        }
        theGroupRepets.insert( std::make_pair(group,SpatialGroupEntry(pattern,index)) );
      }
    }

    if(theEnableTimeRepet) doGroupTime(addr,pc,evict,beginend);
  }
  */

  void doRepetFill(MemoryAddress block, bool offChip) {
    if (!offChip) {
      return;
    }

    if (theCptType == 3) {
      GroupIndex group = makeGroupIndex(block, theSgpBits);
      GroupRepetIter iter = theGroupAssocRepets.find(group);
      if (iter != theGroupAssocRepets.end()) {
        //iter->second.fills |= makeGroupPattern(block, theSgpBits);
        //theGroupAssocRepets.move_back(iter);
        iter->second.offChipFills |= makeGroupPattern(block, theSgpBits);
      }
    } else {
      GroupAddress group = makeGroupAddress(block, theSgpBits);
      GroupRepetIter iter = theGroupRepets.find(group);
      if (iter != theGroupRepets.end()) {
        //iter->second.fills |= makeGroupPattern(block, theSgpBits);
        iter->second.offChipFills |= makeGroupPattern(block, theSgpBits);
      }
    }
  }

  int32_t finalizeGroupRepet(RepetIndex index, SpatialPattern current, SpatialPattern fills, SpatialPattern offChipFills, bool & removed) {
    if (index == 0) {
      DBG_( Crit, ( << "Warning: zero index.  This is suspicious, but may be legitimate " ) );
    }
    //DBG_Assert(index != 0);
    if (doRotate()) {
      GroupOffset first = makeOffsetFromConcat(index);
      //DBG_(Dev, ( << "current=" << std::hex << current << " first=" << first ) );
      //current = current << (32 - first);
      //fills = fills << (32 - first);
      current = doRotate(current, first);
      fills = doRotate(fills, first);
      offChipFills = doRotate(offChipFills, first);
      //DBG_(Dev, ( << "  current=" << std::hex << current ) );
    }
    index = makeIndexFromConcat(index);

    SpatialPattern toUpdate(current);
    if (theRepetFills) {
      toUpdate = fills;
    }

    SpatialHistory * hist = 0;
    RepetHistoryIter iter;
    if (thePhtAssoc > 0) {
      iter = theRepetAssocHistory.find(index);
      if (iter != theRepetAssocHistory.end()) {
        hist = &(iter->second);
      }
    } else {
      iter = theRepetHistory.find(index);
      if (iter != theRepetHistory.end()) {
        hist = &(iter->second);
      }
    }

    int32_t retHits = 0;

    statUsageAccess << std::make_pair( countBits(current), 1 );
    statUsageFill << std::make_pair( countBits(fills), 1 );
    statUsageOffChip << std::make_pair( countBits(offChipFills), 1 );

    if (hist) {
      statRepetYes++;
      SpatialPattern last( hist->extract() );

      // compare the current spatial pattern to the previous one
      SpatialPattern hits( last & current );
      SpatialPattern training( current & (~hits) );
      SpatialPattern mispred( last & (~hits) );

      statRepetCountYes += countBits(current);
      statRepetUsageAccess << std::make_pair( countBits(current), 1 );

      statRepetHit += countBits(hits);
      statRepetTraining += countBits(training);
      statRepetMispred += countBits(mispred);

      // compare the fill pattern to the previous spatial pattern
      SpatialPattern fillHits( last & fills );
      SpatialPattern fillTraining( fills & (~fillHits) );
      SpatialPattern fillMispred( last & (~fillHits) );

      statRepetFillsYes += countBits(fills);
      statRepetUsageFill << std::make_pair( countBits(fills), 1 );
      if (countBits(fillHits) > 1) {
        statRepetUsageCov << std::make_pair( countBits(fills), countBits(fillHits) - 1 );
        statRepetCovGen++;
      }

      statRepetFillHit += countBits(fillHits);
      statRepetFillTraining += countBits(fillTraining);
      statRepetFillMispred += countBits(fillMispred);

      if (theRepetType == 1) {
        // only for PC+BLoffset
        GroupOffset initOffset = extractGroupOffset(index, theSgpBits);
        int32_t actualFillHits = countBits(fillHits);
        if (fillHits.test(initOffset)) {
          actualFillHits--;
        }
        if (actualFillHits > 0) {
          statRepetActualFillHits << std::make_pair(actualFillHits, 1);
          statRepetActualHitGens++;
        }
      }

      // compare the off-chip fill pattern to the previous spatial pattern
      SpatialPattern offChipHits( last & offChipFills );
      SpatialPattern offChipTraining( offChipFills & (~offChipHits) );
      SpatialPattern offChipMispred( last & (~offChipHits) );

      statRepetOffChipYes += countBits(offChipFills);
      statRepetUsageOffChip << std::make_pair( countBits(offChipFills), 1 );

      statRepetOffChipHit += countBits(offChipHits);
      statRepetOffChipTraining += countBits(offChipTraining);
      statRepetOffChipMispred += countBits(offChipMispred);

      if (countBits(fillHits) > 1) {
        //uint64_t key = (index<<32) | current;
        //statRepetPattern << std::make_pair( index,countBits(fillHits)-1 );
      }
      retHits = countBits(fillHits);

      // compare the access pattern to the fill pattern
      SpatialPattern fillSame( current & fills );
      SpatialPattern fillOnlyAccess( current & (~fillSame) );
      SpatialPattern fillOnlyFill( fills & (~fillSame) );

      statRepetAccessFill += countBits(fillSame);
      statRepetOnlyAccess += countBits(fillOnlyAccess);
      statRepetOnlyFill += countBits(fillOnlyFill);

      hist->update(toUpdate, theCounter);
      if ( (!theSparseOpt) || (countBits(hist->extract()) > 1) ) {
        // move to LRU position
        if (thePhtAssoc > 0) {
          theRepetAssocHistory.move_back(iter);
        } else {
          theRepetHistory.move_back(iter);
        }
      } else {
        removed = true;
        if (thePhtAssoc > 0) {
          theRepetAssocHistory.erase(iter);
        } else {
          theRepetHistory.erase(iter);
        }
        statRepetSparseRemove++;
      }
    } else {
      // no prior history
      statRepetNo++;
      statRepetCountNo += countBits(current);
      statRepetFillsNo += countBits(fills);
      statRepetOffChipNo += countBits(offChipFills);
      if ( (!theSparseOpt) || (countBits(toUpdate) > 1) ) {
        if (thePhtAssoc > 0) {
          // set-associative, finite
          if (theRepetAssocHistory.size() >= thePhtAssoc) {
            if (theEnableOrdering) {
              OrderingHistoryIter order = theOrderingHistory.find(theRepetAssocHistory.front_key());
              DBG_Assert(order != theOrderingHistory.end());
              theOrderingHistory.erase(order);
            }
            theRepetAssocHistory.pop_front();
          }
          theRepetAssocHistory.insert( std::make_pair(index, SpatialHistory(toUpdate, theCounter)) );
        } else {
          // fully-associate
          if ( (thePhtSize > 0) && (theRepetHistory.size() >= thePhtSize) ) {
            // finite
            if (theEnableOrdering) {
              OrderingHistoryIter order = theOrderingHistory.find(theRepetHistory.front_key());
              DBG_Assert(order != theOrderingHistory.end());
              theOrderingHistory.erase(order);
            }
            theRepetHistory.pop_front();
          }
          theRepetHistory.insert( std::make_pair(index, SpatialHistory(toUpdate, theCounter)) );
        }
      } else {
        removed = true;
        statRepetSparseNew++;
      }
    }

    return retHits;
  }

  bool movesRight(OrderingMap & order) {
    bool isRight = true;
    OrderingSeqIter dirIter = order.beginSeq();
    if (dirIter != order.endSeq()) {
      int32_t blk1 = dirIter->first;
      dirIter++;
      if (dirIter != order.endSeq()) {
        int32_t blk2 = dirIter->first;
        dirIter++;
        if (dirIter != order.endSeq()) {
          int32_t blk3 = dirIter->first;
          // now perform the actual calculation
          if (blk2 > blk1) {
            if ( (blk3 > blk1) && (blk3 < blk2) ) {
              isRight = false;
            } else {
              isRight = true;
            }
          } else {
            if ( (blk3 > blk2) && (blk3 < blk1) ) {
              isRight = true;
            } else {
              isRight = false;
            }
          }
        }
      }
    }
    return isRight;
  }

  int32_t getDenseThreshold() {
    int32_t denseThreshold = -1;
    if (theSgpBlocks == 128) {
      denseThreshold = 16;
    } else if (theSgpBlocks == 64) {
      denseThreshold = 10;
    } else if (theSgpBlocks == 32) {
      denseThreshold = 6;
    } else if (theSgpBlocks == 16) {
      denseThreshold = 4;
    } else if (theSgpBlocks == 8) {
      denseThreshold = 3;
    } else if (theSgpBlocks == 4) {
      denseThreshold = 2;
    } else if (theSgpBlocks == 2) {
      denseThreshold = 1;
    } else {
      DBG_Assert(false, ( << "Group size of " << theSgpBlocks << " not supported for ordering." ) );
    }
    return denseThreshold;
  }

  void finalizeGroupOrdering(RepetIndex index, OrderingMap & orig, int32_t fillHits, bool removed) {
    OrderingMap build;
    if (doRotate()) {
      GroupOffset offset = makeOffsetFromConcat(index);
      OrderingSeqIter seq = orig.beginSeq();
      while (seq != orig.endSeq()) {
        if (seq->first >= offset) {
          build.push_back( std::make_pair(seq->first - offset, seq->second) );
        } else {
          build.push_back( std::make_pair(theSgpBlocks - (offset - seq->first), seq->second) );
        }
        ++seq;
      }
    } else {
      build = orig;
    }
    index = makeIndexFromConcat(index);

    OrderingHistoryIter histIter = theOrderingHistory.find(index);
    if (histIter == theOrderingHistory.end()) {
      if (!removed) {
        // new group ordering
        theOrderingHistory.insert( std::make_pair(index, build) );
      }
      return;
    }

    /*
    OrderingMap & history = histIter->second;
    int32_t denseThreshold = getDenseThreshold();

    // determine if the historical seq is moving to the left or right
    // - if the seq is empty, direction is irrelevant
    // - if the seq has one element, direction is irrelevant (no hits)
    // - if the seq has two elements, consider it moving to right
    // - if the seq has three or more elements, see below
    bool histIsRight = movesRight(history);

    // find the earliest block in the current sequence that occurs in the history
    OrderingSeqIter seqIter = build.beginSeq();
    OrderingIter blockIter = history.find(seqIter->first);
    while(blockIter == history.end()) {
      seqIter++;
      if(seqIter == build.endSeq()) {
        break;
      }
      blockIter = history.find(seqIter->first);
    }
    int64_t len = 0;
    if(blockIter != history.end()) {
      int32_t histSeq = blockIter->second;   // intentional conversion from uint8_t
      seqIter++;
      while(seqIter != build.endSeq()) {
        blockIter = history.find(seqIter->first);
        if(blockIter != history.end()) {
          int32_t dist = (int)blockIter->second - histSeq;
          statOrderDist << std::make_pair(dist,1);
          len++;
          histSeq = blockIter->second;
        }
        seqIter++;
      }
    }

    // now take care of some stats
    if(fillHits > denseThreshold) {
      statOrderBaseHitsDense += fillHits;
      statOrderBaseLenDense << std::make_pair(len,1);
    }
    else {
      statOrderBaseHitsSparse += fillHits;
      statOrderBaseLenSparse << std::make_pair(len,1);
    }

    // walk through the current sequence and determine deltas
    seqIter = build.beginSeq();
    if(seqIter != build.endSeq()) {
      int32_t currBlock = seqIter->first;
      while((++seqIter) != build.endSeq()) {
        if(fillHits > denseThreshold) {
          statOrderDeltasDense << std::make_pair((int)seqIter->first - currBlock, 1);
        } else {
          statOrderDeltasSparse << std::make_pair((int)seqIter->first - currBlock, 1);
        }
        currBlock = seqIter->first;
      }

      // now compare the current sequence against sorted historical seqs
      OrderingMap sortedRight;
      int32_t currSeq = 0;
      seqIter = history.beginSeq();
      currBlock = seqIter->first;
      unsigned ii;
      for(ii = 0; ii < theSgpBlocks; ii++) {
        if(currBlock == (int)theSgpBlocks) {
          currBlock = 0;
        }
        if(history.find(currBlock) != history.end()) {
          sortedRight.push_back( std::make_pair(currBlock,currSeq) );
          currSeq++;
        }
        currBlock++;
      }

      seqIter = build.beginSeq();
      blockIter = sortedRight.find(seqIter->first);
      while(blockIter == sortedRight.end()) {
        seqIter++;
        if(seqIter == build.endSeq()) {
          break;
        }
        blockIter = sortedRight.find(seqIter->first);
      }
      if(blockIter != sortedRight.end()) {
        OrderingIter block2Iter = history.find(seqIter->first);
        DBG_Assert(block2Iter != history.end());
        int32_t histSeq = blockIter->second;   // intentional conversion from uint8_t
        int32_t hist2Seq = block2Iter->second;   // intentional conversion from uint8_t
        seqIter++;
        while(seqIter != build.endSeq()) {
          int32_t dist, dist2;
          blockIter = sortedRight.find(seqIter->first);
          if(blockIter != sortedRight.end()) {
            dist = (int)blockIter->second - histSeq;
            if(fillHits > denseThreshold) {
              statOrderRightJumpsDense << std::make_pair(dist,1);
              if(histIsRight) {
                statOrderSameDirJumpsDense << std::make_pair(dist,1);
              } else {
                statOrderDiffDirJumpsDense << std::make_pair(dist,1);
              }
            }
            else {
              statOrderRightJumpsSparse << std::make_pair(dist,1);
              if(histIsRight) {
                statOrderSameDirJumpsSparse << std::make_pair(dist,1);
              } else {
                statOrderDiffDirJumpsSparse << std::make_pair(dist,1);
              }
            }
            histSeq = blockIter->second;

            block2Iter = history.find(seqIter->first);
            DBG_Assert(block2Iter != history.end());
            dist2 = (int)block2Iter->second - hist2Seq;
            hist2Seq = block2Iter->second;
            if(histIsRight) {
              if(fillHits > denseThreshold) {
                if( (abs(dist) <= 2) && (abs(dist2) <= 2) ) {
                  statOrderDenseSeqOrdYesYes++;
                } else if( (abs(dist) <= 2) ) {
                  statOrderDenseSeqOrdYesNo++;
                } else if( (abs(dist2) <= 2) ) {
                  statOrderDenseSeqOrdNoYes++;
                } else {
                  statOrderDenseSeqOrdNoNo++;
                }
              }
              else {
                if( (abs(dist) <= 2) && (abs(dist2) <= 2) ) {
                  statOrderSparseSeqOrdYesYes++;
                } else if( (abs(dist) <= 2) ) {
                  statOrderSparseSeqOrdYesNo++;
                } else if( (abs(dist2) <= 2) ) {
                  statOrderSparseSeqOrdNoYes++;
                } else {
                  statOrderSparseSeqOrdNoNo++;
                }
              }
            }
          }
          seqIter++;
        }
      }

      OrderingMap sortedLeft;
      currSeq = 0;
      seqIter = history.beginSeq();
      currBlock = seqIter->first;
      for(ii = 0; ii < theSgpBlocks; ii++) {
        if(currBlock < 0) {
          currBlock = (int)theSgpBlocks - 1;
        }
        if(history.find(currBlock) != history.end()) {
          sortedLeft.push_back( std::make_pair(currBlock,currSeq) );
          currSeq++;
        }
        currBlock--;
      }

      seqIter = build.beginSeq();
      blockIter = sortedLeft.find(seqIter->first);
      while(blockIter == sortedLeft.end()) {
        seqIter++;
        if(seqIter == build.endSeq()) {
          break;
        }
        blockIter = sortedLeft.find(seqIter->first);
      }
      if(blockIter != sortedLeft.end()) {
        OrderingIter block2Iter = history.find(seqIter->first);
        DBG_Assert(block2Iter != history.end());
        int32_t histSeq = blockIter->second;   // intentional conversion from uint8_t
        int32_t hist2Seq = block2Iter->second;   // intentional conversion from uint8_t
        seqIter++;
        while(seqIter != build.endSeq()) {
          int32_t dist, dist2;
          blockIter = sortedLeft.find(seqIter->first);
          if(blockIter != sortedLeft.end()) {
            dist = (int)blockIter->second - histSeq;
            if(fillHits > denseThreshold) {
              statOrderLeftJumpsDense << std::make_pair(dist,1);
              if(!histIsRight) {
                statOrderSameDirJumpsDense << std::make_pair(dist,1);
              } else {
                statOrderDiffDirJumpsDense << std::make_pair(dist,1);
              }
            }
            else {
              statOrderLeftJumpsSparse << std::make_pair(dist,1);
              if(!histIsRight) {
                statOrderSameDirJumpsSparse << std::make_pair(dist,1);
              } else {
                statOrderDiffDirJumpsSparse << std::make_pair(dist,1);
              }
            }
            histSeq = blockIter->second;

            block2Iter = history.find(seqIter->first);
            DBG_Assert(block2Iter != history.end());
            dist2 = (int)block2Iter->second - hist2Seq;
            hist2Seq = block2Iter->second;
            if(!histIsRight) {
              if(fillHits > denseThreshold) {
                if( (abs(dist) <= 2) && (abs(dist2) <= 2) ) {
                  statOrderDenseSeqOrdYesYes++;
                } else if( (abs(dist) <= 2) ) {
                  statOrderDenseSeqOrdYesNo++;
                } else if( (abs(dist2) <= 2) ) {
                  statOrderDenseSeqOrdNoYes++;
                } else {
                  statOrderDenseSeqOrdNoNo++;
                }
              }
              else {
                if( (abs(dist) <= 2) && (abs(dist2) <= 2) ) {
                  statOrderSparseSeqOrdYesYes++;
                } else if( (abs(dist) <= 2) ) {
                  statOrderSparseSeqOrdYesNo++;
                } else if( (abs(dist2) <= 2) ) {
                  statOrderSparseSeqOrdNoYes++;
                } else {
                  statOrderSparseSeqOrdNoNo++;
                }
              }
            }
          }
          seqIter++;
        }
      }

    }
    */

    if (removed) {
      // removed from PHT - remove here as well
      theOrderingHistory.erase(histIter);
    } else {
      // finally replace the old history with the current order
      histIter->second = build;
    }
  }

  void doBufFetchGen(MemoryAddress addr, MemoryAddress pc, int64_t aSeqNo) {
    if (theSendStreams) {
      theCurrPrefetchCommand = new PrefetchCommand(PrefetchCommand::ePrefetchAddressList);
      theCurrPrefetchCommand->tag() = aSeqNo;
    }

    statBufFetchTriggers++;
    theCurrPrefetchGen = 0;

    doBufFetchGen2(addr, pc);

    if (theCurrPrefetchGen > 0) {
      statBufFetchGens++;
      statBufFetchGenSize << std::make_pair(theCurrPrefetchGen, 1);
    }

    if (theSendStreams) {
      if (!theCurrPrefetchCommand->addressList().empty()) {
        std::stringstream prefetchList;
        std::list<PhysicalMemoryAddress>::iterator iter = theCurrPrefetchCommand->addressList().begin();
        while (iter != theCurrPrefetchCommand->addressList().end()) {
          prefetchList << std::hex << makeGroupOffset(*iter, theSgpBits) << " ";
          ++iter;
        }
        DBG_(Trace, ( << theName << " group_" << std::hex << makeGroupAddress(addr, theSgpBits) << " prefetch bits " << prefetchList.str() ) );
        thePrefetchCommandList.push_back(theCurrPrefetchCommand);
      }
      theCurrPrefetchCommand = 0;
    }
  }

  void doBufFetchGen2(MemoryAddress addr, MemoryAddress pc) {
    if (theEnableStreaming) {
      return doStreamGen(addr, pc);
    }

    if (doRotate()) {
      return doBufFetchGenRotate(addr, pc);
    }

    // try to find an existing pattern to prefetch
    RepetIndex index = makeRepetIndex(addr, pc);
    GroupOffset offset = makeGroupOffset(addr, theSgpBits);
    SpatialPattern toPrefetch;
    if (thePhtAssoc > 0) {
      RepetHistoryIter iter = theRepetAssocHistory.find(index);
      if (iter != theRepetAssocHistory.end()) {
        toPrefetch = iter->second.extract();
      }
    } else {
      RepetHistoryIter iter = theRepetHistory.find(index);
      if (iter != theRepetHistory.end()) {
        toPrefetch = iter->second.extract();
      }
    }

    if (toPrefetch.any()) {
      SpatialPattern prefetches(toPrefetch.size());
      if ((int)toPrefetch.count() > getDenseThreshold()) {
        theCurrDense = true;
      } else {
        theCurrDense = false;
      }

      GroupAddress group = makeGroupAddress(addr, theSgpBits);
      GroupRepetIter groupIter = theGroupRepets.find(group);
      SpatialPattern alreadyPrefetched(toPrefetch.size());
      if (theEnableStitch) {
        if (groupIter != theGroupRepets.end()) {
          alreadyPrefetched = groupIter->second.prefetched;
        }
      }

      // found a spatial pattern - prefetch the indicated blocks
      MemoryAddress base = makeGroupAddress(addr, theSgpBits);
      addr = base;
      uint32_t ii;
      for (ii = 0; ii < theSgpBlocks; ii++) {
        if (ii != offset) {
          if (toPrefetch.test(ii)) {
            if (!alreadyPrefetched.test(ii)) {
              //DBG_(Dev, ( << "  prefetching " << std::hex << addr ) );
              if (doBufFetchBlock(addr)) {
                statBufFetchPrefetch++;
                prefetches.set(ii);
              } else {
                statBufFetchDupFetch++;
              }
            } else {
              statBufFetchDupStitch++;
            }
          }
        }
        addr += theBlockSize;
      }

      if (theEnableStitch) {
        if (groupIter != theGroupRepets.end()) {
          groupIter->second.prefetched |= toPrefetch;
        }
      }

      SpatialPattern ttPref(toPrefetch);
      ttPref.reset(offset);
      theTraceTracker.sgpPredict(theNodeId, base, (void *)&ttPref);

      if (theEnableActive && prefetches.any()) {
        DBG_Assert(theActiveGroups.find(base) == theActiveGroups.end());
        theActiveGroups.insert( std::make_pair(base, ActiveGroupEntry(theNumAccesses, prefetches)) );
      }
    }

  }

  void doBufFetchGenShift(MemoryAddress addr, MemoryAddress pc) {
    RepetIndex index = makeRepetIndex(addr, pc);
    index = makeIndexFromConcat(index);

    // try to find an existing pattern to prefetch
    SpatialPattern toPrefetch;
    if (thePhtAssoc > 0) {
      RepetHistoryIter iter = theRepetAssocHistory.find(index);
      if (iter != theRepetAssocHistory.end()) {
        toPrefetch = iter->second.extract();
      }
    } else {
      RepetHistoryIter iter = theRepetHistory.find(index);
      if (iter != theRepetHistory.end()) {
        toPrefetch = iter->second.extract();
      }
    }

    if (toPrefetch.any()) {
      // found a spatial pattern - prefetch the indicated blocks
      MemoryAddress base = makeBlockAddress(addr);
      uint32_t ii;

      addr = base;
      for (ii = 33; ii < (theSgpBlocks + 32); ii++) {
        addr += theBlockSize;
        if (toPrefetch.test(ii)) {
          if (doBufFetchBlock(addr)) {
            statBufFetchPrefetch++;
          } else {
            statBufFetchDupFetch++;
          }
        }
      }

      addr = base;
      for (ii = 31; ii > (32 - theSgpBlocks); ii--) {
        addr -= theBlockSize;
        if (toPrefetch.test(ii)) {
          if (doBufFetchBlock(addr)) {
            statBufFetchPrefetch++;
          } else {
            statBufFetchDupFetch++;
          }
        }
      }

    }
  }

  void doBufFetchGenRotate(MemoryAddress addr, MemoryAddress pc) {
    DBG_Assert(!theEnableStitch); // not much to do - just add the same support as above

    RepetIndex index = makeRepetIndex(addr, pc);
    GroupOffset offset = makeOffsetFromConcat(index);
    index = makeIndexFromConcat(index);

    // try to find an existing pattern to prefetch
    SpatialPattern toPrefetch;
    if (thePhtAssoc > 0) {
      RepetHistoryIter iter = theRepetAssocHistory.find(index);
      if (iter != theRepetAssocHistory.end()) {
        toPrefetch = iter->second.extract();
      }
    } else {
      RepetHistoryIter iter = theRepetHistory.find(index);
      if (iter != theRepetHistory.end()) {
        toPrefetch = iter->second.extract();
      }
    }

    if (toPrefetch.any()) {
      SpatialPattern prefetches(toPrefetch.size());
      if ((int)toPrefetch.count() > getDenseThreshold()) {
        theCurrDense = true;
      } else {
        theCurrDense = false;
      }

      // found a spatial pattern - prefetch the indicated blocks
      MemoryAddress base = makeBlockAddress(addr);
      //DBG_(Dev, ( << "base=" << std::hex << base << " offset=" << offset << " pattern=" << toPrefetch ) );
      uint32_t ii;

      addr = base;
      for (ii = 1; ii < (theSgpBlocks - offset); ii++) {
        addr += theBlockSize;
        if (toPrefetch.test(ii)) {
          //DBG_(Dev, ( << "  prefetching " << std::hex << addr ) );
          if (doBufFetchBlock(addr)) {
            statBufFetchPrefetch++;
            prefetches.set(ii);
          } else {
            statBufFetchDupFetch++;
          }
        }
      }

      addr = base;
      for (ii = theSgpBlocks - 1; ii >= (theSgpBlocks - offset); ii--) {
        addr -= theBlockSize;
        if (toPrefetch.test(ii)) {
          //DBG_(Dev, ( << "  prefetching " << std::hex << addr ) );
          if (doBufFetchBlock(addr)) {
            statBufFetchPrefetch++;
            prefetches.set(ii);
          } else {
            statBufFetchDupFetch++;
          }
        }
      }

      if (theEnableActive && prefetches.any()) {
        DBG_Assert(theActiveGroups.find(base) == theActiveGroups.end());
        theActiveGroups.insert( std::make_pair(base, ActiveGroupEntry(theNumAccesses, prefetches)) );
      }

    }
  }

  // returns true if block not already present in prefetch buffer
  bool doBufFetchBlock(MemoryAddress block) {
    if (theEnablePrefetch) {
      //theRegionFile << std::hex << block << std::endl;
      if (theSendStreams) {
        DBG_Assert(theCurrPrefetchCommand);
        theCurrPrefetchCommand->addressList().push_back( PhysicalMemoryAddress(block) );
      } else {
        thePrefetches.push_back(block);
      }
      theCurrPrefetchGen++;
      statPrefetchCount++;
    }

    if (!theFetchDist) {
      return theFetchedBlocks.insert(block).second;
    }

    bool ret = true;
    OrderedBlocksIter iter = theOrderedBlocks.find(block);
    if (iter != theOrderedBlocks.end()) {
      iter->second.cycles = getCycleCount();
      theOrderedBlocks.move_back(iter);
      ret = false;
    } else {
      if ( (theBufSize > 0) && (theOrderedBlocks.size() >= theBufSize) ) {
        theOrderedBlocks.pop_front();
        statBufFetchDiscard++;
      }
      theOrderedBlocks.insert( std::make_pair(block, OrderedBlockEntry(getCycleCount(), theCurrDense)) );
    }
    return ret;
  }

  // returns true if block actually removed from prefetch buffer
  bool doBufFetchErase(MemoryAddress block, bool isMiss) {
    if (theEnableActive) {
      ActiveGroupIter iter = theActiveGroups.find(makeGroupAddress(block, theSgpBits));
      if (iter != theActiveGroups.end()) {
        GroupOffset offset = makeGroupOffset(block, theSgpBits);
        if (isMiss && iter->second.outstanding.test(offset)) {
          iter->second.recent = theNumAccesses;
        }
        iter->second.outstanding.reset(offset);
        if (iter->second.outstanding.none()) {
          finalizeActiveGroup(iter->second);
          theActiveGroups.erase(iter);
        }
      }
    }

    return doBufFetchErase2(block, isMiss);
  }

  bool doBufFetchErase2(MemoryAddress block, bool isMiss) {
    if (!theFetchDist) {
      return theFetchedBlocks.erase(block) > 0;
    }

    bool ret = false;
    OrderedBlocksIter iter = theOrderedBlocks.find(block);
    if (iter != theOrderedBlocks.end()) {
      ret = true;
      if (isMiss) {
        //statBufFetchDist << theOrderedBlocks.dist_back(iter);
        if (iter->second.dense) {
          statBufFetchDenseDist << (getCycleCount() - iter->second.cycles);
        } else {
          statBufFetchSparseDist << (getCycleCount() - iter->second.cycles);
        }
      }
      theOrderedBlocks.erase(iter);
    }
    return ret;
  }

  void doBufFetchMiss(MemoryAddress block, bool write, bool priv) {
    if (theEnableStreaming) {
      return;
    }

    if (write) {
      statBufFetchMissWrite++;
    } else {
      statBufFetchMissRead++;
    }

    // this is a cache miss - check if the block was prefetched
    if (doBufFetchErase(block, true)) {
      if (write) {
        statBufFetchGoodWrite++;
        if (priv) statBufFetchGoodWriteOS++;
      } else {
        statBufFetchGoodRead++;
        if (priv) statBufFetchGoodReadOS++;
      }
    }
  }

  void doBufFetchInval(MemoryAddress block) {
    // this is an invalidation
    if (doBufFetchErase(block, false)) {
      statBufFetchInval++;
    }
  }

  void doBufFetchEndGen(GroupAddress group) {
    theTraceTracker.endSpatialGen(theNodeId, group);
    if (!theEnableActive) {
      return;
    }

    ActiveGroupIter iter = theActiveGroups.find(group);
    if (iter != theActiveGroups.end()) {
      DBG_Assert(iter->second.outstanding.any());
      MemoryAddress addr = group;
      uint32_t ii;
      for (ii = 0; ii < theSgpBlocks; ii++) {
        if (iter->second.outstanding.test(ii)) {
          bool found = doBufFetchErase2(addr, false);
          DBG_Assert(found);
        }
        addr += theBlockSize;
      }

      finalizeActiveGroup(iter->second);
      theActiveGroups.erase(iter);
    }
  }

  void finalizeActiveGroup(ActiveGroupEntry & entry) {
    theActiveGenBegins[entry.start]++;
    theActiveGenEnds[entry.recent]++;
  }

  uint8_t makeNext(uint8_t curr, bool isRight) {
    uint8_t nextBlock;
    if (isRight) {
      nextBlock = curr + 1;
      if (nextBlock == theSgpBlocks) {
        nextBlock = 0;
      }
    } else {
      nextBlock = curr - 1;
      if (curr == 0) {
        nextBlock = theSgpBlocks - 1;
      }
    }
    return nextBlock;
  }

  void freeStreamDesc() {
    GroupAddress group = theCurrentStreams.front_key();
    CurrentStreamEntry const & entry = theCurrentStreams.front();
    theDoneStreams.insert( std::make_pair(group, DoneStreamEntry(entry.isDense(), entry.possible(), entry.hits())) );
    theCurrentStreams.pop_front();
  }

  void doStreamGen(MemoryAddress addr, MemoryAddress pc) {
    DBG_Assert(!theEnableStitch);
    // try to find an existing sequence to prefetch
    RepetIndex index = makeRepetIndex(addr, pc);
    GroupOffset offset = makeGroupOffset(addr, theSgpBits);
    if (doRotate()) {
      GroupOffset off2 = makeOffsetFromConcat(index);
      DBG_Assert(offset == off2);
      index = makeIndexFromConcat(index);
    }

    OrderingHistoryIter iter = theOrderingHistory.find(index);
    if (iter != theOrderingHistory.end()) {
      // verification
      SpatialPattern toPrefetch;
      if (thePhtAssoc > 0) {
        RepetHistoryIter iter = theRepetAssocHistory.find(index);
        DBG_Assert(iter != theRepetAssocHistory.end());
        toPrefetch = iter->second.extract();
      } else {
        RepetHistoryIter iter = theRepetHistory.find(index);
        DBG_Assert(iter != theRepetHistory.end());
        toPrefetch = iter->second.extract();
      }

      // ignore old stream for this spatial group
      GroupAddress group = makeGroupAddress(addr, theSgpBits);
      CurrentStreamIter stream = theCurrentStreams.find(group);
      if (stream != theCurrentStreams.end()) {
        if (stream->second.isDense()) {
          statStreamLenDense << std::make_pair(stream->second.hits(), 1);
          statStreamLenPossibleDense << std::make_pair(stream->second.pattern().count() - 1, 1);
        } else {
          statStreamLenSparse << std::make_pair(stream->second.hits(), 1);
          statStreamLenPossibleSparse << std::make_pair(stream->second.order().size() - 1, 1);
        }
        theCurrentStreams.erase(stream);
        statStreamReplaceStream++;
      }

      // now check for "done-with-fetch" streams
      DoneStreamIter stream2 = theDoneStreams.find(group);
      if (stream2 != theDoneStreams.end()) {
        if (stream2->second.isDense()) {
          statStreamLenDense << std::make_pair(stream2->second.hits(), 1);
          statStreamLenPossibleDense << std::make_pair(stream2->second.possible() - 1, 1);
        } else {
          statStreamLenSparse << std::make_pair(stream2->second.hits(), 1);
          statStreamLenPossibleSparse << std::make_pair(stream2->second.possible() - 1, 1);
        }
        theDoneStreams.erase(stream2);
      }

      GroupOffset stop = offset;
      if (doRotate()) {
        stop = 0;
      }

      // perform streaming
      OrderingMap & order = iter->second;
      if (theStreamDense && ((int)order.size() > getDenseThreshold())) {
        theCurrDense = true;
        bool isRight = movesRight(order);
        //DBG_(Dev, ( << "init dense stream, " << (isRight?"right":"left") << ", group=" << std::hex << group << ", offset=" << std::dec << offset ) );
        //DBG_(Dev, ( << "  pattern: " << toPrefetch ) );
        int32_t numBlocks = 0;
        uint8_t nextBlock = makeNext(stop, isRight);
        //DBG_(Dev, ( << "  next block init: " << (unsigned)nextBlock ) );
        while (numBlocks < theStreamWindow) {
          if (nextBlock == stop) {
            break;
          }
          if (toPrefetch.test(nextBlock)) {
            //DBG_(Dev, ( << "  bit set for block: " << (unsigned)nextBlock ) );
            numBlocks++;
            GroupOffset prefetch = nextBlock;
            //DBG_(Dev, ( << "  prefetch block: " << (unsigned)prefetch ) );
            if (doRotate()) {
              prefetch += offset;
              if (prefetch >= theSgpBlocks) {
                prefetch -= theSgpBlocks;
              }
              //DBG_(Dev, ( << "    prefetch offset: " << (unsigned)prefetch ) );
            }
            addr = group + (theBlockSize * prefetch);
            //DBG_(Dev, ( << "  streaming " << std::hex << addr << " (block no " << (unsigned)prefetch << ")" ) );
            if (doBufFetchBlock(addr)) {
              //DBG_(Dev, ( << "    fetched block " << std::hex << addr ) );
              statBufFetchPrefetch++;
            } else {
              //DBG_(Dev, ( << "    dup block " << std::hex << addr ) );
              statBufFetchDupFetch++;
            }
          }
          nextBlock = makeNext(nextBlock, isRight);
          //DBG_(Dev, ( << "  next block now: " << (unsigned)nextBlock ) );
        }
        if ( (theStreamDescs > 0) && ( theCurrentStreams.size() >= theStreamDescs) ) {
          freeStreamDesc();
          statStreamEndStream++;
        }
        if (!theSendStreams) {
          //DBG_(Dev, ( << "  new stream, next block: " << std::dec << (unsigned)nextBlock ) );
          theCurrentStreams.insert( std::make_pair(group, CurrentStreamEntry(toPrefetch, nextBlock, offset, isRight)) );
          statStreamNewStream++;
        } else {
          //DBG_(Dev, ( << "  done stream" ) );
          DBG_Assert(nextBlock == stop);
          theDoneStreams.insert( std::make_pair(group, DoneStreamEntry(true, toPrefetch.count(), 0)) );
          statStreamShortStream++;
        }
      } else {
        theCurrDense = false;
        OrderingSeqIter seq = order.beginSeq();
        int32_t numBlocks = 0;
        uint8_t lastBlock = stop;
        //DBG_(Dev, ( << "init sparse stream, group=" << std::hex << group << ", offset=" << std::dec << offset ) );
        //DBG_(Dev, ( << "  sequence: " << printSequence(order) ) );
        while (numBlocks < theStreamWindow) {
          if (seq == order.endSeq()) {
            break;
          }

          DBG_Assert(toPrefetch.test(seq->first));
          if (seq->first != stop) {
            numBlocks++;
            GroupOffset prefetch = seq->first;
            //DBG_(Dev, ( << "  prefetch block: " << (unsigned)prefetch ) );
            if (doRotate()) {
              prefetch += offset;
              if (prefetch >= theSgpBlocks) {
                prefetch -= theSgpBlocks;
              }
              //DBG_(Dev, ( << "    prefetch offset: " << (unsigned)prefetch ) );
            }
            addr = group + (theBlockSize * prefetch);
            //DBG_(Dev, ( << "  streaming " << std::hex << addr ) );
            if (doBufFetchBlock(addr)) {
              //DBG_(Dev, ( << "    fetched block " << std::hex << addr ) );
              statBufFetchPrefetch++;
            } else {
              //DBG_(Dev, ( << "    dup block " << std::hex << addr ) );
              statBufFetchDupFetch++;
            }
          } else {
            //DBG_(Dev, ( << "  ignoring block no " << std::dec << (unsigned)seq->first ) );
          }
          lastBlock = seq->first;

          ++seq;
        }
        if ( (theStreamDescs > 0) && ( theCurrentStreams.size() >= theStreamDescs) ) {
          freeStreamDesc();
          statStreamEndStream++;
        }
        if (!theSendStreams) {
          //DBG_(Dev, ( << "  new stream, last block: " << std::dec << (unsigned)lastBlock ) );
          theCurrentStreams.insert( std::make_pair(group, CurrentStreamEntry(order, lastBlock, offset)) );
          statStreamNewStream++;
        } else {
          //DBG_(Dev, ( << "  done stream" ) );
          DBG_Assert(seq == order.endSeq());
          theDoneStreams.insert( std::make_pair(group, DoneStreamEntry(false, order.size(), 0)) );
          statStreamShortStream++;
        }
      }

    }
  }

  void doStreamAccess(MemoryAddress block, bool write, bool miss, bool priv, RepetReturnType repet) {
    if (miss) {
      if (write) {
        statBufFetchMissWrite++;
      } else {
        statBufFetchMissRead++;
      }
    }

    // this might be a cache miss - check if the block was prefetched
    if (doBufFetchErase(block, miss)) {
      if (miss) {
        if (write) {
          statBufFetchGoodWrite++;
          if (priv) statBufFetchGoodWriteOS++;
        } else {
          statBufFetchGoodRead++;
          if (priv) statBufFetchGoodReadOS++;
        }
      } else {
        if (write) {
          statBufFetchAccessWrite++;
        } else {
          statBufFetchAccessRead++;
        }
      }

      if (repet == kRepetRetNewGen) {
        // new generation - cannot possibly advance a stream
        return;
      }

      // advance the stream
      GroupAddress group = makeGroupAddress(block, theSgpBits);
      CurrentStreamIter iter = theCurrentStreams.find(group);
      if (iter != theCurrentStreams.end()) {
        iter->second.hit();
        if (iter->second.isDense()) {
          theCurrDense = true;
          SpatialPattern & pattern = iter->second.pattern();
          int32_t numBlocks = 0;
          GroupOffset stop = iter->second.offset();
          if (doRotate()) {
            stop = 0;
          }
          //DBG_(Dev, ( << "advance dense stream, " << (iter->second.isRight()?"right":"left") << ", group=" << std::hex << group << ", addr=" << block << ", offset=" << std::dec << iter->second.offset() ) );
          //DBG_(Dev, ( << "  pattern: " << pattern ) );
          //DBG_(Dev, ( << "  next block init: " << (unsigned)iter->second.block() ) );
          while (numBlocks < 1) {
            if (iter->second.block() == stop) {
              break;
            }
            if (pattern.test(iter->second.block())) {
              //DBG_(Dev, ( << "  bit set for block: " << (unsigned)iter->second.block() ) );
              numBlocks++;
              GroupOffset prefetch = iter->second.block();
              //DBG_(Dev, ( << "  prefetch block: " << (unsigned)prefetch ) );
              if (doRotate()) {
                prefetch += iter->second.offset();
                if (prefetch >= theSgpBlocks) {
                  prefetch -= theSgpBlocks;
                }
                //DBG_(Dev, ( << "    prefetch offset: " << (unsigned)prefetch ) );
              }
              MemoryAddress addr = group + (theBlockSize * prefetch);
              //DBG_(Dev, ( << "  streaming " << std::hex << addr ) );
              if (doBufFetchBlock(addr)) {
                //DBG_(Dev, ( << "    fetched block " << std::hex << addr ) );
                statBufFetchPrefetch++;
              } else {
                //DBG_(Dev, ( << "    dup block " << std::hex << addr ) );
                statBufFetchDupFetch++;
              }
            }
            iter->second.block() = makeNext(iter->second.block(), iter->second.isRight());
            //DBG_(Dev, ( << "  next block now: " << (unsigned)iter->second.block() ) );
          }
          /*
          if(iter->second.block() == stop) {
            //DBG_(Dev, ( << "  done stream" ) );
            statStreamEndStream++;
            theDoneStreams.insert( std::make_pair(group,DoneStreamEntry(true,pattern.count(),iter->second.hits())) );
            theCurrentStreams.erase(iter);
          }
          */
        } else {
          theCurrDense = false;
          OrderingMap & order = iter->second.order();
          //DBG_(Dev, ( << "advance sparse stream, group=" << std::hex << group << ", addr=" << block << ", offset=" << std::dec << iter->second.offset() ) );
          //DBG_(Dev, ( << "  sequence: " << printSequence(order) ) );
          //DBG_(Dev, ( << "  last block no = " << std::dec << (unsigned)iter->second.block() ) );
          OrderingSeqIter seq = order.findSeq(iter->second.block());
          DBG_Assert(seq != order.endSeq());
          ++seq;
          if (seq != order.endSeq()) {
            GroupOffset prefetch = seq->first;
            //DBG_(Dev, ( << "  prefetch block: " << (unsigned)prefetch ) );
            if (doRotate()) {
              prefetch += iter->second.offset();
              if (prefetch >= theSgpBlocks) {
                prefetch -= theSgpBlocks;
              }
              //DBG_(Dev, ( << "    prefetch offset: " << (unsigned)prefetch ) );
            }
            MemoryAddress addr = group + (theBlockSize * prefetch);
            //DBG_(Dev, ( << "  streaming " << std::hex << addr ) );
            if (doBufFetchBlock(addr)) {
              //DBG_(Dev, ( << "    fetched block " << std::hex << addr ) );
              statBufFetchPrefetch++;
            } else {
              //DBG_(Dev, ( << "    dup block " << std::hex << addr ) );
              statBufFetchDupFetch++;
            }
            iter->second.block() = seq->first;
          }
          /*
          else {
            //DBG_(Dev, ( << "  done stream" ) );
            statStreamEndStream++;
            theDoneStreams.insert( std::make_pair(group,DoneStreamEntry(false,order.size(),iter->second.hits())) );
            theCurrentStreams.erase(iter);
          }
          */
        }
        theCurrentStreams.move_back(iter);
      } else {
        // check with the "done" streams
        DoneStreamIter stream = theDoneStreams.find(group);
        if (stream != theDoneStreams.end()) {
          //DBG_(Dev, ( << "hit done stream, group=" << std::hex << group << ", addr=" << block ) );
          stream->second.hit();
        } else {
          // no stream info (neither current nor done)
          //DBG_(Dev, ( << "hit no stream, group=" << std::hex << group << ", addr=" << block ) );
          statStreamOrphanHit++;
        }
      }
    } else {
      if (repet == kRepetRetNewGen) {
        // new generation - cannot possibly jump a stream
        return;
      }

      // if this was a miss that was not streamed, but still maps to a group
      // that is currently streaming, we can "jump" in the stream
      if (repet == kRepetRetFirstAccess) {
        GroupAddress group = makeGroupAddress(block, theSgpBits);
        CurrentStreamIter iter = theCurrentStreams.find(group);
        if (iter != theCurrentStreams.end()) {
          GroupOffset offset = makeGroupOffset(block, theSgpBits);
          //DBG_(Dev, ( << "stream miss, block=" << std::hex << group << " offset=" << std::dec << offset ) );
          if (doRotate()) {
            if (offset >= iter->second.offset()) {
              offset -= iter->second.offset();
            } else {
              offset = theSgpBlocks - (iter->second.offset() - offset);
            }
          }

          if (iter->second.isDense()) {
            theCurrDense = true;
            SpatialPattern & pattern = iter->second.pattern();
            int32_t numBlocks = 0;
            GroupOffset stop = iter->second.offset();
            if (doRotate()) {
              stop = 0;
            }
            //DBG_(Dev, ( << "reposition dense stream, " << (iter->second.isRight()?"right":"left") << ", group=" << std::hex << group << ", offset=" << std::dec << iter->second.offset() << ", jump offset=" << offset ) );
            //DBG_(Dev, ( << "  pattern: " << pattern ) );
            // only "jump" if this miss was part of the previous pattern
            if (pattern.test(offset)) {
              // reset next block based on current miss
              iter->second.block() = makeNext(offset, iter->second.isRight());
              //DBG_(Dev, ( << "  next block reset: " << (unsigned)iter->second.block() ) );
              // now continue as before
              while (numBlocks < theStreamWindow) {
                if (iter->second.block() == stop) {
                  break;
                }
                if (pattern.test(iter->second.block())) {
                  //DBG_(Dev, ( << "  bit set for block: " << (unsigned)iter->second.block() ) );
                  numBlocks++;
                  GroupOffset prefetch = iter->second.block();
                  //DBG_(Dev, ( << "  prefetch block: " << (unsigned)prefetch ) );
                  if (doRotate()) {
                    prefetch += iter->second.offset();
                    if (prefetch >= theSgpBlocks) {
                      prefetch -= theSgpBlocks;
                    }
                    //DBG_(Dev, ( << "    prefetch offset: " << (unsigned)prefetch ) );
                  }
                  MemoryAddress addr = group + (theBlockSize * prefetch);
                  //DBG_(Dev, ( << "  streaming " << std::hex << addr ) );
                  if (doBufFetchBlock(addr)) {
                    //DBG_(Dev, ( << "    fetched block " << std::hex << addr ) );
                    statBufFetchPrefetch++;
                  } else {
                    //DBG_(Dev, ( << "    dup block " << std::hex << addr ) );
                    statBufFetchDupFetch++;
                  }
                }
                iter->second.block() = makeNext(iter->second.block(), iter->second.isRight());
                //DBG_(Dev, ( << "  next block now: " << (unsigned)iter->second.block() ) );
              }
              /*
              if(iter->second.block() == stop) {
                //DBG_(Dev, ( << "  done stream" ) );
                statStreamEndStream++;
                theDoneStreams.insert( std::make_pair(group,DoneStreamEntry(true,pattern.count(),iter->second.hits())) );
                theCurrentStreams.erase(iter);
              }
              */
            }
          } else {
            theCurrDense = false;
            OrderingMap & order = iter->second.order();
            OrderingSeqIter seq = order.findSeq(offset);
            int32_t numBlocks = 0;
            // only "jump" if this miss was part of the previous sequence
            if (seq != order.endSeq()) {
              //DBG_(Dev, ( << "reposition sparse stream, group=" << std::hex << group << ", offset=" << std::dec << iter->second.offset() << ", jump offset=" << offset ) );
              //DBG_(Dev, ( << "  sequence: " << printSequence(order) ) );
              while (numBlocks < theStreamWindow) {
                ++seq;
                if (seq == order.endSeq()) {
                  break;
                }

                numBlocks++;
                GroupOffset prefetch = seq->first;
                //DBG_(Dev, ( << "  prefetch block: " << (unsigned)prefetch ) );
                if (doRotate()) {
                  prefetch += iter->second.offset();
                  if (prefetch >= theSgpBlocks) {
                    prefetch -= theSgpBlocks;
                  }
                  //DBG_(Dev, ( << "    prefetch offset: " << (unsigned)prefetch ) );
                }
                MemoryAddress addr = group + (theBlockSize * prefetch);
                //DBG_(Dev, ( << "  streaming " << std::hex << addr ) );
                if (doBufFetchBlock(addr)) {
                  //DBG_(Dev, ( << "    fetched block " << std::hex << addr ) );
                  statBufFetchPrefetch++;
                } else {
                  //DBG_(Dev, ( << "    dup block " << std::hex << addr ) );
                  statBufFetchDupFetch++;
                }
                iter->second.block() = seq->first;
              }
              /*
              if(seq == order.endSeq()) {
                //DBG_(Dev, ( << "  done stream" ) );
                statStreamEndStream++;
                theDoneStreams.insert( std::make_pair(group,DoneStreamEntry(false,order.size(),iter->second.hits())) );
                theCurrentStreams.erase(iter);
              }
              */
            }
          }
          theCurrentStreams.move_back(iter);

        }
      }
    }
  }

  std::string printSequence(OrderingMap & order) {
    std::stringstream ret;
    OrderingSeqIter iter2 = order.beginSeq();
    uint8_t seqNo = 0;
    while (iter2 != order.endSeq()) {
      DBG_Assert(iter2->second == seqNo);
      ++seqNo;
      ret <<  " " << (uint32_t)iter2->first;
      ++iter2;
    }
    return ret.str();
  }

  void doGroupTime(MemoryAddress addr, MemoryAddress pc, bool evict, bool beginend) {
    GroupAddress group = makeGroupAddress(addr, theSgpBits);
    uint64_t curr = theFlexus->cycleCount();
    GroupTimeIter iter = theGroupTimes.find(group);
    if (iter != theGroupTimes.end()) {
      if (!evict) {
        // this is an access
        if (iter->second.live) {
          statTimeInterAccess << (curr - iter->second.lastAccess);
        } else {
          statTimeInterDeadAccess << (curr - iter->second.lastAccess);
        }
        iter->second.lastAccess = curr;
        if (beginend) {
          DBG_Assert(!iter->second.live);
          iter->second.live = true;
          statTimeGroupDead << (curr - iter->second.lastEnd);
          statTimeInterBegin << (curr - iter->second.lastBegin);
          iter->second.lastBegin = curr;
        }
      } else {
        // this is an evict
        if (iter->second.live) {
          statTimeInterLiveEvict << (curr - iter->second.lastEvict);
        } else {
          statTimeInterEvict << (curr - iter->second.lastEvict);
        }
        iter->second.lastEvict = curr;
        if (beginend) {
          DBG_Assert(iter->second.live);
          iter->second.live = false;
          statTimeGroupLive << (curr - iter->second.lastBegin);
          statTimeInterEnd << (curr - iter->second.lastEnd);
          iter->second.lastEnd = curr;
        }
      }
    } else {
      // first occurence of this group
      DBG_Assert(!evict);
      DBG_Assert(beginend);
      theGroupTimes.insert( std::make_pair(group, GroupTimeEntry(curr)) );
    }
  }

  uint64_t getCycleCount() {
    //return nDecoupledFeeder::theTraceHackManager->cycleCount(theNodeId);
    return 0;
  }

  bool doRotate() {
    if ( theRepetType == 6 || theRepetType == 7 ) {
      return true;
    }
    return false;
  }

  SpatialPattern doRotate(SpatialPattern pattern, GroupOffset offset) {
    SpatialPattern top( pattern << (theSgpBlocks - offset) );
    SpatialPattern bottom( pattern >> offset );
    return (top | bottom);
  }

  BlockAddress makeBlockAddress(MemoryAddress addr) {
    BlockAddress res = (addr & ~(theBlockOffsetMask));
    //DBG_(Dev, ( << "makeBlockAddress: addr=" << std::hex << addr << "res=" << res ) );
    return res;
  }

  BlockAddress makeShiftedBlockAddress(MemoryAddress addr) {
    BlockAddress res = addr >> theBlockBits;
    //DBG_(Dev, ( << "makeShiftedBlockAddress: addr=" << std::hex << addr << "res=" << res ) );
    return res;
  }

  GroupAddress makeGroupAddress(MemoryAddress addr, int32_t groupBits) {
    uint32_t offsetMask = (theBlockSize << groupBits) - 1;
    GroupAddress res = (addr & ~(offsetMask));
    //DBG_(Dev, ( << "makeGroupAddress: addr=" << std::hex << addr << " bits=" << groupBits << " res=" << res ) );
    return res;
  }

  SpatialPattern makeGroupPattern(MemoryAddress addr, int32_t groupBits) {
    //SpatialPattern res = (1ULL << (uint64_t)makeGroupOffset(addr,groupBits));
    unsigned len = 1UL << groupBits;
    SpatialPattern res(len);
    res.set(makeGroupOffset(addr, groupBits));
    //DBG_(Dev, ( << "makeGroupPattern: addr=" << std::hex << addr << " bits=" << groupBits << " res=" << res ) );
    return res;
  }

  GroupOffset makeGroupIndex(MemoryAddress addr, int32_t groupBits) {
    GroupOffset res = addr >> (theBlockBits + groupBits);
    //DBG_(Dev, ( << "makeGroupIndex: addr=" << std::hex << addr << " bits=" << groupBits << " res=" << res ) );
    return res;
  }

  GroupOffset makeGroupOffset(MemoryAddress addr, int32_t groupBits) {
    GroupOffset res = ( (addr >> theBlockBits) & ((1 << groupBits) - 1) );
    //DBG_(Dev, ( << "makeGroupOffset: addr=" << std::hex << addr << " bits=" << groupBits << " res=" << res ) );
    return res;
  }

  GroupOffset makeWordOffset(MemoryAddress addr) {
    GroupOffset res = ( (addr >> 3) & ((1 << theWordBits) - 1) );
    //DBG_(Dev, ( << "makeWordOffset: addr=" << std::hex << addr << " res=" << res ) );
    return res;
  }

  GroupOffset makeByteOffset(MemoryAddress addr) {
    GroupOffset res = ( addr & ((1 << theByteBits) - 1) );
    //DBG_(Dev, ( << "makeByteOffset: addr=" << std::hex << addr << " res=" << res ) );
    return res;
  }

  RepetIndex makeRepetIndex(MemoryAddress addr, MemoryAddress pc) {
    pc = (pc >> 2) & thePCmask;
    RepetIndex res = 0;
    GroupOffset groupOffset(0);
    switch (theRepetType) {
      case 0:  // PC only
        res = pc;
        break;
      case 1:  // PC + block offset within SG
        groupOffset = makeGroupOffset(addr, theSgpBits);
        res = (uint64_t)pc << theSgpBits;
        res |= groupOffset;
        break;
      case 2:  // PC + address
        //res = ((uint64_t)pc << 32) | (uint64_t) makeBlockAddress(addr);
        res = ((uint64_t) makeShiftedBlockAddress(addr) << thePcBits) | (uint64_t)pc;
        break;
      case 3:  // block address only
        res = makeBlockAddress(addr);
        break;
      case 4:  // PC + word offset within SG
        groupOffset = makeWordOffset(addr);
        res = (uint64_t)pc << theWordBits;
        res |= groupOffset;
        break;
      case 5:  // PC + byte offset within SG
        groupOffset = makeByteOffset(addr);
        res = (uint64_t)pc << theByteBits;
        res |= groupOffset;
        break;
      case 6:  // PC + rotated block offset
        // create the index as for PC + block offset - rotating is handled elsewhere
        groupOffset = makeGroupOffset(addr, theSgpBits);
        res = (uint64_t)pc << theSgpBits;
        res |= groupOffset;
        break;
      case 7:  // PC + rotated word offset
        // create the index as for PC + word offset - rotating is handled elsewhere
        groupOffset = makeWordOffset(addr);

        //groupOffset &= ~((1<<theWordInBlockBits)-1);
        //groupOffset |= ( (addr>>12) & ((1<<theWordInBlockBits)-1) );

        res = (uint64_t)pc << theWordBits;
        res |= groupOffset;
        break;
      case 8:  // block offset only
        res = makeGroupOffset(addr, theSgpBits);
        break;
      case 9:  // group address only
        res = makeBlockAddress(addr);
        break;
      default:
        DBG_Assert(false, ( << "invalid repet type: " << theRepetType ) );
    }
    //DBG_(Dev, ( << "makeRepetIndex (type=" << RepetTypeStr[theRepetType] << "): addr=" << std::hex << addr
    //            << " pc=" << pc << " offset=" << groupOffset << " res=" << res ) );
    return res;
  }

  RepetIndex makeIndexFromConcat(RepetIndex index) {
    switch (theRepetType) {
      case 6:  // PC + rotated block offset
        index >>= theSgpBits;
        break;
      case 7:  // PC + rotated word offset
        index = ((index >> theWordBits) << theWordInBlockBits) | (index & ((1 << theWordInBlockBits) - 1));
        break;
      default:
        // do nothing
        break;
    }
    return index;
  }
  GroupOffset makeOffsetFromConcat(RepetIndex index) {
    switch (theRepetType) {
      case 6:  // PC + rotated block offset
        index &= (theSgpBlocks - 1);
        break;
      case 7:  // PC + rotated word offset
        index = (index >> theWordInBlockBits) & (theSgpBlocks - 1);
        break;
      default:
        DBG_Assert(false, ( << "not a rotated repet type: " << theRepetType ) );
    }
    return index;
  }

  MemoryAddress extractPC(RepetIndex idx, int32_t groupBits) {
    DBG_Assert(theRepetType == 1);
    MemoryAddress res = (idx >> groupBits) << 2;
    //DBG_(Dev, ( << "extractPC: idx=" << std::hex << idx << " res=" << res ) );
    return res;
  }

  GroupOffset extractGroupOffset(RepetIndex idx, int32_t groupBits) {
    DBG_Assert(theRepetType == 1);
    GroupOffset res = idx & ((1 << groupBits) - 1);
    //DBG_(Dev, ( << "extractGroupOffset: idx=" << std::hex << idx << " res=" << res ) );
    return res;
  }

  PhtIndex makePhtIndex(MemoryAddress addr, MemoryAddress pc, int32_t groupBits) {
    GroupOffset groupOffset = makeGroupOffset(addr, groupBits);
    uint64_t temp = ((uint64_t)pc << groupBits) >> 2;
    temp |= groupOffset;
    PhtIndex res = (PhtIndex)temp;
    //DBG_(Dev, ( << "makePhtIndex: addr=" << std::hex << addr << " pc=" << pc << " bits="
    //            << groupBits << " offset=" << groupOffset << " res=" << res ) );
    return res;
  }

  /*
  uint32_t countBits(SpatialPattern pattern) {
    //DBG_(Dev, ( << "countBits: pattern=" << std::hex << pattern ) );
    uint32_t count = 0;
    int32_t ii;
    for(ii = 0; ii < 64; ii++) {
      if(pattern & 0x1) {
        count++;
      }
      pattern >>= 1;
    }
    //DBG_(Dev, ( << "   res=" << std::dec << count ) );
    return count;
  }
  */

  uint32_t countBits(SpatialPattern pattern) {
    //DBG_(Dev, ( << "countBits: pattern=" << std::hex << pattern ) );
    uint32_t count = pattern.count();
    //DBG_(Dev, ( << "   res=" << std::dec << count ) );
    return count;
  }

  uint32_t mylog2(uint32_t num) {
    uint32_t ii = 0;
    while (num > 1) {
      ii++;
      num >>= 1;
    }
    return ii;
  }

  void saveState(std::ostream & ofs) {
    ofs << std::dec << theBlockSize << std::endl;
    ofs << std::dec << theSgpBlocks << std::endl;
    ofs << std::dec << theRepetType << std::endl;
    ofs << std::dec << (theRepetFills ? 1 : 0) << std::endl;
    ofs << std::dec << (theSparseOpt ? 1 : 0) << std::endl;
    ofs << std::dec << thePcBits << std::endl;
    ofs << std::dec << (theFetchDist ? 1 : 0) << std::endl;
    ofs << std::dec << (theEnableOrdering ? 1 : 0) << std::endl;
    ofs << std::dec << SpatialHistory::type() << std::endl;
    ofs << std::dec << theCptType << std::endl;
    ofs << std::dec << theCptSize << std::endl;
    ofs << std::dec << theCptAssoc << std::endl;
    ofs << std::dec << (theCptSparse ? 1 : 0) << std::endl;
    ofs << std::dec << thePhtSize << std::endl;
    ofs << std::dec << thePhtAssoc << std::endl;

    ofs << "CPTtable" << std::endl;
    ofs << "{" << std::endl;
    if (theCptAssoc > 0) {
      GroupRepetSeq iter = theGroupAssocRepets.seq_begin();
      while (iter != theGroupAssocRepets.seq_end()) {
        ofs << std::hex << iter->first << " " << iter->second << std::endl;
        theGroupAssocRepets.seq_next(iter);
      }
    } else {
      GroupRepetSeq iter = theGroupRepets.beginSeq();
      while (iter != theGroupRepets.endSeq()) {
        ofs << std::hex << iter->first << " " << iter->second << std::endl;
        ++iter;
      }
    }
    ofs << "}" << std::endl;

    if (theEnableOrdering) {
      ofs << "CPTorder" << std::endl << "{" << std::endl;
      OrderingBuildIter iter = theOrderingBuild.begin();
      while (iter != theOrderingBuild.end()) {
        ofs << std::hex << iter->first;
        OrderingMap & order = iter->second.theOrder;
        OrderingSeqIter iter2 = order.beginSeq();
        uint8_t seqNo = 0;
        while (iter2 != order.endSeq()) {
          DBG_Assert(iter2->second == seqNo);
          ++seqNo;
          ofs << " " << (uint32_t)iter2->first;
          ++iter2;
        }
        ofs << std::endl;
        ++iter;
      }
      ofs << "}" << std::endl;
    }

    ofs << "PHTtable" << std::endl;
    ofs << "{" << std::endl;
    if (thePhtAssoc > 0) {
      // first build a map to order the entries
      std::map<uint64_t, RepetIndex> repetOrder;
      RepetHistoryIter iter = theRepetAssocHistory.index_begin();
      while (iter != theRepetAssocHistory.index_end()) {
        //DBG_(Dev, ( << "adding repet index " << std::hex << iter->first ) );
        bool ok = repetOrder.insert( std::make_pair(iter->second.lastAccess(), iter->first) ).second;
        DBG_Assert(ok);
        theRepetAssocHistory.index_next(iter);
      }
      // now walk through the map (it will be sorted) to write things in order
      std::map<uint64_t, RepetIndex>::iterator iter2 = repetOrder.begin();
      uint64_t lastAccess = 0;
      while (iter2 != repetOrder.end()) {
        DBG_Assert(iter2->first > lastAccess);
        lastAccess = iter2->first;
        iter = theRepetAssocHistory.find(iter2->second);
        ofs << std::hex << iter->first << " " << iter->second << std::endl;
        ++iter2;
      }
    } else {
      // first build a map to order the entries
      std::map<uint64_t, RepetIndex> repetOrder;
      RepetHistoryIter iter = theRepetHistory.begin();
      while (iter != theRepetHistory.end()) {
        bool ok = repetOrder.insert( std::make_pair(iter->second.lastAccess(), iter->first) ).second;
        DBG_Assert(ok);
        ++iter;
      }
      // now walk through the map (it will be sorted) to write things in order
      std::map<uint64_t, RepetIndex>::iterator iter2 = repetOrder.begin();
      uint64_t lastAccess = 0;
      while (iter2 != repetOrder.end()) {
        DBG_Assert(iter2->first > lastAccess);
        lastAccess = iter2->first;
        iter = theRepetHistory.find(iter2->second);
        ofs << std::hex << iter->first << " " << iter->second << std::endl;
        ++iter2;
      }
    }
    ofs << "}" << std::endl;

    if (theEnableOrdering) {
      ofs << "PHTorder" << std::endl << "{" << std::endl;
      OrderingHistoryIter iter = theOrderingHistory.begin();
      while (iter != theOrderingHistory.end()) {
        ofs << std::hex << iter->first;
        OrderingMap & order = iter->second;
        OrderingSeqIter iter2 = order.beginSeq();
        uint8_t seqNo = 0;
        while (iter2 != order.endSeq()) {
          DBG_Assert(iter2->second == seqNo);
          ++seqNo;
          ofs << " " << (uint32_t)iter2->first;
          ++iter2;
        }
        ofs << std::endl;
        ++iter;
      }
      ofs << "}" << std::endl;
    }

    ofs << "Buf" << std::endl << "{" << std::endl;
    if (theFetchDist) {
      OrderedBlocksMap::seq_iter iter = theOrderedBlocks.beginSeq();
      while (iter != theOrderedBlocks.endSeq()) {
        ofs << std::hex << iter->first << std::endl;
        ++iter;
      }
    } else {
      std::set<MemoryAddress>::iterator iter = theFetchedBlocks.begin();
      while (iter != theFetchedBlocks.end()) {
        ofs << std::hex << *iter << std::endl;
        ++iter;
      }
    }
    ofs << "}" << std::endl;
  }

  bool loadState(std::istream & ifs) {
    uint32_t tempVal;
    bool tempBool;
    std::string tempStr;
    std::string token;
    const int32_t bufferSize = 10240;
    char buffer[bufferSize];

    ifs >> tempVal;
    if (tempVal != theBlockSize) {
      DBG_(Dev, ( << "Error: parsed block size " << tempVal << " does not match config" ) );
      return false;
    }
    ifs >> tempVal;
    if (tempVal != theSgpBlocks) {
      DBG_(Dev, ( << "Error: parsed sgp blocks " << tempVal << " does not match config" ) );
      return false;
    }
    ifs >> tempVal;
    if (tempVal != theRepetType) {
      DBG_(Dev, ( << "Error: parsed repet type " << tempVal << " does not match config" ) );
      return false;
    }
    ifs >> tempBool;
    if (tempBool != theRepetFills) {
      DBG_(Dev, ( << "Error: parsed repet fills " << tempBool << " does not match config" ) );
      return false;
    }
    ifs >> tempBool;
    if (tempBool != theSparseOpt) {
      DBG_(Dev, ( << "Error: parsed sparse opt " << tempBool << " does not match config" ) );
      return false;
    }
    ifs >> tempVal;
    if (tempVal != thePcBits) {
      DBG_(Dev, ( << "Error: parsed PC bits " << tempVal << " does not match config" ) );
      return false;
    }
    ifs >> tempBool;
    if (tempBool != theFetchDist) {
      DBG_(Dev, ( << "Error: parsed fetch dist " << tempBool << " does not match config" ) );
      return false;
    }
    ifs >> tempBool;
    if (tempBool != theEnableOrdering) {
      DBG_(Dev, ( << "Error: parsed enable ordering " << tempBool << " does not match config" ) );
      return false;
    }
    ifs >> tempStr;
    if (tempStr != SpatialHistory::type()) {
      DBG_(Dev, ( << "Error: parsed repet history " << tempStr << " does not match simulator" ) );
      return false;
    }
    ifs >> tempVal;
    if (tempVal != theCptType) {
      DBG_(Dev, ( << "Error: parsed CPT type " << tempVal << " does not match config" ) );
      return false;
    }
    ifs >> tempVal;
    if (tempVal != theCptSize) {
      DBG_(Dev, ( << "Error: parsed CPT size " << tempVal << " does not match config" ) );
      return false;
    }
    ifs >> tempVal;
    if (tempVal != theCptAssoc) {
      DBG_(Dev, ( << "Error: parsed CPT assoc " << tempVal << " does not match config" ) );
      return false;
    }
    ifs >> tempBool;
    if (tempBool != theCptSparse) {
      DBG_(Dev, ( << "Error: parsed CPT sparse " << tempBool << " does not match config" ) );
      return false;
    }
    ifs >> tempVal;
    if (tempVal != thePhtSize) {
      DBG_(Dev, ( << "Error: parsed PHT size " << tempVal << " does not match config" ) );
      return false;
    }
    ifs >> tempVal;
    if (tempVal != thePhtAssoc) {
      DBG_(Dev, ( << "Error: parsed PHT assoc " << tempVal << " does not match config" ) );
      return false;
    }

    ifs >> token;
    DBG_Assert(token == "CPTtable", ( << "token=\"" << token << "\"" ) );
    ifs >> token;
    DBG_Assert(token == "{");
    ifs >> token;
    while (token != "}") {
      GroupAddress addr;
      std::istringstream buf(token);
      buf >> std::hex >> addr;
      //DBG_(Dev, ( << "read addr=" << std::hex << addr ) );
      ifs.getline(buffer, bufferSize);
      //DBG_(Dev, ( << "read line=" << buffer ) );
      std::istringstream buf2(buffer);
      SpatialGroupEntry temp(buf2, theSgpBlocks);
      if (countBits(temp.pattern) <= 1) {
        theCptSparseCount++;
      }
      if (theCptAssoc > 0) {
        theGroupAssocRepets.push_back( std::make_pair(addr, temp) );
        if (theGroupAssocRepets.size() > theCptAssoc) {
          theGroupAssocRepets.pop_front();
        }
      } else {
        theGroupRepets.push_back( std::make_pair(addr, temp) );
        if (theCptSize > 0) {
          if (theCptSparse) {
            if ((theGroupRepets.size() - theCptSparseCount) > theCptSize) {
              // find the oldest non-sparse
              GroupRepetSeq seq = theGroupRepets.beginSeq();
              while (countBits(seq->second.pattern) <= 1) {
                ++seq;
                DBG_Assert(seq != theGroupRepets.endSeq());
              }
              theGroupRepets.eraseSeq(seq);
            }
            if (theCptFilterSize > 0) {
              if (theCptSparseCount > theCptFilterSize) {
                // find the oldest sparse
                GroupRepetSeq seq = theGroupRepets.beginSeq();
                while (countBits(seq->second.pattern) > 1) {
                  ++seq;
                  DBG_Assert(seq != theGroupRepets.endSeq());
                }
                theGroupRepets.eraseSeq(seq);
                theCptSparseCount--;
              }
            }
          } else {
            if (theGroupRepets.size() > theCptSize) {
              theGroupRepets.pop_front();
            }
          }
        }
      }
      ifs >> token;
    }

    if (theEnableOrdering) {
      ifs >> token;
      DBG_Assert(token == "CPTorder", ( << "token=\"" << token << "\"" ) );
      ifs >> token;
      DBG_Assert(token == "{");
      ifs >> token;
      while (token != "}") {
        std::istringstream buf(token);
        GroupAddress addr;
        buf >> std::hex >> addr;
        //DBG_(Dev, ( << "read addr=" << std::hex << addr ) );
        bool found = false;
        if (theCptAssoc > 0) {
          if (theGroupAssocRepets.find(addr) != theGroupAssocRepets.end()) {
            found = true;
          }
        } else {
          if (theGroupRepets.find(addr) != theGroupRepets.end()) {
            found = true;
          }
        }
        ifs.getline(buffer, bufferSize);
        //DBG_(Dev, ( << "read line=" << buffer ) );
        std::istringstream buf2(buffer);
        if (found) {
          OrderingBuildIter iter = theOrderingBuild.insert( std::make_pair(addr, GroupOrderingEntry()) ).first;
          while (buf2 >> std::hex >> tempVal) {
            //DBG_(Dev, ( << "appended block no " << tempVal ) );
            iter->second.nextBlock(tempVal);
          }
        }
        ifs >> token;
      }
    }

    ifs >> token;
    DBG_Assert(token == "PHTtable", ( << "token=\"" << token << "\"" ) );
    ifs >> token;
    DBG_Assert(token == "{");
    ifs >> token;
    while (token != "}") {
      std::istringstream buf(token);
      RepetIndex index;
      buf >> std::hex >> index;
      //DBG_(Dev, ( << "read index=" << std::hex << index ) );
      ifs.getline(buffer, bufferSize);
      //DBG_(Dev, ( << "read line=" << buffer ) );
      std::istringstream buf2(buffer);
      SpatialHistory temp(buf2, theSgpBlocks);
      if (temp.lastAccess() > theCounter) {
        theCounter = temp.lastAccess();
      }
      if (thePhtAssoc > 0) {
        theRepetAssocHistory.push_back( std::make_pair(index, temp) );
        if (theRepetAssocHistory.size() > thePhtAssoc) {
          theRepetAssocHistory.pop_front();
        }
      } else {
        theRepetHistory.push_back( std::make_pair(index, temp) );
        if ( (thePhtSize > 0) && (theRepetHistory.size() > thePhtSize) ) {
          theRepetHistory.pop_front();
        }
      }
      ifs >> token;
    }

    if (theEnableOrdering) {
      ifs >> token;
      DBG_Assert(token == "PHTorder", ( << "token=\"" << token << "\"" ) );
      ifs >> token;
      DBG_Assert(token == "{");
      ifs >> token;
      while (token != "}") {
        std::istringstream buf(token);
        RepetIndex index;
        buf >> std::hex >> index;
        //DBG_(Dev, ( << "read index=" << std::hex << index ) );
        bool found = false;
        if (thePhtAssoc > 0) {
          if (theRepetAssocHistory.find(index) != theRepetAssocHistory.end()) {
            found = true;
          }
        } else {
          if (theRepetHistory.find(index) != theRepetHistory.end()) {
            found = true;
          }
        }
        ifs.getline(buffer, bufferSize);
        //DBG_(Dev, ( << "read line=" << buffer ) );
        std::istringstream buf2(buffer);
        if (found) {
          OrderingHistoryIter iter = theOrderingHistory.insert( std::make_pair(index, OrderingMap()) ).first;
          uint8_t seqNo = 0;
          while (buf2 >> std::hex >> tempVal) {
            //DBG_(Dev, ( << "appended block no " << tempVal ) );
            iter->second.push_back( std::make_pair(tempVal, seqNo) );
            seqNo++;
          }
        }
        ifs >> token;
      }
    }

    ifs >> token;
    DBG_Assert(token == "Buf", ( << "token=\"" << token << "\"" ) );
    ifs >> token;
    DBG_Assert(token == "{");
    ifs >> token;
    while (token != "}") {
      std::istringstream buf(token);
      MemoryAddress addr;
      buf >> std::hex >> addr;
      //DBG_(Dev, ( << "read addr=" << std::hex << addr ) );
      if (theFetchDist) {
        theOrderedBlocks.push_back( std::make_pair(addr, OrderedBlockEntry(0, false)) );
      } else {
        theFetchedBlocks.insert(addr);
      }
      ifs >> token;
    }

    return true;
  }

}; // end class SpatialPrefetcher

} // end namespace nSpatialPrefetcher

#define DBG_Reset
#include DBG_Control()
