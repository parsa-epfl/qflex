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
#ifndef FLEXUS_FASTREGIONSCOUTCACHE_CACHESTATS_HPP_INCLUDED
#define FLEXUS_FASTREGIONSCOUTCACHE_CACHESTATS_HPP_INCLUDED

#include <core/stats.hpp>

namespace nFastCache {

namespace Stat = Flexus::Stat;

struct CacheStats {
  Stat::StatCounter Hit_Read_Migratory_stat;
  Stat::StatCounter Hit_Read_Modified_stat;
  Stat::StatCounter Hit_Read_Owned_stat;
  Stat::StatCounter Hit_Read_Exclusive_stat;
  Stat::StatCounter Hit_Read_Shared_stat;
  Stat::StatCounter Miss_Read_Invalid_stat;

  Stat::StatCounter Hit_Fetch_Migratory_stat;
  Stat::StatCounter Hit_Fetch_Modified_stat;
  Stat::StatCounter Hit_Fetch_Owned_stat;
  Stat::StatCounter Hit_Fetch_Exclusive_stat;
  Stat::StatCounter Hit_Fetch_Shared_stat;
  Stat::StatCounter Miss_Fetch_Invalid_stat;

  Stat::StatCounter Hit_Write_Migratory_stat;
  Stat::StatCounter Hit_Write_Modified_stat;
  Stat::StatCounter Miss_Write_Owned_stat;
  Stat::StatCounter Hit_Write_Exclusive_stat;
  Stat::StatCounter Miss_Write_Shared_stat;
  Stat::StatCounter Miss_Write_Invalid_stat;

  Stat::StatCounter Hit_NAW_Migratory_stat;
  Stat::StatCounter Hit_NAW_Modified_stat;
  Stat::StatCounter Miss_NAW_Owned_stat;
  Stat::StatCounter Hit_NAW_Exclusive_stat;
  Stat::StatCounter Miss_NAW_Shared_stat;
  Stat::StatCounter Miss_NAW_Invalid_stat;

  Stat::StatCounter Hit_Upgrade_Migratory_stat;
  Stat::StatCounter Hit_Upgrade_Modified_stat;
  Stat::StatCounter Miss_Upgrade_Owned_stat;
  Stat::StatCounter Hit_Upgrade_Exclusive_stat;
  Stat::StatCounter Miss_Upgrade_Shared_stat;
  Stat::StatCounter Miss_Upgrade_Invalid_stat;

  Stat::StatCounter Hit_Evict_Migratory_stat;
  Stat::StatCounter Hit_Evict_Modified_stat;
  Stat::StatCounter Hit_Evict_Owned_stat;
  Stat::StatCounter Hit_Evict_Exclusive_stat;
  Stat::StatCounter Hit_Evict_Shared_stat;
  Stat::StatCounter Miss_Evict_Invalid_stat;

  Stat::StatCounter Hit_EvictW_Migratory_stat;
  Stat::StatCounter Hit_EvictW_Modified_stat;
  Stat::StatCounter Miss_EvictW_Owned_stat;
  Stat::StatCounter Hit_EvictW_Exclusive_stat;
  Stat::StatCounter Miss_EvictW_Shared_stat;
  Stat::StatCounter Miss_EvictW_Invalid_stat;

  Stat::StatCounter Hit_EvictD_Migratory_stat;
  Stat::StatCounter Hit_EvictD_Modified_stat;
  Stat::StatCounter Miss_EvictD_Owned_stat;
  Stat::StatCounter Hit_EvictD_Exclusive_stat;
  Stat::StatCounter Miss_EvictD_Shared_stat;
  Stat::StatCounter Miss_EvictD_Invalid_stat;

  Stat::StatCounter Hit_ReturnReq_Migratory_stat;
  Stat::StatCounter Hit_ReturnReq_Modified_stat;
  Stat::StatCounter Hit_ReturnReq_Owned_stat;
  Stat::StatCounter Hit_ReturnReq_Exclusive_stat;
  Stat::StatCounter Hit_ReturnReq_Shared_stat;
  Stat::StatCounter Miss_ReturnReq_Invalid_stat;

  Stat::StatCounter Hit_Invalidate_Migratory_stat;
  Stat::StatCounter Hit_Invalidate_Modified_stat;
  Stat::StatCounter Hit_Invalidate_Owned_stat;
  Stat::StatCounter Hit_Invalidate_Exclusive_stat;
  Stat::StatCounter Hit_Invalidate_Shared_stat;
  Stat::StatCounter Miss_Invalidate_Invalid_stat;

  Stat::StatCounter Hit_Downgrade_Migratory_stat;
  Stat::StatCounter Hit_Downgrade_Modified_stat;
  Stat::StatCounter Hit_Downgrade_Owned_stat;
  Stat::StatCounter Hit_Downgrade_Exclusive_stat;
  Stat::StatCounter Hit_Downgrade_Shared_stat;
  Stat::StatCounter Miss_Downgrade_Invalid_stat;

  Stat::StatCounter theTagMatches_Invalid_stat;

  Stat::StatCounter thePrefetchHits_Read_stat;
  Stat::StatCounter thePrefetchHits_Write_stat;
  Stat::StatCounter thePrefetchHits_Evict_stat;
  Stat::StatCounter thePrefetchHits_ButUpgrade_stat;

  int64_t HitReadMigratory;
  int64_t HitReadModified;
  int64_t HitReadOwned;
  int64_t HitReadExclusive;
  int64_t HitReadShared;
  int64_t MissReadInvalid;

  int64_t HitFetchMigratory;
  int64_t HitFetchModified;
  int64_t HitFetchOwned;
  int64_t HitFetchExclusive;
  int64_t HitFetchShared;
  int64_t MissFetchInvalid;

  int64_t HitWriteMigratory;
  int64_t HitWriteModified;
  int64_t MissWriteOwned;
  int64_t HitWriteExclusive;
  int64_t MissWriteShared;
  int64_t MissWriteInvalid;

  int64_t HitNAWMigratory;
  int64_t HitNAWModified;
  int64_t MissNAWOwned;
  int64_t HitNAWExclusive;
  int64_t MissNAWShared;
  int64_t MissNAWInvalid;

  int64_t HitUpgradeMigratory;
  int64_t HitUpgradeModified;
  int64_t MissUpgradeOwned;
  int64_t HitUpgradeExclusive;
  int64_t MissUpgradeShared;
  int64_t MissUpgradeInvalid;

  int64_t HitEvictMigratory;
  int64_t HitEvictModified;
  int64_t HitEvictOwned;
  int64_t HitEvictExclusive;
  int64_t HitEvictShared;
  int64_t MissEvictInvalid;

  int64_t HitEvictWMigratory;
  int64_t HitEvictWModified;
  int64_t MissEvictWOwned;
  int64_t HitEvictWExclusive;
  int64_t MissEvictWShared;
  int64_t MissEvictWInvalid;

  int64_t HitEvictDMigratory;
  int64_t HitEvictDModified;
  int64_t MissEvictDOwned;
  int64_t HitEvictDExclusive;
  int64_t MissEvictDShared;
  int64_t MissEvictDInvalid;

  int64_t HitReturnReqMigratory;
  int64_t HitReturnReqModified;
  int64_t HitReturnReqOwned;
  int64_t HitReturnReqExclusive;
  int64_t HitReturnReqShared;
  int64_t MissReturnReqInvalid;

  int64_t HitInvalidateMigratory;
  int64_t HitInvalidateModified;
  int64_t HitInvalidateOwned;
  int64_t HitInvalidateExclusive;
  int64_t HitInvalidateShared;
  int64_t MissInvalidateInvalid;

  int64_t HitDowngradeMigratory;
  int64_t HitDowngradeModified;
  int64_t HitDowngradeOwned;
  int64_t HitDowngradeExclusive;
  int64_t HitDowngradeShared;
  int64_t MissDowngradeInvalid;

  int64_t theTagMatches_Invalid;

  int64_t thePrefetchHits_Read;
  int64_t thePrefetchHits_Write;
  int64_t thePrefetchHits_Evict;
  int64_t thePrefetchHits_ButUpgrade;

  CacheStats(std::string const & theName)
    : Hit_Read_Migratory_stat(theName + "-Hits:Read:Migratory")
    , Hit_Read_Modified_stat(theName + "-Hits:Read:Modified")
    , Hit_Read_Owned_stat(theName + "-Hits:Read:Owned")
    , Hit_Read_Exclusive_stat(theName + "-Hits:Read:Exclusive")
    , Hit_Read_Shared_stat(theName + "-Hits:Read:Shared")
    , Miss_Read_Invalid_stat(theName + "-Misses:Read:Invalid")

    , Hit_Fetch_Migratory_stat(theName + "-Hits:Fetch:Migratory")
    , Hit_Fetch_Modified_stat(theName + "-Hits:Fetch:Modified")
    , Hit_Fetch_Owned_stat(theName + "-Hits:Fetch:Owned")
    , Hit_Fetch_Exclusive_stat(theName + "-Hits:Fetch:Exclusive")
    , Hit_Fetch_Shared_stat(theName + "-Hits:Fetch:Shared")
    , Miss_Fetch_Invalid_stat(theName + "-Misses:Fetch:Invalid")

    , Hit_Write_Migratory_stat(theName + "-Hits:Write:Migratory")
    , Hit_Write_Modified_stat(theName + "-Hits:Write:Modified")
    , Miss_Write_Owned_stat(theName + "-Misses:Write:Owned")
    , Hit_Write_Exclusive_stat(theName + "-Hits:Write:Exclusive")
    , Miss_Write_Shared_stat(theName + "-Misses:Write:Shared")
    , Miss_Write_Invalid_stat(theName + "-Misses:Write:Invalid")

    , Hit_NAW_Migratory_stat(theName + "-Hits:NAW:Migratory")
    , Hit_NAW_Modified_stat(theName + "-Hits:NAW:Modified")
    , Miss_NAW_Owned_stat(theName + "-Misses:NAW:Owned")
    , Hit_NAW_Exclusive_stat(theName + "-Hits:NAW:Exclusive")
    , Miss_NAW_Shared_stat(theName + "-Misses:NAW:Shared")
    , Miss_NAW_Invalid_stat(theName + "-Misses:NAW:Invalid")

    , Hit_Upgrade_Migratory_stat(theName + "-Hits:Upgrade:Migratory")
    , Hit_Upgrade_Modified_stat(theName + "-Hits:Upgrade:Modified")
    , Miss_Upgrade_Owned_stat(theName + "-Misses:Upgrade:Owned")
    , Hit_Upgrade_Exclusive_stat(theName + "-Hits:Upgrade:Exclusive")
    , Miss_Upgrade_Shared_stat(theName + "-Misses:Upgrade:Shared")
    , Miss_Upgrade_Invalid_stat(theName + "-Misses:Upgrade:Invalid")

    , Hit_Evict_Migratory_stat(theName + "-Hits:Evict:Migratory")
    , Hit_Evict_Modified_stat(theName + "-Hits:Evict:Modified")
    , Hit_Evict_Owned_stat(theName + "-Hits:Evict:Owned")
    , Hit_Evict_Exclusive_stat(theName + "-Hits:Evict:Exclusive")
    , Hit_Evict_Shared_stat(theName + "-Hits:Evict:Shared")
    , Miss_Evict_Invalid_stat(theName + "-Misses:Evict:Invalid")

    , Hit_EvictW_Migratory_stat(theName + "-Hits:EvictWritable:Migratory")
    , Hit_EvictW_Modified_stat(theName + "-Hits:EvictWritable:Modified")
    , Miss_EvictW_Owned_stat(theName + "-Misses:EvictWritable:Owned")
    , Hit_EvictW_Exclusive_stat(theName + "-Hits:EvictWritable:Exclusive")
    , Miss_EvictW_Shared_stat(theName + "-Misses:EvictWritable:Shared")
    , Miss_EvictW_Invalid_stat(theName + "-Misses:EvictWritable:Invalid")

    , Hit_EvictD_Migratory_stat(theName + "-Hits:EvictD:Migratory")
    , Hit_EvictD_Modified_stat(theName + "-Hits:EvictD:Modified")
    , Miss_EvictD_Owned_stat(theName + "-Misses:EvictD:Owned")
    , Hit_EvictD_Exclusive_stat(theName + "-Hits:EvictD:Exclusive")
    , Miss_EvictD_Shared_stat(theName + "-Misses:EvictD:Shared")
    , Miss_EvictD_Invalid_stat(theName + "-Misses:EvictD:Invalid")

    , Hit_ReturnReq_Migratory_stat(theName + "-Snoops:ReturnReq:Migratory")
    , Hit_ReturnReq_Modified_stat(theName + "-Snoops:ReturnReq:Modified")
    , Hit_ReturnReq_Owned_stat(theName + "-Snoops:ReturnReq:Owned")
    , Hit_ReturnReq_Exclusive_stat(theName + "-Snoops:ReturnReq:Exclusive")
    , Hit_ReturnReq_Shared_stat(theName + "-Snoops:ReturnReq:Shared")
    , Miss_ReturnReq_Invalid_stat(theName + "-Snoops:ReturnReq:Invalid")

    , Hit_Invalidate_Migratory_stat(theName + "-Snoops:Invalidate:Migratory")
    , Hit_Invalidate_Modified_stat(theName + "-Snoops:Invalidate:Modified")
    , Hit_Invalidate_Owned_stat(theName + "-Snoops:Invalidate:Owned")
    , Hit_Invalidate_Exclusive_stat(theName + "-Snoops:Invalidate:Exclusive")
    , Hit_Invalidate_Shared_stat(theName + "-Snoops:Invalidate:Shared")
    , Miss_Invalidate_Invalid_stat(theName + "-Snoops:Invalidate:Invalid")

    , Hit_Downgrade_Migratory_stat(theName + "-Snoops:Downgrade:Migratory")
    , Hit_Downgrade_Modified_stat(theName + "-Snoops:Downgrade:Modified")
    , Hit_Downgrade_Owned_stat(theName + "-Snoops:Downgrade:Owned")
    , Hit_Downgrade_Exclusive_stat(theName + "-Snoops:Downgrade:Exclusive")
    , Hit_Downgrade_Shared_stat(theName + "-Snoops:Downgrade:Shared")
    , Miss_Downgrade_Invalid_stat(theName + "-Snoops:Downgrade:Invalid")

    , theTagMatches_Invalid_stat(theName + "-TagMatchesInvalid")

    , thePrefetchHits_Read_stat(theName + "-PrefetchHits:Read")
    , thePrefetchHits_Write_stat(theName + "-PrefetchHits:Write")
    , thePrefetchHits_Evict_stat(theName + "-PrefetchHits:Evict")
    , thePrefetchHits_ButUpgrade_stat(theName + "-PrefetchHits:ButUpgrade")

  {
    HitReadMigratory = 0;
    HitReadModified = 0;
    HitReadOwned = 0;
    HitReadExclusive = 0;
    HitReadShared = 0;
    MissReadInvalid = 0;

    HitFetchMigratory = 0;
    HitFetchModified = 0;
    HitFetchOwned = 0;
    HitFetchExclusive = 0;
    HitFetchShared = 0;
    MissFetchInvalid = 0;

    HitWriteMigratory = 0;
    HitWriteModified = 0;
    MissWriteOwned = 0;
    HitWriteExclusive = 0;
    MissWriteShared = 0;
    MissWriteInvalid = 0;

    HitNAWMigratory = 0;
    HitNAWModified = 0;
    MissNAWOwned = 0;
    HitNAWExclusive = 0;
    MissNAWShared = 0;
    MissNAWInvalid = 0;

    HitUpgradeMigratory = 0;
    HitUpgradeModified = 0;
    MissUpgradeOwned = 0;
    HitUpgradeExclusive = 0;
    MissUpgradeShared = 0;
    MissUpgradeInvalid = 0;

    HitEvictMigratory = 0;
    HitEvictModified = 0;
    HitEvictOwned = 0;
    HitEvictExclusive = 0;
    HitEvictShared = 0;
    MissEvictInvalid = 0;

    HitEvictDMigratory = 0;
    HitEvictDModified = 0;
    MissEvictDOwned = 0;
    HitEvictDExclusive = 0;
    MissEvictDShared = 0;
    MissEvictDInvalid = 0;

    HitEvictWMigratory = 0;
    HitEvictWModified = 0;
    MissEvictWOwned = 0;
    HitEvictWExclusive = 0;
    MissEvictWShared = 0;
    MissEvictWInvalid = 0;

    HitReturnReqMigratory = 0;
    HitReturnReqModified = 0;
    HitReturnReqOwned = 0;
    HitReturnReqExclusive = 0;
    HitReturnReqShared = 0;
    MissReturnReqInvalid = 0;

    HitInvalidateMigratory = 0;
    HitInvalidateModified = 0;
    HitInvalidateOwned = 0;
    HitInvalidateExclusive = 0;
    HitInvalidateShared = 0;
    MissInvalidateInvalid = 0;

    HitDowngradeMigratory = 0;
    HitDowngradeModified = 0;
    HitDowngradeOwned = 0;
    HitDowngradeExclusive = 0;
    HitDowngradeShared = 0;
    MissDowngradeInvalid = 0;

    theTagMatches_Invalid  = 0;
    thePrefetchHits_Read   = 0;
    thePrefetchHits_Write  = 0;
    thePrefetchHits_Evict  = 0;
    thePrefetchHits_ButUpgrade = 0;
  }

  void update() {
    Hit_Read_Migratory_stat += HitReadMigratory;
    Hit_Read_Modified_stat += HitReadModified;
    Hit_Read_Owned_stat += HitReadOwned;
    Hit_Read_Exclusive_stat += HitReadExclusive;
    Hit_Read_Shared_stat += HitReadShared;
    Miss_Read_Invalid_stat += MissReadInvalid;

    Hit_Fetch_Migratory_stat += HitFetchMigratory;
    Hit_Fetch_Modified_stat += HitFetchModified;
    Hit_Fetch_Owned_stat += HitFetchOwned;
    Hit_Fetch_Exclusive_stat += HitFetchExclusive;
    Hit_Fetch_Shared_stat += HitFetchShared;
    Miss_Fetch_Invalid_stat += MissFetchInvalid;

    Hit_Write_Migratory_stat += HitWriteMigratory;
    Hit_Write_Modified_stat += HitWriteModified;
    Miss_Write_Owned_stat += MissWriteOwned;
    Hit_Write_Exclusive_stat += HitWriteExclusive;
    Miss_Write_Shared_stat += MissWriteShared;
    Miss_Write_Invalid_stat += MissWriteInvalid;

    Hit_NAW_Migratory_stat += HitNAWMigratory;
    Hit_NAW_Modified_stat += HitNAWModified;
    Miss_NAW_Owned_stat += MissNAWOwned;
    Hit_NAW_Exclusive_stat += HitNAWExclusive;
    Miss_NAW_Shared_stat += MissNAWShared;
    Miss_NAW_Invalid_stat += MissNAWInvalid;

    Hit_Upgrade_Migratory_stat += HitUpgradeMigratory;
    Hit_Upgrade_Modified_stat += HitUpgradeModified;
    Miss_Upgrade_Owned_stat += MissUpgradeOwned;
    Hit_Upgrade_Exclusive_stat += HitUpgradeExclusive;
    Miss_Upgrade_Shared_stat += MissUpgradeShared;
    Miss_Upgrade_Invalid_stat += MissUpgradeInvalid;

    Hit_Evict_Migratory_stat += HitEvictMigratory;
    Hit_Evict_Modified_stat += HitEvictModified;
    Hit_Evict_Owned_stat += HitEvictOwned;
    Hit_Evict_Exclusive_stat += HitEvictExclusive;
    Hit_Evict_Shared_stat += HitEvictShared;
    Miss_Evict_Invalid_stat += MissEvictInvalid;

    Hit_EvictW_Migratory_stat += HitEvictWMigratory;
    Hit_EvictW_Modified_stat += HitEvictWModified;
    Miss_EvictW_Owned_stat += MissEvictWOwned;
    Hit_EvictW_Exclusive_stat += HitEvictWExclusive;
    Miss_EvictW_Shared_stat += MissEvictWShared;
    Miss_EvictW_Invalid_stat += MissEvictWInvalid;

    Hit_EvictD_Migratory_stat += HitEvictDMigratory;
    Hit_EvictD_Modified_stat += HitEvictDModified;
    Miss_EvictD_Owned_stat += MissEvictDOwned;
    Hit_EvictD_Exclusive_stat += HitEvictDExclusive;
    Miss_EvictD_Shared_stat += MissEvictDShared;
    Miss_EvictD_Invalid_stat += MissEvictDInvalid;

    Hit_ReturnReq_Migratory_stat += HitReturnReqMigratory;
    Hit_ReturnReq_Modified_stat += HitReturnReqModified;
    Hit_ReturnReq_Owned_stat += HitReturnReqOwned;
    Hit_ReturnReq_Exclusive_stat += HitReturnReqExclusive;
    Hit_ReturnReq_Shared_stat += HitReturnReqShared;
    Miss_ReturnReq_Invalid_stat += MissReturnReqInvalid;

    Hit_Invalidate_Migratory_stat += HitInvalidateMigratory;
    Hit_Invalidate_Modified_stat += HitInvalidateModified;
    Hit_Invalidate_Owned_stat += HitInvalidateOwned;
    Hit_Invalidate_Exclusive_stat += HitInvalidateExclusive;
    Hit_Invalidate_Shared_stat += HitInvalidateShared;
    Miss_Invalidate_Invalid_stat += MissInvalidateInvalid;

    Hit_Downgrade_Migratory_stat += HitDowngradeMigratory;
    Hit_Downgrade_Modified_stat += HitDowngradeModified;
    Hit_Downgrade_Owned_stat += HitDowngradeOwned;
    Hit_Downgrade_Exclusive_stat += HitDowngradeExclusive;
    Hit_Downgrade_Shared_stat += HitDowngradeShared;
    Miss_Downgrade_Invalid_stat += MissDowngradeInvalid;

    theTagMatches_Invalid_stat  += theTagMatches_Invalid  ;
    thePrefetchHits_Read_stat   += thePrefetchHits_Read   ;
    thePrefetchHits_Write_stat  += thePrefetchHits_Write  ;
    thePrefetchHits_Evict_stat  += thePrefetchHits_Evict  ;
    thePrefetchHits_ButUpgrade_stat += thePrefetchHits_ButUpgrade;

    HitReadMigratory = 0;
    HitReadModified = 0;
    HitReadOwned = 0;
    HitReadExclusive = 0;
    HitReadShared = 0;
    MissReadInvalid = 0;

    HitFetchMigratory = 0;
    HitFetchModified = 0;
    HitFetchOwned = 0;
    HitFetchExclusive = 0;
    HitFetchShared = 0;
    MissFetchInvalid = 0;

    HitNAWMigratory = 0;
    HitNAWModified = 0;
    MissNAWOwned = 0;
    HitNAWExclusive = 0;
    MissNAWShared = 0;
    MissNAWInvalid = 0;

    HitWriteMigratory = 0;
    HitWriteModified = 0;
    MissWriteOwned = 0;
    HitWriteExclusive = 0;
    MissWriteShared = 0;
    MissWriteInvalid = 0;

    HitUpgradeMigratory = 0;
    HitUpgradeModified = 0;
    MissUpgradeOwned = 0;
    HitUpgradeExclusive = 0;
    MissUpgradeShared = 0;
    MissUpgradeInvalid = 0;

    HitEvictMigratory = 0;
    HitEvictModified = 0;
    HitEvictOwned = 0;
    HitEvictExclusive = 0;
    HitEvictShared = 0;
    MissEvictInvalid = 0;

    HitEvictDMigratory = 0;
    HitEvictDModified = 0;
    MissEvictDOwned = 0;
    HitEvictDExclusive = 0;
    MissEvictDShared = 0;
    MissEvictDInvalid = 0;

    HitEvictWMigratory = 0;
    HitEvictWModified = 0;
    MissEvictWOwned = 0;
    HitEvictWExclusive = 0;
    MissEvictWShared = 0;
    MissEvictWInvalid = 0;

    HitReturnReqMigratory = 0;
    HitReturnReqModified = 0;
    HitReturnReqOwned = 0;
    HitReturnReqExclusive = 0;
    HitReturnReqShared = 0;
    MissReturnReqInvalid = 0;

    HitInvalidateMigratory = 0;
    HitInvalidateModified = 0;
    HitInvalidateOwned = 0;
    HitInvalidateExclusive = 0;
    HitInvalidateShared = 0;
    MissInvalidateInvalid = 0;

    HitDowngradeMigratory = 0;
    HitDowngradeModified = 0;
    HitDowngradeOwned = 0;
    HitDowngradeExclusive = 0;
    HitDowngradeShared = 0;
    MissDowngradeInvalid = 0;

    theTagMatches_Invalid  = 0;
    thePrefetchHits_Read   = 0;
    thePrefetchHits_Write  = 0;
    thePrefetchHits_Evict  = 0;
    thePrefetchHits_ButUpgrade = 0;
  }
};

}  // namespace nFastCache

#endif /* FLEXUS_FASTREGIONSCOUTCACHE_CACHESTATS_HPP_INCLUDED */

