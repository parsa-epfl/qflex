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
#ifndef FLEXUS_CORE_DEBUG_AUX__OPERATIONS_HPP_INCLUDED
#define FLEXUS_CORE_DEBUG_AUX__OPERATIONS_HPP_INCLUDED

#define DBG__internal_STREAM_TRICK( args ) \
  static_cast<std::stringstream &> ( std::stringstream() << std::dec args ).str()

#define DBG__internal_VALID_COMMAND_Message ()
#define DBG__internal_VALID_COMMAND_Msg ()
#define DBG__internal_REMAP_COMMAND_Message(arg) Message, arg,
#define DBG__internal_REMAP_COMMAND_Msg(arg) Message, arg,
//Remap the unnamed debug operation to Message
#define DBG__internal_REMAP_COMMAND_Unnamed(arg) Message, arg,

#define DBG__internal_MakeOutput_Message(args, state)                                                     \
      BOOST_PP_CAT(DBG__internal_MakeOutput_Message_, DBG__internal_State_GetNoMessage(state)) (args,state)   /**/

/* first time message has been called */
#define DBG__internal_MakeOutput_Message_1(args, state)         \
        /*entry*/.set("Message",  DBG__internal_STREAM_TRICK(args) )  /**/

/* subsequent invocations of message */
#define DBG__internal_MakeOutput_Message_0(args, state)             \
        /*entry*/.append("Message",  DBG__internal_STREAM_TRICK(args) )   /**/

#define DBG__internal_NextState_Message(args, state)  \
      DBG__internal_State_SetNoMessage(state, 0)          /**/

#define DBG__internal_VALID_COMMAND_Set ()
#define DBG__internal_REMAP_COMMAND_Set(arg) Set, arg,

#define DBG__internal_Set_FRONT_Wrap(x) x,

#define DBG__internal_Set_FRONT( args )  DBG__internal_Set_FRONT_D( DBG__internal_Set_FRONT_Wrap args )
#define DBG__internal_Set_FRONT_D( args )  DBG__internal_FIRST( args )

#define DBG__internal_MakeOutput_Set(args, state)                                       \
        DBG__internal_MakeOutput_Set_D( DBG__internal_Set_FRONT(args), DBG__internal_remove_front(args) )   /**/

#define DBG__internal_MakeOutput_Set_D(name, val)                                     \
        /*entry*/.set( BOOST_PP_STRINGIZE(name) ,  DBG__internal_STREAM_TRICK( val ) )  /**/

#define DBG__internal_NextState_Set(args, state)  state

#define DBG__internal_VALID_COMMAND_SetNumeric ()
#define DBG__internal_REMAP_COMMAND_SetNumeric(arg) SetNumeric, arg,

#define DBG__internal_MakeOutput_SetNumeric(args, state)                                                              \
        DBG__internal_MakeOutput_SetNumeric_D( DBG__internal_Set_FRONT(args), DBG__internal_remove_front(args) )   /**/

#define DBG__internal_MakeOutput_SetNumeric_D(name, val)    \
        /*entry*/.set( BOOST_PP_STRINGIZE(name) ,  val )        /**/

#define DBG__internal_NextState_SetNumeric(args, state)  state

#define DBG__internal_VALID_COMMAND_Category ()
#define DBG__internal_VALID_COMMAND_Cat ()
#define DBG__internal_VALID_COMMAND_AddCat ()
#define DBG__internal_VALID_COMMAND_AddCategory ()
#define DBG__internal_REMAP_COMMAND_Category(arg) AddCategory, arg,
#define DBG__internal_REMAP_COMMAND_Cat(arg) AddCategory, arg,
#define DBG__internal_REMAP_COMMAND_AddCategory(arg) AddCategory, arg,
#define DBG__internal_REMAP_COMMAND_AddCat(arg) AddCategory, arg,

#define DBG__internal_MakeOutput_AddCategory(args, state)  /* No output */

#define DBG__internal_NextState_AddCategory(args, state)                                                         \
      BOOST_PP_CAT(DBG__internal_NextState_AddCategory_, DBG__internal_State_GetHasCategories(state)) (args,state)   /**/

/* append to the existing categories */
#define DBG__internal_NextState_AddCategory_1(args, state)                                                     \
        DBG__internal_NextState_AddCategory_1_Condition( args, DBG__internal_State_AddCategories( state, args ) )    /**/

#define DBG__internal_NextState_AddCategory_1_Condition(args, state)                                           \
        DBG__internal_State_AddCategoryCondition( state, DBG_Cats:: BOOST_PP_CAT(args,_debug_enabled) )              /**/

#define DBG__internal_NextState_AddCategory_0(args, state)                                                     \
        DBG__internal_NextState_AddCategory_0_Condition( args, DBG__internal_State_SetCategories(state, args) )      /**/

#define DBG__internal_NextState_AddCategory_0_Condition(args, state)                                                                           \
        DBG__internal_NextState_AddCategory_0_D(  DBG__internal_State_SetCategoryCondition( state, DBG_Cats:: BOOST_PP_CAT(args,_debug_enabled) ) )  /**/

/* Indicate that there are categories */
#define DBG__internal_NextState_AddCategory_0_D(state)        \
        DBG__internal_State_SetHasCategories( state, 1 )            /**/

/* add to or initialize the categories for this object */

#define DBG__internal_VALID_COMMAND_Condition ()
#define DBG__internal_VALID_COMMAND_Cond ()
#define DBG__internal_REMAP_COMMAND_Condition(arg) Condition, arg,
#define DBG__internal_REMAP_COMMAND_Cond(arg) Condition, arg,

#define DBG__internal_MakeOutput_Condition(args, state)                     \
        /*entry*/.append("Condition",  BOOST_PP_STRINGIZE(args) )   /**/

#define DBG__internal_NextState_Condition(args, state)                                                         \
      BOOST_PP_CAT(DBG__internal_NextState_Condition_, DBG__internal_State_GetHasCondition(state)) (args,state)   /**/

/* append to the existing categories */
#define DBG__internal_NextState_Condition_1(args, state)   \
        DBG__internal_State_AddCondition( state, args )          /**/

/* No existing categories, use the Cat implementation */
#define DBG__internal_NextState_Condition_0(args, state)                                    \
        DBG__internal_NextState_Condition_0_D( DBG__internal_State_SetCondition(state, args ) )   /**/

#define DBG__internal_NextState_Condition_0_D(state)     \
        DBG__internal_State_SetHasCondition(state, 1 )         /**/

#define DBG__internal_VALID_COMMAND_Core ()
#define DBG__internal_REMAP_COMMAND_Core(arg) Core, arg,

#define DBG__internal_MakeOutput_Core(args, state)  /* No output */

#define DBG__internal_NextState_Core(args, state)     \
      DBG__internal_NextState_AddCategory(Core, state)    /**/

#define DBG__internal_VALID_COMMAND_Assert ()
#define DBG__internal_REMAP_COMMAND_Assert(arg) Assert, arg,

#define DBG__internal_MakeOutput_Assert(args, state)  /* No output */

#define DBG__internal_NextState_Assert(args, state)                                       \
      DBG__internal_NextState_Assert_0( DBG__internal_NextState_AddCategory(Assert, state) )  /**/

#define DBG__internal_NextState_Assert_0(state)                                           \
      DBG__internal_State_SetAssert( state, 1 )                                               /**/

#define DBG__internal_VALID_COMMAND_NoDefaultOps ()
#define DBG__internal_REMAP_COMMAND_NoDefaultOps(arg) NoDefaultOps, arg,

#define DBG__internal_MakeOutput_NoDefaultOps(args, state)  /* No output */

#define DBG__internal_NextState_NoDefaultOps(args, state)                                   \
      DBG__internal_State_SetSuppressDefaultOps( state, 1 )                                     /**/

#define DBG__internal_VALID_COMMAND_Component ()
#define DBG__internal_VALID_COMMAND_Comp ()
#define DBG__internal_REMAP_COMMAND_Component(arg) Component, arg,
#define DBG__internal_REMAP_COMMAND_Comp(arg) Component, arg,

#define DBG__internal_MakeOutput_Component(args, state)                                 \
        /*entry*/.set("CompName",  (args).statName() )                                      \
        /*entry*/.set("CompIndex",    (args).flexusIndex() )                                /**/

#define DBG__internal_NextState_Component(args, state)                                      \
      DBG__internal_NextState_Component_0( args, DBG__internal_State_SetComponent(state, args) )      /**/

#define DBG__internal_NextState_Component_0(args, state)                                          \
      DBG__internal_NextState_Component_1( args, DBG__internal_State_SetHasComponent( state, 1 )  )   /**/

#define DBG__internal_NextState_Component_1(args, state)                      \
      DBG__internal_NextState_Condition( (args).debugEnabled()  , state)          /**/

#define DBG__internal_VALID_COMMAND_Address ()
#define DBG__internal_VALID_COMMAND_Addr ()
#define DBG__internal_REMAP_COMMAND_Address(arg) Address, arg,
#define DBG__internal_REMAP_COMMAND_Addr(arg) Address, arg,

#define DBG__internal_MakeOutput_Address(args, state)     \
        /*entry*/.set( "Address" ,  (args) )                  /**/

#define DBG__internal_NextState_Address(args, state)  state

#endif //FLEXUS_CORE_DEBUG_AUX__OPERATIONS_HPP_INCLUDED
