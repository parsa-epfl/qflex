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
#ifndef FLEXUS_CORE_AUX__STATS_VALUES__HPP__INCLUDED
#define FLEXUS_CORE_AUX__STATS_VALUES__HPP__INCLUDED

#include <string>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <numeric>
#include <functional>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/export.hpp>
#include <core/boost_extensions/intrusive_ptr.hpp>
#include <core/boost_extensions/lexical_cast.hpp>

namespace Flexus {
namespace Stat {
namespace aux_ {

class StatValue_AvgAccumulator;
class StatValue_StdDevAccumulator;

class StatValue_Counter : public StatValueBase {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueBase>(*this);
    ar & theValue;
  }
public:
  StatValue_Counter() {}

  friend class StatValue_AvgAccumulator;
  friend class StatValue_StdDevAccumulator;
public:
  typedef int64_t update_type;
  typedef int64_t value_type;
private:
  value_type theValue;
public:
  StatValue_Counter(value_type aValue)
    : theValue(aValue)
  {}

  void reduceSum( StatValueBase const & aBase) {
    StatValue_Counter const & ctr = dynamic_cast<StatValue_Counter const &>(aBase);
    reduceSum(ctr);
  }
  void reduceSum( StatValue_Counter const & aCounter) {
    theValue += aCounter.theValue;
  }

  boost::intrusive_ptr<StatValueBase> sumAccumulator() {
    return new StatValue_Counter(*this);
  }
  boost::intrusive_ptr<StatValueBase> avgAccumulator();
  boost::intrusive_ptr<StatValueBase> stdevAccumulator();

  void update( update_type anUpdate ) {
    theValue += anUpdate;
  }
  void reset( value_type aValue) {
    theValue = aValue;
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    anOstream << theValue;
  }
  int64_t asLongLong() const {
    return theValue;
  };
};

/*
  class StatValue_DoubleCounter : public StatValueBase {
    private:
      friend class boost::serialization::access;
      template<class Archive>
      void serialize(Archive &ar, uint32_t version) {
          ar & boost::serialization::base_object<StatValueBase>(*this);
          ar & theValue;
      }
      StatValue_DoubleCounter() {}

      friend class StatValue_AvgAccumulator;
      friend class StatValue_StdDevAccumulator;
    public:
      typedef double update_type;
      typedef double value_type;
    private:
      value_type theValue;
    public:
      StatValue_DoubleCounter(value_type aValue)
        : theValue(aValue)
        {}

      void reduceSum( StatValueBase const & aBase) {
        StatValue_DoubleCounter const & ctr = dynamic_cast<StatValue_DoubleCounter const &>(aBase);
        reduceSum(ctr);
      }
      void reduceSum( StatValue_DoubleCounter const & aCounter) {
        theValue += aCounter.theValue;
      }

      boost::intrusive_ptr<StatValueBase> sumAccumulator() { return new StatValue_DoubleCounter(*this); }
      boost::intrusive_ptr<StatValueBase> avgAccumulator();
      boost::intrusive_ptr<StatValueBase> stdevAccumulator();

      void update( update_type anUpdate ) {
        theValue += anUpdate;
      }
      void reset( value_type aValue) {
        theValue = aValue;
      }
      void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
        anOstream << theValue;
      }
      int64_t asLongLong() const { return static_cast<int64_t>( theValue ); };
  };
*/

class StatValue_PredictionCounter : public StatValueBase {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueBase>(*this);
    ar & theValue;
    ar & thePending;
  }
  StatValue_PredictionCounter() {}

public:
  typedef Prediction & update_type;
  typedef int64_t value_type;
private:
  value_type theValue;
  value_type thePending;
public:
  StatValue_PredictionCounter(value_type aValue)
    : theValue(aValue)
  {}
  boost::intrusive_ptr<StatValueBase> sumAccumulator() {
    return new StatValue_PredictionCounter(*this);
  }
  void reduceSum( StatValueBase const & aBase) {
    const StatValue_PredictionCounter & ptr = dynamic_cast<const StatValue_PredictionCounter &>(aBase);
    reduceSum(ptr);
  }
  void reduceSum( const StatValue_PredictionCounter & aCounter) {
    theValue += aCounter.theValue;
    thePending += aCounter.thePending;
  }
  void update( update_type anUpdate ) {
    anUpdate.connectCounter(this);
    thePending++;
  }
  void confirm( value_type anUpdate ) {
    theValue += anUpdate;
    thePending--;
  }
  void dismiss( ) {
    thePending--;
  }
  void reset( value_type aValue) {
    theValue = aValue;
  }
  void guess( value_type anUpdate ) {
    theValue += anUpdate;
  }
  void goodGuess( ) {
    thePending--;
  }
  void reject( value_type anUpdate ) {
    theValue -= anUpdate;
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    if (options == "pending") {
      anOstream << thePending;
    }
    anOstream << theValue;
  }
  int64_t asLongLong() const {
    return theValue;
  };
};

class StatValue_Annotation : public StatValueBase {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueBase>(*this);
    ar & theValue;
  }
  StatValue_Annotation() {}

public:
  typedef std::string update_type;
  typedef std::string value_type;
private:
  value_type theValue;
public:
  StatValue_Annotation(value_type aValue)
    : theValue(aValue)
  {}

  void reduceSum( StatValueBase const & aBase) {
    const StatValue_Annotation & ptr = dynamic_cast<const StatValue_Annotation &>(aBase);
    reduceSum(ptr);
  }
  void reduceAvg( StatValueBase const & aBase) {
    const StatValue_Annotation & ptr = dynamic_cast<const StatValue_Annotation &>(aBase);
    reduceSum(ptr); //Same as sum for annotations
  }
  void reduceSum( const StatValue_Annotation & other) {
    if (other.theValue.length() > theValue.length()) {
      theValue = other.theValue;
    }
  }

  boost::intrusive_ptr<StatValueBase> sumAccumulator() {
    return new StatValue_Annotation(*this);
  }
  boost::intrusive_ptr<StatValueBase> avgAccumulator() {
    return new StatValue_Annotation(*this);
  }

  void update( update_type anUpdate ) {
    theValue = anUpdate;
  }
  void reset( value_type aValue) {
    theValue = aValue;
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    anOstream << theValue;
  }
  int64_t asLongLong() const {
    return 0;
  };
};

class StatValue_Max : public StatValueBase {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueBase>(*this);
    ar & theValue;
    ar & theIsValid;
  }
public:
  StatValue_Max() {}

public:
  typedef int64_t update_type;
  typedef int64_t value_type;
private:
  value_type theValue;
  bool theIsValid;
public:
  StatValue_Max(value_type /*ignored*/)
    : theIsValid(false)
  {}
  void reduceSum( const StatValueBase & aBase) {
    const StatValue_Max & ptr = dynamic_cast<const StatValue_Max &>(aBase);
    reduceSum(ptr);
  }
  void reduceSum( const StatValue_Max & aMax ) {
    if (! aMax.theIsValid) {
      return;
    }
    if (! theIsValid) {
      theIsValid = true;
      theValue = aMax.theValue;
      return;
    }
    if (aMax.theValue > theValue) {
      theValue = aMax.theValue;
    }
  }

  boost::intrusive_ptr<StatValueBase> sumAccumulator() {
    return new StatValue_Max(*this);
  }

  void update( update_type anUpdate ) {
    if (! theIsValid) {
      theIsValid = true;
      theValue = anUpdate;
      return;
    }
    if (anUpdate > theValue) {
      theValue = anUpdate;
    }
  }
  void reset( value_type /*ignored*/) {
    theIsValid = false;
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    if (theIsValid) {
      anOstream << theValue;
    } else {
      anOstream << "(none)";
    }
  }
  int64_t asLongLong() const {
    if (theIsValid) {
      return theValue;
    } else {
      return 0;
    }
  };
};

class StatValue_Average: public StatValueBase {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueBase>(*this);
    ar & theTotal;
    ar & theCount;
  }
public:
  StatValue_Average() {}

  friend class StatValue_AvgAccumulator;
  friend class StatValue_StdDevAccumulator;
public:
  typedef std::pair<int64_t, int64_t> update_type;
  typedef int64_t value_type;
private:
  value_type theTotal;
  value_type theCount;
public:
  StatValue_Average(value_type /*ignored*/)
    : theTotal(0)
    , theCount(0)
  {}

  void reduceSum( const StatValueBase & aBase) {
    const StatValue_Average & ptr = dynamic_cast<const StatValue_Average &>(aBase);
    reduceSum(ptr);
  }
  void reduceSum( const StatValue_Average & anAverage ) {
    theTotal += anAverage.theTotal;
    theCount += anAverage.theCount;
  }
  boost::intrusive_ptr<StatValueBase> sumAccumulator() {
    return new StatValue_Average(*this);
  }
  boost::intrusive_ptr<StatValueBase> avgAccumulator();
  boost::intrusive_ptr<StatValueBase> stdevAccumulator();

  void update( update_type anUpdate ) {
    theTotal += anUpdate.first * anUpdate.second;
    theCount += anUpdate.second;
  }
  void reset( value_type /*ignored*/) {
    theTotal = 0;
    theCount = 0;
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    if (theCount > 0) {
      anOstream << static_cast<double>(theTotal) / theCount;
    } else {
      anOstream << "{nan}";
    }
  }
  int64_t asLongLong() const {
    if (theCount > 0) {
      return theTotal / theCount;
    } else {
      return 0;
    }
  };
  double asDouble() const {
    if (theCount > 0) {
      return static_cast<double>(theTotal) / theCount;
    } else {
      return 0;
    }
  };
};

//Despite the fact that this implementation is wacked and wastes storage,
//we will keep it for compatability with previous stat files.
//A better implementation is available at:
//http://www.answers.com/topic/algorithms-for-calculating-variance
class StatValue_StdDev : public StatValueBase {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueBase>(*this);
    ar & k;
    ar & Sum;
    ar & SumSq;
    ar & SigmaSqSum;
  }
public:
  StatValue_StdDev() {}

public:
  typedef int64_t update_type;
  typedef double value_type;
private:
  uint64_t k;
  double Sum;
  double SumSq;
  double SigmaSqSum;

public:
  StatValue_StdDev(value_type /*ignored*/)
    : k(0)
    , Sum(0.0)
    , SumSq(0.0)
    , SigmaSqSum(0.0)
  {}
  void reduceSum( const StatValueBase & aBase) {
    const StatValue_StdDev & ptr = dynamic_cast<const StatValue_StdDev &>(aBase);
    reduceSum(ptr);
  }
  void reduceSum( const StatValue_StdDev & aStdDev ) {
    //First, compute the variance that results from summing the two
    Sum = Sum + aStdDev.Sum;
    SumSq = SumSq + aStdDev.SumSq;
    k = k + aStdDev.k;

    double variance = SumSq / k - ( Sum / k ) * ( Sum / k );
    SigmaSqSum = variance * k * k;
  }

  boost::intrusive_ptr<StatValueBase> sumAccumulator() {
    return new StatValue_StdDev(*this);
  }

  void update( update_type anUpdate ) {
    const double Xsq = anUpdate * anUpdate;

    SigmaSqSum += SumSq + k * Xsq - 2 * anUpdate * Sum;

    k++;
    Sum += anUpdate;
    SumSq += Xsq;
  }
  void reset( value_type /*ignored*/) {
    k = 0;
    Sum = 0;
    SumSq = 0;
    SigmaSqSum = 0;
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    if (k == 0) {
      anOstream << "{nan}";
    } else {
      anOstream << std::sqrt(SigmaSqSum) / static_cast<double>(k);
    }
  }
  int64_t asLongLong() const {
    if (k == 0) {
      return 0;
    } else {
      return static_cast<int64_t>( sqrt(SigmaSqSum) / static_cast<double>(k) );
    }
  }
  double asDouble() const {
    if (k == 0) {
      return 0;
    } else {
      return sqrt(SigmaSqSum) / static_cast<double>(k);
    }
  }
};

} // end aux_
} // end Stat
} // end Flexus

#endif //FLEXUS_CORE_AUX__STATS_VALUES__HPP__INCLUDED
