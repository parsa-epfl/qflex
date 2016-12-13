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
#ifndef SET_ASSOC_ARRAY_HPP_
#define SET_ASSOC_ARRAY_HPP_

#include "SetAssocLookupResult.hpp"

namespace nSetAssoc {

struct Dynamic {};
struct HighAssociative {};
struct LowAssociative {};
struct Infinite {};

template < class UserDefinedSet, class Implementation = LowAssociative >
class SetAssocArray;
//UserDefinedSet must be:
//derived from BaseSetDefinition
//
//Implementation may be:
//Dynamic
//HighAssociative
//LowAssociative
//Infinite

template < class UserDefinedSet >
class SetAssocArray<UserDefinedSet, LowAssociative> {

public:
  // allow others to extract the block and set types
  typedef UserDefinedSet Set;
  typedef typename Set::Block Block;

  // convenience typedefs for basic types
  typedef typename Set::Tag Tag;
  typedef typename Set::BlockNumber BlockNumber;
  typedef typename Set::Index Index;

  typedef aux_::LowAssociativityLookupResult<Set> BaseLookupResult;

private:
  // The number of sets
  uint32_t myNumSets;

  // The associativity of each set
  uint32_t myAssoc;

  // The actual sets (which contain blocks and hence data)
  Set * theSets;

  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const uint32_t version) {
    ar & myNumSets;
    for (uint32_t i = 0; i < myNumSets; ++i) {
      ar & theSets[i];
    }
  }

public:
  // CONSTRUCTION
  SetAssocArray(uint32_t aNumSets, uint32_t anAssoc)
    : myNumSets(aNumSets)
    , myAssoc(anAssoc) {
    theSets = (Set *)( new char[myNumSets * sizeof(Set)] );
    // this uses placement new, so on delete, make sure to manually call
    // the destructor for each Set, then delete[] the array
    for (uint32_t ii = 0; ii < myNumSets; ii++) {
      new(theSets + ii) Set(myAssoc, Index(ii));
    }

    //DBG_( VVerb, ( << "Constructed a dynamic set-associative array" ) );
  }

  // SET LOOKUP
  Set & operator[](const Index & anIndex) {
    // TODO: bounds checking/assertion?
    return theSets[anIndex];
  }
  const Set & operator[](const Index & anIndex) const {
    // TODO: bounds checking/assertion?
    return theSets[anIndex];
  }

};  // end class SetAssocArray

}  // end namespace nSetAssoc

#endif
