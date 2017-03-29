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
//FIXME Removed WhiteBox (commented out) of all places used in the simulator


#define FLEXUS_WIRING_FILE
#include <core/simulator_layout.hpp>


//This section contains the name of the simulator
#include <core/simulator_name.hpp>
namespace Flexus {
//Simulator Name
std::string theSimulatorName = "Keen Kraken v1.0";
}


#include FLEXUS_BEGIN_DECLARATION_SECTION()

#include <components/DecoupledFeederQEMU/DecoupledFeeder.hpp>
#include <components/FastCache/FastCache.hpp>
#include <components/FastCMPCache/FastCMPCache.hpp>
#include <components/FastMemoryLoopback/FastMemoryLoopback.hpp>
#include <components/MagicBreakQEMU/MagicBreak.hpp>
#include <components/BPWarm/BPWarm.hpp>
//#include <components/WhiteBox/WhiteBox.hpp>

#include FLEXUS_END_DECLARATION_SECTION()

#include FLEXUS_BEGIN_COMPONENT_CONFIGURATION_SECTION()

CREATE_CONFIGURATION( DecoupledFeeder, "feeder", theFeederCfg );
CREATE_CONFIGURATION( FastCache, "L1d", theL1DCfg );
CREATE_CONFIGURATION( FastCache, "L1i", theL1ICfg );
CREATE_CONFIGURATION( FastCMPCache, "L2", theL2Cfg );
CREATE_CONFIGURATION( FastMemoryLoopback, "memory", theMemoryCfg );
CREATE_CONFIGURATION( MagicBreak, "magic-break", theMagicBreakCfg );
CREATE_CONFIGURATION( BPWarm, "bpwarm", theBPWarmCfg );
//CREATE_CONFIGURATION( WhiteBox, "white-box", theWhiteBoxCfg );

//You may optionally initialize configuration parameters from within this
//function.  This initialization occur before the command line is processed,
//so they will be overridden from the command line.
//
//Return value indicates whether simulation should abort if any parameters
//are left at their default values;
bool initializeParameters() {
  DBG_( Dev, ( << " initializing Parameters..." ) );

  theBPWarmCfg.Cores.initialize(getSystemWidth());

//  theFeederCfg.SimicsQuantum.initialize(100);
  theFeederCfg.CMPWidth.initialize( getSystemWidth() );
  theFeederCfg.TrackIFetch.initialize(true);
  theFeederCfg.HousekeepingPeriod.initialize(1000);
  theFeederCfg.SystemTickFrequency.initialize(0.0);

  static const int K = 1024;
  static const int M = 1024 * K;

  theL1DCfg.MTWidth.initialize( 1 );
  theL1DCfg.Size.initialize(32 * K);
  theL1DCfg.Associativity.initialize(2);
  theL1DCfg.BlockSize.initialize(64);
  theL1DCfg.CleanEvictions.initialize(false);
  theL1DCfg.CacheLevel.initialize(eL1);
  theL1DCfg.TraceTracker.initialize(false);
  theL1DCfg.NotifyReads.initialize(false);
  theL1DCfg.NotifyWrites.initialize(false);

  theL1ICfg.MTWidth.initialize( 1 );
  theL1ICfg.Size.initialize(32 * K);
  theL1ICfg.Associativity.initialize(2);
  theL1ICfg.BlockSize.initialize(64);
  theL1ICfg.CleanEvictions.initialize(false);
  theL1ICfg.CacheLevel.initialize(eL1I);
  theL1ICfg.TraceTracker.initialize(false);
  theL1ICfg.NotifyReads.initialize(false);
  theL1ICfg.NotifyWrites.initialize(false);

  theL2Cfg.CMPWidth.initialize( getSystemWidth() );
  theL2Cfg.Size.initialize(8 * M);
  theL2Cfg.Associativity.initialize(8);
  theL2Cfg.BlockSize.initialize(64);
  theL2Cfg.CleanEvictions.initialize(false);
  theL2Cfg.CacheLevel.initialize(eL2);
  theL2Cfg.TraceTracker.initialize(false);
  theL2Cfg.SeparateID.initialize(true);

  theMagicBreakCfg.CkptCycleInterval.initialize(0);
  theMagicBreakCfg.CkptCycleName.initialize(0);
  theMagicBreakCfg.CheckpointOnIteration.initialize(false);
  theMagicBreakCfg.CheckpointEveryXTransactions.initialize(false);
  theMagicBreakCfg.TerminateOnTransaction.initialize(-1);
  theMagicBreakCfg.FirstTransactionIs.initialize(0);
  theMagicBreakCfg.CycleMinimum.initialize(0);
  theMagicBreakCfg.TransactionStatsInterval.initialize(10000);
  theMagicBreakCfg.StopCycle.initialize(0);
  theMagicBreakCfg.EnableTransactionCounts.initialize(false);
  theMagicBreakCfg.TransactionType.initialize(0);
  theMagicBreakCfg.TerminateOnIteration.initialize(-1);
  theMagicBreakCfg.TerminateOnMagicBreak.initialize(-1);
  theMagicBreakCfg.EnableIterationCounts.initialize(false);

  theFlexus->setStatInterval( "10000000" );     //10M
  theFlexus->setProfileInterval( "10000000" );  //10M
  theFlexus->setTimestampInterval( "1000000" ); //1M

  return false; //Abort simulation if parameters are not initialized
}

#include FLEXUS_END_COMPONENT_CONFIGURATION_SECTION()


#include FLEXUS_BEGIN_COMPONENT_INSTANTIATION_SECTION()
//All component Instances are created here.  This section
//also creates handles for each component


FLEXUS_INSTANTIATE_COMPONENT( DecoupledFeeder, theFeederCfg, theFeeder );
FLEXUS_INSTANTIATE_COMPONENT_ARRAY( BPWarm, theBPWarmCfg, theBPWarm, SCALE_WITH_SYSTEM_WIDTH, MULTIPLY, 1);
FLEXUS_INSTANTIATE_COMPONENT_ARRAY( FastCache, theL1DCfg, theL1D, SCALE_WITH_SYSTEM_WIDTH, MULTIPLY, 1);
FLEXUS_INSTANTIATE_COMPONENT_ARRAY( FastCache, theL1ICfg, theL1I, SCALE_WITH_SYSTEM_WIDTH, MULTIPLY, 1);
FLEXUS_INSTANTIATE_COMPONENT( FastCMPCache, theL2Cfg, theL2 );
FLEXUS_INSTANTIATE_COMPONENT( FastMemoryLoopback, theMemoryCfg, theMemory );
FLEXUS_INSTANTIATE_COMPONENT( MagicBreak, theMagicBreakCfg, theMagicBreak );
//FLEXUS_INSTANTIATE_COMPONENT( WhiteBox, theWhiteBoxCfg, theWhiteBox );

#include FLEXUS_END_COMPONENT_INSTANTIATION_SECTION()



#include FLEXUS_BEGIN_COMPONENT_WIRING_SECTION()

//FROM                                  TO
//====                                  ==
WIRE(theFeeder, ToL1D,                  theL1D, RequestIn)
WIRE(theFeeder, ToL1I,                  theL1I, FetchRequestIn)
WIRE(theFeeder, ToBPred,                theBPWarm, ITraceInModern)
WIRE(theFeeder, ToDMA,                  theMemory, DMA)

WIRE(theL1D, RequestOut,                theL2, RequestIn)
WIRE(theL1I, RequestOut,                theL2, FetchRequestIn)

WIRE(theL2, SnoopOutI,                  theL1I, SnoopIn)
WIRE(theL2, SnoopOutD,                  theL1D, SnoopIn)

WIRE(theL1D, RegionNotify,              theL2, RegionNotify)

WIRE(theL2, RequestOut,                 theMemory, FromCache)
WIRE(theL2, RegionProbe,                theL1D, RegionProbe)

WIRE(theMemory, ToCache,                theL2, SnoopIn)

//Fetch, L1I and Execute


#include FLEXUS_END_COMPONENT_WIRING_SECTION()


#include FLEXUS_BEGIN_DRIVE_ORDER_SECTION()

DRIVE( theMagicBreak, TickDrive )
, DRIVE( theL1D, UpdateStatsDrive )
, DRIVE( theL1I, UpdateStatsDrive )
, DRIVE( theL2, UpdateStatsDrive )

#include FLEXUS_END_DRIVE_ORDER_SECTION()

