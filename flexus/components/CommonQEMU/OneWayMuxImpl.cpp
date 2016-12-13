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
#include <components/CommonQEMU/OneWayMux.hpp>

#define FLEXUS_BEGIN_COMPONENT OneWayMux
#include FLEXUS_BEGIN_COMPONENT_IMPLEMENTATION()

namespace nOneWayMux {

using namespace Flexus;
using namespace Core;
using namespace SharedTypes;

class FLEXUS_COMPONENT(OneWayMux) {
  FLEXUS_COMPONENT_IMPL( OneWayMux );

public:
  FLEXUS_COMPONENT_CONSTRUCTOR(OneWayMux)
    : base( FLEXUS_PASS_CONSTRUCTOR_ARGS )
  {}

public:
  // Initialization
  void initialize() {
  }

  // Added by PLotfi
  void finalize() {
  }
  // end PLotfi

  bool isQuiesced() const {
    return true; //Mux is always quiesced
  }

  // Ports
  // From the instruction cache
  bool available( interface::InI const &,
                  index_t anIndex ) {
    return true;
  }

  void push( interface::InI const &,
             index_t         anIndex,
             MemoryMessage & aMessage ) {
    FLEXUS_CHANNEL( Out ) << aMessage;
  }

  // From the data cache
  bool available( interface::InD const &,
                  index_t anIndex ) {
    return true;
  }

  void push( interface::InD const &,
             index_t         anIndex,
             MemoryMessage & aMessage ) {
    FLEXUS_CHANNEL( Out ) << aMessage;
  }

};

} //End Namespace nOneWayMux

FLEXUS_COMPONENT_INSTANTIATOR( OneWayMux, nOneWayMux );

FLEXUS_PORT_ARRAY_WIDTH( OneWayMux, InI)      {
  return 1;
}
FLEXUS_PORT_ARRAY_WIDTH( OneWayMux, InD)      {
  return 1;
}

#include FLEXUS_END_COMPONENT_IMPLEMENTATION()
#define FLEXUS_END_COMPONENT OneWayMux
