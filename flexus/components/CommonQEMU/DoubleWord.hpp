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
#ifndef FLEXUS_COMMON__DOUBLE_WORD_HPP_INCLUDED
#define FLEXUS_COMMON__DOUBLE_WORD_HPP_INCLUDED

#include <core/debug/debug.hpp>

namespace Flexus {
namespace SharedTypes {

struct DoubleWord {
  uint64_t theDoubleWord;
  uint8_t theValidBits;

  DoubleWord()
    : theDoubleWord(0xEAEAEAEA)
    , theValidBits(0)
  {}

  DoubleWord(uint64_t aULL)
    : theDoubleWord(aULL)
    , theValidBits(0xFF)
  {}

  DoubleWord(uint64_t aBase, DoubleWord const & anApplyVal)
    : theDoubleWord(aBase)
    , theValidBits(0xFF) {
    apply(anApplyVal);
  }

  void set(uint64_t aValue, int32_t aSize, uint32_t anOffset) {
    switch (aSize) {
      case 1:
        getByte( anOffset ) = aValue;
        theValidBits |= (0x1 << (7 - anOffset));
        break;
      case 2:
        getHalfWord( anOffset ) = aValue;
        theValidBits |= (0x3 << (6 - anOffset));
        break;
      case 4:
        getWord( anOffset ) = aValue;
        theValidBits |= (0xF << (4 - anOffset));
        break;
      case 8:
        getDoubleWord( anOffset ) = aValue;
        theValidBits = 0xFF;
        break;
      default:
        DBG_Assert(false, ( << "Unsupported size for DoubleWord::set") );
    }
  }

  bool isValid(int32_t aSize, uint32_t anOffset) const {
    uint8_t bits = 0;
    switch (aSize) {
      case 1:
        bits = 0x1 << (7 - anOffset);
        break;
      case 2:
        bits = 0x3 << (6 - anOffset);
        break;
      case 4:
        bits = 0xF << (4 - anOffset);
        break;
      case 8:
        bits = 0xFF;
        break;
      default:
        return false;
    }
    bool ret_val = (theValidBits & bits) == bits;
    return ret_val;
  }

  bool isEqual(uint64_t aValue, int32_t aSize, uint32_t anOffset) {
    if (! isValid(aSize, anOffset)) {
      return false;
    }
    switch (aSize) {
      case 1:
        return getByte( anOffset ) == static_cast<uint8_t>(aValue);
      case 2:
        return getHalfWord( anOffset ) == static_cast<uint16_t>(aValue);
      case 4:
        return getWord( anOffset ) == static_cast<uint32_t>(aValue);
      case 8:
        return getDoubleWord( anOffset ) == aValue;
      default:
        DBG_Assert(false, ( << "Unsupported size for DoubleWord::set") );
        return false;
    }
  }

  uint8_t & getByte( uint32_t anOffset) {
    return *( reinterpret_cast< uint8_t *>(&theDoubleWord) + (7 - anOffset) );
  }
  uint16_t & getHalfWord( uint32_t anOffset) {
    return *( reinterpret_cast< uint16_t *>(&theDoubleWord) + (3 - (anOffset >> 1)) );
  }
  uint32_t & getWord( uint32_t anOffset) {
    return *( reinterpret_cast< uint32_t *>(&theDoubleWord) + (1 - (anOffset >> 2) ) );
  }
  uint64_t & getDoubleWord( uint32_t anOffset /* ignored */) {
    DBG_Assert(anOffset == 0);
    return theDoubleWord;
  }

  void apply( DoubleWord const & aMask) {
    if (aMask.theValidBits != 0) {
      uint64_t masked_val = 0;
      for ( int32_t i = 7; i >= 0; --i) {
        masked_val <<= 8;
        if (aMask.theValidBits & (1 << i)) {
          theValidBits |= (1 << i);
          masked_val |= ((aMask.theDoubleWord >> (8 * i )) & 0xFF);
        } else {
          masked_val |= ((theDoubleWord >> (8 * i )) & 0xFF);
        }
      }
      theDoubleWord = masked_val;
    }
  }

};

bool operator== (uint64_t compare, DoubleWord const & aDoubleWord);
bool operator== (DoubleWord const & entry, uint64_t compare);
std::ostream & operator <<(std::ostream & anOstream, DoubleWord const & aDoubleWord);

} //SharedTypes
} //Flexus

#endif //FLEXUS_COMMON__DOUBLE_WORD_HPP_INCLUDED
