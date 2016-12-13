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
#ifndef FLEXUS_CORE_AUX__STATS_BASE__HPP__INCLUDED
#define FLEXUS_CORE_AUX__STATS_BASE__HPP__INCLUDED

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

struct CalcException {
  std::string theReason;

  CalcException(std::string const & aReason)
    : theReason(aReason)
  {}
};

struct StatValueBase : public boost::counted_base {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) { }
public:
  friend std::ostream & operator << ( std::ostream & anOstream, StatValueBase const & aValue) {
    aValue.print(anOstream);
    return anOstream;
  }
  virtual void print(std::ostream & anOstream, std::string const & options = std::string("")) const {};
  virtual int64_t asLongLong() const {
    return 0;
  };
  virtual int64_t asLongLong(std::string const & options) const {
    throw CalcException(std::string("{Expression options not supported for this stat type}"));
  }
  virtual double asDouble() const {
    return asLongLong();
  };
  virtual double asDouble(std::string const & options) const {
    throw CalcException(std::string("{Expression options not supported for this stat type}"));
  }
  virtual ~StatValueBase() {}
  virtual boost::intrusive_ptr<const StatValueBase> serialForm() const {
    return this;
  }
  virtual boost::intrusive_ptr<StatValueBase> sumAccumulator() {
    throw 1; /* by default, stat's don't support sum accumulation */
  }
  virtual void reduceSum(StatValueBase const & anRHS) { };
  virtual boost::intrusive_ptr<StatValueBase> avgAccumulator() {
    throw 1; /* by default, stat's don't support average accumulation */
  }
  virtual void reduceAvg(StatValueBase const & anRHS) { };
  virtual boost::intrusive_ptr<StatValueBase> stdevAccumulator() {
    throw 1; /* by default, stat's don't support standard deviation accumulation */
  }
  virtual void reduceStdDev(StatValueBase const & anRHS) { };
  virtual boost::intrusive_ptr<StatValueBase> countAccumulator();
  virtual void reduceCount(StatValueBase const & anRHS) { };
};

struct StatValueArrayBase : public StatValueBase {
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueBase>(*this);
  }
public:
  virtual StatValueBase & operator[](int32_t anIndex) = 0;
  virtual void newValue(accumulation_type aValueType) = 0;
  virtual std::size_t size() = 0;
};

template<class Archive>
void save(Archive & ar, ::boost::intrusive_ptr<StatValueBase> const & ptr, uint32_t version);

template<class Archive>
void load(Archive & ar, ::boost::intrusive_ptr<StatValueBase> & ptr, uint32_t version);

template<class Archive>
void save(Archive & ar, ::boost::intrusive_ptr<StatValueArrayBase> const & ptr, uint32_t version);

template<class Archive>
void load(Archive & ar, ::boost::intrusive_ptr<StatValueArrayBase> & ptr, uint32_t version);

} //aux_
} //Stat
} //Flexus

BOOST_SERIALIZATION_ASSUME_ABSTRACT(Flexus::Stat::aux_::StatValueArrayBase)

namespace boost {
namespace serialization {
template<class Archive>
inline void serialize( Archive & ar, ::boost::intrusive_ptr<Flexus::Stat::aux_::StatValueBase> const & t, const uint32_t file_version ) {
  split_free(ar, t, file_version);
}
template<class Archive>
inline void serialize( Archive & ar, ::boost::intrusive_ptr<Flexus::Stat::aux_::StatValueArrayBase> const & t, const uint32_t file_version ) {
  split_free(ar, t, file_version);
}
}
}

namespace Flexus {
namespace Stat {
namespace aux_ {

class StatUpdaterBase; //fwd decl.

class StatValueHandle_Base {
protected:
  std::string const * theStatName;
  boost::intrusive_ptr<StatUpdaterBase> theUpdater;

private:
  friend class boost::serialization::access;
  template<class Archive>
  void save(Archive & ar, uint32_t version) const {
    ar & *theStatName;
  }
  template<class Archive>
  void load(Archive & ar, uint32_t version) {
    std::string temp;
    ar & temp;
    theStatName = new std::string(temp); //Leaks, but doesn't matter
    theUpdater = 0;
  }
  BOOST_SERIALIZATION_SPLIT_MEMBER()

public:
  StatValueHandle_Base();
  StatValueHandle_Base(Stat * aStat, boost::intrusive_ptr<StatUpdaterBase> anUpdater);
  StatValueHandle_Base(std::string const & aName);
  StatValueHandle_Base(const StatValueHandle_Base & aHandle);
  StatValueHandle_Base & operator =(const StatValueHandle_Base & aHandle);
  StatValueHandle_Base & operator +=(const StatValueHandle_Base & aHandle);
  void reset();
  void releaseUpdater();
  std::string name();
  void rename(std::string aName);
};

class StatValueHandle : public StatValueHandle_Base {
  boost::intrusive_ptr< StatValueBase > theValue;
private:
  friend class boost::serialization::access;
  template<class Archive>
  void save(Archive & ar, uint32_t version) const {
    ar & boost::serialization::base_object<StatValueHandle_Base>(*this);
    boost::intrusive_ptr<const StatValueBase> temp(theValue->serialForm());
    ar & temp;
  }
  template<class Archive>
  void load(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueHandle_Base>(*this);
    ar & theValue;
  }
  BOOST_SERIALIZATION_SPLIT_MEMBER()

public:
  StatValueHandle();
  StatValueHandle(Stat * aStat, boost::intrusive_ptr< StatValueBase > aValue,  boost::intrusive_ptr<StatUpdaterBase> anUpdater);
  StatValueHandle(std::string const & aName, boost::intrusive_ptr< StatValueBase > aValue);
  StatValueHandle(const StatValueHandle & aHandle);
  StatValueHandle & operator =(const StatValueHandle & aHandle);
  StatValueHandle & operator +=(const StatValueHandle & aHandle);
  boost::intrusive_ptr<StatValueBase> sumAccumulator();
  boost::intrusive_ptr<StatValueBase> avgAccumulator();
  boost::intrusive_ptr<StatValueBase> stdevAccumulator();
  boost::intrusive_ptr<StatValueBase> countAccumulator();
  friend std::ostream & operator << (std::ostream & anOstream, StatValueHandle const & aValueHandle);
  void print(std::ostream & anOstream, std::string const & options = std::string(""));
  int64_t asLongLong();
  int64_t asLongLong(std::string const & options);
  double asDouble();
  double asDouble(std::string const & options);
  boost::intrusive_ptr< StatValueBase > getValue() {
    return theValue;
  }
  void setValue(boost::intrusive_ptr< StatValueBase > aValue) {
    theValue = aValue;
  }
};

class StatValueArrayHandle : public StatValueHandle_Base {
  boost::intrusive_ptr< StatValueArrayBase > theValue;
private:
  friend class boost::serialization::access;
  template<class Archive>
  void save(Archive & ar, uint32_t version) const {
    ar & boost::serialization::base_object<StatValueHandle_Base>(*this);
    boost::intrusive_ptr<const StatValueBase> temp(theValue->serialForm());
    ar & temp;
  }
  template<class Archive>
  void load(Archive & ar, uint32_t version) {
    ar & boost::serialization::base_object<StatValueHandle_Base>(*this);
    ar & theValue;
  }
  BOOST_SERIALIZATION_SPLIT_MEMBER()

public:
  StatValueArrayHandle();
  StatValueArrayHandle(Stat * aStat, boost::intrusive_ptr< StatValueArrayBase > aValue,  boost::intrusive_ptr<StatUpdaterBase> anUpdater);
  StatValueArrayHandle(const StatValueArrayHandle & aHandle);
  StatValueArrayHandle & operator =(const StatValueArrayHandle & aHandle);
  friend std::ostream & operator << (std::ostream & anOstream, StatValueArrayHandle const & aValueHandle);
  void print(std::ostream & anOstream, std::string const & options = std::string(""));
  void newValue( accumulation_type anAccumulationType );
  int64_t asLongLong();
  int64_t asLongLong(std::string const & options);
};

} // end aux_
} // end Stat
} // end Flexus

#endif //FLEXUS_CORE_AUX__STATS_BASE__HPP__INCLUDED
