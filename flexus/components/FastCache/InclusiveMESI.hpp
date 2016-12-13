// DO-NOT-REMOVE begin-copyright-block 
//
// Redistributions of any form whatsoever must retain and/or include the
// following acknowledgment, notices and disclaimer:
//
// This product includes software developed by Carnegie Mellon University.
//
// Copyright 2012 by Mohammad Alisafaee, Eric Chung, Michael Ferdman, Brian 
// Gold, Jangwoo Kim, Pejman Lotfi-Kamran, Onur Kocberber, Djordje Jevdjic, 
// Jared Smolens, Stephen Somogyi, Evangelos Vlachos, Stavros Volos, Jason 
// Zebchuk, Babak Falsafi, Nikos Hardavellas and Tom Wenisch for the SimFlex 
// Project, Computer Architecture Lab at Carnegie Mellon, Carnegie Mellon University.
//
// For more information, see the SimFlex project website at:
//   http://www.ece.cmu.edu/~simflex
//
// You may not use the name "Carnegie Mellon University" or derivations
// thereof to endorse or promote products derived from this software.
//
// If you modify the software you must place a notice on or within any
// modified version provided or made available to any third party stating
// that you have modified the software.  The notice shall include at least
// your name, address, phone number, email address and the date and purpose
// of the modification.
//
// THE SOFTWARE IS PROVIDED "AS-IS" WITHOUT ANY WARRANTY OF ANY KIND, EITHER
// EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO ANY WARRANTY
// THAT THE SOFTWARE WILL CONFORM TO SPECIFICATIONS OR BE ERROR-FREE AND ANY
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE,
// TITLE, OR NON-INFRINGEMENT.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
// BE LIABLE FOR ANY DAMAGES, INCLUDING BUT NOT LIMITED TO DIRECT, INDIRECT,
// SPECIAL OR CONSEQUENTIAL DAMAGES, ARISING OUT OF, RESULTING FROM, OR IN
// ANY WAY CONNECTED WITH THIS SOFTWARE (WHETHER OR NOT BASED UPON WARRANTY,
// CONTRACT, TORT OR OTHERWISE).
//
// DO-NOT-REMOVE end-copyright-block   
#ifndef FLEXUS_FASTCACHE_INCLUSIVEMESI_HPP_INCLUDED
#define FLEXUS_FASTCACHE_INCLUSIVEMESI_HPP_INCLUDED

#include "CacheStats.hpp"

namespace nFastCache {

class InclusiveMESI : public CoherenceProtocol {
public:
  InclusiveMESI(bool using_traces, message_function_t fwd, message_function_t cnt, invalidate_function_t inval, bool DowngradeLRU = false, bool SnoopLRU = false)
    : CoherenceProtocol(fwd, cnt, inval) {

#if 0
    DECLARE_REQUEST_ACTION( kModified, kReadAccess, SendNone, NoAllocate, SameState, UpdateLRU, FillWritable, HitReadModified );
    DECLARE_REQUEST_ACTION( kModified, kWriteAccess, SendNone, NoAllocate, SameState, UpdateLRU, FillWritable, HitWriteModified );
    DECLARE_REQUEST_ACTION( kModified, kFetchAccess, SendNone, NoAllocate, SameState, UpdateLRU, FillValid,  HitFetchModified );
    DECLARE_REQUEST_ACTION( kModified, kUpgrade,  SendNone, NoAllocate, SameState, UpdateLRU, FillWritable, HitUpgradeModified );
    DECLARE_REQUEST_ACTION( kModified, kEvictClean, SendNone, NoAllocate, SameState, UpdateLRU, NoResponse, HitEvictModified );
    DECLARE_REQUEST_ACTION( kModified, kEvictWritable, SendNone, NoAllocate, SameState, UpdateLRU, NoResponse, HitEvictWModified );
    DECLARE_REQUEST_ACTION( kModified, kEvictDirty, SendNone, NoAllocate, SameState, UpdateLRU, NoResponse, HitEvictDModified );
#endif
    DECLARE_REQUEST_ACTION( kModified, kReadAccess, SendNone, NoAllocate, DependState, UpdateLRU, FillDirty, HitReadModified );
    DECLARE_REQUEST_ACTION( kModified, kWriteAccess, SendNone, NoAllocate, E_State,  UpdateLRU, FillDirty, HitWriteModified );
    DECLARE_REQUEST_ACTION( kModified, kStoreAccess, SendNone, NoAllocate, SameState,  UpdateLRU, FillDirty, HitWriteModified );
    DECLARE_REQUEST_ACTION( kModified, kNAWAccess,  SendNone, NoAllocate, SameState,  UpdateLRU, FillDirty, HitNAWModified );
    DECLARE_REQUEST_ACTION( kModified, kFetchAccess, SendNone, NoAllocate, SameState,  UpdateLRU, FillValid,  HitFetchModified );
    DECLARE_REQUEST_ACTION( kModified, kUpgrade,  SendNone, NoAllocate, E_State,  UpdateLRU, FillDirty, HitUpgradeModified );
    DECLARE_REQUEST_ACTION( kModified, kEvictClean, SendNone, NoAllocate, SameState,  UpdateLRU, NoResponse, HitEvictModified );
    DECLARE_REQUEST_ACTION( kModified, kEvictWritable, POISON,  POISON,  POISON,  POISON,  POISON,  HitEvictWModified );
    DECLARE_REQUEST_ACTION( kModified, kEvictDirty, POISON,  POISON,  POISON,  POISON,  POISON,  HitEvictDModified );

    DECLARE_REQUEST_ACTION( kExclusive, kReadAccess, SendNone, NoAllocate, SameState, UpdateLRU, FillValid,   HitReadExclusive );
    DECLARE_REQUEST_ACTION( kExclusive, kWriteAccess, SendNone, NoAllocate, SameState, UpdateLRU, FillWritable, HitWriteExclusive );
    DECLARE_REQUEST_ACTION( kExclusive, kStoreAccess, SendNone, NoAllocate, M_State, UpdateLRU, FillWritable, HitWriteExclusive );
    DECLARE_REQUEST_ACTION( kExclusive, kNAWAccess,  SendNone, NoAllocate, M_State, UpdateLRU, FillWritable, HitNAWExclusive );
    DECLARE_REQUEST_ACTION( kExclusive, kFetchAccess, SendNone, NoAllocate, SameState, UpdateLRU, FillValid,  HitFetchExclusive );
    DECLARE_REQUEST_ACTION( kExclusive, kUpgrade,  SendNone, NoAllocate, SameState, UpdateLRU, FillWritable, HitUpgradeExclusive );
    DECLARE_REQUEST_ACTION( kExclusive, kEvictClean, SendNone, NoAllocate, SameState, UpdateLRU, NoResponse,  HitEvictExclusive );
    DECLARE_REQUEST_ACTION( kExclusive, kEvictWritable, SendNone, NoAllocate, SameState, UpdateLRU, NoResponse,  HitEvictWExclusive );
    DECLARE_REQUEST_ACTION( kExclusive, kEvictDirty, SendNone, NoAllocate, M_State, UpdateLRU, NoResponse,  HitEvictDExclusive);

    DECLARE_REQUEST_ACTION( kShared, kReadAccess, SendNone,  NoAllocate, SameState, UpdateLRU, FillValid,  HitReadShared );
    DECLARE_REQUEST_ACTION( kShared, kWriteAccess, SendUpgrade, NoAllocate, E_State, UpdateLRU, FillWritable, MissWriteShared );
    DECLARE_REQUEST_ACTION( kShared, kStoreAccess, SendUpgrade, NoAllocate, M_State, UpdateLRU, FillWritable, MissWriteShared );
    DECLARE_REQUEST_ACTION( kShared, kNAWAccess,  SendUpgrade, NoAllocate, M_State, UpdateLRU, FillWritable, MissNAWShared );
    DECLARE_REQUEST_ACTION( kShared, kFetchAccess, SendNone,  NoAllocate, SameState, UpdateLRU, FillValid,  HitFetchShared );
    DECLARE_REQUEST_ACTION( kShared, kUpgrade,  SendUpgrade, NoAllocate, E_State, UpdateLRU, FillWritable, MissUpgradeShared );
    DECLARE_REQUEST_ACTION( kShared, kEvictClean, SendNone,  NoAllocate, SameState, UpdateLRU, NoResponse,  HitEvictShared );
    if (using_traces) {
      DECLARE_REQUEST_ACTION( kShared, kEvictWritable, SendNone, NoAllocate, SameState, UpdateLRU, NoResponse,  MissEvictWShared );
      DECLARE_REQUEST_ACTION( kShared, kEvictDirty, SendUpgrade, NoAllocate, M_State, UpdateLRU, NoResponse,  MissEvictDShared );
    } else {
      DECLARE_REQUEST_ACTION( kShared, kEvictWritable, POISON,  POISON,  POISON,  POISON,  POISON,   MissEvictWShared );
      DECLARE_REQUEST_ACTION( kShared, kEvictDirty, POISON,  POISON,  POISON,  POISON,  POISON,   MissEvictDShared );
    }

    DECLARE_REQUEST_ACTION( kInvalid, kReadAccess, SendRead, Allocate, DependState, UpdateLRU, FillDepend, MissReadInvalid );
    DECLARE_REQUEST_ACTION( kInvalid, kWriteAccess, SendWrite, Allocate, E_State,  UpdateLRU, FillDepend, MissWriteInvalid );
    DECLARE_REQUEST_ACTION( kInvalid, kStoreAccess, SendWrite, Allocate, M_State,  UpdateLRU, FillDepend, MissWriteInvalid );
    DECLARE_REQUEST_ACTION( kInvalid, kNAWAccess,  SendNAW, NoAllocate, SameState, NoUpdateLRU, FillDepend, MissNAWInvalid );
    DECLARE_REQUEST_ACTION( kInvalid, kFetchAccess, SendFetch, Allocate, DependState, UpdateLRU, FillValid, MissFetchInvalid );

    if (using_traces) {
      DECLARE_REQUEST_ACTION( kInvalid, kUpgrade,  SendWrite, Allocate,  M_State, UpdateLRU,  FillDepend,  MissUpgradeInvalid );
      DECLARE_REQUEST_ACTION( kInvalid, kEvictClean, SendNone, NoAllocate, SameState, NoUpdateLRU, NoResponse,  MissEvictInvalid );
      DECLARE_REQUEST_ACTION( kInvalid, kEvictWritable, SendNone, NoAllocate, SameState, NoUpdateLRU, NoResponse,  MissEvictWInvalid );
      DECLARE_REQUEST_ACTION( kInvalid, kEvictDirty, SendNone, NoAllocate, SameState, NoUpdateLRU, NoResponse,  MissEvictDInvalid );
    } else {
      DECLARE_REQUEST_ACTION( kInvalid, kUpgrade,  POISON,  POISON,  POISON,  POISON,  POISON,  MissUpgradeInvalid );
      DECLARE_REQUEST_ACTION( kInvalid, kEvictClean, POISON,  POISON,  POISON,  POISON,  POISON,  MissEvictInvalid );
      DECLARE_REQUEST_ACTION( kInvalid, kEvictWritable, POISON,  POISON,  POISON,  POISON,  POISON,  MissEvictWInvalid );
      DECLARE_REQUEST_ACTION( kInvalid, kEvictDirty, POISON,  POISON,  POISON,  POISON,  POISON,  MissEvictDInvalid );
    }

#define MM MemoryMessage

    if (SnoopLRU) {
      // The arguments here are really template parameters
      // This means they have to be constants, so we have to replace SnoopLRU with true or false as the last argument
      DECLARE_SNOOP_ACTION( kModified, MM::ReturnReq, MM::ReturnReplyDirty, NoSnoop, S_State, HitReturnReqModified, false, true );
      DECLARE_SNOOP_ACTION( kExclusive, MM::ReturnReq, MM::ReturnReply, DoSnoop, S_State, HitReturnReqExclusive, false, true );
    } else {
      DECLARE_SNOOP_ACTION( kModified, MM::ReturnReq, MM::ReturnReplyDirty, NoSnoop, S_State, HitReturnReqModified, false, false );
      DECLARE_SNOOP_ACTION( kExclusive, MM::ReturnReq, MM::ReturnReply, DoSnoop, S_State, HitReturnReqExclusive, false, false );
    }
    DECLARE_SNOOP_ACTION( kShared, MM::ReturnReq, MM::ReturnReply, NoSnoop, SameState, HitReturnReqShared, false, false );
    DECLARE_SNOOP_ACTION( kInvalid, MM::ReturnReq, MM::ReturnNAck,  NoSnoop, SameState, MissReturnReqInvalid, false, false );

    DECLARE_SNOOP_ACTION( kModified, MM::Invalidate, MM::InvUpdateAck, DoSnoop, I_State, HitInvalidateModified, false, true );
    DECLARE_SNOOP_ACTION( kExclusive, MM::Invalidate, SnoopDepend,  DoSnoop, I_State, HitInvalidateExclusive, false, true );
    DECLARE_SNOOP_ACTION( kShared, MM::Invalidate, MM::InvalidateAck, DoSnoop, I_State, HitInvalidateShared, false, true );
    DECLARE_SNOOP_ACTION( kInvalid, MM::Invalidate, MM::Invalidate,  NoSnoop, SameState, MissInvalidateInvalid, false, false );

    if (DowngradeLRU) {
      // See comment for SnoopLRU above
      DECLARE_SNOOP_ACTION( kModified, MM::Downgrade, MM::DownUpdateAck, DoSnoop, S_State,  HitDowngradeModified, false, true );
      DECLARE_SNOOP_ACTION( kExclusive, MM::Downgrade, SnoopDepend,  DoSnoop, S_State,  HitDowngradeExclusive, false, true );
    } else {
      DECLARE_SNOOP_ACTION( kModified, MM::Downgrade, MM::DownUpdateAck, DoSnoop, S_State,  HitDowngradeModified, false, true );
      DECLARE_SNOOP_ACTION( kExclusive, MM::Downgrade, SnoopDepend,  DoSnoop, S_State,  HitDowngradeExclusive, false, true );
    }
    DECLARE_SNOOP_ACTION( kShared, MM::Downgrade, MM::DowngradeAck, NoSnoop, SameState,  HitDowngradeShared, false, false );
    DECLARE_SNOOP_ACTION( kInvalid, MM::Downgrade, MM::DowngradeAck, NoSnoop, SameState,  MissDowngradeInvalid, false, false );
#undef MM

  }

  // Need to maintain inclusion on evictions
  virtual MemoryMessage::MemoryMessageType evict(uint64_t tagset, CoherenceState_t state) {
    bool was_dirty = sendInvalidate(tagset, true, true);
    if (was_dirty || state == kModified) {
      return MemoryMessage::EvictDirty;
    } else if (state == kExclusive) {
      return MemoryMessage::EvictWritable;
    }
    return MemoryMessage::EvictClean;
  }
};

}

#endif
