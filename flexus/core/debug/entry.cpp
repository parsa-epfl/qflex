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
#include <string>
#include <set>

#include <utility>
#include <map>
#include <vector>
#include <boost/optional.hpp>
#include <core/boost_extensions/lexical_cast.hpp>

#include <core/debug/entry.hpp>

namespace Flexus {
namespace Dbg {

Entry::Entry(Severity aSeverity, char const * aFile, int64_t aLine, char const * aFunction, int64_t aGlobalCount, int64_t aCycleCount)
  : theSeverity(aSeverity) {
  set("Severity", toString(theSeverity) );
  set("SeverityNumeric", theSeverity );
  std::string path(aFile);
  set("FilePath", path);
  std::string::size_type last_slash(path.rfind('/'));
  if (last_slash == std::string::npos) {
    set("File", path);
  } else {
    set("File", std::string(path, last_slash + 1));
  }
  set("Line", aLine);
  set("Function", std::string(aFunction) + "()" );
  set("GlobalCount", aGlobalCount);
  set("Cycles", aCycleCount);
  set("Categories", "");
}

Entry & Entry::set(std::string const & aFieldName) {
  theFields.insert(aFieldName);
  return *this;
}
Entry & Entry::set(std::string const & aFieldName, std::string const & aFieldValue) {
  std::set<Field>::iterator iter(theFields.insert(aFieldName).first);
  iter->setValue(aFieldValue);
  return *this;
}
Entry & Entry::set(std::string const & aFieldName, int64_t aFieldValue) {
  std::set<Field>::iterator iter(theFields.insert(aFieldName).first);
  iter->setValue(aFieldValue);
  return *this;
}
Entry & Entry::append(std::string const & aFieldName, std::string const & aFieldValue) {
  std::set<Field>::iterator iter(theFields.insert(aFieldName).first);
  iter->setValue( iter->value() + aFieldValue);
  return *this;
}
Entry & Entry::addCategory(Category * aCategory) {
  if (theCategories.insert(aCategory->number()).second) {
    set("Categories", get("Categories") + ' ' + aCategory->name());
  }
  return *this;
}

Entry & Entry::addCategories(CategoryHolder const & aCategoryHolder) {
  CategoryHolder::const_iterator iter(aCategoryHolder.begin());
  while (iter != aCategoryHolder.end()) {
    if (theCategories.insert((*iter)->number()).second) {
      set("Categories", get("Categories") + ' ' + (*iter)->name());
    }
    ++iter;
  }
  return *this;
}

std::string Entry::get(std::string aFieldName) const {
  std::set<Field>::const_iterator iter(theFields.find(aFieldName));
  if (iter != theFields.end()) {
    return iter->value();
  } else {
    return "<undefined>";
  }
}

int64_t Entry::getNumeric(std::string aFieldName) const  {
  std::set<Field>::const_iterator iter = theFields.find(aFieldName);
  if (iter != theFields.end()) {
    return iter->numericValue();
  } else {
    return 0;
  }
}

bool Entry::exists(std::string aFieldName) const  {
  return (theFields.find(aFieldName) != theFields.end());
}

bool Entry::hasCategory(Category const * aCategory) const  {
  return (theCategories.find(aCategory->number()) != theCategories.end());
}

} //Dbg
} //Flexus

