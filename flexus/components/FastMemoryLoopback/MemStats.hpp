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
#ifndef FLEXUS_FASTMEMORYLOOPBACK_MEMSTATS_HPP_INCLUDED
#define FLEXUS_FASTMEMORYLOOPBACK_MEMSTATS_HPP_INCLUDED

#include <core/stats.hpp>

#define debug(AAA) DBG_(Tmp, ( <<"TRACING  "<< AAA << std::dec) );

namespace nFastMemoryLoopback {

namespace Stat = Flexus::Stat;

struct MemStats {
  Stat::StatCounter theReadRequests_stat;
  Stat::StatCounter theWriteRequests_stat;
  Stat::StatCounter theEvictDirtys_stat;
  Stat::StatCounter theEvictCleans_stat;
  Stat::StatCounter theEvictWritables_stat;
  Stat::StatCounter theNonAllocatingStoreReq_stat;
  Stat::StatCounter theUpgradeRequest_stat;
  Stat::StatCounter theWriteDMA_stat;
  Stat::StatCounter theReadDMA_stat;
  
 MemStats(std::string const & theName)
    : theReadRequests_stat(theName + "-Reads")
    , theWriteRequests_stat(theName + "-Writes")
    , theEvictDirtys_stat(theName + "-Evict:Dirty")
    , theEvictCleans_stat(theName + "-Evict:Clean")
    , theEvictWritables_stat(theName + "-Evict:Writable")
    , theNonAllocatingStoreReq_stat(theName + "-NonAllocStore")
    , theUpgradeRequest_stat(theName + "-Upgrade")
    , theWriteDMA_stat(theName + "-DMA:Write")
    , theReadDMA_stat(theName + "-DMA:Read")
  {
  }
  void update() {
  }
};

}  // namespace nFastMemoryLoopback
#endif /* FLEXUS_FASTMEMORYLOOPBACK_MEMSTATS_HPP_INCLUDED */
