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
#ifndef FLEXUS_SLICES__DIRECTORY_ENTRY_HPP_INCLUDED
#define FLEXUS_SLICES__DIRECTORY_ENTRY_HPP_INCLUDED

#include <iostream>
#include <boost/serialization/version.hpp>
#include <boost/serialization/split_member.hpp>
#include <core/boost_extensions/intrusive_ptr.hpp>
#include <core/types.hpp>
#include <core/debug/debug.hpp>

#ifdef FLEXUS_DirectoryEntry_TYPE_PROVIDED
#error "Only one component may provide the Flexus::SharedTypes::DirectoryEntry data type"
#endif
#define FLEXUS_DirectoryEntry_TYPE_PROVIDED

namespace Flexus {
namespace SharedTypes {

enum tDirState {
  DIR_STATE_INVALID  = 0,
  DIR_STATE_SHARED   = 1,
  DIR_STATE_MODIFIED = 2
};

std::ostream & operator << (std::ostream & anOstream, tDirState const x);

typedef uint32_t node_id_t;

////////////////////////////////
//
// definition of the directory entry type
//
class DirectoryEntry : public boost::counted_base {
public:
  static const uint64_t kPastReaders = 0xFFFFFFFFFFFFFFFFULL;
  static const uint32_t kWasModified = 0x10000UL;
  static const uint32_t kStateMask = 0xC0000UL;
  static const uint32_t kStateShift = 18;

private:
  //data members
  uint8_t theState;
  bool theWasModified;
  uint64_t thePastReaders;
  uint64_t theNodes;

  friend class boost::serialization::access;
  template<class Archive>
  void save(Archive & ar, const uint32_t version) const {
    ar & theState;
    ar & theWasModified;
    ar & thePastReaders;
    ar & theNodes;
  }

  template<class Archive>
  void load(Archive & ar, const uint32_t version) {
    switch (version) {
      case 0:
        uint32_t tempEncodedState;
        uint16_t tempNodes;

        ar & tempEncodedState;
        theState = (tempEncodedState & kStateMask) >> kStateShift;
        theWasModified = (tempEncodedState & kWasModified);
        thePastReaders = (tempEncodedState & 0xFFFFUL);
        ar & tempNodes;
        theNodes = (uint64_t)tempNodes;
        break;
      case 1:
        ar & theState;
        ar & theWasModified;
        ar & thePastReaders;
        ar & theNodes;
        break;
      default:
        DBG_Assert(0, ( << "Unknown version for directory entry: " << version));
        break;
    }
  }
  BOOST_SERIALIZATION_SPLIT_MEMBER()

  void assertStateValid() const {
    int32_t state = getState();
    DBG_Assert( state == DIR_STATE_INVALID || state == DIR_STATE_SHARED || state == DIR_STATE_MODIFIED );
  }
  tDirState getState() const {
    return tDirState(theState);
  }

public:

  DirectoryEntry()
    : theState(DIR_STATE_INVALID)
    , theWasModified(false)
    , thePastReaders(0)
    , theNodes(0)
  { }
  DirectoryEntry(const DirectoryEntry & oldEntry)
    : theState (oldEntry.theState)
    , theWasModified (oldEntry.theWasModified)
    , thePastReaders (oldEntry.thePastReaders)
    , theNodes (oldEntry.theNodes)
  { }

  DirectoryEntry & operator= (DirectoryEntry const & oldEntry) {
    theState = oldEntry.theState;
    theWasModified = oldEntry.theWasModified;
    thePastReaders = oldEntry.thePastReaders;
    theNodes = oldEntry.theNodes;
    return *this;
  }

  friend std::ostream & operator << (std::ostream & aStream, DirectoryEntry const & anEntry);

  tDirState state() const {
    assertStateValid();
    return getState();
  }
  node_id_t owner() const {
    DBG_Assert( getState() == DIR_STATE_MODIFIED );
    DBG_Assert( theNodes < 64 );
    return theNodes;
  }
  uint64_t sharers() const {
    DBG_Assert( getState() != DIR_STATE_MODIFIED);
    return theNodes;
  }
  bool isSharer(node_id_t aNode) const {
    DBG_Assert( getState() != DIR_STATE_MODIFIED );
    return theNodes & (1ULL << (uint64_t)aNode);
  }
  bool wasSharer(node_id_t aNode) const {
    return getPastReaders() & (1ULL << (uint64_t)aNode);
  }

  void markModified() {
    theWasModified = true;
  }
  bool wasModified() const {
    return theWasModified;
  }
  void setState(tDirState aState) {
    theState = aState;
  }
  void setOwner(node_id_t anOwner) {
    DBG_Assert(getState() == DIR_STATE_MODIFIED);
    thePastReaders |= 1ULL << (uint64_t)anOwner;
    theNodes = anOwner;  // BTG: why don't we continue the one-hot encoding??
  }
  void setSharer(node_id_t aNode) {
    DBG_Assert(getState() != DIR_STATE_MODIFIED);
    thePastReaders |= 1ULL << (uint64_t)aNode;
    theNodes |= 1ULL << (uint64_t)aNode;
  }
  void setSharers(uint64_t aVal) {
    DBG_Assert(getState() != DIR_STATE_MODIFIED);
    thePastReaders |= aVal;
    theNodes = aVal;
  }
  void clearSharer(node_id_t aNode) {
    DBG_Assert(getState() != DIR_STATE_MODIFIED);
    theNodes &= ~(1ULL << (uint64_t)aNode);
  }
  void clearSharers() {
    DBG_Assert(getState() != DIR_STATE_MODIFIED);
    theNodes = 0;
  }
  void setAllSharers() {
    DBG_Assert(getState() != DIR_STATE_MODIFIED);
    thePastReaders |= kPastReaders;
    theNodes |= kPastReaders;
  }

  uint64_t getPastReaders() const {
    return thePastReaders;
  }
  void setPastReaders (uint64_t s) {
    thePastReaders = s;
  }
};

} //End SharedTypes
} //End Flexus

BOOST_CLASS_VERSION(Flexus::SharedTypes::DirectoryEntry, 1)  // all new checkpoints get version #1

#endif //FLEXUS_SLICES__DIRECTORY_ENTRY_HPP_INCLUDED
