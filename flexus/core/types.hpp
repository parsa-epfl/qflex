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
#ifndef TYPES_HPP
#define TYPES_HPP

#include <iostream>
#include <iomanip>
#include <boost/serialization/serialization.hpp>
#include <boost/operators.hpp>
#if __cplusplus > 199711L
  #include <cstdint>
#else
  #include <stdint.h>
#endif

#include <core/target.hpp>

namespace Flexus {
namespace Core {

// specifiers for address lengths
struct Address32Bit {};
struct Address64Bit {};

typedef uint32_t index_t;
typedef uint32_t node_id_t;

} // namespace Core
} // namespace Flexus

namespace Flexus {
namespace Core {

template < class underlying_type, bool isVirtual = false >
class MemoryAddress_
  : boost::totally_ordered < MemoryAddress_<underlying_type, isVirtual>
  , boost::additive < MemoryAddress_<underlying_type, isVirtual>, int
    > > {
public:
  MemoryAddress_()
    : address(0)
  {}
  explicit MemoryAddress_(underlying_type newAddress)
    : address(newAddress)
  {}
  bool operator< (MemoryAddress_ const & other) {
    return (address < other.address);
  }
  bool operator== (MemoryAddress_ const & other) {
    return (address == other.address);
  }
  MemoryAddress_& operator= (MemoryAddress_ const & other) {
    address = other.address;
    return *this;
  }
  MemoryAddress_& operator += (underlying_type const & addend) {
    address += addend;
    return *this;
  }
  MemoryAddress_& operator -= (underlying_type const & addend) {
    address -= addend;
    return *this;
  }
  operator underlying_type() const {
    return address;
  }
  friend std::ostream & operator << ( std::ostream & anOstream, MemoryAddress_ const & aMemoryAddress) {
    anOstream << (isVirtual ? "v:" : "p:" ) << std::hex << std::setw(9) << std::right << std::setfill('0') << aMemoryAddress.address << std::dec;
    return anOstream;
  }
private:
  underlying_type address;

  //Serialization Support
public:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, uint32_t version) {
    ar & address;
  }

}; // class MemoryAddress_

typedef uint32_t Word32Bit;
typedef int64_t Word64Bit;

} // end namespace Core

namespace SharedTypes {

#define FLEXUS_PhysicalMemoryAddress_TYPE_PROVIDED
#define FLEXUS_DataWord_TYPE_PROVIDED
#if (FLEXUS_TARGET_PA_BITS == 32)
// typedef Core::MemoryAddress_< Core::Word32Bit, false > PhysicalMemoryAddress;
// typedef Core :: Word32Bit DataWord;
typedef Core::MemoryAddress_< Core::Word64Bit, false > PhysicalMemoryAddress;
typedef Core :: Word64Bit DataWord;
#elif (FLEXUS_TARGET_PA_BITS == 64)
typedef Core::MemoryAddress_< Core::Word64Bit, false > PhysicalMemoryAddress;
typedef Core :: Word64Bit DataWord;
#else
#error "FLEXUS_TARGET_PA_BITS is not set. Did you forget to set a target?"
#endif

#define FLEXUS_VirtualMemoryAddress_TYPE_PROVIDED
#if (FLEXUS_TARGET_VA_BITS == 32)
// typedef Core::MemoryAddress_< Core::Word32Bit, true > VirtualMemoryAddress;
typedef Core::MemoryAddress_< Core::Word64Bit, true > VirtualMemoryAddress;
#elif (FLEXUS_TARGET_VA_BITS == 64)
typedef Core::MemoryAddress_< Core::Word64Bit, true > VirtualMemoryAddress;
#else
#error "FLEXUS_TARGET_PA_BITS is not set. Did you forget to set a target?"
#endif

using Flexus::Core::index_t;
using Flexus::Core::node_id_t;

} // end namespace SharedTypes
} // end namespace Flexus

#endif // TYPES_HPP
