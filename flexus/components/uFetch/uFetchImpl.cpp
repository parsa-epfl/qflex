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
#include <fstream>
#include <set>

#include <boost/none.hpp>
#include <core/boost_extensions/padded_string_cast.hpp>
#include <core/stats.hpp>

#include <components/uFetch/uFetch.hpp>
#include <components/uFetch/uFetchTypes.hpp>
#include <components/MTManager/MTManager.hpp>

#include <components/CommonQEMU/Transports/MemoryTransport.hpp>
#include <components/CommonQEMU/Slices/MemoryMessage.hpp>
#include <components/CommonQEMU/Slices/TransactionTracker.hpp>
#include <components/CommonQEMU/Slices/ExecuteState.hpp>

#define FLEXUS_BEGIN_COMPONENT uFetch
#include FLEXUS_BEGIN_COMPONENT_IMPLEMENTATION()

#define DBG_DefineCategories uFetch
#define DBG_SetDefaultOps AddCat(uFetch)
#include DBG_Control()

#include <core/qemu/mai_api.hpp>
#include <components/CommonQEMU/seq_map.hpp>

#define LOG2(x)         \
  ((x)==1 ? 0 :         \
  ((x)==2 ? 1 :         \
  ((x)==4 ? 2 :         \
  ((x)==8 ? 3 :         \
  ((x)==16 ? 4 :        \
  ((x)==32 ? 5 :        \
  ((x)==64 ? 6 :        \
  ((x)==128 ? 7 :       \
  ((x)==256 ? 8 :       \
  ((x)==512 ? 9 :       \
  ((x)==1024 ? 10 :     \
  ((x)==2048 ? 11 :     \
  ((x)==4096 ? 12 :     \
  ((x)==8192 ? 13 :     \
  ((x)==16384 ? 14 :    \
  ((x)==32768 ? 15 :    \
  ((x)==65536 ? 16 :    \
  ((x)==131072 ? 17 :   \
  ((x)==262144 ? 18 :   \
  ((x)==524288 ? 19 :   \
  ((x)==1048576 ? 20 :  \
  ((x)==2097152 ? 21 :  \
  ((x)==4194304 ? 22 :  \
  ((x)==8388608 ? 23 :  \
  ((x)==16777216 ? 24 : \
  ((x)==33554432 ? 25 : \
  ((x)==67108864 ? 26 : -0xffff)))))))))))))))))))))))))))

namespace nuFetch {

using namespace Flexus;
using namespace Core;
using namespace SharedTypes;

static const uint64_t kBadTag = 0xFFFFFFFFFFFFFFFFULL;

typedef flexus_boost_set_assoc<uint64_t, int> SimCacheArray;
typedef SimCacheArray::iterator SimCacheIter;
struct SimCache {
  SimCacheArray theCache;
  int32_t theCacheSize;
  int32_t theCacheAssoc;
  int32_t theCacheBlockShift;
  int32_t theBlockSize;
  std::string theName;

  void init(int32_t aCacheSize, int32_t aCacheAssoc, int32_t aBlockSize, const std::string & aName) {
    theCacheSize = aCacheSize;
    theCacheAssoc = aCacheAssoc;
    theBlockSize = aBlockSize;
    theCacheBlockShift = LOG2(theBlockSize);
    theCache.init( theCacheSize / theBlockSize, theCacheAssoc, 0 );
    theName = aName;
  }

  void loadState(std::string const & aDirName) {
    std::string fname(aDirName);
    DBG_(Dev, ( << "Loading state: " << fname << " for ufetch order L1i cache" ) );
    std::ifstream ifs(fname.c_str());
    if (! ifs.good()) {
      DBG_( Dev, ( << " saved checkpoint state " << fname << " not found.  Resetting to empty cache. " )  );
    } else {
      ifs >> std::skipws;

      if ( ! loadArray( ifs ) ) {
        DBG_ ( Dev, ( << "Error loading checkpoint state from file: " << fname <<
                      ".  Make sure your checkpoints match your current cache configuration." ) );
        DBG_Assert ( false );
      }
      ifs.close();
    }
  }
  bool loadArray( std::istream & s ) {
    static const int32_t kSave_ValidBit = 1;
    int32_t tagShift = LOG2(theCache.sets());

    char paren;
    int32_t dummy;
    int32_t load_state;
    uint64_t load_tag;
    for ( uint32_t i = 0; i < theCache.sets() ; i++ ) {
      s >> paren; // {
      if ( paren != '{' ) {
        DBG_ (Crit, ( << "Expected '{' when loading checkpoint" ) );
        return false;
      }
      for ( uint32_t j = 0; j < theCache.assoc(); j++ ) {
        s >> paren >> load_state >> load_tag >> paren;
        DBG_(Trace,  ( << theName << " Loading block " << std::hex << (((load_tag << tagShift) | i) << theCacheBlockShift) << " with state " << ((load_state & kSave_ValidBit) ? "Shared" : "Invalid" ) << " in way " << j ));
        if (load_state & kSave_ValidBit) {
          theCache.insert( std::make_pair(((load_tag << tagShift) | i), 0) );
          DBG_Assert(theCache.size() <= theCache.assoc());
        }
      }
      s >> paren; // }
      if ( paren != '}' ) {
        DBG_ (Crit, ( << "Expected '}' when loading checkpoint" ) );
        return false;
      }

      // useless associativity information
      s >> paren; // <
      if ( paren != '<' ) {
        DBG_ (Crit, ( << "Expected '<' when loading checkpoint" ) );
        return false;
      }
      for ( uint32_t j = 0; j < theCache.assoc(); j++ ) {
        s >> dummy;
      }
      s >> paren; // >
      if ( paren != '>' ) {
        DBG_ (Crit, ( << "Expected '>' when loading checkpoint" ) );
        return false;
      }
    }
    return true;
  }

  uint64_t insert(uint64_t addr) {
    uint64_t ret_val = 0;
    addr = addr >> theCacheBlockShift;
    SimCacheIter iter = theCache.find(addr);
    if (iter != theCache.end()) {
      theCache.move_back(iter);
      return ret_val;  //already present
    }
    if ((int)theCache.size() >= theCacheAssoc) {
      ret_val = theCache.front_key() << theCacheBlockShift;
      theCache.pop_front();
    }
    theCache.insert( std::make_pair(addr, 0) );
    return ret_val;
  }

  bool lookup(uint64_t addr) {
    addr = addr >> theCacheBlockShift;
    SimCacheIter iter = theCache.find(addr);
    if (iter != theCache.end()) {
      theCache.move_back(iter);
      return true;  // present
    }
    return false; //not present
  }

  bool inval(uint64_t addr) {
    addr = addr >> theCacheBlockShift;
    SimCacheIter iter = theCache.find(addr);
    if (iter != theCache.end()) {
      theCache.erase(iter);
      return true;  // invalidated
    }
    return false;  // not present
  }

};

class FLEXUS_COMPONENT(uFetch) {
  FLEXUS_COMPONENT_IMPL(uFetch);

  std::vector< std::list< FetchAddr > > theFAQ;

  //This opcode is used to signal an ITLB miss to the core, to force
  //a resync with Qemu 
  static const int32_t kITLBMiss = 0UL; //illtrap

  //Statistics on the ICache
  Stat::StatCounter theFetchAccesses;
  Stat::StatCounter theFetches;
  Stat::StatCounter thePrefetches;
  Stat::StatCounter theFailedTranslations;
  Stat::StatCounter theMisses;
  Stat::StatCounter theHits;
  Stat::StatCounter theMissCycles;
  Stat::StatCounter theAllocations;
  Stat::StatMax  theMaxOutstandingEvicts;

  //The I-cache
  SimCache theI;

  //Indicates whether a miss is outstanding, and what the physical address
  //of the miss is
  std::vector< boost::optional< PhysicalMemoryAddress > > theIcacheMiss;
  std::vector< boost::optional< VirtualMemoryAddress > > theIcacheVMiss;
  std::vector< boost::intrusive_ptr<TransactionTracker> > theFetchReplyTransactionTracker;
  std::vector< boost::optional< std::pair<PhysicalMemoryAddress, tFillLevel> > > theLastMiss;

  //Indicates whether a prefetch is outstanding, and what paddr was prefetched
  std::vector< boost::optional< PhysicalMemoryAddress > > theIcachePrefetch;
  //Indicates whether a prefetch is outstanding, and what paddr was prefetched
  std::vector< boost::optional< uint64_t > > theLastPrefetchVTagSet;

  // Set of outstanding evicts.
  std::set< uint64_t > theEvictSet;

  //Cache the last translation to avoid calling Qemu 
  uint64_t theLastVTagSet;
  PhysicalMemoryAddress theLastPhysical;

  uint32_t theIndexShift;
  uint64_t theBlockMask;
  uint32_t theMissQueueSize;

  std::list< MemoryTransport > theMissQueue;
  std::list< MemoryTransport > theSnoopQueue;
  std::list< MemoryTransport > theReplyQueue;

  std::vector<CPUState> theCPUState;

private:
  //I-Cache manipulation functions
  //=================================================================
  /*
      uint64_t index( PhysicalMemoryAddress const & anAddress ) {
        return ( static_cast<uint64_t>(anAddress) >> theIndexShift ) & theIndexMask;
      }

      uint64_t tag( PhysicalMemoryAddress const & anAddress ) {
        return ( static_cast<uint64_t>(anAddress) >> theTagShift );
      }
  */
  bool lookup( PhysicalMemoryAddress const & anAddress ) {
    if (theI.lookup(anAddress)) {
      DBG_( Verb, Comp(*this) ( << "Core[" << std::setfill('0') << std::setw(2) << flexusIndex() << "] I-Lookup hit: " << anAddress ));
      return true;
    }
    DBG_( Verb, Comp(*this) ( << "Core[" << std::setfill('0') << std::setw(2) << flexusIndex() << "] I-Lookup Miss addr: " << anAddress ));
    return false;
  }

  PhysicalMemoryAddress insert( PhysicalMemoryAddress const & anAddress ) {
    PhysicalMemoryAddress ret_val(0);
    if (! lookup(anAddress) ) {
      ++theAllocations;
      ret_val = PhysicalMemoryAddress( theI.insert(anAddress) );
    }
    return ret_val;
  }
  bool inval( PhysicalMemoryAddress const & anAddress ) {
    return theI.inval(anAddress);
  }

public:
  FLEXUS_COMPONENT_CONSTRUCTOR(uFetch)
    : base( FLEXUS_PASS_CONSTRUCTOR_ARGS )
    , theFetchAccesses( statName() + "-FetchAccess" )
    , theFetches( statName() + "-Fetches" )
    , thePrefetches( statName() + "-Prefetches" )
    , theFailedTranslations( statName() + "-FailedTranslations" )
    , theMisses( statName() + "-Misses" )
    , theHits( statName() + "-Hits" )
    , theMissCycles( statName() + "-MissCycles" )
    , theAllocations( statName() + "-Allocations" )
    , theMaxOutstandingEvicts( statName() + "-MaxEvicts" )
    , theLastVTagSet(0)
    , theLastPhysical(0)
  {}

  void initialize() {

    theI.init( cfg.Size, cfg.Associativity, cfg.ICacheLineSize, statName() );
    theIndexShift = LOG2( cfg.ICacheLineSize );
    theBlockMask = ~ (cfg.ICacheLineSize - 1);

    theFAQ.resize(cfg.Threads);
    theIcacheMiss.resize(cfg.Threads);
    theIcacheVMiss.resize(cfg.Threads);
    theFetchReplyTransactionTracker.resize(cfg.Threads);
    theLastMiss.resize(cfg.Threads);
    theIcachePrefetch.resize(cfg.Threads);
    theLastPrefetchVTagSet.resize(cfg.Threads);
    theCPUState.resize(cfg.Threads);

    theMissQueueSize = cfg.MissQueueSize;
  }

  void finalize() {}

  bool isQuiesced() const {
    if (! theMissQueue.empty() || ! theSnoopQueue.empty() || !theReplyQueue.empty()) {
      return false;
    }
    for (uint32_t i = 0; i < cfg.Threads; ++i) {
      if (! theFAQ[i].empty() ) {
        return false;
      }
    }
    return true;
  }

  void saveState(std::string const & aDirName) {
    std::string fname( aDirName);
    fname += "/" + boost::padded_string_cast < 3, '0' > (flexusIndex()) + "-L1i";
    //Not supported
  }

  void loadState(std::string const & aDirName) {
    std::string fname( aDirName);
    if (flexusWidth() == 1) {
      fname += "/sys-L1i";
    } else {
      fname += "/" + boost::padded_string_cast < 2, '0' > (flexusIndex()) + "-L1i";
    }
    theI.loadState(fname);
  }

  //FetchAddressIn
  FLEXUS_PORT_ARRAY_ALWAYS_AVAILABLE(FetchAddressIn);
  void push( interface::FetchAddressIn const &, index_t anIndex, boost::intrusive_ptr<FetchCommand> & aCommand) {
    std::copy
    ( aCommand->theFetches.begin()
      , aCommand->theFetches.end()
      , std::back_inserter( theFAQ[anIndex] )
    );
  }

  //AvailableFAQOut
  FLEXUS_PORT_ARRAY_ALWAYS_AVAILABLE(AvailableFAQOut);
  int32_t pull( interface::AvailableFAQOut const &, index_t anIndex) {
    return cfg.FAQSize - theFAQ[anIndex].size() ;
  }

  //SquashIn
  FLEXUS_PORT_ARRAY_ALWAYS_AVAILABLE(SquashIn);
  void push( interface::SquashIn const &, index_t anIndex, eSquashCause  & aReason) {
    DBG_( Iface, Comp(*this) ( << "CPU[" << std::setfill('0') << std::setw(2) << flexusIndex() << "." << anIndex << "] Fetch SQUASH: " << aReason));
    theFAQ[anIndex].clear();
    theIcacheMiss[anIndex] = boost::none;
    theIcacheVMiss[anIndex] = boost::none;
    theFetchReplyTransactionTracker[anIndex] = nullptr;
    theLastMiss[anIndex] = boost::none;
    theIcachePrefetch[anIndex] = boost::none;
    theLastPrefetchVTagSet[anIndex] = 0;
  }

  //ChangeCPUState
  FLEXUS_PORT_ARRAY_ALWAYS_AVAILABLE(ChangeCPUState);
  void push( interface::ChangeCPUState const &, index_t anIndex, CPUState & aState) {
    DBG_( Iface, Comp(*this) ( << "CPU[" << std::setfill('0') << std::setw(2) << flexusIndex() << "." << anIndex << "] Change CPU State.  TL: " << aState.theTL << " PSTATE: " << std::hex << aState.thePSTATE << std::dec ));
    theCPUState[anIndex] = aState;
  }

  //FetchMissIn
  FLEXUS_PORT_ALWAYS_AVAILABLE(FetchMissIn);
  void push( interface::FetchMissIn const &, MemoryTransport & aTransport ) {
    DBG_( Trace, Comp(*this) ( << "CPU[" << std::setfill('0') << std::setw(2) << flexusIndex() << "] Fetch Miss Reply Received on Port FMI: " << *aTransport[MemoryMessageTag]));
    fetchReply( aTransport );
  }

  FLEXUS_PORT_ARRAY_ALWAYS_AVAILABLE(ICount);
  int32_t pull(ICount const &, index_t anIndex) {
    return theFAQ[anIndex].size();
  }

  FLEXUS_PORT_ARRAY_ALWAYS_AVAILABLE(Stalled);
  bool pull(Stalled const &, index_t anIndex) {
    int32_t available_fiq = 0;
    DBG_Assert( FLEXUS_CHANNEL_ARRAY( AvailableFIQ, anIndex ).available() ) ;
    FLEXUS_CHANNEL_ARRAY( AvailableFIQ, anIndex ) >> available_fiq;
    return theFAQ[anIndex].empty() || available_fiq == 0 || theIcacheMiss[anIndex];
  }

public:
  void drive( interface::uFetchDrive const & ) {
    bool garbage = true;
    FLEXUS_CHANNEL( ClockTickSeen ) << garbage;

    int32_t td = 0;
    if (cfg.Threads > 1) {
      td = nMTManager::MTManager::get()->scheduleFThread(flexusIndex());
    }
    doFetch(td);
    sendMisses();
  }

  Qemu::Processor cpu(index_t anIndex) {
    return Qemu::Processor::getProcessor(flexusIndex() * cfg.Threads + anIndex);
  }

private:
  void prefetchNext(index_t anIndex) {

    // Limit the number of prefetches.  With some backpressure, the number of
    // outstanding prefetches can otherwise be unbounded.
    if ( theMissQueue.size() >= theMissQueueSize )
      return;

    if (cfg.PrefetchEnabled && theLastPrefetchVTagSet[anIndex]) {
      //Prefetch the line following theLastPrefetchVTagSet
      //(if it has a valid translation)

      ++ (*theLastPrefetchVTagSet[anIndex]);
      VirtualMemoryAddress vprefetch = VirtualMemoryAddress( *theLastPrefetchVTagSet[anIndex] << theIndexShift );
      Flexus::Qemu::Translation xlat;
      xlat.theVaddr = vprefetch;
      xlat.theTL = theCPUState[anIndex].theTL;
      xlat.thePSTATE = theCPUState[anIndex].thePSTATE;
      xlat.theType = Flexus::Qemu::Translation::eFetch;
      cpu(anIndex)->translate(xlat, false /* do not trap */ );
      if (! xlat.thePaddr ) {
        //Unable to translate for prefetch
        theLastPrefetchVTagSet[anIndex] = boost::none;
        return;
      } else {
        if (lookup( xlat.thePaddr )) {
          //No need to prefetch, already in cache
          theLastPrefetchVTagSet[anIndex] = boost::none;
        } else {
          theIcachePrefetch[anIndex] = xlat.thePaddr;
          DBG_( Iface, Comp(*this) ( << "CPU[" << std::setfill('0') << std::setw(2) << flexusIndex() << "." << anIndex << "] L1I PREFETCH " << *theIcachePrefetch[anIndex] ));
          issueFetch( xlat.thePaddr , vprefetch);
          ++thePrefetches;
        }
      }
    }
  }

  bool icacheLookup( index_t anIndex, VirtualMemoryAddress vaddr ) {
    //Translate virtual address to physical.
    //First, see if it is our cached translation
    PhysicalMemoryAddress paddr;
    uint64_t tagset = vaddr >> theIndexShift;
    if ( tagset == theLastVTagSet ) {
      paddr = theLastPhysical;
      if (paddr == 0) {
        ++theFailedTranslations;
        return true; //Failed translations are treated as hits - they will cause an ITLB miss in the pipe.
      }
    } else {
      Flexus::Qemu::Translation xlat;
      xlat.theVaddr = vaddr;
      xlat.theTL = theCPUState[anIndex].theTL;
      xlat.thePSTATE = theCPUState[anIndex].thePSTATE;
      xlat.theType = Flexus::Qemu::Translation::eFetch;
      cpu(anIndex)->translate(xlat, false /* do not trap */ );
      paddr = xlat.thePaddr;
      if (paddr == 0) {
        ++theFailedTranslations;
        return true; //Failed translations are treated as hits - they will cause an ITLB miss in the pipe.
      }
      //Cache translation
      theLastPhysical = paddr;
      theLastVTagSet = tagset;
    }

    bool hit = lookup(paddr);
    ++theFetchAccesses;
    if (hit) {
      ++theHits;
      if (theLastPrefetchVTagSet[anIndex] && ( *theLastPrefetchVTagSet[anIndex] == tagset ) ) {
        prefetchNext(anIndex);
      }
      return true;
    }
    ++theMisses;

    //It is an error to get a second miss while one is outstanding
    DBG_Assert( ! theIcacheMiss[anIndex] );
    //Record the miss address, so we know when it comes back
    PhysicalMemoryAddress temp(paddr & theBlockMask);
    theIcacheMiss[anIndex] = temp;
    theIcacheVMiss[anIndex] = vaddr;
    theFetchReplyTransactionTracker[anIndex] = nullptr;

    DBG_( Iface, Comp(*this) ( << "CPU[" << std::setfill('0') << std::setw(2) << flexusIndex() << "." << anIndex << "] L1I MISS " << vaddr << " " << *theIcacheMiss[anIndex]));

    if ( theIcachePrefetch[anIndex] && *theIcacheMiss[anIndex] == *theIcachePrefetch[anIndex] ) {
      theIcachePrefetch[anIndex] = boost::none;
      //We have already sent a prefetch request out for this miss. No need
      //to request again.  However, we can advance the prefetcher to the
      //next miss
      prefetchNext(anIndex);
    } else {
      theIcachePrefetch[anIndex] = boost::none;
      // Need to issue the miss.
      issueFetch(*theIcacheMiss[anIndex], *theIcacheVMiss[anIndex]);

      //Also issue a new prefetch
      theLastPrefetchVTagSet[anIndex] = tagset;
      prefetchNext(anIndex);
    }
    return false;
  }

  void sendMisses() {
    while ( ! theMissQueue.empty() && FLEXUS_CHANNEL(FetchMissOut).available() ) {
      MemoryTransport trans = theMissQueue.front();

      if (!trans[MemoryMessageTag]->isEvictType()) {
        PhysicalMemoryAddress temp(trans[MemoryMessageTag]->address() & theBlockMask);
        if (theEvictSet.find(temp) != theEvictSet.end()) {
          DBG_(Trace, Comp(*this) ( << "Trying to fetch block while evict in process, stalling miss: " << *trans[MemoryMessageTag] ));
          break;
        }
      }

      DBG_( Trace, Comp(*this) ( << "CPU[" << std::setfill('0') << std::setw(2) << flexusIndex() << "] L1I Sending Miss on Port FMO: " << *trans[MemoryMessageTag] ));;

      trans[MemoryMessageTag]->coreIdx()=flexusIndex();
      FLEXUS_CHANNEL(FetchMissOut) << trans;
      theMissQueue.pop_front();
    }

    while ( ! theSnoopQueue.empty() && FLEXUS_CHANNEL(FetchSnoopOut).available() ) {
      MemoryTransport trans = theSnoopQueue.front();
      DBG_( Trace, Comp(*this) ( << "CPU[" << std::setfill('0') << std::setw(2) << flexusIndex() << "] L1I Sending Snoop on Port FSO: " << *trans[MemoryMessageTag] ));;
      trans[MemoryMessageTag]->coreIdx()=flexusIndex();
      FLEXUS_CHANNEL(FetchSnoopOut) << trans;
      theSnoopQueue.pop_front();
    }

    if (cfg.UseReplyChannel) {
      while ( ! theReplyQueue.empty() && FLEXUS_CHANNEL(FetchReplyOut).available() ) {
        MemoryTransport trans = theReplyQueue.front();
        DBG_( Trace, Comp(*this) ( << "CPU[" << std::setfill('0') << std::setw(2) << flexusIndex() << "] L1I Sending Reply on Port FRO: " << *trans[MemoryMessageTag] ));;
        trans[MemoryMessageTag]->coreIdx()=flexusIndex();
        FLEXUS_CHANNEL(FetchReplyOut) << trans;
        theReplyQueue.pop_front();
      }
    }
  }

  void queueSnoopMessage ( MemoryTransport          &         aTransport,
                           MemoryMessage::MemoryMessageType   aType,
                           PhysicalMemoryAddress       &      anAddress,
                           bool                               aContainsData = false ) {
    boost::intrusive_ptr<MemoryMessage> msg = new MemoryMessage ( aType, anAddress );
    msg->reqSize() = cfg.ICacheLineSize;
    if (aType == MemoryMessage::FetchAck) {
      msg->ackRequiresData() = aContainsData;
    }
    aTransport.set ( MemoryMessageTag, msg );
    if (cfg.UseReplyChannel) {
      theReplyQueue.push_back ( aTransport );
    } else {
      theSnoopQueue.push_back ( aTransport );
    }
  }

  void fetchReply( MemoryTransport & aTransport) {
    boost::intrusive_ptr<MemoryMessage> reply = aTransport[MemoryMessageTag];
    boost::intrusive_ptr<TransactionTracker> tracker = aTransport[ TransactionTrackerTag ];

    //The fetch unit better only get load replies
    //DBG_Assert (reply->type() == MemoryMessage::FetchReply);

    switch ( reply->type() ) {
      case MemoryMessage::FwdReply:   // JZ: For new protoocl
      case MemoryMessage::FwdReplyOwned: // JZ: For new protoocl
      case MemoryMessage::MissReply:
      case MemoryMessage::FetchReply:
      case MemoryMessage::MissReplyWritable: {
        //Insert the address into the array
        PhysicalMemoryAddress replacement = insert( reply->address());

        issueEvict( replacement );

        //See if it is our outstanding miss or our outstanding prefetch
        for (uint32_t i = 0; i < cfg.Threads; ++i) {
          if (theIcacheMiss[i] && *theIcacheMiss[i] == reply->address()) {
            DBG_( Iface, Comp(*this) ( << "CPU[" << std::setfill('0') << std::setw(2) << flexusIndex() << "." << i << "] L1I FILL " << reply->address()));
            theIcacheMiss[i] = boost::none;
            theIcacheVMiss[i] = boost::none;
            theFetchReplyTransactionTracker[i] = tracker;
            if (aTransport[TransactionTrackerTag] && aTransport[TransactionTrackerTag]->fillLevel()) {
              theLastMiss[i] = std::make_pair(PhysicalMemoryAddress(reply->address() & theBlockMask), *aTransport[TransactionTrackerTag]->fillLevel());
            } else {
              DBG_(Dev, ( << "Received Fetch Reply with no TransactionTrackerTag" ));
            }
          }

          if (theIcachePrefetch[i] && *theIcachePrefetch[i] == reply->address()) {
            DBG_( Iface, Comp(*this) ( << "CPU[" << std::setfill('0') << std::setw(2) << flexusIndex() << "." << i << "] L1I PREFETCH-FILL " << reply->address()));
            theIcachePrefetch[i] = boost::none;
          }
        }

        // Send an Ack if necessary
        if (cfg.SendAcks && reply->ackRequired()) {
          DBG_Assert( (aTransport[DestinationTag]) );
          aTransport[DestinationTag]->type = DestinationMessage::Directory;

          // We should include data in the ack iff:
          //   we received a reply directly from memory and we want to send a copy to the L2
          //   we received a reply from a peer cache (if the L2 had a copy, it would have replied instead of Fwding the request)
          MemoryMessage::MemoryMessageType ack_type = MemoryMessage::FetchAck;
          bool contains_data = reply->ackRequiresData();
          if (reply->type() == MemoryMessage::FwdReplyOwned) {
            contains_data = true;
            ack_type = MemoryMessage::FetchAckDirty;
          }
          queueSnoopMessage( aTransport, ack_type, reply->address(), contains_data );
        }
        break;
      }

      case MemoryMessage::Invalidate:

        inval( reply->address());
        if ( aTransport[DestinationTag] ) {
          aTransport[DestinationTag]->type = DestinationMessage::Requester;
        }
        queueSnoopMessage ( aTransport, MemoryMessage::InvalidateAck, reply->address() );
        break;

      case MemoryMessage::ReturnReq:
        queueSnoopMessage ( aTransport, MemoryMessage::ReturnReply, reply->address() );
        break;

      case MemoryMessage::Downgrade:
        // We can always reply to this message, regardless of the hit status
        queueSnoopMessage ( aTransport, MemoryMessage::DowngradeAck, reply->address() );
        break;

      case MemoryMessage::ReadFwd:
      case MemoryMessage::FetchFwd:
        // If we have the data, send a FwdReply
        // Otherwise send a FwdNAck
        DBG_Assert( aTransport[DestinationTag] );
        aTransport[TransactionTrackerTag]->setFillLevel(Flexus::SharedTypes::ePeerL1Cache);
        aTransport[TransactionTrackerTag]->setPreviousState(eShared);
        if (lookup(reply->address())) {
          aTransport[DestinationTag]->type = DestinationMessage::Requester;
          queueSnoopMessage( aTransport, MemoryMessage::FwdReply, reply->address());
        } else {
          std::set<uint64_t>::iterator iter = theEvictSet.find(reply->address());
          if (iter != theEvictSet.end()) {
            aTransport[DestinationTag]->type = DestinationMessage::Requester;
            queueSnoopMessage( aTransport, MemoryMessage::FwdReply, reply->address());
          } else {
            aTransport[DestinationTag]->type = DestinationMessage::Directory;
            queueSnoopMessage( aTransport, MemoryMessage::FwdNAck, reply->address());
          }
        }
        break;

      case MemoryMessage::WriteFwd:
        // similar, but invalidate data
        DBG_Assert( aTransport[DestinationTag] );
        aTransport[TransactionTrackerTag]->setFillLevel(Flexus::SharedTypes::ePeerL1Cache);
        aTransport[TransactionTrackerTag]->setPreviousState(eShared);
        if (inval(reply->address())) {
          aTransport[DestinationTag]->type = DestinationMessage::Requester;
          queueSnoopMessage( aTransport, MemoryMessage::FwdReplyWritable, reply->address());
        } else {
          std::set<uint64_t>::iterator iter = theEvictSet.find(reply->address());
          if (iter != theEvictSet.end()) {
            aTransport[DestinationTag]->type = DestinationMessage::Requester;
            queueSnoopMessage( aTransport, MemoryMessage::FwdReplyWritable, reply->address());
          } else {
            aTransport[DestinationTag]->type = DestinationMessage::Directory;
            queueSnoopMessage( aTransport, MemoryMessage::FwdNAck, reply->address());
          }
        }
        break;

      case MemoryMessage::BackInvalidate:
        // Same as invalidate
        aTransport[DestinationTag]->type = DestinationMessage::Directory;
#if 0
        // Only send an Ack if we actually have the block
        // If we don't have the block, then our InvalAck will race with an earlier Evict msg
        // let the Evict msg serve a dual purpose and skip the inval ack.
        if (inval( reply->address())) {
          queueSnoopMessage ( aTransport, MemoryMessage::InvalidateAck, reply->address() );
        } else {
          DBG_(Trace, Comp(*this) ( << "Received BackInvalidate for block not present in cache: " << *reply ));
        }
#endif

        // small protocol change - always send the InvalAck, whether we have the block or not
        inval(reply->address());
        queueSnoopMessage ( aTransport, MemoryMessage::InvalidateAck, reply->address() );
        break;

      case MemoryMessage::EvictAck: {
        std::set<uint64_t>::iterator iter = theEvictSet.find(reply->address());
        DBG_Assert(iter != theEvictSet.end(), Comp(*this) ( << "Block not found in EvictBuffer upon receipt of Ack: " << *reply ));
        theEvictSet.erase(iter);
        break;
      }
      default:
        DBG_Assert ( false, Comp(*this) ( << "Unhandled message received: " << *reply ) );
    }
  }

  void issueEvict( PhysicalMemoryAddress anAddress) {
    if (cfg.CleanEvict && anAddress != 0) {
      MemoryTransport transport;
      boost::intrusive_ptr<MemoryMessage> operation( new MemoryMessage(MemoryMessage::EvictClean, anAddress ) );
      operation->reqSize() = 64;

      // Put in evict buffer and wait for Ack
      std::pair<std::set<uint64_t>::iterator, bool> ret = theEvictSet.insert(anAddress);
      DBG_Assert(ret.second);
      theMaxOutstandingEvicts << theEvictSet.size();
      operation->ackRequired() = true;

      boost::intrusive_ptr<TransactionTracker> tracker = new TransactionTracker;
      tracker->setAddress( anAddress );
      tracker->setInitiator(flexusIndex());
      transport.set(TransactionTrackerTag, tracker);
      transport.set(MemoryMessageTag, operation);

      if (cfg.EvictOnSnoop) {
        theSnoopQueue.push_back ( transport );
      } else {
        theMissQueue.push_back( transport );
      }
    }
  }

  void issueFetch( PhysicalMemoryAddress anAddress, VirtualMemoryAddress vPC) {
    DBG_Assert( anAddress != 0);
    MemoryTransport transport;
    boost::intrusive_ptr<MemoryMessage> operation( MemoryMessage::newFetch(anAddress, vPC));
    operation->reqSize() = 64;

    boost::intrusive_ptr<TransactionTracker> tracker = new TransactionTracker;
    tracker->setAddress( anAddress );
    tracker->setInitiator(flexusIndex());
    tracker->setFetch(true);
    tracker->setSource("uFetch");
    transport.set(TransactionTrackerTag, tracker);
    transport.set(MemoryMessageTag, operation);

    theMissQueue.push_back(transport);
  }

  //Implementation of the FetchDrive drive interface
  void doFetch(index_t anIndex) {

    if (theIcacheMiss[anIndex]) {
      ++theMissCycles;
      return;
    }

    //Determine available FIQ this cycle
    int32_t available_fiq = 0;
    DBG_Assert( FLEXUS_CHANNEL_ARRAY( AvailableFIQ, anIndex ).available() ) ;
    FLEXUS_CHANNEL_ARRAY( AvailableFIQ, anIndex ) >> available_fiq;

    if (available_fiq > 0 && ( theFAQ[anIndex].size() > 1 || theFlexus->quiescing()) ) {
      pFetchBundle bundle(new FetchBundle);

      std::set< VirtualMemoryAddress> available_lines;
      int32_t remaining_fetch = cfg.MaxFetchInstructions;
      if (available_fiq < remaining_fetch) {
        remaining_fetch = available_fiq;
      }

      while ( remaining_fetch > 0 && ( theFAQ[anIndex].size() > 1 || theFlexus->quiescing())) {
        bool from_icache(false);

        FetchAddr fetch_addr = theFAQ[anIndex].front();
        VirtualMemoryAddress block_addr( fetch_addr.theAddress & theBlockMask);

        if ( available_lines.count( block_addr ) == 0) {
          //Line needs to be fetched from I-cache
          if (available_lines.size() >= cfg.MaxFetchLines) {
            //Reached limit of I-cache reads per cycle
            break;
          }

          // Notify the PowerTracker of Icache access
          bool garbage = true;
          FLEXUS_CHANNEL(InstructionFetchSeen) << garbage;

          if ( ! cfg.PerfectICache ) {
            //Do I-cache access here.
            if (! icacheLookup( anIndex, block_addr ) ) {
              break;
            }
            from_icache = true;
          }

          available_lines.insert( block_addr );
        }

        int64_t op_code = fetchFromQemu( anIndex, fetch_addr.theAddress );

        theFAQ[anIndex].pop_front();
        DBG_(Verb, ( << "Fetched " << fetch_addr.theAddress ) );
        bundle->theOpcodes.push_back( FetchedOpcode( fetch_addr.theAddress
                                      , theFAQ[anIndex].empty() ?  VirtualMemoryAddress(0) : theFAQ[anIndex].front().theAddress
                                      , op_code
                                      , fetch_addr.theBPState
                                      , theFetchReplyTransactionTracker[anIndex]
                                                   )
                                    );
        if (from_icache && theLastMiss[anIndex] && theLastPhysical == theLastMiss[anIndex]->first) {
          bundle->theFillLevels.push_back(theLastMiss[anIndex]->second);
          theLastMiss[anIndex] = boost::none;
        } else {
          bundle->theFillLevels.push_back(eL1I);
        }

        ++theFetches;
        if (op_code == kITLBMiss) {
          //stop fetch on an MMU exception
          break;
        }
        --remaining_fetch;
      }
      if (bundle->theOpcodes.size() > 0) {
        FLEXUS_CHANNEL_ARRAY( FetchBundleOut, anIndex ) << bundle;
      }
    }

  }

  int64_t fetchFromQemu(index_t anIndex, VirtualMemoryAddress const & anAddress) {
    int64_t op_code;
    Flexus::Qemu::Translation xlat;
    xlat.theVaddr = anAddress;
    xlat.theTL = theCPUState[anIndex].theTL;
    xlat.thePSTATE = theCPUState[anIndex].thePSTATE;
    xlat.theType = Flexus::Qemu::Translation::eFetch;
    op_code = cpu(anIndex)->fetchInstruction(xlat, false /* do not take traps - the OOO core will do it */ );
    if (xlat.theException == 0) {
      DBG_(Verb, Comp(*this) ( << "Fetch " << anAddress << " op: " << std::hex << std::setw(8) << op_code << std::dec ) );
      return op_code;
    } else {
      DBG_(Iface, Comp(*this) ( << "No translation for " << anAddress << " TL: " << std::hex << theCPUState[anIndex].theTL << " PSTATE: " << theCPUState[anIndex].thePSTATE << " MMU exception: " << xlat.theException << std::dec ) );
      return kITLBMiss /* or other exception - OoO will figure it out */;
    }
  }
};

}//End namespace nuFetch

FLEXUS_COMPONENT_INSTANTIATOR(uFetch, nuFetch);

FLEXUS_PORT_ARRAY_WIDTH( uFetch, FetchAddressIn )           {
  return (cfg.Threads);
}
FLEXUS_PORT_ARRAY_WIDTH( uFetch, SquashIn )      {
  return (cfg.Threads);
}
FLEXUS_PORT_ARRAY_WIDTH( uFetch, ChangeCPUState)   {
  return (cfg.Threads);
}
FLEXUS_PORT_ARRAY_WIDTH( uFetch, AvailableFAQOut )    {
  return (cfg.Threads);
}
FLEXUS_PORT_ARRAY_WIDTH( uFetch, AvailableFIQ )   {
  return (cfg.Threads);
}
FLEXUS_PORT_ARRAY_WIDTH( uFetch, FetchBundleOut)   {
  return (cfg.Threads);
}
FLEXUS_PORT_ARRAY_WIDTH( uFetch, ICount )   {
  return (cfg.Threads);
}
FLEXUS_PORT_ARRAY_WIDTH( uFetch, Stalled )   {
  return (cfg.Threads);
}

#include FLEXUS_END_COMPONENT_IMPLEMENTATION()
#define FLEXUS_END_COMPONENT uFetch

#define DBG_Reset
#include DBG_Control()
