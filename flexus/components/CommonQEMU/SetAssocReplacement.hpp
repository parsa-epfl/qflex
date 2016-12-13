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
#ifndef SET_ASSOC_REPLACEMENT_HPP_
#define SET_ASSOC_REPLACEMENT_HPP_

#include <boost/serialization/base_object.hpp>

namespace nSetAssoc {

template < class BaseSet >
class LowAssociativeMRUSet : public BaseSet {
public:

  // convenience typedefs
  typedef typename BaseSet::Tag Tag;
  typedef typename BaseSet::BlockNumber BlockNumber;
  typedef typename BaseSet::Index Index;
  typedef typename BaseSet::Block Block;

  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const uint32_t version) {
    ar & boost::serialization::base_object<BaseSet>(*this);
    for (uint32_t i = 0; i < this->myAssoc; ++i) {
      ar & mruList[i];
    }
  }

  LowAssociativeMRUSet(const uint32_t anAssoc,
                       const Index anIndex)
    : BaseSet(anAssoc, anIndex)
    , mruList(0) {
    init();
  }

  void init() {
    uint32_t ii;
    mruList = (BlockNumber *)( new char[this->myAssoc * sizeof(BlockNumber)] );
    // this uses placement new, so on delete, make sure to manually call
    // the destructor for each BlockNumber, then delete[] the array
    for (ii = 0; ii < this->myAssoc; ii++) {
      new(mruList + ii) BlockNumber(ii);
    }
  }

  // returns the most recently used block
  BlockNumber listHead() {
    return mruList[0];
  }

  // returns the least recently used block
  BlockNumber listTail() {
    return mruList[this->myAssoc-1];
  }

  // moves the indicated block to the MRU position
  void moveToHead(BlockNumber aNumber) {
    int32_t ii = 0;
    // find the list entry for the specified index
    while (mruList[ii] != aNumber) {
      ii++;
    }
    // move appropriate entries down the MRU chain
    while (ii > 0) {
      mruList[ii] = mruList[ii-1];
      ii--;
    }
    mruList[0] = aNumber;
  }

  // moves the indicated block to the LRU position
  void moveToTail(BlockNumber aNumber) {
    uint32_t ii = 0;
    // find the list entry for the specified index
    while (mruList[ii] != aNumber) {
      ii++;
    }
    // move appropriate entries up the MRU chain
    while (ii < this->myAssoc - 1) {
      mruList[ii] = mruList[ii+1];
      ii++;
    }
    mruList[this->myAssoc-1] = aNumber;
  }

  // The list itself.  This is an array of indices corresponding to
  // blocks in the set.  The list is arranged in order of most recently
  // used to least recently used.
  BlockNumber * mruList;

};  // end struct MruSetExtension

}  // end namespace nSetAssoc

#endif //SET_ASSOC_REPLACEMENT_HPP_
