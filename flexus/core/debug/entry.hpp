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
#ifndef FLEXUS_CORE_DEBUG_ENTRY_HPP_INCLUDED
#define FLEXUS_CORE_DEBUG_ENTRY_HPP_INCLUDED

#include <set>

#include <core/debug/field.hpp>
#include <core/debug/severity.hpp>
#include <core/debug/category.hpp>

namespace Flexus {
namespace Dbg {

class Entry {
  std::set<Field> theFields;
  std::set<int> theCategories;
  Severity theSeverity;
public:
  Entry(Severity aSeverity, char const * aFile, int64_t aLine, char const * aFunction, int64_t aGlobalCount, int64_t aCycleCount);
  Entry & set(std::string const & aFieldName);
  Entry & set(std::string const & aFieldName, std::string const & aFieldValue);
  Entry & set(std::string const & aFieldName, int64_t aFieldValue);
  Entry & append(std::string const & aFieldName, std::string const & aFieldValue);
  Entry & addCategory(Category * aCategory);
  Entry & addCategories(CategoryHolder const & aCategoryHolder);
  std::string get(std::string aFieldName) const;
  int64_t getNumeric(std::string aFieldName) const;
  bool exists(std::string aFieldName) const;
  bool hasCategory(Category const * aCategory) const;

};

} //Dbg
} //Flexus

#endif //FLEXUS_CORE_DEBUG_ENTRY_HPP_INCLUDED

