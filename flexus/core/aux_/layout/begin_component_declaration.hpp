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
#ifndef FLEXUS_IMPLEMENTATION_FILE

#ifndef FLEXUS__LAYOUT_DECLARATION_SECTION
#error "Component declaration files must be included in the component declaration section of wiring.cpp"
#endif //FLEXUS_LAYOUT_COMPONENT_DECLARATION_SECTION

#ifndef FLEXUS_BEGIN_COMPONENT
#error "This component never declared the FLEXUS_BEGIN_COMPONENT macro"
#endif //FLEXUS_COMPONENT

#ifndef FLEXUS_END_COMPONENT
#error "Previous component forget to set the FLEXUS_END_COMPONENT macro"
#endif //FLEXUS_END_COMPONENT

#endif //FLEXUS_IMPLEMENTATION_FILE

#define FLEXUS__LAYOUT_IN_COMPONENT_DECLARATION

using namespace Flexus::SharedTypes;

#define COMPONENT_NO_PARAMETERS FLEXUS_DECLARE_COMPONENT_NO_PARAMETERS( FLEXUS_BEGIN_COMPONENT )
#define COMPONENT_PARAMETERS( x ) FLEXUS_DECLARE_COMPONENT_PARAMETERS( FLEXUS_BEGIN_COMPONENT, x )
#define PARAMETER FLEXUS_PARAMETER

#define COMPONENT_EMPTY_INTERFACE FLEXUS_COMPONENT_EMPTY_INTERFACE( FLEXUS_BEGIN_COMPONENT )
#define COMPONENT_INTERFACE( x ) FLEXUS_COMPONENT_INTERFACE( FLEXUS_BEGIN_COMPONENT, x )
#define PORT( x, y, z ) FLEXUS_IFACE_PORT( x, y, z)
#define PORT_ARRAY( x, y, z, w ) FLEXUS_IFACE_PORT_ARRAY( x, y, z, w)
#define DYNAMIC_PORT_ARRAY( x, y, z ) FLEXUS_IFACE_DYNAMIC_PORT_ARRAY( x, y, z)
#define DRIVE( x ) FLEXUS_IFACE_DRIVE( x )
