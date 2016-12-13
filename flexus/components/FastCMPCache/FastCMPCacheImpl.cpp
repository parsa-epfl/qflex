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
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

#include <components/FastCMPCache/FastCMPCache.hpp>

#include <components/FastCMPCache/AbstractProtocol.hpp>
#include <components/FastCMPCache/AbstractDirectory.hpp>
#include <components/FastCMPCache/AbstractCache.hpp>

#include <components/FastCMPCache/DirectoryStats.hpp>
#include <components/FastCMPCache/CacheStats.hpp>
#include <components/FastCMPCache/RTCache.hpp>
#include <components/FastCMPCache/StdCache.hpp>

#include <core/performance/profile.hpp>

#include <boost/intrusive_ptr.hpp>

#include <list>
#include <fstream>

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include <stdlib.h> // for random()

/*
  #define DBG_DefineCategories CMPCache
  #define DBG_SetDefaultOps AddCat(CMPCache) Comp(*this)
  #include DBG_Control()
*/

#define FLEXUS_BEGIN_COMPONENT FastCMPCache
#include FLEXUS_BEGIN_COMPONENT_IMPLEMENTATION()

namespace nFastCMPCache {

using namespace Flexus;
using namespace Flexus::Core;
using namespace Flexus::SharedTypes;
using namespace nFastCMPCache;

class FLEXUS_COMPONENT(FastCMPCache) {
  FLEXUS_COMPONENT_IMPL( FastCMPCache );

  // some bookkeeping vars
  uint32_t    theCMPWidth;       // # cores per CMP chip
  uint32_t    theNumCaches;       // # caches tracked by directory

  PhysicalMemoryAddress theCoherenceUnitMask;

  // the cache and the associated statistics
  DirectoryStats    *    theDirStats;

  AbstractProtocol * theProtocol;

  // For Directory
  AbstractDirectory * theDirectory;

  // For Cache
  AbstractCache * theCache;
  CacheStats  * theCacheStats;

  uint64_t theBlockMask;

  MemoryMessage theEvictMessage;
  MemoryMessage theInvalidateMessage;

  LookupResult_p lookup;
  LookupResult_p snp_lookup;

  int32_t theIndex;

  bool theAlwaysMulticast;

  std::list<std::function<void(void)> > theScheduledActions;

  inline MemoryMessage::MemoryMessageType combineSnoopResponse( MemoryMessage::MemoryMessageType & a, MemoryMessage::MemoryMessageType & b) const {

    if (a == MemoryMessage::NoRequest) {
      return b;
    } else if (b == MemoryMessage::NoRequest) {
      return a;
    } else if (a == MemoryMessage::InvUpdateAck || b == MemoryMessage::InvUpdateAck) {
      return MemoryMessage::InvUpdateAck;
    } else if (a == MemoryMessage::InvalidateAck || b == MemoryMessage::InvalidateAck) {
      return MemoryMessage::InvalidateAck;
    } else if (a == MemoryMessage::Invalidate || b == MemoryMessage::Invalidate) {
      return MemoryMessage::Invalidate;
    } else if (a == MemoryMessage::ReturnReplyDirty || b == MemoryMessage::ReturnReplyDirty) {
      return MemoryMessage::ReturnReplyDirty;
    } else if (a == MemoryMessage::ReturnReply || b == MemoryMessage::ReturnReply) {
      return MemoryMessage::ReturnReply;
    } else if (a == MemoryMessage::ReturnNAck && b == MemoryMessage::ReturnNAck) {
      return MemoryMessage::ReturnNAck;
    } else {
      DBG_Assert( false, ( << "Unknown message types to combine '" << a << "' and '" << b << "'" ));
      return a;
    }
  }

  inline MemoryMessage::MemoryMessageType calculateResponse( const MemoryMessage::MemoryMessageType & a, const MemoryMessage::MemoryMessageType & b) const {
    return b;
  }

public:
  FLEXUS_COMPONENT_CONSTRUCTOR(FastCMPCache)
    : base( FLEXUS_PASS_CONSTRUCTOR_ARGS )
    , theEvictMessage(MemoryMessage::EvictDirty)
    , theInvalidateMessage(MemoryMessage::Invalidate)
  { }

  //InstructionOutputPort
  //=====================
  bool isQuiesced() const {

    return true;
  }

  void finalize( void ) {
    // Flush any pending actions
    performDelayedActions();

    theDirStats->update();
    theCacheStats->update();
    theDirectory->finalize();
  }

  void sendRegionProbe(RegionScoutMessage & msg, int32_t index) {
    FLEXUS_CHANNEL_ARRAY(RegionProbe, index) << msg;
  }

  void scheduleDelayedAction(std::function<void(void)> fn) {
    theScheduledActions.push_back(fn);
  }

  void performDelayedActions() {
    while (!theScheduledActions.empty()) {
      theScheduledActions.front()();
      theScheduledActions.pop_front();
    }
  }

  void invalidateBlock(PhysicalMemoryAddress address, SharingVector sharers) {
    std::list<int> slist(sharers.toList());

    MemoryMessage msg(MemoryMessage::Invalidate, address);
    while (!slist.empty()) {
      DBG_(Iface, Addr(address) Comp(*this) ( << "Sending Invalidate to core " << slist.front() << " for block " << std::hex << address ));
      msg.type() = MemoryMessage::Invalidate;

      if (cfg.SeparateID) {
        int32_t core = slist.front() >> 1;
        if (slist.front() & 1) {
          FLEXUS_CHANNEL_ARRAY(SnoopOutI, core) << msg;
        } else {
          FLEXUS_CHANNEL_ARRAY(SnoopOutD, core) << msg;
        }
      } else {
        FLEXUS_CHANNEL_ARRAY(SnoopOutI, slist.front()) << msg;
        FLEXUS_CHANNEL_ARRAY(SnoopOutD, slist.front()) << msg;
      }
      slist.pop_front();
    }

  }

  // Not sure if we need this function any more
  bool sendInvalidate(uint64_t addr, bool icache, bool dcache) {
    bool was_dirty = false;
    theInvalidateMessage.address() = PhysicalMemoryAddress(addr);

    if (icache) {
      theInvalidateMessage.type() = MemoryMessage::Invalidate;
      DBG_(Iface, Addr(addr) Comp(*this) ( << "Sending ICache Invalidate: " << std::hex << addr << std::dec ));
      FLEXUS_CHANNEL( SnoopOutI) << theInvalidateMessage;
      // We can cached a dirty copy in the ICache too!!
      was_dirty = (theInvalidateMessage.type() == MemoryMessage::InvUpdateAck);
    }

    if (dcache) {
      theInvalidateMessage.type() = MemoryMessage::Invalidate;
      DBG_(Iface, Addr(addr) Comp(*this) ( << "Sending DCache Invalidate: " << std::hex << addr << std::dec ));
      FLEXUS_CHANNEL( SnoopOutD ) << theInvalidateMessage;
    }

    return (was_dirty || (theInvalidateMessage.type() == MemoryMessage::InvUpdateAck));
  }

  void evict(uint64_t aTagset, CoherenceState_t aLineState) {

    theEvictMessage.address() = PhysicalMemoryAddress(aTagset);

    theEvictMessage.type() = theProtocol->evict(aTagset, aLineState);

   // if (theEvictMessage.type() == MemoryMessage::EvictDirty || cfg.CleanEvictions)
     {
      DBG_( Iface, Addr(aTagset) Comp(*this) ( << "Evict: " << theEvictMessage ));
      FLEXUS_CHANNEL( RequestOut ) << theEvictMessage;
    }
  }

  void evictRegion(uint64_t aTagset, int32_t owner) {
    RegionScoutMessage message(RegionScoutMessage::eRegionEvict, PhysicalMemoryAddress(aTagset));
    message.setOwner(owner);

    FLEXUS_CHANNEL( RegionNotifyOut ) << message;
  }

  void initialize(void) {
    theCMPWidth = (cfg.CMPWidth ? cfg.CMPWidth : Flexus::Core::ComponentManager::getComponentManager().systemWidth());

    static volatile bool widthPrintout = true;
    if (widthPrintout) {
      DBG_( Crit, ( << "Running with CMP width " << theCMPWidth ) );
      widthPrintout = false;
    }
    if (((theCMPWidth - 1) & theCMPWidth) != 0) {
      DBG_( Crit, ( << "CMP width is NOT a power of 2, some stats will be invalid!"));
    }

    DBG_Assert ( theCMPWidth <= MAX_NUM_SHARERS, ( << "This implementation only supports up to " << MAX_NUM_SHARERS << " nodes" ) );

    theDirStats = new DirectoryStats(statName());
    theCacheStats = new CacheStats(statName());

    Stat::getStatManager()->addFinalizer([this](){this->finalize();});

    std::string empty_string;

    theNumCaches = theCMPWidth;

    if (cfg.SeparateID) {
      theNumCaches *= 2;
    }

    theDirectory = CREATE_DIRECTORY(cfg.DirectoryType);
    theDirectory->setNumCores(theCMPWidth);
    theDirectory->setNumCaches(theCMPWidth);
    theDirectory->setBlockSize(cfg.CoherenceUnit);
    theDirectory->setPortOperations( [this](RegionScoutMessage & msg, int32_t index){this->sendRegionProbe(msg,index);},
                                     [this](std::function<void(void)> fn){this->scheduleDelayedAction(fn);} );
    theDirectory->setInvalidateAction( [this](PhysicalMemoryAddress address, SharingVector sharers){this->invalidateBlock(address, sharers);} );
    theDirectory->initialize(statName());

    theProtocol = CREATE_PROTOCOL(cfg.Protocol);

    theCoherenceUnitMask = PhysicalMemoryAddress(~(cfg.CoherenceUnit - 1));

    DBG_( Dev, ( << "Coherence Unit is: " << cfg.CoherenceUnit ));
    DBG_( Dev, ( << "sizeof(PhysicalMemoryAddress) = " << (int)sizeof(PhysicalMemoryAddress) ));

    theAlwaysMulticast = cfg.AlwaysMulticast;

    //Confirm that BlockSize is a power of 2
    DBG_Assert( (cfg.BlockSize & (cfg.BlockSize - 1)) == 0);
    DBG_Assert( cfg.BlockSize  >= 4);

    int32_t num_sets = cfg.Size / cfg.BlockSize / cfg.Associativity;

    //Confirm that num_sets is a power of 2
    DBG_Assert( (num_sets & (num_sets  - 1)) == 0);

    //Confirm that settings are consistent
    DBG_Assert( cfg.BlockSize * num_sets * cfg.Associativity == cfg.Size);

    //Calculate shifts and masks
    theBlockMask = ~(cfg.BlockSize - 1);

    theIndex = flexusIndex();

    if (cfg.StdArray) {
      theCache = new StdCache(statName(),
                              cfg.BlockSize,
                              num_sets,
                              cfg.Associativity,
                              [this](uint64_t aTagset, CoherenceState_t aLineState){ return this->evict(aTagset, aLineState);},
                              [this](uint64_t addr, bool icache, bool dcache){ return this->sendInvalidate(addr, icache, dcache);},
                              theIndex,
                              cfg.CacheLevel,
                              cfg.ReplPolicy
                             );
    } else {
      theCache = new RTCache( statName(),
                              cfg.BlockSize,
                              num_sets,
                              cfg.Associativity,
                              [this](uint64_t aTagset, CoherenceState_t aLineState){ return this->evict(aTagset, aLineState);},
                              [this](uint64_t aTagset, int32_t owner){ return this->evictRegion(aTagset, owner);},
                              [this](uint64_t addr, bool icache, bool dcache){ return this->sendInvalidate(addr, icache, dcache);},
                              theIndex,
                              cfg.CacheLevel,
                              cfg.RegionSize,
                              cfg.RTAssoc,
                              cfg.RTSize,
                              cfg.ERBSize,
                              false,
                              cfg.ReplPolicy
                            );
    }

    Flexus::Stat::getStatManager()->addFinalizer( [this](){ return this->theCacheStats->update(); } );

  }

  ////////////////////// from ICache
  bool available( interface::FetchRequestIn const &,
                  index_t anIndex ) {
    return true;
  }

  void push( interface::FetchRequestIn const &,
             index_t         anIndex,
             MemoryMessage & aMessage ) {
    DBG_( Iface, Addr(aMessage.address()) ( << " Received on Port FetchRequestIn[" << anIndex << "] Request: " << aMessage ));
    aMessage.dstream() = false;
    if (cfg.SeparateID) {
      anIndex = (anIndex << 1) | 1;
    }
    processRequest( anIndex, aMessage );
  }

  ////////////////////// from DCache
  bool available( interface::RequestIn const &,
                  index_t anIndex) {
    return true;
  }

  void push( interface::RequestIn const &,
             index_t         anIndex,
             MemoryMessage & aMessage ) {
    DBG_( Iface, Comp(*this) Addr(aMessage.address()) ( << "Received on Port RequestIn[" << anIndex << "] Request: " << aMessage ));
    aMessage.dstream() = true;

    if (cfg.SeparateID) {
      anIndex <<= 1;
    }
    processRequest( anIndex, aMessage );
  }

  ////////////////////// snoop port
  FLEXUS_PORT_ALWAYS_AVAILABLE(SnoopIn);
  void push( interface::SnoopIn const &,
             MemoryMessage & aMessage) {
    DBG_( Iface, Comp(*this) Addr(aMessage.address()) ( << "Received on Port SnoopIn: " << aMessage ));

    //DBG_Assert(false, ( << "AHHH someone tried to send us a snoop message! What were they thinking?!?" ));
    PhysicalMemoryAddress addr(aMessage.address() & theCoherenceUnitMask);

    performDelayedActions();

    // First we do a directory lookup
    // This gives us the following things:
    //   1. vector of sharers
    //   2. sharing state
    //   3. list of messages that were sent during the lookup
    //   4. location at which the remaining logic is performed
    //   5. list of "secondary actions", mostly these are for fixed size directories where we might need to evict things to allocate a new directory entry.
    //  6. a pointer to a directory entry so we can easily update it without doing more directory lookups

    SharingVector sharers;
    SharingState state;
    AbstractEntry_p dir_entry;
    std::list<std::function<void(void)> > extra_actions;
    bool dir_valid;

    // Perform directory lookup
    std::tie(sharers, state, dir_entry, dir_valid) = theDirectory->snoopLookup(0, addr, aMessage.type());

    // Perform cache lookup
    lookup = theCache->lookup(addr);

    DBG_(Trace, Comp(*this) Addr(aMessage.address()) ( << "Received SNOOP " << aMessage.type() << " for 0x" << std::hex << addr << std::dec << ", state = " << SharingStatePrinter(state) << ", sharers = " << sharers << ", cache state = " << state2String(lookup->getState()) ));

    if (aMessage.type() == MemoryMessage::Downgrade) {
      // We can safely keep an Exclusive State, and Modified should transition to Excl
      if (lookup->getState() == kModified) {
        lookup->changeState(kExclusive, false, false);
      }
      if (sharers.countSharers() > 1) {
        return;
      }
    } else {
      DBG_Assert(aMessage.type() == MemoryMessage::Invalidate);
      if (lookup->getState() != kInvalid) {
        lookup->changeState(kInvalid, false, false);
      }
    }

    if (!dir_valid) {
      return;
    }

    MemoryMessage snoop_msg(aMessage);
    std::list<int> snoop_list = sharers.toList();
    std::list<int>::iterator index_iter = snoop_list.begin();
    for (; index_iter != snoop_list.end(); index_iter++) {
      int32_t snoop_core = *index_iter;
      if (cfg.SeparateID) {
        snoop_core = (*index_iter) >> 1;
      }
      if (cfg.SeparateID) {
        snoop_msg.type() = aMessage.type();
        if ((*index_iter) & 1) {
          FLEXUS_CHANNEL_ARRAY(SnoopOutI, snoop_core) << snoop_msg;
          DBG_(Iface, Comp(*this) Addr(aMessage.address()) ( << "Sending " << aMessage.type() << " to "
               << snoop_core << "-I after recieving " << aMessage.type() << " from DMA reponse is " << snoop_msg.type() ) );
        } else {
          FLEXUS_CHANNEL_ARRAY(SnoopOutD, snoop_core) << snoop_msg;
          DBG_(Iface, Comp(*this) Addr(aMessage.address()) ( << "Sending " << aMessage.type() << " to "
               << snoop_core << "-D after recieving " << aMessage.type() << " from DMA reponse is " << snoop_msg.type() ) );
        }
      } else {
        snoop_msg.type() = aMessage.type();
        FLEXUS_CHANNEL_ARRAY(SnoopOutD, *index_iter) << snoop_msg;
        DBG_(Iface, Comp(*this) Addr(aMessage.address()) ( << "Sending " << aMessage.type() << " to "
             << (*index_iter) << "-D after recieving " << aMessage.type() << " from DMA reponse is " << snoop_msg.type() ) );
      }

      // Update directory contents
      theDirectory->processSnoopResponse(*index_iter, snoop_msg.type(), dir_entry, addr);
    }
  }

  ////////////////////// from DCache
  bool available( interface::RegionNotify const &,
                  index_t anIndex) {
    return true;
  }

  void push( interface::RegionNotify const &,
             index_t         anIndex,
             RegionScoutMessage & aMessage ) {
    DBG_( Iface, Addr(aMessage.region()) ( << "Received on Port RegionNotify[" << anIndex << "] Request: " << aMessage ));

    // Need to re-think this
    //DBG_Assert(false);
    // For now region notify events go to the directory
    // there might be cause to send them to the cache in the future
    theDirectory->regionNotify(anIndex, aMessage);
  }

  void drive( interface::UpdateStatsDrive const &) {
    theDirStats->update();
    theCacheStats->update();
  }

private:
  /////////////////////////////////////////////////////////////////////////////////////
  // Helper functions

  /////////////////////////////////////////////////////////////////////////////////////
  ////// process incoming requests
  void processRequest( const index_t   anIndex,
                       MemoryMessage & aMessage ) {
    FLEXUS_PROFILE();
    int32_t potential_sharers = 0;
    int32_t unnecessary_snoops_sent = 0;
    int32_t extra_snoops_sent = 0;
    bool accessed_memory = false;
    PhysicalMemoryAddress addr(aMessage.address() & theCoherenceUnitMask);

    performDelayedActions();

    // First we do a directory lookup
    // This gives us the following things:
    //   1. vector of sharers
    //   2. sharing state
    //   3. list of messages that were sent during the lookup
    //   4. location at which the remaining logic is performed
    //   5. list of "secondary actions", mostly these are for fixed size directories where we might need to evict things to allocate a new directory entry.
    //  6. a pointer to a directory entry so we can easily update it without doing more directory lookups

    SharingVector sharers;
    SharingState state;
    AbstractEntry_p dir_entry;
    std::list<std::function<void(void)> > extra_actions;

    // Perform directory lookup
    std::tie(sharers, state, dir_entry) = theDirectory->lookup(anIndex, addr, aMessage.type(), extra_actions);

    // TODO: check if we need to remap cur_location

    // Perform cache lookup
    lookup = theCache->lookup(addr);

    // Next, we use the sharing state and the request type to determine the actions based on some coherence protocol
    // The actions consist of the following information:
    //   - Type of snoops to send: Invalidate, ReturnReq, None
    //   - Bit vector of potential snoop destinations
    //   - Whether snoop should be multicast to all destinations, or sent serially
    //   - If snoops are sent serially, what response(s) terminates the snoop process
    //   - What kind of Fill to send, (normal, writable, dirty)
    //   - is this a valid state & request combination
    DBG_(Trace, Comp(*this) Addr(aMessage.address()) ( << "Received " << aMessage.type() << " for 0x" << std::hex << addr << std::dec << " from " << anIndex << ", state = " << SharingStatePrinter(state) << ", sharers = " << sharers << ", cache state = " << state2String(lookup->getState()) ));

    const PrimaryAction & action = theProtocol->getAction(lookup->getState(), state, aMessage.type(), aMessage.address());

    DBG_Assert( !action.poison , ( << "POISON: c_state = " << state2String(lookup->getState()) << ", d_state = " << SharingStatePrinter(state) << ", request = " << aMessage ) );

    DBG_(Iface, Comp(*this) Addr(aMessage.address()) ( << "Snoop is " << action.snoop_type << ", Multicast " << action.multicast ));

    // If snoops are required, this is where we do them
    MemoryMessage::MemoryMessageType response = MemoryMessage::NoRequest;
    MemoryMessage::MemoryMessageType our_response = action.response[0];
    bool snoop_success = false;
    bool use_ns1 = false; // By default, use next state 0.
    if (action.snoop_type != MemoryMessage::NoRequest) {
      MemoryMessage snoop_msg(aMessage);

      // We want to turn the vector of sharers into an ordered list that determines the order of the snoops
      // In simple cases, the order is determined solely by the topology (ie. snoop closest nodes first)
      // In more complex cases, the Directory may want to control the order as well
      // (ie. Local node knows of some sharers, if they miss the main directory provides others
      // Since the Directory has a pointer to the Topology,
      // We provide flexibility by always letting the directory order the snoops

      std::list<int> snoop_list = sharers.toList();

      bool multiple_snoops = snoop_list.size() > 1;

      potential_sharers = snoop_list.size();
      // Send snoops to all the nodes in the list we just made
      // We also track the messages we send, and use responses to update the directory information
      // If we receive the terminating condition, then stop snooping
      std::list<int>::iterator index_iter = snoop_list.begin();
      for (; index_iter != snoop_list.end(); index_iter++) {
        DBG_(Iface, Comp(*this) Addr(aMessage.address()) ( << "Snoop list contains " << (*index_iter) ));
        if (snoop_list.size() == 2 && ((uint32_t)*index_iter == anIndex)) {
          multiple_snoops = false;
        }
      }
      index_iter = snoop_list.begin();

      bool found_terminal = false;

      for (; index_iter != snoop_list.end(); index_iter++) {
        // Skip the requesting node
        if ((uint32_t)*index_iter == anIndex) {
          DBG_(Iface, Comp(*this) Addr(aMessage.address()) ( << "Skipping snoop to " << (*index_iter) ));
          //potential_sharers--;
          continue;
        }

        int32_t snoop_core = *index_iter;
        if (cfg.SeparateID) {
          snoop_core = (*index_iter) >> 1;
        }

        if (cfg.SeparateID) {

          snoop_msg.type() = action.snoop_type;

          if ((*index_iter) & 1) {
            FLEXUS_CHANNEL_ARRAY(SnoopOutI, snoop_core) << snoop_msg;
            DBG_(Iface, Comp(*this) Addr(aMessage.address()) ( << "Sending " << action.snoop_type << " to " << snoop_core << "-I after recieving " << aMessage.type() << " from " << anIndex << " reponse is " << snoop_msg.type() ) );
          } else {
            FLEXUS_CHANNEL_ARRAY(SnoopOutD, snoop_core) << snoop_msg;
            DBG_(Iface, Comp(*this) Addr(aMessage.address()) ( << "Sending " << action.snoop_type << " to " << snoop_core << "-D after recieving " << aMessage.type() << " from " << anIndex << " reponse is " << snoop_msg.type() ) );
          }

          response = combineSnoopResponse(response, snoop_msg.type());

        } else {
          snoop_msg.type() = action.snoop_type;
          FLEXUS_CHANNEL_ARRAY(SnoopOutD, *index_iter) << snoop_msg;
          DBG_(Iface, Comp(*this) Addr(aMessage.address()) ( << "Sending " << action.snoop_type << " to " << (*index_iter) << "-D after recieving " << aMessage.type() << " from " << anIndex << " reponse is " << snoop_msg.type() ) );
          response = combineSnoopResponse(response, snoop_msg.type());
        }

        if (response != action.terminal_response[0] && response == action.terminal_response[1]) {
          our_response = action.response[1];
          use_ns1 = true; // response[1] also corresponds to next state 1.
        }

        theDirectory->processSnoopResponse(*index_iter, snoop_msg.type(), dir_entry, addr);

        if (!action.multicast && !(theAlwaysMulticast && multiple_snoops)) {
          if (response == action.terminal_response[0] || response == action.terminal_response[1]) {
            DBG_(Iface, Comp(*this) Addr(aMessage.address()) ( << "Got terminal response, ending snoop process" ));
            break;
          } else {
            unnecessary_snoops_sent++;
          }
        } else if (snoop_msg.type() != action.terminal_response[0] && snoop_msg.type() != action.terminal_response[1]) {
          unnecessary_snoops_sent++;
        } else if (!action.multicast && theAlwaysMulticast && multiple_snoops &&
                   (snoop_msg.type() == action.terminal_response[0] || snoop_msg.type() == action.terminal_response[1])) {
          if (!found_terminal) {
            found_terminal = true;
          } else {
            extra_snoops_sent++;
          }
        }
      }
      if (response == action.terminal_response[0] || response == action.terminal_response[1]) {
        snoop_success = true;
      }
    }

    // Remember the original message type
    MemoryMessage::MemoryMessageType orig_msg_type = aMessage.type();

    // Next we forward the response to memory
    // If we reached the termination condition when doing snoops, then there's no need to forward to memory
    if (!snoop_success && action.forward_request) {
      accessed_memory = true;
      FLEXUS_CHANNEL(RequestOut) << aMessage;
      response = aMessage.type();
    }

    // Now, we perform any necessary secondary actions
    // This basically assumes we wait until we get a response from the memory before allocating a new directory entry
    while (!extra_actions.empty()) {
      extra_actions.front()();
      extra_actions.pop_front();
    }

    // Now we can caluclate what response we will return
    // This depends on the Snoop response or the response from memory, as well as the fill type specified by the protocol
    aMessage.type() = calculateResponse(response, our_response);

    // Update the directory, passing it the original message type, the source, and the response we're sending
    theDirectory->processRequestResponse(anIndex, orig_msg_type, aMessage.type(), dir_entry, addr, accessed_memory);
    theDirectory->updateLRU(anIndex, dir_entry, addr);

    // Update the Cache
    // First determine the new state
    CoherenceState_t nstate = action.next_state[0];
    //if (aMessage.type() == action.response[1]) {
    if (use_ns1) {
      nstate = action.next_state[1];
    }
    if (action.allocate) {
      lookup->allocate(nstate);
    } else {
      if (nstate != lookup->getState()) {
        DBG_Assert((lookup->getState() != kInvalid), ( << statName() << "- trying the change state from Invalid to " << state2String(nstate) << " without allocating it." ));
        lookup->changeState(nstate, action.update, false);
      } else if (action.update && lookup->getState() != kInvalid) {
        lookup->updateLRU();
      }
    }

    (theCacheStats->*action.stat)++;

    if (accessed_memory) {
      switch (orig_msg_type) {
        case MemoryMessage::ReadReq:
          theDirStats->theReadsOffChip++;
          theDirStats->theReadsOffChip_PSharers << std::make_pair(potential_sharers, 1);
          break;
        case MemoryMessage::WriteReq:
          theDirStats->theWritesOffChip++;
          theDirStats->theWritesOffChip_PSharers << std::make_pair(potential_sharers, 1);
          break;
        case MemoryMessage::FetchReq:
          theDirStats->theFetchesOffChip++;
          theDirStats->theFetchesOffChip_PSharers << std::make_pair(potential_sharers, 1);
          break;
        default:
          break;
      }
    } else {
      switch (orig_msg_type) {
        case MemoryMessage::ReadReq:
          theDirStats->theReadsOnChip++;
          theDirStats->theReadsOnChip_PSharers << std::make_pair(potential_sharers, 1);
          theDirStats->theReadsOnChip_FSharers << std::make_pair(unnecessary_snoops_sent, 1);
          theDirStats->theReadsOnChip_ASharers << std::make_pair(extra_snoops_sent, 1);
          break;
        case MemoryMessage::WriteReq:
          theDirStats->theWritesOnChip++;
          theDirStats->theWritesOnChip_PSharers << std::make_pair(potential_sharers, 1);
          theDirStats->theWritesOnChip_FSharers << std::make_pair(unnecessary_snoops_sent, 1);
          theDirStats->theWritesOnChip_ASharers << std::make_pair(extra_snoops_sent, 1);
          break;
        case MemoryMessage::FetchReq:
          theDirStats->theFetchesOnChip++;
          theDirStats->theFetchesOnChip_PSharers << std::make_pair(potential_sharers, 1);
          theDirStats->theFetchesOnChip_FSharers << std::make_pair(unnecessary_snoops_sent, 1);
          theDirStats->theFetchesOnChip_ASharers << std::make_pair(extra_snoops_sent, 1);
          break;
        case MemoryMessage::UpgradeReq:
          theDirStats->theUpgradesOnChip++;
          theDirStats->theUpgradesOnChip_PSharers << std::make_pair(potential_sharers, 1);
          theDirStats->theUpgradesOnChip_FSharers << std::make_pair(unnecessary_snoops_sent, 1);
          theDirStats->theUpgradesOnChip_ASharers << std::make_pair(extra_snoops_sent, 1);
          break;
        default:
          break;
      }
    }

//Classification of hits and misses
   if (accessed_memory) { //requests that have to be satisfied from memory => L2 misses
      switch (orig_msg_type) {
        case MemoryMessage::ReadReq:
        	theCacheStats->Misses_Offchip_Read++;
            break;
        case MemoryMessage::FetchReq:
        	theCacheStats->Misses_Offchip_Fetch++;
        	break;
        case MemoryMessage::WriteReq:
        	theCacheStats->Misses_Offchip_Write++;
           break;
        case MemoryMessage::NonAllocatingStoreReq:
        	theCacheStats->Misses_Offchip_NAStore++;
            break;
        default:
            DBG_Assert( false, ( << "Illegal message type " << orig_msg_type));
            break;
      }
    }

   if (snoop_success) { // request satisfied from another L1 cache, because it wasn't in L2, or it wasn't in appropriate state in L2 (coherence miss)
         switch (orig_msg_type) {
 	        case MemoryMessage::ReadReq:
            	theCacheStats->Misses_Onchip_Read++;
               	break;
            case MemoryMessage::FetchReq:
            	theCacheStats->Misses_Onchip_Fetch++;
            	break;
            case MemoryMessage::WriteReq:
            	theCacheStats->Misses_Onchip_Write++;
            	break;
            case MemoryMessage::NonAllocatingStoreReq:
            	theCacheStats->Misses_Onchip_NAStore++;
                break;
	        case MemoryMessage::UpgradeReq:
	        	break;  //do nothing!
	        default:
	        	DBG_Assert( false, ( << "Illegal message type " << orig_msg_type));
	        	break;
         }
    }

   if(!snoop_success && !accessed_memory){ //request satisfied in L2 => L2 hit
          switch (orig_msg_type) {
            case MemoryMessage::ReadReq:
            	theCacheStats->Hits_Read++;
               	break;
            case MemoryMessage::FetchReq:
            	theCacheStats->Hits_Fetch++;
            	break;
            case MemoryMessage::WriteReq:
            	theCacheStats->Hits_Write++;
            	break;
            case MemoryMessage::NonAllocatingStoreReq:
            	theCacheStats->Hits_NAStore++;
                break;
            case MemoryMessage::EvictClean:
            case MemoryMessage::EvictDirty:
            case MemoryMessage::EvictWritable:
            case MemoryMessage::UpgradeReq:
            	break;  //do nothing!
            default:
            	DBG_Assert( false, ( << "Illegal message type " << orig_msg_type));
            	break;
	 }
    }
  }

  void saveState(std::string const & aDirName) {
    std::string fname( aDirName );
    fname += "/" + statName() + "-dir.gz";
    std::ofstream ofs(fname.c_str(), std::ios::binary);

    boost::iostreams::filtering_stream<boost::iostreams::output> out;
    out.push(boost::iostreams::gzip_compressor(boost::iostreams::gzip_params(9)));
    out.push(ofs);

    theDirectory->saveState ( out , aDirName );

    fname = aDirName;
    fname += "/" + statName() + "-cache.gz";
    std::ofstream c_ofs(fname.c_str(), std::ios::binary);

    boost::iostreams::filtering_stream<boost::iostreams::output> c_out;
    c_out.push(boost::iostreams::gzip_compressor(boost::iostreams::gzip_params(9)));
    c_out.push(c_ofs);

    theCache->saveState ( c_out );
  }

  void loadState( std::string const & aDirName ) {
    std::string fname( aDirName);
    fname += "/" + statName() + "-dir.gz";
    std::ifstream ifs(fname.c_str(), std::ios::binary);
    if (! ifs.good()) {
      DBG_( Dev, ( << " saved checkpoint state " << fname << " not found.  Resetting to empty cache. " )  );
    } else {
      //ifs >> std::skipws;

      boost::iostreams::filtering_stream<boost::iostreams::input> in;
      in.push(boost::iostreams::gzip_decompressor());
      in.push(ifs);

      if ( ! theDirectory->loadState( in, aDirName ) ) {
        DBG_ ( Dev, ( << "Error loading checkpoint state from file: " << fname <<
                      ".  Make sure your checkpoints match your current cache configuration." ) );
        DBG_Assert ( false );
      }
    }
    DBG_(Dev, ( << " Directory state loaded"));
    std::string c_fname( aDirName);
    c_fname += "/" + statName() + "-cache.gz";
    std::ifstream c_ifs(c_fname.c_str(), std::ios::binary);
    if (! c_ifs.good()) {
      DBG_( Dev, ( << " saved checkpoint state " << c_fname << " not found.  Resetting to empty cache. " )  );
    } else {
      //ifs >> std::skipws;

      boost::iostreams::filtering_stream<boost::iostreams::input> c_in;
      c_in.push(boost::iostreams::gzip_decompressor());
      c_in.push(c_ifs);

      if ( ! theCache->loadState( c_in ) ) {
        DBG_ ( Dev, ( << "Error loading checkpoint state from file: " << c_fname <<
                      ".  Make sure your checkpoints match your current cache configuration." ) );
        DBG_Assert ( false );
      }
    }
    DBG_(Dev, ( << " Cache state loaded"));
  }

};  // end class FastCMPCache

}  // end Namespace nFastCMPCache

FLEXUS_COMPONENT_INSTANTIATOR( FastCMPCache, nFastCMPCache);

FLEXUS_PORT_ARRAY_WIDTH( FastCMPCache, RequestIn)      {
  return (cfg.CMPWidth ? cfg.CMPWidth : Flexus::Core::ComponentManager::getComponentManager().systemWidth());
}
FLEXUS_PORT_ARRAY_WIDTH( FastCMPCache, FetchRequestIn) {
  return (cfg.CMPWidth ? cfg.CMPWidth : Flexus::Core::ComponentManager::getComponentManager().systemWidth());
}
FLEXUS_PORT_ARRAY_WIDTH( FastCMPCache, SnoopOutI)       {
  return (cfg.CMPWidth ? cfg.CMPWidth : Flexus::Core::ComponentManager::getComponentManager().systemWidth());
}
FLEXUS_PORT_ARRAY_WIDTH( FastCMPCache, SnoopOutD)       {
  return (cfg.CMPWidth ? cfg.CMPWidth : Flexus::Core::ComponentManager::getComponentManager().systemWidth());
}
FLEXUS_PORT_ARRAY_WIDTH( FastCMPCache, RegionProbe)    {
  return (cfg.CMPWidth ? cfg.CMPWidth : Flexus::Core::ComponentManager::getComponentManager().systemWidth());
}
FLEXUS_PORT_ARRAY_WIDTH( FastCMPCache, RegionNotify)   {
  return (cfg.CMPWidth ? cfg.CMPWidth : Flexus::Core::ComponentManager::getComponentManager().systemWidth());
}
FLEXUS_PORT_ARRAY_WIDTH( FastCMPCache, RegionNotifyOut)   {
  return (cfg.CMPWidth ? cfg.CMPWidth : Flexus::Core::ComponentManager::getComponentManager().systemWidth());
}

#include FLEXUS_END_COMPONENT_IMPLEMENTATION()
#define FLEXUS_END_COMPONENT FastCMPCache

#define DBG_Reset
#include DBG_Control()
