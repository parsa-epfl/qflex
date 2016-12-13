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
#ifndef FLEXUS_FASTCMPCACHE_RTCACHE_HPP_INCLUDED
#define FLEXUS_FASTCMPCACHE_RTCACHE_HPP_INCLUDED

#include <components/CommonQEMU/TraceTracker.hpp>
#include <components/FastCMPCache/AbstractCache.hpp>

#include <cstring>

#include <functional>
#include <boost/dynamic_bitset.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

using namespace boost::multi_index;

#include <core/types.hpp>

struct RTSerializer {
  uint64_t tag;
  uint8_t  state;
  uint8_t  way;
  int8_t  owner;

  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const uint32_t version) {
    ar & tag;
    ar & state;
    ar & owner;
  }
};
BOOST_CLASS_TRACKING(RTSerializer, boost::serialization::track_never)

#if 0
struct BlockSerializer {
  uint64_t tag;
  uint8_t  way;
  uint8_t  state;

  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const uint32_t version) {
    ar & tag;
    ar & state;
  }
};
BOOST_CLASS_TRACKING(BlockSerializer, boost::serialization::track_never)
#endif

#include <components/CommonQEMU/Serializers.hpp>

using nCommonSerializers::BlockSerializer;

using Flexus::SharedTypes::PhysicalMemoryAddress;

namespace nFastCMPCache {

using evict_function_t = std::function<void( uint64_t tagset, CoherenceState_t state) >;
using region_evict_function_t = std::function<void( uint64_t tagset, int32_t owner ) >;
using invalidate_function_t = std::function<bool( uint64_t tagset, bool icache, bool dcache ) >;

class RTCache : public AbstractCache {
public:

  static const uint64_t kStateMask = 3;

  enum RegionState {
    NON_SHARED_REGION,
    PARTIAL_SHARED_REGION,
    SHARED_REGION
  };

  // RegionTracker (RVA) entry structure
  struct RTEntry {
    RTEntry(int32_t blocksPerRegion) : region_state(SHARED_REGION), owner(-1),
      state(blocksPerRegion, kInvalid), ways(blocksPerRegion, 0), shared(blocksPerRegion) {}
    RTEntry(int32_t blocksPerRegion, uint64_t tag,  int32_t way)
      : tag(tag), way(way), region_state(SHARED_REGION), owner(-1),
        state(blocksPerRegion, kInvalid), ways(blocksPerRegion, 0), shared(blocksPerRegion) {}
    RTEntry(int32_t blocksPerRegion, uint64_t tag,  int32_t way, RegionState state, int32_t owner)
      : tag(tag), way(way), region_state(state), owner(owner),
        state(blocksPerRegion, kInvalid), ways(blocksPerRegion, 0), shared(blocksPerRegion) {}
    RTEntry(const RTEntry & entry)
      : tag(entry.tag), way(entry.way), region_state(entry.region_state), owner(entry.owner), state(entry.state),
        ways(entry.ways), shared(entry.shared) {}

    uint64_t tag;
    //uint64_t set;
    int32_t way;
    RegionState region_state;
    int32_t owner;

    std::vector<CoherenceState_t> state;
    std::vector<uint16_t> ways;
    boost::dynamic_bitset<> shared;

    bool hasCachedBlocks() const {
      for (uint32_t i = 0; i < state.size(); i++) {
        // If we have a valid copy, or if a higher level has it cached
        if (isValid(state[i])) return true;
      }
      return false;
    }

    uint32_t validVector() const {
      uint32_t ret = 0;
      uint32_t mask = 1;
      for (uint32_t i = 0; i < state.size(); i++, mask <<= 1) {
        // If we have a valid copy, or if a higher level has it cached
        if (isValid(state[i])) {
          ret |= mask;
        }
      }
      return ret;
    }
  };

  // Modifier Objects for updating RTEntry objects in multi-index containers

  struct ChangeRTState {
    ChangeRTState(int32_t offset, CoherenceState_t state) : offset(offset), state(state) {}
    void operator()(RTEntry & entry) {
      DBG_(Verb, Addr(entry.tag | (offset << 6)) ( << "Changing state of block " << std::hex << (entry.tag | (offset << 6)) << " from " << entry.state[offset] << " to " << state << std::dec ));
      entry.state[offset] = state;
    }
  private:
    int32_t offset;
    CoherenceState_t state;
  };

  struct InvalidateRTState {
    InvalidateRTState(int32_t offset) : offset(offset) {}
    void operator()(RTEntry & entry) {
      // Only keep the cached bit
      entry.state[offset] = kInvalid;
    }
  private:
    int32_t offset;
  };

  struct ChangeRegionState {
    ChangeRegionState(const RegionState & state) : state(state) {}
    void operator()(RTEntry & entry) {
      entry.region_state = state;
    }
  private:
    RegionState state;
  };

  struct ChangeRegionOwner {
    ChangeRegionOwner(const int32_t & owner) : owner(owner) {}
    void operator()(RTEntry & entry) {
      entry.owner = owner;
    }
  private:
    int32_t owner;
  };

  struct CorrectRTWay {
    CorrectRTWay(int32_t offset, uint16_t way) : offset(offset), way(way) {}
    void operator()(RTEntry & entry) {
      entry.ways[offset] = way;
    }
  private:
    int32_t offset;
    uint16_t way;
  };

  struct AllocateBlock {
    AllocateBlock(int32_t offset, CoherenceState_t state, uint16_t way) : offset(offset), state(state), way(way) {}
    void operator()(RTEntry & entry) {
      entry.state[offset] = state;
      entry.ways[offset] = way;
    }
  private:
    int32_t offset;
    CoherenceState_t state;
    uint16_t way;
  };

  struct ChangeSharedBlocks {
    ChangeSharedBlocks(const uint32_t & shared) : shared(shared) {}
    void operator()(RTEntry & entry) {
      entry.shared = boost::dynamic_bitset<>(entry.state.size(), shared);
    }
  private:
    uint32_t shared;
  };

  struct SetSharedBlock {
    SetSharedBlock(const int32_t & offset) : offset(offset) {}
    void operator()(RTEntry & entry) {
      entry.shared[offset] = true;
    }
  private:
    int32_t offset;
  };

  struct by_order {};
  struct by_tag {};
  struct by_way {};

  struct ULLHash {
    const std::size_t operator()(uint64_t x) const {
      return (std::size_t)x;
    }
  };

  using rt_set_t = multi_index_container
  < RTEntry
  , indexed_by
  < sequenced < tag<by_order> >
  , hashed_unique
  < tag<by_tag>
  , member<RTEntry, uint64_t, &RTEntry::tag>
  , ULLHash
  >
  >
  >
 ;

  using rt_order_iterator = rt_set_t::index<by_order>::type::iterator;
  using rt_iterator = rt_set_t::index<by_tag>::type::iterator;
  using rt_index = rt_set_t::index<by_tag>::type;

  // Structur to mimic traditional tag array (BST like structure)
  struct BlockEntry {
    uint64_t tag;
    CoherenceState_t state;
    uint16_t way;

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

  using block_set_t = multi_index_container
  < BlockEntry
  , indexed_by
  < sequenced < tag<by_order> >
  , hashed_unique
  < tag<by_tag>
  , member<BlockEntry, uint64_t, &BlockEntry::tag>
  , ULLHash
  >
  , hashed_unique
  < tag<by_way>
  , member<BlockEntry, uint16_t, &BlockEntry::way>
  >
  >
  >
 ;

  using order_iterator = block_set_t::index<by_order>::type::iterator;
  using tag_iterator = block_set_t::index<by_tag>::type::iterator;
  using way_iterator = block_set_t::index<by_way>::type::iterator;

  using way_index = block_set_t::index<by_way>::type;

  class RTLookupResult : public LookupResult {
  public:
    RTCache  * rt_cache;
    rt_set_t * rt_set;
    block_set_t * block_set;
    rt_iterator  region;
    way_iterator block;
    int    offset;
    uint64_t tagset;

    bool   valid_result;

    RTLookupResult() : rt_cache(nullptr), valid_result(false) {}
    RTLookupResult(bool valid, RTCache * rtc, uint64_t addr) : rt_cache(rtc), tagset(addr), valid_result(valid) {}

    friend class RTCache;

    virtual void allocate(CoherenceState_t new_state) {
      RTLookupResult ret(rt_cache->allocate(tagset, new_state));
      rt_cache = ret.rt_cache;
      rt_set = ret.rt_set;
      block_set = ret.block_set;
      region = ret.region;
      block = ret.block;
      offset = ret.offset;
      tagset = ret.tagset;
      valid_result = ret.valid_result;

      //*this = rt_cache->allocate(tagset, new_state);
    }

    virtual void changeState(CoherenceState_t new_state, bool make_MRU, bool make_LRU) {
      rt_cache->updateState(*this, new_state, make_MRU, make_LRU);
    }

    virtual void updateLRU() {
      region = rt_cache->update_lru(rt_set, region);
      block = rt_cache->update_lru(block_set, block);
    }

    virtual CoherenceState_t getState() {
      if (!valid_result) {
        return kInvalid;
      } else {
        return block->state;
      }
    }
    virtual PhysicalMemoryAddress address() {
      return PhysicalMemoryAddress(tagset);
    }
  };

  using RTLookupResult_p = boost::intrusive_ptr<RTLookupResult>;

  enum RTReplPolicy_t { SET_LRU, REGION_LRU };

private:
  // Private data

  bool theTraceTrackerFlag;

  std::string theName;

  int32_t theBlockSize;
  int32_t theRegionSize;
  int32_t theNumDataSets;
  int32_t theAssociativity;

  int32_t theNumRTSets;
  int32_t theRTAssociativity;

  int32_t theERBSize;

  bool theCleanEvicts;
  bool theMetaInclusive;
  bool theImpreciseRS;
  bool theBlockScout;
  bool theSkewBlockSetIndex;

  RTReplPolicy_t theReplPolicy;

  evict_function_t evict;
  region_evict_function_t evictRegion;
  invalidate_function_t sendInvalidate;
  int32_t theIndex;
  Flexus::SharedTypes::tFillLevel theLevel;

  block_set_t * theBlocks;
  rt_set_t * theRVA;
  rt_set_t theERB;

  uint64_t rtSetMask;
  uint64_t rtTagMask;
  uint64_t blockSetMask;
  uint64_t blockTagMask;
  uint64_t blockOffsetMask;
  uint64_t rtTopMask;

  int32_t rtTopShift, blockTagShift;
  int32_t regionShift, blockShift;
  int32_t theBlocksPerRegion;

  uint64_t theAllocateAddr;
  bool theAllocateInProgress;

  inline uint64_t get_rt_set(uint64_t addr) {
    return ( (addr >> regionShift) & rtSetMask );
  }

  inline uint64_t get_rt_tag(uint64_t addr) {
    return (addr & rtTagMask);
  }

  inline uint64_t get_block_offset(uint64_t addr) {
    return ( (addr >> blockShift) & blockOffsetMask );
  }

  inline uint64_t get_block_set(uint64_t addr, uint64_t way) {
    if (theSkewBlockSetIndex) {
      uint64_t set_index =  ( ( (addr >> blockShift ) ^
                                ( ( ( ( addr >> blockTagShift ) & rtTopMask ) | ( way << rtTopShift ) ) & blockOffsetMask ) ) & blockSetMask);
      return set_index;
    } else {
      return ( (addr >> blockShift) & blockSetMask );
    }
  }

  inline uint64_t get_block_tag(uint64_t addr) {
    return (addr & blockTagMask);
  }

public:

  RTReplPolicy_t string2ReplPolicy(std::string & policy) {
    if (policy == "RegionLRU") {
      return REGION_LRU;
    } else if (policy == "SetLRU") {
      return SET_LRU;
    } else {
      DBG_(Crit, ( << "Unknown RTCache Replacement Policy '" << policy << "'" ));
    }
    return SET_LRU;
  }

  RTCache( std::string aName,
           int32_t aBlockSize,
           int32_t aNumSets,
           int32_t anAssociativity,
           evict_function_t anEvict,
           region_evict_function_t aRegionEvict,
           invalidate_function_t aSendInvalidate,
           int32_t anIndex,
           Flexus::SharedTypes::tFillLevel aLevel,
           int32_t aRegionSize,
           int32_t anRTAssoc,
           int32_t anRTSize,
           int32_t anERBSize,
           bool aSkewBlockSetIndex,
           std::string & anRTReplPolicy
         )
    : theName(aName) {
    theNumDataSets = aNumSets;
    theAssociativity = anAssociativity;
    theBlockSize = aBlockSize;
    theRegionSize = aRegionSize;

    theAllocateInProgress = false;

    theBlocks = new block_set_t[theNumDataSets];

    evict = anEvict;
    evictRegion = aRegionEvict;
    sendInvalidate = aSendInvalidate;

    theIndex = anIndex;
    theLevel = aLevel;

    DBG_Assert( anRTAssoc > 0 );
    DBG_Assert( aRegionSize >= aBlockSize );
    DBG_Assert( anRTSize > 0 );
    DBG_Assert( anERBSize > 0 );

    theReplPolicy = string2ReplPolicy(anRTReplPolicy);

    theNumRTSets = anRTSize / anRTAssoc;
    theRTAssociativity = anRTAssoc;

    theBlocksPerRegion = aRegionSize / aBlockSize;

    // Make sure everything is a power of 2
    DBG_Assert( ((theNumRTSets - 1) & theNumRTSets) == 0 );
    DBG_Assert( ((aRegionSize - 1) & aRegionSize) == 0 );
    DBG_Assert( ((aBlockSize - 1) & aBlockSize) == 0 );

    theRVA = new rt_set_t[theNumRTSets];

    theERBSize = anERBSize;

    // Calculate shifts and masks

    regionShift = log_base2(aRegionSize);
    rtSetMask = (theNumRTSets - 1);
    rtTagMask = ~(aRegionSize - 1);

    blockShift  = log_base2(aBlockSize);
    blockTagMask = ~(aBlockSize - 1);
    blockSetMask = (theNumDataSets - 1);
    blockOffsetMask = (theBlocksPerRegion - 1);

    theSkewBlockSetIndex = aSkewBlockSetIndex;
    blockTagShift = log_base2(theNumDataSets) + blockShift;
    rtTopShift = log_base2(theNumRTSets) + regionShift - blockTagShift;
    rtTopMask = (1ULL << rtTopShift) - 1;
  }

  bool isCached(CoherenceState_t state) {
    return isValid(state);
  }

  bool isAllocate(CoherenceState_t cur_state, CoherenceState_t new_state) {
    return !isValid(cur_state) && isValid(new_state);
  }

  uint64_t getState(RTLookupResult & result) {
    return (result.valid_result ? result.block->state : kInvalid);
  }

  virtual void updateOwner( LookupResult_p result, int32_t owner, uint64_t tagset, bool shared = true) {
    uint64_t rt_tag = get_rt_tag(tagset);
    DBG_(Verb, ( << theIndex << " - Change ownership of region " << std::hex << rt_tag << " to " << owner ));

    RegionState new_state = (shared ? SHARED_REGION : NON_SHARED_REGION);

    RTLookupResult & my_result = *(dynamic_cast<RTLookupResult *>(result.get()));

    if (!my_result.valid_result || my_result.region->tag != rt_tag) {
      int rt_set_index = get_rt_set(tagset);

      rt_set_t * rt_set = &(theRVA[rt_set_index]);
      rt_index * rt  = &(rt_set->get<by_tag>());
      rt_iterator entry = rt->find(rt_tag);
      rt_iterator end  = rt->end();

      if (entry == end) {
        rt_set  = &theERB;
        rt  = &(theERB.get<by_tag>());
        entry = rt->find(rt_tag);
        end  = rt->end();
      }

      if (entry != end) {
        rt->modify(entry, ChangeRegionOwner(owner));

        if (entry->region_state != new_state) {
          rt->modify(entry, ChangeRegionState(new_state));
        }
      } else {
        // there are rare cases where this can happen
        return;
        DBG_Assert(false, ( << "Tried to set ownership of region " << std::hex << rt_tag << " to " << std::dec << owner << " BUT REGION NOT FOUND IN CACHE." ));
      }
    } else {
      DBG_(Verb, ( << "Changing owner of region " << std::hex << rt_tag << " to " << std::dec << owner ));
      my_result.rt_set->get<by_tag>().modify(my_result.region, ChangeRegionOwner(owner));
      if (my_result.region->region_state != new_state) {
        my_result.rt_set->get<by_tag>().modify(my_result.region, ChangeRegionState(new_state));
      }
    }
  }

  virtual int32_t getOwner( LookupResult_p result, uint64_t tagset) {
    RTLookupResult & my_result = *(dynamic_cast<RTLookupResult *>(result.get()));
    uint64_t rt_tag = get_rt_tag(tagset);
    if (!my_result.valid_result || my_result.region->tag != rt_tag) {
      int rt_set_index = get_rt_set(tagset);

      rt_set_t * rt_set = &(theRVA[rt_set_index]);
      rt_index * rt  = &(rt_set->get<by_tag>());
      rt_iterator entry = rt->find(rt_tag);
      rt_iterator end  = rt->end();

      if (entry == end) {
        rt_set  = &theERB;
        rt  = &(theERB.get<by_tag>());
        entry = rt->find(rt_tag);
        end  = rt->end();
      }

      if (entry != end) {
        return entry->owner;
      } else {
        return -1;
      }
    } else {
      return my_result.region->owner;
    }
  }

  inline rt_iterator update_lru(rt_set_t * rt_set, rt_iterator entry) {
    rt_order_iterator cur = rt_set->project<by_order>(entry);
    rt_order_iterator tail = rt_set->get<by_order>().end();
    rt_set->get<by_order>().relocate(tail, cur);
    return rt_set->project<by_tag>(cur);
  }

  inline way_iterator update_lru(block_set_t * set, way_iterator entry) {
    order_iterator cur = set->project<by_order>(entry);
    order_iterator tail = set->get<by_order>().end();
    set->get<by_order>().relocate(tail, cur);
    return set->project<by_way>(cur);
  }

  inline way_iterator make_block_lru(block_set_t * set, way_iterator entry) {
    order_iterator cur = set->project<by_order>(entry);
    order_iterator head = set->get<by_order>().begin();
    set->get<by_order>().relocate(head, cur);
    return set->project<by_way>(cur);
  }

  inline tag_iterator make_block_lru(block_set_t * set, tag_iterator entry) {
    order_iterator cur = set->project<by_order>(entry);
    order_iterator head = set->get<by_order>().begin();
    set->get<by_order>().relocate(head, cur);
    return set->project<by_tag>(cur);
  }

  way_iterator replace_lru_block(block_set_t * set, uint64_t tag, CoherenceState_t state) {
    order_iterator block = set->get<by_order>().begin();
    DBG_Assert(block != set->get<by_order>().end());
    if (isValid(block->state)) {
      uint64_t rt_tag = get_rt_tag(block->tag);
      uint64_t rt_set_index = get_rt_set(block->tag);
      int32_t offset = get_block_offset(block->tag);

      rt_set_t * rt_set = &(theRVA[rt_set_index]);
      rt_index * rt  = &(rt_set->get<by_tag>());
      rt_iterator entry = rt->find(rt_tag);
      rt_iterator end  = rt->end();

      if (entry == end) {
        rt  = &(theERB.get<by_tag>());
        entry = rt->find(rt_tag);
        end  = rt->end();
        if (entry == end) {
          // Should NEVER get here
          DBG_Assert( false, ( << "Core: " << theIndex << " - Failed to find Region Entry for BST block " << std::hex << block->tag << " in Region " << rt_tag << " block state = " << block->state ));
        }
      }
      rt->modify(entry, InvalidateRTState(offset));

      // Do the eviction
      DBG_(Verb, ( << "evicting tail block"));
      DBG_(Iface, Addr(block->tag) ( << "Core: " << theIndex << " evicting block " << std::hex << block->tag << " in state " << block->state ));
      evict( block->tag, block->state );
    }

    set->get<by_order>().modify(block, ReplaceBlock(tag, state));
    order_iterator tail = set->get<by_order>().end();
    set->get<by_order>().relocate(tail, block);
    return set->project<by_way>(block);
  }

  inline bool isConsistent( uint64_t tagset) {
    // return true;
#if 1
    block_set_t * block_set = &(theBlocks[get_block_set(tagset, 0)]);
    tag_iterator block = block_set->get<by_tag>().find(get_block_tag(tagset));
    tag_iterator bend   = block_set->get<by_tag>().end();

    uint64_t rt_tag = get_rt_tag(tagset);
    int rt_set_index = get_rt_set(tagset);
    int32_t offset = get_block_offset(tagset);

    rt_set_t * rt_set = &(theRVA[rt_set_index]);
    rt_index * rt  = &(rt_set->get<by_tag>());
    rt_iterator entry = rt->find(rt_tag);
    rt_iterator end  = rt->end();

    if (entry == end) {
      rt_set  = &theERB;
      rt  = &(theERB.get<by_tag>());
      entry = rt->find(rt_tag);
      end  = rt->end();
    } else {
      rt_set_t * erb_set = &(theERB);
      rt_index * erb  = &(erb_set->get<by_tag>());
      rt_iterator erb_entry = erb->find(rt_tag);
      rt_iterator erb_end  = erb->end();

      if (erb_entry != erb_end) {
        DBG_(Dev, ( << "Region " << std::hex << rt_tag << " found in both RVA and ERB!" ));
        return false;
      }
    }

    if (entry == end) {
      if (block != bend) {
        if (isValid(block->state)) {
          DBG_(Dev, ( << "block " << std::hex << tagset << " region not present but block is valid"));
          return false;
        }
      }
      return true;
    }
    if ((block == bend) && isValid(entry->state[offset])) {
      DBG_(Dev, ( << "block " << std::hex << tagset << " not present but RVA says valid"));
      return false;
    }
    if ((block != bend) && entry->state[offset] != block->state)  {
      DBG_(Dev, ( << "block " << std::hex << tagset << " RVA and BST states do not match (" << entry->state[offset] << " != " << block->state << ")" ));
      return false;
    }
    if ((block != bend) && (entry->ways[offset] != block->way) && isValid(block->state)) {
      DBG_(Dev, ( << "block " << std::hex << tagset << " RVA and BST ways do not match (" << entry->ways[offset] << " != " << block->way << ")" ));
      return false;
    }
    return true;
#endif
  }

  LookupResult_p lookup(uint64_t tagset) {

    uint64_t rt_tag = get_rt_tag(tagset);
    int rt_set_index  = get_rt_set(tagset);
    int32_t offset    = get_block_offset(tagset);

    rt_set_t * rt_set = &(theRVA[rt_set_index]);
    rt_index * rt  = &(rt_set->get<by_tag>());
    rt_iterator entry = rt->find(rt_tag);
    rt_iterator end  = rt->end();

    if (entry == end) {
      // No region entry in RVA, check ERB
      rt_set  = &theERB;
      rt  = &(theERB.get<by_tag>());
      entry = rt->find(rt_tag);
      end  = rt->end();
    }

    if (entry != end) {
      block_set_t * block_set = &(theBlocks[get_block_set(tagset, entry->way)]);

      // We have a region entry
      if (isValid(entry->state[offset])) {
        // The block is valid

        tag_iterator t_block = block_set->get<by_tag>().find(get_block_tag(tagset));
        tag_iterator t_end   = block_set->get<by_tag>().end();

        if (t_block == t_end) {
          DBG_(Dev, ( << "Matched RT Tag 0x" << std::hex << entry->tag << std::dec << ", block way = " << entry->ways[offset] ));
          for (t_block = block_set->get<by_tag>().begin(); t_block != t_end; t_block++) {
            DBG_(Dev, ( << "Found Way " << t_block->way << ", Tag: 0x" << std::hex << t_block->tag ));
          }
        }
        DBG_Assert( t_block != t_end, ( << "tagset 0x" << std::hex << (uint64_t)tagset << ", BTag 0x" << (uint64_t)get_block_tag(tagset) << ", rt_tag 0x" << rt_tag << ", rt_set 0x" << rt_set_index << ", offset " << std::dec << offset ));

        way_iterator block = block_set->project<by_way>(t_block);

        RTLookupResult_p ret(new RTLookupResult(true, this, tagset));

        ret->rt_set  = rt_set;
        ret->offset  = offset;
        ret->region  = entry;
        ret->block  = block;
        ret->block_set = block_set;

        DBG_Assert( isConsistent(tagset) );

        return ret;
      }
    }

    return RTLookupResult_p(new RTLookupResult(false, this, tagset));
  }

  RTLookupResult allocate(uint64_t tagset, CoherenceState_t new_state) {

    theAllocateInProgress = true;
    theAllocateAddr = tagset;

    uint64_t rt_tag = get_rt_tag(tagset);
    int rt_set_index  = get_rt_set(tagset);
    int32_t offset    = get_block_offset(tagset);

    rt_set_t * rt_set = &(theRVA[rt_set_index]);
    rt_index * rt  = &(rt_set->get<by_tag>());
    rt_iterator entry = rt->find(rt_tag);
    rt_iterator end  = rt->end();

    DBG_(Verb, ( << theIndex << " - Allocating block " << std::hex << tagset << " in region " << rt_tag ));

    if (entry == end) {
      DBG_(Verb, ( << "Region not found in RVA, checking ERB."));
      // No region entry in RVA, check ERB
      rt_set  = &theERB;
      rt  = &(theERB.get<by_tag>());
      entry = rt->find(rt_tag);
      end  = rt->end();
    }

    // We didn't find a region entry in the RVA or the ERB
    if (entry == end) {
      DBG_(Verb, ( << "Region not found in ERB."));
      DBG_(Verb, ( << "Allocating new RVA entry with tag: " << std::hex << rt_tag << std::dec ));
      DBG_(Verb, ( << "Allocating new RVA entry with tag: " << std::hex << rt_tag << std::dec ));
      rt_set = &(theRVA[rt_set_index]);
      rt  = &(rt_set->get<by_tag>());

      // Start by adding another way to the set
      int32_t way = rt->size();

      // If the RVA set has too may ways, move one to the ERB
      if (way >= theRTAssociativity) {
        way = rt_set->get<by_order>().front().way;
        theERB.get<by_order>().push_back(rt_set->get<by_order>().front());
        rt_set->get<by_order>().pop_front();
      }

      // Allocate the new region entry in the RVA set
      // It's important to do this before evicting any blocks or region so that delayed actions in the directory are handled properly
      std::pair<rt_order_iterator, bool> temp;
      //temp = rt_set->get<by_order>().push_back(RTEntry(theBlocksPerRegion, rt_tag, rt_set_index, way));
      temp = rt_set->get<by_order>().push_back(RTEntry(theBlocksPerRegion, rt_tag, way));
      DBG_Assert(temp.second);
      entry = rt_set->project<by_tag>(temp.first);
      end   = rt->end();
      DBG_(Verb, ( << "Allocated Reigon " << std::hex << entry->tag ));

      // Now go back and see if we need to evict something from the ERB

      // If the ERB is full, need to evict all the blocks from the oldest entry
      if ((int)theERB.size() > theERBSize) {

        const RTEntry & region = theERB.get<by_order>().front();
        uint64_t addr = region.tag;

        DBG_(Verb, ( << "Evicting region with tag: " << std::hex << region.tag << std::dec ));

        for (int32_t i = 0; i < theBlocksPerRegion; i++, addr += theBlockSize) {
          block_set_t * tag_set = theBlocks + get_block_set(addr, region.way);

          DBG_(Iface, Addr(addr) ( << "RegionEviction: Core: " << theIndex << " evicting block " << std::hex << addr << " in state " << region.state[i] ));
          DBG_Assert( isConsistent(addr), ( << "Inconsistent state before evicting block " << std::hex << addr ));
          if (isValid(region.state[i])) {
            way_iterator block = tag_set->get<by_way>().find(region.ways[i]);
            DBG_Assert( block != tag_set->get<by_way>().end() );

            // Paranoid consistency check
            if (get_rt_tag(block->tag) != region.tag) {
              DBG_( Verb, ( << "RT tag: " << std::hex << region.tag << " Block tag: " << block->tag << " offset: " << i << std::dec ));
              tag_iterator t_block = tag_set->get<by_tag>().find(addr);
              tag_iterator t_end = tag_set->get<by_tag>().end();
              if (t_block != t_end) {
                DBG_Assert( false, ( << "found block in way " << t_block->way << " instead of way " << region.ways[i] ));
              } else {
                DBG_Assert( false, ( << "Did not find matching block tag." ));
              }
            } else {
              DBG_(Verb, ( << "Matching region and block tags in region eviction." ));
            }

            DBG_Assert( isConsistent(addr), ( << "Inconsistent state while evicting block " << std::hex << addr ));

            tag_set->get<by_way>().modify(block, ChangeBlockState(kInvalid));
            make_block_lru(tag_set, block);

            CoherenceState_t state = region.state[i];

            // evictBlock call-back to handle inclusivity if necessary
            // also handles Clean/Dirty evict messages to lower levels
            DBG_(Verb, ( << "Evicting Block " << std::hex << addr ));
            evict(addr, state);

            // Invalidate block, just to be paranoid
            theERB.modify(theERB.get<by_order>().begin(), InvalidateRTState(i));
          }
          DBG_Assert( isConsistent(addr), ( << "Inconsistent state after evicting block " << std::hex << addr ));
        }
        DBG_(Verb, ( << "Evicting Reigon " << std::hex << region.tag ));
        // Send notifications of region eviction event
        evictRegion(region.tag, region.owner);
        theERB.get<by_order>().pop_front();
      }

    }

    DBG_Assert( entry != end );

    block_set_t * block_set = &(theBlocks[get_block_set(tagset, entry->way)]);

    // We have a region entry
    if (isValid(entry->state[offset])) {
      DBG_(Verb, ( << "Blocks is Valid??" ));
      // The block is valid

      tag_iterator t_block = block_set->get<by_tag>().find(get_block_tag(tagset));
      tag_iterator t_end   = block_set->get<by_tag>().end();

      DBG_Assert( t_block != t_end );

      way_iterator block = block_set->project<by_way>(t_block);

      RTLookupResult ret(true, this, tagset);

      ret.rt_set = rt_set;
      ret.offset = offset;
      ret.region = entry;
      ret.block = block;
      ret.block_set = block_set;

      DBG_Assert( isConsistent(tagset) );

      theAllocateInProgress = false;

      return ret;
    } else {
      // The block is not currently valid, so let's allocate space for it

      // First, find a replacement block
      uint64_t block_tag = get_block_tag(tagset);
      way_iterator block = block_set->get<by_way>().end();
      int32_t way = block_set->size();

      // Try to find a matching tag in the BST
      // This is a somewhat un-realistic optimization since the BST
      // doesn't contain tags, but it simplifies things in the code
      // if we re-use entries like this, and it doesn't result in any
      // un-realistic behaviour unless we care about the specific "way" being used
      // Thus, if we implement a Non-LRU block replacement policy, we might not
      // want to do this.

      tag_iterator t_block = block_set->get<by_tag>().find(block_tag);
      tag_iterator t_end   = block_set->get<by_tag>().end();
      if (t_block != t_end) {
        block = block_set->project<by_way>(t_block);
        way = block->way;
        block_set->get<by_way>().modify(block, ChangeBlockState(new_state));
        update_lru(block_set, block);
        DBG_(Verb, ( << "Found matching BST tag, new state is " << block->state));
      } else {
        // First, check if the set is full yet, if not simply add the block to the set.
        if (way < theAssociativity) {
          std::pair<order_iterator, bool> result = block_set->get<by_order>().push_back(BlockEntry(block_tag, new_state, way));
          if (!result.second) {
            DBG_(Dev, ( << "Failed to insert block. Existing block tag: " << std::hex << result.first->tag << std::dec << ", way: " << result.first->way ));
            DBG_(Dev, ( << "Failed to insert block. New block tag: " << std::hex << block_tag << std::dec << ", way: " << way ));
            DBG_(Dev, ( << "Was in state " << std::hex << result.first->state << " new state " << new_state << std::dec ));
            DBG_(Dev, ( << "size: " << block_set->get<by_way>().size() ));
            DBG_(Dev, ( << "set: " << std::hex << block_tag ));
            DBG_(Dev, ( << "[" << theIndex << "]" << "Inserting block " << std::hex << block_tag << " in set: " << get_block_set(tagset, entry->way) << ", way: " << way << ", size: " << block_set->size() ));
            DBG_Assert(false);
          }
          block = block_set->project<by_way>(result.first);
          DBG_(Verb, ( << "Allocated new block, state is " << block->state));
        } else if (!isValid(block_set->get<by_order>().front().state)) {
          // Next, look for an invalid block (that would naturally be at the bottom (front) of the LRU
          way = block_set->get<by_order>().front().way;
          block = replace_lru_block(block_set, block_tag, new_state);
          DBG_(Verb, ( << "Replaced Invalid Block, new state is " << block->state));
        } else {
          // Set is full of valid blocks, use replacement policy
          if (theReplPolicy == SET_LRU) {
            way = block_set->get<by_order>().front().way;
            block = replace_lru_block(block_set, block_tag, new_state);
            DBG_(Verb, ( << "Replaced LRU block, state is " << block->state));
          } else if (theReplPolicy == REGION_LRU) {
            DBG_Assert( false , ( << "RegionLRU Not Yet Supported !!!!" ));

            // This actually becomes complicated and needs additional support
            // if we want to implement it.
            // Each BST set contains blocks that map to multiple RVA sets
            // So we need an LRU order between all of the blocks in those RVA
            // sets, and then we need to pick the block that maps the the least
            // recently used RVA entry. Keeping in mind that there are more RVA
            // entries than there are blocks in the BST set.

          } else {
            // Don't know any other policies
            DBG_Assert( false );
          }
        }
      }
      DBG_(Verb, ( << "[" << theIndex << "]" << "Inserting block in set: " << std::hex << get_block_set(tagset, entry->way) << ", way: " << way << ", size: " << block_set->size()));

      DBG_Assert(block != block_set->get<by_way>().end());

      rt->modify(entry, AllocateBlock(offset, new_state, way));

      DBG_(Verb, ( << "RVA state set to: " << entry->state[offset] ));

      entry = update_lru(rt_set, entry);

      RTLookupResult ret(true, this, tagset);
      ret.rt_set  = rt_set;
      ret.offset  = offset;
      ret.region  = entry;
      ret.block  = block;
      ret.block_set = block_set;

      DBG_Assert( isConsistent(tagset) );

      theAllocateInProgress = false;

      return ret;
    }

    theAllocateInProgress = false;
  }

  void updateState(RTLookupResult & result, CoherenceState_t new_state, bool make_MRU, bool make_LRU) {
    DBG_Assert(result.valid_result, ( << theName << " - updateState() for invalid result." ));

    result.rt_set->get<by_tag>().modify(result.region, ChangeRTState(result.offset, new_state));
    result.block_set->get<by_way>().modify(result.block, ChangeBlockState(new_state));

    // Fixup LRU order
    if (make_MRU) {
      result.region = update_lru(result.rt_set, result.region);
      result.block = update_lru(result.block_set, result.block);
    } else if (make_LRU) {
      make_block_lru(result.block_set, result.block);
    }
  }

  uint32_t regionProbe( uint64_t tagset) {

    uint32_t ret = 0;

    uint64_t rt_tag = get_rt_tag(tagset);
    int rt_set_index = get_rt_set(tagset);

    rt_set_t * rt_set = &(theRVA[rt_set_index]);
    rt_index * rt  = &(rt_set->get<by_tag>());
    rt_iterator entry = rt->find(rt_tag);
    rt_iterator end  = rt->end();

    if (entry == end) {
      rt_set  = &theERB;
      rt  = &(theERB.get<by_tag>());
      entry = rt->find(rt_tag);
      end  = rt->end();
    }

    if (entry != end) {
      if (theBlockScout) {
        if (entry->region_state != PARTIAL_SHARED_REGION) {
          rt->modify(entry, ChangeRegionState(PARTIAL_SHARED_REGION));
        }
        int32_t offset = get_block_offset(tagset);
        rt->modify(entry, SetSharedBlock(offset));
        ret = entry->validVector();
      } else {
        if (entry->region_state != SHARED_REGION) {
          rt->modify(entry, ChangeRegionState(SHARED_REGION));
        }
        // Make sure we actually have a block in this region cached!
        if (entry->hasCachedBlocks() || theImpreciseRS) {
          ret = 1;
        }
      }
    }

    DBG_(Verb, ( << "Core " << theIndex << ": regionProbe(" << std::hex << tagset << std::dec << ") = " << ret ));
    return ret;
  }

  uint32_t blockScoutProbe( uint64_t tagset) {

    uint32_t ret = 0;

    uint64_t rt_tag = get_rt_tag(tagset);
    int rt_set_index = get_rt_set(tagset);

    rt_set_t * rt_set = &(theRVA[rt_set_index]);
    rt_index * rt  = &(rt_set->get<by_tag>());
    rt_iterator entry = rt->find(rt_tag);
    rt_iterator end  = rt->end();

    if (entry == end) {
      rt_set  = &theERB;
      rt  = &(theERB.get<by_tag>());
      entry = rt->find(rt_tag);
      end  = rt->end();
    }

    if (entry != end) {
      ret = entry->validVector();
    }

    DBG_(Verb, ( << "Core " << theIndex << ": blockSetProbe(" << std::hex << tagset << std::dec << ") = " << ret ));
    return ret;
  }

  uint32_t blockProbe( uint64_t tagset) {

    uint32_t ret = 0;

    uint64_t rt_tag = get_rt_tag(tagset);
    int rt_set_index = get_rt_set(tagset);

    rt_set_t * rt_set = &(theRVA[rt_set_index]);
    rt_index * rt  = &(rt_set->get<by_tag>());
    rt_iterator entry = rt->find(rt_tag);
    rt_iterator end  = rt->end();

    if (entry == end) {
      rt_set  = &theERB;
      rt  = &(theERB.get<by_tag>());
      entry = rt->find(rt_tag);
      end  = rt->end();
    }

    if (entry != end) {
      int32_t offset = get_block_offset(tagset);
      ret = (uint32_t)isValid(entry->state[offset]);
    }

    DBG_(Verb, ( << "Core " << theIndex << ": blockProbe(" << std::hex << tagset << std::dec << ") = " << ret ));
    return ret;
  }

  void setNonSharedRegion( uint64_t tagset) {

    DBG_(Verb, ( << "Core " << theIndex << ": setNonSharedRegion(" << std::hex << tagset << std::dec << ")" ));

    uint64_t rt_tag = get_rt_tag(tagset);
    int rt_set_index = get_rt_set(tagset);

    rt_set_t * rt_set = &(theRVA[rt_set_index]);
    rt_index * rt  = &(rt_set->get<by_tag>());
    rt_iterator entry = rt->find(rt_tag);
    rt_iterator end  = rt->end();

    if (entry == end) {
      rt_set  = &theERB;
      rt  = &(theERB.get<by_tag>());
      entry = rt->find(rt_tag);
      end  = rt->end();
    }

    if (entry != end) {
      if (entry->region_state != NON_SHARED_REGION) {
        rt->modify(entry, ChangeRegionState(NON_SHARED_REGION));
      }
      if (theBlockScout) {
        rt->modify(entry, ChangeSharedBlocks(0));
      }
    }
  }

  void setPartialSharedRegion( uint64_t tagset, uint32_t shared) {

    DBG_(Verb, ( << "Core " << theIndex << ": setNonSharedRegion(" << std::hex << tagset << std::dec << ")" ));

    if (!theBlockScout) {
      return;
    }

    uint64_t rt_tag = get_rt_tag(tagset);
    int rt_set_index = get_rt_set(tagset);

    rt_set_t * rt_set = &(theRVA[rt_set_index]);
    rt_index * rt  = &(rt_set->get<by_tag>());
    rt_iterator entry = rt->find(rt_tag);
    rt_iterator end  = rt->end();

    if (entry == end) {
      rt_set  = &theERB;
      rt  = &(theERB.get<by_tag>());
      entry = rt->find(rt_tag);
      end  = rt->end();
    }

    if (entry != end) {
      rt->modify(entry, ChangeRegionState(PARTIAL_SHARED_REGION));
      rt->modify(entry, ChangeSharedBlocks(shared));
    }
  }

  bool isNonSharedRegion( uint64_t tagset) {
    bool ret = false;

    uint64_t rt_tag = get_rt_tag(tagset);
    int rt_set_index = get_rt_set(tagset);

    rt_set_t * rt_set = &(theRVA[rt_set_index]);
    rt_index * rt  = &(rt_set->get<by_tag>());
    rt_iterator entry = rt->find(rt_tag);
    rt_iterator end  = rt->end();

    if (entry == end) {
      rt_set  = &theERB;
      rt  = &(theERB.get<by_tag>());
      entry = rt->find(rt_tag);
      end  = rt->end();
    }

    if (entry != end) {
      if (entry->region_state == NON_SHARED_REGION) {
        ret = true;
      } else if (theBlockScout && (entry->region_state == PARTIAL_SHARED_REGION)) {
        int32_t offset = get_block_offset(tagset);
        ret = ! entry->shared[offset];
      }
    }
    DBG_(Verb, ( << "Core " << theIndex << ": isNonSharedRegion(" << std::hex << tagset << std::dec << ") = " << ret ));
    return ret;
  }

  void getSetTags(uint64_t region, std::list<PhysicalMemoryAddress> &tags) {

    uint64_t tagset = region;
    int32_t set_index = get_block_set(tagset, 0);

    // First, check if we're in the process of allocating a new block to this set
    // (This happens if this was called as a resul of evicting a block during the allocation process)
    if (theAllocateInProgress) {
      int32_t alloc_set = get_block_set(theAllocateAddr, 0);
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

  void saveState( std::ostream & s ) {
    boost::archive::binary_oarchive oa(s);
    DBG_(Verb, ( << "Saving RT state." ));

    uint64_t set_count = theNumRTSets;
    uint32_t associativity = theRTAssociativity;

    oa << set_count;
    oa << associativity;

    for (int32_t set = 0; set < theNumRTSets; set++) {
      rt_order_iterator entry = theRVA[set].get<by_order>().begin();
      rt_order_iterator end = theRVA[set].get<by_order>().end();
      int32_t way = 0;
      RTSerializer serial;
      for (; entry != end; way++, entry++) {
        serial.tag = entry->tag;
        serial.owner = entry->owner;
        serial.way = entry->way;
        switch (entry->region_state) {
          case NON_SHARED_REGION:
            serial.state = 'N';
            break;
          case SHARED_REGION:
            serial.state = 'S';
            break;
          case PARTIAL_SHARED_REGION:
            serial.state = 'P';
            break;
          default:
            DBG_Assert(false, ( << "Don't know how to save state " << (int)entry->region_state ));
            break;
        }
        DBG_(Verb, ( << theIndex << " - Save Region " << std::hex << serial.tag << " owner = " << (int)serial.owner << " state = " << serial.state ));

        oa << serial;
      }
      serial.tag = -1;
      serial.owner = -1;
      serial.state = 'S';
      for (; way < theRTAssociativity; way++) {
        serial.way = way;
        oa << serial;
      }
    }

    int32_t size = theERB.size();
    oa << size;
    rt_order_iterator entry = theERB.get<by_order>().begin();
    rt_order_iterator end = theERB.get<by_order>().end();
    for (; entry != end; entry++) {
      RTSerializer serial;
      serial.tag = entry->tag;
      serial.owner = entry->owner;
      serial.way = entry->way;
      switch (entry->region_state) {
        case NON_SHARED_REGION:
          serial.state = 'N';
          break;
        case SHARED_REGION:
          serial.state = 'S';
          break;
        case PARTIAL_SHARED_REGION:
          serial.state = 'P';
          break;
        default:
          DBG_Assert(false, ( << "Don't know how to save state " << (int)entry->region_state ));
          break;
      }

      DBG_(Verb, ( << theIndex << " - Save Region " << std::hex << serial.tag << " owner = " << (int)serial.owner << " state = " << serial.state ));
      oa << serial;
    }

    set_count = theNumDataSets;
    associativity = theAssociativity;

    oa << set_count;
    oa << associativity;

    BlockSerializer bs;
    for (int32_t set = 0; set < theNumDataSets; set++) {
      order_iterator block = theBlocks[set].get<by_order>().begin();
      order_iterator end = theBlocks[set].get<by_order>().end();
      int32_t way = 0;
      for (; block != end; block++, way++) {
        bs.tag = block->tag;
        bs.way = block->way;
        //s << "<" << block->tag << "," << block->way << ",";
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
        //s << ">";
        oa << bs;
        DBG_Assert( isConsistent(bs.tag) );
      }
      bs.state = 'I';
      bs.tag = 0;
      for (; way < theAssociativity; way++) {
        bs.way = way;
        oa << bs;
        //s << "< 0," << way << ",I>";
      }
      //s << std::endl;
    }
  }

  bool loadState( std::istream & s ) {
    boost::archive::binary_iarchive ia(s);

    uint64_t set_count = 0;
    uint32_t associativity = 0;

    ia >> set_count;
    ia >> associativity;

    DBG_Assert( set_count == (uint64_t)theNumRTSets, ( << "Error loading cache state. Flexpoint contains " << set_count << " RT sets but simulator configured for " << theNumRTSets << " RT sets." ));
    DBG_Assert( associativity == (uint64_t)theRTAssociativity, ( << "Error loading cache state. Flexpoint contains " << associativity << "-way RT sets but simulator configured for " << theRTAssociativity << "-way RT sets." ));

    for (int32_t set = 0; set < theNumRTSets; set++) {
      for (int32_t way = 0; way < theRTAssociativity; way++) {
        RTSerializer serial;
        ia >> serial;
        RegionState rstate = SHARED_REGION;
        switch (serial.state) {
          case 'N':
            rstate = NON_SHARED_REGION;
            break;
          case 'S':
            rstate = SHARED_REGION;
            break;
          case 'P':
            rstate = PARTIAL_SHARED_REGION;
            break;
          default:
            DBG_Assert(false, ( << "Unknown RegionState: " << serial.state));
            break;
        }
        theRVA[set].get<by_order>().push_back(RTEntry(theBlocksPerRegion, serial.tag, way, rstate, serial.owner));
      }
    }

    int32_t erb_size;
    ia >> erb_size;
    for (int32_t way = 0; way < erb_size; way++) {
      RTSerializer serial;
      ia >> serial;
      RegionState rstate = SHARED_REGION;
      switch (serial.state) {
        case 'N':
          rstate = NON_SHARED_REGION;
          break;
        case 'S':
          rstate = SHARED_REGION;
          break;
        case 'P':
          rstate = PARTIAL_SHARED_REGION;
          break;
        default:
          DBG_Assert(false, ( << "Unknown RegionState: " << serial.state));
          break;
      }
      theERB.get<by_order>().push_back(RTEntry(theBlocksPerRegion, serial.tag, way, rstate, serial.owner));
    }

    ia >> set_count;
    ia >> associativity;

    DBG_Assert( set_count == (uint64_t)theNumDataSets, ( << "Error loading cache state. Flexpoint contains " << set_count << " sets but simulator configured for " << theNumDataSets << " sets." ));
    DBG_Assert( associativity == (uint64_t)theAssociativity, ( << "Error loading cache state. Flexpoint contains " << associativity << "-way sets but simulator configured for " << theAssociativity << "-way sets." ));

    for (int32_t set = 0; set < theNumDataSets; set++) {
      for (int32_t way = 0; way < theAssociativity; way++) {
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
        theBlocks[set].get<by_order>().push_back(BlockEntry(bs.tag, bstate, way));
        if (theIndex == 11) {
          DBG_(Trace, ( << "Loading block " << std::hex << bs.tag << " in state " << bs.state << std::dec << " in set " << set << ", way " << way ));
        }

        if (bstate != kInvalid) {
          uint64_t rt_tag = get_rt_tag(bs.tag);
          int rt_set_index  = get_rt_set(bs.tag);
          int32_t offset    = get_block_offset(bs.tag);

          rt_set_t * rt_set = &(theRVA[rt_set_index]);
          rt_index * rt  = &(rt_set->get<by_tag>());
          rt_iterator entry = rt->find(rt_tag);
          rt_iterator end  = rt->end();
          if (entry == end) {
            rt_set = &theERB;
            rt  = &(rt_set->get<by_tag>());
            entry = rt->find(rt_tag);
            end  = rt->end();
            DBG_Assert(entry != end);
          }
          rt->modify(entry, AllocateBlock(offset, bstate, way));
        }
        DBG_Assert( isConsistent(bs.tag) );
      }
    }

    return true;
  }

};

};  // namespace nFastCMPCache

#endif /* FLEXUS_FASTCMPCACHE_RTCACHE_HPP_INCLUDED */
