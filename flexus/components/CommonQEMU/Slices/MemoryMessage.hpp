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
#ifndef FLEXUS_SLICES__MEMORYMESSAGE_HPP_INCLUDED
#define FLEXUS_SLICES__MEMORYMESSAGE_HPP_INCLUDED

#ifdef FLEXUS_MemoryMessage_TYPE_PROVIDED
#error "Only one component may provide the Flexus::SharedTypes::MemoryMessage data type"
#endif
#define FLEXUS_MemoryMessage_TYPE_PROVIDED

#include <core/boost_extensions/intrusive_ptr.hpp>

#include <core/types.hpp>
#include <core/exception.hpp>

#include <components/CommonQEMU/Slices/FillLevel.hpp>
#include <components/CommonQEMU/Slices/FillType.hpp>
#include <components/uFetch/uFetchTypes.hpp>

namespace Flexus {
namespace SharedTypes {

using namespace Flexus::Core;
using boost::intrusive_ptr;

uint32_t memoryMessageSerial ( void );

#define HEADER_SIZE 8

struct MemoryMessage : public boost::counted_base { /*, public FastAlloc*/
  typedef PhysicalMemoryAddress MemoryAddress;

  // enumerated message type
  enum MemoryMessageType {
    // CPU-initiated requests
    LoadReq,
    // This is a request to read a word of data (really, it can be any
    // length, as int64_t as it does not span multiple cache blocks).
    // LoadReply is the only valid response.
    StoreReq,
    // This is a request to write a word of data (again, any length
    // except spanning multiple blocks), which is contained in the
    // request.  StoreReply is the only valid response.
    StorePrefetchReq,
    // This is a request to write SPECULATIVELY a word of data (again, any length
    // except spanning multiple blocks), which is contained in the
    // request.  StorePrefetchReply is the only valid response.
    FetchReq,
    // This is a request to read an instruction cache line
    // In the SimFlex tracer this request can be initiated by
    // either the CPU or the cache (request from front side).
    NonAllocatingStoreReq,
    // This is a request to read an instruction cache line
    // In the SimFlex tracer this request can be initiated by
    // either the CPU or the cache (request from front side).

    RMWReq,
    CmpxReq,
    // This is a compare and swap atomic operation. It obtains write
    // permission to the appropriate cache block, reads the previous
    // value of the specified word and updates the word with data
    // contained in the request.  The previous word value is returned
    // as part of CmpxReply, the only valid reply.

    AtomicPreloadReq,
    // This is a request to read the value of a cache line for
    // a TSO++ atomic preload.  If the data is available in L1,
    // this request immediately generates an AtomicPreloadReply
    // containing the data.  Additionally, if the line is not
    // writable, this message is treated like a store prefetch (but
    // the AtomicPreloadReply is sent right away).

    // Requests from front side (initiated by CPU or cache)
    FlushReq,
    // This is a request to flush dirty data out of the memory hierarchy.
    // Lines will be written back and not invalidated.  It is guaranteed
    // that a FlushReq propagates the entire depth of the hierarchy.
    // There is no response.
    //
    // FetchReq,
    // This is a request to read an instruction cache line
    // In the SimFlex tracer this request can be initiated by
    // either the CPU or the cache (request from front side).

    // Requests from front side (initiated by cache only)
    ReadReq,
    // This is a request for a chunk of data to be read.  The data does
    // not need be modifiable, although it may be.  The valid response
    // types are MissReply, MissReplyWritable, and MissReplyDirty.
    WriteReq,
    // This is a request for a chunk of data to be read with modify
    // permission.  MissReplyWritable and MissReplyDirty are valid responses.
    WriteAllocate,
    // This is a request to write a chunk of data (which is contained in
    // the request) and also to read a (possibly larger) chunk of data.
    // This allows the write operation to occur at the current level and
    // also at the lower level.  The data chunk retrieved must be
    // modifiable.  MissReplyWritable and MissReplyDirty are the valid
    // response types.
    UpgradeReq,
    // This is a request to obtain modify permission for a chunk of data.
    // The originator must have the line in non-modifiable state.  There
    // are two possible responses:
    //   1. UpgradeReply (normal case)
    //   2. MissReplyWritable (if the data were updated without informing
    //      the originator - contains this updated data)
    UpgradeAllocate,
    // This is a request to obtain modify permission for a chunk of data,
    // and also write a chunk of data at the lower level (this data is
    // included in the request).  The originator must have the line in
    // non-modifiable state.  There are two possible responses:
    //   1. UpgradeReply (normal case)
    //   2. MissReplyWritable (if the data were updated without informing
    //      the originator - contains this updated data)
    Flush,
    // This is a request to flush dirty data out of the memory hierarchy,
    // including the dirty data contained in the request.  See FlushReq.
    // There is no response.
    EvictDirty,
    // This is a request to write back dirty data at a lower level in
    // the hierarchy (this data is included in the request), because the
    // originator has invalidated the line.  There is no response.
    EvictWritable,
    // This message indicates that the originator invalidated a line that
    // was not dirty but was modifiable.  No response is necessary.
    EvictClean,
    // This message indicates that the originator invalidated a line that
    // was not dirty.  No response is necessary.

    SVBClean,
    // This message indicates that a line has been dropped from the SVB.
    // It is used by the SimplePrefetchController to figure out when a block
    // is evicted from both the SVB and the L1 cache.

    // Responses to CPU
    LoadReply,
    // This is a response to a LoadReq; carries word data for which
    // write permission is not granted.
    StoreReply,
    // This is a response indicating that a StoreReq has been fulfilled.
    StorePrefetchReply,
    // This is a response indicating that a StorePrefetchReq has been fulfilled.
    FetchReply,
    // Response to a FetchRequest
    RMWReply,
    CmpxReply,
    // This is a response to a CmpxReq, containing previous word data,
    // and indicating that the swap has been fulfilled.

    AtomicPreloadReply,

    // Responses to front side
    MissReply,
    // This is a response that contains data for which write permission
    // is not granted.
    MissReplyWritable,
    // This is a response that contains data which may be modified.
    MissReplyDirty,
    // This is a response that contains data which may be modified and
    // is already dirty.
    UpgradeReply,
    // This is a response to indicate that modify permission has been given
    // for the desired chunk of data.
    NonAllocatingStoreReply,
    // A non-allocating store went to memory and was completed without
    // allocating the block.

    // Requests from back side
    Invalidate,
    // This is a request to ensure the line is not present anywhere above
    // the originator's level in the memory hierarchy.  There are two
    // possible responses:
    //   1. InvalidateAck (no dirty data found)
    //   2. InvUpdateAck (dirty data found and included in response)
    Downgrade,
    // This is a request to ensure the line is not dirty or modifiable
    // anywhere above the originator's lever in the memory hierarchy.
    // There are two possible responses:
    //   1. DowngradeAck (no dirty data found)
    //   2. DownUpdateAck (dirty data found and included in response)
    Probe,
    // This is a request to determine the status of a particular block
    // in the cache hierarchy.  There are four possible responses:
    //   1. ProbedNotPresent (block not present in hierarchy)
    //   2. ProbedClean (block present and not modifiable)
    //   3. ProbedWritable (block present and modifiable, but not dirty)
    //   4. ProbedDirty (block present and modified)
    DownProbe,
    ReturnReq,
    // JCS - Asks a higher-level cache to return a clean copy of a
    //       block, but not invalidate/evict the line

    // Responses to back side
    InvalidateAck,
    // This is a response to indicate the Invalidate has been satisfied
    // in the hierarchy above.
    InvUpdateAck,
    // This is a response to indicate the Invalidate has been satisfied
    // in the hierarchy above.  It contains data that had been dirty in
    // the hierarchy.
    DowngradeAck,
    // This is a response to indicate the Downgrade has been satisfied
    // in the hierarchy above.
    DownUpdateAck,
    // This is a response to indicate the Downgrade has been satisfied
    // in the hierarchy above.  It contains data that had been dirty in
    // the hierarchy.
    ProbedNotPresent,
    // This is a response to a Probe that indicates the block is not
    // present in the hierarchy above.
    ProbedClean,
    // This is a response to a Probe that indicates the block is present
    // in the hierarchy above, but not modified or writable (in any level).
    ProbedWritable,
    // This is a response to a Probe that indicates the block is present
    // and writable (but not dirty) in at least one level of the hierarchy above.
    ProbedDirty,
    // This is a response to a Probe that indicates the block is present
    // and modified in at least one level of the hierarchy above.
    DownProbePresent,
    DownProbeNotPresent,
    ReturnReply,
    // JCS - Acknowledgement with data from a ReturnReq

    // Prefetch requests (from the front side)
    StreamFetch,
    // Requests a block be streamed from L2 to SVB.
    PrefetchReadNoAllocReq,
    // This is a request for a chunk of data to be read.  It will be
    // inserted into a prefetch buffer upon reply. Caches that receive this message
    // should respond with PrefetchReadRedundant if they have the block.  If
    // they do not have the block, they should pass a PrefetchReadNoAllocReq
    // to the next cache level, and return either PrefetchReadReply or
    // PrefetchWritableReply.  The cache should NOT allocate the block when
    // it is returned, it should be passed through to the prefetcer above.
    PrefetchReadAllocReq,
    // This is a request for a chunk of data to be allocated in
    // lower levels of the hierarchy.  The cache that receives the
    // PrefetchReadAllocReq and all caches below it should allocate the block.
    // If the block is already present at the cache level receiving this
    // message, the cache should reply with PrefetchReadRedundant.
    // Valid responses are the same as for PrefetchReadReq.
    PrefetchInsert,
    // This is a request that carries data, asks that the cache
    // allocates space for it, and is the result of a prefetch
    // operation.  The data is clean and non-modifiable.  There is
    // no response.
    PrefetchInsertWritable,
    // This is a request that carries data, asks that the cache
    // allocates space for it, and is the result of a prefetch
    // operation.  The data is clean and modifiable.  There is no
    // response.

    // Prefetch responses (from the back side)
    PrefetchReadReply,
    // This is a response to a prefetch read operation.  It contains
    // data for which write permission is not granted.
    PrefetchWritableReply,
    // This is a response to a prefetch read operation.  It contains
    // data which may be modified.
    PrefetchDirtyReply,
    // May only be sent in response to a stream fetch operation.
    PrefetchReadRedundant,
    // This is a response to a prefetch read opertion, indicating
    // that the requested block is already present in the hierarchy.
    // It contains no data.
    StreamFetchWritableReply,
    StreamFetchRejected,

    // New types added by JZ for Directory and InclusiveMOESI
    ReturnNAck,
    ReturnReplyDirty,

    FetchFwd,
    ReadFwd,
    WriteFwd,

    FwdNAck,
    FwdReply,
    FwdReplyOwned,
    FwdReplyWritable,
    FwdReplyDirty,

    ReadAck,
    ReadAckDirty,
    FetchAck,
    FetchAckDirty,
    WriteAck,
    UpgradeAck,
    NASAck,

    ReadNAck,
    FetchNAck,
    WriteNAck,
    UpgradeNAck,
    NASNAck,

    // Notification from directory that indicates how many outstanding invalidates to expect for a write miss in progress
    MissNotify,
    MissNotifyData,

    // BackInvalidate msgs are generated to maintain inclusion (directory inclusion, cache inclusion)
    // To avoid changin L1 caches, L1's don't know about BackInvalidates
    BackInvalidate,

    InvalidateNAck,
    ProtocolMessage,

    EvictAck,
    WriteRetry,

    NumMemoryMessageTypes
  };

  explicit MemoryMessage(MemoryMessageType aType)
    : theType(aType)
    , theAddress(0)
    , theAssociatedPC(0)
    , theData(0)
    , theReqSize(0)
    , theCoreIdx(0)
    , theSerial(memoryMessageSerial() )
    , thePriv(false)
    , theAnyInvs(false)
    , theDstream(true)
    , theFillLevel(eUnknown)
    , theOutstandingMessages(0)
    , theAckRequired(true)
    , theAckRequiresData(false)
    , theEvictHasData(false)
    , theBranchType(kNonBranch)
    , theBranchAnnul(false)
  {}
  explicit MemoryMessage(MemoryMessageType aType, MemoryAddress anAddress)
    : theType(aType)
    , theAddress(anAddress)
    , theAssociatedPC(0)
    , theData(0)
    , theReqSize(0)
    , theCoreIdx(0)
    , theSerial(memoryMessageSerial() )
    , thePriv(false)
    , theAnyInvs(false)
    , theDstream(true)
    , theFillLevel(eUnknown)
    , theOutstandingMessages(0)
    , theAckRequired(true)
    , theAckRequiresData(false)
    , theEvictHasData(false)
    , theBranchType(kNonBranch)
    , theBranchAnnul(false)
  {}
  explicit MemoryMessage(MemoryMessageType aType, MemoryAddress anAddress, VirtualMemoryAddress aPC)
    : theType(aType)
    , theAddress(anAddress)
    , theAssociatedPC(aPC)
    , theData(0)
    , theReqSize(0)
    , theCoreIdx(0)
    , theSerial(memoryMessageSerial() )
    , thePriv(false)
    , theAnyInvs(false)
    , theDstream(true)
    , theFillLevel(eUnknown)
    , theOutstandingMessages(0)
    , theAckRequired(true)
    , theAckRequiresData(false)
    , theEvictHasData(false)
    , theBranchType(kNonBranch)
    , theBranchAnnul(false)
  {}
  explicit MemoryMessage(MemoryMessageType aType, MemoryAddress anAddress, VirtualMemoryAddress aPC, DataWord aData)
    : theType(aType)
    , theAddress(anAddress)
    , theAssociatedPC(aPC)
    , theData(aData)
    , theReqSize(0)
    , theCoreIdx(0)
    , theSerial(memoryMessageSerial() )
    , thePriv(false)
    , theAnyInvs(false)
    , theDstream(true)
    , theFillLevel(eUnknown)
    , theOutstandingMessages(0)
    , theAckRequired(true)
    , theAckRequiresData(false)
    , theEvictHasData(false)
    , theBranchType(kNonBranch)
    , theBranchAnnul(false)
  {}
  explicit MemoryMessage(MemoryMessage & aMsg)
    : theType(aMsg.theType)
    , theAddress(aMsg.theAddress)
    , theAssociatedPC(aMsg.theAssociatedPC)
    , theData(aMsg.theData)
    , theReqSize(aMsg.theReqSize)
    , theCoreIdx(0)
    , theSerial(memoryMessageSerial() )
    , thePriv(aMsg.thePriv)
    , theAnyInvs(false)
    , theDstream(aMsg.theDstream)
    , theFillLevel(eUnknown)
    , theOutstandingMessages(0)
    , theAckRequired(aMsg.theAckRequired)
    , theAckRequiresData(aMsg.theAckRequiresData)
    , theEvictHasData(aMsg.theEvictHasData)
    , theBranchType(aMsg.theBranchType)
    , theBranchAnnul(aMsg.theBranchAnnul)
  {}

  static intrusive_ptr<MemoryMessage> newLoad(MemoryAddress anAddress, VirtualMemoryAddress aPC) {
    intrusive_ptr<MemoryMessage> msg = new MemoryMessage(LoadReq, anAddress, aPC);
    return msg;
  }
  static intrusive_ptr<MemoryMessage> newStore(MemoryAddress anAddress, VirtualMemoryAddress aPC, DataWord aData) {
    intrusive_ptr<MemoryMessage> msg = new MemoryMessage(StoreReq, anAddress, aPC, aData);
    return msg;
  }

  static intrusive_ptr<MemoryMessage> newRMW(MemoryAddress anAddress, VirtualMemoryAddress aPC, DataWord aData) {
    intrusive_ptr<MemoryMessage> msg = new MemoryMessage(RMWReq, anAddress, aPC, aData);
    return msg;
  }
  static intrusive_ptr<MemoryMessage> newCAS(MemoryAddress anAddress, VirtualMemoryAddress aPC, DataWord aData) {
    intrusive_ptr<MemoryMessage> msg = new MemoryMessage(CmpxReq, anAddress, aPC, aData);
    return msg;
  }
  static intrusive_ptr<MemoryMessage> newAtomicPreload(MemoryAddress anAddress, VirtualMemoryAddress aPC) {
    intrusive_ptr<MemoryMessage> msg = new MemoryMessage(AtomicPreloadReq, anAddress, aPC);
    return msg;
  }
  static intrusive_ptr<MemoryMessage> newStorePrefetch(MemoryAddress anAddress, VirtualMemoryAddress aPC, DataWord aData) {
    intrusive_ptr<MemoryMessage> msg = new MemoryMessage(StorePrefetchReq, anAddress, aPC, aData);
    return msg;
  }

  static intrusive_ptr<MemoryMessage> newFetch(MemoryAddress anAddress, VirtualMemoryAddress vPC) {
    intrusive_ptr<MemoryMessage> msg = new MemoryMessage(FetchReq, anAddress, vPC);
    return msg;
  }

  static intrusive_ptr<MemoryMessage> newFlush(MemoryAddress anAddress) {
    intrusive_ptr<MemoryMessage> msg = new MemoryMessage(FlushReq, anAddress);
    return msg;
  }

  const MemoryMessageType type() const {
    return theType;
  }
  const MemoryAddress address() const {
    return theAddress;
  }
  const VirtualMemoryAddress pc() const {
    return theAssociatedPC;
  }
  const DataWord data() const {
    return theData;
  }
  const int32_t reqSize() const {
    return theReqSize;
  }
  bool isPriv() const {
    return thePriv;
  }
  bool isDstream() const {
    return theDstream;
  }
  MemoryMessageType & type() {
    return theType;
  }
  MemoryAddress & address() {
    return theAddress;
  }
  VirtualMemoryAddress & pc() {
    return theAssociatedPC;
  }
  DataWord & data() {
    return theData;
  }
  int32_t & reqSize() {
    return theReqSize;
  }
  bool & priv() {
    return thePriv;
  }
  int & tl() {
    return theTL;
  }
  bool & dstream() {
    return theDstream;
  }
  void setPriv() {
    thePriv = true;
  }
  bool anyInvs() {
    return theAnyInvs;
  }
  void setInvs() {
    theAnyInvs = true;
  }
  const int32_t coreIdx() const {
    return theCoreIdx;
  }
  int32_t & coreIdx() {
    return theCoreIdx;
  }
  const tFillLevel fillLevel() const {
    return theFillLevel;
  }
  tFillLevel & fillLevel() {
    return theFillLevel;
  }
  uint32_t serial() const {
    return theSerial;
  }
  const tFillType fillType() const {
    return theFillType;
  }
  tFillType & fillType() {
    return theFillType;
  }

  const int32_t outstandingMsgs() const {
    return theOutstandingMessages;
  }
  int32_t & outstandingMsgs() {
    return theOutstandingMessages;
  }

  bool & ackRequired() {
    return theAckRequired;
  }

  bool ackRequired() const {
    return theAckRequired;
  }

  bool ackRequiresData() const {
    return theAckRequiresData;
  }

  bool & ackRequiresData() {
    return theAckRequiresData;
  }

  bool evictHasData() const {
    return theEvictHasData;
  }

  bool & evictHasData() {
    return theEvictHasData;
  }

  eBranchType & branchType() {
    return theBranchType;
  }

  bool & branchAnnul() {
    return theBranchAnnul;
  }

  bool isRequest() const {
    switch (theType) {
      case LoadReq:
      case StoreReq:
      case StorePrefetchReq:
      case NonAllocatingStoreReq:
      case FetchReq:
      case RMWReq:
      case CmpxReq:
      case AtomicPreloadReq:
      case FlushReq:
      case ReadReq:
      case WriteReq:
      case WriteAllocate:
      case UpgradeReq:
      case UpgradeAllocate:
      case Flush:
      case EvictDirty:
      case EvictWritable:
      case EvictClean:
      case SVBClean:
      case Invalidate:
      case Downgrade:
      case Probe:
      case DownProbe:
      case ReturnReq:
      case StreamFetch:
      case PrefetchReadNoAllocReq:
      case PrefetchReadAllocReq:
      case PrefetchInsert:
      case PrefetchInsertWritable:
        return true;
      case LoadReply:
      case StoreReply:
      case StorePrefetchReply:
      case FetchReply:
      case RMWReply:
      case CmpxReply:
      case AtomicPreloadReply:
      case MissReply:
      case MissReplyWritable:
      case MissReplyDirty:
      case UpgradeReply:
      case NonAllocatingStoreReply:
      case InvalidateAck:
      case InvalidateNAck:
      case InvUpdateAck:
      case DowngradeAck:
      case DownUpdateAck:
      case ProbedNotPresent:
      case ProbedClean:
      case ProbedWritable:
      case ProbedDirty:
      case ReturnReply:
      case PrefetchReadReply:
      case PrefetchWritableReply:
      case PrefetchDirtyReply:
      case PrefetchReadRedundant:
      case DownProbePresent:
      case DownProbeNotPresent:
      case StreamFetchWritableReply:
      case StreamFetchRejected:
        return false;
      default:
        break;
    }
    throw FlexusException("isRequest not recognized: " + theType);
  }

  bool isWrite() const {
    switch (theType) {
      case StoreReq:
      case StorePrefetchReq:
      case NonAllocatingStoreReq:
      case RMWReq:
      case CmpxReq:
      case WriteReq:
      case WriteAllocate:
      case UpgradeReq:
      case UpgradeAllocate:
        return true;
      case Flush:
      case EvictDirty:
      case EvictWritable:
      case EvictClean:
      case SVBClean:
      case Invalidate:
      case Downgrade:
      case Probe:
      case DownProbe:
      case ReturnReq:
      case StreamFetch:
      case PrefetchReadNoAllocReq:
      case PrefetchReadAllocReq:
      case PrefetchInsert:
      case PrefetchInsertWritable:
      case FlushReq:
      case ReadReq:
      case RMWReply:
      case CmpxReply:
      case LoadReq:
      case AtomicPreloadReq:
      case LoadReply:
      case AtomicPreloadReply:
      case FetchReq:
      case FetchReply:
      case StoreReply:
      case StorePrefetchReply:
      case MissReply:
      case MissReplyWritable:
      case MissReplyDirty:
      case UpgradeReply:
      case NonAllocatingStoreReply:
      case InvalidateAck:
      case InvUpdateAck:
      case DowngradeAck:
      case DownUpdateAck:
      case ProbedNotPresent:
      case ProbedClean:
      case ProbedWritable:
      case ProbedDirty:
      case DownProbePresent:
      case DownProbeNotPresent:
      case ReturnReply:
      case PrefetchReadReply:
      case PrefetchWritableReply:
      case PrefetchDirtyReply:
      case PrefetchReadRedundant:
      case StreamFetchWritableReply:
      case StreamFetchRejected:
        return false;
      default:
        break;
    }
    throw FlexusException("isWrite unrecognizedMessage: " + theType);
  }

  bool isSnoopType()const  {
    switch (theType) {
      case Invalidate:
      case Downgrade:
      case InvalidateAck:
      case InvUpdateAck:
      case DowngradeAck:
      case DownUpdateAck:
        return true;
      default:
        break;
    }
    return false;
  }

  bool isAckType()const  {
    switch (theType) {
      case ReadAck:
      case FetchAck:
      case WriteAck:
      case UpgradeAck:
      case NASAck:
        return true;
      default:
        return false;
    }
  }

  static bool isEvictType(const MemoryMessageType & aType) {
    switch (aType) {
      case EvictDirty:
      case EvictWritable:
      case EvictClean:
        return true;
      default:
        break;
    }
    return false;
  }

  bool isEvictType()const  {
    switch (theType) {
      case EvictDirty:
      case EvictWritable:
      case EvictClean:
        return true;
      default:
        break;
    }
    return false;
  }

  bool isProbeType() const {
    switch (theType) {
      case Probe:
      case ProbedNotPresent:
      case ProbedClean:
      case ProbedWritable:
      case ProbedDirty:
      case DownProbe:
      case DownProbePresent:
      case DownProbeNotPresent:
        return true;
      default:
        break;
    }
    return false;
  }

  bool isPrefetchType() const {
    switch (theType) {
      case StreamFetch:
      case PrefetchReadNoAllocReq:
      case PrefetchReadAllocReq:
      case PrefetchInsert:
      case PrefetchInsertWritable:
      case PrefetchReadReply:
      case PrefetchWritableReply:
      case PrefetchDirtyReply:
      case PrefetchReadRedundant:
      case StreamFetchWritableReply:
      case StreamFetchRejected:
        return true;
      default:
        break;
    }
    return false;
  }

  bool usesSnoopChannel() const {
    switch (theType) {
      case FlushReq:
      case Flush:
      case EvictDirty:
      case EvictWritable:
      case EvictClean:
      case SVBClean:
      case InvalidateNAck:
      case InvalidateAck:
      case InvUpdateAck:
      case DowngradeAck:
      case DownUpdateAck:
      case ProbedNotPresent:
      case ProbedClean:
      case ProbedWritable:
      case ProbedDirty:
      case PrefetchInsert:
      case PrefetchInsertWritable:
      case DownProbe:
      case ReturnReply:
        return true;
      case LoadReq:
      case AtomicPreloadReq:
      case StoreReq:
      case StorePrefetchReq:
      case NonAllocatingStoreReq:
      case FetchReq:
      case RMWReq:
      case CmpxReq:
      case ReadReq:
      case WriteReq:
      case WriteAllocate:
      case UpgradeReq:
      case UpgradeAllocate:
      case StreamFetch:
      case PrefetchReadNoAllocReq:
      case PrefetchReadAllocReq:
        return false;
        //The rest go up the heirarchy, thus the question doesn't apply
      case Invalidate:
      case Downgrade:
      case Probe:
      case LoadReply:
      case AtomicPreloadReply:
      case StoreReply:
      case StorePrefetchReply:
      case FetchReply:
      case RMWReply:
      case CmpxReply:
      case MissReply:
      case MissReplyWritable:
      case MissReplyDirty:
      case UpgradeReply:
      case NonAllocatingStoreReply:
      case PrefetchReadReply:
      case PrefetchWritableReply:
      case PrefetchDirtyReply:
      case PrefetchReadRedundant:
      case DownProbePresent:
      case DownProbeNotPresent:
      case ReturnReq:
      case StreamFetchWritableReply:
      case StreamFetchRejected:
        return false;
      default:
        break;
    }
    throw FlexusException("usesSnoopChannel unrecognized message: " + theType);
  }

  bool directionToBack() const {
    switch (theType) {
      case LoadReq:
      case AtomicPreloadReq:
      case StoreReq:
      case StorePrefetchReq:
      case NonAllocatingStoreReq:
      case FetchReq:
      case RMWReq:
      case CmpxReq:
      case FlushReq: //
      case ReadReq:
      case WriteReq:
      case WriteAllocate:
      case UpgradeReq:
      case UpgradeAllocate:
      case Flush:    //
      case EvictDirty: //
      case EvictWritable: //
      case EvictClean:  //
      case SVBClean:  //
      case StreamFetch:
      case PrefetchReadNoAllocReq:
      case PrefetchReadAllocReq:
      case PrefetchInsert:
      case PrefetchInsertWritable:
      case InvalidateAck: //
      case InvUpdateAck: //
      case DowngradeAck: //
      case DownUpdateAck: //
      case ProbedNotPresent: //
      case ProbedClean: //
      case ProbedWritable: //
      case ProbedDirty: //
      case DownProbe: //
      case ReturnReply:
        return true;
      case Invalidate:
      case Downgrade:
      case Probe:
      case LoadReply:
      case AtomicPreloadReply:
      case StoreReply:
      case StorePrefetchReply:
      case FetchReply:
      case RMWReply:
      case CmpxReply:
      case MissReply:
      case MissReplyWritable:
      case MissReplyDirty:
      case UpgradeReply:
      case NonAllocatingStoreReply:
      case PrefetchReadReply:
      case PrefetchWritableReply:
      case PrefetchDirtyReply:
      case PrefetchReadRedundant:
      case DownProbePresent: //
      case DownProbeNotPresent: //
      case ReturnReq:
      case StreamFetchWritableReply:
      case StreamFetchRejected:
        return false;
      default:
        break;
    }
    throw FlexusException("directionToBack unrecognized message: " + theType);
  }

  int32_t messageSize() {
    switch (theType) {
      case FetchReq:
      case ReadReq:
      case WriteReq:
      case UpgradeReq:
      case EvictWritable:
      case EvictClean:
      case UpgradeReply:
      case NonAllocatingStoreReply:
      case Invalidate:
      case InvalidateAck:
      case InvalidateNAck:
      case FetchFwd:
      case ReadFwd:
      case WriteFwd:
      case FwdNAck:
      case WriteAck:
      case UpgradeAck:
      case NASAck:
      case ReadNAck:
      case FetchNAck:
      case WriteNAck:
      case UpgradeNAck:
      case NASNAck:
      case MissNotify:
      case BackInvalidate:
      case ProtocolMessage:
      case EvictAck:
      case WriteRetry:
        return HEADER_SIZE;
        break;
      case ReadAck:
      case FetchAck:
        if (theAckRequiresData) {
          return HEADER_SIZE + theReqSize;
        } else {
          return HEADER_SIZE;
        }
        break;
      case ReadAckDirty:
      case FetchAckDirty:
      case MissNotifyData:
      case NonAllocatingStoreReq:
      case FwdReply:
      case FwdReplyOwned:
      case FwdReplyWritable:
      case FwdReplyDirty:
      case InvUpdateAck:
      case FetchReply:
      case MissReply:
      case MissReplyWritable:
      case MissReplyDirty:
      case EvictDirty:
        return HEADER_SIZE + theReqSize;
        break;
      default:
        //throw FlexusException("No size for message type: " + theType);
        return -1;
        break;
    }
  }

  bool directionToFront() const {
    return !directionToBack();
  }

  static MemoryMessageType maxProbe(const MemoryMessageType a, const MemoryMessageType b) {
    if ( (a == ProbedDirty) || (b == ProbedDirty) ) {
      return ProbedDirty;
    }
    if ( (a == ProbedWritable) || (b == ProbedWritable) ) {
      return ProbedWritable;
    }
    if ( (a == ProbedClean) || (b == ProbedClean) ) {
      return ProbedClean;
    }
    return ProbedNotPresent;
  }

private:
  MemoryMessageType theType;
  MemoryAddress theAddress;
  VirtualMemoryAddress theAssociatedPC;
  DataWord theData;
  int32_t theReqSize;
  int32_t  theCoreIdx;
  uint32_t theSerial;
  bool thePriv;
  int theTL;
  bool theAnyInvs;
  bool theDstream;
  tFillLevel theFillLevel;
  tFillType theFillType;
  int32_t theOutstandingMessages;
  bool theAckRequired;
  bool theAckRequiresData;
  bool theEvictHasData;
  eBranchType theBranchType;
  bool theBranchAnnul;
};

std::ostream & operator << (std::ostream & s, MemoryMessage const & aMemMsg);
std::ostream & operator << (std::ostream & s, MemoryMessage::MemoryMessageType const & aMemMsgType);

} //End MemoryCommon
} //End Flexus

#endif //FLEXUS_SLICES__MEMORYMESSAGE_HPP_INCLUDED
