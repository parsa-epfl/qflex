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
#ifndef FLEXUS_CORE_DEBUG_FIELD_HPP_INCLUDED
#define FLEXUS_CORE_DEBUG_FIELD_HPP_INCLUDED

#include <utility>
#include <string>
#include <boost/optional.hpp>

namespace Flexus {
namespace Dbg {

using namespace std::rel_ops;

class Field {
  std::string theName;
  //These members are declared mutable for 2 reasons:
  //1 - the numeric to string conversion for theTextValue is done
  //    when value() is called, and value needs to be a const member.
  //2 - Changing these members does not affect the ordering of Fields
  mutable boost::optional<std::string> theTextValue;
  mutable boost::optional<int64_t> theNumericValue;

public:
  bool operator ==(Field const & aField) const;
  bool operator < (Field const & aField) const;
  Field(std::string const & aName);

  //This method is declared const because it does not affecgt the ordering
  //of Fields
  void setValue(std::string const & aString) const;

  //This method is declared const because it does not affecgt the ordering
  //of Fields
  void setValue(int64_t aValue) const;
  bool isNumeric() const;
  std::string const & value() const;
  int64_t numericValue() const;
};

} //Dbg
} //Flexus

#endif //FLEXUS_CORE_DEBUG_FIELD_HPP_INCLUDED

