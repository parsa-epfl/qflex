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
#ifndef FLEXUS__SIMULATOR_LAYOUT_INCLUDED

#ifdef FLEXUS_WIRING_FILE
//We are being included from wiring.cpp

#include <core/target.hpp>
#include <core/debug/debug.hpp>
#include <core/component.hpp>
#include <core/configuration.hpp>

namespace Flexus {
namespace SharedTypes { } }

#define FLEXUS_BEGIN_DECLARATION_SECTION()                        <core/aux_/layout/begin_declaration_section.hpp>
#define FLEXUS_END_DECLARATION_SECTION()                          <core/aux_/layout/end_declaration_section.hpp>

#define FLEXUS_BEGIN_GROUP_DEFINITION_SECTION()                   <core/aux_/layout/begin_group_definition_section.hpp>
#define FLEXUS_END_GROUP_DEFINITION_SECTION()                     <core/aux_/layout/end_group_definition_section.hpp>
#define FLEXUS_BEGIN_COMPONENT_CONFIGURATION_SECTION()            <core/aux_/layout/begin_comp_configuration_sec.hpp>
#define FLEXUS_END_COMPONENT_CONFIGURATION_SECTION()              <core/aux_/layout/end_comp_configuration_sec.hpp>
#define FLEXUS_BEGIN_COMPONENT_INSTANTIATION_SECTION()            <core/aux_/layout/begin_comp_instantiation_sec.hpp>
#define FLEXUS_END_COMPONENT_INSTANTIATION_SECTION()              <core/aux_/layout/end_comp_instantiation_sec.hpp>
#define FLEXUS_BEGIN_COMPONENT_WIRING_SECTION()                   <core/aux_/layout/begin_comp_wiring_sec.hpp>
#define FLEXUS_END_COMPONENT_WIRING_SECTION()                     <core/aux_/layout/end_comp_wiring_sec.hpp>
#define FLEXUS_BEGIN_DRIVE_ORDER_SECTION()                        <core/aux_/layout/begin_drive_section.hpp>
#define FLEXUS_END_DRIVE_ORDER_SECTION()                          <core/aux_/layout/end_drive_section.hpp>

#define FLEXUS_BEGIN_COMPONENT_DECLARATION()                      <core/aux_/layout/begin_component_declaration.hpp>
#define FLEXUS_END_COMPONENT_DECLARATION()                        <core/aux_/layout/end_component_declaration.hpp>

#else //! FLEXUS_WIRING_FILE
//We are being included from a component declaration file within its
//implementation file

#define FLEXUS_IMPLEMENTATION_FILE

#include <core/types.hpp>
#include <core/debug/debug.hpp>
#include <core/component.hpp>
#include <core/configuration.hpp>

namespace Flexus {
namespace SharedTypes { } }

#define FLEXUS_BEGIN_COMPONENT_DECLARATION()                      <core/aux_/layout/begin_component_declaration.hpp>
#define FLEXUS_END_COMPONENT_DECLARATION()                        <core/aux_/layout/end_component_declaration.hpp>

#define FLEXUS_BEGIN_COMPONENT_IMPLEMENTATION()                   <core/aux_/layout/begin_component_implementation.hpp>
#define FLEXUS_END_COMPONENT_IMPLEMENTATION()                     <core/aux_/layout/end_component_implementation.hpp>

#endif //FLEXUS_WIRING_FILE

#endif //FLEXUS__SIMULATOR_LAYOUT_INCLUDED
