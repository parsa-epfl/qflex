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
#if !defined(BOOST_CIRCULAR_BUFFER_HPP)
#define BOOST_CIRCULAR_BUFFER_HPP

#if defined(_MSC_VER) && _MSC_VER >= 1200
#pragma once
#endif

#include <boost/type_traits.hpp>
#include <boost/detail/workaround.hpp>

#if defined(NDEBUG) || defined(BOOST_DISABLE_CB_DEBUG)
#define BOOST_CB_ASSERT(Expr) ((void)0)
#define BOOST_CB_ENABLE_DEBUG 0
#else
#include <boost/assert.hpp>
#define BOOST_CB_ASSERT(Expr) BOOST_ASSERT(Expr)
#define BOOST_CB_ENABLE_DEBUG 1
#endif

#if BOOST_WORKAROUND(__BORLANDC__, <= 0x0550) || \
    BOOST_WORKAROUND(__MWERKS__, <= 0x2407) || \
    BOOST_WORKAROUND(BOOST_MSVC, <= 1300)
#define BOOST_CB_IS_CONVERTIBLE(Iterator, Type) ((void)0)
#else
#include <boost/static_assert.hpp>
#include <boost/detail/iterator.hpp>
#define BOOST_CB_IS_CONVERTIBLE(Iterator, Type) \
        BOOST_STATIC_ASSERT((is_convertible<typename detail::iterator_traits<Iterator>::value_type, Type>::value));
#endif

#if !defined(BOOST_NO_EXCEPTIONS)
#define BOOST_CB_TRY try {
#define BOOST_CB_UNWIND(action) } catch(...) { action; throw; }
#else
#define BOOST_CB_TRY {
#define BOOST_CB_UNWIND(action) }
#endif

#include "circular_buffer_fwd.hpp"
#include "circular_buffer/debug.hpp"
#include "circular_buffer/details.hpp"
#include "circular_buffer/base.hpp"
#include "circular_buffer/adaptor.hpp"

#undef BOOST_CB_UNWIND
#undef BOOST_CB_TRY
#undef BOOST_CB_IS_CONVERTIBLE
#undef BOOST_CB_ENABLE_DEBUG
#undef BOOST_CB_ASSERT

#endif // #if !defined(BOOST_CIRCULAR_BUFFER_HPP)
