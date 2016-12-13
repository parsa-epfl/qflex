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
#ifndef FLEXUS_SLICES__ARCHITECTURAL_INSTRUCTION_HPP_INCLUDED
#define FLEXUS_SLICES__ARCHITECTURAL_INSTRUCTION_HPP_INCLUDED

#ifdef FLEXUS_ArchitecturalInstruction_TYPE_PROVIDED
#error "Only one component may provide the Flexus::SharedTypes::ArchitecturalInstruction data type"
#endif
#define FLEXUS_ArchitecturalInstruction_TYPE_PROVIDED

#include <core/boost_extensions/intrusive_ptr.hpp>
#include <core/types.hpp>
#include <components/CommonQEMU/DoubleWord.hpp>

namespace Flexus {
namespace SharedTypes {

//Forward declare
//class SimicsTraceConsumer;
class StoreBuffer;

enum execType {
  Normal,
  Second_MemPart,
  Hardware_Walk,
  Halt_instr
};

//InorderInstructionImpl - Implementation class for SPARC v9 memory operations
//==================================
class ArchitecturalInstruction : public boost::counted_base { /*, public FastAlloc*/
  enum eOpType {
    Nop,
    Read,
    Write,
    Rmw,
    Membar
  };

  // A memory operation needs to know its address, data, and operation type
  PhysicalMemoryAddress thePhysicalAddress;
  VirtualMemoryAddress theVirtualAddress;
  PhysicalMemoryAddress thePhysicalInstructionAddress;    // i.e. the PC
  VirtualMemoryAddress theVirtualInstructionAddress;      // i.e. the PC
  DoubleWord theData;
  eOpType theOperation;
 // SimicsTraceConsumer * theConsumer;
  bool theReleased;
  bool thePerformed;
  bool theCommitted;
  bool theSync;
  bool thePriv;
  bool theShadow;
  bool theTrace;  // a traced instruction
  bool theIsAtomic;
  uint64_t theStartTime;
  StoreBuffer * theStoreBuffer;
  uint32_t theOpcode;
  char theSize;
  char instructionSize;//the size of the instruction...for x86 purposes
  execType hasIfetchPart;//for instructions that perform 2 memory operations, or hardware walks
  char theIO;

private:
  ArchitecturalInstruction(ArchitecturalInstruction * anOriginal)
    : thePhysicalAddress(anOriginal->thePhysicalAddress)
    , theVirtualAddress(anOriginal->theVirtualAddress)
    , thePhysicalInstructionAddress(anOriginal->thePhysicalInstructionAddress)
    , theVirtualInstructionAddress(anOriginal->theVirtualInstructionAddress)
    , theData(anOriginal->theData)
    , theOperation(anOriginal->theOperation)
    , theReleased(false)
    , thePerformed(false)
    , theCommitted(false)
    , theSync(anOriginal->theSync)
    , thePriv(anOriginal->thePriv)
    , theShadow(true)
    , theTrace(anOriginal->theTrace)
    , theStartTime(0)
    , theStoreBuffer(anOriginal->theStoreBuffer)
    , theOpcode(anOriginal->theOpcode)
    , theSize(anOriginal->theSize)
    , instructionSize(anOriginal->instructionSize)
    , hasIfetchPart(Normal)
    , theIO(anOriginal->theIO)
  {}

public:
  ArchitecturalInstruction()
    : thePhysicalAddress(0)
    , theVirtualAddress(0)
    , thePhysicalInstructionAddress(0)
    , theVirtualInstructionAddress(0)
    , theData(0)
    , theOperation(Nop)
    , theReleased(false)
    , thePerformed(false)
    , theCommitted(false)
    , theSync(false)
    , thePriv(false)
    , theShadow(false)
    , theTrace(false)
    , theStartTime(0)
    , theStoreBuffer(0)
    , theOpcode(0)
    , theSize(0)
    , instructionSize(0)
    , hasIfetchPart(Normal)
    , theIO(false)
  {}

  /*explicit ArchitecturalInstruction(SimicsTraceConsumer * aConsumer)
    : thePhysicalAddress(0)
    , theVirtualAddress(0)
    , thePhysicalInstructionAddress(0)
    , theVirtualInstructionAddress(0)
    , theData(0)
    , theOperation(Nop)
//    , theConsumer(aConsumer)
    , theReleased(false)
    , thePerformed(false)
    , theCommitted(false)
    , theSync(false)
    , thePriv(false)
    , theShadow(false)
    , theTrace(false)
    , theIsAtomic(false)
    , theStartTime(0)
    , theStoreBuffer(0)
    , theOpcode(0)
    , theSize(0)
    , instructionSize(0)
    , hasIfetchPart(Normal)
    , theIO(false)
  {}*/

  boost::intrusive_ptr<ArchitecturalInstruction> createShadow() {
    return boost::intrusive_ptr<ArchitecturalInstruction> (new ArchitecturalInstruction(this)  );
  }

  //ArchitecturalInstruction Interface functions
  //Query for type of operation
  bool isNOP() const {
    return (theOperation == Nop)  ;
  }
  bool isMemory() const {
    return (theOperation == Read) || (theOperation == Write) || (theOperation == Rmw) || (theOperation == Membar) ;
  }
  bool isLoad() const {
    return (theOperation == Read);
  }
  bool isStore() const {
    return (theOperation == Write);
  }
  bool isRmw() const {
    return (theOperation == Rmw);
  }
  bool isMEMBAR() const {
    return (theOperation == Membar);
  }
  bool isSync() const {
    return theSync;
  }
  bool isPriv() const {
    return thePriv;
  }

  bool isShadow() const {
    return theShadow;
  }

  bool isTrace() const {
    return theTrace;
  }

  uint64_t getStartTime() const {
    return theStartTime;
  }

  bool isCommitted() const {
    return theCommitted;
  }
  bool isIO() const {
    return theIO;
  }
  void setIO() {
    theIO = true;
  }

  void commit() {
    DBG_Assert( (theCommitted == false) );
    theCommitted = true;;
  }

  bool isReleased() const {
    return theReleased;
  }

  bool canRelease() const {
    //TSO
    if (isSync()) {
      return isPerformed() && isCommitted() && !isReleased();
    } else {
      return isCommitted() && !isReleased();
    }
  }

  void release();

  bool isPerformed() const {
    return thePerformed;
  }

  DoubleWord const & data() const {
    return theData;
  }

  uint32_t size() const {
    return theSize;
  }

  uint32_t InstructionSize() const {
    return instructionSize;
  }

  uint32_t opcode() const {
    return theOpcode;
  }

  //Perform the instruction
  void perform();

  //InorderInstructionImpl Interface functions
  void setSync() {
    theSync = true;
  }
  void setPriv() {
    thePriv = true;
  }

  //Set operation types
  void setIsNop() {
    theOperation = Nop;
  }
  void setIsLoad() {
    theOperation = Read;
  }
  void setIsMEMBAR() {
    theOperation = Membar;
    setSync();
  }
  void setIsStore() {
    theOperation = Write;
  }
  void setIsRmw() {
    theOperation = Rmw;
    setSync();
  }

  void setShadow() {
    theShadow = true;
  }

  void setTrace() {
    theTrace = true;
  }

  void setStartTime(uint64_t start) {
    theStartTime = start;
  }

  void setSize(char aSize) {
    theSize = aSize;
  }

  void setInstructionSize(char const aSize) {
    instructionSize = aSize;
  }

  //Set the address for a memory operation
  void setAddress(PhysicalMemoryAddress const & addr) {
    thePhysicalAddress = addr;
  }

  //Set the virtual address for a memory operation
  void setVirtualAddress(VirtualMemoryAddress const & addr) {
    theVirtualAddress = addr;
  }

  void setIfPart(execType const hasifpart) {
    hasIfetchPart = hasifpart;
  }

  //Set the PC for the operation
  void setPhysInstAddress(PhysicalMemoryAddress const & addr) {
    thePhysicalInstructionAddress = addr;
  }
  void setVirtInstAddress(VirtualMemoryAddress const & addr) {
    theVirtualInstructionAddress = addr;
  }

  void setStoreBuffer(StoreBuffer * aStoreBuffer) {
    theStoreBuffer = aStoreBuffer;
  }

  void setData(DoubleWord const & aData) {
    theData = aData;
  }

  void setOpcode(uint32_t anOpcode) {
    theOpcode = anOpcode;
  }

  //Get the address for the instruction reference
  PhysicalMemoryAddress physicalInstructionAddress() const {
    return thePhysicalInstructionAddress;
  }

  VirtualMemoryAddress virtualInstructionAddress() const {
    return theVirtualInstructionAddress;
  }

  //Get the address for a memory operation
  PhysicalMemoryAddress physicalMemoryAddress() const {
    return thePhysicalAddress;
  }

  VirtualMemoryAddress virtualMemoryAddress() const {
    return theVirtualAddress;
  }

  execType getIfPart() const {
    return hasIfetchPart;
  }

  const char * opName() const {
    const char * opTypeStr[] = {"nop", "read", "write", "rmw", "membar"};
    return opTypeStr[theOperation];
  }

  //Forwarding functions from SimicsV9MemoryOp
  bool isBranch() const {
    return false;
  }
  bool isInterrupt() const {
    return false;
  }
  bool requiresSync() const {
    return false;
  }
  void execute() {
    if (!isMemory()) perform();
  }
  void squash() {
    DBG_Assert(false, ( << "squash not supported") );
  }
  bool isValid() const {
    return true;
  }
  bool isFetched() const {
    return true;
  }
  bool isDecoded() const {
    return true;
  }
  bool isExecuted() const {
    return true;
  }
  bool isSquashed() const {
    return false;  //Squashing not supported
  }
  bool isExcepted() const {
    return false;  //Exceptions not supported
  }
  bool wasTaken() const {
    return false;  //Branches not supported
  }
  bool wasNotTaken() const {
    return false;  //Branches not supported
  }
  bool canExecute() const {
    return true;  //execute() always true
  }
  bool canPerform() const {
    return true;  //execute() always true
  }

}; //End ArchitecturalInstruction

std::ostream & operator <<(std::ostream & anOstream, const ArchitecturalInstruction & aMemOp);
bool operator == (const ArchitecturalInstruction & a, const ArchitecturalInstruction & b);

} //End SharedTypes
} //End Flexus

#endif //FLEXUS_SLICES__ARCHITECTURAL_INSTRUCTION_HPP_INCLUDED
