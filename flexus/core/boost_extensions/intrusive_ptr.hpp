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
#ifndef FLEXUS_CORE_BOOST_EXTENSIONS_INTRUSIVE_PTR_HPP_INCLUDED
#define FLEXUS_CORE_BOOST_EXTENSIONS_INTRUSIVE_PTR_HPP_INCLUDED

#include <boost/intrusive_ptr.hpp>
#include <boost/lambda/lambda.hpp>

namespace boost {

struct counted_base {
  mutable int32_t theRefCount;

  counted_base() : theRefCount(0) {}
  virtual ~counted_base() {}
};

template <class T>
void intrusive_ptr_add_ref(T * p) {
  static_cast<boost::counted_base const *>(p)->theRefCount++;
}

template <class T>
void intrusive_ptr_release(T * p) {
  if ( -- static_cast<boost::counted_base const *>(p)->theRefCount == 0) {
    delete p;
  }
}

}

namespace boost {
namespace serialization {
template<class Archive, class T>
void save(Archive & ar, ::boost::intrusive_ptr<T> const & ptr, uint32_t version) {
  T * t = ptr.get();
  ar & t;
}

template<class Archive, class T>
void load(Archive & ar, ::boost::intrusive_ptr<T> & ptr, uint32_t version) {
  T * t;
  ar & t;
  ptr = boost::intrusive_ptr<T> (t);
}

template<class Archive, class T>
inline void serialize( Archive & ar, ::boost::intrusive_ptr<T> & t, const uint32_t file_version ) {
  split_free(ar, t, file_version);
}
}
}

namespace boost {
namespace lambda {
namespace detail {

template <class A> struct contentsof_type< boost::intrusive_ptr< A > >  {
  typedef A & type;
};

}
}
}

#endif //FLEXUS_CORE_BOOST_EXTENSIONS_INTRUSIVE_PTR_HPP_INCLUDED

