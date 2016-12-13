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
#include <core/debug/debug.hpp>

#if defined(DBG_NewCategories)
#error "DBG_NewCategories has been replaced with DBG_DefineCategories"
#endif

#if defined(DBG_SetCompileTimeMinSev)

#undef DBG__internal_tmp_MinimumSeverity
#define DBG__internal_tmp_MinimumSeverity                                                                                   \
    BOOST_PP_CAT(DBG__Undefined_Severity_Level__,DBG_SetCompileTimeMinSev) ( DBG_internal_Sev_to_int) ( DBG_SetCompileTimeMinSev )       /**/

#include <core/debug/aux_/clear_macros.hpp>

#include <core/debug/aux_/tmp_macros.hpp>
#include <core/debug/aux_/crit_macros.hpp>
#include <core/debug/aux_/dev_macros.hpp>
#include <core/debug/aux_/trace_macros.hpp>
#include <core/debug/aux_/iface_macros.hpp>
#include <core/debug/aux_/verb_macros.hpp>
#include <core/debug/aux_/vverb_macros.hpp>
#include <core/debug/aux_/inv_macros.hpp>

#undef DBG_SetCompileTimeMinSev

#endif //DBG_SetCompileTimeMinSev

#if defined(DBG_SetInitialRuntimeMinSev)
#ifdef DBG__internal_GlobalRunTimeMinSev_SET
#error "Runtime global minimum severity may only be set once"
#else
#define DBG__internal_GlobalRunTimeMinSev_SET
#endif

#include <core/debug/aux_/runtime_sev.hpp>

#undef DBG_SetInitialRuntimeMinSev

#endif //DBG_SetGlobalRunTimeMinSev

#if defined(DBG_DefineCategories)

#include <core/debug/aux_/new_categories.hpp>
#undef DBG_DefineCategories

#endif //DBG_NewCategories

#if defined(DBG_DeclareCategories)

#include <core/debug/aux_/declare_new_categories.hpp>
#undef DBG_DeclareCategories

#endif //DBG_DeclareCategories

#if defined(DBG_SetAssertions)

#include <core/debug/aux_/assert_macros.hpp>
#undef DBG_SetAssertions

#endif //DBG_SetMinimumSeverity

#if defined(DBG_SetDefaultOps)

#include <core/debug/aux_/default_ops.hpp>

#endif //DBG_SetDefaultOps

#if defined(DBG_Reset)

#undef DBG_Reset
#include <core/debug/aux_/reset.hpp>

#endif //DBG_SetDefaultOps
