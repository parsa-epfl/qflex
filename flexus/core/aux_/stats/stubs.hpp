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
#ifndef FLEXUS_CORE_AUX__STATS_STUBS__HPP__INCLUDED
#define FLEXUS_CORE_AUX__STATS_STUBS__HPP__INCLUDED

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

template<class Archive>
void save(Archive & ar, ::boost::intrusive_ptr<StatValueBase> const & ptr, uint32_t version) {
  StatValueBase * svb = ptr.get();
  ar & svb;
}

template<class Archive>
void load(Archive & ar, ::boost::intrusive_ptr<StatValueBase> & ptr, uint32_t version) {
  StatValueBase * svb;
  ar & svb;
  ptr = boost::intrusive_ptr<Flexus::Stat::aux_::StatValueBase> (svb);
}

template<class Archive>
void save(Archive & ar, ::boost::intrusive_ptr<StatValueArrayBase> const & ptr, uint32_t version) {
  StatValueArrayBase * svab = ptr.get();
  ar & svab;
}

template<class Archive>
void load(Archive & ar, ::boost::intrusive_ptr<StatValueArrayBase> & ptr, uint32_t version) {
  StatValueArrayBase * svab;
  ar & svab;
  ptr = boost::intrusive_ptr<Flexus::Stat::aux_::StatValueArrayBase> (svab);
}

template <class StatUpdater>
struct StatUpdaterLink {
  virtual ~StatUpdaterLink() {}
  virtual void setNextUpdater(StatUpdater * aLink) = 0;
};

template <class StatUpdater>
class StatUpdaterLinkImpl : public StatUpdaterLink<StatUpdater> {
protected:
  StatUpdaterLink<StatUpdater> * thePreviousUpdater;
  StatUpdater * theNextUpdater;

  StatUpdaterLinkImpl( StatUpdaterLink<StatUpdater> * aPrevious, StatUpdater * aNext)
    : thePreviousUpdater(aPrevious)
    , theNextUpdater(aNext) {
    if (theNextUpdater) {
      theNextUpdater->setPreviousUpdater(this);
    }
  }

  virtual ~StatUpdaterLinkImpl() {
    if (theNextUpdater) {
      theNextUpdater->setPreviousUpdater(thePreviousUpdater);
    }
    thePreviousUpdater = 0;
    theNextUpdater = 0;
  }

  void getNextUpdater() {
    return theNextUpdater;
  }
  virtual void setNextUpdater(StatUpdater * aLink) {
    theNextUpdater = aLink;
  }
  virtual void setPreviousUpdater(StatUpdaterLink<StatUpdater> * aLink) {
    thePreviousUpdater = aLink;
  }
};

struct StatUpdaterBase : public boost::counted_base {
  virtual ~StatUpdaterBase() {
  }
  virtual void reset() = 0;
};

template < class UpdateType >
struct StatUpdater : public StatUpdaterBase, public StatUpdaterLinkImpl< StatUpdater<UpdateType> > {
  StatUpdater(StatUpdaterLink< StatUpdater<UpdateType> > * aPrevious, StatUpdater<UpdateType> * aNext)
    : StatUpdaterLinkImpl< StatUpdater<UpdateType> >(aPrevious, aNext) {
    if (this->thePreviousUpdater) {
      this->thePreviousUpdater->setNextUpdater(this);
    }
  }
  virtual ~StatUpdater() {
    if (this->thePreviousUpdater) {
      this->thePreviousUpdater->setNextUpdater(this->theNextUpdater);
    }
  }
  virtual void update( UpdateType anUpdate ) = 0;
};

template < class StatValueType, class UpdateType >
class SimpleStatUpdater : public StatUpdater<UpdateType> {
  boost::intrusive_ptr<StatValueType> theValue;
  typename StatValueType::value_type theResetValue;
public:
  SimpleStatUpdater( boost::intrusive_ptr<StatValueType> aValue, typename StatValueType::value_type aResetValue, StatUpdaterLink<StatUpdater<UpdateType> > * aPrevious, StatUpdater<UpdateType> * aNext)
    : StatUpdater<UpdateType> (aPrevious, aNext)
    , theValue(aValue)
    , theResetValue(aResetValue)
  {}
  virtual ~SimpleStatUpdater() {}
  virtual void update( UpdateType anUpdate ) {
    theValue->update(anUpdate);
    if (this->theNextUpdater) {
      this->theNextUpdater->update(anUpdate);
    }
  }
  virtual void reset() {
    theValue->reset(theResetValue);
    if (this->theNextUpdater) {
      this->theNextUpdater->reset();
    }
  }
};

} // end aux_
} // end Stat
} // end Flexus

#endif //FLEXUS_CORE_AUX__STATS_STUBS__HPP__INCLUDED
