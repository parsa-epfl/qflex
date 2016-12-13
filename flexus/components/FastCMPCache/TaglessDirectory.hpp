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
#ifndef __TAGLESS_DIRECTORY_HPP__
#define __TAGLESS_DIRECTORY_HPP__

#include <boost/archive/binary_iarchive.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/composite_key.hpp>
using namespace boost::multi_index;

#include <components/CommonQEMU/GlobalHasher.hpp>
#include <components/CommonQEMU/Util.hpp>
#include <components/CommonQEMU/Serializers.hpp>
using nCommonUtil::log_base2;
using nCommonUtil::AddressHash;
using nCommonSerializers::StdDirEntrySerializer;

namespace nCMPCache {

template < typename _State, typename _EState = _State >
class TaglessDirectory : public AbstractDirectory<_State, _EState> {
public:

  class TaglessLookupResult : public AbstractLookupResult<_State> {
  private:
    MemoryAddress  theAddress;
    std::set<int>  theBucketList;
    std::vector<_State> &theSet;
    _State    theAggregateState;

    TaglessLookupResult(MemoryAddress addr, const std::set<int>& buckets, std::vector<_State> &set, int32_t num_sharers)
      : theAddress(addr), theBucketList(buckets), theSet(set), theAggregateState(num_sharers) {
      std::set<int>::iterator bucket = theBucketList.begin();
      theAggregateState = theSet[*bucket];
      bucket++;
      for (; bucket != theBucketList.end(); bucket++) {
        theAggregateState &= theSet[*bucket];
      }
    }

    friend class TaglessDirectory<_State, _EState>;
  public:
    virtual ~TaglessLookupResult() {}
    virtual bool found() {
      return true;
    }
    virtual bool	isProtected() {
      return false;
    }
    virtual void setProtected(bool val) { }
    virtual const _State & state() const {
      return theAggregateState;
    }

    virtual void addSharer( int32_t sharer) {
      theAggregateState.addSharer(sharer);
      std::set<int>::iterator bucket = theBucketList.begin();
      for (; bucket != theBucketList.end(); bucket++) {
        theSet[*bucket].addSharer(sharer);
      }

    }
    virtual void removeSharer( int32_t sharer) {
      DBG_Assert( false, ( << "Interface not supported by TaglessDirectory. Use remove(sharer,msg) instead." ));
    }
    void removeSharer( int32_t sharer, TaglessDirMsg_p msg) {
      if (msg->getConflictFreeBuckets().size() == 0) {
        return;
      }
      DBG_Assert(msg->getConflictFreeBuckets().size() == 1);
      const std::set<int> &conflict_free(msg->getConflictFreeBuckets().begin()->second);
      std::set<int>::const_iterator bucket = conflict_free.begin();
      for (; bucket != conflict_free.end(); bucket++) {
        theSet[*bucket].removeSharer(sharer);
      }
    }
    virtual void setSharer( int32_t sharer) {
      DBG_Assert( false, ( << "Interface not supported by TaglessDirectory. Use set(sharer,msg) instead." ));
    }
    void setSharer( int32_t sharer, TaglessDirMsg_p msg) {
      addSharer(sharer);
      const std::map<int, std::set<int> >& free_map = msg->getConflictFreeBuckets();
      std::map<int, std::set<int> >::const_iterator map_iter = free_map.begin();
      for (; map_iter != free_map.end(); map_iter++) {
        int32_t old_sharer = map_iter->first;
        std::set<int>::const_iterator bucket = map_iter->second.begin();
        for (; bucket != map_iter->second.end(); bucket++) {
          theSet[*bucket].removeSharer(old_sharer);
        }
      }
    }
    virtual void setState( const _State & state ) {
      DBG_Assert( false, ( << "Interface not supported by TaglessDirectory. No way to set state with a tagles directory." ));
    }

  };

private:
  std::vector<std::vector<_State> > theDirectory;

  int32_t theNumSharers;
  int32_t theBlockSize;
  int32_t theBanks;
  int32_t theTotalBanks;
  int32_t theGlobalBankIndex;
  int32_t theLocalBankIndex;
  int32_t theBankInterleaving;
  int32_t theGroups;
  int32_t theGroupIndex;
  int32_t theGroupInterleaving;

  int32_t theBankShift;
  int32_t theBankMask;
  int32_t theGroupShift;
  int32_t theGroupMask;

  int32_t theNumLocalSets;
  int32_t theTotalNumSets;
  int32_t theNumBuckets;
  int32_t theTotalNumBuckets;
  bool thePartitioned;

  int32_t setLowShift, setMidShift, setHighShift, setLowMask, setMidMask, setHighMask;

  int32_t getBank(uint64_t addr) {
    return (addr >> theBankShift) & theBankMask;
  }
  int32_t getGroup(uint64_t addr) {
    return (addr >> theGroupShift) & theGroupMask;
  }

  int32_t get_set(uint64_t addr) {
    return ((addr >> setLowShift) & setLowMask) | ((addr >> setMidShift) & setMidMask) | ((addr >> setHighShift) & setHighMask);
  }

public:
  typedef TaglessLookupResult LookupResult;
  typedef typename boost::intrusive_ptr<LookupResult> LookupResult_p;

  TaglessDirectory(const CMPCacheInfo & theInfo, std::list<std::pair<std::string, std::string> > &args)
    : theNumLocalSets(-1), theTotalNumSets(-1), theNumBuckets(-1), thePartitioned(false) {
    theNumSharers = theInfo.theCores;
    theBlockSize = theInfo.theBlockSize;
    theBanks = theInfo.theNumBanks;
    theBankInterleaving = theInfo.theBankInterleaving;
    theGroups = theInfo.theNumGroups;
    theGroupInterleaving = theInfo.theGroupInterleaving;

    theGlobalBankIndex = theInfo.theNodeId;
    theLocalBankIndex = theInfo.theNodeId % theBanks;
    theGroupIndex = theInfo.theNodeId / theBanks;
    theTotalBanks = theBanks * theGroups;

    std::list<std::string> theHashFns;

    std::list<std::pair<std::string, std::string> >::iterator arg = args.begin();
    for (; arg != args.end(); arg++) {
      if (strcasecmp(arg->first.c_str(), "hash") == 0) {
        theHashFns.push_back(arg->second);
      } else if (strcasecmp(arg->first.c_str(), "total_sets") == 0) {
        theTotalNumSets = boost::lexical_cast<int>(arg->second);
        theNumLocalSets = -1;
      } else if (strcasecmp(arg->first.c_str(), "local_sets") == 0) {
        theNumLocalSets = boost::lexical_cast<int>(arg->second);
        theTotalNumSets = -1;
      } else if (strcasecmp(arg->first.c_str(), "buckets") == 0) {
        theNumBuckets = boost::lexical_cast<int>(arg->second);
      } else if (strcasecmp(arg->first.c_str(), "partitioned") == 0) {
        thePartitioned = boost::lexical_cast<bool>(arg->second);
      } else {
        DBG_Assert(false, ( << "Unexpected parameter '" << arg->first << "' passed to Tagless Directory." ));
      }
    }

    if (theNumLocalSets > 0) {
      theTotalNumSets = theNumLocalSets * theTotalBanks;
    } else if (theTotalNumSets > 0) {
      theNumLocalSets = theTotalNumSets / theTotalBanks;
      DBG_Assert( (theNumLocalSets * theTotalBanks) == theTotalNumSets, ( << "Invalid number of sets. Total Sets = " << theTotalNumSets << ", Num Banks = " << theTotalBanks ));
    } else {
      DBG_Assert( false, ( << "Failed to set number of sets." ));
    }

    int32_t theHashShift = log_base2(theTotalNumSets) + log_base2(theBlockSize);

    DBG_Assert( theNumBuckets > 0 );
    DBG_Assert( theHashFns.size() > 0 );

    nGlobalHasher::GlobalHasher::theHasher().initialize(theHashFns, theHashShift, theNumBuckets, thePartitioned);

    theTotalNumBuckets = theNumBuckets;
    if (thePartitioned) {
      theTotalNumBuckets *= theHashFns.size();
    }

    DBG_Assert( ((theBlockSize - 1) & theBlockSize) == 0);
    DBG_Assert( ((theNumLocalSets - 1) & theNumLocalSets) == 0);
    DBG_Assert( ((theGroups - 1) & theGroups) == 0);
    DBG_Assert( ((theBanks - 1) & theBanks) == 0);
    DBG_Assert( ((theBankInterleaving - 1) & theBankInterleaving) == 0);
    DBG_Assert( ((theGroupInterleaving - 1) & theGroupInterleaving) == 0);

    DBG_Assert( (theBankInterleaving * theBanks) <= theGroupInterleaving, ( << "Invalid interleaving: BI = "
                << theBankInterleaving << ", Banks = " << theBanks << ", GI = " << theGroupInterleaving << ", Groups = " << theGroups ));

    DBG_(Dev, ( << "Creating directory with " << theNumLocalSets << " sets and " << theTotalNumBuckets << " total buckets." ));

    theDirectory.resize(theNumLocalSets, std::vector<_State>(theTotalNumBuckets, _State(theNumSharers) ));

    DBG_Assert( ((theNumLocalSets - 1) & theNumLocalSets) == 0 );

    int32_t blockOffsetBits = log_base2(theBlockSize);
    int32_t indexBits = log_base2(theNumLocalSets);
    int32_t bankBits = log_base2(theBanks);
    int32_t bankInterleavingBits = log_base2(theBankInterleaving);
    int32_t groupBits = log_base2(theGroups);
    int32_t groupInterleavingBits = log_base2(theGroupInterleaving);

    int32_t lowBits = bankInterleavingBits - blockOffsetBits;
    int32_t midBits = groupInterleavingBits - bankInterleavingBits - bankBits;
    int32_t highBits = indexBits - midBits - lowBits;
    if ((midBits + lowBits) > indexBits) {
      midBits = indexBits - lowBits;
      highBits = 0;
    }

    setLowMask = (1 << lowBits) - 1;
    setMidMask = ((1 << midBits) - 1) << lowBits;
    setHighMask = ((1 << highBits) - 1) << (midBits + lowBits);

    setLowShift = blockOffsetBits;
    setMidShift = bankBits + blockOffsetBits;
    setHighShift = groupBits + bankBits + blockOffsetBits;

    theBankMask = theBanks - 1;
    theBankShift = bankInterleavingBits;
    theGroupMask = theGroups - 1;
    theGroupShift = groupInterleavingBits;

  }

  virtual bool allocate(boost::intrusive_ptr<AbstractLookupResult<_State> > lookup, MemoryAddress address, const _State & state) {
    DBG_Assert(false, ( << "Tagless Directory does not support 'allocate()' function."  ));
    return false;
  }

  virtual boost::intrusive_ptr<AbstractLookupResult<_State> > lookup(MemoryAddress address) {
    return nativeLookup(address);
  }

  LookupResult_p nativeLookup(MemoryAddress address) {
    int32_t set_index = get_set(address);
    std::set<int> bucket_list(nGlobalHasher::GlobalHasher::theHasher().hashAddr(address));

    return LookupResult_p(new LookupResult(address, bucket_list, theDirectory[set_index], theNumSharers));
  }

  virtual bool sameSet(MemoryAddress a, MemoryAddress b) {
    return (get_set(a) == get_set(b));
  }

  // We don't have an evict buffer, just return null
  // (the policy should be aware of this and provide it's own InfiniteEvictBuffer to make everything work nicely
  virtual DirEvictBuffer<_EState>* getEvictBuffer() {
    return nullptr;
  }

  virtual bool loadState(std::istream & is) {
    boost::archive::binary_iarchive ia(is);
    MemoryAddress addr(0);
    int32_t local_set = 0;
    int32_t global_set = 0;

    int32_t sets, buckets;
    ia >> sets;
    ia >> buckets;

    DBG_Assert( sets == theTotalNumSets , ( << "Error loading flexpoint. Flexpoint created with " << sets << " sets, but simulator configured for " << theTotalNumSets << " sets." ));
    DBG_Assert( buckets == theNumBuckets , ( << "Error loading flexpoint. Flexpoint created with " << buckets << " buckets, but simulator configured for " << theNumBuckets << " buckets." ));

    for (; global_set < theTotalNumSets; global_set++, addr += theBlockSize) {
      if ((getBank(addr) == theLocalBankIndex) && (getGroup(addr) == theGroupIndex)) {
        for (int32_t bucket = 0; bucket < theNumBuckets; bucket++) {
          StdDirEntrySerializer serializer;
          ia >> serializer;
          theDirectory[local_set][bucket] = serializer.state;
        }
        local_set++;
      } else {
        for (int32_t bucket = 0; bucket < theNumBuckets; bucket++) {
          StdDirEntrySerializer serializer;
          ia >> serializer;
          // Skip over entries that belong to other banks
        }
      }
    }
    return true;
  }
};

}; // namespace nCMPCache

#endif // __TAGLESS_DIRECTORY_HPP__

