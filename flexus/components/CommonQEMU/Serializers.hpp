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
#ifndef _COMMON_SERIALIZERS_HPP_
#define _COMMON_SERIALIZERS_HPP_

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/tracking.hpp>
#include <bitset>
#define MAX_NUM_SHARERS 512

namespace boost { 
namespace serialization { 

template <class Archive, std::size_t size>
inline void save(
  Archive & ar,
  std::bitset<size> const & t,
  const unsigned int /* version */
){
  const std::string bits = t.template to_string<
    std::string::value_type,
    std::string::traits_type,
    std::string::allocator_type
  >();
  ar << BOOST_SERIALIZATION_NVP( bits );
}

template <class Archive, std::size_t size>
inline void load(
  Archive & ar,
  std::bitset<size> & t,
  const unsigned int /* version */
){
  std::string bits;
  ar >> BOOST_SERIALIZATION_NVP( bits );
  t = std::bitset<size>(bits);
}

template <class Archive, std::size_t size>
inline void serialize(
  Archive & ar,
  std::bitset<size> & t,
  const unsigned int version
){
  boost::serialization::split_free( ar, t, version );
}

// don't track bitsets since that would trigger tracking
// all over the program - which probably would be a surprise.
// also, tracking would be hard to implement since, we're
// serialization a representation of the data rather than
// the data itself.

template <std::size_t size>
struct tracking_level<std::bitset<size> >
    : mpl::int_<track_never> {} ;
}; 
};

namespace nCommonSerializers {

struct StdDirEntryExtendedSerializer {
  StdDirEntryExtendedSerializer(uint64_t t = 0, std::bitset<MAX_NUM_SHARERS> s = 0): tag(t), state(s) {} 
  uint64_t tag;
  std::bitset<MAX_NUM_SHARERS> state;
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const uint32_t version){
    ar & tag;
    ar & state;
  }
};

inline std::ostream & operator<<(std::ostream & os, const StdDirEntryExtendedSerializer & serializer) {
  os << "Addr: " << std::hex << serializer.tag;
  os << ", State: ";
  uint64_t partialState =0;
  uint32_t counter = 0;
  for (int32_t i = MAX_NUM_SHARERS -1; i>=0;i--){
    partialState = partialState << 1;
    partialState |= (serializer.state[i] ? 1 : 0);
    if (i % 64 == 0){ 
      if (counter !=0) os << std::hex << partialState;
      partialState = 0;
      counter++;
    }
  }
  return os;
}

struct StdDirEntrySerializer {
  StdDirEntrySerializer(uint64_t t = 0, uint64_t s = 0) : tag(t), state(s) {}

  uint64_t tag;
  uint64_t state;
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const uint32_t version) {
    ar & tag;
    ar & state;
  }
};

inline std::ostream & operator<<(std::ostream & os, const StdDirEntrySerializer & serializer) {
  os << "Addr: " << std::hex << serializer.tag;
  os << ", State: " << std::hex << serializer.state;
  return os;
}

struct RegionDirEntrySerializer {
  RegionDirEntrySerializer(uint64_t t = 0, uint8_t n = 0, int8_t o = -1)
    : tag(t), num_blocks(n), owner(o) {}

  uint64_t tag;
  std::vector<uint64_t> state;
  uint8_t  num_blocks;
  int8_t  owner;
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const uint32_t version) {
    ar & tag;
    ar & num_blocks;
    ar & owner;
    ar & state;
  }
};

inline std::ostream & operator<<(std::ostream & os, const RegionDirEntrySerializer & serializer) {
  os << std::hex << serializer.tag;
  os << std::dec << ": owner = " << (int)serializer.owner << ", blocks = ";
  for (uint32_t i = 0; i < serializer.num_blocks; i++) {
    os << std::hex << "<" << serializer.state[i] << ">";
  }
  return os;
}

class SerializableRTDirEntry {
private:
  int64_t   theAddress;
  std::vector<int8_t>     theWays;

  SerializableRTDirEntry() { }
public:
  int8_t owner;
  bool shared;

  SerializableRTDirEntry(int32_t num_blocks) : theAddress(0), theWays(num_blocks, -1), owner(-1), shared(true) {}
  SerializableRTDirEntry(const SerializableRTDirEntry & entry)
    : theAddress(entry.theAddress), theWays(entry.theWays), owner(entry.owner), shared(entry.shared) {}

  const int64_t & tag() {
    return theAddress;
  }

  void reset(Flexus::SharedTypes::PhysicalMemoryAddress addr) {
    theAddress = addr;
    std::for_each(theWays.begin(), theWays.end(), boost::lambda::_1 = -1);
    owner = -1;
    shared = true;
  }

  int8_t & operator[](int32_t offset) {
    return theWays[offset];
  }

  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const uint32_t version) {
    ar & theAddress;
    ar & theWays;
    ar & owner;
    ar & shared;
    DBG_(Trace, ( << "serialize Region: " << std::hex << theAddress << " owner = " << std::dec << (int)owner << (shared ? " Shared" : " Non-Shared") ));
  }
};

struct BlockSerializer {
  uint64_t    tag;
  uint8_t     way;
  uint8_t     state;

  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const uint32_t version) {
    ar & tag;
    ar & state;
  }
};

}; // namespace nCommonSerializers

BOOST_CLASS_TRACKING(nCommonSerializers::StdDirEntrySerializer, boost::serialization::track_never);
BOOST_CLASS_TRACKING(nCommonSerializers::RegionDirEntrySerializer, boost::serialization::track_never);
BOOST_CLASS_TRACKING(nCommonSerializers::SerializableRTDirEntry, boost::serialization::track_never);
BOOST_CLASS_TRACKING(nCommonSerializers::BlockSerializer, boost::serialization::track_never);

#endif // ! _COMMON_SERIALIZERS_HPP_
