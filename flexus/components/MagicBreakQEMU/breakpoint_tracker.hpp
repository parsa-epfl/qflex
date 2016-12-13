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
#include <iostream>
#include <core/boost_extensions/intrusive_ptr.hpp>

namespace nMagicBreak {

class IterationTracker;
class RegressionTracker;
class CycleTracker;
class ConsoleStringTracker;

struct BreakpointTracker : public boost::counted_base {
public:
  static boost::intrusive_ptr<IterationTracker> newIterationTracker();
  static boost::intrusive_ptr<RegressionTracker> newRegressionTracker();
  static boost::intrusive_ptr<BreakpointTracker> newTransactionTracker( int32_t aTransactionType = 1, int32_t aStopTransactionCount = -1, int32_t aStatInterval = -1, int32_t aCkptInterval = -1, int32_t aFirstTransactionIs = 0, uint64_t aMinCycles = 0 );
  static boost::intrusive_ptr<BreakpointTracker> newTerminateOnMagicBreak(int32_t aMagicBreakpoint);
  static boost::intrusive_ptr<BreakpointTracker> newSimPrintHandler();
  static boost::intrusive_ptr<CycleTracker> newCycleTracker( uint64_t aStopCycle, uint64_t aCkptInterval, uint32_t aCkptNameStart );
  static boost::intrusive_ptr<BreakpointTracker> newPacketTracker( int32_t aSrcPortNumber, char aServerMACCode, char aClientMACCode );
  static boost::intrusive_ptr<ConsoleStringTracker> newConsoleStringTracker();
  virtual ~BreakpointTracker() {};
};

struct IterationTracker : public BreakpointTracker {
  virtual void printIterationCounts(std::ostream & aStream) = 0;
  virtual void setIterationCount(uint32_t aCPU, int32_t aCount) = 0;
  virtual void enable() = 0;
  virtual void endOnIteration(int32_t aCount) = 0;
  virtual void enableCheckpoints() = 0;
  virtual void saveState(std::ostream &) = 0;
  virtual void loadState(std::istream &) = 0;
  virtual ~IterationTracker() {};
};

struct RegressionTracker : public BreakpointTracker {
  virtual void enable() = 0;
  virtual ~RegressionTracker () {};
};

struct ConsoleStringTracker : public BreakpointTracker {
  virtual void addString(std::string const &) = 0;
  virtual ~ConsoleStringTracker () {};
};

struct SimPrintHandler : public BreakpointTracker {
  virtual ~SimPrintHandler() {};
};

struct CycleTracker : public BreakpointTracker {
  virtual void tick() = 0;
  virtual ~CycleTracker() {};
};

} //namespace nMagicBreak

