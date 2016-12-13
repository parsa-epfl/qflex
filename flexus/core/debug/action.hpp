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
#ifndef FLEXUS_CORE_DEBUG_ACTION_HPP_INCLUDED
#define FLEXUS_CORE_DEBUG_ACTION_HPP_INCLUDED

#include <iostream>
#include <string>
#include <vector>
#include <memory>

#include <core/debug/entry.hpp>
#include <core/debug/format.hpp>

namespace Flexus {
namespace Dbg {

class Action {
public:
  virtual void printConfiguration(std::ostream & anOstream, std::string const & anIndent) = 0;
  virtual void process(Entry const & anEntry) = 0;
  virtual ~Action() {}
};

class CompoundAction : public Action {
  std::vector<Action *> theActions; //These actions are owned by the pointers in theActions
public:
  virtual void printConfiguration(std::ostream & anOstream, std::string const & anIndent);
  virtual void process(Entry const & anEntry);
  virtual ~CompoundAction();
  void add(std::unique_ptr<Action> anAction);
};

class ConsoleLogAction : public Action {
  std::unique_ptr<Format> theFormat;
public:
  ConsoleLogAction(Format * aFormat)
    : theFormat(aFormat)
  {};
  virtual void printConfiguration(std::ostream & anOstream, std::string const & anIndent);
  virtual void process(Entry const & anEntry);
};

class FileLogAction : public Action {
  std::string theFilename;
  std::ostream & theOstream;
  std::unique_ptr<Format> theFormat;
public:
  FileLogAction(std::string aFilename, Format * aFormat);
  virtual void printConfiguration(std::ostream & anOstream, std::string const & anIndent);
  virtual void process(Entry const & anEntry);
};

struct AbortAction : public Action {
  virtual void printConfiguration(std::ostream & anOstream, std::string const & anIndent);
  virtual void process(Entry const & anEntry);
};

struct BreakAction : public Action {
  virtual void printConfiguration(std::ostream & anOstream, std::string const & anIndent);
  virtual void process(Entry const & anEntry);
};

struct PrintStatsAction : public Action {
  virtual void printConfiguration(std::ostream & anOstream, std::string const & anIndent);
  virtual void process(Entry const & anEntry);
};

struct SeverityAction : public Action {
  uint32_t theSeverity;
public:
  SeverityAction(uint32_t aSeverity);
  virtual void printConfiguration(std::ostream & anOstream, std::string const & anIndent);
  virtual void process(Entry const & anEntry);
};

#if 0
class SaveAction : public Action {
  std::string theBufferName;
  uint32_t theSize;
public:
  SaveAction(std::string aBufferName, uint32_t aCircularBufferSize);
  virtual void printConfiguration(std::ostream & anOstream, std::string const & anIndent);
  virtual void process(Entry const & anEntry);
};

class FileSpillAction : public Action {
  std::string theBufferName;
  std::string theFilename;
  std::ostream & theOstream;
  std::unique_ptr<Format> theFormat;
public:
  FileSpillAction(std::string const & aBufferName, std::string const & aFilename, Format * aFormat);
  virtual void printConfiguration(std::ostream & anOstream, std::string const & anIndent);
  virtual void process(Entry const & anEntry);
};

class ConsoleSpillAction : public Action {
  std::string theBufferName;
  std::unique_ptr<Format> theFormat;
public:
  ConsoleSpillAction(std::string const & aBufferName, Format * aFormat);
  virtual void printConfiguration(std::ostream & anOstream, std::string const & anIndent);
  virtual void process(Entry const & anEntry);
};
#endif

} //Dbg
} //Flexus

#endif //FLEXUS_CORE_DEBUG_ACTION_HPP_INCLUDED

