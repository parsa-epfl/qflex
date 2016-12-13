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
#ifndef FLEXUS_CORE_STATS_HPP__INCLUDED
#define FLEXUS_CORE_STATS_HPP__INCLUDED

#include <string>
#include <iostream>
#include <functional>
#include <core/boost_extensions/intrusive_ptr.hpp>

#include <core/types.hpp>
#include <core/debug/debug.hpp>

namespace Flexus {
namespace Stat {

namespace aux_ {
class StatValue_PredictionCounter;
}

class Stat; //forward declare

enum class accumulation_type {
  Accumulate
  , Reset
};

class Prediction : public boost::counted_base {
  std::vector< boost::intrusive_ptr< aux_::StatValue_PredictionCounter > > theCounters;
  int64_t theUpdate;
public:
  Prediction(int64_t anUpdate);
  ~Prediction();
  void connectCounter( boost::intrusive_ptr< aux_::StatValue_PredictionCounter > aCounter);
  void confirm();
  void confirm(int64_t anOverride);
  void dismiss();
  void guess();
  void guess(int64_t anOverride);
  void goodGuess();
  void reject();
  void reject(int64_t anOverride);
};

enum class eReduction {
  eSum
  , eAverage
  , eStdDev
  , eCount
};

} //Flexus
} //Stat

#include <core/aux_/stats/stats_aux_.hpp>

namespace Flexus {
namespace Stat {

struct StatManager {
  virtual ~StatManager() {}
  virtual void registerStat(Stat *) = 0;
  virtual void initialize() = 0;
  virtual void finalize() = 0;
  virtual boost::intrusive_ptr<aux_::Measurement> openMeasurement(std::string const & aName, std::string const & aStatSpec = std::string(".*")) = 0;
  virtual void openPeriodicMeasurement(std::string const & aName, int64_t aPeriod, accumulation_type anAccumulation, std::string const & aStatSpec = std::string(".*")) = 0;
  virtual void openLoggedPeriodicMeasurement(std::string const & aName, int64_t aPeriod, accumulation_type anAccumulation, std::ostream & anOstream, std::string const & aStatSpec = std::string(".*")) = 0;
  virtual void closeMeasurement(std::string const & aName) = 0;
  virtual void listStats(std::ostream & anOstream) = 0;
  virtual void listMeasurements(std::ostream & anOstream) = 0;
  virtual void printMeasurement(std::string const & aMeasurementSpec, std::ostream & anOstream) = 0;
  virtual void format(std::string const & aMeasurementSpec, std::string const & aFormat, std::ostream & anOstream) = 0;
  virtual void formatFile(std::string const & aMeasurementSpec, std::string const & aFileName, std::ostream & anOstream) = 0;
  virtual void collapse(std::string const & aMeasurementSpec, std::string const & aFormat, std::ostream & anOstream) = 0;
  virtual void collapseFile(std::string const & aMeasurementSpec, std::string const & aFileName, std::ostream & anOstream) = 0;
  virtual void reduce(eReduction aReduction, std::string const & aMeasurementSpec, std::string const & aDestMeasurement, std::ostream & anOstream) = 0;
  virtual void saveMeasurements(std::string const & aMeasurementSpec, std::string const & aFileName) const = 0;
  virtual void tick(int64_t anAdvance = 1) = 0;
  virtual int64_t ticks() = 0;
  virtual void reduceNodes(std::string const & aMeasurementSpec) = 0;
  virtual void addEvent(int64_t aDeadline, std::function<void()> anEvent) = 0;
  virtual void addFinalizer(std::function<void()> aFinalizer) = 0;
  virtual void save(std::ostream & anOstream) const = 0;
  virtual void load(std::istream & anIstream) = 0;
  virtual void loadMore(std::istream & anIstream, std::string const & aPrefix) = 0;
};

StatManager * getStatManager();

class Stat : public boost::counted_base {
  std::string theFullName;
public:
  Stat( std::string const & aName)
    : theFullName(aName)
  { }
  void registerStat() {
    getStatManager()->registerStat(this);
  }
  virtual ~Stat() {}
  std::string const & name() const {
    return theFullName;
  }
  std::string const * namePtr() const {
    return &theFullName;
  }
  virtual std::string const & type() const = 0;
  virtual aux_::StatValueHandle createValue() = 0;
  virtual aux_::StatValueArrayHandle createValueArray() = 0;
  friend std::ostream & operator << (std::ostream & anOstream, Stat const & aStat) {
    anOstream << aStat.name();
    return anOstream;
  }
};

class StatCounter : public Stat, public aux_::StatUpdaterLink< aux_::StatUpdater< aux_::StatValue_Counter::update_type > > {

public:
  typedef aux_::StatValue_Counter stat_value_type;
  typedef aux_::StatValueArray_Counter stat_value_array_type;

  // Updater Link Implementation
private:
  typedef aux_::StatUpdater< stat_value_type::update_type > updater_type;
  updater_type * theUpdater;
  int64_t theInitialValue;
public:
  virtual void setNextUpdater(updater_type * aLink) {
    theUpdater = aLink;
  }

  // Interface to Measurements
  aux_::StatValueHandle createValue() {
    boost::intrusive_ptr<stat_value_type> new_value(new stat_value_type(theInitialValue));
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_type, stat_value_type::update_type>(new_value, theInitialValue, this, theUpdater ) );
    return aux_::StatValueHandle( this, new_value, new_updater );
  }
  aux_::StatValueArrayHandle createValueArray() {
    boost::intrusive_ptr<stat_value_array_type> new_value(new stat_value_array_type(theInitialValue));
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_array_type, stat_value_array_type::update_type>(new_value, theInitialValue, this, theUpdater ) );
    return aux_::StatValueArrayHandle( this, new_value, new_updater );
  }

public:
  //Create a counter with a specific name
  StatCounter( std::string const & aName, int64_t anInitialValue = 0 )
    : Stat( aName )
    , theUpdater(0)
    , theInitialValue(anInitialValue) {
    registerStat();
  }

  template <class Component>
  StatCounter( std::string const & aName , Component * aComponent, int64_t anInitialValue = 0)
    : Stat( aComponent->statName() + "-" + aName)
    , theUpdater(0)
    , theInitialValue(anInitialValue) {
    registerStat();
  }

  std::string const & type() const {
    static std::string theType("Counter");
    return theType;
  }

  //Increment Counter
  StatCounter & operator ++() {
    if (theUpdater) theUpdater->update(1);
    return *this;
  }
  StatCounter & operator ++(int) {
    if (theUpdater) theUpdater->update(1);
    return *this;
  }

  //Decrement Counter
  StatCounter & operator --() {
    if (theUpdater) theUpdater->update(-1);
    return *this;
  }
  StatCounter & operator --(int) {
    if (theUpdater) theUpdater->update(-1);
    return *this;
  }

  //Increase Counter
  StatCounter & operator +=( stat_value_type::update_type anUpdate) {
    if (theUpdater) theUpdater->update(anUpdate);
    return *this;
  }

  //Decrease Counter
  StatCounter & operator -=( stat_value_type::update_type anUpdate) {
    if (theUpdater) theUpdater->update(-anUpdate);
    return *this;
  }

  bool enabled() {
    return true;
  }

};

/*
class StatDoubleCounter : public Stat, public aux_::StatUpdaterLink< aux_::StatUpdater< aux_::StatValue_DoubleCounter::update_type > > {

  public:
    typedef aux_::StatValue_DoubleCounter stat_value_type;
    typedef aux_::StatValueArray_DoubleCounter stat_value_array_type;

  // Updater Link Implementation
    private:
      typedef aux_::StatUpdater< stat_value_type::update_type > updater_type;
      updater_type * theUpdater;
      double theInitialValue;
    public:
      virtual void setNextUpdater(updater_type * aLink) {
        theUpdater = aLink;
      }

  // Interface to Measurements
    aux_::StatValueHandle createValue() {
      boost::intrusive_ptr<stat_value_type> new_value(new stat_value_type(theInitialValue));
      boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_type, stat_value_type::update_type>(new_value, theInitialValue, this, theUpdater ) );
      return aux_::StatValueHandle( this, new_value, new_updater );
    }
    aux_::StatValueArrayHandle createValueArray() {
      boost::intrusive_ptr<stat_value_array_type> new_value(new stat_value_array_type(theInitialValue));
      boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_array_type, stat_value_array_type::update_type>(new_value, theInitialValue, this, theUpdater ) );
      return aux_::StatValueArrayHandle( this, new_value, new_updater );
    }

  public:
    //Create a counter with a specific name
    StatDoubleCounter( std::string const & aName, double anInitialValue = 0 )
      : Stat( aName )
      , theUpdater(0)
      , theInitialValue(anInitialValue)
      {
        registerStat();
      }

    template <class Component>
    StatDoubleCounter( std::string const & aName , Component * aComponent, int64_t anInitialValue = 0)
      : Stat( aComponent->statName() + "-" + aName)
      , theUpdater(0)
      , theInitialValue(anInitialValue)
      {
        registerStat();
      }

    std::string const & type() const { static std::string theType("Counter"); return theType; }

    //Increment Counter
    StatDoubleCounter & operator ++() { if (theUpdater) theUpdater->update(1); return *this; }
    StatDoubleCounter & operator ++(int) { if (theUpdater) theUpdater->update(1); return *this; }

    //Decrement Counter
    StatDoubleCounter & operator --() { if (theUpdater) theUpdater->update(-1); return *this; }
    StatDoubleCounter & operator --(int) { if (theUpdater) theUpdater->update(-1); return *this; }

    //Increase Counter
    StatDoubleCounter & operator +=( stat_value_type::update_type anUpdate) { if (theUpdater) theUpdater->update(anUpdate); return *this; }

    //Decrease Counter
    StatDoubleCounter & operator -=( stat_value_type::update_type anUpdate) { if (theUpdater) theUpdater->update(-anUpdate); return *this; }

    bool enabled() { return true; }

};
*/

class StatPredictionCounter : public Stat, public aux_::StatUpdaterLink< aux_::StatUpdater< aux_::StatValue_PredictionCounter::update_type > > {

public:
  typedef aux_::StatValue_PredictionCounter stat_value_type;
  typedef aux_::StatValue_PredictionCounter stat_value_array_type; //Not supported

  // Updater Link Implementation
  typedef aux_::StatUpdater< stat_value_type::update_type > updater_type;
  updater_type * theUpdater;
public:
  virtual void setNextUpdater(updater_type * aLink) {
    theUpdater = aLink;
  }

  // Interface to Measurements
  aux_::StatValueHandle createValue() {
    boost::intrusive_ptr<stat_value_type> new_value(new stat_value_type(0));
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_type, stat_value_type::update_type>(new_value, 0, this, theUpdater ) );
    return aux_::StatValueHandle( this, new_value, new_updater );
  }
  aux_::StatValueArrayHandle createValueArray() {
    DBG_Assert(false);
    return aux_::StatValueArrayHandle( 0, 0, 0 ); //suppress warning
  }

public:
  //Create a counter with a specific name
  StatPredictionCounter( std::string const & aName )
    : Stat( aName )
    , theUpdater(0) {
    registerStat();
  }

  template <class Component>
  StatPredictionCounter( std::string const & aName , Component * aComponent, int64_t anInitialValue = 0)
    : Stat( aComponent->statName() + "-" + aName)
    , theUpdater(0) {
    registerStat();
  }

  std::string const & type() const {
    static std::string theType("PredictionCounter");
    return theType;
  }

  //Increment Counter
  boost::intrusive_ptr<Prediction> predict(int64_t anIncrement = 1) {
    boost::intrusive_ptr<Prediction> ret_val = new Prediction(anIncrement);
    if (theUpdater) {
      theUpdater->update(*ret_val);
    }
    return ret_val;
  }

  bool enabled() {
    return true;
  }

};

class StatMax : public Stat, public aux_::StatUpdaterLink< aux_::StatUpdater< aux_::StatValue_Max::update_type > >  {

public:
  typedef aux_::StatValue_Max stat_value_type;
  typedef aux_::StatValueArray_Max stat_value_array_type;

  // Updater Link Implementation
private:
  typedef aux_::StatUpdater< stat_value_type::update_type > updater_type;
  updater_type * theUpdater;
public:
  virtual void setNextUpdater(updater_type * aLink) {
    theUpdater = aLink;
  }

  // Interface to Measurements
  aux_::StatValueHandle createValue() {
    boost::intrusive_ptr<stat_value_type> new_value(new stat_value_type(0));
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_type, stat_value_type::update_type>(new_value, 0, this, theUpdater ) );
    return aux_::StatValueHandle( this, new_value, new_updater );
  }
  aux_::StatValueArrayHandle createValueArray() {
    boost::intrusive_ptr<stat_value_array_type> new_value(new stat_value_array_type(0));
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_array_type, stat_value_array_type::update_type>(new_value, 0, this, theUpdater ) );
    return aux_::StatValueArrayHandle( this, new_value, new_updater );
  }

public:
  //Create a counter with a specific name
  StatMax( std::string const & aName )
    : Stat( aName )
    , theUpdater(0) {
    registerStat();
  }

  template <class Component>
  StatMax( std::string const & aName , Component * aComponent)
    : Stat(aComponent->statName() + "-" + aName)
    , theUpdater(0) {
    registerStat();
  }

  std::string const & type() const {
    static std::string theType("Max");
    return theType;
  }

  //Increment Counter
  StatMax & operator <<(stat_value_type::update_type anUpdate) {
    if (theUpdater) theUpdater->update(anUpdate);
    return *this;
  }

  bool enabled() {
    return true;
  }
};

class StatAnnotation : public Stat, public aux_::StatUpdaterLink< aux_::StatUpdater< aux_::StatValue_Annotation::update_type > > {

public:
  typedef aux_::StatValue_Annotation stat_value_type;
  typedef aux_::StatValueArray_Annotation stat_value_array_type;

  // Updater Link Implementation
private:
  typedef aux_::StatUpdater< stat_value_type::update_type > updater_type;
  updater_type * theUpdater;
public:
  virtual void setNextUpdater(updater_type * aLink) {
    theUpdater = aLink;
  }

  // Interface to Measurements
  aux_::StatValueHandle createValue() {
    boost::intrusive_ptr<stat_value_type> new_value(new stat_value_type(theInitialValue));
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_type, stat_value_type::update_type>(new_value, theInitialValue, this, theUpdater ) );
    return aux_::StatValueHandle( this, new_value, new_updater );
  }
  aux_::StatValueArrayHandle createValueArray() {
    boost::intrusive_ptr<stat_value_array_type> new_value(new stat_value_array_type(""));
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_array_type, stat_value_array_type::update_type>(new_value, theInitialValue, this, theUpdater ) );
    return aux_::StatValueArrayHandle( this, new_value, new_updater );
  }

  std::string theInitialValue;

public:
  //Create a counter with a specific name
  StatAnnotation( std::string const & aName, std::string const & aValue = "")
    : Stat( aName )
    , theUpdater(0)
    , theInitialValue(aValue) {
    registerStat();
  }

  template <class Component>
  StatAnnotation( std::string const & aName , Component * aComponent, std::string const & aValue = "")
    : Stat(aComponent->statName() + "-" + aName)
    , theUpdater(0)
    , theInitialValue(aValue) {
    registerStat();
  }

  std::string const & type() const {
    static std::string theType("Annotation");
    return theType;
  }

  //Increment Counter
  StatAnnotation & operator = (stat_value_type::update_type anUpdate) {
    if (theUpdater) theUpdater->update(anUpdate);
    theInitialValue = anUpdate;
    return *this;
  }
  StatAnnotation & operator << (stat_value_type::update_type anUpdate) {
    if (theUpdater) theUpdater->update(anUpdate);
    return *this;
  }

  bool enabled() {
    return true;
  }
};

class StatAverage : public Stat, public aux_::StatUpdaterLink< aux_::StatUpdater< aux_::StatValue_Average::update_type > > {

public:
  typedef aux_::StatValue_Average stat_value_type;
  typedef aux_::StatValueArray_Average stat_value_array_type;

  // Updater Link Implementation
private:
  typedef aux_::StatUpdater< stat_value_type::update_type > updater_type;
  updater_type * theUpdater;
public:
  virtual void setNextUpdater(updater_type * aLink) {
    theUpdater = aLink;
  }

  // Interface to Measurements
  aux_::StatValueHandle createValue() {
    boost::intrusive_ptr<stat_value_type> new_value(new stat_value_type(0));
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_type, stat_value_type::update_type>(new_value, 0, this, theUpdater ) );
    return aux_::StatValueHandle( this, new_value, new_updater );
  }
  aux_::StatValueArrayHandle createValueArray() {
    boost::intrusive_ptr<stat_value_array_type> new_value(new stat_value_array_type(0));
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_array_type, stat_value_array_type::update_type>(new_value, 0, this, theUpdater ) );
    return aux_::StatValueArrayHandle( this, new_value, new_updater );
  }

public:
  //Create a counter with a specific name
  StatAverage( std::string const & aName )
    : Stat( aName )
    , theUpdater(0) {
    registerStat();
  }

  template <class Component>
  StatAverage( std::string const & aName , Component * aComponent)
    : Stat(aComponent->statName() + "-" + aName)
    , theUpdater(0) {
    registerStat();
  }

  std::string const & type() const {
    static std::string theType("Average");
    return theType;
  }

  //Increment Counter
  StatAverage & operator <<(stat_value_type::update_type anUpdate) {
    if (theUpdater) theUpdater->update(anUpdate);
    return *this;
  }
  StatAverage & operator <<(stat_value_type::update_type::first_type anUpdate) {
    if (theUpdater) theUpdater->update( std::make_pair(anUpdate, 1));
    return *this;
  }

  bool enabled() {
    return true;
  }
};

class StatStdDev : public Stat, public aux_::StatUpdaterLink< aux_::StatUpdater< aux_::StatValue_StdDev::update_type > > {

public:
  typedef aux_::StatValue_StdDev stat_value_type;
  typedef aux_::StatValueArray_StdDev stat_value_array_type;

  // Updater Link Implementation
private:
  typedef aux_::StatUpdater< stat_value_type::update_type > updater_type;
  updater_type * theUpdater;
public:
  virtual void setNextUpdater(updater_type * aLink) {
    theUpdater = aLink;
  }

  // Interface to Measurements
  aux_::StatValueHandle createValue() {
    boost::intrusive_ptr<stat_value_type> new_value(new stat_value_type(0));
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_type, stat_value_type::update_type>(new_value, 0, this, theUpdater ) );
    return aux_::StatValueHandle( this, new_value, new_updater );
  }
  aux_::StatValueArrayHandle createValueArray() {
    boost::intrusive_ptr<stat_value_array_type> new_value(new stat_value_array_type(0));
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_array_type, stat_value_array_type::update_type>(new_value, 0, this, theUpdater ) );
    return aux_::StatValueArrayHandle( this, new_value, new_updater );
  }

public:
  //Create a counter with a specific name
  StatStdDev( std::string const & aName )
    : Stat( aName )
    , theUpdater(0) {
    registerStat();
  }

  template <class Component>
  StatStdDev( std::string const & aName , Component * aComponent)
    : Stat(aComponent->statName() + "-" + aName)
    , theUpdater(0) {
    registerStat();
  }

  std::string const & type() const {
    static std::string theType("StdDev");
    return theType;
  }

  //Increment Counter
  StatStdDev & operator <<(stat_value_type::update_type anUpdate) {
    if (theUpdater) theUpdater->update(anUpdate);
    return *this;
  }

  bool enabled() {
    return true;
  }
};

class StatLog2Histogram : public Stat, public aux_::StatUpdaterLink< aux_::StatUpdater< aux_::StatValue_Log2Histogram::update_type > > {

public:
  typedef aux_::StatValue_Log2Histogram stat_value_type;
  typedef aux_::StatValue_Log2Histogram stat_value_array_type; //Arrays not supported

  // Updater Link Implementation
private:
  typedef aux_::StatUpdater< stat_value_type::update_type > updater_type;
  updater_type * theUpdater;
public:
  virtual void setNextUpdater(updater_type * aLink) {
    theUpdater = aLink;
  }

  // Interface to Measurements
  aux_::StatValueHandle createValue() {
    boost::intrusive_ptr<stat_value_type> new_value(new stat_value_type(0));
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_type, stat_value_type::update_type>(new_value, 0, this, theUpdater ) );
    return aux_::StatValueHandle( this, new_value, new_updater );
  }
  aux_::StatValueArrayHandle createValueArray() {
    DBG_Assert(0);
    return aux_::StatValueArrayHandle(0, 0, 0); //Suppress warning
  }

public:
  //Create a counter with a specific name
  StatLog2Histogram( std::string const & aName )
    : Stat( aName )
    , theUpdater(0) {
    registerStat();
  }

  template <class Component>
  StatLog2Histogram( std::string const & aName , Component * aComponent)
    : Stat(aComponent->statName() + "-" + aName)
    , theUpdater(0) {
    registerStat();
  }

  std::string const & type() const {
    static std::string theType("Log2Histogram");
    return theType;
  }

  //Increment Counter
  StatLog2Histogram & operator <<(stat_value_type::update_type anUpdate) {
    if (theUpdater) theUpdater->update(anUpdate);
    return *this;
  }
  //Decrement Counter
  StatLog2Histogram & operator >>(stat_value_type::update_type anUpdate) {
    if (theUpdater) theUpdater->update(-1LL * anUpdate);
    return *this;
  }

  bool enabled() {
    return true;
  }
};

class StatWeightedLog2Histogram : public Stat, public aux_::StatUpdaterLink< aux_::StatUpdater< aux_::StatValue_WeightedLog2Histogram::update_type > > {

public:
  typedef aux_::StatValue_WeightedLog2Histogram stat_value_type;
  typedef aux_::StatValue_WeightedLog2Histogram stat_value_array_type; //Arrays not supported

  // Updater Link Implementation
private:
  typedef aux_::StatUpdater< stat_value_type::update_type > updater_type;
  updater_type * theUpdater;
public:
  virtual void setNextUpdater(updater_type * aLink) {
    theUpdater = aLink;
  }

  // Interface to Measurements
  aux_::StatValueHandle createValue() {
    boost::intrusive_ptr<stat_value_type> new_value(new stat_value_type(0));
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_type, stat_value_type::update_type>(new_value, 0, this, theUpdater ) );
    return aux_::StatValueHandle( this, new_value, new_updater );
  }
  aux_::StatValueArrayHandle createValueArray() {
    DBG_Assert(0);
    return aux_::StatValueArrayHandle(0, 0, 0); //Suppress warning
  }

public:
  //Create a counter with a specific name
  StatWeightedLog2Histogram( std::string const & aName )
    : Stat( aName )
    , theUpdater(0) {
    registerStat();
  }

  template <class Component>
  StatWeightedLog2Histogram( std::string const & aName , Component * aComponent)
    : Stat(aComponent->statName() + "-" + aName)
    , theUpdater(0) {
    registerStat();
  }

  std::string const & type() const {
    static std::string theType("WeightedLog2Histogram");
    return theType;
  }

  //Increment Counter
  StatWeightedLog2Histogram & operator <<(stat_value_type::update_type anUpdate) {
    if (theUpdater) theUpdater->update(anUpdate);
    return *this;
  }
  //Decrement Counter
  StatWeightedLog2Histogram & operator >>(stat_value_type::update_type anUpdate) {
    anUpdate.second = -1LL * anUpdate.second;
    if (theUpdater) theUpdater->update(anUpdate);
    return *this;
  }

  bool enabled() {
    return true;
  }
};

class StatStdDevLog2Histogram : public Stat, public aux_::StatUpdaterLink< aux_::StatUpdater< aux_::StatValue_StdDevLog2Histogram::update_type > > {

public:
  typedef aux_::StatValue_StdDevLog2Histogram stat_value_type;
  typedef aux_::StatValue_StdDevLog2Histogram stat_value_array_type; //Arrays not supported

  // Updater Link Implementation
private:
  typedef aux_::StatUpdater< stat_value_type::update_type > updater_type;
  updater_type * theUpdater;
public:
  virtual void setNextUpdater(updater_type * aLink) {
    theUpdater = aLink;
  }

  // Interface to Measurements
  aux_::StatValueHandle createValue() {
    boost::intrusive_ptr<stat_value_type> new_value(new stat_value_type(0));
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_type, stat_value_type::update_type>(new_value, 0, this, theUpdater ) );
    return aux_::StatValueHandle( this, new_value, new_updater );
  }
  aux_::StatValueArrayHandle createValueArray() {
    DBG_Assert(0);
    return aux_::StatValueArrayHandle(0, 0, 0); //Suppress warning
  }

public:
  //Create a counter with a specific name
  StatStdDevLog2Histogram( std::string const & aName )
    : Stat( aName )
    , theUpdater(0) {
    registerStat();
  }

  template <class Component>
  StatStdDevLog2Histogram( std::string const & aName , Component * aComponent)
    : Stat(aComponent->statName() + "-" + aName)
    , theUpdater(0) {
    registerStat();
  }

  std::string const & type() const {
    static std::string theType("StdDevLog2Histogram");
    return theType;
  }

  //Increment Counter
  StatStdDevLog2Histogram & operator <<(stat_value_type::update_type anUpdate) {
    if (theUpdater) theUpdater->update(anUpdate);
    return *this;
  }
  bool enabled() {
    return true;
  }
};

template <class CountedValueType>
class StatUniqueCounter : public Stat, public aux_::StatUpdaterLink< aux_::StatUpdater< CountedValueType > > {

public:
  typedef aux_::StatValue_UniqueCounter<CountedValueType> stat_value_type;
  typedef aux_::StatValueArray_UniqueCounter<CountedValueType> stat_value_array_type;

  // Updater Link Implementation
private:
  typedef aux_::StatUpdater< typename stat_value_type::update_type > updater_type;
  updater_type * theUpdater;
public:
  virtual void setNextUpdater(updater_type * aLink) {
    theUpdater = aLink;
  }

  // Interface to Measurements
  aux_::StatValueHandle createValue() {
    boost::intrusive_ptr<stat_value_type> new_value(new stat_value_type);
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_type, typename stat_value_type::update_type>(new_value, 0, this, theUpdater ) );
    return aux_::StatValueHandle( this, new_value, new_updater );
  }
  aux_::StatValueArrayHandle createValueArray() {
    boost::intrusive_ptr<stat_value_array_type> new_value(new stat_value_array_type);
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_array_type, typename stat_value_array_type::update_type>(new_value, 0, this, theUpdater ) );
    return aux_::StatValueArrayHandle( this, new_value, new_updater );
  }

public:
  //Create a counter with a specific name
  StatUniqueCounter( std::string const & aName)
    : Stat( aName )
    , theUpdater(0) {
    registerStat();
  }

  template <class Component>
  StatUniqueCounter( std::string const & aName , Component * aComponent)
    : Stat(aComponent->statName() + "-" + aName)
    , theUpdater(0) {
    registerStat();
  }

  std::string const & type() const {
    static std::string theType("UniqueCounter");
    return theType;
  }

  //Count operator
  StatUniqueCounter & operator <<( typename stat_value_type::update_type anUpdate) {
    if (theUpdater) theUpdater->update(anUpdate);
    return *this;
  }

  bool enabled() {
    return true;
  }

};

template <class CountedValueType>
class StatInstanceCounter : public Stat, public aux_::StatUpdaterLink< aux_::StatUpdater< typename aux_::StatValue_InstanceCounter<CountedValueType>::update_type > > {

public:
  typedef aux_::StatValue_InstanceCounter<CountedValueType> stat_value_type;
  typedef aux_::StatValueArray_InstanceCounter<CountedValueType> stat_value_array_type;

  // Updater Link Implementation
private:
  typedef aux_::StatUpdater< typename stat_value_type::update_type > updater_type;
  updater_type * theUpdater;
public:
  virtual void setNextUpdater(updater_type * aLink) {
    theUpdater = aLink;
  }

  // Interface to Measurements
  aux_::StatValueHandle createValue() {
    boost::intrusive_ptr<stat_value_type> new_value(new stat_value_type);
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_type, typename stat_value_type::update_type>(new_value, 0, this, theUpdater ) );
    return aux_::StatValueHandle( this, new_value, new_updater );
  }
  aux_::StatValueArrayHandle createValueArray() {
    boost::intrusive_ptr<stat_value_array_type> new_value(new stat_value_array_type);
    boost::intrusive_ptr<updater_type> new_updater(new aux_::SimpleStatUpdater<stat_value_array_type, typename stat_value_array_type::update_type>(new_value, 0, this, theUpdater ) );
    return aux_::StatValueArrayHandle( this, new_value, new_updater );
  }

public:
  //Create a counter with a specific name
  StatInstanceCounter( std::string const & aName)
    : Stat( aName )
    , theUpdater(0) {
    registerStat();
  }

  template <class Component>
  StatInstanceCounter( std::string const & aName , Component * aComponent)
    : Stat(aComponent->statName() + "-" + aName)
    , theUpdater(0) {
    registerStat();
  }

  std::string const & type() const {
    static std::string theType("InstanceCounter");
    return theType;
  }

  bool enabled() {
    return true;
  }

  //Count operator
  StatInstanceCounter & operator <<( typename stat_value_type::update_type anUpdate) {
    if (theUpdater) theUpdater->update(anUpdate);
    return *this;
  }
  //Remove operator
  StatInstanceCounter & operator >>( typename stat_value_type::update_type anUpdate) {
    anUpdate.second = -1LL * anUpdate.second;
    if (theUpdater) theUpdater->update(anUpdate);
    return *this;
  }

};

} // end Stat
} // end Flexus

#endif //FLEXUS_CORE_STATS_HPP__INCLUDED
