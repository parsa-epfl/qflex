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

#include <components/CommonQEMU/Slices/MemoryMessage.hpp>
#include <components/CommonQEMU/Slices/RegionScoutMessage.hpp>

#define FLEXUS_BEGIN_COMPONENT FastCMPCache
#include FLEXUS_BEGIN_COMPONENT_DECLARATION()

COMPONENT_PARAMETERS(
  PARAMETER( CMPWidth, int, "Number of cores per CMP chip (0 = sys width)", "CMPWidth", 16 )

  PARAMETER( Size, int, "Cache size in bytes", "size", 65536 )
  PARAMETER( Associativity, int, "Set associativity", "assoc", 2 )
  PARAMETER( BlockSize, int, "Block size", "bsize", 64 )
  PARAMETER( CleanEvictions, bool, "Issue clean evictions", "clean_evict", false )
  PARAMETER( CacheLevel, Flexus::SharedTypes::tFillLevel, "CacheLevel", "level", Flexus::SharedTypes::eUnknown )
  PARAMETER( TraceTracker, bool, "Turn trace tracker on/off", "trace_tracker_on", false )

  PARAMETER( ReplPolicy, std::string, "Cache replacement policy (LRU | SetLRU | RegionLRU)", "repl", "LRU" )

  PARAMETER( RegionSize, int, "Region size in bytes", "rsize", 1024 )
  PARAMETER( RTAssoc, int, "RegionTracker Associativity", "rt_assoc", 16 )
  PARAMETER( RTSize, int, "RegionTracker size (number of regions tracked)", "rt_size", 8192 )
  PARAMETER( ERBSize, int, "Evicted Region Buffer size", "erb_size", 8 )

  PARAMETER( StdArray, bool, "Use Standard Tag Array (true) or RegionTracker (false) default is true", "std_array", true )

  PARAMETER( DirectoryType, std::string, "Directory Type", "directory_type", "Infinite")
  PARAMETER( Protocol, std::string, "Protocol Type", "protocol", "SingleCMP")
  PARAMETER( AlwaysMulticast, bool, "Perform multicast instead of serial snooping", "always_multicast", false)
  PARAMETER( SeparateID, bool, "Track Instruction and Data caches separately", "seperate_id", false)

  PARAMETER( CoherenceUnit, uint64_t, "Coherence Unit", "coherence_unit", 64)

);

COMPONENT_INTERFACE(
  DYNAMIC_PORT_ARRAY( PushInput, MemoryMessage, RequestIn )
  DYNAMIC_PORT_ARRAY( PushInput, MemoryMessage, FetchRequestIn )
  DYNAMIC_PORT_ARRAY( PushOutput, MemoryMessage, SnoopOutD )
  DYNAMIC_PORT_ARRAY( PushOutput, MemoryMessage, SnoopOutI )

  DYNAMIC_PORT_ARRAY( PushInput, RegionScoutMessage, RegionNotify )
  DYNAMIC_PORT_ARRAY( PushOutput, RegionScoutMessage, RegionNotifyOut )
  DYNAMIC_PORT_ARRAY( PushOutput, RegionScoutMessage, RegionProbe )

  // From Directory OUT to Memory Controller
  PORT( PushInput, MemoryMessage, SnoopIn ) // From MemController IN to topology
  PORT( PushOutput, MemoryMessage, RequestOut ) // From topology OUT to memcontroller

  DRIVE( UpdateStatsDrive )
);

#include FLEXUS_END_COMPONENT_DECLARATION()
#define FLEXUS_END_COMPONENT FastCMPCache
