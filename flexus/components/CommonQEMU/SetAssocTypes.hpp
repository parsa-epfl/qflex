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
#ifndef SET_ASSOC_TYPES_HPP_
#define SET_ASSOC_TYPES_HPP_

namespace nSetAssoc {

template<class UnderlyingType>
class SimpleTag {
public:
  //Underlying type must be:
  //Assignable
  //Copy-constructible
  //Default-constructible
  //Comparable
  //writable to an ostream

  SimpleTag () {} //Can only be used if UnderlyingType is default-constructable
  explicit SimpleTag(UnderlyingType newTag)
    : tag(newTag)
  {}
  SimpleTag(const SimpleTag & aTag)
    : tag(aTag.tag)
  {}
  SimpleTag & operator= (const SimpleTag & aTag) {
    tag = aTag.tag;
    return *this;
  }
  operator UnderlyingType () const {
    return tag;
  }
  bool operator ==(SimpleTag const & anOther) const {
    return tag == anOther.tag;
  }
  friend std::ostream & operator <<(std::ostream & anOstream, SimpleTag const & aTag) {
    anOstream << aTag.tag;
    return anOstream;
  }

  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const uint32_t version) {
    ar & tag;
  }

private:
  UnderlyingType tag;
};

class SimpleIndex  {
public:
  explicit SimpleIndex (int32_t newIndex)
    : index(newIndex)
  {}
  SimpleIndex(const SimpleIndex & anIndex)
    : index(anIndex.index)
  {}
  SimpleIndex & operator= (const SimpleIndex & anIndex) {
    index = anIndex.index;
    return *this;
  }
  operator int32_t () const {
    return index;
  }
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const uint32_t version) {
    ar & index;
  }

private:
  int32_t index;
};

class SimpleBlockNumber {
public:
  explicit SimpleBlockNumber(int32_t newNumber)
    : number(newNumber)
  {}
  SimpleBlockNumber(const SimpleBlockNumber & aBlockNumber)
    : number(aBlockNumber.number)
  {}
  SimpleBlockNumber & operator= (const SimpleBlockNumber & aBlockNumber) {
    number = aBlockNumber.number;
    return *this;
  }
  SimpleBlockNumber & operator= (int32_t newNumber) {
    number = newNumber;
    return *this;
  }
  operator int32_t () const {
    return number;
  }
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const uint32_t version) {
    ar & number;
  }

private:
  int32_t number;
};

}  // end namespace nSetAssoc

#endif
