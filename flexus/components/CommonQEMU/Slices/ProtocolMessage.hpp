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
#ifndef FLEXUS_SLICES__PROTOCOLMESSAGE_HPP_INCLUDED
#define FLEXUS_SLICES__PROTOCOLMESSAGE_HPP_INCLUDED

#ifdef FLEXUS_ProtocolMessage_TYPE_PROVIDED
#error "Only one component may provide the Flexus::SharedTypes::ProtocolMessage data type"
#endif
#define FLEXUS_ProtocolMessage_TYPE_PROVIDED

#include <core/types.hpp>
#include <core/boost_extensions/intrusive_ptr.hpp>

#include <components/CommonQEMU/Slices/DirectoryEntry.hpp>

namespace Flexus {
namespace SharedTypes {

class TransactionTracker;
class DirectoryEntry;

typedef int32_t tVC;

static const tVC LocalVC0 = 0; // requests from local cache/cpu
static const tVC LocalVC1 = 1; // replies from local cache/cpu
static const tVC VC0 = 2;      // requests from RE
static const tVC VC1 = 3;      // requests from HE, or replies to requests from RE
static const tVC VC2 = 4;      // replies to requests from HE

static const tVC LocalVCLow  = LocalVC0;
static const tVC LocalVCHigh = LocalVC1;

static const tVC NetVCLow  = VC0;
static const tVC NetVCHigh = VC2;   // The number of virtual channels between PEs.

static const tVC kMinVC = LocalVC0;
static const tVC kMaxVC = VC2;

namespace Protocol {
enum ProtocolMessageType {

  // Requests From Cache to Directory
  ReadReq,                // VC 0. Delivered to HE (starts a new thread).
  WriteReq,               // VC 0. Delivered to HE (starts a new thread).
  UpgradeReq,             // VC 0. Delivered to HE (starts a new thread).
  FlushReq,               // VC 1. Contains data. Delivered to HE (starts a new thread).
  WritebackReq,           // VC 1. Contains data. Delivered to HE (starts a new thread, or delivered to existing thread).
  // ReadExReq,           // - Not used
  // WriteAllReq,         // - Not used
  // ReadAndForgetReq,    // - Not used

  // Requests From Directory to Cache
  LocalInvalidationReq,   // VC 1. Delivered to RE (starts a new thread or delivered to existing thread).
  RemoteInvalidationReq,  // VC 1. Delivered to RE (starts a new thread or delivered to existing thread).
  ForwardedReadReq,       // VC 1. Delivered to RE (starts a new thread or delivered to existing thread).
  ForwardedWriteReq,      // VC 1. Delivered to RE (starts a new thread or delivered to existing thread).
  RecallReadReq,          // VC 1. Delivered to RE (starts a new thread or delivered to existing thread).
  RecallWriteReq,         // VC 1. Delivered to RE (starts a new thread or delivered to existing thread).

  // Replies From Cache to Directory
  ForwardedWriteAck,      // VC 2. Delivered to HE (delivered to existing thread).
  ForwardedReadAck,       // VC 2. Contains data. Delivered to HE (delivered to existing thread).
  RecallReadAck,          // VC 2. Contains data. Delivered to HE (delivered to existing thread).
  RecallWriteAck,         // VC 2. Contains data. Delivered to HE (delivered to existing thread).
  LocalInvalidationAck,   // VC 2. Delivered to HE (delivered to existing thread).

  // Replies From Directory to Cache
  ReadAck,                // VC 2. Contains data and HomeNodeSharer flag. Delivered to RE (delivered to existing thread).
  WriteAck,               // VC 2. Contains data, #inval acks to expect and HomeNodeSharer flag. Delivered to RE (delivered to existing thread).
  UpgradeAck,             // VC 2. Contains #inval acks to expect and HomeNodeSharer flag. Delivered to RE (delivered to existing thread).
  WritebackAck,           // VC 2. Delivered to RE (delivered to existing thread).
  WritebackStaleRdAck,    // VC 2. Delivered to RE (delivered to existing thread).
  WritebackStaleWrAck,    // VC 2. Delivered to RE (delivered to existing thread).
  FlushAck,               // VC 2. Delivered to RE (delivered to existing thread).

  // Replies From Cache to Cache
  RemoteInvalidationAck,  // VC 2. Delivered to RE (delivered to existing thread).
  ReadFwd,                // VC 2. Contains Data. Delivered to RE (delivered to existing thread).
  WriteFwd,               // VC 2. Contains Data. Delivered to RE (delivered to existing thread).

  // Requests from local Caches to PE.
  LocalRead,              // VC 0. Read request.
  LocalWriteAccess,       // VC 0. Write request with cache block ownership.
  LocalUpgradeAccess,     // VC 0. Upgrade request with cache block ownership.
  LocalFlush,             // VC 0. Contains data.
  LocalDropHint,          // VC 0. Evicting a clean line. Normally dropped on the floor.
  LocalEvict,             // VC 0. Contains data. Evicting a dirty line. Causes a Writeback to HE.
  LocalPrefetchRead,      // VC 0. Prefetch read request.
  // LocalReadEx,         // - Not used
  // LocalWriteAll,       // - Not used
  // LocalReadAndForget   // - Not used

  // Replies from local Caches to PE
  InvAck,                 // VC 1.  The cache acknowledges an Invalidate CPU_OP. Delivered to HE or RE (delivered to existing thread).
  InvUpdateAck,           // VC 1.  Contains data. The cache acknowledges an Invalidate CPU_OP. Delivered to HE or RE (delivered to existing thread).
  DowngradeAck,           // VC 1.  The cache acknowledges a Downgrade CPU_OP. Delivered to HE or RE (delivered to existing thread).
  DowngradeUpdateAck,     // VC 1.  Contains data. The cache acknowledges a Downgrade CPU_OP. Delivered to HE or RE (delivered to existing thread).

  // Error packet
  ProtocolError           // ERROR IN PROTOCOL!!!
};
bool isRequest(ProtocolMessageType msg_type);
bool isPotentialReply(ProtocolMessageType msg_type);
bool isRemote(ProtocolMessageType msg_type);
bool carriesData(ProtocolMessageType msg_type);
std::ostream & operator << (std::ostream & anOstream, ProtocolMessageType msg_type);
tVC networkMessageTypeToVC(ProtocolMessageType aMessage);
}

struct ProtocolMessage : public boost::counted_base {
  typedef Flexus::SharedTypes::PhysicalMemoryAddress tAddress;
private:
  bool          theLocal;
  tAddress      theAddress;      // Address pertaining to request
  node_id_t     theDest;         // Node id for this message's destination
  tVC           theVC;           // Virtual channel the request travels on
  node_id_t     theSource;       // Node id this message comes from
  node_id_t     theRequester;    // This is the node the request originated from (the one who created the thread).
  // For instance, say the state of a cache line is shared and its home node is B.
  // When a WrReq arrives at the home engine from node A, then node A is the requester.
  // When the home engine sends out invalidations to other nodes, the invalidations
  // specify A as the requester, even though the invalidation requests originate from
  // node B (the directory). Then, all InvalAcks will be sent to A for collection.
  // Any node can send to node B using the ADDR_TO_DIR_NODE_ID(addr) macro.
  // There is no reason for the message to carry this information along (not yet!).
  // As a rule of thumb, requests on VC0 and writebacks have their source as the requester,
  // and the messages the directory sends on their behalf preserve the requester.
  // Replies from the other nodes to the directory or to the requester preserve it too,
  // with a few exceptions in order to resolve races (e.g. stale acks).
  Protocol::ProtocolMessageType  theType; // Message type
  unsigned      theInvalCount;   // Where applicable: invalidations count or InvalAcks count
  bool theAnyInvalidations; // Where applicable: indicates whether there were any invalidations (including to home CPU).
  boost::intrusive_ptr<DirectoryEntry const> theDirEntry; // Directory info passed from the local engine
  bool          theIsPrefetch;
  boost::intrusive_ptr<TransactionTracker> theTracker;
public:
  int64_t     theInputQTimestamp;

public:
  //Network packets
  ProtocolMessage(tAddress anAddress, uint32_t aDest, tVC aVC, node_id_t aSrc, node_id_t aRequester, Protocol::ProtocolMessageType aMessageType, unsigned anInvalidationCount, bool anAnyInvalidations, bool isPrefetch, boost::intrusive_ptr<TransactionTracker> aTracker);

  //Local requests
  ProtocolMessage(tAddress anAddress, tVC aVC, Protocol::ProtocolMessageType aMessageType, boost::intrusive_ptr<DirectoryEntry const> anEntry, bool isPrefetch, boost::intrusive_ptr<TransactionTracker> aTracker);

  friend std::ostream & operator<<(std::ostream & anOstream, const ProtocolMessage & aPacket);

  bool isLocal() const {
    return theLocal;
  }
  tAddress address() const {
    return theAddress;
  }
  bool isPrefetch() const {
    return theIsPrefetch;
  }
  Protocol::ProtocolMessageType type() const {
    return theType;
  }
  node_id_t requester() const {
    DBG_Assert( ! theLocal );
    return theRequester;
  }
  node_id_t respondent() const {
    DBG_Assert( ! theLocal );
    return theSource;
  }
  node_id_t source() const {
    DBG_Assert( ! theLocal );
    return theSource;
  }
  node_id_t dest() const {
    DBG_Assert( ! theLocal );
    return theDest;
  }
  node_id_t invalidationCount() const {
    DBG_Assert( ! theLocal );
    return theInvalCount;
  }
  bool anyInvalidations() const {
    DBG_Assert( ! theLocal );
    return theAnyInvalidations;
  }
  boost::intrusive_ptr<DirectoryEntry const> dirEntry() const;
  tVC VC() const {
    return theVC;
  }
  boost::intrusive_ptr<TransactionTracker> transactionTracker() const;

  void setType( Protocol::ProtocolMessageType aType);

  void setRequester( node_id_t aRequester) {
    theRequester = aRequester;
  }

  void setSource( node_id_t aSource) {
    theSource = aSource;
  }

};

} //End SharedTypes
} //End Flexus

#endif //FLEXUS_SLICES__PROTOCOLMESSAGE_HPP_INCLUDED
