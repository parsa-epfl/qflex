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
#ifndef FLEXUS_COMPONENTS_MEMORYMAP_HPP__INCLUDED
#define FLEXUS_COMPONENTS_MEMORYMAP_HPP__INCLUDED

#include <core/types.hpp>
#include <core/boost_extensions/intrusive_ptr.hpp>

namespace Flexus {
namespace SharedTypes {

typedef Flexus::Core::index_t node_id_t;

struct MemoryMap : public boost::counted_base {
  static boost::intrusive_ptr<MemoryMap> getMemoryMap();
  static boost::intrusive_ptr<MemoryMap> getMemoryMap(Flexus::Core::index_t aRequestingNode);

  enum AccessType {
    Read
    , Write
    , NumAccessTypes /* Must be last */
  };

  static std::string const & AccessType_toString(AccessType anAccessType) {
    static std::string access_type_names[] = {
      "Read "
      , "Write"
    };
    //xyzzy
    DBG_Assert((anAccessType < NumAccessTypes));
    return access_type_names[anAccessType];
  }

  virtual bool isCacheable(Flexus::SharedTypes::PhysicalMemoryAddress const &) = 0;
  virtual bool isMemory(Flexus::SharedTypes::PhysicalMemoryAddress const &) = 0;
  virtual bool isIO(Flexus::SharedTypes::PhysicalMemoryAddress const &) = 0;
  virtual node_id_t node(Flexus::SharedTypes::PhysicalMemoryAddress const &) = 0;
  virtual void loadState(std::string const &) = 0;
  virtual void recordAccess(Flexus::SharedTypes::PhysicalMemoryAddress const &, AccessType anAccessType) = 0;

};

} //SharedTypes
} //Flexus

#endif //FLEXUS_COMPONENTS_MEMORYMAP_HPP__INCLUDED
