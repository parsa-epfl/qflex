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

#include <core/boost_extensions/lexical_cast.hpp>

#include <core/debug/entry.hpp>
#include <core/debug/category.hpp>

#include <core/debug/filter.hpp>

namespace Flexus {
namespace Dbg {

typedef Filter::MatchResult MatchResult;

void CompoundFilter::destruct() {
  std::for_each( theFilters.begin(), theFilters.end(), [](Filter* aFilter) {
      delete aFilter;
  });//Clean up all pointers owned by theActions
}

//Returns Exclude if any filter returns exclude.
//Returns Include if at least one filter returns Include and none return Exclude
//Otherwise, returns NoMatch
MatchResult CompoundFilter::match(Entry const & anEntry) {
  MatchResult result = NoMatch;
  std::vector<Filter *>::iterator iter = theFilters.begin();
  while (iter != theFilters.end()) {
    MatchResult test = (*iter)->match(anEntry);
    if (test == Exclude) {
      result = Exclude;
      break;
    } else if (test == Include) {
      result = Include;
    }
    ++iter;
  }
  return result;
}

void CompoundFilter::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
  std::for_each( theFilters.begin(), theFilters.end(), [&anOstream, &anIndent](Filter* aFilter) {
      aFilter->printConfiguration(anOstream, anIndent);
  });
};

void CompoundFilter::add(Filter * aFilter) {
  theFilters.push_back(aFilter); //Ownership assumed by theFilters
}

MatchResult IncludeFilter::match(Entry const & anEntry) {
  MatchResult result(Include);
  std::vector<Filter *>::iterator iter(theFilters.begin());
  while (iter != theFilters.end() && result == Include) {
    result = (*iter)->match(anEntry);
    ++iter;
  }
  if (result != Include) {
    result = NoMatch;
  }
  return result;
}

void IncludeFilter::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
  std::vector<Filter *>::iterator iter(theFilters.begin());
  if (iter != theFilters.end()) {
    (*iter)->printConfiguration(anOstream, anIndent + "+ ");
    ++iter;
    std::for_each(iter, theFilters.end(), [&anOstream, &anIndent](Filter* aFilter){ return aFilter->printConfiguration(anOstream, anIndent + "& ");});
    //std::for_each(iter, theFilters.end(), std::bind(&Filter::printConfiguration, std::_1, std::var(anOstream), anIndent + "& ") );
  }
  anOstream << anIndent << ";\n";
};

MatchResult ExcludeFilter::match(Entry const & anEntry) {
  MatchResult result(Include);
  std::vector<Filter *>::iterator iter = theFilters.begin();
  while (iter != theFilters.end() && result == Include) {
    result = (*iter)->match(anEntry);
    ++iter;
  }
  if (result == Include) {
    result = Exclude;
  } else {
    result = NoMatch;
  }
  return result;
}

void ExcludeFilter::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
  std::vector<Filter *>::iterator iter(theFilters.begin());
  if (iter != theFilters.end()) {
    (*iter)->printConfiguration(anOstream, anIndent + "- ");
    ++iter;
    std::for_each(iter, theFilters.end(), [&anOstream, &anIndent](Filter* aFilter){ return aFilter->printConfiguration(anOstream, anIndent + "& ");});
    //std::for_each(iter, theFilters.end(), std::bind(&Filter::printConfiguration, std::_1, std::var(anOstream), anIndent + "& ") );
  }
  anOstream << anIndent << ";\n";
};

ExistsFilter::ExistsFilter(std::string const & aField)
  : SimpleFilter(aField)
{}

MatchResult ExistsFilter::match(Entry const & anEntry) {
  if (anEntry.exists(theField)) {
    return Include;
  } else {
    return NoMatch;
  }
}

void ExistsFilter::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
  anOstream << anIndent << '{' << theField << "} exists\n";
};

CategoryFilter::CategoryFilter(std::string const & aCategory)
  : theCategory(& CategoryMgr::categoryMgr().category(aCategory))
{ }

MatchResult CategoryFilter::match(Entry const & anEntry) {
  if (anEntry.hasCategory(theCategory)) {
    return Include;
  } else {
    return NoMatch;
  }
}

void CategoryFilter::printConfiguration(std::ostream & anOstream, std::string const & anIndent) {
  anOstream << anIndent << theCategory->name() << "\n";
};

} //Dbg
} //Flexus

