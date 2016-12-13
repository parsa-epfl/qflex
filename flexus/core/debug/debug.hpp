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
#ifndef FLEXUS_CORE_DEBUG_DEBUG_HPP_INCLUDED
#define FLEXUS_CORE_DEBUG_DEBUG_HPP_INCLUDED

#include <sstream>

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/comparison/less_equal.hpp>
#include <boost/preprocessor/stringize.hpp>

#include <core/debug/debugger.hpp>

#include <core/debug/aux_/state.hpp>
#include <core/debug/aux_/operations.hpp>
#include <core/debug/aux_/process.hpp>

#define DBG_Control()     <core/debug/control.hpp>

#define DBG_(Sev, operations) \
  BOOST_PP_CAT(DBG__Undefined_Severity_Level__,Sev) ( BOOST_PP_CAT(DBG__internal_,Sev) ) ( DBG_internal_Sev_to_int(Sev) , operations)   /**/

#define DBG_Assert(condition, ... ) \
  DBG__internal_Assert( DBG_internal_Sev_to_int(Crit) , condition, __VA_ARGS__ )   /**/

#define DBG_AssertSev(Sev, condition, ... ) \
  BOOST_PP_CAT(DBG__Undefined_Severity_Level__,Sev) ( DBG__internal_Assert ) ( DBG_internal_Sev_to_int(Sev), condition,  __VA_ARGS__ )   /**/

#define DBG__Undefined_Severity_Level__None(Macro) Macro
#define DBG__Undefined_Severity_Level__Tmp(Macro) Macro
#define DBG__Undefined_Severity_Level__Crit(Macro) Macro
#define DBG__Undefined_Severity_Level__Dev(Macro) Macro
#define DBG__Undefined_Severity_Level__Trace(Macro) Macro
#define DBG__Undefined_Severity_Level__Iface(Macro) Macro
#define DBG__Undefined_Severity_Level__Verb(Macro) Macro
#define DBG__Undefined_Severity_Level__VVerb(Macro) Macro
#define DBG__Undefined_Severity_Level__Inv(Macro) Macro

#define DBG__Undefined_Severity_Level__none(Macro) Macro
#define DBG__Undefined_Severity_Level__tmp(Macro) Macro
#define DBG__Undefined_Severity_Level__crit(Macro) Macro
#define DBG__Undefined_Severity_Level__dev(Macro) Macro
#define DBG__Undefined_Severity_Level__trace(Macro) Macro
#define DBG__Undefined_Severity_Level__iface(Macro) Macro
#define DBG__Undefined_Severity_Level__verb(Macro) Macro
#define DBG__Undefined_Severity_Level__vverb(Macro) Macro
#define DBG__Undefined_Severity_Level__inv(Macro) Macro

#define DBG_internal_Sev_to_int(Sev)          \
    BOOST_PP_CAT(DBG_internal_Sev_to_int_,Sev)  /**/

//These defines must match what is set in the tSeverity enumeration in core/debug/severity.hpp
#define DBG_internal_Sev_to_int_None  8
#define DBG_internal_Sev_to_int_Tmp   7
#define DBG_internal_Sev_to_int_Crit  6
#define DBG_internal_Sev_to_int_Dev   5
#define DBG_internal_Sev_to_int_Trace 4
#define DBG_internal_Sev_to_int_Iface 3
#define DBG_internal_Sev_to_int_Verb  2
#define DBG_internal_Sev_to_int_VVerb 1
#define DBG_internal_Sev_to_int_Inv   0

#define DBG_internal_Sev_to_int_none  8
#define DBG_internal_Sev_to_int_tmp   7
#define DBG_internal_Sev_to_int_crit  6
#define DBG_internal_Sev_to_int_dev   5
#define DBG_internal_Sev_to_int_trace 4
#define DBG_internal_Sev_to_int_iface 3
#define DBG_internal_Sev_to_int_verb  2
#define DBG_internal_Sev_to_int_vverb 1
#define DBG_internal_Sev_to_int_inv   0

//No default ops
#define DBG__internal_DEFAULT_OPS

namespace DBG_Cats {
extern Flexus::Dbg::Category Core;
extern bool Core_debug_enabled;
extern Flexus::Dbg::Category Assert;
extern bool Assert_debug_enabled;
extern Flexus::Dbg::Category Stats;
extern bool Stats_debug_enabled;
}

#ifndef SELECTED_DEBUG
#warning "SELECTED_DEBUG not passed in on command line.  Defaulting to Iface"
#define DBG_SetCompileTimeMinSev Iface
#else
#define DBG_SetCompileTimeMinSev SELECTED_DEBUG
#endif
#define DBG_SetCoreDebugging true
#define DBG_SetAssertions true
#include DBG_Control()

#endif //FLEXUS_CORE_DEBUG_DEBUG_HPP_INCLUDED

