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
#ifndef FLEXUS_FASTCACHE_COHERENCE_PROTOCOL_HPP_INCLUDED
#define FLEXUS_FASTCACHE_COHERENCE_PROTOCOL_HPP_INCLUDED

#include <core/debug/debug.hpp>

#include "CacheStats.hpp"
#include "LookupResult.hpp"

#include <iostream>

namespace nFastCache {

class CoherenceProtocol {
public:

  // Define access types
  typedef uint16_t access_t;
  static const access_t kReadAccess = 0x0;
  static const access_t kWriteAccess = 0x1;
  static const access_t kFetchAccess = 0x2;
  static const access_t kUpgrade  = 0x3;
  static const access_t kEvictClean = 0x4;
  static const access_t kEvictWritable = 0x5;
  static const access_t kEvictDirty = 0x6;
  static const access_t kStoreAccess = 0x7;
  static const access_t kNAWAccess = 0x8;
  static const access_t kUnknownAccess = 0xFFFF;
  virtual std::string Short2Req(access_t r) {
    switch (r) {
      case kReadAccess:
        return "Read";
        break;
      case kWriteAccess:
        return "Write";
        break;
      case kFetchAccess:
        return "Fetch";
        break;
      case kUpgrade:
        return "Upgrade";
        break;
      case kEvictClean:
        return "EvictClean";
        break;
      case kEvictWritable:
        return "EvictWritable";
        break;
      case kEvictDirty:
        return "EvictDirty";
        break;
      case kStoreAccess:
        return "Store";
        break;
      case kNAWAccess:
        return "NonAllocateWrite";
        break;
      default:
        return "Unknown";
        break;
    }
  }

  static access_t message2access(MemoryMessage::MemoryMessageType type) {
    switch (type) {
      case MemoryMessage::ReadReq:
        return kReadAccess;
        break;
      case MemoryMessage::WriteReq:
        return kWriteAccess;
        break;
      case MemoryMessage::FetchReq:
        return kFetchAccess;
        break;
      case MemoryMessage::UpgradeReq:
        return kUpgrade;
        break;
      case MemoryMessage::EvictClean:
        return kEvictClean;
        break;
      case MemoryMessage::EvictWritable:
        return kEvictWritable;
        break;
      case MemoryMessage::EvictDirty:
        return kEvictDirty;
        break;

        // Add support for use as an L1 cache
      case MemoryMessage::LoadReq:
        return kReadAccess;
        break;
      case MemoryMessage::RMWReq:
      case MemoryMessage::CmpxReq:
      case MemoryMessage::StoreReq:
        return kStoreAccess;
        break;

      case MemoryMessage::NonAllocatingStoreReq:
        return kNAWAccess;
        break;

        // Perfectly Legitimate Processor requests
        // But we don't expect them in functional simulations
      case MemoryMessage::StorePrefetchReq:
      case MemoryMessage::FlushReq:
      case MemoryMessage::WriteAllocate:
      case MemoryMessage::UpgradeAllocate:
      case MemoryMessage::Flush:
        DBG_Assert(false, ( << "Unexpected memory message type " << type ));
        break;

        // Reply Types are not access types
      case MemoryMessage::LoadReply:
      case MemoryMessage::StoreReply:
      case MemoryMessage::StorePrefetchReply:
      case MemoryMessage::FetchReply:
      case MemoryMessage::RMWReply:
      case MemoryMessage::CmpxReply:
      case MemoryMessage::MissReply:
      case MemoryMessage::MissReplyWritable:
      case MemoryMessage::MissReplyDirty:
      case MemoryMessage::UpgradeReply:
        DBG_Assert(false, ( << "Asked to map Reply type " << type << " to access type." ));
        break;

        // Back-side request types
      case MemoryMessage::Invalidate:
      case MemoryMessage::Downgrade:
      case MemoryMessage::Probe:
      case MemoryMessage::DownProbe:
      case MemoryMessage::ReturnReq:
        DBG_Assert(false, ( << "Asked to map Backside request type " << type << " to access type." ));
        break;

        // Back-side response types
      case MemoryMessage::InvalidateAck:
      case MemoryMessage::InvUpdateAck:
      case MemoryMessage::DowngradeAck:
      case MemoryMessage::DownUpdateAck:
      case MemoryMessage::ProbedNotPresent:
      case MemoryMessage::ProbedClean:
      case MemoryMessage::ProbedWritable:
      case MemoryMessage::ProbedDirty:
      case MemoryMessage::DownProbePresent:
      case MemoryMessage::DownProbeNotPresent:
      case MemoryMessage::ReturnReply:
        DBG_Assert(false, ( << "Asked to map Backside response type " << type << " to access type." ));
        break;

        // Special request types
      case MemoryMessage::StreamFetch:
      case MemoryMessage::PrefetchReadNoAllocReq:
      case MemoryMessage::PrefetchReadAllocReq:
      case MemoryMessage::PrefetchInsert:
      case MemoryMessage::PrefetchInsertWritable:
      case MemoryMessage::PrefetchReadReply:
      case MemoryMessage::PrefetchWritableReply:
      case MemoryMessage::PrefetchDirtyReply:
      case MemoryMessage::PrefetchReadRedundant:
      case MemoryMessage::SVBClean:
        DBG_Assert(false, ( << "Asked to map Special Request/Response type " << type << " to access type." ));
        break;

      default:
        DBG_Assert(false, ( << "Asked to map unknown type " << type ));
        break;
    }

    return kUnknownAccess;
  }

  static const int16_t POISON = 0;
  static const int16_t SendNone = -1;
  static const int16_t SendUpgrade = -2;
  static const int16_t SendWrite = -3;
  static const int16_t SendRead = -4;
  static const int16_t SendFetch = -5;
  static const int16_t NoAllocate = -6;
  static const int16_t Allocate = -7;
  static const int16_t SameState = -8;
  static const int16_t M_State = -9;
  static const int16_t O_State = -10;
  static const int16_t E_State = -11;
  static const int16_t S_State = -12;
  static const int16_t I_State = -13;
  static const int16_t UpdateLRU = -14;
  static const int16_t NoUpdateLRU = -15;
  static const int16_t FillWritable = -16;
  static const int16_t FillValid = -17;
  static const int16_t FillDepend = -18;
  static const int16_t FillDirty = -19;
  static const int16_t NoResponse = -20;
  static const int16_t DependState = -21;
  static const int16_t SnoopDepend = -22;
  static const int16_t MigratoryDepend = -23;
  static const int16_t DoSnoop = -24;
  static const int16_t NoSnoop = -25;
  static const int16_t SendNAW = -27;

  // Template for coherence actions
  // The idea is to create specialized versions for any combinations that actually happen
  // And then have a map of <state,request> to specific actions
  template <int16_t Send, int16_t Alloc, int16_t NState, int16_t Order, int16_t Response>
  void doCoherenceAction(LookupResult_p lookup, MemoryMessage & message) {
    DBG_Assert(false, ( << "Invalid Request+State combination: CurState = " << Short2State(lookup->getState()) << ", Message = " << message ) );
  }

  // Generic Snoop Action template
  // These are function objects to allow for partial specialization, yay!
  class BaseSnoopAction {
  public:
    BaseSnoopAction(CoherenceProtocol * proto) : protocol(proto) {}
    virtual ~BaseSnoopAction() {}
    virtual void operator()(LookupResult_p lookup, MemoryMessage & message) = 0;
  protected:
    CoherenceProtocol * protocol;
  };

  template<typename T, T Resp, int16_t Snoop, int16_t NState, bool MakeMRU, bool MakeLRU>
  class SnoopAction : public BaseSnoopAction {
  public:
    SnoopAction(CoherenceProtocol * proto) : BaseSnoopAction(proto) {
      DBG_Assert(NState == POISON);
    }
    virtual void operator()(LookupResult_p lookup, MemoryMessage & message) {
      DBG_(Dev, ( << "Type: " << Resp << ", Snoop: " << Snoop << ", NState: " << NState ));
      DBG_Assert(false, ( << "Invalid SnoopRequest+State combination: CurState = " << Short2State(lookup->getState()) << ", Message = " << message ) );
    }
  };

  typedef void (CoherenceProtocol::*CoherenceActionPtr)(LookupResult_p lookup, MemoryMessage & message);
  typedef int64_t CacheStats::* StatMemberPtr;

  std::pair<CoherenceActionPtr, StatMemberPtr> getCoherenceAction(CoherenceState_t s, access_t a) {
    coherence_map_t::iterator iter = coherence_map.find(std::make_pair(s, a));
    if (iter == coherence_map.end()) {
      DBG_Assert(false, ( << "Invalid Coherence State+Request Pair: " << Short2State(s) << "," << Short2Req(a) ));
    }
    return iter->second;
  }

  // Stuff for handling Snoops

  std::pair<BaseSnoopAction *, StatMemberPtr> getSnoopAction(CoherenceState_t s, MemoryMessage::MemoryMessageType t) {
    snoop_map_t::iterator iter = snoop_map.find(std::make_pair(s, t));
    if (iter == snoop_map.end()) {
      DBG_Assert(false, ( << "Invalid Snoop State+Request Pair: " << Short2State(s) << "," << t ));
    }
    return iter->second;
  }

  typedef std::function<void(MemoryMessage & message)> message_function_t;
  typedef std::function<bool(uint64_t, bool, bool)> invalidate_function_t;

  CoherenceProtocol(message_function_t fwd, message_function_t cnt, invalidate_function_t inval)
    : forwardMessage(fwd), continueSnoop(cnt), sendInvalidate(inval) {}

  virtual ~CoherenceProtocol() {
    clean_snoop_map();
  }

  virtual MemoryMessage::MemoryMessageType evict(uint64_t tagset, CoherenceState_t state) = 0;

protected:
  typedef std::map<std::pair<CoherenceState_t, access_t>, std::pair<CoherenceActionPtr, StatMemberPtr> > coherence_map_t;
  typedef std::map<std::pair<CoherenceState_t, MemoryMessage::MemoryMessageType>, std::pair<BaseSnoopAction *, StatMemberPtr> > snoop_map_t;
  coherence_map_t coherence_map;
  snoop_map_t snoop_map;

  message_function_t forwardMessage;
  message_function_t continueSnoop;
  invalidate_function_t sendInvalidate;

#define zxstr(s) zstr(s)
#define zstr(s) #s

#define DECLARE_REQUEST_ACTION(state, req, send, alloc, nstate, order, resp, stat) \
   (coherence_map[std::make_pair(state,req)] = std::make_pair(&CoherenceProtocol::doCoherenceAction<send,alloc,nstate,order,resp>, &CacheStats::stat))

#define DECLARE_SNOOP_ACTION(state, req, resp, snoop, nstate, stat, mru, lru) \
   (snoop_map[std::make_pair(state,req)] = std::make_pair(new SnoopAction<__typeof__(resp), resp, snoop, nstate, mru, lru>(this), &CacheStats::stat))

  void clean_snoop_map() {
    snoop_map_t::iterator iter = snoop_map.begin();
    for (; iter != snoop_map.end(); iter++) {
      delete iter->second.first;
      iter->second.first = nullptr;
    }
    snoop_map.clear();
  }
};

#define CP CoherenceProtocol

// Required specializations
// Only a few actual cases
template<>
void CP::doCoherenceAction< CP::SendNone, CP::NoAllocate, CP::SameState, CP::NoUpdateLRU, CP::NoResponse>(LookupResult_p lookup, MemoryMessage & message) {
}

template<>
void CP::doCoherenceAction< CP::SendNone, CP::NoAllocate, CP::SameState, CP::UpdateLRU, CP::FillWritable>(LookupResult_p lookup, MemoryMessage & message) {

  lookup->updateLRU();
  message.type() = MemoryMessage::MissReplyWritable;

}

template<>
void CP::doCoherenceAction< CP::SendNone, CP::NoAllocate, CP::DependState, CP::UpdateLRU, CP::FillDirty>(LookupResult_p lookup, MemoryMessage & message) {

  if (message.type() == MemoryMessage::LoadReq) {
    lookup->updateLRU();
  } else {
    lookup->changeState(kExclusive, true, false);
  }
  message.type() = MemoryMessage::MissReplyDirty;

}

template<>
void CP::doCoherenceAction< CP::SendNone, CP::NoAllocate, CP::SameState, CP::UpdateLRU, CP::FillDirty>(LookupResult_p lookup, MemoryMessage & message) {

  lookup->updateLRU();
  message.type() = MemoryMessage::MissReplyDirty;

}

template<>
void CP::doCoherenceAction< CP::SendNone, CP::NoAllocate, CP::E_State, CP::UpdateLRU, CP::FillDirty>(LookupResult_p lookup, MemoryMessage & message) {

  lookup->changeState(kExclusive, true, false);
  message.type() = MemoryMessage::MissReplyDirty;

}

template<>
void CoherenceProtocol::doCoherenceAction< CP::SendNone, CP::NoAllocate, CP::SameState, CP::UpdateLRU, CP::FillValid>(LookupResult_p lookup, MemoryMessage & message) {

  lookup->updateLRU();
  message.type() = MemoryMessage::MissReply;

}

template<>
void CoherenceProtocol::doCoherenceAction< CP::SendNone, CP::NoAllocate, CP::M_State, CP::UpdateLRU, CP::FillWritable>(LookupResult_p lookup, MemoryMessage & message) {

  lookup->changeState(kModified, true, false);
  message.type() = MemoryMessage::MissReplyWritable;

}

template<>
void CoherenceProtocol::doCoherenceAction< CP::SendNone, CP::NoAllocate, CP::SameState, CP::UpdateLRU, CP::NoResponse>(LookupResult_p lookup, MemoryMessage & message) {

  lookup->updateLRU();

}

template<>
void CoherenceProtocol::doCoherenceAction< CP::SendUpgrade, CP::NoAllocate, CP::M_State, CP::UpdateLRU, CP::FillWritable>(LookupResult_p lookup, MemoryMessage & message) {

  message.type() = MemoryMessage::UpgradeReq;
  forwardMessage(message);

  lookup->changeState(kModified, true, false);
  message.type() = MemoryMessage::MissReplyWritable;

}
template<>
void CoherenceProtocol::doCoherenceAction< CP::SendUpgrade, CP::NoAllocate, CP::E_State, CP::UpdateLRU, CP::FillWritable>(LookupResult_p lookup, MemoryMessage & message) {

  message.type() = MemoryMessage::UpgradeReq;
  forwardMessage(message);

  lookup->changeState(kExclusive, true, false);
  message.type() = MemoryMessage::MissReplyWritable;

}

template<>
void CoherenceProtocol::doCoherenceAction< CP::SendNone, CP::NoAllocate, CP::M_State, CP::UpdateLRU, CP::NoResponse>(LookupResult_p lookup, MemoryMessage & message) {

  lookup->changeState(kModified, true, false);

}

template<>
void CoherenceProtocol::doCoherenceAction< CP::SendNone, CP::NoAllocate, CP::E_State, CP::UpdateLRU, CP::NoResponse>(LookupResult_p lookup, MemoryMessage & message) {

  lookup->changeState(kExclusive, true, false);

}

template<>
void CoherenceProtocol::doCoherenceAction< CP::SendUpgrade, CP::NoAllocate, CP::M_State, CP::UpdateLRU, CP::NoResponse>(LookupResult_p lookup, MemoryMessage & message) {

  message.type() = MemoryMessage::UpgradeReq;
  forwardMessage(message);
  lookup->changeState(kModified, true, false);

}

template<>
void CoherenceProtocol::doCoherenceAction< CP::SendUpgrade, CP::NoAllocate, CP::E_State, CP::UpdateLRU, CP::NoResponse>(LookupResult_p lookup, MemoryMessage & message) {

  message.type() = MemoryMessage::UpgradeReq;
  forwardMessage(message);
  lookup->changeState(kExclusive, true, false);

}

template<>
void CoherenceProtocol::doCoherenceAction<CP::SendRead, CP::Allocate, CP::DependState, CP::UpdateLRU, CP::FillDepend>(LookupResult_p lookup, MemoryMessage & message) {

  bool is_load = (message.type() == MemoryMessage::LoadReq);

  message.type() = MemoryMessage::ReadReq;
  forwardMessage(message);

  CoherenceState_t new_state = kShared;
  if (message.type() == MemoryMessage::MissReply) {
    new_state = kShared;
  } else if (message.type() == MemoryMessage::MissReplyWritable) {
    new_state = kExclusive;
  } else if (message.type() == MemoryMessage::MissReplyDirty) {
    if (is_load) {
      new_state = kModified;
    } else {
      new_state = kExclusive;
    }
  } else if (message.type() == MemoryMessage::FwdReplyOwned) {
    new_state = kOwned;
    message.type() = MemoryMessage::MissReply;
  } else {
    DBG_Assert( false, ( << "Unexpected response '" << message.type() << "' to ReadReq" ));
  }
  lookup->allocate(new_state);
}

template<>
void CoherenceProtocol::doCoherenceAction< CP::SendWrite, CP::Allocate, CP::M_State, CP::UpdateLRU, CP::FillDepend>(LookupResult_p lookup, MemoryMessage & message) {

  message.type() = MemoryMessage::WriteReq;
  forwardMessage(message);
  lookup->allocate(kModified);
}

template<>
void CoherenceProtocol::doCoherenceAction< CP::SendWrite, CP::Allocate, CP::E_State, CP::UpdateLRU, CP::FillDepend>(LookupResult_p lookup, MemoryMessage & message) {

  message.type() = MemoryMessage::WriteReq;
  forwardMessage(message);
  lookup->allocate(kExclusive);
}

template<>
void CoherenceProtocol::doCoherenceAction< CP::SendFetch, CP::Allocate, CP::S_State, CP::UpdateLRU, CP::FillValid>(LookupResult_p lookup, MemoryMessage & message) {

  message.type() = MemoryMessage::FetchReq;
  forwardMessage(message);

  lookup->allocate(kShared);
  message.type() = MemoryMessage::MissReply;
}

template<>
void CoherenceProtocol::doCoherenceAction< CP::SendFetch, CP::Allocate, CP::DependState, CP::UpdateLRU, CP::FillValid>(LookupResult_p lookup, MemoryMessage & message) {

  message.type() = MemoryMessage::FetchReq;
  forwardMessage(message);

  CoherenceState_t new_state = kShared;
  if (message.type() == MemoryMessage::FwdReplyOwned) {
    new_state = kOwned;
  }

  lookup->allocate(new_state);
  message.type() = MemoryMessage::MissReply;
}

template<>
void CoherenceProtocol::doCoherenceAction< CP::SendNAW, CP::NoAllocate, CP::SameState, CP::NoUpdateLRU, CP::FillDepend>(LookupResult_p lookup, MemoryMessage & message) {
  forwardMessage(message);
}

// 6 Specializations:
#define MMType MemoryMessage::MemoryMessageType

template<MMType Resp>
class CoherenceProtocol::SnoopAction<MMType, Resp, CP::NoSnoop, CP::SameState, false, false> : public BaseSnoopAction {
public:
  SnoopAction(CoherenceProtocol * proto) : BaseSnoopAction(proto) {}
  virtual void operator()(LookupResult_p lookup, MemoryMessage & message) {
    // Should we update the LRU order here?
    message.type() = Resp;
  }
};

template<MMType Resp, bool MRU, bool LRU>
class CoherenceProtocol::SnoopAction<MMType, Resp, CP::NoSnoop, CP::S_State, MRU, LRU> : public BaseSnoopAction {
public:
  SnoopAction(CoherenceProtocol * proto) : BaseSnoopAction(proto) {}
  virtual void operator()(LookupResult_p lookup, MemoryMessage & message) {
    // Should we update the LRU order here?
    message.type() = Resp;
    lookup->changeState(kShared, MRU, LRU);
  }
};

template<MMType Resp, bool MRU, bool LRU>
class CoherenceProtocol::SnoopAction<MMType, Resp, CP::DoSnoop, CP::S_State, MRU, LRU> : public BaseSnoopAction {
public:
  SnoopAction(CoherenceProtocol * proto) : BaseSnoopAction(proto) {}
  virtual void operator()(LookupResult_p lookup, MemoryMessage & message) {
    // Should we update the LRU order here?
    protocol->continueSnoop(message);
    message.type() = Resp;
    lookup->changeState(kShared, MRU, LRU);
  }
};

template<MMType T, bool MRU, bool LRU> class CP::SnoopAction<MMType, T, CP::DoSnoop, CP::I_State, MRU, LRU> : public BaseSnoopAction {
public:
  SnoopAction(CoherenceProtocol * proto) : BaseSnoopAction(proto) {}
  virtual void operator()(LookupResult_p lookup, MemoryMessage & message) {
    // Should we update the LRU order here?
    protocol->continueSnoop(message);
    lookup->changeState(kInvalid, MRU, LRU);
    message.type() = T;
  }
};

template<MMType T, bool MRU, bool LRU> class CP::SnoopAction<MMType, T, CP::DoSnoop, CP::O_State, MRU, LRU> : public BaseSnoopAction {
public:
  SnoopAction(CoherenceProtocol * proto) : BaseSnoopAction(proto) {
  }
  virtual void operator()(LookupResult_p lookup, MemoryMessage & message) {
    // Should we update the LRU order here?
    protocol->continueSnoop(message);
    lookup->changeState(kOwned, MRU, LRU);
    message.type() = T;
  }
};

template<bool MRU, bool LRU> class CP::SnoopAction<__typeof__(CP::SnoopDepend), CP::SnoopDepend, CP::DoSnoop, CP::I_State, MRU, LRU> : public BaseSnoopAction {
public:
  SnoopAction(CoherenceProtocol * proto) : BaseSnoopAction(proto) {
  }
  virtual void operator()(LookupResult_p lookup, MemoryMessage & message) {
    // Should we update the LRU order here?
    protocol->continueSnoop(message);
    lookup->changeState(kInvalid, MRU, LRU);
    if (message.type() == MemoryMessage::Invalidate) {
      message.type() = MemoryMessage::InvalidateAck;
    }
  }
};

template<bool MRU, bool LRU> class CP::SnoopAction<__typeof__(CP::SnoopDepend), CP::SnoopDepend, CP::DoSnoop, CP::S_State, MRU, LRU> : public BaseSnoopAction {
public:
  SnoopAction(CoherenceProtocol * proto) : BaseSnoopAction(proto) {
  }
  virtual void operator()(LookupResult_p lookup, MemoryMessage & message) {
    // Should we update the LRU order here?
    protocol->continueSnoop(message);
    lookup->changeState(kShared, MRU, LRU);
    if (message.type() == MemoryMessage::Downgrade) {
      message.type() = MemoryMessage::DowngradeAck;
    }
  }
};

template<bool MRU, bool LRU> class CP::SnoopAction<__typeof__(CP::SnoopDepend), CP::SnoopDepend, CP::DoSnoop, CP::DependState, MRU, LRU> : public BaseSnoopAction {
public:
  SnoopAction(CoherenceProtocol * proto) : BaseSnoopAction(proto) {}
  virtual void operator()(LookupResult_p lookup, MemoryMessage & message) {
    // Should we update the LRU order here?
    protocol->continueSnoop(message);
    if (message.type() == MemoryMessage::DownUpdateAck) {
      lookup->changeState(kOwned, MRU, LRU);
    } else {
      lookup->changeState(kShared, MRU, LRU);
      message.type() = MemoryMessage::DowngradeAck;
    }
  }
};

template<bool MRU, bool LRU> class CP::SnoopAction<MMType, MemoryMessage::ReturnReply, CP::DoSnoop, CP::DependState, MRU, LRU> : public BaseSnoopAction {
public:
  SnoopAction(CoherenceProtocol * proto) : BaseSnoopAction(proto) {}
  virtual void operator()(LookupResult_p lookup, MemoryMessage & message) {
    // Should we update the LRU order here?
    message.type() = MemoryMessage::Downgrade;
    protocol->continueSnoop(message);
    if (message.type() == MemoryMessage::DownUpdateAck) {
      lookup->changeState(kOwned, MRU, LRU);
    } else {
      lookup->changeState(kShared, MRU, LRU);
    }
    message.type() = MemoryMessage::ReturnReply;
  }
};

};

// Grab all the specific protocols and define a generator function
#include "InclusiveMOESI.hpp"
#include "InclusiveMESI.hpp"

namespace nFastCache {

CoherenceProtocol*
GenerateCoherenceProtocol(std::string name, bool using_traces, CP::message_function_t fwd, CP::message_function_t cnt, CP::invalidate_function_t inval, bool downgrade_lru, bool snoop_lru) {
  if (name == "InclusiveMOESI") {
    return new InclusiveMOESI(using_traces, fwd, cnt, inval, downgrade_lru, snoop_lru);
  } else if (name == "InclusiveMESI") {
    return new InclusiveMESI(using_traces, fwd, cnt, inval, downgrade_lru, snoop_lru);
  } else {
    DBG_Assert(false, ( << "Unknown CoherenceProtocol '" << name << "'") );
  }
  return nullptr;
}

};

#endif
