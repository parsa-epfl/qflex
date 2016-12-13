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
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <map>
#include <set>

#include <boost/optional.hpp>
#include <core/boost_extensions/lexical_cast.hpp>

#include <core/target.hpp>

//FIXME need to make sure that CONFIG_QEMU actually works
#ifndef CONFIG_QEMU
#define CONFIG_QEMU
#endif
// #ifndef CONFIG_QEMU
// #include <core/simics/control_api.hpp>
// #endif

#include <core/flexus.hpp>
#include <core/stats.hpp>

#include <core/boost_extensions/circular_buffer.hpp>

#include <core/debug/action.hpp>
#include <core/debug/debugger.hpp>

namespace Flexus {

namespace Core {
void Break();
}

namespace Dbg {

void CompoundAction::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
	for(auto* anAction: theActions)
	{
		anAction->printConfiguration(anOstream, anIndent);
	}
}

void CompoundAction::process(Entry const & anEntry) {
	for (auto* anAction : theActions)
	{
		anAction->process(anEntry);
	}
}

CompoundAction::~CompoundAction() {
	for (auto* anAction : theActions)
	{
		delete anAction;
	}
}

void CompoundAction::add(std::unique_ptr<Action> anAction) {
  theActions.push_back(anAction.release()); //Ownership assumed by theActions
}

void ConsoleLogAction::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
  anOstream << anIndent << "log console";
  theFormat->printConfiguration(anOstream, "");
  anOstream << ";\n";
}

void ConsoleLogAction::process(Entry const & anEntry) {
  theFormat->format(std::cerr, anEntry);
  std::cerr.flush();
}

class StreamManager {
  typedef std::map<std::string, std::ofstream *> stream_map;
  stream_map theStreams;
public:
  std::ostream & getStream(std::string const & aFile) {
    std::pair<stream_map::iterator, bool> insert_result;
    insert_result = theStreams.insert( std::make_pair( aFile, static_cast<std::ofstream *>(0) ));
    if ( insert_result.second ) {
      (*insert_result.first).second = new std::ofstream(aFile.c_str());
      std::cout << "Opening debug output file: " << aFile << "\n";
    }
    return *(*insert_result.first).second;
  }

  ~StreamManager() {
    stream_map::iterator iter = theStreams.begin();
    while (iter != theStreams.end()) {
      (*iter).second->close();
      delete (*iter).second;
      ++iter;
    }
  }
};

//The StreamManager instance
std::unique_ptr<StreamManager> theStreamManager;

//StreamManager accessor
inline StreamManager & streamManager() {
  if (theStreamManager.get() == 0) {
    theStreamManager.reset(new StreamManager());
  }
  return *theStreamManager;
}

FileLogAction::FileLogAction(std::string aFilename, Format * aFormat)
  : theFilename(aFilename)
  , theOstream(streamManager().getStream(aFilename) )
  , theFormat(aFormat)
{};

void FileLogAction::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
  anOstream << anIndent << "log " << theFilename;
  theFormat->printConfiguration(anOstream, "");
  anOstream << ";\n";
}

void FileLogAction::process(Entry const & anEntry) {
  theFormat->format(theOstream, anEntry);
  theOstream.flush();
}

void AbortAction::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
  anOstream << anIndent << "abort ;";
}

void AbortAction::process(Entry const & anEntry) {
  std::abort();
}

void BreakAction::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
  anOstream << anIndent << "break ;";
}

void BreakAction::process(Entry const & anEntry) {
  Flexus::Core::Break();
}

void PrintStatsAction::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
  anOstream << anIndent << "print-stats ;";
}

void PrintStatsAction::process(Entry const & anEntry) {
  Flexus::Stat::getStatManager()->printMeasurement("all", std::cout);
}

SeverityAction::SeverityAction(uint32_t aSeverity)
  : theSeverity(aSeverity)
{};

void SeverityAction::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
  anOstream << anIndent << "set-global-severity " << toString(Severity(theSeverity)) << " ;";
}

void SeverityAction::process(Entry const & anEntry) {
  Flexus::Dbg::Debugger::theDebugger->setMinSev( Severity(theSeverity) );
}

#if 0
class SaveBufferManager {
  typedef boost::circular_buffer< Entry const & > entry_buffer;
  typedef std::map<std::string, std::shared_ptr< entry_buffer > > buf_map;
  buf_map theBuffers;
public:
  //Need destructor

  void create(std::string const & aBufferName, uint32_t aSize) {
    if (theBuffers[aBufferName].get() == 0) {
      theBuffers[aBufferName].reset( new entry_buffer (aSize));
    } else {
      if (theBuffers[aBufferName]->capacity() != aSize) {
        theBuffers[aBufferName]->resize(aSize);
      }
    }
  }

  void add(std::string const & aBufferName, Entry const & anEntry) {
    theBuffers[aBufferName]->push_back(anEntry);
  }

  void spill(std::string const & aBufferName, Format const & aFormat, std::ostream & anOstream) {
    entry_buffer::iterator iter(theBuffers[aBufferName]->begin());
    while (iter != theBuffers[aBufferName]->end()) {
      aFormat.format(anOstream, **iter);
      ++iter;
    }
  }
};

std::unique_ptr<SaveBufferManager> theSaveBufferManager;

inline SaveBufferManager & saveBufferManager() {
  if (theSaveBufferManager.get() == 0) {
    theSaveBufferManager.reset(new SaveBufferManager());
  }
  return *theSaveBufferManager;
}

SaveAction::SaveAction(std::string aBufferName, uint32_t aCircularBufferSize)
  : theBufferName(aBufferName)
  , theSize(aCircularBufferSize) {
  saveBufferManager().create(theBufferName, theSize);
}

void SaveAction::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
  anOstream << anIndent << "save (" << theBufferName << ") " << theSize << " ;";
}

void SaveAction::process(Entry const & anEntry) {
  saveBufferManager().add(theBufferName, anEntry);
}

FileSpillAction::FileSpillAction(std::string const & aBufferName, std::string const & aFilename, Format * aFormat)
  : theBufferName(aBufferName)
  , theFilename(aFilename)
  , theOstream(streamManager().getStream(aFilename) )
  , theFormat(aFormat)
{ };

void FileSpillAction::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
  anOstream << anIndent << "spill (" << theBufferName << ")->(" << theFilename << ") ";
  theFormat->printConfiguration(anOstream, "");
  anOstream << ";\n";
}

void FileSpillAction::process(Entry const & anEntry) {
  saveBufferManager().spill(theBufferName, *theFormat, theOstream);
  theOstream.flush();
}

ConsoleSpillAction::ConsoleSpillAction(std::string const & aBufferName, Format * aFormat)
  : theBufferName(aBufferName)
  , theFormat(aFormat)
{ };

void ConsoleSpillAction::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
  anOstream << anIndent << "spill (" << theBufferName << ") -> console ";
  theFormat->printConfiguration(anOstream, "");
  anOstream << ";\n";
}

void ConsoleSpillAction::process(Entry const & anEntry) {
  saveBufferManager().spill(theBufferName, *theFormat, std::cerr);
}
#endif

} //Dbg
} //Flexus

