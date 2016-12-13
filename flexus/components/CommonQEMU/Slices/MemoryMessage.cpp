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
#include <components/CommonQEMU/Slices/MemoryMessage.hpp>

namespace Flexus {
namespace SharedTypes {

static uint32_t theMemoryMessageSerial = 0;

uint32_t memoryMessageSerial ( void ) {
  return ++theMemoryMessageSerial;
}

std::ostream & operator << (std::ostream & s, MemoryMessage::MemoryMessageType const & aMemMsgType) {
  static char const * message_types[] = {
    "Load Request",
    "Store Request",
    "Store Prefetch Request",
    "Fetch Request",
    "Non-allocating Store Request",
    "RMW Request",
    "Cmp-Swap Request",
    "Atomic Preload Request",
    "Flush Request",
    "Read Request",

    "Write Request",
    "Write Allocate",
    "Upgrade Request",
    "Upgrade Allocate",
    "Flush",
    "Eviction (dirty)",
    "Eviction (writable)",
    "Eviction (clean)",
    "SVB Clean Eviction",
    "Load Reply",
    "Store Reply",
    "Store Prefetch Reply",
    "Fetch Reply",
    "RMW Reply",
    "Cmp-Swap Reply",
    "Atomic Preload Reply",
    "Miss Reply",
    "Miss Writable Reply",
    "Miss Dirty Reply",

    "Upgrade Reply",
    "Non-Allocating Store Reply",
    "Invalidate",
    "Downgrade",
    "Probe",
    "DownProbe",
    "ReturnReq",
    "Invalidate Ack",
    "Invalidate Update Ack",
    "Downgrade Ack",

    "Downgrade Update Ack",
    "Probed - Not Present",
    "Probed - Clean",
    "Probed - Writable",
    "Probed - Dirty",
    "DownProbe - Present",
    "DownProbe - Not Present",
    "ReturnReply",
    "Stream Fetch",

    "Prefetch Read No-Allocate Request",
    "Prefetch Read Allocate",
    "Prefetch Insert Request",
    "Prefetch Insert Writable Request",
    "Prefetch Read Reply",
    "Prefetch Read Reply Writable",
    "Prefetch Read Reply Dirty",
    "Prefetch Read Redundant",
    "Stream Fetch Writable Reply"
    , "Stream Fetch Rejected"
    , "ReturnNAck"
    , "ReturnReplyDirty"
    , "FetchFwd"
    , "ReadFwd"
    , "WriteFwd"
    , "FwdNAck"
    , "FwdReply"
    , "FwdReplyOwned"
    , "FwdReplyWritable"
    , "FwdReplyDirty"
    , "ReadAck"
    , "ReadAckDirty"
    , "FetchAck"
    , "FetchAckDirty"
    , "WriteAck"
    , "UpgradeAck"
    , "Non-Allocating Store Ack"
    , "ReadNAck"
    , "FetchNAck"
    , "WriteNAck"
    , "UpgradeNAck"
    , "Non-Allocating Store NAck"
    , "MissNotify"
    , "MissNotifyData"
    , "BackInvalidate"
    , "InvalidateNAck"
    , "Protocol Message"
    , "Evict Ack"
    , "Write Retry"
    , "NumMemoryMessageTypes"
  };
  assert(aMemMsgType <= MemoryMessage::NumMemoryMessageTypes );
  return s << message_types[aMemMsgType];
}
std::ostream & operator << (std::ostream & s, MemoryMessage const & aMemMsg) {
  return s << "MemoryMessage[" << aMemMsg.type()
         << "]: Addr:0x" << std::hex << aMemMsg.address()
         << " Size:" << std::dec << aMemMsg.reqSize() << " Serial: " << aMemMsg.serial()
         << " Core: " << aMemMsg.coreIdx()
         << " DStream: " << std::boolalpha << aMemMsg.isDstream()
         << " Outstanding Msgs: " << aMemMsg.outstandingMsgs()
         << (aMemMsg.ackRequired() ? (aMemMsg.ackRequiresData() ? " Requires Ack+Data" : " Requires Ack") : "");
}

} //namespace SharedTypes
} //namespace Flexus

