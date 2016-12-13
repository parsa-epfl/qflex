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
#ifndef SET_ASSOC_LOOKUP_RESULT_HPP_
#define SET_ASSOC_LOOKUP_RESULT_HPP_

#include <core/exception.hpp>

namespace nSetAssoc {
namespace aux_ {

template <class UserDefinedSet>
struct BaseLookupResult {
  typedef UserDefinedSet Set;
  typedef typename Set::Block Block;
  typedef typename Set::Tag Tag;

public:
  //! HIT/MISS check
  bool hit() const {
    return isHit;
  }
  bool miss() const {
    return !isHit;
  }

  //! SET access
  Set & set() {
    return theSet;
  }
  const Set & set() const {
    return theSet;
  }

  //! BLOCK access
  Block & block() {
    if (theBlock) {
      return *theBlock;
    }
    throw Flexus::Core::FlexusException("no block");
  }
  const Block & block() const {
    if (theBlock) {
      return *theBlock;
    }
    throw Flexus::Core::FlexusException("no block");
  }

protected:
  //! CONSTRUCTORS
  BaseLookupResult(Set & aSet,
                   Block & aBlock,
                   bool aHit)
    : theSet(aSet)
    , theBlock(&aBlock)
    , isHit(aHit)
  {}
  BaseLookupResult(Set & aSet,
                   bool aHit)
    : theSet(aSet)
    , theBlock(0)
    , isHit(aHit)
  {}

  //! MEMBERS
  Set & theSet;
  Block * theBlock;
  bool isHit;

};

template <class UserDefinedSet>
struct LowAssociativityLookupResult : public BaseLookupResult<UserDefinedSet> {

  //! CREATION
  static typename LowAssociativityLookupResult::Block * find(typename LowAssociativityLookupResult::Set & aSet, const typename LowAssociativityLookupResult::Tag & aTag) {
    for (typename LowAssociativityLookupResult::Set::BlockIterator iter = aSet.blocks_begin(); iter != aSet.blocks_end(); ++iter) {
      if (iter->tag() == aTag) {
        if (iter->valid()) {
          return iter;
        }
      }
    }
    return 0;
  }

protected:
  //! CONSTRUCTORS
  LowAssociativityLookupResult(typename LowAssociativityLookupResult::Set & aSet,
                               typename LowAssociativityLookupResult::Block & aBlock,
                               bool aHit)
    : BaseLookupResult<UserDefinedSet>(aSet, aBlock, aHit)
  {}
  LowAssociativityLookupResult(typename LowAssociativityLookupResult::Set & aSet,
                               bool aHit)
    : BaseLookupResult<UserDefinedSet>(aSet, aHit)
  {}

};

} //namepspace aux_
} //namepspace nSetAssoc

#endif //SET_ASSOC_LOOKUP_RESULT_HPP_
