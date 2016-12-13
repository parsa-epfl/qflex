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
#ifndef FLEXUS_CORE_AUX__STATS_HISTOGRAMS__HPP__INCLUDED
#define FLEXUS_CORE_AUX__STATS_HISTOGRAMS__HPP__INCLUDED

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

struct HistogramPrint {
  virtual ~HistogramPrint() {};
  virtual int32_t buckets() const = 0;
  virtual int64_t bucketVal(int32_t i) const = 0;
  virtual int64_t sum() const = 0;

  void doPrint(std::ostream & anOstream, std::string const & options = std::string("")) const;
};

struct InstanceCounterPrint {
  struct sort_helper {
    int64_t value;
    int64_t count;
    sort_helper() {}
    sort_helper(int64_t v, int64_t c)
      : value(v)
      , count(c)
    {}
    bool operator < (sort_helper const & other) const {
      return count > other.count;
    }
  };

  virtual ~InstanceCounterPrint() {};
  virtual void fillVector(std::vector<sort_helper> & aVector) const = 0;
  virtual int64_t count(int64_t aKey) const = 0;
  virtual int64_t sum() const = 0;
  virtual int64_t weightedSum() const = 0;
  virtual int64_t buckets() const = 0;
  void doPrint(std::ostream & anOstream, std::string const & options = std::string("")) const;
};

class StatValue_Log2Histogram : public StatValueBase, private HistogramPrint {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueBase>(*this);
    ar & theBuckets;
  }
  StatValue_Log2Histogram() {}

public:
  typedef int64_t update_type;
  typedef int64_t value_type;
  std::vector<value_type> theBuckets;
private:
  int64_t log2(int64_t aVal) const {
    uint32_t ii = 0;
    while (aVal > 1) {
      ii++;
      aVal >>= 1;
    }
    return ii;
  }
public:
  StatValue_Log2Histogram(value_type /*ignored*/)
  {}
  void reduceSum( const StatValueBase & aBase) {
    const StatValue_Log2Histogram & ptr = dynamic_cast<const StatValue_Log2Histogram &>(aBase);
    reduceSum(ptr);
  }
  void reduceSum( const StatValue_Log2Histogram & aHistogram ) {
    if (theBuckets.size() < aHistogram.theBuckets.size()) {
      theBuckets.resize(aHistogram.theBuckets.size(), 0);
    }
    for (int32_t i = 0; i < static_cast<int>(aHistogram.theBuckets.size()); ++i) {
      theBuckets[i] += aHistogram.theBuckets[i];
    }
  }
  boost::intrusive_ptr<StatValueBase> sumAccumulator() {
    return new StatValue_Log2Histogram(*this);
  }
  void update( update_type anUpdate ) {
    if (anUpdate == 0) {
      if (theBuckets.size() == 0) {
        theBuckets.resize(1, 0);
      }
      theBuckets[0]++;
    } else {
      value_type log = log2(anUpdate);
      if (theBuckets.size() < unsigned(log + 2)) {
        theBuckets.resize(log + 2, 0);
      }
      theBuckets[log+1]++;
    }
  }
  void reset( value_type /*ignored*/) {
    theBuckets.clear();
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    doPrint(anOstream, options);
  }
  int32_t buckets() const {
    return theBuckets.size();
  }
  virtual int64_t bucketVal(int32_t i) const {
    return theBuckets[i];
  }
  virtual int64_t sum() const {
    int64_t ret = 0;
    for (int32_t i = 0; i < static_cast<int>(theBuckets.size()); ++i) {
      ret += theBuckets[i];
    }
    return ret;
  }
  virtual int64_t asLongLong(std::string const & options) const {
    if (options.size() > 0) {
      if (options == "buckets") {
        return buckets();
      } else if (options.substr(0, 4) == "val:") {
        int64_t name = boost::lexical_cast<int64_t>(options.substr(4));
        int64_t key = 0;
        if (name != 0) {
          key = (int64_t)log2(name);
          if ((1 << key) != name) {
            throw CalcException(std::string("{bucket must be a power of two}"));
          }
          key++;
        }
        return bucketVal(key);
      } else if (options.substr(0, 5) == "count") {
        return sum();
      } else {
        throw CalcException(std::string("{Unable to parse options: " + options + " }"));
      }
    }
    return 0;
  }
};

class StatValue_WeightedLog2Histogram : public StatValueBase, private HistogramPrint  {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueBase>(*this);
    ar & theBuckets;
  }
  StatValue_WeightedLog2Histogram() {}

public:
  typedef std::pair<int64_t, int64_t> update_type;
  typedef int64_t value_type;
  std::vector<value_type> theBuckets;
private:
  int64_t log2(int64_t aVal) const {
    uint32_t ii = 0;
    while (aVal > 1) {
      ii++;
      aVal >>= 1;
    }
    return ii;
  }
public:
  StatValue_WeightedLog2Histogram(value_type /*ignored*/)
  {}
  void reduceSum( const StatValueBase & aBase) {
    const StatValue_WeightedLog2Histogram & ptr = dynamic_cast<const StatValue_WeightedLog2Histogram &>(aBase);
    reduceSum(ptr);
  }
  void reduceSum( const StatValue_WeightedLog2Histogram & aHistogram ) {
    if (theBuckets.size() < aHistogram.theBuckets.size()) {
      theBuckets.resize(aHistogram.theBuckets.size(), 0);
    }
    for (int32_t i = 0; i < static_cast<int>(aHistogram.theBuckets.size()); ++i) {
      theBuckets[i] += aHistogram.theBuckets[i];
    }
  }
  boost::intrusive_ptr<StatValueBase> sumAccumulator() {
    return new StatValue_WeightedLog2Histogram(*this);
  }

  void update( update_type anUpdate ) {
    if (anUpdate.first == 0) {
      if (theBuckets.size() == 0) {
        theBuckets.resize(1, 0);
      }
      theBuckets[0] += anUpdate.second;
    } else {
      value_type log = log2(anUpdate.first);
      if (theBuckets.size() < unsigned(log + 2)) {
        theBuckets.resize(log + 2, 0);
      }
      theBuckets[log+1] += anUpdate.second;
    }
  }
  void reset( value_type /*ignored*/) {
    theBuckets.clear();
  }
  int32_t buckets() const {
    return theBuckets.size();
  }
  virtual int64_t bucketVal(int32_t i) const {
    return theBuckets[i];
  }
  virtual int64_t sum() const {
    int64_t ret = 0;
    for (int32_t i = 0; i < static_cast<int>(theBuckets.size()); ++i) {
      ret += theBuckets[i];
    }
    return ret;
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    doPrint(anOstream, options);
  }
  int64_t asLongLong(std::string const & options) const {
    if (options.size() > 0) {
      if (options == "buckets") {
        return buckets();
      } else if (options.substr(0, 4) == "val:") {
        int64_t name = boost::lexical_cast<int64_t>(options.substr(4));
        int64_t key = 0;
        if (name != 0) {
          key = (int64_t)log2(name);
          if ((1 << key) != name) {
            throw CalcException(std::string("{bucket must be a power of two}"));
          }
          key++;
        }
        return bucketVal(key);
      } else if (options.substr(0, 5) == "count") {
        return sum();
      } else {
        throw CalcException(std::string("{Unable to parse options: " + options + " }"));
      }
    }
    return 0;
  }
};

class StatValue_StdDevLog2Histogram : public StatValueBase, private HistogramPrint  {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueBase>(*this);
    ar & theBuckets;
    ar & theBucketCounts;
  }
  StatValue_StdDevLog2Histogram() {}

public:
  typedef std::pair < int64_t /*mean*/, int64_t /*stdd*/ > update_type;
  typedef int64_t value_type;
  std::vector<value_type> theBuckets;
  std::vector<value_type> theBucketCounts;
private:
  int64_t log2(int64_t aVal) const {
    uint32_t ii = 0;
    while (aVal > 1) {
      ii++;
      aVal >>= 1;
    }
    return ii;
  }
  int64_t calculate(int64_t oldStdDev, int64_t oldPopulation, int64_t newStdDev, int64_t newPopulation) {
    return static_cast<int64_t>(sqrt(  (oldPopulation * oldStdDev * oldStdDev + newPopulation * newStdDev * newStdDev)
                                       / static_cast<double>(oldPopulation + newPopulation)));
  }
public:
  StatValue_StdDevLog2Histogram(value_type /*ignored*/)
  {}
  void collapse( const StatValueBase * aBase) {
    const StatValue_StdDevLog2Histogram * ptr = dynamic_cast<const StatValue_StdDevLog2Histogram *>(aBase);
    if (ptr) {
      collapse(*ptr);
    } else {
      std::cerr << "Collapse cannot cast (StatValue_StdDevLog2Histogram)" << std::endl;
    }
  }
  void collapse( const StatValue_StdDevLog2Histogram & aHistogram ) {
    if (theBuckets.size() < aHistogram.theBuckets.size()) {
      theBuckets.resize(aHistogram.theBuckets.size(), 0);
      theBucketCounts.resize(aHistogram.theBucketCounts.size(), 0);
    }
    for (int32_t i = 0; i < static_cast<int>(aHistogram.theBuckets.size()); ++i) {
      theBuckets[i] = calculate(theBuckets[i], theBucketCounts[i], aHistogram.theBuckets[i], aHistogram.theBucketCounts[i]);
      theBucketCounts[i] += aHistogram.theBucketCounts[i];
    }
  }
  void update( update_type anUpdate ) {
    if (anUpdate.first == 0) {
      if (theBuckets.size() == 0) {
        theBuckets.resize(1, 0);
        theBucketCounts.resize(1, 0);
      }
      theBuckets[0] = calculate(theBuckets[0], theBucketCounts[0], anUpdate.second, 1);
      theBucketCounts[0] ++;
    } else {
      value_type log = log2(anUpdate.first);
      if (theBuckets.size() < unsigned(log + 2)) {
        theBuckets.resize(log + 2, 0);
        theBucketCounts.resize(log + 2, 0);
      }
      theBuckets[log+1] = calculate(theBuckets[log+1], theBucketCounts[log+1], anUpdate.second, 1);
      theBucketCounts[log+1] ++;
    }
  }
  void reset( value_type /*ignored*/) {
    theBuckets.clear();
    theBucketCounts.clear();
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    doPrint(anOstream, options);
  }
  int32_t buckets() const {
    return theBuckets.size();
  }
  virtual int64_t bucketVal(int32_t i) const {
    return theBuckets[i];
  }
  virtual int64_t sum() const {
    int64_t ret = 0;
    for (int32_t i = 0; i < static_cast<int>(theBuckets.size()); ++i) {
      ret += theBuckets[i];
    }
    return ret;
  }
  int64_t asLongLong(std::string const & options) const {
    if (options.size() > 0) {
      if (options == "buckets") {
        return buckets();
      } else if (options.substr(0, 4) == "val:") {
        int64_t name = boost::lexical_cast<int64_t>(options.substr(4));
        int64_t key = 0;
        if (name != 0) {
          key = (int64_t)log2(name);
          if ((1 << key) != name) {
            throw CalcException(std::string("{bucket must be a power of two}"));
          }
          key++;
        }
        return bucketVal(key);
      } else if (options.substr(0, 5) == "count") {
        return sum();
      } else {
        throw CalcException(std::string("{Unable to parse options: " + options + " }"));
      }
    }
    return 0;
  }
};

template <class CountedValueType>
class StatValue_UniqueCounter : public StatValueBase {

public:
  virtual boost::intrusive_ptr<const StatValueBase> serialForm() const {
    boost::intrusive_ptr<const StatValueBase> ret_val( new StatValue_Counter(theSet.size()));
    return ret_val;
  };

public:
  typedef CountedValueType update_type;
  typedef int64_t value_type;
private:
  std::set<CountedValueType> theSet;
public:
  StatValue_UniqueCounter()
  {}
  void reduceSum( const StatValueBase & aBase) {
    const StatValue_UniqueCounter & ptr = dynamic_cast<const StatValue_UniqueCounter &>(aBase);
    reduceSum(ptr);
  }
  void reduceSum( const StatValue_UniqueCounter & anUnique ) {
    theSet.insert( anUnique.theSet.begin(), anUnique.theSet.end() );
  }
  boost::intrusive_ptr<StatValueBase> sumAccumulator() {
    return new StatValue_UniqueCounter(*this);
  }

  void update( update_type anUpdate ) {
    theSet.insert( anUpdate );
  }
  void reset( value_type /*ignored*/) {
    theSet.clear();
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    anOstream << theSet.size();
  }
  int64_t asLongLong() const {
    return theSet.size();
  };
};

template <>
class StatValue_UniqueCounter<uint32_t> : public StatValueBase {

public:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueBase>(*this);
    ar & theSet;
  }

  virtual boost::intrusive_ptr<StatValueBase> serialForm() {
    return this;
  }

public:
  typedef uint32_t update_type;
  typedef int64_t value_type;
private:
  std::set<uint32_t> theSet;
public:
  StatValue_UniqueCounter()
  {}
  void reduceSum( const StatValueBase & aBase) {
    const StatValue_UniqueCounter & ptr = dynamic_cast<const StatValue_UniqueCounter &>(aBase);
    reduceSum(ptr);
  }
  void reduceSum( const StatValue_UniqueCounter & anUnique ) {
    theSet.insert( anUnique.theSet.begin(), anUnique.theSet.end() );
  }
  boost::intrusive_ptr<StatValueBase> sumAccumulator() {
    return new StatValue_UniqueCounter(*this);
  }

  void update( update_type anUpdate ) {
    theSet.insert( anUpdate );
  }
  void reset( value_type /*ignored*/) {
    theSet.clear();
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    anOstream << theSet.size();
  }
  int64_t asLongLong() const {
    return theSet.size();
  };
};

template <class CountedValueType>
class StatValueArray_UniqueCounter : public StatValueArrayBase {

public:
  virtual boost::intrusive_ptr<const StatValueBase> serialForm() const {
    boost::intrusive_ptr<const StatValueBase> ret_val( new StatValueArray_Counter(theValues, theSet.size()));
    return ret_val;
  };

public:
  typedef CountedValueType update_type;
  typedef int64_t value_type;
  typedef StatValue_Counter simple_type;

private:
  std::vector<simple_type> theValues;
  std::set<CountedValueType> theSet;
public:
  StatValueArray_UniqueCounter() {}
  void reduceSum( const StatValueBase & aBase) {
    const StatValueArray_UniqueCounter & ptr = dynamic_cast<const StatValueArray_UniqueCounter &>(aBase);
    reduceSum(ptr);
  }
  void reduceSum( const StatValueArray_UniqueCounter & anUniqueArray ) {
    for (int32_t i = 0; i < static_cast<int>(anUniqueArray.theValues.size()); ++i) {
      theValues[i].reduceSum( anUniqueArray.theValues[i] );
    }
    theSet.insert( anUniqueArray.theSet.begin(), anUniqueArray.theSet.end() );
  }
  boost::intrusive_ptr<StatValueBase> sumAccumulator() {
    return new StatValueArray_UniqueCounter(*this);
  }
  void update( update_type anUpdate ) {
    theSet.insert(anUpdate);
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    for (int32_t i = 0; i < static_cast<int>(theValues.size()); ++i) {
      anOstream << theValues[i] << ", ";
    }
    anOstream << theSet.size();
  }
  void newValue(accumulation_type aValueType) {
    theValues.push_back( simple_type(theSet.size()) ) ;
    if (aValueType == accumulation_type::Reset) {
      theSet.clear();
    }
  }
  void reset( value_type /*ignored*/ ) {
    theSet.clear();
  }
  StatValueBase & operator[](int32_t anIndex) {
    return theValues[anIndex];
  }
  std::size_t size() {
    return theValues.size();
  }
};

template <class CountedValueType>
class StatValue_InstanceCounter;

template <>
class StatValue_InstanceCounter<std::string> : public StatValueBase, private InstanceCounterPrint   {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueBase>(*this);
    ar & theMap;
  }

  virtual boost::intrusive_ptr<const StatValueBase> serialForm() const {
    return this;
  }

public:
  typedef std::pair<std::string, int> update_type;
  typedef int64_t value_type;
private:
  typedef std::map<std::string, int64_t> instance_map;
  instance_map theMap;

public:
  StatValue_InstanceCounter()
  {}
  void reduceSum( const StatValueBase & aBase) {
    const StatValue_InstanceCounter & ptr = dynamic_cast<const StatValue_InstanceCounter &>(aBase);
    reduceSum(ptr);
  }
  void reduceSum( const StatValue_InstanceCounter & anInstance ) {
    instance_map::const_iterator iter = anInstance.theMap.begin();
    instance_map::const_iterator end = anInstance.theMap.end();
    while (iter != end) {
      theMap[iter->first] += iter->second;
      ++iter;
    }
  }
  boost::intrusive_ptr<StatValueBase> sumAccumulator() {
    return new StatValue_InstanceCounter(*this);
  }
  void update( update_type anUpdate ) {
    theMap[anUpdate.first] += anUpdate.second;
  }
  void set( std::string anUpdate, value_type aValue ) {
    theMap[anUpdate] = aValue;
  }
  void reset( value_type /*ignored*/) {
    theMap.clear();
  }
  void fillVector(std::vector<sort_helper> & aVector) const {
    instance_map::const_iterator iter = theMap.begin();
    instance_map::const_iterator end = theMap.end();
    while (iter != end) {
      aVector.push_back( sort_helper(boost::lexical_cast<int64_t>(iter->first), iter->second));
      ++iter;
    }
  }
  int64_t buckets() const {
    return theMap.size();
  }
  int64_t count(int64_t aKey) const {
    std::string key = std::to_string( aKey );
    instance_map::const_iterator iter = theMap.find(key);
    if (iter != theMap.end()) {
      return iter->second;
    } else {
      return 0;
    }
  }
  int64_t sum() const {
    int64_t ret_val = 0;
    instance_map::const_iterator iter = theMap.begin();
    instance_map::const_iterator end = theMap.end();
    while (iter != end) {
      ret_val += iter->second;
      ++iter;
    }
    return ret_val;
  }
  int64_t weightedSum() const {
    int64_t ret_val = 0;
    instance_map::const_iterator iter = theMap.begin();
    instance_map::const_iterator end = theMap.end();
    while (iter != end) {
      int64_t weight = boost::lexical_cast<int64_t >( iter->first );
      ret_val += weight * iter->second;
      ++iter;
    }
    return ret_val;
  }

  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    doPrint(anOstream, options);
  }
};

template <>
class StatValue_InstanceCounter<int64_t> : public StatValueBase, private InstanceCounterPrint   {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueBase>(*this);
    ar & theMap;
  }

  virtual boost::intrusive_ptr<const StatValueBase> serialForm() const {
    return this;
  }

public:
  typedef std::pair<int64_t, int> update_type;
  typedef int64_t value_type;

  typedef std::map<int64_t, int64_t> instance_map;
  instance_map theMap;

public:
  StatValue_InstanceCounter()
  {}
  void reduceSum( const StatValueBase & aBase) {
    const StatValue_InstanceCounter & ptr = dynamic_cast<const StatValue_InstanceCounter &>(aBase);
    reduceSum(ptr);
  }
  void reduceSum( const StatValue_InstanceCounter & anInstance ) {
    instance_map::const_iterator iter = anInstance.theMap.begin();
    instance_map::const_iterator end = anInstance.theMap.end();
    while (iter != end) {
      theMap[iter->first] += iter->second;
      ++iter;
    }
  }
  boost::intrusive_ptr<StatValueBase> sumAccumulator() {
    return new StatValue_InstanceCounter(*this);
  }
  void update( update_type anUpdate ) {
    theMap[anUpdate.first] += anUpdate.second;
  }
  void set( int64_t anUpdate, value_type aValue ) {
    theMap[anUpdate] = aValue;
  }
  void reset( value_type /*ignored*/) {
    theMap.clear();
  }
  void fillVector(std::vector<sort_helper> & aVector) const {
    instance_map::const_iterator iter = theMap.begin();
    instance_map::const_iterator end = theMap.end();
    while (iter != end) {
      aVector.push_back( sort_helper(iter->first, iter->second));
      ++iter;
    }
  }
  int64_t buckets() const {
    return theMap.size();
  }
  int64_t count(int64_t aKey) const {
    instance_map::const_iterator iter = theMap.find(aKey);
    if (iter != theMap.end()) {
      return iter->second;
    } else {
      return 0;
    }
  }
  int64_t sum() const {
    int64_t ret_val = 0;
    instance_map::const_iterator iter = theMap.begin();
    instance_map::const_iterator end = theMap.end();
    while (iter != end) {
      ret_val += iter->second;
      ++iter;
    }
    return ret_val;
  }
  int64_t weightedSum() const {
    int64_t ret_val = 0;
    instance_map::const_iterator iter = theMap.begin();
    instance_map::const_iterator end = theMap.end();
    while (iter != end) {
      int64_t weight = boost::lexical_cast<int64_t >( iter->first );
      ret_val += weight * iter->second;
      ++iter;
    }
    return ret_val;
  }
  int64_t asLongLong(std::string const & options) const {
    bool calcPercentile = false;
    float percentile = 0.5;
    bool pct100 = false;
    if (options.size() > 0) {
      try {
        if (options.substr(0, 4) == "val:") {
          int64_t key = boost::lexical_cast<int64_t>(options.substr(4));
          return count(key);
        } else if (options.substr(0, 5) == "count") {
          return sum();
        } else if (options.substr(0, 6) == "weight") {
          return weightedSum();
        } else if (options.substr(0, 7) == "buckets") {
          return buckets();
        } else if (options.substr(0, 6) == "median") {
          calcPercentile = true;
          percentile = 0.5;
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
            throw CalcException(std::string("{sum bounds cannot be parsed}"));
          } catch (std::out_of_range &) {
            throw CalcException(std::string("{sum bounds out of range}"));
          }
          int64_t sum = 0;
          for (int32_t i = 0; i < (int)elements.size(); i++) {
            int64_t val = elements[i].value;
            int64_t cnt = elements[i].count;
            if (val >= begin && val <= end) sum += cnt;
          }
          return sum;
        } else if (options.substr(0, 7) == "pctile:") {
          float pct = boost::lexical_cast<float>(options.substr(7));
          if (! (pct > 0.0 && pct <= 100.0) ) {
            throw CalcException(std::string("{Percentile must be between 1 and 100}"));
          }
          if (pct == 100.0) pct100 = true;
          calcPercentile = true;
          percentile = ((float)pct) / 100.0;
        } else {
          throw CalcException(std::string("{Unrecognized option: " + options + " }"));
        }
      } catch (boost::bad_lexical_cast &) {
        throw CalcException(std::string("{Unable to parse options: " + options + " }"));
      }
    }
    if (calcPercentile) {
      std::vector<sort_helper> elements;
      fillVector(elements);
      if (pct100) {
        return elements[elements.size()-1].value;
      } else {
        float sum_f = sum();
        float cum_pct = 0.0;
        for (uint32_t i = 0; i < elements.size(); ++i) {
          float pct = static_cast<float>(elements[i].count) / sum_f;
          cum_pct += pct;
          if (cum_pct >= percentile) {
            return elements[i].value;
          }
        }
      }
    }

    return 0;
  }
  //double asDouble(std::string const & options) const {
  // return 0;
  //}
  double asDouble(std::string const & options) const {
    if (options.size() > 0) {
      try {
        if (options.substr(0, 3) == "avg") {
          return static_cast<double>(weightedSum()) / sum() ;
        } else if (options.substr(0, 3) == "mlp") {
          return static_cast<double>(weightedSum()) / (sum() - count(0));
        } else {
          return asLongLong(options);
        }
      } catch (boost::bad_lexical_cast &) {
        throw CalcException(std::string("{Unable to parse options: " + options + " }"));
      }
    }
    return 0;
  }

  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    doPrint(anOstream, options);
  }

};

template <class CountedValueType>
class StatValueArray_InstanceCounter : public StatValueArrayBase {
  //NOT YET IMPLEMENTED

public:
  virtual boost::intrusive_ptr<const StatValueBase> serialForm() const {
    //NOT YET IMPLEMENTED
    boost::intrusive_ptr<const StatValueBase> ret_val( new StatValue_Counter(0));
    return ret_val;
  };

public:
  typedef std::pair<CountedValueType, int> update_type;
  typedef int64_t value_type;

  typedef std::map<CountedValueType, int64_t> instance_map;
  instance_map theMap;

public:
  StatValueArray_InstanceCounter() {}
  void reduceSum( const StatValueBase * aBase) {
    std::cerr << "Reductions not supported (StatValueArray_InstanceCounter)" << std::endl;
  }
  void update( update_type anUpdate ) {
    theMap[anUpdate.first] += anUpdate.second;
  }
  void print(std::ostream & anOstream, std::string const & options = std::string("")) const {
    //???
  }
  void newValue(accumulation_type aValueType) {
    //???
  }
  void reset( value_type /*ignored*/ ) {
    theMap.clear();
  }
  StatValueBase & operator[](int32_t anIndex) {
    return *this;
  }
  std::size_t size() {
    return 0;
  }
};

} // end aux_
} // end Stat
} // end Flexus

#endif //FLEXUS_CORE_AUX__STATS_HISTOGRAMS__HPP__INCLUDED
