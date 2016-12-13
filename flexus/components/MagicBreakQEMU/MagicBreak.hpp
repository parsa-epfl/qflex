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
#include <core/simulator_layout.hpp>

#define FLEXUS_BEGIN_COMPONENT MagicBreak
#include FLEXUS_BEGIN_COMPONENT_DECLARATION()

#define MagicBreak_IMPLEMENTATION    (<components/MagicBreak/MagicBreakImpl.hpp>)

COMPONENT_PARAMETERS(
  PARAMETER( EnableIterationCounts, bool, "Enable Iteration Counts", "iter", false )
  PARAMETER( TerminateOnMagicBreak, int, "Terminate simulation on a specific magic breakpoint", "stop_on_magic", -1 )
  PARAMETER( TerminateOnIteration, int, "Terminate simulation when CPU 0 reaches iteration.  -1 disables", "end_iter", -1 )
  PARAMETER( CheckpointOnIteration, bool, "Checkpoint simulation when CPU 0 reaches each iteration.", "ckpt_iter", false )
  PARAMETER( TerminateOnTransaction, int, "Terminate simulation after ## transactions.  -1 disables", "end_trans", -1 )
  PARAMETER( EnableTransactionCounts, bool, "Enable Transaction Counts", "trans", false )
  PARAMETER( TransactionType, int, "Workload type.  0=TPCC/JBB  1=WEB", "trans_type", 0 )
  PARAMETER( TransactionStatsInterval, int, "Statistics interval on ## transactions.  -1 disables", "stats_trans", -1 )
  PARAMETER( CheckpointEveryXTransactions, int, "Quiesce and save every X transactions. -1 disables", "ckpt_trans", -1 )
  PARAMETER( FirstTransactionIs, int, "Transaction number for first transaction.", "first_trans", 0 )
  PARAMETER( CycleMinimum, uint64_t, "Minimum number of cycles to run when TerminateOnTransaction is enabled.", "min_cycle", 0 )
  PARAMETER( StopCycle, uint64_t, "Cycle on which to halt simulation.", "stop_cycle", 0 )
  PARAMETER( CkptCycleInterval, uint64_t, "# of cycles between checkpoints.", "ckpt_cycle", 0 )
  PARAMETER( CkptCycleName, uint32_t, "Base cycle # from which to build checkpoint names.", "ckpt_cycle_name", 0 )
);

COMPONENT_INTERFACE(
  DRIVE( TickDrive )
);

#include FLEXUS_END_COMPONENT_DECLARATION()
#define FLEXUS_END_COMPONENT MagicBreak

