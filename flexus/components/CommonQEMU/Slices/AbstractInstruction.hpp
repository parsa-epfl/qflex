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
#ifndef FLEXUS_SLICES_ABSTRACTINSTRUCTION_HPP_INCLUDED
#define FLEXUS_SLICES_ABSTRACTINSTRUCTION_HPP_INCLUDED

#include <ostream>
#include <core/boost_extensions/intrusive_ptr.hpp>
#include <components/CommonQEMU/Slices/TransactionTracker.hpp>

#include <components/CommonQEMU/Slices/FillLevel.hpp>

namespace Flexus {
namespace SharedTypes {

struct AbstractInstruction  : public boost::counted_base {
  boost::intrusive_ptr<TransactionTracker> theFetchTransaction;
protected:
  tFillLevel theInsnSourceLevel;

public:

  virtual void describe(std::ostream & anOstream) const;
  virtual bool haltDispatch() const;
  virtual void setFetchTransactionTracker(boost::intrusive_ptr<TransactionTracker> aTransaction) {
    theFetchTransaction = aTransaction;
  }
  virtual boost::intrusive_ptr<TransactionTracker> getFetchTransactionTracker() const {
    return theFetchTransaction;
  }
  AbstractInstruction() : theInsnSourceLevel(eL1I) {}
  virtual ~AbstractInstruction() {}

  virtual void setSourceLevel(tFillLevel aLevel) {
    theInsnSourceLevel = aLevel;
  }
  virtual tFillLevel sourceLevel() const {
    return theInsnSourceLevel;
  }

};

enum eSquashCause {
  kResynchronize = 1
  , kBranchMispredict = 2
  , kException = 3
  , kInterrupt = 4
  , kFailedSpec = 5
};

std::ostream & operator << ( std::ostream & anOstream, eSquashCause aCause );

std::ostream & operator << (std::ostream & anOstream, AbstractInstruction const & anInstruction);

} //Flexus
} //SharedTypes

#endif //FLEXUS_SLICES_ABSTRACTINSTRUCTION_HPP_INCLUDED
