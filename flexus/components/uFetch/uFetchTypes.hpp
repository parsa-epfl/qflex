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
#ifndef FLEXUS_uFETCH_TYPES_HPP_INCLUDED
#define FLEXUS_uFETCH_TYPES_HPP_INCLUDED

#include <list>
#include <iostream>

#include <core/boost_extensions/intrusive_ptr.hpp>
#include <components/CommonQEMU/Slices/TransactionTracker.hpp>
#include <components/CommonQEMU/Slices/FillLevel.hpp>

namespace Flexus {
namespace SharedTypes {

using boost::counted_base;
using Flexus::SharedTypes::VirtualMemoryAddress;
using Flexus::SharedTypes::TransactionTracker;

enum eBranchType {
  kNonBranch
  , kConditional
  , kUnconditional
  , kCall
  , kReturn
  , kLastBranchType
};
std::ostream & operator << (std::ostream & anOstream, eBranchType aType);

enum eDirection {
  kStronglyTaken
  , kTaken                //Bimodal
  , kNotTaken             //gShare
  , kStronglyNotTaken
};
std::ostream & operator << (std::ostream & anOstream, eDirection aDir);

struct BPredState : boost::counted_base {
  eBranchType thePredictedType;
  VirtualMemoryAddress thePredictedTarget;
  eDirection thePrediction;
  eDirection theBimodalPrediction;
  eDirection theMetaPrediction;
  eDirection theGSharePrediction;
  uint32_t theGShareShiftReg;
  uint32_t theSerial;
};

struct FetchAddr {
  Flexus::SharedTypes::VirtualMemoryAddress theAddress;
  boost::intrusive_ptr<BPredState> theBPState;
  FetchAddr(Flexus::SharedTypes::VirtualMemoryAddress anAddress)
    : theAddress(anAddress)
  { }
};

struct FetchCommand : boost::counted_base {
  std::list< FetchAddr > theFetches;
};

struct BranchFeedback : boost::counted_base {
  VirtualMemoryAddress thePC;
  eBranchType theActualType;
  eDirection theActualDirection;
  VirtualMemoryAddress theActualTarget;
  boost::intrusive_ptr<BPredState> theBPState;
};

typedef int64_t Opcode;

struct FetchedOpcode {
  VirtualMemoryAddress thePC;
  VirtualMemoryAddress theNextPC;
  Opcode theOpcode;
  boost::intrusive_ptr<BPredState> theBPState;
  boost::intrusive_ptr<TransactionTracker> theTransaction;

  FetchedOpcode( VirtualMemoryAddress anAddr
                 , VirtualMemoryAddress aNextAddr
                 , Opcode anOpcode
                 , boost::intrusive_ptr<BPredState> aBPState
                 , boost::intrusive_ptr<TransactionTracker> aTransaction
               )
    : thePC(anAddr)
    , theNextPC(aNextAddr)
    , theOpcode(anOpcode)
    , theBPState(aBPState)
    , theTransaction(aTransaction)
  { }
};

struct FetchBundle : public boost::counted_base {
  std::list< FetchedOpcode > theOpcodes;
  std::list< tFillLevel > theFillLevels;
};

typedef boost::intrusive_ptr<FetchBundle> pFetchBundle;

struct CPUState {
  int32_t theTL;
  int32_t thePSTATE;
};

} // end namespace SharedTypes
} // end namespace Flexus

#endif //FLEXUS_uFETCH_TYPES_HPP_INCLUDED

