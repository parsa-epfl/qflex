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
#include <functional>

#ifndef FLEXUS_CORE_FLEXUS_HPP__INCLUDED
#define FLEXUS_CORE_FLEXUS_HPP__INCLUDED

namespace Flexus {
namespace Core {

class FlexusInterface {
public:
  //Initialization functions
  virtual ~FlexusInterface() {}
  virtual void initializeComponents() = 0;

  //The main cycle function
  virtual void doCycle() = 0;
  //fast-forward through cycles
  //Note: does not tick stats, check watchdogs, or call drives
  virtual void advanceCycles(int64_t aCycleCount) = 0;
  virtual void invokeDrives() = 0;

  //Simulator state inquiry
  virtual bool isFastMode() const = 0;
  virtual bool isQuiesced() const = 0;
  virtual bool quiescing() const = 0;
  virtual uint64_t cycleCount() const = 0;
  virtual bool initialized() const = 0;

  //Watchdog Functions
  virtual void watchdogCheck() = 0;
  virtual void watchdogIncrement() = 0;
  virtual void watchdogReset(uint32_t anIndex) = 0;

  //Debugging support functions
  virtual int32_t breakCPU() const = 0;
  virtual int32_t breakInsn() const = 0;

  virtual void onTerminate( std::function<void () > ) = 0;
  virtual void terminateSimulation() = 0;
  virtual void quiesce() = 0;
  virtual void quiesceAndSave(uint32_t aSaveNum) = 0;
  virtual void quiesceAndSave() = 0;

  virtual void setDebug(std::string const & aDebugSeverity) = 0;

  virtual void setStatInterval(std::string const & aValue) = 0;
  virtual void setProfileInterval(std::string const & aValue) = 0;
  virtual void setTimestampInterval(std::string const & aValue) = 0;
  virtual void setRegionInterval(std::string const & aValue) = 0;

};

extern FlexusInterface * theFlexus;

}  //End Namespace Core
} //namespace Flexus

#endif //FLEXUS_CORE_FLEXUS_HPP__INCLUDED
