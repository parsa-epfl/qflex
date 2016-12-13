// DO-NOT-REMOVE begin-copyright-block 
//QFlex consists of several software components that are governed by various
//licensing terms, in addition to software that was developed internally.
//Anyone interested in using QFlex needs to fully understand and abide by the
//licenses governing all the software components.
//
//### Software developed externally (not by the QFlex group)
//
//    * [NS-3](https://www.gnu.org/copyleft/gpl.html)
//    * [QEMU](http://wiki.qemu.org/License) 
//    * [SimFlex] (http://parsa.epfl.ch/simflex/)
//
//Software developed internally (by the QFlex group)
//**QFlex License**
//
//QFlex
//Copyright (c) 2016, Parallel Systems Architecture Lab, EPFL
//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without modification,
//are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright notice,
//      this list of conditions and the following disclaimer in the documentation
//      and/or other materials provided with the distribution.
//    * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
//      nor the names of its contributors may be used to endorse or promote
//      products derived from this software without specific prior written
//      permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
//ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
//EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
//GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
//HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
//THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// DO-NOT-REMOVE end-copyright-block   
#ifndef FLEXUS_SLICES__MEMOP_HPP_INCLUDED
#define FLEXUS_SLICES__MEMOP_HPP_INCLUDED

#include <functional>
#include <boost/dynamic_bitset.hpp>

#include <components/CommonQEMU/Slices/AbstractInstruction.hpp>
#include <components/CommonQEMU/Slices/TransactionTracker.hpp>

namespace Flexus {
namespace SharedTypes {

enum eOperation { //Sorted by priority for requesting memory ports
  kLoad
  , kAtomicPreload
  , kRMW
  , kCAS
  , kStorePrefetch
  , kStore
  , kInvalidate
  , kDowngrade
  , kProbe
  , kReturnReq
  , kLoadReply
  , kAtomicPreloadReply
  , kStoreReply
  , kStorePrefetchReply
  , kRMWReply
  , kCASReply
  , kDowngradeAck
  , kInvAck
  , kProbeAck
  , kReturnReply
  , kMEMBARMarker
  , kINVALID_OPERATION
  , kLastOperation
};
std::ostream & operator << ( std::ostream & anOstream, eOperation op);

enum eSize {
  kByte = 1
  , kHalfWord = 2
  , kWord = 4
  , kDoubleWord = 8
};

struct MemOp : boost::counted_base {
  eOperation theOperation;
  eSize theSize;
  VirtualMemoryAddress theVAddr;
  int32_t theASI;
  PhysicalMemoryAddress thePAddr;
  VirtualMemoryAddress thePC;
  uint64_t theValue;
  uint64_t theExtendedValue;
  bool theReverseEndian;
  bool theNonCacheable;
  bool theSideEffect;
  bool theAtomic;
  bool theNAW;
  boost::intrusive_ptr< TransactionTracker > theTracker;
  MemOp( )
    : theOperation( kINVALID_OPERATION )
    , theSize( kWord )
    , theVAddr( VirtualMemoryAddress(0) )
    , theASI( 0x80 )
    , thePAddr( PhysicalMemoryAddress(0) )
    , thePC (VirtualMemoryAddress(0) )
    , theValue( 0 )
    , theExtendedValue ( 0 )
    , theReverseEndian(false)
    , theNonCacheable(false)
    , theSideEffect(false)
    , theAtomic(false)
    , theNAW(false)
  {}
  MemOp( MemOp const & anOther)
    : theOperation( anOther.theOperation )
    , theSize( anOther.theSize)
    , theVAddr( anOther.theVAddr)
    , theASI( anOther.theASI )
    , thePAddr( anOther.thePAddr)
    , thePC( anOther.thePC)
    , theValue( anOther.theValue)
    , theExtendedValue( anOther.theExtendedValue)
    , theReverseEndian(anOther.theReverseEndian)
    , theNonCacheable(anOther.theNonCacheable)
    , theSideEffect(anOther.theSideEffect)
    , theAtomic(anOther.theAtomic)
    , theNAW(anOther.theNAW)
    , theTracker( anOther.theTracker )
  {}

};

std::ostream & operator << ( std::ostream & anOstream, MemOp const & aMemOp);

} //SharedTypes
} //Flexus

#endif //FLEXUS_SLICES__MEMOP_HPP_INCLUDED

