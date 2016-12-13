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
#ifndef FLEXUS__COMPONENT_INTERFACE_HPP_INCLUDED
#define FLEXUS__COMPONENT_INTERFACE_HPP_INCLUDED

#include <core/types.hpp>
#include <core/aux_/wiring/channels.hpp>

namespace Flexus {
namespace Core {

struct ComponentInterface {

  //This allows components to have push output ports
  template <class Port, typename AvailableFn, typename ManipulateFn >
  static Flexus::Core::aux_::port<Port, AvailableFn, ManipulateFn>
  get_channel(Port const &, AvailableFn avail, ManipulateFn manip, Flexus::Core::index_t aComponentIndex) {
    return Flexus::Core::aux_::port<Port, AvailableFn, ManipulateFn>(avail, manip, aComponentIndex);
  }

  template <class Port, typename AvailableFn, typename ManipulateFn >
  static Flexus::Core::aux_::port<Port, AvailableFn, ManipulateFn>
  get_channel_array(Port const &, AvailableFn avail, ManipulateFn manip, Flexus::Core::index_t aComponentIndex, Flexus::Core::index_t aPortIndex, Flexus::Core::index_t aPortWidth) {
    DBG_Assert( aPortIndex < aPortWidth, ( << "PortIndex: " << aPortIndex << " Width: " << aPortWidth ) );
    return Flexus::Core::aux_::port<Port, AvailableFn, ManipulateFn > (avail, manip, aComponentIndex * aPortWidth + aPortIndex );
  }

  //All components must provide the following members
  virtual void initialize() = 0;
  // added by PLotfi
  virtual void finalize() = 0;
  // end PLotfi
  virtual bool isQuiesced() const = 0;
  virtual void saveState(std::string const & aDirectory) = 0;
  virtual void loadState(std::string const & aDirectory) = 0;
  virtual std::string name() const = 0;
  virtual ~ComponentInterface() {}

};

}//namespace ComponentInterface
}//namespace Flexus

#endif //FLEXUS__COMPONENT_INTERFACE_HPP_INCLUDED

