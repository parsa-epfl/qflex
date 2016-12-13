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
#ifndef SET_ASSOC_SET_DEFN_HPP_
#define SET_ASSOC_SET_DEFN_HPP_

namespace nSetAssoc {

template
< class Types
, class TBlock
>
class BaseSetDefinition {
  //Types must provide
  // a Tag typedef
  // a BlockNumber typedef
  // an Index typedef
  //TBlock should be the block type.
  //It must supply at least these functions:
  //Default-constructor
  //

  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const uint32_t version) {
    ar & myAssoc;
    ar & myIndex;
    for (uint32_t i = 0; i < myAssoc; ++i) {
      ar & theBlocks[i];
    }
  }

public:

  // convenience typedefs
  typedef typename Types::Tag Tag;
  typedef typename Types::BlockNumber BlockNumber;
  typedef typename Types::Index Index;

  // allow others to extract the block type
  typedef TBlock Block;

  // CONSTRUCTION
  BaseSetDefinition(const uint32_t anAssoc,
                    const Index anIndex)
    : myIndex(anIndex)
    , myAssoc(anAssoc) {
    uint32_t ii;
    theBlocks = (Block *)( new char[myAssoc * sizeof(Block)] );
    // this uses placement new, so on delete, make sure to manually call
    // the destructor for each Block, then delete[] the array
    for (ii = 0; ii < myAssoc; ii++) {
      new(theBlocks + ii) Block();  //No corresponding delete yet
    }
  }

  // BLOCK LOOKUP
  Block & operator[] (const BlockNumber & aBlockNumber) {
    // TODO: bounds checking/assertion?
    return theBlocks[aBlockNumber];
  }
  const Block & operator[] (const BlockNumber & aBlockNumber) const {
    // TODO: bounds checking/assertion?
    return theBlocks[aBlockNumber];
  }

  // INDEXING
  BlockNumber indexOf(const Block & aBlock) const {
    int32_t index = &aBlock - theBlocks;
    DBG_Assert((index >= 0) && (index < (int)myAssoc), ( << "index=" << index ) );
    return BlockNumber(index);
  }
  Index index() const {
    return myIndex;
  }

  // ITERATION
  typedef Block * BlockIterator;

  BlockIterator blocks_begin() {
    return theBlocks;
  }
  const BlockIterator blocks_begin() const {
    return theBlocks;
  }
  BlockIterator blocks_end() {
    return theBlocks + myAssoc;
  }
  const BlockIterator blocks_end() const {
    return theBlocks + myAssoc;
  }

  friend std::ostream & operator << (std::ostream & anOstream, const BaseSetDefinition<Types, TBlock> & aSet) {
    anOstream << "set " << aSet.myIndex << ": ";
    for (BlockIterator iter = aSet.blocks_begin(); iter != aSet.blocks_end(); ++iter) {
      anOstream << *iter;
    }
    return anOstream;
  }

protected:
  // The index of this set
  Index myIndex;

  // The associativity of this set
  uint32_t myAssoc;

  // The actual blocks that belong to this set
  Block * theBlocks;

};  // end BaseSetDefinition

}  // end namespace nSetAssoc

#endif
