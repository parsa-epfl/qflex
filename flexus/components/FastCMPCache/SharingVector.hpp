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
#ifndef __FASTCMPCACHE_SHARINGVECTOR_HPP__
#define __FASTCMPCACHE_SHARINGVECTOR_HPP__

#include <bitset>
#include <list>

namespace nFastCMPCache {

#define MAX_NUM_SHARERS 512

class SharingVector {
protected:
  std::bitset<MAX_NUM_SHARERS> sharers;

  SharingVector(const std::bitset<MAX_NUM_SHARERS> &s) : sharers(s) {}

public:
  virtual ~SharingVector() {}

  SharingVector() {
    sharers.reset();
  }

  virtual void addSharer(int32_t index) {
    DBG_Assert( (index >= 0) && (index < MAX_NUM_SHARERS), ( << "Invalid index " << index ));
    sharers[index] = true;
  }

  virtual void removeSharer(int32_t index) {
    DBG_Assert( (index >= 0) && (index < MAX_NUM_SHARERS), ( << "Invalid index " << index ));
    sharers[index] = false;
  }

  virtual int32_t countSharers() const {
    return sharers.count();
  }

  virtual bool isSharer(int32_t index) const {
    DBG_Assert( (index >= 0) && (index < MAX_NUM_SHARERS), ( << "Invalid index " << index ));
    return sharers[index];
  }

  virtual int32_t getFirstSharer() const {
    for (int32_t i = 0; i < MAX_NUM_SHARERS; i++) {
      if (sharers[i]) {
        return i;
      }
    }
    return -1;
  }

  int32_t getClosestSharer(int32_t index) const {
    int32_t lhs = index + 1;
    int32_t rhs = index - 1;
    for (; lhs < MAX_NUM_SHARERS; lhs++) {
      if (sharers[lhs]) break;
    }
    for (; rhs >= 0; rhs--) {
      if (sharers[rhs]) break;
    }
    if (rhs >= 0) {
      if (lhs < MAX_NUM_SHARERS) {
        if ((index - rhs) < (lhs - index)) {
          return rhs;
        } else {
          return lhs;
        }
      } else {
        return rhs;
      }
    } else if (lhs < MAX_NUM_SHARERS) {
      return lhs;
    } else if (sharers[index]) {
      return index;
    }
    return -1;
  }

  virtual std::list<int> toList() const {
    std::list<int> l;
    for (int32_t i = 0; i < MAX_NUM_SHARERS; i++) {
      if (sharers[i]) {
        l.push_back(i);
      }
    }
    return l;
  }

  virtual const std::bitset<MAX_NUM_SHARERS>& getSharers() const {
    return sharers;
  }

  virtual const bool operator==(const SharingVector & a) const {
    return (sharers == a.sharers);
  }

  virtual const bool operator!=(const SharingVector & a) const {
    return (sharers != a.sharers);
  }

  virtual const SharingVector operator&(const SharingVector & a) const {
    return SharingVector(sharers & a.sharers);
  }

  virtual SharingVector & operator=(const SharingVector & a) {
    sharers = a.sharers;
    return *this;
  }

  virtual SharingVector & operator&=(const SharingVector & a) {
    sharers &= a.sharers;
    return *this;
  }

  virtual SharingVector & operator|=(const SharingVector & a) {
    sharers |= a.sharers;
    return *this;
  }

  virtual void clear() {
    sharers.reset();
  }

  virtual void flip() {
    sharers.flip();
  }

  virtual const bool any() const {
    return sharers.any();
  }

  virtual uint64_t getUInt64() const {
    uint64_t ret = 0;
    for (int32_t i = 63; i >= 0; i--) {
      ret = ret << 1;
      ret |= (sharers[i] ? 1 : 0);
    }
    return ret;
  }

  virtual void setSharers(std::bitset<MAX_NUM_SHARERS> s){
    for (int32_t i=0; i< MAX_NUM_SHARERS; i++)
      sharers[i]=s[i]; 
  }

//  virtual void setSharers(uint64_t s) {
//#if MAX_NUM_SHARERS > 64
//#error "MAX_NUM_SHARERS must be less than or equal to 64, or you just re-write some functions
//#endif
//    for (int32_t i = 0; i < 64; i++, s >>= 1) {
//      sharers[i] = (s & 1);
//    }
//  }
};

inline std::ostream & operator<<(std::ostream & os, const SharingVector & sharers) {
  os << sharers.getSharers();
  return os;
}

}; // namespace

#endif // __FASTCMPCACHE_SHARINGVECTOR_HPP__
