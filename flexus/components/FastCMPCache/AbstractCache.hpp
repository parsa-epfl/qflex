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
#ifndef FLEXUS_FASTCMPCACHE_ABSTRACT_CACHE_HPP_INCLUDED
#define FLEXUS_FASTCMPCACHE_ABSTRACT_CACHE_HPP_INCLUDED

#include <components/FastCMPCache/CoherenceStates.hpp>
#include <components/FastCMPCache/LookupResult.hpp>

#include <functional>
#include <boost/dynamic_bitset.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

using namespace boost::multi_index;

#include <core/types.hpp>

using Flexus::SharedTypes::PhysicalMemoryAddress;

namespace nFastCMPCache {

typedef std::function<void( uint64_t tagset, CoherenceState_t state) > evict_function_t;
typedef std::function<void( uint64_t tagset, int32_t owner ) > region_evict_function_t;
typedef std::function<bool( uint64_t tagset, bool icache, bool dcache ) > invalidate_function_t;

class AbstractCache {
public:
  virtual ~AbstractCache() {}

  virtual LookupResult_p lookup(uint64_t tagset) = 0;

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

  virtual int32_t getOwner( LookupResult_p result, uint64_t tagset) {
    DBG_Assert(false, ( << "getOwner() function not supported by this Cache structure." ));
    return -1;
  }

  virtual void updateOwner( LookupResult_p result, int32_t owner, uint64_t tagset, bool shared = true) {
    DBG_Assert(false, ( << "updateOwner() function not supported by this Cache structure." ));
  }

  virtual void finalize() {
    DBG_Assert(false, ( << "finalize() function not supported by this Cache structure." ));
  }

  virtual void getSetTags(uint64_t region, std::list<PhysicalMemoryAddress> &tags) = 0;

  virtual void saveState( std::ostream & s ) = 0;

  virtual bool loadState( std::istream & s ) = 0;
};

}  // namespace nFastCMPCache

#endif /* FLEXUS_FASTCMPCACHE_ABSTRACT_CACHE_HPP_INCLUDED */
