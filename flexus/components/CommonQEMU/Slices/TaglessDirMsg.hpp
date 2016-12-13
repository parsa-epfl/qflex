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
#ifndef FLEXUS_SLICES__TAGLESS_DIR_MSG_HPP_INCLUDED
#define FLEXUS_SLICES__TAGLESS_DIR_MSG_HPP_INCLUDED

#include <iostream>
#include <core/boost_extensions/intrusive_ptr.hpp>
#include <core/types.hpp>
#include <core/component.hpp>

#include <components/CommonQEMU/GlobalHasher.hpp>
#include <components/CommonQEMU/Util.hpp>

#ifdef FLEXUS_TaglessDirMsg_TYPE_PROVIDED
#error "Only one component may provide the Flexus::SharedTypes::TaglessDirMsg data type"
#endif
#define FLEXUS_TaglessDirMsg_TYPE_PROVIDED

namespace Flexus {
namespace SharedTypes {

typedef Flexus::SharedTypes::PhysicalMemoryAddress DestinationAddress;

struct TaglessDirMsg : public boost::counted_base {

  TaglessDirMsg()
  { }

  TaglessDirMsg(const TaglessDirMsg & msg)
    : theConflictFreeBuckets(msg.theConflictFreeBuckets)
  {}

  TaglessDirMsg(const TaglessDirMsg * msg)
    : theConflictFreeBuckets(msg->theConflictFreeBuckets)
  {}

  TaglessDirMsg(boost::intrusive_ptr<TaglessDirMsg> msg)
    : theConflictFreeBuckets(msg->theConflictFreeBuckets)
  {}

  std::map<int, std::set<int> > theConflictFreeBuckets;

  void addConflictFreeSet(int32_t core, std::set<int> &buckets) {
    theConflictFreeBuckets.insert(std::make_pair(core, buckets));
  }

  const std::map<int, std::set<int> >& getConflictFreeBuckets() const {
    return theConflictFreeBuckets;
  }

  void merge(boost::intrusive_ptr<TaglessDirMsg> msg) {
    theConflictFreeBuckets.insert(msg->theConflictFreeBuckets.begin(), msg->theConflictFreeBuckets.end());
  }

  int32_t messageSize() {
    int32_t map_entries = theConflictFreeBuckets.size();

    // If there's zero or one set of buckets, assume we have room in the control packet.
    if (map_entries < 2) {
      return 0;
    }

    int32_t core_id_bits = nCommonUtil::log_base2(Flexus::Core::ComponentManager::getComponentManager().systemWidth());
    int32_t num_hashes = nGlobalHasher::GlobalHasher::theHasher().numHashes();

    // Total size = # of entries (max = num cores - 1 -> need core_id_bits)
    //    + num_entries * entry size
    // Entry size = core_id + bucket bitmap
    int32_t total_bits = core_id_bits + map_entries * (core_id_bits + num_hashes);

    // Convert to number of bytes (round up)
    return ((total_bits + 7) / 8);
  }

};

typedef boost::intrusive_ptr<TaglessDirMsg> TaglessDirMsg_p;

inline std::ostream & operator<< (std::ostream & aStream, const TaglessDirMsg & msg) {

  aStream << "TaglessDirMsg: Conflict Free Buckets = ";
  std::map<int, std::set<int> >::const_iterator map_iter = msg.theConflictFreeBuckets.begin();
  for (; map_iter != msg.theConflictFreeBuckets.end(); map_iter++) {
    aStream << "Core " << map_iter->first << " = {";
    std::set<int>::iterator set_iter = map_iter->second.begin();
    bool needs_comma = false;
    for (; set_iter != map_iter->second.end(); set_iter++) {
      if (needs_comma) {
        aStream << ", ";
      }
      aStream << (*set_iter);
    }
    aStream << "},";
  }

  return aStream;
}

} //End SharedTypes
} //End Flexus

#endif //FLEXUS_SLICES__TAGLESS_DIR_MSG_HPP_INCLUDED
