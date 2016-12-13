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
#ifndef __REGION_TRACKER_COORDINATOR_HPP__
#define __REGION_TRACKER_COORDINATOR_HPP__

#include <functional>
#include <boost/dynamic_bitset.hpp>

#include <vector>

#include <core/types.hpp>

namespace nRTCoordinator {

typedef Flexus::SharedTypes::PhysicalMemoryAddress MemoryAddress;

struct RTFunctionList {
  std::function<int32_t (MemoryAddress)> probeOwner;
  std::function<bool (MemoryAddress, int)> setOwner;
  std::function<boost::dynamic_bitset<> (MemoryAddress)> probePresence;
};

struct NotifierFunctionList {
  std::function<void (MemoryAddress)> regionEvictNotify;
  std::function<void (MemoryAddress)> regionAllocNotify;
};

class RTCoordinator {
private:
  RTCoordinator() {}

  std::vector<RTFunctionList> theRTFunctions;
  std::vector<RTFunctionList> thePassThroughFunctions;
  std::vector<NotifierFunctionList> theNotifiers;

public:
  static RTCoordinator & getCoordinator() {
    static RTCoordinator * theCoordinator = nullptr;
    if (theCoordinator == nullptr) {
      theCoordinator = new RTCoordinator();
    }
    return *theCoordinator;
  }

  void registerRTFunctions(int32_t index, RTFunctionList functions) {
    if (index >= (int)theRTFunctions.size()) {
      theRTFunctions.resize(index + 1);
    }
    theRTFunctions[index] = functions;
  }

  void registerPassThroughFunctions(int32_t index, RTFunctionList functions) {
    if (index >= (int)thePassThroughFunctions.size()) {
      thePassThroughFunctions.resize(index + 1);
    }
    thePassThroughFunctions[index] = functions;
  }

  RTFunctionList & getRTFunctions(int32_t index) {
    return theRTFunctions[index];
  }

  RTFunctionList & getPassThroughFunctions(int32_t index) {
    return thePassThroughFunctions[index];
  }

  int32_t probeOwner(int32_t index, MemoryAddress addr) {
    return theRTFunctions[index].probeOwner(addr);
  }

  bool setOwner(int32_t index, MemoryAddress addr, int32_t owner) {
    return theRTFunctions[index].setOwner(addr, owner);
  }

  int32_t probePTOwner(int32_t index, MemoryAddress addr) {
    return thePassThroughFunctions[index].probeOwner(addr);
  }

  bool setPTOwner(int32_t index, MemoryAddress addr, int32_t owner) {
    return thePassThroughFunctions[index].setOwner(addr, owner);
  }

  boost::dynamic_bitset<> probePresence(int32_t index, MemoryAddress addr) {
    return theRTFunctions[index].probePresence(addr);
  }

  void registerNotifierFunctions(int32_t index, NotifierFunctionList functions) {
    if (index >= (int)theNotifiers.size()) {
      theNotifiers.resize(index + 1);
    }
    theNotifiers[index] = functions;
  }

  void regionAllocNotify(int32_t index, MemoryAddress addr) {
    if (index < (int)theNotifiers.size() && theNotifiers[index].regionAllocNotify) {
      theNotifiers[index].regionAllocNotify(addr);
    }
  }

  void regionEvictNotify(int32_t index, MemoryAddress addr) {
    if (index < (int)theNotifiers.size() && theNotifiers[index].regionEvictNotify) {
      theNotifiers[index].regionEvictNotify(addr);
    }
  }

};

}; // namespace nRTCoordinator

#endif // ! __REGION_TRACKER_COORDINATOR_HPP__
