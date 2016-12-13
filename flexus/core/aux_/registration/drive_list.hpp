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
#ifndef FLEXUS_CORE_AUX__REGISTRATION_DRIVE_LIST_HPP_INCLUDED
#define FLEXUS_CORE_AUX__REGISTRATION_DRIVE_LIST_HPP_INCLUDED

namespace Flexus {
namespace Core {
namespace Detail {

#define DRIVE_STEP0                                                                             \
    typedef                                                                                       \
      typename                                                                                    \
        mpl::push_front                                                                           \
          < base                                                                                  \
          , DriveHandle< typename DriveListIter::type, ComponentHandle, ComponentHandle::tag >    \
          >::type step1                                                                           /**/

#define DRIVE_STEP(N)                                                                                                   \
    typedef                                                                                                               \
      typename                                                                                                            \
        mpl::push_front                                                                                                   \
          < BOOST_PP_CAT(step,N)                                                                                          \
          , DriveHandle< typename mpl::advance_c<DriveListIter,N>::type::type, ComponentHandle, ComponentHandle::tag >    \
          >::type BOOST_PP_CAT(step,BOOST_PP_INC(N))                                                                      /**/

template < int32_t N, class DriveListIter, class ComponentHandle, class Base>
struct make_drive_list__drive_step {
  typedef
  typename
  make_drive_list__drive_step
  < N - 5
  , typename mpl::advance_c<DriveListIter, 5>::type
  , ComponentHandle
  , Base
  >::type base;
  DRIVE_STEP0;
  DRIVE_STEP(1);
  DRIVE_STEP(2);
  DRIVE_STEP(3);
  DRIVE_STEP(4);
  typedef step5 type;
};

template < class DriveListIter, class ComponentHandle, class Base>
struct make_drive_list__drive_step<5, DriveListIter, ComponentHandle, Base> {
  typedef Base base;
  DRIVE_STEP0;
  DRIVE_STEP(1);
  DRIVE_STEP(2);
  DRIVE_STEP(3);
  DRIVE_STEP(4);
  typedef step5 type;
};

template < class DriveListIter, class ComponentHandle, class Base>
struct make_drive_list__drive_step<4, DriveListIter, ComponentHandle, Base> {
  typedef Base base;
  DRIVE_STEP0;
  DRIVE_STEP(1);
  DRIVE_STEP(2);
  DRIVE_STEP(3);
  typedef step4 type;
};

template < class DriveListIter, class ComponentHandle, class Base>
struct make_drive_list__drive_step<3, DriveListIter, ComponentHandle, Base> {
  typedef Base base;
  DRIVE_STEP0;
  DRIVE_STEP(1);
  DRIVE_STEP(2);
  typedef step3 type;
};

template < class DriveListIter, class ComponentHandle, class Base>
struct make_drive_list__drive_step<2, DriveListIter, ComponentHandle, Base> {
  typedef Base base;
  DRIVE_STEP0;
  DRIVE_STEP(1);
  typedef step2 type;
};

template < class DriveListIter, class ComponentHandle, class Base>
struct make_drive_list__drive_step<1, DriveListIter, ComponentHandle, Base> {
  typedef Base base;
  DRIVE_STEP0;
  typedef step1 type;
};

template < class DriveListIter, class ComponentHandle, class Base>
struct make_drive_list__drive_step<0, DriveListIter, ComponentHandle, Base> {
  typedef Base type;
};

#undef DRIVE_STEP0
#undef DRIVE_STEP

#define COMPONENT_STEP0                                                                                       \
    typedef                                                                                                     \
      typename                                                                                                  \
        make_drive_list__drive_step                                                                             \
          < mpl::size< typename mpl::deref<ComponentListIter>::type::component::DriveInterfaces>::value         \
          , typename mpl::begin<typename mpl::deref<ComponentListIter>::type::component::DriveInterfaces>::type \
          , typename mpl::deref<ComponentListIter>::type                                                        \
          , base                                                                                                \
          >::type step1                                                                                         /**/

#define COMPONENT_STEP(N)                                                                                                                         \
    typedef                                                                                                                                         \
      typename                                                                                                                                      \
        make_drive_list__drive_step                                                                                                                 \
          < mpl::size< typename mpl::deref<typename mpl::advance_c<ComponentListIter,N>::type>::type::component::DriveInterfaces>::value            \
          , typename mpl::begin< typename mpl::deref<typename mpl::advance_c<ComponentListIter,N>::type>::type::component::DriveInterfaces>::type   \
          , typename mpl::deref<typename mpl::advance_c<ComponentListIter,N>::type>::type                                                           \
          , BOOST_PP_CAT(step,N)                                                                                                                    \
          >::type BOOST_PP_CAT(step,BOOST_PP_INC(N))                                                                                                /**/

template < int32_t N, class ComponentListIter>
struct make_drive_list__component_step {
  typedef
  typename
  make_drive_list__component_step
  < N - 5
  , typename mpl::advance_c<ComponentListIter, 5>::type
  >::type base;

  COMPONENT_STEP0;
  COMPONENT_STEP(1);
  COMPONENT_STEP(2);
  COMPONENT_STEP(3);
  COMPONENT_STEP(4);
  typedef step5 type;
};

template < class ComponentListIter>
struct make_drive_list__component_step< 5, ComponentListIter > {
  typedef mpl::vector<> base;
  COMPONENT_STEP0;
  COMPONENT_STEP(1);
  COMPONENT_STEP(2);
  COMPONENT_STEP(3);
  COMPONENT_STEP(4);
  typedef step5 type;
};

template < class ComponentListIter>
struct make_drive_list__component_step< 4, ComponentListIter > {
  typedef mpl::vector<> base;
  COMPONENT_STEP0;
  COMPONENT_STEP(1);
  COMPONENT_STEP(2);
  COMPONENT_STEP(3);
  typedef step4 type;
};

template < class ComponentListIter>
struct make_drive_list__component_step< 3, ComponentListIter > {
  typedef mpl::vector<> base;
  COMPONENT_STEP0;
  COMPONENT_STEP(1);
  COMPONENT_STEP(2);
  typedef step3 type;
};

template < class ComponentListIter>
struct make_drive_list__component_step< 2, ComponentListIter > {
  typedef mpl::vector<> base;
  COMPONENT_STEP0;
  COMPONENT_STEP(1);
  typedef step2 type;
};

template < class ComponentListIter>
struct make_drive_list__component_step< 1, ComponentListIter > {
  typedef mpl::vector<> base;
  COMPONENT_STEP0;
  typedef step1 type;
};

template < class ComponentListIter>
struct make_drive_list__component_step< 0, ComponentListIter > {
  typedef mpl::vector<> type;
};

#undef COMPONENT_STEP0
#undef COMPONENT_STEP

template < class ComponentList >
struct make_drive_list
    : public make_drive_list__component_step< mpl::size<ComponentList>::value, typename mpl::begin<ComponentList>::type>  { };

} //namespace Detail
} //namespace Core
} //namespace Flexus

#endif //FLEXUS_CORE_AUX__REGISTRATION_DRIVE_LIST_HPP_INCLUDED
