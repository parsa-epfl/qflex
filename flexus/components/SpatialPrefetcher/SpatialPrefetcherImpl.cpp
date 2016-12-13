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
#include <components/SpatialPrefetcher/SpatialPrefetcher.hpp>

#include <memory>
#include <zlib.h>

#include <components/CommonQEMU/Slices/TransactionTracker.hpp>

#include <components/CommonQEMU/TraceTracker.hpp>

#include <core/stats.hpp>
#include <core/performance/profile.hpp>

#define FLEXUS_BEGIN_COMPONENT SpatialPrefetcher
#include FLEXUS_BEGIN_COMPONENT_IMPLEMENTATION()

namespace nSpatialPrefetcher {

using namespace Flexus::Core;
using namespace Flexus::SharedTypes;
namespace Stat = Flexus::Stat;

using boost::intrusive_ptr;

typedef uint32_t MemoryAddress;

class FLEXUS_COMPONENT(SpatialPrefetcher) {
  FLEXUS_COMPONENT_IMPL( SpatialPrefetcher );

public:
  FLEXUS_COMPONENT_CONSTRUCTOR(SpatialPrefetcher)
    : base( FLEXUS_PASS_CONSTRUCTOR_ARGS )
  {}

public:
  void initialize() {
    if (cfg.UsageEnable || cfg.RepetEnable || cfg.PrefetchEnable) {
      theTraceTracker.initSGP( flexusIndex(), cfg.CacheLevel,
                               cfg.UsageEnable, cfg.RepetEnable, cfg.BufFetchEnable,
                               cfg.TimeRepetEnable, cfg.PrefetchEnable, cfg.ActiveEnable,
                               cfg.OrderEnable, cfg.StreamEnable,
                               cfg.BlockSize, cfg.SgpBlocks, cfg.RepetType, cfg.RepetFills,
                               cfg.SparseOpt, cfg.PhtSize, cfg.PhtAssoc, cfg.PcBits,
                               cfg.CptType, cfg.CptSize, cfg.CptAssoc, cfg.CptSparse,
                               cfg.FetchDist, cfg.SgpBlocks /*window*/, cfg.StreamDense, true /*sendStreams*/,
                               cfg.BufSize, cfg.StreamDescs, cfg.DelayedCommits, cfg.CptFilter);
    }
    if (!cfg.PrefetchEnable) {
      theTraceTracker.initOffChipTracking(flexusIndex());
    }
  }

  bool isQuiesced() const {
    return true;
  }

  void saveState(std::string const & aDirName) {
    if (cfg.RepetEnable) {
      theTraceTracker.saveSGP(flexusIndex(), cfg.CacheLevel, aDirName);
    }
  }

  void loadState(std::string const & aDirName) {
    if (cfg.RepetEnable) {
      theTraceTracker.loadSGP(flexusIndex(), cfg.CacheLevel, aDirName);
    } else {
      DBG_(Dev, ( << "Warning: SGP state not loaded because Repet not enabled" ) );
    }
  }

public:

public:
  //The PrefetchDrive drive checks if it should issue a prefetch this cycle.
  void drive(interface::PrefetchDrive const &) {
    //Implementation is in the tryPrefetch() member below
    if (cfg.PrefetchEnable == true) {
      while (theTraceTracker.prefetchReady(flexusIndex(), cfg.CacheLevel)) {
        if (FLEXUS_CHANNEL(PrefetchOut_1).available() && FLEXUS_CHANNEL(PrefetchOut_2).available()) {
          PrefetchTransport transport;
          transport.set( PrefetchCommandTag, theTraceTracker.getPrefetchCommand(flexusIndex(), cfg.CacheLevel));
          DBG_(Trace, Comp(*this)  ( << "issuing prefetch: " << *transport[PrefetchCommandTag]) );
          FLEXUS_CHANNEL(PrefetchOut_1) << transport;
          FLEXUS_CHANNEL(PrefetchOut_2) << transport;
        } else {
          // port not available - don't loop forever
          break;
        }
      }
    }
  }

};

} // end namespace nSpatialPrefetcher

FLEXUS_COMPONENT_INSTANTIATOR( SpatialPrefetcher,  nSpatialPrefetcher);

#include FLEXUS_END_COMPONENT_IMPLEMENTATION()
#define FLEXUS_END_COMPONENT SpatialPrefetcher

#define DBG_Reset
#include DBG_Control()
