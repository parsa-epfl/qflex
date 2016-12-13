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
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

#include <algorithm>
#include <iomanip>
#include <queue>
#include <list>
#include <fstream>
#include <boost/regex.hpp>

#include <boost/throw_exception.hpp>
#include <boost/serialization/split_free.hpp>
#include <functional>
#include <boost/spirit/include/classic_file_iterator.hpp>
#include <boost/optional.hpp>

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include <core/stats.hpp>

namespace Flexus {
namespace Stat {
namespace aux_ {

StatValueHandle_Base::StatValueHandle_Base()
  : theStatName(0)
  , theUpdater(0)
{}
StatValueHandle_Base::StatValueHandle_Base(Stat * aStat, boost::intrusive_ptr<StatUpdaterBase> anUpdater)
  : theStatName(aStat->namePtr())
  , theUpdater(anUpdater)
{}
StatValueHandle_Base::StatValueHandle_Base(std::string const & aStat)
  : theStatName(new std::string(aStat))
  , theUpdater(0)
{}
StatValueHandle_Base::StatValueHandle_Base(const StatValueHandle_Base & aHandle)
  : theStatName(aHandle.theStatName)
  , theUpdater(aHandle.theUpdater)
{ }
StatValueHandle_Base & StatValueHandle_Base::operator =(const StatValueHandle_Base & aHandle) {
  theStatName = aHandle.theStatName;
  theUpdater = aHandle.theUpdater;
  return *this;
}
StatValueHandle_Base & StatValueHandle_Base::operator +=(const StatValueHandle_Base & aHandle) {
  return *this;
}

void StatValueHandle_Base::releaseUpdater() {
  theUpdater = 0;
}

void StatValueHandle_Base::reset() {
  if (theUpdater) {
    theUpdater->reset();
  }
}

std::string StatValueHandle_Base::name() {
  if (theStatName) {
    return *theStatName;
  } else {
    return std::string("");
  }
}

void StatValueHandle_Base::rename(std::string aName) {
  if (theStatName) {
    delete theStatName;
  }
  theStatName = new std::string(aName);
}

std::ostream & operator << (std::ostream & anOstream, StatValueHandle const & aValueHandle) {
  if (aValueHandle.theStatName && aValueHandle.theValue) {
    anOstream << std::setw(40) << std::left << * aValueHandle.theStatName << " " << *aValueHandle.theValue;
  }
  return anOstream;
}
std::ostream & operator << (std::ostream & anOstream, StatValueArrayHandle const & aValueHandle) {
  if (aValueHandle.theStatName && aValueHandle.theValue) {
    anOstream << std::setw(40) << std::left << *aValueHandle.theStatName << " " << *aValueHandle.theValue;
  }
  return anOstream;
}

StatValueHandle::StatValueHandle() {}
StatValueHandle::StatValueHandle(Stat * aStat, boost::intrusive_ptr< StatValueBase > aValue,  boost::intrusive_ptr<StatUpdaterBase> anUpdater)
  : StatValueHandle_Base(aStat, anUpdater)
  , theValue(aValue)
{}
StatValueHandle::StatValueHandle(std::string const & aStat, boost::intrusive_ptr< StatValueBase > aValue)
  : StatValueHandle_Base(aStat)
  , theValue(aValue)
{}
StatValueHandle::StatValueHandle(const StatValueHandle & aHandle)
  : StatValueHandle_Base(aHandle)
  , theValue(aHandle.theValue)
{}
StatValueHandle & StatValueHandle::operator =(const StatValueHandle & aHandle) {
  this->StatValueHandle_Base::operator =(aHandle);
  theValue = aHandle.theValue;
  return *this;
}
StatValueHandle & StatValueHandle::operator +=(const StatValueHandle & aHandle) {
  this->StatValueHandle_Base::operator +=(aHandle);
  theValue->reduceSum(*aHandle.theValue);
  return *this;
}

boost::intrusive_ptr<StatValueBase> StatValueHandle::sumAccumulator() {
  return theValue->sumAccumulator();
}
boost::intrusive_ptr<StatValueBase> StatValueHandle::avgAccumulator() {
  return theValue->avgAccumulator();
}
boost::intrusive_ptr<StatValueBase> StatValueHandle::stdevAccumulator() {
  return theValue->stdevAccumulator();
}
boost::intrusive_ptr<StatValueBase> StatValueHandle::countAccumulator() {
  return theValue->countAccumulator();
}

void StatValueHandle::print(std::ostream & anOstream, std::string const & options) {
  if (theValue) {
    if (options == "name") {
      anOstream << name();
    } else {
      theValue->print(anOstream, options);
    }
  }
}
int64_t StatValueHandle::asLongLong() {
  if (theValue) {
    return theValue->asLongLong();
  } else {
    return 0;
  }
}
int64_t StatValueHandle::asLongLong(std::string const & options) {
  if (theValue) {
    return theValue->asLongLong(options);
  } else {
    return 0;
  }
}
double StatValueHandle::asDouble() {
  if (theValue) {
    return theValue->asDouble();
  } else {
    return 0;
  }
}
double StatValueHandle::asDouble(std::string const & options) {
  if (theValue) {
    return theValue->asDouble(options);
  } else {
    return 0;
  }
}

StatValueArrayHandle::StatValueArrayHandle() {}
StatValueArrayHandle::StatValueArrayHandle(Stat * aStat, boost::intrusive_ptr< StatValueArrayBase > aValue,  boost::intrusive_ptr<StatUpdaterBase> anUpdater)
  : StatValueHandle_Base(aStat, anUpdater)
  , theValue(aValue)
{}
StatValueArrayHandle::StatValueArrayHandle(const StatValueArrayHandle & aHandle)
  : StatValueHandle_Base(aHandle)
  , theValue(aHandle.theValue)
{}
StatValueArrayHandle & StatValueArrayHandle::operator =(const StatValueArrayHandle & aHandle) {
  this->StatValueHandle_Base::operator =(aHandle);
  theValue = aHandle.theValue;
  return *this;
}

void StatValueArrayHandle::newValue(accumulation_type aType) {
  theValue->newValue(aType);
}
void StatValueArrayHandle::print(std::ostream & anOstream, std::string const & options) {
  if (theValue) {
    if (options == "name") {
      anOstream << name();
    } else {
      theValue->print(anOstream, options);
    }
  }
}
int64_t StatValueArrayHandle::asLongLong() {
  if (theValue) {
    return theValue->asLongLong();
  } else {
    return 0;
  }
}
int64_t StatValueArrayHandle::asLongLong(std::string const & options) {
  if (theValue) {
    return theValue->asLongLong(options);
  } else {
    return 0;
  }
}

}

Prediction::Prediction(int64_t anUpdate)
  : theUpdate(anUpdate)
{}

Prediction::~Prediction() {
  dismiss();
}

void Prediction::connectCounter( boost::intrusive_ptr< aux_::StatValue_PredictionCounter > aCounter) {
  theCounters.push_back( aCounter );
}

void Prediction::confirm() {
  for(auto& aCounter: theCounters)
    aCounter->confirm(theUpdate);
  // std::vector< boost::intrusive_ptr< aux_::StatValue_PredictionCounter > >::iterator iter = theCounters.begin();
  // std::vector< boost::intrusive_ptr< aux_::StatValue_PredictionCounter > >::iterator end = theCounters.end();
  // while (iter != end) {
  //   (*iter)->confirm(theUpdate);
  //   ++iter;
  // }
  theCounters.clear();
}

void Prediction::confirm(int64_t anOverride) {
  for(auto& aCounter: theCounters)
    aCounter->confirm(anOverride);
  // std::vector< boost::intrusive_ptr< aux_::StatValue_PredictionCounter > >::iterator iter = theCounters.begin();
  // std::vector< boost::intrusive_ptr< aux_::StatValue_PredictionCounter > >::iterator end = theCounters.end();
  // while (iter != end) {
  //   (*iter)->confirm(anOverride);
  //   ++iter;
  // }
}

void Prediction::dismiss() {
  for(auto& aCounter: theCounters)
    aCounter->dismiss();
  // std::vector< boost::intrusive_ptr< aux_::StatValue_PredictionCounter > >::iterator iter = theCounters.begin();
  // std::vector< boost::intrusive_ptr< aux_::StatValue_PredictionCounter > >::iterator end = theCounters.end();
  // while (iter != end) {
  //   (*iter)->dismiss();
  //   ++iter;
  // }
  theCounters.clear();
}

void Prediction::guess() {
  for(auto& aCounter: theCounters)
    aCounter->guess(theUpdate);
  // std::vector< boost::intrusive_ptr< aux_::StatValue_PredictionCounter > >::iterator iter = theCounters.begin();
  // std::vector< boost::intrusive_ptr< aux_::StatValue_PredictionCounter > >::iterator end = theCounters.end();
  // while (iter != end) {
  //   (*iter)->guess(theUpdate);
  //   ++iter;
  // }
}

void Prediction::guess(int64_t anOverride) {
  for(auto& aCounter: theCounters)
    aCounter->guess(anOverride);
  // std::vector< boost::intrusive_ptr< aux_::StatValue_PredictionCounter > >::iterator iter = theCounters.begin();
  // std::vector< boost::intrusive_ptr< aux_::StatValue_PredictionCounter > >::iterator end = theCounters.end();
  // while (iter != end) {
  //   (*iter)->guess(anOverride);
  //   ++iter;
  // }
}

void Prediction::goodGuess() {
  for(auto& aCounter: theCounters)
    aCounter->goodGuess();
  // std::vector< boost::intrusive_ptr< aux_::StatValue_PredictionCounter > >::iterator iter = theCounters.begin();
  // std::vector< boost::intrusive_ptr< aux_::StatValue_PredictionCounter > >::iterator end = theCounters.end();
  // while (iter != end) {
  //   (*iter)->goodGuess();
  //   ++iter;
  // }
}

void Prediction::reject() {
  for(auto& aCounter: theCounters)
    aCounter->reject(theUpdate);
  // std::vector< boost::intrusive_ptr< aux_::StatValue_PredictionCounter > >::iterator iter = theCounters.begin();
  // std::vector< boost::intrusive_ptr< aux_::StatValue_PredictionCounter > >::iterator end = theCounters.end();
  // while (iter != end) {
  //   (*iter)->reject(theUpdate);
  //   ++iter;
  // }
}

void Prediction::reject(int64_t anOverride) {
  for(auto& aCounter: theCounters)
    aCounter->reject(anOverride);
  // std::vector< boost::intrusive_ptr< aux_::StatValue_PredictionCounter > >::iterator iter = theCounters.begin();
  // std::vector< boost::intrusive_ptr< aux_::StatValue_PredictionCounter > >::iterator end = theCounters.end();
  // while (iter != end) {
  //   (*iter)->reject(anOverride);
  //   ++iter;
  // }
}

namespace aux_ {

void doEXPR( std::ostream & anOstream, std::string const & options, std::map<std::string, Measurement *> & measurements,  std::string const & default_measurement );

template <class BiDirIter>
class LineFormatter {
public:
  typedef std::map<std::string, Measurement *> measurement_map;
private:
  measurement_map & theMeasurements;
  std::ostream & theOut;
  boost::regex theFieldRegex;
  boost::regex theFieldDetailsRegex;
  boost::regex theOperationDetailsRegex;
  std::string theDefaultMeasurement;
public:
  LineFormatter( measurement_map & aMeasurementSet, std::ostream & anOut)
    : theMeasurements(aMeasurementSet)
    , theOut(anOut)
    , theFieldRegex("([^\\{<]*)(\\{[^\\}]*\\})?(<[^>]*>)?")
    , theFieldDetailsRegex("\\{([^;@]*)(;[^@]+)?(@.+)?\\}")
    , theOperationDetailsRegex("<([^:]*)(:.+)?>")
    , theDefaultMeasurement(theMeasurements.begin()->first)
  { }

  bool operator()(const boost::match_results< BiDirIter >& aMatch) {
    if (aMatch[1].matched) {
      theOut << aMatch.str(1);
    }
    if (aMatch[2].matched) {
      boost::match_results<BiDirIter> details;
      if ( boost::regex_match(aMatch[2].first, aMatch[2].second, details, theFieldDetailsRegex)) {
        std::string field(details[1].first, details[1].second);
        std::string options;
        std::string measurement(theDefaultMeasurement);
        if (details[2].matched) {
          BiDirIter iter(details[2].first);
          iter++;
          options = std::string(iter, details[2].second);
        }
        if (details[3].matched) {
          BiDirIter iter(details[3].first);
          iter++;
          measurement = std::string(iter, details[3].second);
        }

        measurement_map::iterator iter = theMeasurements.find(measurement);
        if (iter != theMeasurements.end()) {
          iter->second->format(theOut, field, options);
        } else {
          theOut << "{ERR:No Such Measurement}";
        }
      } else {
        theOut << "{ERR:Bad Field}";
      }
    }
    if (aMatch[3].matched) {
      boost::match_results<BiDirIter> details;
      if ( boost::regex_match(aMatch[3].first, aMatch[3].second, details, theOperationDetailsRegex)) {
        std::string operation(details[1].first, details[1].second);
        std::string options;
        std::string measurement(theDefaultMeasurement);
        if (details[2].matched) {
          BiDirIter iter(details[2].first);
          iter++;
          options = std::string(iter, details[2].second);
        }

        if (operation == "EXPR") {
          doEXPR(theOut, options, theMeasurements, theDefaultMeasurement);
        } else {
          std::map<std::string, Measurement *>::iterator iter = theMeasurements.find(measurement);
          if (iter != theMeasurements.end()) {
            iter->second->doOp(theOut, operation, options);
          } else {
            theOut << "{ERR:No Such Measurement}";
          }
        }
      } else {
        theOut << "{ERR:Bad Operation}";
      }
    }
    return true;
  }

  void format( BiDirIter aFirst, BiDirIter aSecond) {
    boost::regex_iterator<BiDirIter> iter(aFirst, aSecond, theFieldRegex), end{};
    std::for_each(iter, end, *this);
    if (aFirst != aSecond && iter != end) {
    //if (aFirst != aSecond && boost::regex_grep(*this, aFirst, aSecond, theFieldRegex)  > 0) {
      theOut << std::endl;
    }
  }

  void setDefaultMeasurement(std::string const & aMeasurement) {
    theDefaultMeasurement = aMeasurement;
  }

  measurement_map & measurements() {
    return theMeasurements;
  }
  std::ostream & out() {
    return theOut;
  }
};

template <class BiDirIter>
struct Directive : public boost::counted_base {
  virtual bool ready() = 0;
  virtual void addLine(BiDirIter aStart, BiDirIter anEnd) = 0;
  virtual void addDirective(boost::intrusive_ptr<Directive> aDirective) = 0;
  virtual bool process() = 0;
  virtual bool isEnd() {
    return false;
  }
  virtual ~Directive() {}
};

template <class BiDirIter>
struct EndDirective : public Directive<BiDirIter> {
  bool ready() {
    return true;
  }
  void addLine(BiDirIter aStart, BiDirIter anEnd) { }
  void addDirective(boost::intrusive_ptr<Directive<BiDirIter> > aDirective) { }
  bool process() {
    return true;
  }
  bool isEnd() {
    return true;
  }
};

template <class BiDirIter>
class RootDirective {
  boost::intrusive_ptr< Directive<BiDirIter> > theSubDirective;
  std::pair< BiDirIter, BiDirIter > theLine;
  LineFormatter<BiDirIter> & theFormatter;
public:
  RootDirective( LineFormatter<BiDirIter> & aFormatter)
    : theFormatter(aFormatter)
  {}
  bool ready() { return theSubDirective ? theSubDirective->ready(): true;
    // if (theSubDirective) {
    //   return theSubDirective->ready();
    // }
    // return true;
  }
  void addLine(BiDirIter aStart, BiDirIter anEnd) {
    if (theSubDirective) {
      theSubDirective->addLine(aStart, anEnd);
    } else {
      theLine = std::make_pair(aStart, anEnd);
    }
  }
  void addDirective(boost::intrusive_ptr<Directive<BiDirIter> > aDirective) {
    if (theSubDirective) {
      theSubDirective->addDirective(aDirective);
    } else {
      theSubDirective = aDirective;
    }
  }
  bool process() {
    if (theSubDirective) {
      if (theSubDirective->process()) {
        theSubDirective = 0;
      }
    } else {
      theFormatter.format(theLine.first, theLine.second);
    }
    return true;
  }
};

template <class BiDirIter>
class ForEachMeasurementDirective : public Directive<BiDirIter> {
  struct LineEntry {
    boost::intrusive_ptr< Directive<BiDirIter> > theDirective;
    boost::optional< std::pair< BiDirIter, BiDirIter > > theLine;
    LineEntry() {}
    LineEntry(boost::intrusive_ptr< Directive<BiDirIter> > aDirective)
      : theDirective(aDirective)
    {}
    LineEntry(std::pair< BiDirIter, BiDirIter > aLine)
      : theLine(aLine)
    {}
  };

  boost::intrusive_ptr< Directive<BiDirIter> > theSubDirective;
  typedef std::vector< LineEntry > line_vector;
  line_vector theLines;
  LineFormatter<BiDirIter> & theFormatter;
  typedef typename LineFormatter<BiDirIter>::measurement_map measurement_map;
  std::string theMeasurementSpec;
  bool isReady;
public:
  ForEachMeasurementDirective( LineFormatter<BiDirIter> & aFormatter, std::string const & aMeasurementSpec)
    : theFormatter(aFormatter)
    , theMeasurementSpec(aMeasurementSpec)
    , isReady(false)
  {}
  bool ready() {
    if (theSubDirective) {
      return false;
    }
    return isReady;
  }
  void addLine(BiDirIter aStart, BiDirIter anEnd) {
    if (theSubDirective) {
      theSubDirective->addLine(aStart, anEnd);
      if (theSubDirective->ready()) {
        theSubDirective = 0;
      }
    } else {
      theLines.push_back( LineEntry( std::make_pair(aStart, anEnd) ) );
    }
  }
  void addDirective(boost::intrusive_ptr<Directive<BiDirIter> > aDirective) {
    if (theSubDirective) {
      theSubDirective->addDirective(aDirective);
      if (theSubDirective->ready()) {
        theSubDirective = 0;
      }
    } else if ( aDirective->isEnd() ) {
      isReady = true;
    } else {
      theSubDirective = aDirective;
      theLines.push_back( LineEntry( aDirective) );
    }
  }
  bool process() {

    try {
      boost::regex measurement_filter(theMeasurementSpec);
      for(auto& aMeasurement: theFormatter.measurements()){
        if (boost::regex_match(aMeasurement.first, measurement_filter)) {
          theFormatter.setDefaultMeasurement(aMeasurement.first);

          for(auto& aLine: theLines){
            if(aLine.theDirective)
              aLine.theDirective->process();
            else
              theFormatter.format(aLine.theLine->first, aLine.theLine->second);
          }
        }
      }
      // typename measurement_map::iterator iter = theFormatter.measurements().begin();
      // typename measurement_map::iterator end = theFormatter.measurements().end();

      // while (iter != end) {
      //   if (std::regex_match(iter->first, measurement_filter)) {
      //     theFormatter.setDefaultMeasurement(iter->first);

      //     for(auto& aLine: theLines){
      //       if(aLine.theDirective)
      //         aLine.theDirective->process();
      //       else
      //         theFormatter.format(aLine.theLine->first, aLine.theLine->second);
      //     }
      //     // typename line_vector::iterator l_iter = theLines.begin();
      //     // typename line_vector::iterator l_end = theLines.end();

      //     // while (l_iter != l_end) {
      //     //   if (l_iter->theDirective) {
      //     //     l_iter->theDirective->process();
      //     //   }
      //     //   if (l_iter->theLine) {
      //     //     theFormatter.format(l_iter->theLine->first, l_iter->theLine->second);
      //     //   }
      //     //   ++l_iter;
      //     // }
      //   }
      //   ++iter;
      // }

    } catch (boost::regex_error & anExcept) {
      theFormatter.out() << "{ERR:Bad Measurement Spec: " << theMeasurementSpec << "}";
    }

    return true;
  }
};

template <class BiDirIter>
class LineProcessor {
  std::map<std::string, Measurement *> & theMeasurements;
  std::ostream & theOut;
  boost::regex theCommentRegex;
  boost::regex theDirectiveRegex;
  LineFormatter<BiDirIter> theLineFormatter;
  RootDirective<BiDirIter> theDirectiveStack;

public:
  LineProcessor( std::map<std::string, Measurement *> & aMeasurementSet, std::ostream & anOut)
    : theMeasurements(aMeasurementSet)
    , theOut(anOut)
    , theCommentRegex("^[[:space:]]*//.*")
    , theDirectiveRegex("^#([^[:space:]]*)[:space:]?(.+)?")
    , theLineFormatter(aMeasurementSet, anOut)
    , theDirectiveStack(theLineFormatter)
  { }

  bool operator()(const boost::match_results<BiDirIter> & aMatch) {
    //See what kind of line we have
    boost::match_results< BiDirIter > directive_match;
    if (boost::regex_match(aMatch[0].first, aMatch[0].second, theCommentRegex)) {
      //Comments are removed from the output
      return true;
    } else if (boost::regex_match(aMatch[0].first, aMatch[0].second, directive_match, theDirectiveRegex)) {
      if (directive_match.str(1) == "FOR-MEASUREMENTS") {
        std::string measurements(".*");
        if (directive_match[2].matched) {
          measurements = directive_match.str(2);
        }
        theDirectiveStack.addDirective( new ForEachMeasurementDirective<BiDirIter>(theLineFormatter, measurements));
      } else if (directive_match.str(1) == "END") {
        theDirectiveStack.addDirective( new EndDirective<BiDirIter>() ) ;
      } else {
        theOut << "{ERR:Unknown Directive}";
      }
    } else {
      theDirectiveStack.addLine(aMatch[0].first, aMatch[0].second);
    }
    if (theDirectiveStack.ready()) {
      theDirectiveStack.process();
    }
    return true;
  }

};

template <class BiDirIter>
class StatFormatter {
  boost::regex theLineRegex;
  LineProcessor<BiDirIter> theLineProcessor;
public:
  StatFormatter( std::map<std::string, Measurement *> & aMeasurementSet, std::ostream & anOut)
    : theLineRegex(".*$")
    , theLineProcessor(aMeasurementSet, anOut)
  {}

  void format(BiDirIter aBegin, BiDirIter anEnd) {
    boost::regex_iterator<BiDirIter> iter(aBegin, anEnd, theLineRegex);
    boost::regex_iterator<BiDirIter> end{};
    std::for_each(iter, end, theLineProcessor);
    //boost::regex_grep(theLineProcessor, aBegin, anEnd, theLineRegex, std::match_not_dot_newline);
  }
};

}

namespace aux_ {
class StatManagerImpl : public StatManager {
  typedef std::vector< Stat *> stat_collection;
  typedef std::vector< std::string > stat_names;
  typedef std::map< std::string, boost::intrusive_ptr<Measurement> > measurement_collection;

  bool theInitialized;
  stat_collection theStats;
  stat_names theStatNames;
  measurement_collection theMeasurements;
  int64_t theTick;
  boost::intrusive_ptr<Measurement> theAllMeasurement;
  std::list< std::function< void() > > theFinalizers;
  bool theLoaded;

  struct event {
    int64_t theDeadline;
    std::function< void() > theEvent;
    friend bool operator < (event const & aLeft, event const & aRight) {
      return aLeft.theDeadline > aRight.theDeadline; //Lower time means higher priority.
    }
  };

  std::priority_queue<event> theEventQueue;

public:
  StatManagerImpl()
    : theInitialized(false)
    , theTick(0)
    , theLoaded(false)
  {}
  ~StatManagerImpl() {
    // Note: This destructor is never called, the StatManagerImpl is
    // intentionally leaked to allow inclusion of global Stat objects
    // that could otherwise get destroyed before StatManagerImpl and
    // cause an unclean exit.
  }

  void initialize() {
    theInitialized = true;
    theTick = 0;

    theAllMeasurement = openMeasurement("all");
  }

  void registerStat(Stat * aStat) {
    theStats.push_back(aStat);
    theStatNames.push_back(aStat->name());

    for(auto& aMeasurement: theMeasurements)
      if(aMeasurement.second.get() != nullptr)
        aMeasurement.second->addToMeasurement(aStat);

    // measurement_collection::iterator iter = theMeasurements.begin();
    // measurement_collection::iterator end = theMeasurements.end();
    // while (iter != end) {
    //   if (iter->second.get() != 0) {
    //     iter->second->addToMeasurement(aStat);
    //   }
    //   ++iter;
    // }
  }

  boost::intrusive_ptr<Measurement> openMeasurement(std::string const & aName, std::string const & aStatSpec = std::string(".*")) {
    if (theMeasurements.find(aName) == theMeasurements.end()) {
      boost::intrusive_ptr<SimpleMeasurement> measurement(new SimpleMeasurement(aName, aStatSpec));

      for(auto* aStat: theStats)
        measurement->addToMeasurement(aStat);
      // stat_collection::iterator iter = theStats.begin();
      // stat_collection::iterator end = theStats.end();
      // while (iter != end) {
      //   measurement->addToMeasurement(*iter);
      //   ++iter;
      // }

      theMeasurements[aName] = measurement;
      return measurement;
    } else {
      //Need to implement re-opening
      return theMeasurements[aName];
    }
  }

  void openPeriodicMeasurement(std::string const & aName, int64_t aPeriod, accumulation_type anAccumulation, std::string const & aStatSpec = std::string(".*")) {
    if (theMeasurements.find(aName) == theMeasurements.end()) {
      boost::intrusive_ptr<PeriodicMeasurement> measurement(new PeriodicMeasurement(aName, aStatSpec, aPeriod, anAccumulation));
      for(auto* aStat: theStats)
        measurement->addToMeasurement(aStat);
      // stat_collection::iterator iter = theStats.begin();
      // stat_collection::iterator end = theStats.end();
      // while (iter != end) {
      //   measurement->addToMeasurement(*iter);
      //   ++iter;
      // }

      theMeasurements[aName] = measurement;
    } else {
      //Need to implement re-opening
    }
  }

  void openLoggedPeriodicMeasurement(std::string const & aName, int64_t aPeriod, accumulation_type anAccumulation, std::ostream & anOstream, std::string const & aStatSpec = std::string(".*")) {
    if (theMeasurements.find(aName) == theMeasurements.end()) {
      boost::intrusive_ptr<LoggedPeriodicMeasurement> measurement(new LoggedPeriodicMeasurement(aName, aStatSpec, aPeriod, anAccumulation, anOstream));
      for(auto* aStat: theStats)
        measurement->addToMeasurement(aStat);
      // stat_collection::iterator iter = theStats.begin();
      // stat_collection::iterator end = theStats.end();
      // while (iter != end) {
      //   measurement->addToMeasurement(*iter);
      //   ++iter;
      // }

      theMeasurements[aName] = measurement;
    } else {
      //Need to implement re-opening
    }
  }

  void reduceNodes(std::string const & aMeasurementSpec) {
    boost::regex spec(aMeasurementSpec);
    measurement_collection selected_measurements;
    for(auto& aMeasurement: theMeasurements)
      if (boost::regex_match(aMeasurement.first, spec))
        selected_measurements.insert(aMeasurement);
    // measurement_collection::iterator iter = theMeasurements.begin();
    // measurement_collection::iterator end = theMeasurements.end();
    // while (iter != end) {
    //   if (std::regex_match(iter->first, spec)) {
    //     selected_measurements.insert( *iter );
    //   }
    //   ++iter;
    // }

    for(auto& aMeasurement: selected_measurements) {
      auto* msmt = dynamic_cast<SimpleMeasurement *>(aMeasurement.second.get());
      if (msmt) msmt->reduceNodes();
    }
    // measurement_collection::iterator sel_iter = selected_measurements.begin();
    // measurement_collection::iterator sel_end = selected_measurements.end();
    // while (sel_iter != sel_end) {
    //   SimpleMeasurement * msmt = dynamic_cast<SimpleMeasurement *>(sel_iter->second.get());
    //   if (msmt) {
    //     msmt->reduceNodes();
    //   }
    //   ++sel_iter;
    // }
  }

  void finalize() {
    while (! theFinalizers.empty() ) {
      theFinalizers.front()();
      theFinalizers.pop_front();
    }

    for(auto& aMeasurement: theMeasurements)
      aMeasurement.second->close();
    // measurement_collection::iterator iter = theMeasurements.begin();
    // measurement_collection::iterator end = theMeasurements.end();
    // while (iter != end) {
    //   iter->second->close();
    //   ++iter;
    // }
  }

  void closeMeasurement(std::string const & aName) {
    measurement_collection::iterator iter = theMeasurements.find(aName);
    if (iter == theMeasurements.end()) {
      //Trying to close a measurment that doesn't exist
      return;
    }
    Measurement & meas = *iter->second;
    meas.close();
  }

  void listStats(std::ostream & anOstream) {
    for(auto& aStatName: theStatNames)
      anOstream << aStatName << std::endl;
    // stat_names::iterator iter = theStatNames.begin();
    // stat_names::iterator end = theStatNames.end();
    // while (iter != end) {
    //   anOstream << * iter << std::endl;
    //   ++iter;
    // }
  }

  void listMeasurements(std::ostream & anOstream) {
    for(auto& aMeasurement: theMeasurements)
      anOstream << *aMeasurement.second << std::endl;
    // measurement_collection::iterator iter = theMeasurements.begin();
    // measurement_collection::iterator end = theMeasurements.end();
    // while (iter != end) {
    //   anOstream << * iter->second << std::endl;
    //   ++iter;
    // }
  }

  boost::intrusive_ptr<Measurement> doReduce(eReduction aReduction, std::string aName, std::map< std::string, Measurement * > & aMatches) {
    boost::intrusive_ptr<Measurement> collapsedMeasurement( openMeasurement(aName) );

    for(auto& aMatch: aMatches)
      collapsedMeasurement->reduce(aReduction, aMatch.second);
    // std::map< std::string, Measurement * >::iterator matchIter = aMatches.begin();
    // while (matchIter != aMatches.end()) {
    //   collapsedMeasurement->reduce(aReduction, matchIter->second);
    //   ++matchIter;
    // }
    collapsedMeasurement->close();

    return collapsedMeasurement;
  }

  void printMeasurement(std::string const & aMeasurementSpec, std::ostream & anOstream) {
    boost::regex spec(aMeasurementSpec);
    std::map< std::string, Measurement * > matches;
    for(auto& pair: theMeasurements)
      if (boost::regex_match(pair.first, spec))
        matches[pair.first] = pair.second.get();
    // measurement_collection::iterator iter = theMeasurements.begin();
    // measurement_collection::iterator end = theMeasurements.end();
    // while (iter != end) {
    //   if (std::regex_match(iter->first, spec)) {
    //     matches[iter->first] = iter->second.get();
    //   }
    //   ++iter;
    // }
    if (matches.size() == 0) {
      std::cout << "No measurement matches " << aMeasurementSpec << std::endl;
    } else if (matches.size() == 1) {
      std::map< std::string, Measurement * >::iterator matchIter = matches.begin();
      matchIter->second->print(anOstream);
    } else {
      std::cout << "Collapsing all matching measurements:" << std::endl;
      boost::intrusive_ptr<Measurement> collapsedMeasurement( doReduce(eReduction::eSum, "Collapsed", matches) );
      collapsedMeasurement->print(anOstream);
    }
  }

  void format(std::string const & aMeasurementSpec, std::string const & aFormat, std::ostream & anOstream) {
    boost::regex spec(aMeasurementSpec);

    std::map< std::string, Measurement * > matches;
    for(auto& pair: theMeasurements)
      if (boost::regex_match(pair.first, spec))
        matches[pair.first] = pair.second.get();
    // measurement_collection::iterator iter = theMeasurements.begin();
    // measurement_collection::iterator end = theMeasurements.end();
    // while (iter != end) {
    //   if (std::regex_match(iter->first, spec)) {
    //     matches[iter->first] = iter->second.get();
    //   }
    //   ++iter;
    // }
    if (matches.size() == 0) {
      std::cout << "No measurement matches " << aMeasurementSpec << std::endl;
    } else {
      StatFormatter<std::string::const_iterator> formatter(matches, anOstream);
      formatter.format(aFormat.begin(), aFormat.end());
    }
  }

  void formatFile(std::string const & aMeasurementSpec, std::string const & aFile, std::ostream & anOstream) {
    boost::regex spec(aMeasurementSpec);
    std::map< std::string, Measurement * > matches;
    for(auto& pair: theMeasurements)
      if (boost::regex_match(pair.first, spec))
        matches[pair.first] = pair.second.get();
    // measurement_collection::iterator iter = theMeasurements.begin();
    // measurement_collection::iterator end = theMeasurements.end();
    // while (iter != end) {
    //   if (std::regex_match(iter->first, spec)) {
    //     matches[iter->first] = iter->second.get();
    //   }
    //   ++iter;
    // }
    if (matches.size() == 0) {
      std::cout << "No measurement matches " << aMeasurementSpec << std::endl;
    } else {
      boost::spirit::classic::file_iterator<> first(aFile.c_str());

      if (first) {
        boost::spirit::classic::file_iterator<> last = first.make_end();
        StatFormatter< boost::spirit::classic::file_iterator<> > formatter(matches, anOstream);
        formatter.format(first, last);

      } else {
        anOstream << "Format file \""  << aFile <<  "\" not found" << std::endl;
      }

    }
  }

  void collapse(std::string const & aMeasurementSpec, std::string const & aFormat, std::ostream & anOstream) {
    boost::regex spec(aMeasurementSpec);
    std::map< std::string, Measurement * > matches;
    for(auto& pair: theMeasurements)
      if (boost::regex_match(pair.first, spec))
        matches[pair.first] = pair.second.get();
    // measurement_collection::iterator iter = theMeasurements.begin();
    // measurement_collection::iterator end = theMeasurements.end();
    // while (iter != end) {
    //   if (std::regex_match(iter->first, spec)) {
    //     matches[iter->first] = iter->second.get();
    //   }
    //   ++iter;
    // }
    if (matches.size() == 0) {
      std::cout << "No measurement matches " << aMeasurementSpec << std::endl;
    } else {
      std::string collapsedMeasurementName("Collapsed");
      std::map< std::string, Measurement * > toCollapse;
      boost::intrusive_ptr<Measurement> collapsedMeasurement( doReduce(eReduction::eSum, collapsedMeasurementName, matches) );
      toCollapse[collapsedMeasurementName] = collapsedMeasurement.get();

      StatFormatter<std::string::const_iterator> formatter(toCollapse, anOstream);
      formatter.format(aFormat.begin(), aFormat.end());
    }
  }

  void reduce(eReduction aReduction, std::string const & aMeasurementSpec, std::string const & aDestMeasurement, std::ostream & anOstream) {
    boost::regex spec(aMeasurementSpec);
    std::map< std::string, Measurement * > matches;
    for(auto& pair: theMeasurements)
      if (boost::regex_match(pair.first, spec))
        matches[pair.first] = pair.second.get();
    // measurement_collection::iterator iter = theMeasurements.begin();
    // measurement_collection::iterator end = theMeasurements.end();
    // while (iter != end) {
    //   if (std::regex_match(iter->first, spec)) {
    //     matches[iter->first] = iter->second.get();
    //   }
    //   ++iter;
    // }
    if (matches.size() == 0) {
      std::cout << "No measurement matches " << aMeasurementSpec << std::endl;
    } else {
      std::map< std::string, Measurement * > toCollapse;
      boost::intrusive_ptr<Measurement> collapsedMeasurement( doReduce(aReduction, aDestMeasurement, matches) );
      theMeasurements[aDestMeasurement] = collapsedMeasurement.get();
    }
  }

  void collapseFile(std::string const & aMeasurementSpec, std::string const & aFile, std::ostream & anOstream) {
    boost::regex spec(aMeasurementSpec);
    std::map< std::string, Measurement * > matches;
    for(auto& pair: theMeasurements)
      if (boost::regex_match(pair.first, spec))
        matches[pair.first] = pair.second.get();
    // measurement_collection::iterator iter = theMeasurements.begin();
    // measurement_collection::iterator end = theMeasurements.end();
    // while (iter != end) {
    //   if (std::regex_match(iter->first, spec)) {
    //     matches[iter->first] = iter->second.get();
    //   }
    //   ++iter;
    // }
    if (matches.size() == 0) {
      std::cout << "No measurement matches " << aMeasurementSpec << std::endl;
    } else {
      boost::spirit::classic::file_iterator<> first(aFile.c_str());

      if (first) {
        std::string collapsedMeasurementName("Collapsed");
        std::map< std::string, Measurement * > toCollapse;
        boost::intrusive_ptr<Measurement> collapsedMeasurement( doReduce(eReduction::eSum, collapsedMeasurementName, matches) );
        toCollapse[collapsedMeasurementName] = collapsedMeasurement.get();

        boost::spirit::classic::file_iterator<> last = first.make_end();
        StatFormatter< boost::spirit::classic::file_iterator<> > formatter(toCollapse, anOstream);
        formatter.format(first, last);

      } else {
        anOstream << "Format file \""  << aFile <<  "\" not found" << std::endl;
      }

    }
  }

  void saveMeasurements(std::string const & aMeasurementSpec, std::string const & aFile) const {
    boost::regex spec(aMeasurementSpec);
    measurement_collection selected_measurements;
    for(auto& pair: theMeasurements)
      if (boost::regex_match(pair.first, spec))
        selected_measurements.insert(pair);
    // measurement_collection::const_iterator iter = theMeasurements.begin();
    // measurement_collection::const_iterator end = theMeasurements.end();
    // while (iter != end) {
    //   if (std::regex_match(iter->first, spec)) {
    //     selected_measurements.insert( *iter );
    //   }
    //   ++iter;
    // }
    if (selected_measurements.size() == 0) {
      std::cout << "No measurement matches " << aMeasurementSpec << std::endl;
    } else {

      size_t loc = aFile.rfind(".gz");

      std::ofstream anOstream(aFile.c_str());
      boost::iostreams::filtering_ostream aCompressed;

      if (loc != std::string::npos) {
        aCompressed.push(boost::iostreams::gzip_compressor());
        aCompressed.push(anOstream);
      }

      {
        boost::archive::binary_oarchive oa( (loc == std::string::npos)
                                            ? dynamic_cast<std::ostream &>(anOstream)
                                            : dynamic_cast<std::ostream &>(aCompressed)
                                          );

        register_types(oa);

        oa << theStatNames;
        oa << (const measurement_collection)selected_measurements;
        oa << theTick;
      }

      if (loc == std::string::npos) {
        anOstream.close();
      } else {
        aCompressed.reset();
      }

    }
  }

  int64_t ticks() {
    return theTick;
  }
  void tick(int64_t anAdvance = 1) {
    theTick += anAdvance;
    while (! theEventQueue.empty() && ticks() >= theEventQueue.top().theDeadline) {
      theEventQueue.top().theEvent();
      theEventQueue.pop();
    }
  }

  void addFinalizer(std::function<void()> aFinalizer) {
    theFinalizers.push_back(aFinalizer);
  }

  void addEvent(int64_t aDeadline, std::function<void()> anEvent) {
    event evt;
    evt.theDeadline = aDeadline;
    evt.theEvent = anEvent;
    theEventQueue.push(evt);
  }

  template <class Archive>
  void register_types(Archive & ar) const {
    //Only add to the bottom of this list of types
    ar.template register_type<SimpleMeasurement>();
    ar.template register_type<PeriodicMeasurement>();
    ar.template register_type<LoggedPeriodicMeasurement>();

    ar.template register_type<StatValueBase>();
    ar.template register_type<StatValue_Counter>();
    ar.template register_type<StatValueArray_Counter>();
    ar.template register_type<StatValue_Max>();
    ar.template register_type<StatValueArray_Max>();
    ar.template register_type<StatValue_InstanceCounter<std::string> >();
    ar.template register_type<StatValue_Average>();
    ar.template register_type<StatValueArray_Average>();
    ar.template register_type<StatValue_StdDev>();
    ar.template register_type<StatValueArray_StdDev>();
    ar.template register_type<StatValue_Log2Histogram>();
    ar.template register_type<StatValue_Annotation>();
    ar.template register_type<StatValue_PredictionCounter>();
    ar.template register_type<StatValue_InstanceCounter<int64_t> >();
    ar.template register_type<StatValue_WeightedLog2Histogram>();
    ar.template register_type<StatValue_AvgAccumulator>();
    ar.template register_type<StatValue_StdDevAccumulator>();
    ar.template register_type<StatValue_CountAccumulator>();
    ar.template register_type<StatValue_StdDevLog2Histogram>();
    ar.template register_type<StatValue_UniqueCounter<uint32_t> >();
  }

  void save(std::ostream & anOstream) const {
    boost::archive::binary_oarchive oa(anOstream);

    register_types(oa);

    oa << theStatNames;
    oa << theMeasurements;
    oa << theTick;
  }

  void load(std::istream & anIstream) {
    boost::archive::binary_iarchive ia(anIstream);

    register_types(ia);

    ia >> theStatNames;
    ia >> theMeasurements;
    ia >> theTick;
  }

  void loadMore(std::istream & anIstream, std::string const & aPrefix) {
    boost::archive::binary_iarchive ia(anIstream);

    register_types(ia);

    stat_names names;
    measurement_collection measurements;
    int64_t tick;
    ia >> names;
    ia >> measurements;
    ia >> tick;

    for(auto& aMeasurement: measurements){
      auto name = aPrefix + aMeasurement.second->name();
      aMeasurement.second->resetName( name );
      theMeasurements[name] = aMeasurement.second;
    }

    // measurement_collection::iterator iter = measurements.begin();
    // measurement_collection::iterator end = measurements.end();
    // while (iter != end) {
    //   std::string name = iter->second->name();
    //   name = aPrefix + name;
    //   iter->second->resetName( name );
    //   theMeasurements[name] = iter->second;
    //   ++iter;
    // }

  }

};
}

aux_::StatManagerImpl * theStatManagerImpl;

StatManager * getStatManager() {
  if (theStatManagerImpl == nullptr) {
    theStatManagerImpl = new aux_::StatManagerImpl();
  }
  return theStatManagerImpl;
}

namespace aux_ {

void HistogramPrint::doPrint(std::ostream & anOstream, std::string const & options) const {
  if (buckets() > 0 ) {
    if (options.size() > 0) {
      if (options == "buckets") {
        anOstream << buckets();
        return;
      } else if (options.substr(0, 4) == "val:") {
        int64_t name = boost::lexical_cast<int64_t>(options.substr(4));
        int64_t key = 0;
        if (name != 0) {
          key = (int64_t)log2(name);
          if ((1 << key) != name) {
            anOstream << "{bucket must be a power of two}";
            return;
          }
          key++;
        }
        anOstream << bucketVal(key);
        return;
      } else if (options.substr(0, 5) == "count") {
        anOstream << sum();
        return;
      } else if (options.substr(0, 4) == "sum:") {
        int32_t begin = 0;
        int32_t end = buckets() - 1;
        try {
          std::string range = options.substr(4);
          size_t dash = range.find("-");
          if (dash == std::string::npos) {
            //No dash
            begin = end = boost::lexical_cast<int>(range);
          } else {
            std::string begin_str = range.substr(0, dash);
            if (begin_str.size() > 0) {
              begin = boost::lexical_cast<int>(begin_str);
            } else {
              begin = 0;
            }
            std::string end_str = range.substr(dash + 1);
            if (end_str.size() > 0) {
              end = boost::lexical_cast<int>(end_str);
            } else {
              end = buckets() - 1;
            }
          }
        } catch (boost::bad_lexical_cast &) {
          anOstream << "{sum bounds cannot be parsed}";
        } catch (std::out_of_range &) {
          anOstream << "{sum bounds out of range}";
          return;
        }
        int64_t sum = 0;
        for (int32_t i = begin; i <= end && i < buckets(); ++i) {
          sum += bucketVal(i);
        }
        anOstream << sum;
        return;
      } else {
        anOstream << "{Unrecognized option: " << options << " }";
        return;
      }
    }

    anOstream << "\n\tBucket\tSize\n";
    anOstream << "\t" << 0 << ":" << "\t" << bucketVal(0) << "\n";
    int32_t label = 1;
    for (int32_t i = 1; i < buckets(); ++i) {
      anOstream << "\t" << label << ":" << "\t" << bucketVal(i) << "\n";
      label *= 2;
    }
  }
}

void InstanceCounterPrint::doPrint(std::ostream & anOstream, std::string const & options ) const {
  int32_t valuePrintWidth = -1;
  bool valuePrintHex = false;
  bool printAll = false;
  bool calcPercentile = false;
  float percentile = 0.5;
  bool exclude = false;
  int32_t excludeBucket = 0;
  bool countSort = true;
  bool pct100 = false;
  bool printCDF = false;

  if (options.size() > 0) {
    bool done = true;
    try {
      if (options.substr(0, 4) == "val:") {
        int64_t key = boost::lexical_cast<int64_t>(options.substr(4));
        anOstream << count(key);
      } else if (options.substr(0, 5) == "count") {
        anOstream << sum();
      } else if (options.substr(0, 6) == "weight") {
        anOstream << weightedSum();
      } else if (options.substr(0, 3) == "avg") {
        float numer = weightedSum();
        float denom = sum();
        anOstream << (numer / denom);
      } else if (options.substr(0, 7) == "buckets") {
        anOstream << buckets();
      } else if (options.substr(0, 6) == "median") {
        done = false;
        calcPercentile = true;
        percentile = 0.5;
      } else if (options.substr(0, 7) == "pctile:") {
        float pct = boost::lexical_cast<float>(options.substr(7));
        if (! (pct > 0.0 && pct <= 100.0) ) {
          anOstream << "{Percentile must be between (0,100]}";
          return;
        }
        if (pct == 100.0) pct100 = true;
        countSort = false;
        done = false;
        calcPercentile = true;
        percentile = ((float)pct) / 100.0;
      } else if (options.substr(0, 3) == "mlp") {
        anOstream << static_cast<double>(weightedSum()) / (sum() - count(0));
      } else if (options.substr(0, 6) == "width:") {
        valuePrintWidth = boost::lexical_cast<int>(options.substr(6));
        done = false;
      } else if (options.substr(0, 8) == "exclude:") {
        exclude = true;
        excludeBucket = boost::lexical_cast<int>(options.substr(8));
        done = false;
      } else if (options.substr(0, 6) == "hexall") {
        valuePrintHex = true;
        printAll = true;
        done = false;
      } else if (options.substr(0, 3) == "hex") {
        valuePrintHex = true;
        done = false;
      } else if (options.substr(0, 3) == "all") {
        printAll = true;
        done = false;
      } else if (options.substr(0, 6) == "cdfall") {
        printCDF = true;
        countSort = false;
        printAll = true;
        done = false;
      } else if (options.substr(0, 3) == "cdf") {
        printCDF = true;
        countSort = false;
        done = false;
      } else if (options.substr(0, 4) == "sum:") {
        std::vector<sort_helper> elements;
        fillVector(elements);
        int32_t begin = elements[0].value;
        int32_t end = elements[elements.size()-1].value;
        try {
          std::string range = options.substr(4);
          size_t dash = range.find("-");
          if (dash == std::string::npos) {
            //No dash
            begin = end = boost::lexical_cast<int>(range);
          } else {
            std::string begin_str = range.substr(0, dash);
            if (begin_str.size() > 0) {
              begin = boost::lexical_cast<int>(begin_str);
            } else {
              // use default begin (first element)
            }
            std::string end_str = range.substr(dash + 1);
            if (end_str.size() > 0) {
              end = boost::lexical_cast<int>(end_str);
            } else {
              // use default end (last element)
            }
          }
        } catch (boost::bad_lexical_cast &) {
          anOstream << "{sum bounds cannot be parsed}";
        } catch (std::out_of_range &) {
          anOstream << "{sum bounds out of range}";
          return;
        }
        int64_t sum = 0;
        for (int32_t i = 0; i < (int)elements.size(); i++) {
          int64_t val = elements[i].value;
          int64_t cnt = elements[i].count;
          if (val >= begin && val <= end) sum += cnt;
        }
        anOstream << sum;
        done = true;
      } else {
        anOstream << "{Unrecognized option: " << options << " }";
      }
    } catch (boost::bad_lexical_cast &) {
      anOstream << "{Unable to parse options: " << options << " }";
    }
    if (done) {
      return;
    }
  }

  //Enroll all the counts and elements in a vector for sorting
  std::vector<sort_helper> elements;

  fillVector(elements);

  if (countSort) {
    std::sort(elements.begin(), elements.end());
  }

  int64_t sum = 0;
  int64_t count = elements.size();

  for(auto& element: elements){
    sum += element.count;
  }

  float sum_f = sum;

  if (calcPercentile) {
    if (pct100) {
      anOstream << elements[elements.size()-1].value << " (100%)";
      return;
    } else {
      float cum_pct = 0.0;
      for (uint32_t i = 0; i < elements.size(); ++i) {
        float pct = static_cast<float>(elements[i].count) / sum_f;
        cum_pct += pct;
        if (cum_pct >= percentile) {
          anOstream << elements[i].value << " (" << (cum_pct * 100.0) << "%)";
          return;
        }
      }
    }
  }

  //Print implementation here
  anOstream << std::endl;
  if (exclude) {
    anOstream << "Excluding bucket " << std::dec << excludeBucket << std::endl;
  }
  if (printCDF) {
    anOstream << "     " << std::setw(10) << "Count" << std::setw(8) << "Cum" << " Value" << std::endl;
  } else {
    anOstream << "     " << std::setw(10) << "Count" << std::setw(8) << "Pct" << " Value" << std::endl;
  }

  if (exclude) {
    for (uint32_t i = 0; i < elements.size(); ++i) {
      if (elements[i].value == excludeBucket) {
        sum -= elements[i].count;
        sum_f = sum;
        count--;
        break;
      }
    }
  }

  int64_t displayed_count = 0;
  int64_t displayed_sum = 0;
  int64_t overall_sum = 0;
  float cdf_pct = 0;
  for(auto& element: elements){
    auto value = element.value;
    auto count = element.count;
    
    if (exclude && (value == excludeBucket)) continue;

    float pct = static_cast<float>(count) / sum_f * 100.0;
    cdf_pct += pct;
    if (!printAll && count <= 1) break;
    ++displayed_count;
    displayed_sum += count;
    overall_sum += count * value;
    anOstream << "      ";
    anOstream << std::left << std::setw(10) << std::setfill(' ') << count;
    if (printCDF) {
      anOstream << std::right << std::setprecision(2) << std::fixed << std::setw(6) << cdf_pct << "% ";
    } else {
      anOstream << std::right << std::setprecision(2) << std::fixed << std::setw(6) << pct << "% ";
    }
    if (valuePrintWidth >= 0) {
      anOstream << std::setw(valuePrintWidth);
    }
    if (valuePrintHex || ((value > 100000) && ((value & 63) == 0))) {
      anOstream << "0x" << std::hex << value << std::dec;
    } else {
      anOstream << value;
    }
    anOstream << std::endl;
  }
  // for (uint32_t i = 0; i < elements.size(); ++i) {
  //   if (exclude && (elements[i].value == excludeBucket)) continue;

  //   if (exclude && (elements[i].value == excludeBucket)) continue;

  //   float pct = static_cast<float>(elements[i].count) / sum_f * 100.0;
  //   cdf_pct += pct;
  //   if (!printAll && elements[i].count <= 1) break;
  //   ++displayed_count;
  //   displayed_sum += elements[i].count;
  //   overall_sum += elements[i].count * elements[i].value;
  //   anOstream << "      ";
  //   anOstream << std::left << std::setw(10) << std::setfill(' ') << elements[i].count;
  //   if (printCDF) {
  //     anOstream << std::right << std::setprecision(2) << std::fixed << std::setw(6) << cdf_pct << "% ";
  //   } else {
  //     anOstream << std::right << std::setprecision(2) << std::fixed << std::setw(6) << pct << "% ";
  //   }
  //   if (valuePrintWidth >= 0) {
  //     anOstream << std::setw(valuePrintWidth);
  //   }
  //   if (valuePrintHex || ((elements[i].value > 100000) && ((elements[i].value & 63) == 0))) {
  //     anOstream << "0x" << std::hex << elements[i].value << std::dec;
  //   } else {
  //     anOstream << elements[i].value;
  //   }
  //   anOstream << std::endl;
  // }
  if ( displayed_count != count ) {
    float pct = static_cast<float>(sum - displayed_sum) / sum_f * 100.0;
    anOstream << "      ";
    anOstream << std::left << std::setw(10) << std::setfill(' ') << sum - displayed_sum;
    anOstream << std::right << std::setprecision(2) << std::fixed << std::setw(6) << pct << "% ";
    anOstream << "in " << count - displayed_count << " undisplayed elements";
    anOstream << std::endl;
  }
  anOstream << "      --------- ------- ----------" << std::endl;
  anOstream << "      ";
  anOstream << std::left << std::setw(10) << std::setfill(' ') << sum;
  anOstream << "100.00% ";
  anOstream << count << " elements";
  anOstream << std::endl;

  anOstream << "     Average value: " << std::left << std::setw(10) << std::setfill(' ')
            << (double)overall_sum / (double)displayed_sum << std::endl;

}

} // end aux_

} // end Stat
} // end Flexus

