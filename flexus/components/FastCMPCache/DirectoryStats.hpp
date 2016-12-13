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
#ifndef FLEXUS_FASTCMPCACHE_DIRECTORYSTATS_HPP_INCLUDED
#define FLEXUS_FASTCMPCACHE_DIRECTORYSTATS_HPP_INCLUDED

#include <core/stats.hpp>

namespace nFastCMPCache {

namespace Stat = Flexus::Stat;

struct DirectoryStats {
  Stat::StatCounter theReadsOnChip;
  Stat::StatCounter theFetchesOnChip;
  Stat::StatCounter theWritesOnChip;
  Stat::StatCounter theUpgradesOnChip;
  Stat::StatCounter theReadsOffChip;
  Stat::StatCounter theFetchesOffChip;
  Stat::StatCounter theWritesOffChip;

  Stat::StatInstanceCounter<int64_t> theReadsOffChip_PSharers;
  Stat::StatInstanceCounter<int64_t> theReadsOnChip_PSharers;
  Stat::StatInstanceCounter<int64_t> theReadsOnChip_FSharers;
  Stat::StatInstanceCounter<int64_t> theReadsOnChip_ASharers;
  Stat::StatInstanceCounter<int64_t> theWritesOffChip_PSharers;
  Stat::StatInstanceCounter<int64_t> theWritesOnChip_PSharers;
  Stat::StatInstanceCounter<int64_t> theWritesOnChip_FSharers;
  Stat::StatInstanceCounter<int64_t> theWritesOnChip_ASharers;
  Stat::StatInstanceCounter<int64_t> theFetchesOffChip_PSharers;
  Stat::StatInstanceCounter<int64_t> theFetchesOnChip_PSharers;
  Stat::StatInstanceCounter<int64_t> theFetchesOnChip_FSharers;
  Stat::StatInstanceCounter<int64_t> theFetchesOnChip_ASharers;
  Stat::StatInstanceCounter<int64_t> theUpgradesOnChip_PSharers;
  Stat::StatInstanceCounter<int64_t> theUpgradesOnChip_FSharers;
  Stat::StatInstanceCounter<int64_t> theUpgradesOnChip_ASharers;

  Stat::StatCounter theReadsOnChipLatency;
  Stat::StatCounter theWritesOnChipLatency;
  Stat::StatCounter theFetchesOnChipLatency;
  Stat::StatCounter theUpgradesOnChipLatency;
  Stat::StatCounter theOtherOnChipLatency;
  Stat::StatCounter theReadsOffChipLatency;
  Stat::StatCounter theWritesOffChipLatency;
  Stat::StatCounter theFetchesOffChipLatency;
  Stat::StatCounter theOtherOffChipLatency;

  DirectoryStats(std::string const & theName)
    : theReadsOnChip(theName + "-Reads:OnChip")
    , theFetchesOnChip(theName + "-Fetches:OnChip")
    , theWritesOnChip(theName + "-Writes:OnChip")
    , theUpgradesOnChip(theName + "-Upgrades:OnChip")
    , theReadsOffChip(theName + "-Reads:OffChip")
    , theFetchesOffChip(theName + "-Fetches:OffChip")
    , theWritesOffChip(theName + "-Writes:OffChip")

    , theReadsOffChip_PSharers(theName + "-Reads:Off:PotentialSharers")
    , theReadsOnChip_PSharers(theName + "-Reads:On:PotentialSharers")
    , theReadsOnChip_FSharers(theName + "-Reads:On:WrongSharers")
    , theReadsOnChip_ASharers(theName + "-Reads:On:AdditionalSharers")
    , theWritesOffChip_PSharers(theName + "-Writes:Off:PotentialSharers")
    , theWritesOnChip_PSharers(theName + "-Writes:On:PotentialSharers")
    , theWritesOnChip_FSharers(theName + "-Writes:On:WrongSharers")
    , theWritesOnChip_ASharers(theName + "-Writes:On:AdditionalSharers")
    , theFetchesOffChip_PSharers(theName + "-Fetches:Off:PotentialSharers")
    , theFetchesOnChip_PSharers(theName + "-Fetches:On:PotentialSharers")
    , theFetchesOnChip_FSharers(theName + "-Fetches:On:WrongSharers")
    , theFetchesOnChip_ASharers(theName + "-Fetches:On:AdditionalSharers")
    , theUpgradesOnChip_PSharers(theName + "-Upgrades:On:PotentialSharers")
    , theUpgradesOnChip_FSharers(theName + "-Upgrades:On:WrongSharers")
    , theUpgradesOnChip_ASharers(theName + "-Upgrades:On:AdditionalSharers")

    , theReadsOnChipLatency(theName + "Reads:On:Latency")
    , theWritesOnChipLatency(theName + "Writes:On:Latency")
    , theFetchesOnChipLatency(theName + "Fetches:On:Latency")
    , theUpgradesOnChipLatency(theName + "Upgrades:On:Latency")
    , theOtherOnChipLatency(theName + "Other:On:Latency")
    , theReadsOffChipLatency(theName + "Reads:Off:Latency")
    , theWritesOffChipLatency(theName + "Writes:Off:Latency")
    , theFetchesOffChipLatency(theName + "Fetches:Off:Latency")
    , theOtherOffChipLatency(theName + "Other:Off:Latency") {
  }

  void update() {
  }
};

}  // namespace nFastCMPCache

#endif /* FLEXUS_FASTCMPCACHE_DIRECTORYSTATS_HPP_INCLUDED */

