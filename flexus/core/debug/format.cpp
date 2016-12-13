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
#include <iostream>
#include <algorithm>

#include <core/debug/format.hpp>

namespace Flexus {
namespace Dbg {

void translateEscapes(std::string & aString) {
  std::string clean_value(aString);
  std::string::size_type loc(0);
  while ( ( loc = clean_value.find('\\', loc)) != std::string::npos) {
    switch (clean_value.at(loc + 1)) {
      case '\'':
        clean_value.replace(loc, 2, "\'");
        break;
      case '"':
        clean_value.replace(loc, 2, "\"");
        break;
      case '?':
        clean_value.replace(loc, 2, "\?");
        break;
      case '\\':
        clean_value.replace(loc, 2, "\\");
        break;
      case 'a':
        clean_value.replace(loc, 2, "\a");
        break;
      case 'b':
        clean_value.replace(loc, 2, "\b");
        break;
      case 'f':
        clean_value.replace(loc, 2, "\b");
        break;
      case 'n':
        clean_value.replace(loc, 2, "\n");
        break;
      case 'r':
        clean_value.replace(loc, 2, "\r");
        break;
      case 't':
        clean_value.replace(loc, 2, "\t");
        break;
      default:
        //Do nothing
        break;
    }
    ++loc;
  }

  aString.swap(clean_value);
}

StaticFormat::StaticFormat(std::string const & aValue)
  : theValue(aValue) {
  //Replace all escape sequences in the format.  This functionality is broken in the Spirit parser
  translateEscapes(theValue);
}

void StaticFormat::format(std::ostream & anOstream, Entry const & anEntry) const {
  anOstream << theValue;
}

void StaticFormat::printConfiguration(std::ostream & anOstream, std::string const & anIndent) const {
  std::string clean_value(theValue);
  std::string::size_type loc(0);
  while ( ( loc = clean_value.find('\n', loc)) != std::string::npos) {
    clean_value.replace(loc, 1, "\\n");
  }
  anOstream << anIndent << " \"" << clean_value << '\"';
}

FieldFormat::FieldFormat(std::string const & aField)
  : theField(aField)
{}

void FieldFormat::format(std::ostream & anOstream, Entry const & anEntry) const {
  anOstream << anEntry.get(theField);
}
void FieldFormat::printConfiguration(std::ostream & anOstream, std::string const & anIndent) const {
  anOstream << anIndent << " {" << theField << '}';
}

void CompoundFormat::destruct() {
  for(auto* aFormat: theFormats){
    delete aFormat;
  } //Clean up all pointers owned by theFormats
}

//Assumes ownership of the passed in Format
void CompoundFormat::add(Format * aFormat) {
  theFormats.push_back(aFormat); //Ownership assumed by theFormats
}

void CompoundFormat::printConfiguration(std::ostream & anOstream, std::string const & anIndent) const {
  for(auto* aFormat: theFormats){
    aFormat->printConfiguration(anOstream, anIndent);
  }
}

void CompoundFormat::format(std::ostream & anOstream, Entry const & anEntry) const {
  for(auto* aFormat: theFormats){
    aFormat->format(anOstream, anEntry);
  }
}

} //Dbg
} //Flexus
