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
#include <components/MagicBreakQEMU/MagicBreak.hpp>

#define FLEXUS_BEGIN_COMPONENT MagicBreak
#include FLEXUS_BEGIN_COMPONENT_IMPLEMENTATION()

#include <fstream>

#include <components/MagicBreakQEMU/breakpoint_tracker.hpp>
#include <core/qemu/configuration_api.hpp>
#include <core/stats.hpp>

namespace nMagicBreak {

namespace Stat = Flexus::Stat;

using namespace Flexus;
using namespace Core;
using namespace Qemu;
using namespace API;
using namespace SharedTypes;

class ConsoleBreakString_QemuObject_Impl  {
  boost::intrusive_ptr< ConsoleStringTracker > theStringTracker;
public:
  ConsoleBreakString_QemuObject_Impl(Flexus::Qemu::API::conf_object_t * /*ignored*/ ) : theStringTracker(0) {}

  void setConsoleStringTracker(boost::intrusive_ptr< ConsoleStringTracker > aTracker) {
    theStringTracker = aTracker;
  }

  void addString(std::string const & aString) {
    DBG_Assert(theStringTracker);
    theStringTracker->addString(std::string(aString));
  }

};

class ConsoleBreakString_QemuObject : public Qemu::AddInObject <ConsoleBreakString_QemuObject_Impl> {
  typedef Qemu::AddInObject<ConsoleBreakString_QemuObject_Impl> base;
public:
  static const Qemu::Persistence  class_persistence = Qemu::Session;
  static std::string className() {
    return "ConsoleBreakString";
  }
  static std::string classDescription() {
    return "ConsoleBreakString object";
  }

  ConsoleBreakString_QemuObject() : base() { }
  ConsoleBreakString_QemuObject(Qemu::API::conf_object_t * aQemuObject) : base(aQemuObject) {}
  ConsoleBreakString_QemuObject(ConsoleBreakString_QemuObject_Impl * anImpl) : base(anImpl) {}

  template <class Class>
  static void defineClass(Class & aClass) {

#if 0
    aClass.addCommand
    ( & ConsoleBreakString_QemuObject_Impl::addString
      , "add-break-string"
      , "Add a string to the list of console strings that will halt simulation"
      , "string"
    );
#endif
  }

};

Qemu::Factory<ConsoleBreakString_QemuObject> theConBreakFactory;

class RegressionTesting_QemuObject_Impl  {
  boost::intrusive_ptr< RegressionTracker > theRegressionTracker;
public:
  RegressionTesting_QemuObject_Impl(Flexus::Qemu::API::conf_object_t * /*ignored*/ ) : theRegressionTracker(0) {}

  void setRegressionTracker(boost::intrusive_ptr< RegressionTracker > aTracker) {
    theRegressionTracker = aTracker;
  }

  void enable() {
    DBG_Assert(theRegressionTracker);
    theRegressionTracker->enable();
  }

};

class RegressionTesting_QemuObject : public Qemu::AddInObject <RegressionTesting_QemuObject_Impl> {
  typedef Qemu::AddInObject<RegressionTesting_QemuObject_Impl> base;
public:
  static const Qemu::Persistence  class_persistence = Qemu::Session;
  static std::string className() {
    return "RegressionTesting";
  }
  static std::string classDescription() {
    return "RegressionTesting object";
  }

  RegressionTesting_QemuObject() : base() { }
  RegressionTesting_QemuObject(Qemu::API::conf_object_t * aQemuObject) : base(aQemuObject) {}
  RegressionTesting_QemuObject(RegressionTesting_QemuObject_Impl * anImpl) : base(anImpl) {}

  template <class Class>
  static void defineClass(Class & aClass) {

#if 0
    aClass.addCommand
    ( & RegressionTesting_QemuObject_Impl::enable
      , "enable"
      , "Enable regression testing"
    );
#endif
  }

};

Qemu::Factory<RegressionTesting_QemuObject> theRegressionTestingFactory;

class IterationTracker_QemuObject_Impl  {
  boost::intrusive_ptr< IterationTracker > theIterationTracker; //Non-owning pointer
public:
  IterationTracker_QemuObject_Impl(Flexus::Qemu::API::conf_object_t * /*ignored*/ ) : theIterationTracker(0) {}

  void setIterationTracker(boost::intrusive_ptr< IterationTracker > anIterTracker) {
    theIterationTracker = anIterTracker;
  }

  void setIterationCount(int32_t aCpu, int32_t aCount) {
    DBG_Assert(theIterationTracker);
    theIterationTracker->setIterationCount(aCpu, aCount);
  }

  void printIterationCounts() {
    DBG_Assert(theIterationTracker);
    theIterationTracker->printIterationCounts(std::cout);
  }

  void enable() {
    DBG_Assert(theIterationTracker);
    theIterationTracker->enable();
  }

  void enableCheckpoints() {
    DBG_Assert(theIterationTracker);
    theIterationTracker->enableCheckpoints();
  }

  void endOnIteration(int32_t anInteration) {
    DBG_Assert(theIterationTracker);
    theIterationTracker->endOnIteration(anInteration);
  }

  void saveState(std::ostream & aStream) {
    DBG_Assert(theIterationTracker);
    theIterationTracker->saveState(aStream);
  }

  void loadState(std::istream & aStream) {
    DBG_Assert(theIterationTracker);
    theIterationTracker->loadState(aStream);
  }

};

class IterationTracker_QemuObject : public Qemu::AddInObject <IterationTracker_QemuObject_Impl> {
  typedef Qemu::AddInObject<IterationTracker_QemuObject_Impl> base;
public:
  static const Qemu::Persistence  class_persistence = Qemu::Session;
  //These constants are defined in Qemu/simics.cpp
  static std::string className() {
    return "IterationTracker";
  }
  static std::string classDescription() {
    return "IterationTracker object";
  }

  IterationTracker_QemuObject() : base() { }
  IterationTracker_QemuObject(Qemu::API::conf_object_t * aQemuObject) : base(aQemuObject) {}
  IterationTracker_QemuObject(IterationTracker_QemuObject_Impl * anImpl) : base(anImpl) {}

  template <class Class>
  static void defineClass(Class & aClass) {
#if 0
    aClass.addCommand
    ( & IterationTracker_QemuObject_Impl::setIterationCount
      , "set-iteration-count"
      , "Set the iteration count for a cpu"
      , "cpu"
      , "count"
    );

    aClass.addCommand
    ( & IterationTracker_QemuObject_Impl::printIterationCounts
      , "print-counts"
      , "Print out iteration counts"
    );
#endif

  }

};

Qemu::Factory<IterationTracker_QemuObject> theIterationTrackerFactory;

class FLEXUS_COMPONENT(MagicBreak) {
  FLEXUS_COMPONENT_IMPL(MagicBreak);

  std::vector< boost::intrusive_ptr< BreakpointTracker > > theTrackers;
  IterationTracker_QemuObject theIterationTrackerObject;
  RegressionTesting_QemuObject theRegressionTestingObject;
  ConsoleBreakString_QemuObject theConsoleStringObject;
  std::ofstream theTransactionsOut;
  boost::intrusive_ptr<CycleTracker> theCycleTracker;

public:
  FLEXUS_COMPONENT_CONSTRUCTOR(MagicBreak)
    : base( FLEXUS_PASS_CONSTRUCTOR_ARGS ) {
    boost::intrusive_ptr<IterationTracker> tracker(BreakpointTracker::newIterationTracker());
    theTrackers.push_back(tracker);
    theIterationTrackerObject = theIterationTrackerFactory.create("iteration-tracker");
    theIterationTrackerObject->setIterationTracker(tracker);

    boost::intrusive_ptr<RegressionTracker> reg_tracker(BreakpointTracker::newRegressionTracker());
    theTrackers.push_back(reg_tracker);
    theRegressionTestingObject = theRegressionTestingFactory.create("regression-testing");
    theRegressionTestingObject->setRegressionTracker(reg_tracker);

    try {
      boost::intrusive_ptr<ConsoleStringTracker> con_tracker(BreakpointTracker::newConsoleStringTracker());
      theTrackers.push_back(con_tracker);
      theConsoleStringObject = theConBreakFactory.create("console-tracker");
      theConsoleStringObject->setConsoleStringTracker(con_tracker);
    } catch (QemuException e) {
      DBG_(Crit, ( << "Cannot support graphical console. Need to switch to string-based terminal"));
      exit(1);
    }
  }

  bool isQuiesced() const {
    return true; //MagicBreakComponent is always quiesced
  }

  void saveState(std::string const & aDirName) {
    std::string fname( aDirName );
    fname += "/" + statName();
    std::ofstream ofs(fname.c_str());

    theIterationTrackerObject->saveState ( ofs );

    ofs.close();
  }

  void loadState(std::string const & aDirName) {
    std::string fname( aDirName);
    fname += "/" + statName();
    std::ifstream ifs(fname.c_str());
    if (! ifs.good()) {
      DBG_( Dev, ( << " saved checkpoint state " << fname << " not found." )  );
    } else {
      theIterationTrackerObject->loadState ( ifs );
      ifs.close();
    }
  }

  void initialize() {
    if (cfg.EnableIterationCounts) {
      theIterationTrackerObject->enable();
      if (cfg.CheckpointOnIteration) {
        theIterationTrackerObject->enableCheckpoints();
      }
      theIterationTrackerObject->endOnIteration(cfg.TerminateOnIteration);
    }
    if (cfg.TerminateOnMagicBreak >= 0) {
      theTrackers.push_back(BreakpointTracker::newTerminateOnMagicBreak(cfg.TerminateOnMagicBreak));
    }
    if (cfg.EnableTransactionCounts) {
      theTrackers.push_back(BreakpointTracker::newTransactionTracker(cfg.TransactionType, cfg.TerminateOnTransaction, cfg.TransactionStatsInterval, cfg.CheckpointEveryXTransactions, cfg.FirstTransactionIs, cfg.CycleMinimum));
      theTransactionsOut.open("transactions.out");
      Stat::getStatManager()->openLoggedPeriodicMeasurement("Transactions", 1000000, Stat::accumulation_type::Accumulate, theTransactionsOut, "(DB2.*)|(JBB.*)|(WEB.*)");
    }
    if (cfg.StopCycle > 0 || cfg.CkptCycleInterval > 0) {
      theCycleTracker = BreakpointTracker::newCycleTracker( cfg.StopCycle, cfg.CkptCycleInterval, cfg.CkptCycleName );
    }
    theTrackers.push_back(BreakpointTracker::newSimPrintHandler());
    theTrackers.push_back(BreakpointTracker::newPacketTracker(8083 /*SpecWEB port*/, 0x24 /*Server MAC byte*/, 0x25 /*Client MAC byte*/));
  }

  void finalize() {}

  void drive(interface::TickDrive const &) {
    if (theCycleTracker) {
      theCycleTracker->tick();
    }
  }

};

}//End namespace nMagicBreak

FLEXUS_COMPONENT_INSTANTIATOR( MagicBreak, nMagicBreak);

#include FLEXUS_END_COMPONENT_IMPLEMENTATION()
#define FLEXUS_END_COMPONENT MagicBreak

#define DBG_Reset
#include DBG_Control()
