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
#ifndef FLEXUS_SLICES__DESTINATION_MESSAGE_HPP_INCLUDED
#define FLEXUS_SLICES__DESTINATION_MESSAGE_HPP_INCLUDED

#include <list>
#include <iostream>
#include <core/boost_extensions/intrusive_ptr.hpp>
#include <core/types.hpp>

#ifdef FLEXUS_DestinationMessage_TYPE_PROVIDED
#error "Only one component may provide the Flexus::SharedTypes::DestinationMessage data type"
#endif
#define FLEXUS_DestinationMessage_TYPE_PROVIDED

namespace Flexus {
namespace SharedTypes {

typedef Flexus::SharedTypes::PhysicalMemoryAddress DestinationAddress;

struct DestinationMessage : public boost::counted_base {

  enum DestinationType {
    Requester,
    Directory,
    RemoteDirectory,
    Source,
    Memory,
    Multicast,
    Other
  };

  DestinationMessage(DestinationType aType)
    : type(aType)
    , requester(-1)
    , directory(-1)
    , remote_directory(-1)
    , source(-1)
    , memory(-1)
    , other(-1)
    , to_i_cache(false)
    , from_i_cache(false)
  { }

  DestinationMessage(const DestinationMessage & msg)
    : type(msg.type)
    , requester(msg.requester)
    , directory(msg.directory)
    , remote_directory(msg.remote_directory)
    , source(msg.source)
    , memory(msg.memory)
    , multicast_list(msg.multicast_list)
    , other(msg.other)
    , to_i_cache(msg.to_i_cache)
    , from_i_cache(msg.from_i_cache)
  {}

  DestinationMessage(const DestinationMessage * msg)
    : type(msg->type)
    , requester(msg->requester)
    , directory(msg->directory)
    , remote_directory(msg->remote_directory)
    , source(msg->source)
    , memory(msg->memory)
    , multicast_list(msg->multicast_list)
    , other(msg->other)
    , to_i_cache(msg->to_i_cache)
    , from_i_cache(msg->from_i_cache)
  {}

  DestinationMessage(boost::intrusive_ptr<DestinationMessage> msg)
    : type(msg->type)
    , requester(msg->requester)
    , directory(msg->directory)
    , remote_directory(msg->remote_directory)
    , source(msg->source)
    , memory(msg->memory)
    , multicast_list(msg->multicast_list)
    , other(msg->other)
    , to_i_cache(msg->to_i_cache)
    , from_i_cache(msg->from_i_cache)
  {}

  DestinationType type;
  int32_t requester;
  int32_t directory;
  int32_t remote_directory;
  int32_t source;
  int32_t memory;
  std::list<int> multicast_list;
  int32_t other;
  bool to_i_cache;
  bool from_i_cache;

  inline bool isMultipleMsgs() const {
    return ((type == Multicast) && (multicast_list.size() > 1));
  }

  typedef boost::intrusive_ptr<DestinationMessage> DestinationMessage_p;

  DestinationMessage_p removeFirstMulticastDest() {
    DestinationMessage_p ret(new DestinationMessage(this));
    ret->multicast_list.clear();
    ret->other = multicast_list.front();
    ret->type = Other;
    multicast_list.pop_front();
    return ret;
  }

  void convertMulticast() {
    other = multicast_list.front();
    type = Other;
    multicast_list.pop_front();
  }

};

typedef boost::intrusive_ptr<DestinationMessage> DestinationMessage_p;

inline std::ostream & operator<< (std::ostream & aStream, const DestinationMessage & msg) {
  aStream << "DestMsg: type=";
  switch (msg.type) {
    case DestinationMessage::Requester:
      aStream << "Requester, ";
      break;
    case DestinationMessage::Directory:
      aStream << "Directory, ";
      break;
    case DestinationMessage::RemoteDirectory:
      aStream << "RemoteDirectory, ";
      break;
    case DestinationMessage::Source:
      aStream << "Source, ";
      break;
    case DestinationMessage::Memory:
      aStream << "Memory, ";
      break;
    case DestinationMessage::Multicast:
      aStream << "Multicast, ";
      break;
    case DestinationMessage::Other:
      aStream << "Other, ";
      break;
  }
  aStream << "Requester = " << msg.requester << ", ";
  aStream << "Directory = " << msg.directory << ", ";
  aStream << "RemoteDirectory = " << msg.remote_directory << ", ";
  aStream << "Source = " << msg.source << ", ";
  aStream << "Memory = " << msg.memory << ", ";
  aStream << "Multicast = (";
  for (std::list<int>::const_iterator iter = msg.multicast_list.begin(); iter != msg.multicast_list.end(); iter++) {
    aStream  << (*iter) << ",";
  }
  aStream << "), ";
  aStream << "Other = " << msg.other << ", ";
  aStream << "To ICache = " << std::boolalpha << msg.to_i_cache << ", ";
  aStream << "From ICache = " << std::boolalpha << msg.to_i_cache;

  return aStream;
}

} //End SharedTypes
} //End Flexus

#endif //FLEXUS_SLICES__DESTINATION_MESSAGE_HPP_INCLUDED
