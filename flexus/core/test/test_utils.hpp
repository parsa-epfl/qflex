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
#ifndef FLEXUS_CORE_TEST_TEST_UTILS_HPP_INCLUDED
#define FLEXUS_CORE_TEST_TEST_UTILS_HPP_INCLUDED

#include <boost/test/unit_test.hpp>
#include <boost/preprocessor/iteration/iterate.hpp>
#include <boost/preprocessor/iteration/local.hpp>
#include <boost/preprocessor/repetition/enum.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/arithmetic/dec.hpp>
#include <boost/preprocessor/arithmetic/sub.hpp>

#include <core/debug/debug.hpp>

#include <core/metaprogram.hpp>
#include <core/configuration.hpp>
#include <core/component.hpp>
#include <core/simulator_layout.hpp>
#include <core/ports.hpp>
#include <core/wiring.hpp>

using namespace boost::unit_test_framework;
using namespace Flexus::Core;

//V2 versions of macros
#define FLEXUS_TEST_IO_LIST(COUNT, ...) mpl::vector< __VA_ARGS__ >
#define FLEXUS_TEST_IO_LIST_EMPTY mpl::vector<>
#define FLEXUS_TEST_WIRING template <class Handle, class Wiring>
#define FLEXUS_TEST_CHANNEL(COMPONENT,PORT) Wiring::channel ( static_cast<Handle *>(0), static_cast<PORT *>(0), COMPONENT.flexusIndex() )
#define FLEXUS_TEST_CHANNEL_ARRAY(COMPONENT,PORT,IDX) Wiring::channel_array ( static_cast<Handle *>(0), static_cast<PORT *>(0), (COMPONENT).flexusIndex(), IDX )
#define FLEXUS_TEST_DRIVE_LIST(COUNT, ...) mpl::vector< __VA_ARGS__ >
#define FLEXUS_TEST_DRIVE_LIST_EMPTY mpl::vector< >

#endif //FLEXUS_CORE_TEST_TEST_UTILS_HPP_INCLUDED
