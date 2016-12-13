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
//ALEX: THIS FILE IS CURRENTLY A DUMMY. 
//Copied this file from SimFlex. Will gradually populate it properly.

#ifndef FLEXUS_QEMU_MAI_API_HPP_INCLUDED
#define FLEXUS_QEMU_MAI_API_HPP_INCLUDED

#include <boost/utility.hpp>

#include <core/flexus.hpp>
#include <core/exception.hpp>
#include <core/qemu/configuration_api.hpp>
//#include <core/qemu/hap_api.hpp>
#if FLEXUS_TARGET_IS(v9)
#include <core/qemu/sparcmmu.hpp>
#endif

/*
namespace Flexus {
namespace Qemu {
namespace API {
extern "C" {
#define restrict
#include FLEXUS_SIMICS_API_HEADER(processor)
#include FLEXUS_SIMICS_API_HEADER(memory)
#include FLEXUS_SIMICS_API_ARCH_HEADER
#include FLEXUS_SIMICS_API_HEADER(control)
#include FLEXUS_SIMICS_API_HEADER(front)
#include FLEXUS_SIMICS_API_HEADER(event)
#undef restrict
} //extern "C"
} //namespace API
} //namespace Qemu
} //namespace Flexus
*/
namespace Flexus {
namespace Qemu {
struct InstructionRaisesException : public QemuException {
  InstructionRaisesException () : QemuException("InstructionRaisesException") {}
};
struct UnresolvedDependenciesException : public QemuException {
  UnresolvedDependenciesException () : QemuException("UnresolvedDependenciesException") {}
};
struct SpeculativeException : public QemuException {
  SpeculativeException () : QemuException("SpeculativeException ") {}
};
struct StallingException : public QemuException {
  StallingException () : QemuException("StallingException ") {}
};
struct SyncException : public QemuException {
  SyncException() : QemuException("SyncException") {}
};

struct MemoryException : public QemuException {
  MemoryException() : QemuException("MemoryException") {}
};

} //namespace Qemu
} //namespace Flexus


namespace Flexus {
namespace Qemu {

class ProcessorMapper {
private:
  static ProcessorMapper * theMapper;

  int theNumVMs;
  std::vector<std::pair<int, int> > theProcMap;
  std::vector<int> theClientMap;
  std::vector<std::pair<int, bool> > theReverseMap;

  ProcessorMapper();

public:
  static int mapFlexusIndex2ProcNum(int index);
  static int mapClientNum2ProcNum(int index);
  static int mapProcNum2FlexusIndex(int index);
  static int mapFlexusIndex2VM(int index);

  static int numVMs();
  static int numClients();
  static int numProcessors();
};


using Flexus::SharedTypes::VirtualMemoryAddress;
using Flexus::SharedTypes::PhysicalMemoryAddress;

class BaseProcessorImpl {
protected:
  //Note: do not access these variables directly when writing member functions.
  //Instead, use *this whenever you need theProcessor or theProcessorAsConfObject.
  //That way, the conversion operators will get called, and they will fill in the pointers
  //if their current value is not cached.
  API::conf_object_t * theProcessor;
  int theProcessorNumber;
  int theQEMUProcessorNumber;

  void checkException() const {
  //dummy
    assert(false);
    //API::sim_exception_t anException = API::SIM_clear_exception();
    //checkException( anException ) ;
  }

/*  void checkException(API::sim_exception_t anException) const {
    if (anException != API::SimExc_No_Exception) {
      if (anException == API::SimExc_Memory) {
        throw MemoryException();
      } else {
        throw QemuException(API::SIM_last_error());
      }
    }
  }
*/
  BaseProcessorImpl(API::conf_object_t * aProcessor)
    : theProcessor(aProcessor)
    , theProcessorNumber(aProcessor ? ProcessorMapper::mapProcNum2FlexusIndex(API::QEMU_get_processor_number(aProcessor)) : 0)
    , theQEMUProcessorNumber(aProcessor ? API::QEMU_get_processor_number(aProcessor) : 0)
  {}

public:
  virtual ~BaseProcessorImpl() {}

  operator API::conf_object_t * () const {
    return theProcessor;
  }

  //      #if FLEXUS_TARGET_IS(x86)
  //       operator API::processor_t * () {
  //          return API::SIM_conf_object_to_processor(theProcessor);
  //        }
  //      #endif
  std::string disassemble(VirtualMemoryAddress const & anAddress) const {
    /*API::tuple_int_string_t * tuple;
    tuple = API::SIM_disassemble(*this, (API::logical_address_t)anAddress, 1);	//address is logical
    API::sim_exception_t anException = API::SIM_clear_exception();
    if (anException == API::SimExc_No_Exception) {
      if (strcmp("jmpl [%i7 + 8], %g0",tuple->string)==0) return std::string("ret (jmpl [%i7 + 8], %g0)");
      if (strcmp("jmpl [%o7 + 8], %g0",tuple->string)==0) return std::string("retl (jmpl [%o7 + 8], %g0)");
      return std::string(tuple->string);
    } else {*/
      return "disassembly unavailable";
    //}
  }

  std::string disassemble(PhysicalMemoryAddress const & anAddress) const {
    /*API::tuple_int_string_t * tuple;
    tuple = API::SIM_disassemble(*this, (API::physical_address_t)anAddress, 0); // address is physical
    API::sim_exception_t anException = API::SIM_clear_exception();
    if (anException == API::SimExc_No_Exception) {
      if (strcmp("jmpl [%i7 + 8], %g0",tuple->string)==0) return std::string("ret (jmpl [%i7 + 8], %g0)");
      if (strcmp("jmpl [%o7 + 8], %g0",tuple->string)==0) return std::string("retl (jmpl [%o7 + 8], %g0)");
      return std::string(tuple->string);
    } else {*/
      return "disassembly unavailable";
    //}
  }

  std::string describeException(int anException) const {
    /*char const * descr = API::SIM_get_exception_name(*this, anException);
    API::sim_exception_t except = API::SIM_clear_exception();
    if (except == API::SimExc_No_Exception) {
      return std::string(descr);
    } else {*/
      return "unknown_exception";
    //}
  }

  API::cycles_t cycleCount() const {
    //return API::SIM_cycle_count(*this);
    assert(false);
    return 0;
  }

  long long stepCount() const {
    //return API::SIM_step_count(*this);
    assert(false);
    return 0;
  }

  void enable() {
    assert(false);
    //API::SIM_enable_processor(*this);
  }

  void disable() {
    assert(false);
    //API::SIM_disable_processor(*this);
  }

  VirtualMemoryAddress getPC() const {
    return VirtualMemoryAddress( API::QEMU_get_program_counter(*this) );
  }

  unsigned long long readRegister(int aRegister) const {
    uint64_t reg_content;
    API::QEMU_read_register(*this, aRegister, NULL, &reg_content);
    return reg_content;
  }

  void writeRegister(int aRegister, unsigned long long aValue) const {
    assert(false);
    //return API::QEMU_write_register(*this, aRegister, aValue);
  }

  unsigned long long readVAddr(VirtualMemoryAddress anAddress, int aSize) const {
    try {
      API::logical_address_t addr(anAddress);
      API::physical_address_t phy_addr = API::QEMU_logical_to_physical(*this, API::QEMU_DI_Data, addr);
      checkException();

      unsigned long long value = API::QEMU_read_phys_memory( *this, phy_addr, aSize);
      checkException();

      return value;
    } catch (MemoryException & anError ) {
      return 0;
    }
  }

  unsigned long long readPAddr(PhysicalMemoryAddress anAddress, int aSize) const {
    try {
      API::physical_address_t phy_addr(anAddress);
      unsigned long long value = API::QEMU_read_phys_memory( *this, phy_addr, aSize);
      checkException();

      return value;
    } catch (MemoryException & anError ) {
      return 0;
    }
  }
 
/* //added by jevdjic 
 unsigned long long getMemorySizeInMB(){ //memory size in MB
  try{
    Qemu::API::conf_object_t *memory = API::SIM_get_object("server_memory_image");
    if (memory==0) memory = API::SIM_get_object("memory_image");
    if(memory==0) memory = API::SIM_get_object("memory0_image"); 
    if(memory==0) return 0;
    Qemu::API:: attr_value_t size = API::SIM_get_attribute(memory, "size");
    return size.u.integer/(1024*1024);	
  }catch(MemoryException &abError){
    return 0;
  }
 }
 //end jevdjic
*/
  void writePAddr(PhysicalMemoryAddress anAddress, int aSize, unsigned long long aValue) const {
    try {
      assert(false);
      //API::physical_address_t phy_addr(anAddress);
      //Qemu::API::SIM_write_phys_memory( *this, phy_addr, aValue, aSize);
      //checkException();
    } catch (MemoryException & anError ) {
      DBG_( Crit, ( << "Unable to write " << aValue << " to  " << anAddress << "[" << aSize << "]" ) );
    }
  }

  virtual PhysicalMemoryAddress translate(VirtualMemoryAddress anAddress) const {
    try {
      API::logical_address_t addr(anAddress);
      API::physical_address_t phy_addr = API::QEMU_logical_to_physical(*this, API::QEMU_DI_Data, addr);
      checkException();
      return PhysicalMemoryAddress(phy_addr);
    } catch (MemoryException & anError ) {
      return PhysicalMemoryAddress(0);
    }
  }

  virtual PhysicalMemoryAddress translateInstruction(VirtualMemoryAddress anAddress) const {
    try {
      API::logical_address_t addr(anAddress);
      API::physical_address_t phy_addr = API::QEMU_logical_to_physical(*this, API::QEMU_DI_Instruction, addr);
      checkException();

      return PhysicalMemoryAddress(phy_addr);
    } catch (MemoryException & anError ) {
      return PhysicalMemoryAddress(0);
    }
  }

  int id() const {
    return theProcessorNumber;
  }
  int QEMUId() const {
    return theQEMUProcessorNumber;
  }

  bool mai_mode() const {
    /*API::attr_value_t attr;
    attr = API::SIM_get_attribute(*this, "ooo_mode");
    if ( attr.kind == API::Sim_Val_String && std::string(attr.u.string) == "micro-architecture" ) {
      return true;
    }
    return false;*/
    return true;	//ALEX - Assuming this is only called when running Timing
  }

};

#if FLEXUS_TARGET_IS(v9)
struct Translation {
  VirtualMemoryAddress theVaddr;
  int theASI;
  int theTL;
  int thePSTATE;
  enum eTranslationType {
    eStore,
    eLoad,
    eFetch
  } theType;
  PhysicalMemoryAddress thePaddr;
  int theException;
  unsigned long long theTTEEntry;
  bool isCacheable();
  bool isSideEffect();
  bool isXEndian();
  bool isMMU();
  bool isInterrupt();
  bool isTranslating();
};

class v9ProcessorImpl :  public BaseProcessorImpl {
private:
  static const int kReg_npc = 33;
  mutable API::sparc_v9_interface_t * theSparcAPI;
  mutable API::conf_object_t * theMMU;
  mutable API::mmu_interface_t * theMMUAPI;
  int thePendingInterrupt;
  bool theInterruptsConnected;

  void initialize();
  void handleInterrupt( long long aVector);

public:
  explicit v9ProcessorImpl(API::conf_object_t * aProcessor)
    : BaseProcessorImpl(aProcessor)
    , thePendingInterrupt( API::QEMU_PE_No_Exception )
    , theInterruptsConnected(false) {
    theSparcAPI = 0;
    theMMUAPI = 0;
  }

  void initializeMMUs();

  VirtualMemoryAddress getNPC() const {
    uint64_t reg_content;
    API::QEMU_read_register(*this, kReg_npc, NULL, &reg_content);
    return VirtualMemoryAddress( reg_content );
  }

  API::sparc_v9_interface_t * sparc() const {
    if (theSparcAPI == 0) {
      assert(false); //FIXME
      //theSparcAPI = reinterpret_cast<API::sparc_v9_interface *>( API::SIM_get_interface(*this, "sparc_v9"));
      //DBG_Assert( theSparcAPI );
    }
    return theSparcAPI;
  }

  API::mmu_interface_t * mmu() const {
    if (theMMUAPI == 0) {
      //Obtain the MMU object
      API::attr_value_t mmu_obj;
      assert(false);	//FIXME
      //mmu_obj = API::SIM_get_attribute(*this, "mmu");
      theMMU = mmu_obj.u.object;
      DBG_Assert( theMMU );
      //theMMUAPI = reinterpret_cast<API::mmu_interface_t *>( API::SIM_get_interface(theMMU, "mmu"));
      DBG_Assert( theMMUAPI );
    }
    return theMMUAPI;
  }

//FIXME: The following functions for SPARC should be implemented in QEMU
  unsigned long long readF(int aRegister) const {
    return API::QEMU_read_register_by_type(*this, aRegister, 5);
  }

  void writeF(int aRegister, unsigned long long aValue) const {
    assert(false);
    //sparc()->write_fp_register_x(*this, aRegister, aValue);
  }

  unsigned long long readG(int aRegister) const {
    return API::QEMU_read_register_by_type(*this, aRegister, 0);
  }

  unsigned long long readAG(int aRegister) const {
    return API::QEMU_read_register_by_type(*this, aRegister, 1);
  }

  unsigned long long readMG(int aRegister) const {
    return API::QEMU_read_register_by_type(*this, aRegister, 3);
  }

  unsigned long long readIG(int aRegister) const {
    return API::QEMU_read_register_by_type(*this, aRegister, 2);
  }

  unsigned long long readWindowed(int aWindow, int aRegister) const {
    return API::QEMU_read_register_by_type(*this, aWindow*16+aRegister-8, 4);
  }

  unsigned long long interruptRead(VirtualMemoryAddress anAddress, int anASI) const;

  long long getSystemTickInterval() const {
/*
    double freq = 1.0, stick = 1.0;
    API::attr_value_t freq_attr, stick_attr;
    freq_attr = API::SIM_get_attribute(*this, "freq_mhz");
    if ( freq_attr.kind == API::Sim_Val_Integer ) {
      freq = freq_attr.u.integer;
    } else if ( freq_attr.kind == API::Sim_Val_Floating) {
      freq = freq_attr.u.floating;
    } else {
      DBG_Assert(false);
    }
    stick_attr = API::SIM_get_attribute(*this, "system_tick_frequency");
    if ( stick_attr.kind == API::Sim_Val_Integer ) {
      stick = stick_attr.u.integer;
    } else if ( stick_attr.kind == API::Sim_Val_Floating) {
      stick = stick_attr.u.floating;
    } else {
      DBG_Assert(false);
    }
    DBG_Assert( freq != 0 );
    DBG_Assert( stick != 0 );
    long long interval = static_cast<long long>(freq / stick);
    DBG_Assert( interval > 0);
    return interval;
*/
    return (long long)API::QEMU_get_tick_frequency(*this);	//ALEX: FIXME - replaced "interval" with frequency. 
								//Not sure what this is supposed to be.
  }

  //Public MMU API
  void translate(Translation & aTranslation, bool aTakeException) const;
  long /*opcode*/ fetchInstruction(Translation & aTranslation, bool aTakeTrap);

  unsigned long long readVAddr(VirtualMemoryAddress anAddress, int anASI, int aSize) const;
  unsigned long long readVAddrXendian(Translation & aTranslation, int aSize) const;

  MMU::mmu_t getMMU();
  void ckptMMU();
  void releaseMMUCkpt();
  void rollbackMMUCkpts(int n);
  void resyncMMU();
  bool validateMMU(MMU::mmu_t * m = NULL);
  void dumpMMU(MMU::mmu_t * m = NULL);
  void initializeASIInfo();

  unsigned long long mmuRead(VirtualMemoryAddress anAddress, int anASI);
  void mmuWrite(VirtualMemoryAddress anAddress, int anASI, unsigned long long aValue);

  //QemuImpl MMU API
  PhysicalMemoryAddress translateInstruction_QemuImpl(VirtualMemoryAddress anAddress) const;
  long fetchInstruction_QemuImpl(VirtualMemoryAddress const & anAddress);

  std::tuple < PhysicalMemoryAddress, bool/*cacheable*/, bool/*side-effect*/ > translateTSB_QemuImpl(VirtualMemoryAddress anAddress, int anASI) const;
  unsigned long long readVAddr_QemuImpl(VirtualMemoryAddress anAddress, int anASI, int aSize) const;
  unsigned long long readVAddrXendian_QemuImpl(VirtualMemoryAddress anAddress, int anASI, int aSize) const;
  void translate_QemuImpl(  API::v9_memory_transaction_t & xact, VirtualMemoryAddress anAddress, int anASI ) const;

  //MMUImpl
  void translate_MMUImpl( Translation & aTranslation, bool aTakeTrap) const;
  long /*opcode*/ fetchInstruction_MMUImpl(Translation & aTranslation, bool aTakeTrap);
  unsigned long long readVAddr_MMUImpl(Translation & aTranslation, int aSize) const;
  unsigned long long readVAddrXendian_MMUImpl(Translation & aTranslation, int aSize) const;

  int advance(bool anAcceptInterrupt);
  int getPendingException() const;
  int getPendingInterrupt() const;

  void breakSimulation() {
    //API::SIM_break_cycle( *this, 0);
    API::QEMU_break_simulation("");
  }
};
#endif //FLEXUS_TARGET_IS(v9)

#if FLEXUS_TARGET_IS(v9)
#define PROCESSOR_IMPL v9ProcessorImpl
//#elif FLEXUS_TARGET_IS(x86)
//#define PROCESSOR_IMPL x86ProcessorImpl
#else
#error "Architecture does not support MAI"
#endif

class Processor : public BuiltInObject< PROCESSOR_IMPL > {
  typedef BuiltInObject< PROCESSOR_IMPL > base;
public:
  explicit Processor():
    base( 0 ) {}

  explicit Processor(API::conf_object_t * aProcessor):
    base( PROCESSOR_IMPL (aProcessor) ) {}

  operator API::conf_object_t * () {
    return theImpl;
  }

  static Processor getProcessor(int aProcessorNumber) {
    return Processor(API::QEMU_get_cpu_by_index(ProcessorMapper::mapFlexusIndex2ProcNum(aProcessorNumber)));
  }

  static Processor getProcessor(std::string const & aProcessorName) {
    return Processor(API::QEMU_get_object(aProcessorName.c_str()));
//    return Processor(API::QEMU_get_object());
  }

  static Processor current() {
    assert(false);	//ALEX - FIXME
    return getProcessor(0);
    //return Processor(API::SIM_current_processor());
  }
};

#undef PROCESSOR_IMPL

#if FLEXUS_TARGET_IS(v9)
bool isTranslatingASI(int anASI);
#endif

unsigned long long endianFlip(unsigned long long val, int aSize);

} //end Namespace Qemu
} //end namespace Flexus

#endif //FLEXUS_QEMU_MAI_API_HPP_INCLUDED

