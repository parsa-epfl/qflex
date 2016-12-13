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
#include <vector>
#include <string>
#include <memory>
#include <queue>

#include <boost/optional.hpp>

#include <core/boost_extensions/lexical_cast.hpp>

#include <core/debug/severity.hpp>
#include <core/debug/category.hpp>
#include <core/debug/field.hpp>
#include <core/debug/entry.hpp>
#include <core/debug/target.hpp>

#ifndef FLEXUS_CORE_DEBUG_DEBUGGER_HPP_INCLUDED
#define FLEXUS_CORE_DEBUG_DEBUGGER_HPP_INCLUDED

namespace Flexus {
namespace Dbg {

struct At {
  int64_t theCycle;
  Action * theAction;

  At( int64_t aCycle, Action * anAction)
    : theCycle(aCycle)
    , theAction(anAction)
  {}

  friend bool operator < (At const & aLeft, At const & aRight) {
    return aLeft.theCycle > aRight.theCycle;
  }
};

class Debugger {
  std::vector<Target *> theTargets; //Owns all targets
  std::map<std::string, bool *> theCategories;
  std::map<std::string, std::vector< bool *> > theComponents;

  int64_t theCount;
  uint64_t * theCycleCount;

  std::priority_queue<At> theAts; //Owns all targets

public:
  static Debugger * theDebugger;
  Severity theMinimumSeverity;

  Debugger()
    : theCount(0)
    , theCycleCount(0)
    , theMinimumSeverity(SevDev) {
  }

  ~Debugger();
  void initialize();
  void addFile(std::string const &);
  static void constructDebugger();

  int64_t count() {
    return ++theCount;
  }

  int64_t cycleCount() {
    if (theCycleCount)
      return *theCycleCount;
    else
      return 0;
  }

  void connectCycleCount(uint64_t * aCount) {
    theCycleCount = aCount;
  }

  void process(Entry const & anEntry);
  void printConfiguration(std::ostream & anOstream);
  void add(Target * aTarget);
  void registerCategory(std::string const & aCategory, bool * aSwitch);
  bool setAllCategories(bool aValue);
  bool setCategory(std::string const & aCategory, bool aValue);
  void listCategories(std::ostream & anOstream);
  void registerComponent(std::string const & aComponent, uint32_t anIndex, bool * aSwitch);
  bool setAllComponents(int32_t anIndex, bool aValue);
  bool setComponent(std::string const & aCategory, int32_t anIndex, bool aValue);
  void listComponents(std::ostream & anOstream);
  void addAt(int64_t aCycle, Action * anAction);
  void checkAt();
  void doNextAt();
  void reset();

  void setMinSev(Severity aSeverity) {
    theMinimumSeverity = aSeverity;
  }
};

struct DebuggerConstructor {
  DebuggerConstructor() {
    Debugger::constructDebugger();
  }
};

} //Dbg
} //Flexus

namespace {
Flexus::Dbg::DebuggerConstructor construct_debugger;
}

#endif //FLEXUS_CORE_DEBUG_DEBUGGER_HPP_INCLUDED

