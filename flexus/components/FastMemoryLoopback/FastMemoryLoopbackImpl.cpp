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
#include <core/stats.hpp>
#include <components/FastMemoryLoopback/FastMemoryLoopback.hpp>
#include <components/FastMemoryLoopback/MemStats.hpp>


#define FLEXUS_BEGIN_COMPONENT FastMemoryLoopback
#include FLEXUS_BEGIN_COMPONENT_IMPLEMENTATION()

#define DBG_DefineCategories FastMemoryLoopback
#define DBG_SetDefaultOps AddCat(FastMemoryLoopback)
#include DBG_Control()

namespace nFastMemoryLoopback {

using namespace Flexus;
using namespace Flexus::Core;
using namespace Flexus::SharedTypes;
namespace Stat = Flexus::Stat;

class FLEXUS_COMPONENT(FastMemoryLoopback) {
  FLEXUS_COMPONENT_IMPL( FastMemoryLoopback );

  MemStats *theStats;

public:
  FLEXUS_COMPONENT_CONSTRUCTOR(FastMemoryLoopback)
    : base( FLEXUS_PASS_CONSTRUCTOR_ARGS )
  {}

  // Initialization
  void initialize() {
	  theStats = new MemStats(statName());
  }
  
  void finalize() {}

  // Ports
  FLEXUS_PORT_ALWAYS_AVAILABLE(FromCache);
  void push(interface::FromCache const &, MemoryMessage & message) {
   
    DBG_(Iface, Addr(message.address()) ( << "request received: " << message ) );
	switch (message.type()) {
      case MemoryMessage::LoadReq:
      case MemoryMessage::FetchReq:
        message.type() = MemoryMessage::MissReply;
        message.fillLevel() = eLocalMem;
        (theStats->theReadRequests_stat)++;
        break;
      case MemoryMessage::ReadReq:
      case MemoryMessage::PrefetchReadNoAllocReq:
      case MemoryMessage::PrefetchReadAllocReq:
        message.type() = MemoryMessage::MissReply;
        message.fillLevel() = eLocalMem;
        (theStats->theReadRequests_stat)++;
        break;
      case MemoryMessage::StoreReq:
      case MemoryMessage::StorePrefetchReq:
      case MemoryMessage::RMWReq:
      case MemoryMessage::CmpxReq:
      case MemoryMessage::WriteReq:
      case MemoryMessage::WriteAllocate:      
        (theStats->theWriteRequests_stat)++;
      break;
      case MemoryMessage::UpgradeReq:
      case MemoryMessage::UpgradeAllocate:
        message.type() = MemoryMessage::MissReplyWritable;
        message.fillLevel() = eLocalMem;        
        (theStats->theUpgradeRequest_stat)++;
        break;
      case MemoryMessage::EvictDirty:
          (theStats->theEvictDirtys_stat)++;     
               
          break;
      case MemoryMessage::EvictWritable:
    	  (theStats->theEvictWritables_stat)++;    	  
    	  break;
      case MemoryMessage::EvictClean:
        // no response necessary (and no state to track here)
    	  (theStats->theEvictCleans_stat)++;    	  
          break;
      case MemoryMessage::NonAllocatingStoreReq:
        message.type() = MemoryMessage::NonAllocatingStoreReply;
        message.fillLevel() = eLocalMem;
        (theStats->theNonAllocatingStoreReq_stat)++;        
        break;
      default:
        DBG_Assert(false, ( << "unknown request received: " << message));
    }
  }

  FLEXUS_PORT_ALWAYS_AVAILABLE(DMA);
  void push(interface::DMA const &, MemoryMessage & message) {
    DBG_( Iface, Addr(message.address()) ( << "DMA " << message ));
    if (message.type() == MemoryMessage::WriteReq) {
      (theStats->theWriteDMA_stat)++;      
      DBG_( Trace, Addr(message.address()) ( << "Sending Invalidate in response to DMA " << message));
      message.type() = MemoryMessage::Invalidate;
      FLEXUS_CHANNEL( ToCache ) << message;
    } else {
      (theStats->theReadDMA_stat)++;      
      DBG_Assert( message.type() == MemoryMessage::ReadReq );
      message.type() = MemoryMessage::Downgrade;
      FLEXUS_CHANNEL( ToCache ) << message;
    }

  }

  void drive( interface::UpdateStatsDrive const &) {
    theStats->update();
  }

};

}//End namespace nFastMemoryLoopback

FLEXUS_COMPONENT_INSTANTIATOR( FastMemoryLoopback, nFastMemoryLoopback);

#include FLEXUS_END_COMPONENT_IMPLEMENTATION()
#define FLEXUS_END_COMPONENT FastMemoryLoopback

#define DBG_Reset
#include DBG_Control()
