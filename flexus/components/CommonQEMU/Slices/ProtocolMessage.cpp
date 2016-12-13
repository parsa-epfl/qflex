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
#include <components/CommonQEMU/Slices/TransactionTracker.hpp>
#include <components/CommonQEMU/Slices/ProtocolMessage.hpp>

namespace Flexus {
namespace SharedTypes {

ProtocolMessage::ProtocolMessage(tAddress anAddress, uint32_t aDest, tVC aVC, node_id_t aSrc, node_id_t aRequester, Protocol::ProtocolMessageType aMessageType, unsigned anInvalidationCount, bool anAnyInvalidations, bool isPrefetch, boost::intrusive_ptr<TransactionTracker> aTracker)
  : theLocal(false)
  , theAddress(anAddress)
  , theDest(aDest)
  , theVC(aVC)
  , theSource(aSrc)
  , theRequester(aRequester)
  , theType(aMessageType)
  , theInvalCount(anInvalidationCount)
  , theAnyInvalidations(anAnyInvalidations)
  , theIsPrefetch(isPrefetch)
  , theTracker(aTracker)
{}

//Local requests
ProtocolMessage::ProtocolMessage(tAddress anAddress, tVC aVC, Protocol::ProtocolMessageType aMessageType, boost::intrusive_ptr<DirectoryEntry const> anEntry, bool isPrefetch, boost::intrusive_ptr<TransactionTracker> aTracker)
  : theLocal(true)
  , theAddress(anAddress)
  , theDest(0)
  , theVC(aVC)
  , theSource(0)
  , theRequester(0)
  , theType(aMessageType)
  , theInvalCount(0)
  , theAnyInvalidations(0)
  , theDirEntry(anEntry)
  , theIsPrefetch(isPrefetch)
  , theTracker(aTracker)
{}

std::ostream & operator<<(std::ostream & anOstream, const ProtocolMessage & aPacket) {
  anOstream << "VC" << aPacket.VC();
  if (aPacket.isLocal()) {
    anOstream << " local";
  } else {
    anOstream << "  [" << aPacket.respondent() << "]=>[" << aPacket.dest() << "]";
    if (aPacket.requester() != aPacket.respondent()) {
      anOstream << " on behalf of [" << aPacket.requester() << "]";
    }
    anOstream << ": " << aPacket.type() << "{" << &std::hex << aPacket.address() << &std::dec << "}";
  }
  return anOstream;
}

boost::intrusive_ptr<DirectoryEntry const> ProtocolMessage::dirEntry() const {
  DBG_Assert( theLocal );
  return theDirEntry;
}
boost::intrusive_ptr<TransactionTracker> ProtocolMessage::transactionTracker() const {
  return theTracker;
}

void ProtocolMessage::setType( Protocol::ProtocolMessageType aType) {
  networkMessageTypeToVC(aType); //This will check the message type for validity
  theType = aType;
}

namespace Protocol {

tVC networkMessageTypeToVC(ProtocolMessageType msg_type) {
  static const tVC VCs[38] = {

    // Requests From Cache to Directory
    VC0,  // ReadReq
    VC0,  // WriteReq
    VC0,  // UpgradeReq
    VC1,  // FlushReq
    VC1,  // WritebackReq
    // ReadExReq,           // - Not used
    // WriteAllReq,         // - Not used
    // ReadAndForgetReq,    // - Not used

    // Requests From Directory to Cache
    VC1,  // LocalInvalidationReq
    VC1,  // RemoteInvalidationReq
    VC1,  // ForwardedReadReq
    VC1,  // ForwardedWriteReq
    VC1,  // RecallReadReq
    VC1,  // RecallWriteReq

    // Replies From Cache to Directory
    VC2,  // ForwardedWriteAck
    VC2,  // ForwardedReadAck
    VC2,  // RecallReadAck
    VC2,  // RecallWriteAck
    VC2,  // LocalInvalidationAck

    // Replies From Directory to Cache
    VC2,  // ReadAck
    VC2,  // WriteAck
    VC2,  // UpgradeAck
    VC2,  // WritebackAck
    VC2,  // WritebackStaleRdAck
    VC2,  // WritebackStaleWrAck
    VC2,  // FlushAck

    // Replies From Cache to Cache
    VC2,  // RemoteInvalidationAck
    VC2,  // ReadFwd
    VC2,  // WriteFwd

    // Requests from local Caches to PE.
    LocalVC0,  // LocalRead
    LocalVC0,  // LocalWriteAccess
    LocalVC0,  // LocalUpgradeAccess
    LocalVC0,  // LocalFlush
    LocalVC0,  // LocalDropHint
    LocalVC0,  // LocalEvict
    LocalVC0,  // LocalPrefetchRead
    // LocalReadEx,           // - Not used
    // LocalWriteAll,         // - Not used
    // LocalReadAndForget     // - Not used

    // Replies from Caches to PE
    LocalVC1,  // InvAck
    LocalVC1,  // InvUpdateAck
    LocalVC1,  // DowngradeAck
    LocalVC1,  // DowngradeUpdateAck

    // Error packet
    VC0,       // ERROR IN PROTOCOL!!!

  };
  DBG_Assert((msg_type) < static_cast<int>(sizeof(VCs)));

  return VCs[msg_type];
}

std::ostream & operator << (std::ostream & anOstream, ProtocolMessageType msg_type) {
  static const char * const name[38] = {

    // Requests From Cache to Directory
    "ReadReq",                // VC 0.
    "WriteReq",               // VC 0.
    "UpgradeReq",             // VC 0.
    "FlushReq",               // VC 1. Contains data
    "WritebackReq",           // VC 1. Contains data
    // ReadExReq,             // - Not used
    // WriteAllReq,           // - Not used
    // ReadAndForgetReq,      // - Not used

    // Requests From Directory to Cache
    "LocalInvalidationReq",   // VC 1.
    "RemoteInvalidationReq",  // VC 1.
    "ForwardedReadReq",       // VC 1.
    "ForwardedWriteReq",      // VC 1.
    "RecallReadReq",          // VC 1.
    "RecallWriteReq",         // VC 1.

    // Replies From Cache to Directory
    "ForwardedWriteAck",      // VC 2.
    "ForwardedReadAck",       // VC 2. Contains data
    "RecallReadAck",          // VC 2. Contains data
    "RecallWriteAck",         // VC 2. Contains data
    "LocalInvalidationAck",   // VC 2.

    // Replies From Directory to Cache
    "ReadAck",                // VC 2. Contains data and HomeNodeSharer flag
    "WriteAck",               // VC 2. Contains data, #inval acks to expect and HomeNodeSharer flag
    "UpgradeAck",             // VC 2. #inval acks to expect and HomeNodeSharer flag
    "WritebackAck",           // VC 2.
    "WritebackStaleRdAck",    // VC 2.
    "WritebackStaleWrAck",    // VC 2.
    "FlushAck",               // VC 2.

    // Replies From Cache to Cache
    "RemoteInvalidationAck",  // VC 2.
    "ReadFwd",                // VC 2. Contains Data
    "WriteFwd",               // VC 2. Contains Data

    // Requests from local Caches to PE.
    "LocalRead",              // VC 0. Read request.
    "LocalWriteAccess",       // VC 0. Write request with cache block ownership.
    "LocalUpgradeAccess",     // VC 0. Upgrade request with cache block ownership.
    "LocalFlush",             // VC 0. Contains data.
    "LocalDropHint",          // VC 0. Evicting a clean line. Normally dropped on the floor.
    "LocalEvict",             // VC 0. Contains data. Evicting a dirty line. Causes a Writeback to HE.
    "LocalPrefetchRead",      // VC 0. Read request.
    // LocalReadEx,           // - Not used
    // LocalWriteAll,         // - Not used
    // LocalReadAndForget     // - Not used

    // Replies from Caches to PE
    "InvAck",                 // The cache acknowledges an Invalidate CPU_OP.
    "InvUpdateAck",           // Contains data. The cache acknowledges an Invalidate CPU_OP.
    "DowngradeAck",           // The cache acknowledges a Downgrade CPU_OP.
    "DowngradeUpdateAck",     // Contains data. The cache acknowledges a Downgrade CPU_OP.

    // Error packet
    "ERROR!!!!!!!!!!!!"       // ERROR IN PROTOCOL!!!
  };
  DBG_Assert(msg_type < static_cast<int>(sizeof(name)));
  anOstream << name[msg_type];
  return anOstream;
}

bool isRequest(ProtocolMessageType msg_type) {
  static const bool isReqs[38] = {

    // Requests From Cache to Directory
    true,   // ReadReq
    true,   // WriteReq
    true,   // UpgradeReq
    true,   // FlushReq
    true,   // WritebackReq
    // ReadExReq,           // - Not used
    // WriteAllReq,         // - Not used
    // ReadAndForgetReq,    // - Not used

    // Requests From Directory to Cache
    true,   // LocalInvalidationReq
    true,   // RemoteInvalidationReq
    true,   // ForwardedReadReq
    true,   // ForwardedWriteReq
    true,   // RecallReadReq
    true,   // RecallWriteReq

    // Replies From Cache to Directory
    false,  // ForwardedWriteAck
    false,  // ForwardedReadAck
    false,  // RecallReadAck
    false,  // RecallWriteAck
    false,  // LocalInvalidationAck

    // Replies From Directory to Cache
    false,  // ReadAck
    false,  // WriteAck
    false,  // UpgradeAck
    false,  // WritebackAck
    false,  // WritebackStaleRdAck
    false,  // WritebackStaleWrAck
    false,  // FlushAck

    // Replies From Cache to Cache
    false,  // RemoteInvalidationAck
    false,  // ReadFwd
    false,  // WriteFwd

    // Requests from local Caches to PE.
    true,   // LocalRead
    true,   // LocalWriteAccess
    true,   // LocalUpgradeAccess
    true,   // LocalFlush
    true,   // LocalDropHint
    true,   // LocalEvict
    true,   // LocalPrefetchRead
    // LocalReadEx,           // - Not used
    // LocalWriteAll,         // - Not used
    // LocalReadAndForget     // - Not used

    // Replies from Caches to PE
    false,  // InvAck
    false,  // InvUpdateAck
    false,  // DowngradeAck
    false,  // DowngradeUpdateAck

    // Error packet. Should always be delivered
    true,   // ERROR IN PROTOCOL!!!

  };
  DBG_Assert(static_cast<size_t>(msg_type) < sizeof(isReqs));

  return isReqs[msg_type];
}

bool isPotentialReply(ProtocolMessageType msg_type) {
  static const bool isRepl[38] = {

    // Requests From Cache to Directory
    false,   // ReadReq
    false,   // WriteReq
    false,   // UpgradeReq
    true,    // FlushReq
    true,    // WritebackReq
    // ReadExReq,           // - Not used
    // WriteAllReq,         // - Not used
    // ReadAndForgetReq,    // - Not used

    // Requests From Directory to Cache
    true,    // LocalInvalidationReq
    true,    // RemoteInvalidationReq
    true,    // ForwardedReadReq
    true,    // ForwardedWriteReq
    true,    // RecallReadReq
    true,    // RecallWriteReq

    // Replies From Cache to Directory
    true,   // ForwardedWriteAck
    true,   // ForwardedReadAck
    true,   // RecallReadAck
    true,   // RecallWriteAck
    true,   // LocalInvalidationAck

    // Replies From Directory to Cache
    true,   // ReadAck
    true,   // WriteAck
    true,   // UpgradeAck
    true,   // WritebackAck
    true,   // WritebackStaleRdAck
    true,   // WritebackStaleWrAck
    true,   // FlushAck

    // Replies From Cache to Cache
    true,   // RemoteInvalidationAck
    true,   // ReadFwd
    true,   // WriteFwd

    // Requests from local Caches to PE.
    false,  // LocalRead
    false,  // LocalWriteAccess
    false,  // LocalUpgradeAccess
    false,  // LocalFlush
    false,  // LocalDropHint
    false,  // LocalEvict
    false,  // LocalPrefetchRead
    // LocalReadEx,           // - Not used
    // LocalWriteAll,         // - Not used
    // LocalReadAndForget     // - Not used

    // Replies from Caches to PE
    true,   // InvAck
    true,   // InvUpdateAck
    true,   // DowngradeAck
    true,   // DowngradeUpdateAck

    // Error packet. Should always be delivered
    true,   // ERROR IN PROTOCOL!!!

  };
  DBG_Assert(static_cast<size_t>(msg_type) < sizeof(isRepl));

  return isRepl[msg_type];
}

bool isRemote(ProtocolMessageType msg_type) {
  static const bool isRemote[38] = {

    // Requests From Cache to Directory
    true,   // ReadReq
    true,   // WriteReq
    true,   // UpgradeReq
    true,   // FlushReq
    true,   // WritebackReq
    // ReadExReq,           // - Not used
    // WriteAllReq,         // - Not used
    // ReadAndForgetReq,    // - Not used

    // Requests From Directory to Cache
    true,   // LocalInvalidationReq
    true,   // RemoteInvalidationReq
    true,   // ForwardedReadReq
    true,   // ForwardedWriteReq
    true,   // RecallReadReq
    true,   // RecallWriteReq

    // Replies From Cache to Directory
    true,  // ForwardedWriteAck
    true,  // ForwardedReadAck
    true,  // RecallReadAck
    true,  // RecallWriteAck
    true,  // LocalInvalidationAck

    // Replies From Directory to Cache
    true,  // ReadAck
    true,  // WriteAck
    true,  // UpgradeAck
    true,  // WritebackAck
    true,  // WritebackStaleRdAck
    true,  // WritebackStaleWrAck
    true,  // FlushAck

    // Replies From Cache to Cache
    true,  // RemoteInvalidationAck
    true,  // ReadFwd
    true,  // WriteFwd

    // Requests from local Caches to PE.
    false,   // LocalRead
    false,   // LocalWriteAccess
    false,   // LocalUpgradeAccess
    false,   // LocalFlush
    false,   // LocalDropHint
    false,   // LocalEvict
    false,   // LocalPrefetchRead
    // LocalReadEx,           // - Not used
    // LocalWriteAll,         // - Not used
    // LocalReadAndForget     // - Not used

    // Replies from Caches to PE
    false,  // InvAck
    false,  // InvUpdateAck
    false,  // DowngradeAck
    false,  // DowngradeUpdateAck

    // Error packet. Should always be delivered
    true,   // ERROR IN PROTOCOL!!!

  };
  DBG_Assert(static_cast<size_t>(msg_type) < sizeof(isRemote));

  return isRemote[msg_type];
}

bool carriesData(ProtocolMessageType msg_type) {
  static const bool carriesData[38] = {
    // Requests From Cache to Directory
    false,   // ReadReq
    false,   // WriteReq
    false,   // UpgradeReq
    true,   // FlushReq
    true,   // WritebackReq
    // ReadExReq,           // - Not used
    // WriteAllReq,         // - Not used
    // ReadAndForgetReq,    // - Not used

    // Requests From Directory to Cache
    false,   // LocalInvalidationReq
    false,   // RemoteInvalidationReq
    false,   // ForwardedReadReq
    false,   // ForwardedWriteReq
    false,   // RecallReadReq
    false,   // RecallWriteReq

    // Replies From Cache to Directory
    false,  // ForwardedWriteAck
    true,  // ForwardedReadAck
    true,  // RecallReadAck
    true,  // RecallWriteAck
    false,  // LocalInvalidationAck

    // Replies From Directory to Cache
    true,  // ReadAck
    true,  // WriteAck
    true,  // UpgradeAck
    false,  // WritebackAck
    false,  // WritebackStaleRdAck
    false,  // WritebackStaleWrAck
    false,  // FlushAck

    // Replies From Cache to Cache
    false,  // RemoteInvalidationAck
    true,  // ReadFwd
    true,  // WriteFwd

    // Requests from local Caches to PE.
    // THESE MAY BE WRONG!
    false,   // LocalRead
    false,   // LocalWriteAccess
    false,   // LocalUpgradeAccess
    false,   // LocalFlush
    false,   // LocalDropHint
    false,   // LocalEvict
    false,   // LocalPrefetchRead
    // LocalReadEx,           // - Not used
    // LocalWriteAll,         // - Not used
    // LocalReadAndForget     // - Not used

    // Replies from Caches to PE
    false,  // InvAck
    false,  // InvUpdateAck
    false,  // DowngradeAck
    false,  // DowngradeUpdateAck

    // TRUSS
    //     true,  // TrussReadAck
    //     true,  // TrussWriteAck

    // Error packet.
    false,   // ERROR IN PROTOCOL!!!
  };
  DBG_Assert(static_cast<size_t>(msg_type) < sizeof(carriesData));

  return carriesData[msg_type];
}

} //End Protocol

} //End SharedTypes
} //End Flexus

