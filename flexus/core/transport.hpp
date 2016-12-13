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
#ifndef FLEXUS_TRANSPORT_HPP_INCLUDED
#define FLEXUS_TRANSPORT_HPP_INCLUDED

#include <core/metaprogram.hpp>
#include <boost/mpl/contains.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/transform.hpp>

#include <core/boost_extensions/intrusive_ptr.hpp>

namespace Flexus {
namespace Core {

template <class Tag, class Slice>
struct transport_entry {
  typedef Tag tag;
  typedef Slice slice;
};

namespace aux_ {
struct extract_tag {
  template <class Slice>
  struct apply;

  template <class Tag, class Data>
  struct apply< transport_entry<Tag, Data> > {
    typedef Tag type;
  };
};

template <class Transport, class OtherTransport>
struct transport_copier {
  Transport & theDest;
  OtherTransport & theSrc;

  transport_copier(Transport & aDest, OtherTransport & aSrc)
    : theDest(aDest)
    , theSrc(aSrc)
  {}

  template <class Slice, bool IsMatch>
  struct copy {
    void operator()(Transport & theDest, OtherTransport & theSrc) {}
  };

  template <class Slice>
  struct copy<Slice, true> {
    void operator()(Transport & theDest, OtherTransport & theSrc) {
      theDest.set(Slice::tag(), theSrc[Slice::tag()]);
    }
  };

  template <class Slice>
  void operator()(Slice const &) {
    copy<Slice, mpl::contains< Transport, typename Slice::tag >::type::value>() (theDest, theSrc);
  }
};

//This template function does the copy.  As an added bonus, it can deduce its template
//argument types, so its easy to call.
template <class Transport, class OtherTransport>
void transport_copy(Transport & aDest, OtherTransport & aSrc) {
  transport_copier<Transport, OtherTransport> copier(aDest, aSrc);
  mpl::for_each<typename mpl::transform<typename OtherTransport::slice_vector, extract_tag> >(copier);
}
}

namespace aux_ {
template <int32_t N, class SliceIter>
class TransportSlice : public TransportSlice < N - 1, typename mpl::next<SliceIter>::type > {
  typedef TransportSlice < N - 1, typename mpl::next<SliceIter>::type > base;
  typedef typename mpl::deref<SliceIter>::type::tag tag;
  typedef typename mpl::deref<SliceIter>::type::slice data;
  boost::intrusive_ptr< data > theObject;
public:
  using base::operator [];
  using base::set;
  boost::intrusive_ptr<data> operator[](const tag &) {
    return theObject;
  }
  boost::intrusive_ptr<const data> operator[](const tag &) const {
    return theObject;
  }
  void set(const tag &, boost::intrusive_ptr<data> anObject) {
    theObject = anObject;
  }
};

template <class SliceIter>
class TransportSlice<0, SliceIter> {
public:
  struct Unique {};
  //Ensure that using declarations compile
  int32_t operator[](const Unique &);
  void set(const Unique &);
};
}

template < class SliceVector >
struct Transport
    : public aux_::TransportSlice< mpl::size<SliceVector>::value, typename mpl::begin<SliceVector>::type > {
  typedef aux_::TransportSlice< mpl::size<SliceVector>::value, typename mpl::begin<SliceVector>::type > base;
public:
  typedef SliceVector slice_vector;

  //Default constructor - default construct all contained pointers
  Transport() {}

  ~Transport() {}

  using base::operator [];
  using base::set;

  //Copy constructor for a non-matching typelist
  template < class OtherSlices >
  Transport( Transport<OtherSlices> const & anOtherTransport) {
    //Note: On entry to the constructor, all the members of this transport
    //have been default constructed, and are therefore null
    aux_::transport_copy(*this, anOtherTransport);
  }

};

}
}

namespace Flexus {
namespace SharedTypes {

using Flexus::Core::Transport;
using Flexus::Core::transport_entry;

} //SharedTypes
} //Flexus

#endif //FLEXUS_TRANSPORT_HPP_INCLUDED

