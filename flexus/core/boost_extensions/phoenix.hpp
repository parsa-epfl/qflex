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
#ifndef FLEXUS_CORE_BOOST_EXTENSION_PHOENIX_HPP_INCLUDED
#define FLEXUS_CORE_BOOST_EXTENSION_PHOENIX_HPP_INCLUDED

#define PHOENIX_LIMIT 6

#include <boost/spirit/include/phoenix1_functions.hpp>
#include <boost/spirit/include/phoenix1_casts.hpp>
#include <boost/spirit/include/phoenix1_binders.hpp>
// #include <boost/spirit/phoenix/functions.hpp>
// #include <boost/spirit/phoenix/casts.hpp>
// #include <boost/spirit/phoenix/binders.hpp>
#include <boost/type_traits/remove_pointer.hpp>

namespace phoenix {
template <> struct rank<char const *> {
  static int32_t const value = 160;
};

template <> struct rank<char const * const> {
  static int32_t const value = 160;
};

struct new_0_impl {
  template <class Object>
  struct result {
    typedef Object type;
  };

  template <class Object >
  Object operator()(Object const ignored) const {
    return new typename boost::remove_pointer<Object>::type ;
  }
};

struct new_1_impl {
  template <class Object, class Arg1 >
  struct result {
    typedef Object type;
  };

  template <class Object, class Arg1>
  Object operator()(Object const ignored, Arg1 & arg1) const {
    return new typename boost::remove_pointer<Object>::type (arg1) ;
  }
};

struct new_2_impl {
  template <class Object, class Arg1, class Arg2 >
  struct result {
    typedef Object type;
  };

  template <class Object, class Arg1, class Arg2 >
  Object operator()(Object const ignored, Arg1 & arg1, Arg2 & arg2) const {
    return new typename boost::remove_pointer<Object>::type (arg1, arg2) ;
  }
};

struct new_3_impl {
  template <class Object, class Arg1, class Arg2, class Arg3>
  struct result {
    typedef Object type;
  };

  template <class Object, class Arg1, class Arg2, class Arg3 >
  Object operator()(Object const ignored, Arg1 & arg1, Arg2 & arg2, Arg3 & arg3 ) const {
    return new typename boost::remove_pointer<Object>::type (arg1, arg2, arg3) ;
  }
};

phoenix::function<new_0_impl> new_0;
phoenix::function<new_1_impl> new_1;
phoenix::function<new_2_impl> new_2;
phoenix::function<new_3_impl> new_3;

} //phoenix

#endif //FLEXUS_CORE_BOOST_EXTENSION_PHOENIX_HPP_INCLUDED

