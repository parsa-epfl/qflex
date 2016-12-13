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
#define FLEXUS__CORE_TEST IncompatiblePortTest

#include <list>

#include <core/test/test_utils.hpp>

struct Payload {};

FLEXUS_COMPONENT class WiringSource  {
  FLEXUS_COMPONENT_IMPLEMENTATION(WiringSource);
public:

  void initialize() {}

  struct MismatchOut : public PushOutputPort < const Payload > {};

  struct TestDrive {
    typedef FLEXUS_TEST_IO_LIST( 1, Availability< MismatchOut > ) Inputs;
    typedef FLEXUS_TEST_IO_LIST( 1, Value< MismatchOut > ) Outputs;

    FLEXUS_TEST_WIRING
    static void doCycle(self & theComponent) {
      BOOST_CHECK((  FLEXUS_TEST_CHANNEL_AVAILABLE( MismatchOut ) ));
      FLEXUS_TEST_CHANNEL( MismatchOut ) << Payload();
    }

  };

  typedef FLEXUS_TEST_DRIVE_LIST (1, TestDrive) DriveInterfaces;

};
FLEXUS_COMPONENT_EMPTY_CONFIGURATION_TEMPLATE(WiringSourceCfgTempl);

FLEXUS_COMPONENT class WiringSink  {
  FLEXUS_COMPONENT_IMPLEMENTATION(WiringSink);
public:
  void initialize() {}

  struct MismatchOut : public PushOutputPort < const Payload > { };

  struct CheckDrive {
    typedef FLEXUS_TEST_IO_LIST( 1, Availability< MismatchOut > ) Inputs;
    typedef FLEXUS_TEST_IO_LIST( 1, Value< MismatchOut > ) Outputs;

    FLEXUS_TEST_WIRING
    static void doCycle(self & theComponent) {
    }
  };

  typedef FLEXUS_TEST_DRIVE_LIST (1, CheckDrive) DriveInterfaces;
};
FLEXUS_COMPONENT_EMPTY_CONFIGURATION_TEMPLATE(WiringSinkCfgTempl);

namespace FLEXUS__CORE_TEST {
typedef WiringSourceCfgTempl<> WiringSourceCfg_t;
WiringSourceCfg_t WiringSourceCfg("wiring-test");

typedef WiringSinkCfgTempl<> WiringSinkCfg_t;
WiringSinkCfg_t WiringSinkCfg("wiring-test");

FLEXUS_INSTANTIATE_COMPONENT(WiringSource, WiringSourceCfg_t, WiringSourceCfg, NoDebug, theSource);
FLEXUS_INSTANTIATE_COMPONENT(WiringSink, WiringSinkCfg_t, WiringSinkCfg, NoDebug, theSink);
}

#include FLEXUS_BEGIN_COMPONENT_REGISTRATION_SECTION()
FLEXUS__CORE_TEST::theSink
, FLEXUS__CORE_TEST::theSource
#include FLEXUS_END_COMPONENT_REGISTRATION_SECTION()

#include FLEXUS_BEGIN_COMPONENT_WIRING_SECTION()

FROM ( FLEXUS__CORE_TEST::theSource, MismatchOut ) TO ( FLEXUS__CORE_TEST::theSink, MismatchOut )

#include FLEXUS_END_COMPONENT_WIRING_SECTION()

#include FLEXUS_CODE_GENERATION_SECTION()

void testIncompatiblePort() {

  //Initialize components
  BOOST_CHECKPOINT( "About to test wiring" );
  FLEXUS__CORE_TEST::Wiring::theDrive.doCycle();

  BOOST_CHECK_MESSAGE( true, "Wiring test complete" );
}

test_suite * incompatible_port_test_suite() {
  test_suite * test = BOOST_TEST_SUITE( "Mismatched Payload unit test" );

  test->add( BOOST_TEST_CASE( &testIncompatiblePort ) );

  return test;
}

