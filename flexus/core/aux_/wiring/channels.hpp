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
#ifndef FLEXUS_AUX__WIRING_CHANNELS_HPP_INCLUDED
#define FLEXUS_AUX__WIRING_CHANNELS_HPP_INCLUDED

#include <core/debug/debug.hpp>

#ifdef FLEXUS__CORE_TEST
#define nFLEXUS FLEXUS__CORE_TEST
#else
#define nFLEXUS Flexus
#endif //FLEXUS__CORE_TEST

namespace Flexus {
namespace Core {
namespace aux_ { //will change to aux_

struct push {};
struct pull {};

} //aux_
} //Core
} //Flexus

namespace nFLEXUS {
namespace Wiring {

using Flexus::Core::aux_::push;
using Flexus::Core::aux_::pull;

template <class ToInstance, class ToPort, class Type, bool isArray>
struct resolve_channel;

template <class ToInstance, class ToPort>
struct resolve_channel<ToInstance, ToPort, pull, false> {
  static bool invoke_available(Flexus::Core::index_t anIndex) {
    return ToInstance::getReference(anIndex).available( ToPort());
  }
  static void invoke_manip(Flexus::Core::index_t anIndex, typename ToPort::payload & aPayload) {
    aPayload = ToInstance::getReference(anIndex).pull( ToPort());
  }
};

template <class ToInstance, class ToPort>
struct resolve_channel<ToInstance, ToPort, push, false> {
  static bool invoke_available(Flexus::Core::index_t anIndex) {
    return ToInstance::getReference(anIndex).available( ToPort());
  }
  static void invoke_manip(Flexus::Core::index_t anIndex, typename ToPort::payload & aPayload) {
    ToInstance::getReference(anIndex).push( ToPort(), aPayload);
  }
};

template <class ToInstance, class ToPort>
struct resolve_channel<ToInstance, ToPort, push, true> {
  static bool invoke_available(Flexus::Core::index_t anIndex) {
    int32_t width = ToInstance::iface::width(ToInstance::getInstance().theConfiguration, ToPort());
    return ToInstance::getReference(anIndex / width).available( ToPort(), anIndex % width );
  }
  static void invoke_manip(Flexus::Core::index_t anIndex, typename ToPort::payload & aPayload) {
    int32_t width = ToInstance::iface::width(ToInstance::getInstance().theConfiguration, ToPort());
    ToInstance::getReference(anIndex / width).push( ToPort(), anIndex % width, aPayload);
  }
};

template <class ToInstance, class ToPort>
struct resolve_channel<ToInstance, ToPort, pull, true> {
  static bool invoke_available(Flexus::Core::index_t anIndex) {
    int32_t width = ToInstance::iface::width(ToInstance::getInstance().theConfiguration, ToPort());
    return ToInstance::getReference(anIndex / width).available( ToPort(), anIndex % width );
  }
  static void invoke_manip(Flexus::Core::index_t anIndex, typename ToPort::payload & aPayload) {
    int32_t width = ToInstance::iface::width(ToInstance::getInstance().theConfiguration, ToPort());
    aPayload = ToInstance::getReference(anIndex / width).pull( ToPort(), anIndex % width);
  }
};

template <class SrcPort>
struct channel;

} //namespace Wiring
} //namespace Flexus or test

namespace Flexus {
namespace Core {
namespace aux_ { //will change to aux_

template <class PortType, class PortName, typename AvailableFn, typename ManipulateFn >
struct port_impl;

template <class PortName, typename AvailableFn, typename ManipulateFn >
struct port_impl<push, PortName, AvailableFn, ManipulateFn > {
  int32_t theIndex;
  AvailableFn theAvail;
  ManipulateFn thePush;
  typedef port_impl<push, PortName, AvailableFn, ManipulateFn > self;
  port_impl(AvailableFn anAvail, ManipulateFn aPush, int32_t anIndex)
    : theIndex(anIndex)
    , theAvail(anAvail)
    , thePush(aPush)
  {}
  bool available() {
    if (theAvail == 0) {
      return true;  //unwired port
    }
    return (*theAvail)(theIndex);
  }
  self & operator << (typename PortName::payload & aPayload) {
    if (thePush != 0) {
      (*thePush)(theIndex, aPayload);
    }
    return *this;
  }
};

template <class PortName, typename AvailableFn, typename ManipulateFn >
struct port_impl<pull, PortName, AvailableFn, ManipulateFn > {
  int32_t theIndex;
  AvailableFn theAvail;
  ManipulateFn thePull;
  typedef port_impl<pull, PortName, AvailableFn, ManipulateFn > self;
  port_impl(AvailableFn anAvail, ManipulateFn aPull, int32_t anIndex)
    : theIndex(anIndex)
    , theAvail(anAvail)
    , thePull(aPull)
  {}
  bool available() {
    if (theAvail == 0) {
      return false;  //unwired port
    }
    return (*theAvail)(theIndex);
  }
  self & operator >> (typename PortName::payload & aPayload) {
    if (!thePull) {
      DBG_Assert( false, ( << "Pull from an unconnected pull port" ) );
    }
    (*thePull)(theIndex, aPayload);
    return *this;
  }
};

template <class Port, typename AvailableFn, typename ManipulateFn >
struct port : public port_impl<typename Port::port_type, Port, AvailableFn, ManipulateFn > {
  port(AvailableFn avail, ManipulateFn manip, int32_t anIndex)
    : port_impl<typename Port::port_type, Port, AvailableFn, ManipulateFn >( avail, manip, anIndex )
  {}
} ;

}//namespace aux_
}//namespace Core
}//namespace Flexus

#undef nFLEXUS

#endif //FLEXUS_AUX__WIRING_CHANNELS_HPP_INCLUDED

