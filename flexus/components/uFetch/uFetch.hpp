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
#include <core/simulator_layout.hpp>

#include <components/uFetch/uFetchTypes.hpp>

#include <components/CommonQEMU/Transports/MemoryTransport.hpp>
#include <components/CommonQEMU/Slices/AbstractInstruction.hpp>

#define FLEXUS_BEGIN_COMPONENT uFetch
#include FLEXUS_BEGIN_COMPONENT_DECLARATION()

COMPONENT_PARAMETERS(
  PARAMETER( FAQSize, uint32_t, "Fetch address queue size", "faq", 32 )
  PARAMETER( MaxFetchLines, uint32_t, "Max i-cache lines fetched per cycle", "flines", 2 )
  PARAMETER( MaxFetchInstructions, uint32_t, "Max instructions fetched per cycle", "finst", 10 )
  PARAMETER( ICacheLineSize, uint64_t, "Icache line size in bytes", "iline", 64 )
  PARAMETER( PerfectICache, bool, "Use a perfect ICache", "perfect", true )
  PARAMETER( PrefetchEnabled, bool, "Enable Next-line Prefetcher", "prefetch", true )
  PARAMETER( CleanEvict, bool, "Enable eviction messages", "clean_evict", false)
  PARAMETER( Size, int, "ICache size in bytes", "size", 65536 )
  PARAMETER( Associativity, int, "ICache associativity", "associativity", 4 )
  PARAMETER( MissQueueSize, uint32_t, "Maximum size of the fetch miss queue", "miss_queue_size", 4 )
  PARAMETER( Threads, uint32_t, "Number of threads under control of this uFetch", "threads", 1 )
  PARAMETER( SendAcks, bool, "Send acknowledgements when we received data", "send_acks", false )
  PARAMETER( UseReplyChannel, bool, "Send replies on Reply Channel and only Evicts on Snoop Channel", "use_reply_channel", false )
  PARAMETER( EvictOnSnoop, bool, "Send evicts on Snoop Channel (otherwise use Request Channel)", "evict_on_snoop", true )
);

COMPONENT_INTERFACE(
  DYNAMIC_PORT_ARRAY( PushInput, boost::intrusive_ptr<FetchCommand>, FetchAddressIn )
  DYNAMIC_PORT_ARRAY( PushInput, eSquashCause, SquashIn )
  DYNAMIC_PORT_ARRAY( PushInput, CPUState, ChangeCPUState )
  PORT( PushInput, MemoryTransport, FetchMissIn )
  DYNAMIC_PORT_ARRAY( PullOutput, int, AvailableFAQOut )
  DYNAMIC_PORT_ARRAY( PullInput, int, AvailableFIQ )
  DYNAMIC_PORT_ARRAY( PullOutput, int, ICount)
  DYNAMIC_PORT_ARRAY( PullOutput, bool, Stalled)
  DYNAMIC_PORT_ARRAY( PushOutput, pFetchBundle, FetchBundleOut )
  PORT( PushOutput, MemoryTransport, FetchMissOut )
  PORT( PushOutput, MemoryTransport, FetchSnoopOut )
  PORT( PushOutput, MemoryTransport, FetchReplyOut )

  PORT( PushOutput, bool, InstructionFetchSeen ) // Notify PowerTracker when an instruction is fetched.
  PORT( PushOutput, bool, ClockTickSeen )        // Notify PowerTracker when the clock in this core ticks. This goes here just because uFetch is driven first and it's convenient.

  DRIVE( uFetchDrive )
);

#include FLEXUS_END_COMPONENT_DECLARATION()
#define FLEXUS_END_COMPONENT uFetch

