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
#ifndef FLEXUS_CORE_DEBUG_AUX__PROCESS_HPP_INCLUDED
#define FLEXUS_CORE_DEBUG_AUX__PROCESS_HPP_INCLUDED

#include <core/boost_extensions/va.h>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/seq/elem.hpp>
#include <boost/preprocessor/punctuation/comma.hpp>
#include <boost/preprocessor/punctuation/paren.hpp>
#include <boost/preprocessor/logical/and.hpp>
#include <boost/preprocessor/logical/not.hpp>
#include <boost/preprocessor/control/if.hpp>

#define DBG__internal_FIRST(x,y) x

//#define DBG__internal_REMAP_COMMAND_Unnamed(x) Unnamed, x,
//#define DBG__internal_REMAP_COMMAND_Named(x)   Named, x,
//#define DBG__internal_VALID_COMMAND_Named ()
//#define DBG__internal_MakeOutput__Unnamed(x,st) {out x}
//#define DBG__internal_MakeOutput_Named(x,st) {name out x}
//#define DBG__internal_NextState_Unnamed(x,st) st
//#define DBG__internal_NextState_Named(x,st) st

#define DBG__internal_IS_UNNAMED(x) DBG__internal_IS_UNNAMED_D0( DBG__internal_IS_UNNAMED_I(x))
#define DBG__internal_IS_UNNAMED_D0(x) DBG__internal_FIRST(x)

#define DBG__internal_IS_UNNAMED_I(x) DBG__internal_IS_UNNAMED_I_D(DBG__internal_IS_UNNAMED_T x)
#define DBG__internal_IS_UNNAMED_I_D(x) BOOST_PP_CAT(DBG__internal_IsUnnamed_,x)

#define DBG__internal_IS_UNNAMED_T(x) 1
#define DBG__internal_IsUnnamed_DBG__internal_IS_UNNAMED_T 0,
#define DBG__internal_IsUnnamed_1 1,

//{__DBG__internal_IS_UNNAMED__} DBG__internal_IS_UNNAMED( (1) (2) (3) ) === 1
//{__DBG__internal_IS_UNNAMED__} DBG__internal_IS_UNNAMED( Named(1) (2) (3) ) === 0

#define DBG__internal_make_cmd( cmd, input, state) BOOST_PP_CAT(DBG__internal_make_cmd_,DBG__internal_IS_UNNAMED(input)) (cmd, input,state)

//Front output command for the unnamed command
#define DBG__internal_make_cmd_1( cmd, input, state) DBG__internal_make_cmd_named(cmd, state, BOOST_PP_CAT(DBG__internal_REMAP_COMMAND_,Unnamed input))

//Front output command for any named command
#define DBG__internal_make_cmd_0( cmd, input, state) DBG__internal_make_cmd_named(cmd, state, BOOST_PP_CAT(DBG__internal_REMAP_COMMAND_,input))

#define DBG__internal_make_cmd_named( cmd, state, expanding ) DBG__internal_make_cmd_named_d(cmd, state, expanding)
#define DBG__internal_make_cmd_named_d( cmd, state, name, args, junk) BOOST_PP_CAT(cmd,name)(args,state)

//{__DBG__internal_make_cmd__} DBG__internal_make_cmd( Output_, (1) (2) (3),st ) === Output_Unnamed(1,st)
//{__DBG__internal_make_cmd__} DBG__internal_make_cmd( DBG__internal_NextState_, Named(1) (2) (3), st ) === DBG__internal_NextState_Named(1,st)

#define DBG__internal_remove_front(input) BOOST_PP_CAT(DBG__internal_remove_front_,DBG__internal_IS_UNNAMED(input)) (input)

//Unnnamed
#define DBG__internal_remove_front_1( input ) DBG__internal_remove_front_named(BOOST_PP_CAT(DBG__internal_REMAP_COMMAND_,Unnamed input))

#define DBG__internal_remove_front_0( input ) DBG__internal_remove_front_named(BOOST_PP_CAT(DBG__internal_REMAP_COMMAND_,input))

#define DBG__internal_remove_front_named( expanding ) DBG__internal_remove_front_named_d( expanding )
#define DBG__internal_remove_front_named_d( name, front, rest) rest

//{__DBG__internal_remove_front__} DBG__internal_remove_front( (1) (2) (3) ) === (2) (3)
//{__DBG__internal_remove_front__} DBG__internal_remove_front( Named(1) (2) (3) ) === (2) (3)

#define DBG__internal_recognize(input) BOOST_PP_CAT(DBG__internal_recognize_,DBG__internal_IS_UNNAMED(input))(input)
#define DBG__internal_recognize_1(input) 1
#define DBG__internal_recognize_0(input) DBG__internal_recognize_0_D( DBG__internal_recognize_test BOOST_PP_CAT(DBG__internal_VALID_COMMAND_,input) )
#define DBG__internal_recognize_0_D(test) DBG__internal_recognize_named( test )
#define DBG__internal_recognize_named(recognize) DBG__internal_recognize_named_D( BOOST_PP_CAT(DBG__internal_recognize_test_,recognize) )
#define DBG__internal_recognize_named_D(recognize) DBG__internal_FIRST( recognize )
#define DBG__internal_recognize_test() 1
#define DBG__internal_recognize_test_DBG__internal_recognize_test 0,
#define DBG__internal_recognize_test_1 1,

//{__recognize__} DBG__internal_recognize( (1) (2) (3) ) === 1
//{__recognize__} DBG__internal_recognize( Named(1) (2) (3) ) === 1
//{__recognize__} DBG__internal_recognize( Unknown(1) (2) (3) ) === 0

#define DBG__internal_INPUT_OK(d, state)                          \
     DBG__internal_INPUT_OK_input( BOOST_PP_TUPLE_ELEM(3,2,state) ) /**/

#define DBG__internal_INPUT_OK_input(input)                     \
     BOOST_PP_CAT( DBG__internal_INPUT_OK_,                       \
        BOOST_PP_NOT(                                             \
          BOOST_PP_VA_IS_EMPTY(input)                             \
        )                                                         \
     ) (input)                                                    /**/

#define DBG__internal_INPUT_OK_1(input)  DBG__internal_INPUT_OK_check_token(input)
#define DBG__internal_INPUT_OK_0(input)  0

#define DBG__internal_INPUT_OK_EAT(...)
#define DBG__internal_INPUT_OK_EAT_2(...) DBG__internal_INPUT_OK_EAT

#define DBG__internal_INPUT_OK_check_token(input) BOOST_PP_CAT(DBG__internal_INPUT_OK_recognize_,DBG__internal_recognize(input))
#define DBG__internal_INPUT_OK_recognize_0 0 (,;DBG__ERROR_Unrecognized_Operation_In_DBG_Statement;) DBG__internal_INPUT_OK_EAT_2
#define DBG__internal_INPUT_OK_recognize_1 1

#define DBG__internal_PROCESS_INITIAL_STATE(Sev, input)  ( , DBG__internal_OPERATION_INITIAL_STATE(Sev) , input )

#define DBG__internal_PROCESS_STEP(d, state)                                                  \
    DBG__internal_PROCESS_STEP_D(state)                                                          /**/

#define DBG__internal_PROCESS_STEP_D(state)                                                      \
    ( /* previous output*/                                                                        \
          BOOST_PP_TUPLE_ELEM(3,0,state)                                                          \
      /* output command for the next token */                                                     \
          DBG__internal_make_cmd( DBG__internal_MakeOutput_, BOOST_PP_TUPLE_ELEM(3,2,state), BOOST_PP_TUPLE_ELEM(3,1,state))      \
    , /* next state command given next token and present state */                                 \
      DBG__internal_make_cmd( DBG__internal_NextState_, BOOST_PP_TUPLE_ELEM(3,2,state), BOOST_PP_TUPLE_ELEM(3,1,state))       \
    , /*remove first token from input*/                                                           \
      DBG__internal_remove_front(BOOST_PP_TUPLE_ELEM(3,2,state))                                                \
    )                                                                                             /**/

#define DBG__internal_PROCESS( Sev, input )               \
      BOOST_PP_WHILE                                        \
        ( DBG__internal_INPUT_OK                            \
        , DBG__internal_PROCESS_STEP                        \
        , DBG__internal_PROCESS_INITIAL_STATE(Sev,input)    \
        )                                                   /**/

//{__DBG__internal_PROCESS__} DBG__internal_PROCESS( xx, (Foo) )
//{__DBG__internal_PROCESS__} DBG__internal_PROCESS( xx, (Foo) Named(Bar) (Baz) )
//{__DBG__internal_PROCESS__} DBG__internal_PROCESS( xx, (Bar) BadName(Foo) (Foo) )

//Not currently supported
#define DBG__internal_PROCESS_DEFAULT_OPS( output, State, empty )                                                               \
    BOOST_PP_CAT(DBG__internal_PROCESS_DEFAULT_OPS_,DBG__internal_State_GetSuppressDefaultOps(State) ) ( output, State, empty)    /**/

#define DBG__internal_PROCESS_DEFAULT_OPS_0( output, State, empty ) \
      BOOST_PP_WHILE                                                  \
        ( DBG__internal_INPUT_OK                                      \
        , DBG__internal_PROCESS_STEP                                  \
        , ( output,State, DBG__internal_DEFAULT_OPS )                 \
        )                                                             /**/

//Default ops are suppressed
#define DBG__internal_PROCESS_DEFAULT_OPS_1( output, State, empty ) \
        ( output, State, empty )                                      /**/

#define DBG__internal_FINALIZE( output, state, empty )                                                      \
    if ( Flexus::Dbg::Debugger::theDebugger->theMinimumSeverity <= DBG__internal_State_GetSev(state)          \
       &&  ( DBG__internal_ASSERTIONS_ENABLED || ! DBG__internal_State_GetAssert(state) ) ) {                 \
       if ( DBG__internal_BUILTIN_CONDITIONS(state) && DBG__internal_State_GetCondition(state) ) {            \
          using namespace DBG_Cats;                                                                           \
          using DBG_Cats::Core;                                                                               \
          Flexus::Dbg::Entry entry__                                                                          \
            ( /*Severity*/ Flexus::Dbg::Severity(DBG__internal_State_GetSev(state))                           \
            , /*File*/ __FILE__                                                                               \
            , /*Line*/ __LINE__                                                                               \
            , /*Function*/ __FUNCTION__                                                                       \
            , /*GlobalCount*/ Flexus::Dbg::Debugger::theDebugger->count()                                     \
            , /*CycleCount*/ Flexus::Dbg::Debugger::theDebugger->cycleCount()                                 \
            );                                                                                                \
         /* allow all debug methods to mutate entry*/ (void) (entry__) output;                                \
         BOOST_PP_CAT(DBG__internal_FINALIZE_Categories_,DBG__internal_State_GetHasCategories(state))(state)  \
         Flexus::Dbg::Debugger::theDebugger->process(entry__);                                                \
      }                                                                                                       \
    } do {} while(0) /*require a semicolon after DBG_ statements */                                           /**/

#define DBG__internal_FINALIZE_Categories_0( state ) /* nothing */

#define DBG__internal_FINALIZE_Categories_1( state )                                                        \
        (entry__).addCategories( DBG__internal_State_GetCategories(state) );                                   /**/

#define DBG__internal_BUILTIN_CONDITIONS( state )           \
      ( DBG__internal_State_GetCategoryCondition( state ) )   /**/

#define DBG__internal_PROCESS_DBG( Sev, input )                       \
    DBG__internal_PROCESS_DBG_0( DBG__internal_PROCESS(Sev, input) )    /**/

#define DBG__internal_PROCESS_DBG_0( delay )   DBG__internal_PROCESS_DBG_1( DBG__internal_PROCESS_DEFAULT_OPS delay )

#define DBG__internal_PROCESS_DBG_1( delay )   DBG__internal_FINALIZE delay

#endif //FLEXUS_CORE_DEBUG_AUX__PROCESS_HPP_INCLUDED

/*#include <core/debug/aux_/state.hpp>
#include <core/debug/aux_/operations.hpp>

#pragma wave trace(enable)
DBG__internal_PROCESS(Crit, Condition( Foo )  Core() )
#pragma wave trace(disable)
*/
