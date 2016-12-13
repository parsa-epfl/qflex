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
#ifndef FLEXUS_FASTCACHE_STANDARD_CACHE_HPP_INCLUDED
#define FLEXUS_FASTCACHE_STANDARD_CACHE_HPP_INCLUDED

#include <components/FastCache/AbstractCache.hpp>

#include <functional>
#include <boost/dynamic_bitset.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <components/CommonQEMU/Util.hpp>

using namespace boost::multi_index;
using nCommonUtil::log_base2;

#include <core/types.hpp>

using Flexus::SharedTypes::PhysicalMemoryAddress;

namespace nFastCache {

class StdCache : public AbstractCache {
private:

  // Structur to mimic traditional tag array (BST like structure)
  struct BlockEntry {
    int64_t  tag;
    CoherenceState_t    state;
    uint16_t  way;

    BlockEntry(uint64_t t, CoherenceState_t s, uint16_t w)
      : tag(t), state(s), way(w) {}
  };

  // Modifier Objects to update BlockEntry objects in multi-index containers
  struct ChangeBlockState {
    ChangeBlockState(CoherenceState_t state) : state(state) {}
    void operator()(BlockEntry & entry) {
      DBG_(Verb, Addr(entry.tag) ( << "Changing state of block " << std::hex << entry.tag << " from " << entry.state << " to " << state << std::dec ));
      entry.state = state;
    }
  private:
    CoherenceState_t state;
  };

  struct ReplaceBlock {
    ReplaceBlock(uint64_t tag, CoherenceState_t state) : tag(tag), state(state) {}
    void operator()(BlockEntry & entry) {
      DBG_(Verb, Addr(entry.tag) ( << "Replacing block " << std::hex << entry.tag << " (" << entry.state << ") with " << tag << " (" << state << ")" << std::dec ));
      entry.state = state;
      entry.tag = tag;
    }
  private:
    uint64_t  tag;
    CoherenceState_t state;
  };

  struct by_order {};
  struct by_tag {};
  struct by_way {};

  struct Int64Hash {
    const std::size_t operator()(int64_t x) const {
      return (std::size_t)(x >> 6);
    }
  };

  typedef multi_index_container
  < BlockEntry
  , indexed_by
  < sequenced < tag<by_order> >
  , hashed_unique
  < tag<by_tag>
  , member<BlockEntry, int64_t, &BlockEntry::tag>
  , Int64Hash
  >
  , hashed_unique
  < tag<by_way>
  , member<BlockEntry, uint16_t, &BlockEntry::way>
  >
  >
  >
  block_set_t;

  typedef block_set_t::index<by_order>::type::iterator    order_iterator;
  typedef block_set_t::index<by_tag>::type::iterator      tag_iterator;
  typedef block_set_t::index<by_way>::type::iterator      way_iterator;

  typedef block_set_t::index<by_way>::type                way_index;

  class StdLookupResult : public LookupResult {
  public:
    PhysicalMemoryAddress theAddress;
    CoherenceState_t      theState;
    block_set_t  *  theSet;
    tag_iterator    theBlock;
    StdCache  *  theCache;

    StdLookupResult(PhysicalMemoryAddress addr, CoherenceState_t state, block_set_t * set, tag_iterator block, StdCache * cache)
      : theAddress(addr), theState(state), theSet(set), theBlock(block), theCache(cache) {
    }

    virtual ~StdLookupResult() {}

    virtual void allocate(CoherenceState_t new_state) {
      theCache->allocate(theAddress, new_state, *this);
    }

    virtual void changeState(CoherenceState_t new_state, bool make_MRU, bool make_LRU) {
      theCache->updateState(*this, new_state, make_MRU, make_LRU);
    }

    virtual void updateLRU() {
      theBlock = theCache->update_lru(theSet, theBlock);
    }

    virtual CoherenceState_t getState() {
      if (theBlock->tag == theAddress) {
        return theBlock->state;
      } else {
        return theState;
      }
    }

    virtual PhysicalMemoryAddress address() {
      return theAddress;
    }
  };

  typedef boost::intrusive_ptr<StdLookupResult> StdLookupResult_p;

  std::string theName;
  int32_t theNumSets;
  int32_t theAssoc;
  int32_t theBlockSize;

  int32_t blockShift;
  int32_t blockSetMask;
  int64_t blockTagMask;

  enum ReplPolicy_t { TRUE_LRU } theReplPolicy;

  evict_function_t evict;
  invalidate_function_t sendInvalidate;
  int32_t theIndex;
  Flexus::SharedTypes::tFillLevel theLevel;

  block_set_t * theBlocks;

  bool theTextFlexpoints;
  bool theAllocateInProgress;
  int64_t theAllocateAddr;

  int32_t get_set(uint64_t addr) {
    return (addr >> blockShift) & blockSetMask;
  }

  int64_t get_tag(uint64_t addr) {
    return (addr & blockTagMask);
  }

  ReplPolicy_t string2ReplPolicy(std::string policy) {
    if (strcasecmp(policy.c_str(), "LRU") == 0) {
      return TRUE_LRU;
    } else {
      DBG_(Crit, ( << "Unknown replacement policy: '" << policy << "', using LRU." ));
      return TRUE_LRU;
    }
  }

public:

  StdCache( const std::string & aName,
            int32_t aBlockSize,
            int32_t aNumSets,
            int32_t anAssociativity,
            evict_function_t anEvict,
            invalidate_function_t aSendInvalidate,
            int32_t anIndex,
            Flexus::SharedTypes::tFillLevel aLevel,
            std::string aReplPolicy,
            bool aTextFlexpoints
          ) {
    theName = aName;
    theNumSets = aNumSets;
    theAssoc = anAssociativity;
    theBlockSize = aBlockSize;
    theTextFlexpoints = aTextFlexpoints;

    theAllocateInProgress = false;

    theBlocks = new block_set_t[theNumSets];

    evict = anEvict;
    sendInvalidate = aSendInvalidate;

    theIndex = anIndex;
    theLevel = aLevel;

    DBG_Assert(theAssoc > 0);
    DBG_Assert(theNumSets > 0);
    DBG_Assert(theBlockSize > 0);
    DBG_Assert( (theNumSets & (theNumSets - 1)) == 0);
    DBG_Assert( (theBlockSize & (theBlockSize - 1)) == 0);

    blockShift = log_base2(theBlockSize);
    blockTagMask = ~(aBlockSize - 1);
    blockSetMask = (theNumSets - 1);

    theReplPolicy = string2ReplPolicy(aReplPolicy);
  }

  virtual ~StdCache() {
    delete[] theBlocks;
  }

  void allocate(PhysicalMemoryAddress addr, CoherenceState_t new_state, StdLookupResult & lookup) {
    int64_t new_tag = get_tag(addr);

    theAllocateInProgress = true;
    theAllocateAddr = new_tag;

    tag_iterator block = lookup.theBlock;
    block_set_t * set = lookup.theSet;

    if (block->tag != new_tag) {
      if (isValid(block->state)) {
        evict(block->tag, block->state);
      }
      set->get<by_tag>().modify(block, ReplaceBlock(new_tag, new_state));
    } else {
      set->get<by_tag>().modify(block, ChangeBlockState(new_state));
    }

    order_iterator tail = set->get<by_order>().end();
    set->get<by_order>().relocate(tail, set->project<by_order>(block));

    theAllocateInProgress = false;
  }

  inline tag_iterator make_block_lru(block_set_t * set, tag_iterator entry) {
    order_iterator cur  = set->project<by_order>(entry);
    order_iterator head = set->get<by_order>().begin();
    set->get<by_order>().relocate(head, cur);
    return set->project<by_tag>(cur);
  }

  tag_iterator update_lru(block_set_t * set, tag_iterator block) {
    order_iterator cur  = set->project<by_order>(block);
    order_iterator tail = set->get<by_order>().end();
    set->get<by_order>().relocate(tail, cur);
    return set->project<by_tag>(cur);
  }

  void updateState(StdLookupResult & lookup, CoherenceState_t new_state, bool make_MRU, bool make_LRU) {

    tag_iterator block = lookup.theBlock;
    block_set_t * set = lookup.theSet;

    set->get<by_tag>().modify(block, ChangeBlockState(new_state));

    if (make_MRU) {
      lookup.theBlock = update_lru(set, block);
    } else if (make_LRU) {
      lookup.theBlock = make_block_lru(set, block);
    }

  }

  virtual LookupResult_p lookup(uint64_t tagset) {
    int64_t tag = get_tag(tagset);
    int32_t set_index = get_set(tagset);

    block_set_t * set = &(theBlocks[set_index]);

    tag_iterator t_block = set->get<by_tag>().find(tag);
    tag_iterator t_end   = set->get<by_tag>().end();

    // if we didn't find the tag we're looking for
    if (t_block == t_end) {
      if (set->size() < (uint32_t)theAssoc) {
        std::pair<order_iterator, bool> ret = set->get<by_order>().push_front(BlockEntry(tag, kInvalid, set->size()));
        t_block = set->project<by_tag>(ret.first);
      } else {
        t_block = set->project<by_tag>(set->get<by_order>().begin());
      }

      return StdLookupResult_p(new StdLookupResult(PhysicalMemoryAddress(tag), kInvalid, set, t_block, this));
    }

    return StdLookupResult_p(new StdLookupResult(PhysicalMemoryAddress(tag), t_block->state, set, t_block, this));
  }

  virtual uint32_t regionProbe( uint64_t tagset) {
    DBG_Assert(false, ( << "regionProbe() function not supported by this Cache structure." ));
    return 0;
  }

  virtual uint32_t blockScoutProbe( uint64_t tagset) {
    DBG_Assert(false, ( << "blockScoutProbe() function not supported by this Cache structure." ));
    return 0;
  }

  virtual uint32_t blockProbe( uint64_t tagset) {
    DBG_Assert(false, ( << "blockProbe() function not supported by this Cache structure." ));
    return 0;
  }

  virtual void setNonSharedRegion( uint64_t tagset) {
    DBG_Assert(false, ( << "setNonSharedRegion() function not supported by this Cache structure." ));
  }

  virtual void setPartialSharedRegion( uint64_t tagset, uint32_t shared) {
    DBG_Assert(false, ( << "setPartialSharedRegion() function not supported by this Cache structure." ));
  }

  virtual bool isNonSharedRegion( uint64_t tagset) {
    DBG_Assert(false, ( << "isNonSharedRegion() function not supported by this Cache structure." ));
    return false;
  }

  int32_t getOwner( LookupResult_p result, uint64_t tagset) {
    DBG_Assert(false, ( << "getOwner() function not supported by this Cache structure." ));
    return -1;
  }

  void updateOwner( LookupResult_p result, int32_t owner, uint64_t tagset, bool shared = true) {
    DBG_Assert(false, ( << "updateOwner() function not supported by this Cache structure." ));
  }

  virtual void getSetTags(uint64_t address, std::list<PhysicalMemoryAddress> &tags) {
    int32_t set_index = get_set(address);

    // First, check if we're in the process of allocating a new block to this set
    // (This happens if this was called as a result of evicting a block during the allocation process)
    if (theAllocateInProgress) {
      int32_t alloc_set = get_set(theAllocateAddr);
      if (alloc_set == set_index) {
        tags.push_back(PhysicalMemoryAddress(theAllocateAddr));
      }
    }

    // Now check all the Valid blocks in the given set
    block_set_t * block_set = &(theBlocks[set_index]);
    tag_iterator block = block_set->get<by_tag>().begin();
    tag_iterator bend   = block_set->get<by_tag>().end();

    for (; block != bend; block++) {
      if (isValid(block->state)) {
        tags.push_back(PhysicalMemoryAddress(block->tag));
      }
    }
  }

  virtual void saveState( std::ostream & s ) {
    static const int32_t kSave_ValidBit = 1;
    static const int32_t kSave_DirtyBit = 2;
    static const int32_t kSave_ModifiableBit = 4;
    static const int32_t kSave_PrefetchedBit = 8;

    if (theTextFlexpoints) {
      int32_t shift = blockShift + log_base2( theNumSets ) ;

      for (int32_t set = 0; set < theNumSets; set++) {
        order_iterator block = theBlocks[set].get<by_order>().begin();
        order_iterator end = theBlocks[set].get<by_order>().end();
        s << "{";
        int32_t way = 0;
        for (; block != end; block++, way++) {
          uint64_t tag = block->tag >> shift;

          int32_t save_state = 0;
          switch (block->state) {
            case kModified:
              save_state = (kSave_ValidBit | kSave_DirtyBit | kSave_ModifiableBit);
              break;
            case kOwned:
              save_state = (kSave_ValidBit | kSave_DirtyBit);
              break;
            case kExclusive:
              save_state = (kSave_ValidBit | kSave_ModifiableBit);
              break;
            case kShared:
              save_state = (kSave_ValidBit);
              break;
            case kInvalid:
              save_state = 0;
              break;
            default:
              DBG_Assert(false, ( << "Don't know how to save state " << block->state ));
              break;
          }

          DBG_(Trace, ( << theName << " - Saving block " << std::hex << block->tag << " with state " << state2String(block->state) << " in way " << way ));

          s << "[ " << save_state
            << " "  << static_cast<uint64_t>(tag)
            << " ]";
        }
        for (; way < theAssoc; way++) {
          s << "[ 0 0 ]";
        }
        s << "} < ";
        for ( int32_t j = 0; j < theAssoc; j++ ) {
          s << j << " ";
        }
        s << "> " << std::endl;
      }
    } else {
      boost::archive::binary_oarchive oa(s);

      uint64_t set_count = theNumSets;
      uint32_t associativity = theAssoc;

      oa << set_count;
      oa << associativity;

      BlockSerializer bs;
      for (int32_t set = 0; set < theNumSets; set++) {
        order_iterator block = theBlocks[set].get<by_order>().begin();
        order_iterator end = theBlocks[set].get<by_order>().end();
        int32_t way = 0;
        for (; block != end; block++, way++) {
          bs.tag = block->tag;
          bs.way = block->way;
          switch (block->state) {
            case kModified:
              bs.state = (uint8_t)'M';
              break;
            case kOwned:
              bs.state = (uint8_t)'O';
              break;
            case kExclusive:
              bs.state = (uint8_t)'E';
              break;
            case kShared:
              bs.state = (uint8_t)'S';
              break;
            case kInvalid:
              bs.state = (uint8_t)'I';
              break;
            default:
              DBG_Assert(false, ( << "Don't know how to save state " << block->state ));
              break;
          }
          oa << bs;
          DBG_(Trace, Addr(block->tag) ( << theName << ": saving block " << std::hex << block->tag << " in state " << (char)bs.state ));
        }
        bs.state = 'I';
        bs.tag = 0;
        for (; way < theAssoc; way++) {
          bs.way = way;
          oa << bs;
        }
      }
    }

  }

  virtual bool loadState( std::istream & s ) {
    static const int32_t kSave_ValidBit = 1;
    static const int32_t kSave_DirtyBit = 2;
    static const int32_t kSave_ModifiableBit = 4;
    static const int32_t kSave_PrefetchedBit = 8;

    if (theTextFlexpoints) {
      int32_t shift = blockShift + log_base2( theNumSets ) ;

      char paren;
      int32_t dummy;
      int32_t load_state;
      uint64_t load_tag;
      for (int32_t set = 0; set < theNumSets; set++) {
        s >> paren; // {
        if ( paren != '{' ) {
          DBG_ (Crit, ( << "Expected '{' when loading checkpoint" ) );
          return false;
        }
        for ( int32_t j = 0; j < theAssoc; j++ ) {
          s >> paren >> load_state >> load_tag >> paren;

          CoherenceState_t state(kInvalid);
          switch (load_state) {
            case (kSave_ValidBit | kSave_DirtyBit | kSave_ModifiableBit):
              state = kModified;
              break;
            case (kSave_ValidBit | kSave_DirtyBit):
              state = kOwned;
              break;
            case (kSave_ValidBit | kSave_ModifiableBit):
              state = kExclusive;
              break;
            case (kSave_ValidBit):
              state = kShared;
              break;
            case 0:
              state = kInvalid;
              break;
            default:
              DBG_Assert(false, ( << "Don't know how to load state " << load_state ));
              break;
          }

          DBG_(Trace, ( << theName << " - Loading block " << std::hex << ((load_tag << shift) | (set << blockShift)) << " with state " << state2String(state) << " in way " << j ));
          theBlocks[set].get<by_order>().push_back(BlockEntry(((load_tag << shift) | (set << blockShift)), state, j));
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
        for ( int32_t j = 0; j < theAssoc; j++ ) {
          s >> dummy;
        }
        s >> paren; // >
        if ( paren != '>' ) {
          DBG_ (Crit, ( << "Expected '>' when loading checkpoint" ) );
          return false;
        }
      }
    } else {
      boost::archive::binary_iarchive ia(s);

      uint64_t set_count = 0;
      uint32_t associativity = 0;

      ia >> set_count;
      ia >> associativity;

      DBG_Assert( set_count == (uint64_t)theNumSets, ( << "Error loading cache state. Flexpoint contains " << set_count << " sets but simulator configured for " << theNumSets << " sets." ));
      DBG_Assert( associativity == (uint64_t)theAssoc, ( << "Error loading cache state. Flexpoint contains " << associativity << "-way sets but simulator configured for " << theAssoc << "-way sets." ));

      for (int32_t set = 0; set < theNumSets; set++) {
        for (int32_t way = 0; way < theAssoc; way++) {
          BlockSerializer bs;
          ia >> bs;
          CoherenceState_t bstate = kInvalid;
          switch (bs.state) {
            case (uint8_t)'M':
              bstate = kModified;
              break;
            case (uint8_t)'O':
              bstate = kOwned;
              break;
            case (uint8_t)'E':
              bstate = kExclusive;
              break;
            case (uint8_t)'S':
              bstate = kShared;
              break;
            case (uint8_t)'I':
              bstate = kInvalid;
              break;
            default:
              DBG_Assert(false, ( << "Unknown Block State: " << (uint8_t)bs.state));
              break;
          }
          DBG_(Trace, ( << theName << " - Loading block " << std::hex << bs.tag << " in state " << (char)bs.state ));
          theBlocks[set].get<by_order>().push_back(BlockEntry(bs.tag, bstate, way));
        }
      }
    }
    return true;
  }
};

}  // namespace nFastRTCache

#endif /* FLEXUS_FASTCACHE_STANDARD_CACHE_HPP_INCLUDED */
