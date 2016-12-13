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
#include <components/CommonQEMU/Slices/DirectoryEntry.hpp>

namespace Flexus {
namespace SharedTypes {

// for debug output
std::ostream & operator << (std::ostream & anOstream, tDirState const x) {
  const char * const name[3] = { "DIR_STATE_INVALID",
                                 "DIR_STATE_SHARED",
                                 "DIR_STATE_MODIFIED"
                               };
  DBG_Assert(x < static_cast<int>(sizeof(name)));
  anOstream << name[x];
  return anOstream;
}

std::ostream & operator << (std::ostream & aStream, DirectoryEntry const & anEntry) {
  switch (anEntry.getState()) {
    case DIR_STATE_INVALID:
      aStream << anEntry.getState();
      break;
    case DIR_STATE_SHARED: {
      bool first = true;
      aStream << anEntry.getState() << " Sharers{";
      for (int32_t i = 0; i < 16; ++i) {
        if (anEntry.theNodes & (1UL << i)) {
          if (!first) {
            aStream << ",";
          }
          first = false;
          aStream << i;
        }
      }
      aStream << "}";
      break;
    }
    case DIR_STATE_MODIFIED:
      aStream << anEntry.getState() << " Owner=" << anEntry.theNodes;
      break;
  }
  aStream << ( (anEntry.wasModified()) ? "  " : " !" );
  aStream << "WasModified";
  bool first = true;
  aStream << anEntry.getState() << " PastSharers{";
  for (int32_t i = 0; i < 16; ++i) {
    if (anEntry.getPastReaders() & (1UL << i)) {
      if (!first) {
        aStream << ",";
      }
      first = false;
      aStream << i;
    }
  }
  aStream << "}";
  return aStream;
}

} //namespace SharedTypes
} //namespace Flexus

