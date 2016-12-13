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
#ifndef FLEXUS_CORE_AUX__STATS_VALUE_ARRAYS__HPP__INCLUDED
#define FLEXUS_CORE_AUX__STATS_VALUE_ARRAYS__HPP__INCLUDED

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

class StatValueArray_Counter : public StatValueArrayBase {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueArrayBase>(*this);
    ar & theInitialValue;
    ar & theValues;
  }
  StatValueArray_Counter() {}

public:
  typedef int64_t update_type;
  typedef int64_t value_type;
  typedef StatValue_Counter simple_type;

private:
  value_type theInitialValue;
  std::vector<simple_type> theValues;
public:
  StatValueArray_Counter(value_type aValue)
    : theInitialValue(aValue) {
    theValues.push_back(simple_type(theInitialValue));
  }

  //Used by serialization of other stat types
  StatValueArray_Counter( std::vector<simple_type> const & aValueVector, value_type aCurrentValue)
    : theValues(aValueVector) {
    theValues.push_back(simple_type(aCurrentValue));
  }

  void reduceSum( const StatValueBase & aBase) {
    std::cerr << "reductions not supported (StatValueArray_Counter)" << std::endl;
  }
  void update( update_type anUpdate ) {
    theValues.back().update(anUpdate);
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    for (int32_t i = 0; i < static_cast<int>(theValues.size()) - 1; ++i) {
      anOstream << theValues[i] << ", ";
    }
    anOstream << theValues.back();
  }
  void newValue(accumulation_type aValueType) {
    if (aValueType == accumulation_type::Accumulate) {
      theValues.push_back(theValues.back());
    } else {
      theValues.push_back( simple_type(theInitialValue) );
    }
  }
  void reset( value_type aValue) {
    theValues.clear();
    theValues.push_back(aValue);
  }
  StatValueBase & operator[](int32_t anIndex) {
    return theValues[anIndex];
  }
  std::size_t size() {
    return theValues.size();
  }
};

/*
  class StatValueArray_DoubleCounter : public StatValueArrayBase {
    private:
      friend class boost::serialization::access;
      template<class Archive>
      void serialize(Archive &ar, uint32_t version) {
          ar & boost::serialization::base_object<StatValueArrayBase>(*this);
          ar & theInitialValue;
          ar & theValues;
      }
      StatValueArray_DoubleCounter() {}

    public:
      typedef double update_type;
      typedef double value_type;
      typedef StatValue_DoubleCounter simple_type;

    private:
      value_type theInitialValue;
      std::vector<simple_type> theValues;
    public:
      StatValueArray_DoubleCounter(value_type aValue)
        : theInitialValue(aValue)
      {
        theValues.push_back(simple_type(theInitialValue));
      }

      //Used by serialization of other stat types
      StatValueArray_DoubleCounter( std::vector<simple_type> const & aValueVector, value_type aCurrentValue)
        : theValues(aValueVector)
      {
        theValues.push_back(simple_type(aCurrentValue));
      }

      void reduceSum( const StatValueBase & aBase) {
        std::cerr << "reductions not supported (StatValueArray_Counter)" << std::endl;
      }
      void update( update_type anUpdate ) {
        theValues.back().update(anUpdate);
      }
      void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
        for (int32_t i = 0; i < static_cast<int>(theValues.size()) - 1; ++i) {
          anOstream << theValues[i] << ", ";
        }
        anOstream << theValues.back();
      }
      void newValue(accumulation_type aValueType) {
        if (aValueType == accumulation_type::Accumulate) {
          theValues.push_back(theValues.back());
        } else {
          theValues.push_back( simple_type(theInitialValue) );
        }
      }
      void reset( value_type aValue) {
        theValues.clear();
        theValues.push_back(aValue);
      }
      StatValueBase & operator[](int32_t anIndex) { return theValues[anIndex]; }
      std::size_t size() { return theValues.size(); }
  };
*/

class StatValueArray_Annotation : public StatValueArrayBase {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueArrayBase>(*this);
    ar & theInitialValue;
    ar & theValues;
  }
  StatValueArray_Annotation() {}

public:
  typedef std::string update_type;
  typedef std::string value_type;
  typedef StatValue_Annotation simple_type;

private:
  value_type theInitialValue;
  std::vector<simple_type> theValues;
public:
  StatValueArray_Annotation(value_type aValue)
    : theInitialValue(aValue) {
    theValues.push_back(simple_type(theInitialValue));
  }

  //Used by serialization of other stat types
  StatValueArray_Annotation( std::vector<simple_type> const & aValueVector, value_type aCurrentValue)
    : theValues(aValueVector) {
    theValues.push_back(simple_type(aCurrentValue));
  }

  void reduceSum( const StatValueBase & aBase) {
    std::cerr << "reductions not supported (StatValueArray_Annotation)" << std::endl;
  }
  void update( update_type anUpdate ) {
    theValues.back().update(anUpdate);
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    for (int32_t i = 0; i < static_cast<int>(theValues.size()) - 1; ++i) {
      anOstream << theValues[i] << ", ";
    }
    anOstream << theValues.back();
  }
  void newValue(accumulation_type aValueType) {
    if (aValueType == accumulation_type::Accumulate) {
      theValues.push_back(theValues.back());
    } else {
      theValues.push_back( simple_type(theInitialValue) );
    }
  }
  void reset( value_type aValue) {
    theValues.clear();
    theValues.push_back(aValue);
  }
  StatValueBase & operator[](int32_t anIndex) {
    return theValues[anIndex];
  }
  std::size_t size() {
    return theValues.size();
  }
};

class StatValueArray_Max : public StatValueArrayBase {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueArrayBase>(*this);
    ar & theValues;
  }
  StatValueArray_Max() {}

public:
  typedef int64_t update_type;
  typedef int64_t value_type;
  typedef StatValue_Max simple_type;

private:
  std::vector<simple_type> theValues;
public:
  StatValueArray_Max(value_type /*ignored*/) {
    theValues.push_back(simple_type(0));
  }
  void reduceSum( const StatValueBase & aBase) {
    std::cerr << "reductions not supported (StatValueArray_Max)" << std::endl;
  }
  void update( update_type anUpdate ) {
    theValues.back().update(anUpdate);
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    for (int32_t i = 0; i < static_cast<int>(theValues.size()) - 1; ++i) {
      anOstream << theValues[i] << ", ";
    }
    anOstream << theValues.back();
  }
  void newValue(accumulation_type aValueType) {
    if (aValueType == accumulation_type::Accumulate) {
      theValues.push_back(theValues.back());
    } else {
      theValues.push_back( simple_type(0) );
    }
  }
  void reset( value_type /*ignored*/) {
    theValues.clear();
    theValues.push_back( simple_type(0) );
  }
  StatValueBase & operator[](int32_t anIndex) {
    return theValues[anIndex];
  }
  std::size_t size() {
    return theValues.size();
  }
};

class StatValueArray_Average : public StatValueArrayBase {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueArrayBase>(*this);
    ar & theValues;
  }
  StatValueArray_Average() {}

public:
  typedef std::pair< int64_t, int64_t> update_type;
  typedef int64_t value_type;
  typedef StatValue_Average simple_type;

private:
  std::vector<simple_type> theValues;
public:
  StatValueArray_Average(value_type /*ignored*/) {
    theValues.push_back(simple_type(0));
  }
  void reduceSum( const StatValueBase & aBase) {
    std::cerr << "reductions not supported (StatValueArray_Average)" << std::endl;
  }
  void update( update_type anUpdate ) {
    theValues.back().update(anUpdate);
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    for (int32_t i = 0; i < static_cast<int>(theValues.size()) - 1; ++i) {
      anOstream << theValues[i] << ", ";
    }
    anOstream << theValues.back();
  }
  void newValue(accumulation_type aValueType) {
    if (aValueType == accumulation_type::Accumulate) {
      theValues.push_back(theValues.back());
    } else {
      theValues.push_back( simple_type(0) );
    }
  }
  void reset( value_type /*ignored*/) {
    theValues.clear();
    theValues.push_back( simple_type(0) );
  }
  StatValueBase & operator[](int32_t anIndex) {
    return theValues[anIndex];
  }
  std::size_t size() {
    return theValues.size();
  }
};

class StatValueArray_StdDev : public StatValueArrayBase {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueArrayBase>(*this);
    ar & theValues;
  }
  StatValueArray_StdDev() {}

public:
  typedef int64_t update_type;
  typedef double value_type;
  typedef StatValue_StdDev simple_type;

private:
  std::vector<simple_type> theValues;
public:
  StatValueArray_StdDev(value_type /*ignored*/) {
    theValues.push_back(simple_type(0));
  }
  void reduceSum( const StatValueBase & aBase) {
    std::cerr << "reductions not supported (StatValueArray_StdDev)" << std::endl;
  }
  void update( update_type anUpdate ) {
    theValues.back().update(anUpdate);
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    for (int32_t i = 0; i < static_cast<int>(theValues.size()) - 1; ++i) {
      anOstream << theValues[i] << ", ";
    }
    anOstream << theValues.back();
  }
  void newValue(accumulation_type aValueType) {
    if (aValueType == accumulation_type::Accumulate) {
      theValues.push_back(theValues.back());
    } else {
      theValues.push_back( simple_type(0) );
    }
  }
  void reset( value_type /*ignored*/) {
    theValues.clear();
    theValues.push_back( simple_type(0) );
  }
  StatValueBase & operator[](int32_t anIndex) {
    return theValues[anIndex];
  }
  std::size_t size() {
    return theValues.size();
  }
};

} // end aux_
} // end Stat
} // end Flexus

#endif //FLEXUS_CORE_AUX__STATS_VALUE_ARRAYS__HPP__INCLUDED
