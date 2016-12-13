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
#ifndef __BLOCK_DIRECTORY_ENTRY_HPP__
#define __BLOCK_DIRECTORY_ENTRY_HPP__

#include <components/CommonQEMU/Serializers.hpp>

namespace nFastCMPCache {

using nCommonSerializers::StdDirEntrySerializer;

class BlockDirectoryEntry : public AbstractDirectoryEntry {
private:
  PhysicalMemoryAddress theAddress;
  SharingVector theSharers;
  SharingState theState;

public:
  BlockDirectoryEntry(PhysicalMemoryAddress addr) : theAddress(addr), theState(ZeroSharers) {}
  BlockDirectoryEntry() : theAddress(0), theState(ZeroSharers) {}
  virtual ~BlockDirectoryEntry() {}

  const SharingVector & sharers() const {
    return theSharers;
  }
  const SharingState & state() const {
    return theState;
  }
  const PhysicalMemoryAddress & tag() const {
    return theAddress;
  }

  inline void addSharer(int32_t index) {
    theSharers.addSharer(index);
    if (theSharers.countSharers() == 1) {
      theState = OneSharer;
    } else {
      theState = ManySharers;
    }
  }

  inline void makeExclusive(int32_t index) {
    DBG_Assert(theSharers.isSharer(index), ( << "Core " << index << " is not a sharer " << theSharers.getSharers() ) );
    DBG_Assert(theSharers.countSharers() == 1, ( << "Cannot make xclusive, sharers = " << theSharers ));
    theState = OneSharer;
  }

  inline void removeSharer(int32_t index) {
    theSharers.removeSharer(index);
    if (theSharers.countSharers() == 0) {
      theState = ZeroSharers;
    } else if (theSharers.countSharers() == 1) {
      theState = OneSharer;
    }
  }
  void setSharers(const SharingVector & new_sharers) {
    theSharers = new_sharers;
    int32_t count = theSharers.countSharers();
    if (count == 1) {
      theState = OneSharer;
    } else if (count == 0) {
      theState = ZeroSharers;
    } else {
      theState = ManySharers;
    }
  }
  void setSharers(uint64_t new_sharers) {
    theSharers.setSharers(new_sharers);
    int32_t count = theSharers.countSharers();
    if (count == 1) {
      theState = OneSharer;
    } else if (count == 0) {
      theState = ZeroSharers;
    } else {
      theState = ManySharers;
    }
  }

  inline void clear() {
    theSharers.clear();
    theState = ZeroSharers;
  }

  inline void reset(PhysicalMemoryAddress addr) {
    theAddress = addr;
    theSharers.clear();
    theState = ZeroSharers;
  }

  BlockDirectoryEntry & operator&=(const BlockDirectoryEntry & entry) {
    theSharers &= entry.theSharers;
    int32_t count = theSharers.countSharers();
    if (count == 1) {
      theState = OneSharer;
    } else if (count == 0) {
      theState = ZeroSharers;
    } else {
      theState = ManySharers;
    }
    return *this;
  }

  StdDirEntrySerializer getSerializer() {
    return StdDirEntrySerializer((uint64_t)theAddress, theSharers.getUInt64());
  }

  void operator=(const StdDirEntrySerializer & serializer) {
    theAddress = PhysicalMemoryAddress(serializer.tag);
    theSharers.setSharers(serializer.state);
    int32_t count = theSharers.countSharers();
    if (count == 1) {
      theState = OneSharer;
    } else if (count == 0) {
      theState = ZeroSharers;
    } else {
      theState = ManySharers;
    }
  }

};

typedef boost::intrusive_ptr<BlockDirectoryEntry> BlockDirectoryEntry_p;

struct BlockEntryWrapper : public AbstractDirectoryEntry {
  BlockEntryWrapper(BlockDirectoryEntry & block) : block(block) {}
  BlockDirectoryEntry & block;

  BlockDirectoryEntry * operator->() const {
    return &block;
  }
  BlockDirectoryEntry & operator*() const {
    return block;
  }
};

typedef boost::intrusive_ptr<BlockEntryWrapper> BlockEntryWrapper_p;

};

#endif //! __BLOCK_DIRECTORY_ENTRY_HPP__
