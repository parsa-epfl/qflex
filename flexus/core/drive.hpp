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
#ifndef FLEXUS_DRIVE_HPP_INCLUDED
#define FLEXUS_DRIVE_HPP_INCLUDED

#include <core/metaprogram.hpp>
#include <boost/mpl/deref.hpp>
#include <core/drive_reference.hpp>
#include <core/performance/profile.hpp>

namespace Flexus {
namespace Core {

namespace aux_ {

template <int32_t N, class DriveHandleIter>
struct do_cycle_step {
  static void doCycle() {
    {
      FLEXUS_PROFILE_N( mpl::deref<DriveHandleIter>::type::drive::name() );
      for (index_t i = 0; i < mpl::deref<DriveHandleIter>::type::width(); i++) {
        mpl::deref<DriveHandleIter>::type::getReference(i).drive( typename mpl::deref<DriveHandleIter>::type::drive () );
      }
    }
    do_cycle_step < N - 1, typename mpl::next<DriveHandleIter>::type >::doCycle();
  }
};

template <class DriveHandleIter>
struct do_cycle_step<0, DriveHandleIter> {
  static void doCycle() { }
};

template <class DriveHandles>
struct do_cycle {
  static void doCycle() {
    do_cycle_step< mpl::size<DriveHandles>::value, typename mpl::begin<DriveHandles>::type>::doCycle();
  }
};
}

namespace aux_ {
template <int32_t N, class DriveHandleIter>
struct list_drives_step {
  static void listDrives() {
    DBG_( Dev, ( << mpl::deref<DriveHandleIter>::type::drive::name() ) );
    list_drives_step < N - 1, typename mpl::next<DriveHandleIter>::type >::listDrives();
  }
};

template <class DriveHandleIter>
struct list_drives_step<0, DriveHandleIter> {
  static void listDrives() { }
};

template <class DriveHandles>
struct list_drives {
  static void listDrives() {
    DBG_( Dev, ( << "Ordered list of DriveHandles follows:" )) ;
    list_drives_step< mpl::size<DriveHandles>::value, typename mpl::begin<DriveHandles>::type>::listDrives();
  }
};
}

template < class OrderedDriveHandleList >
class Drive : public DriveBase {
  virtual void doCycle() {
    //Through the magic of template expansion and static dispatch, this calls every Drive's
    //do_cycle() method in the order specified in OrderedDriveHandleList.
    FLEXUS_PROFILE_N("Drive::doCycle");
    aux_::do_cycle<OrderedDriveHandleList>::doCycle();
  }

};

} //End Namespace Core
} //namespace Flexus

#endif //FLEXUS_DRIVE_HPP_INCLUDE
