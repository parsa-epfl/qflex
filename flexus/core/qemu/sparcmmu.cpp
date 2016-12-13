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
#include <boost/throw_exception.hpp>
#include <memory>
#include <functional>

#include <core/target.hpp>
#include <core/types.hpp>
#include <core/qemu/api_wrappers.hpp>

#if FLEXUS_TARGET_IS(v9) // For now, just disable that.

#include "mai_api.hpp"

#include <core/qemu/sparcmmu.hpp>

#include <stdio.h>

#define DBG_DefineCategories MMUCat
#define DBG_SetDefaultOps AddCat(MMUCat)
#include DBG_Control()

namespace Flexus {
namespace Qemu {

#if FLEXUS_TARGET_IS(v9)
using namespace Flexus::Qemu::API;

int asi_info_table[256];

bool theMMUs_initialized = false;
std::vector<MMU::mmu_t> theMMUs;
std::vector<std::deque<MMU::mmu_t> > theMMUckpts;

std::vector<int> theMMUMap;

void v9ProcessorImpl::initializeMMUs() {
  if (!theMMUs_initialized) {
    theMMUs_initialized = true;
    ASI_INFO_DEFINITION(asi_info_table);

    int num_procs = ProcessorMapper::numProcessors();
    //int num_procs = SIM_number_processors();
    theMMUs.resize( num_procs );
    theMMUckpts.resize( num_procs );

    theMMUMap.resize( num_procs );

    for (unsigned int i = 0; i < theMMUs.size(); ++i) {
      //API::conf_object_t * cpu = Qemu::API::QEMU_get_processor(ProcessorMapper::mapFlexusIndex2ProcNum(i));
      API::conf_object_t * cpu = Qemu::API::QEMU_get_cpu_by_index(ProcessorMapper::mapFlexusIndex2ProcNum(i));
      //MMU::fm_init_mmu_from_simics( &theMMUs[i], SIM_get_attribute(cpu, "mmu").u.object );
      //ALEX - FIXME: Get mmu from QEMU
    }
  }
}

bool isTranslatingASI(int anASI) {
  switch (anASI) {
    case ASI_NUCLEUS:
    case ASI_NUCLEUS_LITTLE:
    case ASI_AS_IF_USER_PRIMARY:
    case ASI_AS_IF_USER_SECONDARY:
    case ASI_AS_IF_USER_PRIMARY_LITTLE:
    case ASI_AS_IF_USER_SECONDARY_LITTLE:
    case ASI_PRIMARY:
    case ASI_SECONDARY:
    case ASI_PRIMARY_NOFAULT:
    case ASI_SECONDARY_NOFAULT:
    case ASI_PRIMARY_LITTLE:
    case ASI_SECONDARY_LITTLE:
    case ASI_PRIMARY_NOFAULT_LITTLE:
    case ASI_SECONDARY_NOFAULT_LITTLE:
    case ASI_PHYS_USE_EC:
    case ASI_PHYS_BYPASS_EC_WITH_EBIT:
    case ASI_PHYS_USE_EC_LITTLE:
    case ASI_PHYS_BYPASS_EC_WITH_EBIT_LITTLE:
    case ASI_NUCLEUS_QUAD_LDD:
    case ASI_NUCLEUS_QUAD_LDD_L:
    case ASI_BLK_AIUP:
    case ASI_BLK_AIUS:
    case ASI_BLK_AIUPL:
    case ASI_BLK_AIUSL:
    case ASI_BLK_P:
    case ASI_BLK_S:
    case ASI_BLK_PL:
    case ASI_BLK_SL:
    case ASI_PST8_P:
    case ASI_PST8_S:
    case ASI_PST16_P:
    case ASI_PST16_S:
    case ASI_PST32_P:
    case ASI_PST32_S:
    case ASI_PST8_PL:
    case ASI_PST8_SL:
    case ASI_PST16_PL:
    case ASI_PST16_SL:
    case ASI_PST32_PL:
    case ASI_PST32_SL:
    case ASI_FL8_P:
    case ASI_FL8_S:
    case ASI_FL16_P:
    case ASI_FL16_S:
    case ASI_FL8_PL:
    case ASI_FL8_SL:
    case ASI_FL16_PL:
    case ASI_FL16_SL:
    case ASI_BLK_COMMIT_P:
    case ASI_BLK_COMMIT_S:
      //case ASI_QUAD_LDD_PHYS:
      //case ASI_QUAD_LDD_PHYS_L:
      return true;
    default:
      return false;
  }
}

long /*opcode*/ v9ProcessorImpl::fetchInstruction(Translation & aTranslation, bool aTakeTrap) {
  return fetchInstruction_MMUImpl(aTranslation, aTakeTrap);
}

void v9ProcessorImpl::translate(Translation & aTranslation, bool aTakeException) const {
  translate_MMUImpl(aTranslation, aTakeException);
}

unsigned long long v9ProcessorImpl::readVAddr(VirtualMemoryAddress anAddress, int anASI, int aSize) const {
  return readVAddr_QemuImpl(anAddress, anASI, aSize);
}

unsigned long long v9ProcessorImpl::readVAddrXendian(Translation & aTranslation, int aSize) const {
  return readVAddrXendian_MMUImpl(aTranslation, aSize);
}

PhysicalMemoryAddress v9ProcessorImpl::translateInstruction_QemuImpl( VirtualMemoryAddress anAddress) const {
  try {
    API::logical_address_t addr(anAddress);
    API::physical_address_t phy_addr = API::QEMU_logical_to_physical(*this, API::QEMU_DI_Instruction, addr);
    checkException();

    return PhysicalMemoryAddress(phy_addr);
  } catch (MemoryException & anError ) {
    return PhysicalMemoryAddress(0);
  }
}

long v9ProcessorImpl::fetchInstruction_QemuImpl(VirtualMemoryAddress const & anAddress) {
  API::logical_address_t addr(anAddress);
  API::physical_address_t phy_addr = API::QEMU_logical_to_physical(*this, API::QEMU_DI_Instruction, addr);
  checkException();

  long op_code = Qemu::API::QEMU_read_phys_memory( *this, phy_addr, 4);
  checkException();

  return op_code;
}

bool cacheable(API::v9_memory_transaction_t & xact) {
  return (xact.cache_virtual && xact.cache_physical );
}

bool side_effect(API::v9_memory_transaction_t & xact) {
  return (xact.side_effect || xact.s.inverse_endian);
}

std::tuple<PhysicalMemoryAddress, bool, bool> v9ProcessorImpl::translateTSB_QemuImpl(VirtualMemoryAddress anAddress, int anASI) const {
  //Check for known special-case ASIs

  try {
    API::v9_memory_transaction_t xact;
    translate_QemuImpl( xact, anAddress, anASI);
    //return std::make_tuple( PhysicalMemoryAddress( xact.s.physical_address ), (xact.s.physical_address != 0 ) && cacheable(xact), (xact.s.physical_address == 0 ) || side_effect(xact)  );
    return std::make_tuple( PhysicalMemoryAddress( xact.s.physical_address ), (xact.s.physical_address != 0 ) && cacheable(xact), (xact.s.physical_address == 0 ) || side_effect(xact)  );
  } catch (MemoryException & anError ) {
    //return std::make_tuple( PhysicalMemoryAddress(0), false, true);
    return std::make_tuple( PhysicalMemoryAddress(0), false, true);
  }
}

unsigned long long endianFlip(unsigned long long val, int aSize) {
  unsigned long long ret_val = 0;
  switch (aSize) {
    case 1:
      ret_val = val;
      break;
    case 2:
      ret_val = ((val & 0xFF00) >> 8) | ((val & 0x00FF) << 8);
      break;
    case 4:
      ret_val = ((val & 0xFF000000) >> 24) | ((val & 0xFF0000) >> 8) | ((val & 0xFF00) << 8) | ((val & 0xFF) << 24);
      break;
    case 8:
      ret_val =
        ((val & 0xFF00000000000000ULL) >> 56) | ((val & 0xFF000000000000ULL) >> 40) | ((val & 0xFF0000000000ULL) >> 24) | ((val & 0xFF00000000ULL) >> 8) |
        ((val & 0xFFULL) << 56)               | ((val & 0xFF00ULL) << 40)           | ((val & 0xFF0000ULL) << 24)      | ((val & 0xFF000000ULL) << 8)   ;
      break;
    default:
      DBG_Assert( false, ( << "Unsupported size in endian-flip: " << aSize) );
  }
  return ret_val;
}

unsigned long long v9ProcessorImpl::readVAddr_QemuImpl(VirtualMemoryAddress anAddress, int anASI, int aSize) const {
  try {
    API::v9_memory_transaction_t xact;
    translate_QemuImpl( xact, anAddress, anASI );
    unsigned long long value = Qemu::API::QEMU_read_phys_memory( *this, xact.s.physical_address, aSize);
    checkException();

    return value;
  } catch (MemoryException & anError ) {
    return 0;
  }
}

unsigned long long v9ProcessorImpl::readVAddrXendian_QemuImpl(VirtualMemoryAddress anAddress, int anASI, int aSize) const {

  try {
    API::v9_memory_transaction_t xact;
    translate_QemuImpl( xact, anAddress, anASI);

    DBG_(VVerb, ( << "Virtual: " << anAddress << " ASI: " << anASI << " Size: " << aSize << " Physical: " << xact.s.physical_address) );

    unsigned long long value = Qemu::API::QEMU_read_phys_memory( *this, xact.s.physical_address, aSize);
    checkException();

    if (xact.s.inverse_endian) {
      DBG_(Verb, ( << "Inverse endian access to " << anAddress << " ASI: " << anASI << " Size: " << aSize ) );
      value = endianFlip(value, aSize);
    }

    return value;
  } catch (MemoryException & anError ) {
    if (anASI == 0x80) {
      //Try again using the CPU to translate
      try {
        API::logical_address_t addr(anAddress);
        API::physical_address_t phy_addr = API::QEMU_logical_to_physical(*this, API::QEMU_DI_Data, addr);
        checkException();
        unsigned long long value = Qemu::API::QEMU_read_phys_memory( *this, phy_addr, aSize);
        checkException();
        return value;
      } catch (MemoryException & anError ) {}
    }
    return 0;
  }
}

void v9ProcessorImpl::translate_QemuImpl(  API::v9_memory_transaction_t & xact, VirtualMemoryAddress anAddress, int anASI ) const {

  if (! isTranslatingASI(anASI)) {
    throw MemoryException();
  }

  DBG_( Verb, ( << "Translating " << anAddress << " in ASI: " << std::hex << anASI << std::dec ) );
  API::logical_address_t addr(anAddress);
  memset( &xact, 0, sizeof(API::v9_memory_transaction_t ) );
  xact.priv = 1;
#if defined(CONFIG_QEMU)
  xact.access_type = API::V9_Access_Normal;
#elif SIM_VERSION < 1200
  xact.align_kind = API::Align_Natural;
#else
  //align_kind was replaced by access_type in Simics 2.2.x
  xact.access_type = API::V9_Access_Normal;
#endif
  xact.address_space = anASI;
  xact.s.logical_address = addr;
  xact.s.size = 4;
  xact.s.type = API::QEMU_Trans_Load;
  xact.s.inquiry = 1;
  xact.s.ini_type = API::QEMU_Initiator_Other;
  xact.s.exception = API::QEMU_PE_No_Exception;

  API::exception_type_t except;
  DBG_Assert(mmu());
  //except = mmu()->logical_to_physical( theMMU, &xact ) ;  //ALEX - FIXME: Need an MMU API in QEMU
  except = API::QEMU_PE_No_Exception;	//temp dummy

  if (anASI == 0x80) {
    //Translate via the CPU as well, to confirm that it gives the same paddr
    //as the MMU.  If it doesn't, override the MMU's answer with the CPU's
    //answer
    API::logical_address_t addr(anAddress);
    API::physical_address_t phy_addr = API::QEMU_logical_to_physical(*this, API::QEMU_DI_Data, addr);
    checkException();
    if (phy_addr != xact.s.physical_address ) {
      DBG_(Verb, ( << "CPU[" << Qemu::API::QEMU_get_processor_number(*this) << "] Translation difference between CPU and MMU for " << anAddress << ".  Using CPU translation of " << PhysicalMemoryAddress(phy_addr) ));
      xact.s.physical_address = phy_addr;
      return;
    }
  }

  if ( except != API::QEMU_PE_No_Exception ) {
    DBG_( Verb, ( << "Exception during translation: " << except ) );
    throw MemoryException();
  }

}

MMU::mmu_t v9ProcessorImpl::getMMU() {
  return theMMUs[id()];
}

void v9ProcessorImpl::ckptMMU() {
  DBG_(Verb, ( << "CPU[" << Qemu::API::QEMU_get_processor_number(*this) << "] checkpointing MMU. size=" << theMMUckpts[id()].size()));
  theMMUckpts[id()].push_back(theMMUs[id()]);
}

void v9ProcessorImpl::releaseMMUCkpt() {
  DBG_(Verb, ( << "CPU[" << Qemu::API::QEMU_get_processor_number(*this) << "] releasing oldest MMU checkpoint. size=" << theMMUckpts[id()].size()));
  DBG_Assert(!theMMUckpts[id()].empty(), ( << "CPU[" << Qemu::API::QEMU_get_processor_number(*this) << "] has no checkpoint to release"));
  theMMUckpts[id()].pop_front();
}

void v9ProcessorImpl::rollbackMMUCkpts(int n) {
  DBG_(Verb, ( << "CPU[" << Qemu::API::QEMU_get_processor_number(*this) << "] rolling back " << n << " MMU checkpoints"));
  DBG_Assert(theMMUckpts[id()].size() > (unsigned)n, ( << "CPU[" << Qemu::API::QEMU_get_processor_number(*this) << "] has " << theMMUckpts[id()].size() << " but needs > " << n));
  // remove n checkpoints to get back to where we started
  for (int i = 0; i < n; ++i) theMMUckpts[id()].pop_back();
  theMMUs[id()] = theMMUckpts[id()].back();
  theMMUckpts[id()].pop_back();
}

void v9ProcessorImpl::resyncMMU() {
  //MMU::fm_init_mmu_from_simics(&theMMUs[id()], SIM_get_attribute(*this, "mmu").u.object );	//ALEX - FIXME
}

bool v9ProcessorImpl::validateMMU(MMU::mmu_t * anMMU) {
  MMU::mmu_t simics_mmu, *our_mmu;
  //MMU::fm_init_mmu_from_simics(&simics_mmu, SIM_get_attribute(*this, "mmu").u.object);  //ALEX - FIXME
  assert(false);
  if (anMMU == NULL) our_mmu = &(theMMUs[id()]);
  else our_mmu = anMMU;
  if (MMU::fm_compare_mmus(our_mmu, &simics_mmu)) {
    return false;
  }
  return true;
}

unsigned long long v9ProcessorImpl::mmuRead(VirtualMemoryAddress anAddress, int anASI) {
  MMU::mmu_access_t access;
  access.va = anAddress;
  access.asi = anASI;
  access.type = MMU::mmu_access_load;
  mmu_access( &theMMUs[id()], & access );
  return access.val;
}

void v9ProcessorImpl::mmuWrite(VirtualMemoryAddress anAddress, int anASI, unsigned long long aValue) {
  MMU::mmu_access_t access;
  access.va = anAddress;
  access.asi = anASI;
  access.type = MMU::mmu_access_store;
  access.val = aValue;
  mmu_access( &theMMUs[id()], & access );
}

void v9ProcessorImpl::dumpMMU(MMU::mmu_t * anMMU) {
  MMU::mmu_t * m = (anMMU == NULL) ? &(theMMUs[id()]) : anMMU;
  MMU::fm_print_mmu_regs(m);
}

void v9ProcessorImpl::translate_MMUImpl(Translation & aTranslation, bool aTakeException) const {
  MMU::asi_class_t asi_class(MMU::CLASS_ASI_PRIMARY);
  int asi_info = 0;

  //Address mask

  /* Get ASI class: primary, secondary, or nucleus */
  if (aTranslation.theType == Translation::eFetch) {
    asi_class = (aTranslation.theTL == 0) ? MMU::CLASS_ASI_PRIMARY : MMU::CLASS_ASI_NUCLEUS;
    aTranslation.theASI = 0x80;
  } else {
    asi_info = asi_info_table[aTranslation.theASI];  /* this table filled in for us by Simics */
    if (asi_info & Sim_ASI_Translating) {	//ALEX - Where do these Sim_ASI_* come from?
      if (asi_info & Sim_ASI_Primary) {
        /* TODO: this should be asi_class = CLASS_ASI_PRIMARY once I fix the asi itself to be ASI_NUCLEUS */
        if (asi_info & Sim_ASI_As_If_User || aTranslation.theTL == 0) {
          asi_class = MMU::CLASS_ASI_PRIMARY;
        } else {
          asi_class = MMU::CLASS_ASI_NUCLEUS;
        }
      } else if (asi_info & Sim_ASI_Secondary) {
        if (asi_info & Sim_ASI_As_If_User || aTranslation.theTL == 0) {
          asi_class = MMU::CLASS_ASI_SECONDARY;
        } else {
          asi_class = MMU::CLASS_ASI_NUCLEUS;
        }
      } else if (asi_info & Sim_ASI_Nucleus) {
        asi_class = MMU::CLASS_ASI_NUCLEUS;
      } else {
        DBG_Assert( false, ( << "Unknown ASI: " << aTranslation.theASI));
      }
    } else {
      asi_class = MMU::CLASS_ASI_NONTRANSLATING;
    }
  }

  if (asi_class == MMU::CLASS_ASI_NONTRANSLATING) {
    aTranslation.theTTEEntry = 0;
    aTranslation.thePaddr = PhysicalMemoryAddress(aTranslation.theVaddr);
    aTranslation.theException = 0;
  } else {

    DBG_( Verb, ( << "Translating " << aTranslation.theVaddr << " in ASI: " << std::hex << aTranslation.theASI << std::dec ) );

    if (aTranslation.thePSTATE & 0x8) {
      //Address mask bit in PSTATE register is set - clear top 32 bits
      aTranslation.theVaddr = VirtualMemoryAddress(aTranslation.theVaddr & 0xFFFFFFFFULL);
    }

    MMU::mmu_translation_type_t mode = ( aTakeException ) ? MMU::MMU_TRANSLATE : MMU::MMU_TRANSLATE_PF;

    MMU::mmu_exception_t exception(MMU::no_exception);

    MMU::tte_data data = MMU::mmu_lookup(&theMMUs[id()],
                                         (aTranslation.theType == Translation::eFetch),
                                         aTranslation.theVaddr,
                                         asi_class,
                                         aTranslation.theASI,
                                         false /*no_fault*/,
                                         aTranslation.thePSTATE & 0x4,
                                         ((aTranslation.theType == Translation::eStore) ? MMU::mmu_access_store : MMU::mmu_access_load),
                                         & exception,
                                         mode);
    aTranslation.theTTEEntry = data;
    aTranslation.thePaddr = PhysicalMemoryAddress(MMU::mmu_make_paddr(data, aTranslation.theVaddr));
    aTranslation.theException = exception;
    if (exception != MMU::no_exception) {
      aTranslation.thePaddr = PhysicalMemoryAddress(0);
    }
  }

}

long v9ProcessorImpl::fetchInstruction_MMUImpl(Translation & aTranslation, bool aTakeTrap) {
  long op_code = 0;

  translate_MMUImpl(aTranslation, aTakeTrap );
  if (aTranslation.thePaddr != 0) {
    try {
      op_code = Qemu::API::QEMU_read_phys_memory( *this, aTranslation.thePaddr, 4);
      checkException();
    } catch (...) {
      op_code = 0;
    }
  }

  return op_code;
}

unsigned long long v9ProcessorImpl::readVAddr_MMUImpl(Translation & aTranslation, int aSize) const {
  try {
    translate_MMUImpl(aTranslation, false);

    DBG_(VVerb, ( << "Virtual: " << aTranslation.theVaddr << " ASI: " << aTranslation.theASI << " Size: " << aSize << " Physical: " << aTranslation.thePaddr ) );

    unsigned long long value = Qemu::API::QEMU_read_phys_memory( *this, aTranslation.thePaddr, aSize);
    checkException();

    return value;
  } catch (MemoryException & anError ) { }
  return 0;
}

unsigned long long v9ProcessorImpl::readVAddrXendian_MMUImpl(Translation & aTranslation, int aSize) const {
  try {
    translate_MMUImpl(aTranslation, false);

    DBG_(VVerb, ( << "Virtual: " << aTranslation.theVaddr << " ASI: " << aTranslation.theASI << " Size: " << aSize << " Physical: " << aTranslation.thePaddr ) );

    unsigned long long value = Qemu::API::QEMU_read_phys_memory( *this, aTranslation.thePaddr, aSize);
    checkException();

    if (aTranslation.isXEndian()) {
      DBG_(Verb, ( << "Inverse endian access to " << aTranslation.theVaddr  << " ASI: " << aTranslation.theASI  << " Size: " << aSize ) );
      value = endianFlip(value, aSize);
    }

    if (aTranslation.theASI == 0x80 && (aTranslation.thePSTATE & 0x100 /*PSTATE.CLE*/)) {
      DBG_(Verb, ( << "PSTATE.CLE set.  Inverting endianness of  " << aTranslation.theVaddr  << " ASI: " << aTranslation.theASI  << " Size: " << aSize ) );
      value = endianFlip(value, aSize);
    }

    return value;
  } catch (MemoryException & anError ) { }
  return 0;
}

bool Translation::isCacheable() {
  return MMU::mmu_is_cacheable( theTTEEntry );
}
bool Translation::isSideEffect() {
  return MMU::mmu_is_sideeffect( theTTEEntry );
}
bool Translation::isXEndian() {
  return MMU::mmu_is_xendian( theTTEEntry );
}

bool Translation::isMMU() {
  switch (theASI) {
    case 0x50: //I-MMU
    case 0x51: //I-MMU TSB_8KB_PTR_REG
    case 0x52: //I-MMU TSB_64KB_PTR_REG
    case 0x54: //ITLB_DATA_IN_REG
    case 0x57: //IMMU_DEMAP
    case 0x58: //D-MMU
    case 0x59: //D-MMU TSB_8KB_PTR_REG
    case 0x5A: //D-MMU TSB_64KB_PTR_REG
    case 0x5C: //DTLB_DATA_IN_REG
    case 0x5D: //DTLB_DATA_ACCESS_REG
    case 0x5F: //DMMU_DEMAP
      return true;

    default: //all others
      return false;
  }
}
bool Translation::isInterrupt() {
  switch (theASI) {
    case 0x48: //ASI_INTR_DISPATCH_STATUS
    case 0x49: //ASI_INTR_RECEIVE
    case 0x77: //ASI_INTR_DATA_W
    case 0x7F: //ASI_INTR_DATA_R
    case 0x4a: //ASI_FIREPLANE
      return true;

    default: //all others
      return false;
  }
}

bool Translation::isTranslating() {
  return isTranslatingASI(theASI);
}

namespace MMU {

using namespace Flexus::Qemu::API;

#define U_BIT_MASK (1ULL<<43)

static unsigned long long page_mask[4] = {0x1FFF, 0xFFFF, 0x7FFFF, 0x3FFFFF};

address_t mmu_make_paddr(tte_data data, address_t va) {
  int size = (data >> 61ULL) & 3;
  address_t pa = data;
  pa &= ~0xFFFFF80000000000ULL; /* all things above PA */
  pa &= ~page_mask[size];
  pa |= (va & page_mask[size]);
  return pa;
}

#define W_BIT (1ULL<<1)
#define E_BIT (1ULL<<3)
#define CV_BIT (1ULL<<4)
#define CP_BIT (1ULL<<5)
#define L_BIT (1ULL<<6)
#define U_BIT (1ULL<<43)
#define IE_BIT (1ULL<<59)
#define V_BIT (1ULL<<63)

#define CTXT_MASK 0x1FFF

bool mmu_is_cacheable(tte_data data) {
  return (!! (data & CV_BIT)) && (!! (data & CP_BIT));
}

bool mmu_is_sideeffect(tte_data data) {
  return !!(data & E_BIT);
}

bool mmu_is_xendian(tte_data data) {
  return !!(data & IE_BIT);
}

bool mmu_is_writeable(tte_data data) {
  return !!(data & W_BIT);
}

address_t
mmu_translate(mmu_t * mmu,
              unsigned int is_fetch,
              address_t va,
              unsigned int klass,
              unsigned int asi,
              unsigned int nofault,
              unsigned int priv,
              unsigned int access_type,
              mmu_exception_t * except,
              mmu_translation_type_t trans_type) {
  tte_data data = mmu_lookup(mmu, is_fetch, va, klass, asi, nofault, priv, access_type, except, trans_type);
  if (*except != no_exception) {
    return 0;
  }
  return mmu_make_paddr(data, va);
}

tte_data
mmu_lookup(mmu_t * mmu,
           unsigned int is_fetch,
           address_t va,
           unsigned int klass,
           unsigned int asi,
           unsigned int nofault,
           unsigned int priv,
           unsigned int access_type,
           mmu_exception_t * except,
           mmu_translation_type_t trans_type) {
  mmu_reg_t ctxt = 0;
  mmu_reg_t test_ctxt, test_global;
  mmu_reg_t hit_data = 0;
  mmu_reg_t hit_test_ctxt = 0, hit_test_global = 0;
  int way, hit_flag, hit_size = 0;
  mmu_reg_t tsb_ext = 0;

  tte_tag * tag_2w, *data_2w, *tag_fa, *data_fa;
  tlb_regs_t * regs;
  int sets_2w, ways_fa;
  address_t tsb_base, tsb_reg;
  unsigned int tsb_split, tsb_size;
  *except = no_exception;

  /* prelims */
  if (is_fetch) {
    sets_2w = 64;
    ways_fa = 16;
    tag_2w  = mmu->it128_tag;
    data_2w = mmu->it128_data;
    tag_fa  = mmu->it16_tag;
    data_fa = mmu->it16_data;
    regs    = &(mmu->i_regs);
  } else {
    sets_2w = 256;
    ways_fa = 16;
    tag_2w  = mmu->dt512_tag;
    data_2w = mmu->dt512_data;
    tag_fa  = mmu->dt16_tag;
    data_fa = mmu->dt16_data;
    regs    = &(mmu->d_regs);
  }

  /* procedure:
   *  1. get context from appropriate context reg (use class)
   *  2. search 2-way TLB
   *       if hit, set PA
   *  3.   else miss, search FA TLB
   *         if hit, set PA
   *  4.     else miss, set exception
   */

  /* get context */
  switch (klass) {
    case CLASS_ASI_PRIMARY:
      ctxt = mmu->primary_context;
      tsb_ext = regs->tsb_px;
      break;
    case CLASS_ASI_SECONDARY:
      ctxt = mmu->secondary_context;
      tsb_ext = regs->tsb_sx;
      break;
    case CLASS_ASI_NUCLEUS:
      ctxt = 0;  /* hardwired */
      tsb_ext = regs->tsb_nx;
      break;
    default:
      /* TODO: set exception? */
      break;
  }

  DBG_( Verb,  ( << "DBG: context = 0x" << ctxt));

  hit_flag = 0;

  /* 2. look in 2-way table */
  for (way = 0; way < 2; way++) {
    int idx = sets_2w * way + ((va >> 13ULL) % sets_2w); /* FIXME: may not work w/ non-8K pages */
    int size = (data_2w[idx] >> 61ULL) & 3;
    //test_ctxt   = (ctxt << 48ULL) | (va & ~page_mask[size]);
    test_ctxt   = (ctxt) | (va & ~page_mask[size]);
    test_global = (1ULL << 63ULL) | (va & ~page_mask[size]);
    DBG_( VVerb, ( << " idx=" << idx << " tag_2w=" << std::hex << tag_2w[idx] << " data_2w=" << data_2w[way] << " test_ctxt=" << test_ctxt << " test_global=" << test_global << std::dec ) );
    if (tag_2w[idx] == test_ctxt || tag_2w[idx] == test_global) {
      /* hit! */
      hit_flag = 1;
      hit_data = data_2w[idx];
      hit_test_ctxt   = test_ctxt;
      hit_test_global = test_global;

      if (trans_type == MMU_DEMAP_PAGE) {
        tag_2w[idx] = 0;
        data_2w[idx] &= ~(1ULL << 63);
      } else {
        if (trans_type == MMU_TRANSLATE) data_2w[idx] |= U_BIT_MASK;
        break;
      }
    }
  }

  /* 3. look in FA table */
  if (!hit_flag || trans_type == MMU_DEMAP_PAGE) {
    for (way = 0; way < ways_fa; way++) {
      int size = (data_fa[way] >> 61ULL) & 3;
      //test_ctxt   = (ctxt << 48ULL) | (va & ~page_mask[size]);
      test_ctxt   = (ctxt) | (va & ~page_mask[size]);
      test_global = (1ULL << 63ULL) | (va & ~page_mask[size]);
      DBG_( VVerb, ( << " way=" << way << " tag_fa=" << std::hex << tag_fa[way] << " data_fa=" << data_fa[way] << " test_ctxt=" << test_ctxt << " test_global=" << test_global << std::dec ) );
      if (tag_fa[way] == test_ctxt || tag_fa[way] == test_global) {
        /* hit! */
        hit_flag = 1;
        hit_data = data_fa[way];
        hit_test_ctxt   = test_ctxt;
        hit_test_global = test_global;

        if (trans_type == MMU_DEMAP_PAGE) {
          tag_fa[way] = 0;
          data_fa[way] &= ~(1ULL << 63);
        } else {
          if (trans_type == MMU_TRANSLATE) data_fa[way] |= U_BIT_MASK;
          break;
        }
      }
    }
  }

  /* fill in return details */
  if (!hit_flag) {
    if (trans_type == MMU_TRANSLATE || trans_type == MMU_TRANSLATE_PF) {
      unsigned int ow;
      /* take actions on a TLB miss */

      //FIXME - add a way for I-fetch accesses to probe without causing trap
      if (is_fetch) {
        DBG_(Verb, ( << "I-TLB miss to va: " << std::hex << va << std::dec ));

        *except = fast_instruction_access_MMU_miss;

        if (trans_type == MMU_TRANSLATE) {

          /* update registers */
          mmu->i_regs.tsb_tag_target = (ctxt << 48) | (va >> 22);
          mmu->i_regs.tag_access = (va & ~0x1FFF) | ctxt;

          tsb_reg   =  mmu->i_regs.tsb ^ tsb_ext;
          tsb_base  =  tsb_reg & ~0x1FFF;
          tsb_split = (tsb_reg & 0x1000) >> 12;
          tsb_size  =  tsb_reg & 0x7;
          mmu->i_regs.tsbp8k  = mmu_generate_tsb_ptr(va, MMU_TSB_8K_PTR, tsb_base,
                                tsb_split, tsb_size, tsb_ext);
          mmu->i_regs.tsbp64k = mmu_generate_tsb_ptr(va, MMU_TSB_64K_PTR, tsb_base,
                                tsb_split, tsb_size, tsb_ext);

          /* update SFSR -- requires various state info */
          ow  = (mmu->i_regs.sfsr & 0x2) >> 1;  /* set if fv still set */
          mmu->i_regs.sfsr = (nofault << 24)
                             | (asi << 16)
                             | (is_fetch << 15)
                             | (klass << 4)
                             | (priv << 3)
                             | (ow << 1)
                             | (0 /* fv */ );
        }
      } else {
        *except = fast_data_access_MMU_miss;

        if (trans_type == MMU_TRANSLATE) {
          /* update registers */
          mmu->d_regs.tsb_tag_target = (ctxt << 48) | (va >> 22);
          mmu->d_regs.sfar = va;
          mmu->d_regs.tag_access = (va & ~0x1FFF) | ctxt;

          /* generate tsb pointers */
          tsb_reg   =  mmu->d_regs.tsb ^ tsb_ext;
          tsb_base  =  tsb_reg & ~0x1FFF;
          tsb_split = (tsb_reg & 0x1000) >> 12;
          tsb_size  =  tsb_reg & 0x7;
          mmu->d_regs.tsbp8k  = mmu_generate_tsb_ptr(va, MMU_TSB_8K_PTR, tsb_base,
                                tsb_split, tsb_size, tsb_ext);
          mmu->d_regs.tsbp64k = mmu_generate_tsb_ptr(va, MMU_TSB_64K_PTR, tsb_base,
                                tsb_split, tsb_size, tsb_ext);

          /* update SFSR -- requires various state info */
          ow  = (mmu->d_regs.sfsr & 0x2) >> 1;  /* set if fv still set */
          mmu->d_regs.sfsr = (nofault << 24)
                             | (asi << 16)
                             | (!is_fetch << 15)
                             | (klass << 4)
                             | (priv << 3)
                             | ((access_type != 1 /* FIXME: load */)  << 2)
                             | (ow << 1)
                             | (1 /* fv */ );
        }
      }
    }
  } else {
    /* check for protection violations */
    if (access_type == mmu_access_store && !mmu_is_writeable(hit_data)) {
      *except = fast_data_access_protection;

      if (trans_type == MMU_TRANSLATE) {
        unsigned int ow;
        /* update registers */
        mmu->d_regs.tsb_tag_target = (ctxt << 48) | (va >> 22);
        mmu->d_regs.sfar = va;
        mmu->d_regs.tag_access = (va & ~0x1FFF) | ctxt;

        /* generate tsb pointers */
        tsb_reg   =  mmu->d_regs.tsb ^ tsb_ext;
        tsb_base  =  tsb_reg & ~0x1FFF;
        tsb_split = (tsb_reg & 0x1000) >> 12;
        tsb_size  =  tsb_reg & 0x7;
        mmu->d_regs.tsbp8k  = mmu_generate_tsb_ptr(va, MMU_TSB_8K_PTR, tsb_base,
                              tsb_split, tsb_size, tsb_ext);
        mmu->d_regs.tsbp64k = mmu_generate_tsb_ptr(va, MMU_TSB_64K_PTR, tsb_base,
                              tsb_split, tsb_size, tsb_ext);

        /* update SFSR -- requires various state info */
        ow  = (mmu->d_regs.sfsr & 0x2) >> 1;  /* set if fv still set */
        mmu->d_regs.sfsr = (nofault << 24)
                           | (asi << 16)
                           | (!is_fetch << 15)
                           | (klass << 4)
                           | (priv << 3)
                           | ((access_type != 1 /* FIXME: load */)  << 2)
                           | (ow << 1)
                           | (1 /* fv */ );
      }
    }
  }

  DBG_( Verb, ( << "MMU va: " << std::hex << va << " is_fetch: " << is_fetch << " hit: " << hit_flag << " test_ctxt: " << hit_test_ctxt << " test_global " << hit_test_global << " hit_data: " << hit_data << " size: " << hit_size ) );

  return hit_data;
}

address_t
mmu_generate_tsb_ptr(address_t va,
                     mmu_ptr_type_t type,
                     address_t tsb_base_in,
                     unsigned int tsb_split,
                     unsigned int tsb_size,
                     mmu_reg_t tsb_ext) {
  address_t tsb_base = tsb_base_in;
  address_t va_portion;
  address_t tsb_base_mask;
  address_t split_mask;

  va_portion = (va >> ((type == MMU_TSB_8K_PTR) ? 9 : 12)) & 0xFFFFFFFFFFFFFFF0ULL;

  //tsb_base ^= tsb_ext; -- performed before calling mmu_generate_tsb_ptr
  va_portion ^= tsb_ext << 1 & 0x1ff0;
  va_portion ^= tsb_ext & 0x1fe000;

  tsb_base_mask = 0xFFFFFFFFFFFFe000ULL << (tsb_split ? (tsb_size + 1) : tsb_size);

  if (tsb_split) {
    split_mask = 1 << (13 + tsb_size);
    if (type == MMU_TSB_8K_PTR) {
      va_portion &= ~split_mask;
    } else {
      va_portion |= split_mask;
    }
  }
  return (tsb_base & tsb_base_mask) | (va_portion & ~tsb_base_mask);
}

int mmu_fa_choose(mmu_t * mmu ) {
  //First invalid entry
  int i;
  for (i = 0; i < 16; ++i) {
    if ((mmu->dt16_data[i] & V_BIT) == 0) {
      return i;
    }
  }

  //First unlocked, unused entry
  for (i = 0; i < 16; ++i) {
    if (((mmu->dt16_data[i] & L_BIT) == 0) && ((mmu->dt16_data[i] & U_BIT) == 0)) {
      return i;
    }
  }

  //Clear all used bits
  for (i = 0; i < 16; ++i) {
    mmu->dt16_data[i] &= ~ U_BIT;
  }

  //First unlocked entry
  for (i = 0; i < 16; ++i) {
    if ((mmu->dt16_data[i] & L_BIT) == 0) {
      return i;
    }
  }

  //Undefined behavior if all entries locked.
  DBG_Assert( false, ( << "All entries in fully-associative TLB are locked" ) );
  return 0;
}

void
mmu_access(mmu_t * mmu, mmu_access_t * access) {
  unsigned i = 0, tlb = 0;
  unsigned ctxt = 0, type = 0;
  unsigned context = 0;
  address_t va = 0;
  unsigned size = 0;
  mmu_exception_t except;

  switch (access->asi) {

    case 0x58:  /* D-MMU */
      switch (access->va) {
        case 0x0:     /* dTSB Tag Target Registers (read-only) */
          DBG_( Verb, ( << "ASI_DMMU tag target read: " << std::hex << mmu->d_regs.tsb_tag_target));
          assert(access->type == mmu_access_load);
          access->val = mmu->d_regs.tsb_tag_target;
          break;
        case 0x8:     /* Primary Context Register */
          if (access->type == mmu_access_store) {
            DBG_( Verb, ( << "ASI_DMMU prim ctxt write: " << std::hex << access->val));
            mmu->primary_context = access->val;
          } else {
            DBG_( Verb, ( << "ASI_DMMU prim ctxt read: " << std::hex << mmu->primary_context));
            access->val = mmu->primary_context;
          }
          break;
        case 0x10:    /* Secndary Context Register */
          if (access->type == mmu_access_store) {
            DBG_( Verb, ( << "ASI_DMMU scndry ctxt write: " << std::hex << access->val));
            mmu->secondary_context = access->val;
          } else {
            DBG_( Verb, ( << "ASI_DMMU scndry ctxt read: " << std::hex << mmu->secondary_context));
            access->val = mmu->secondary_context;
          }
          break;
        case 0x28:    /* dTSB Base Registers */
          if (access->type == mmu_access_store) {
            DBG_( Verb, ( << "ASI_DMMU dTSB base write: " << std::hex << access->val));
            mmu->d_regs.tsb = access->val;
          } else {
            DBG_( Verb, ( << "ASI_DMMU dTSB base read: " << std::hex << mmu->d_regs.tsb));
            access->val = mmu->d_regs.tsb;
          }
          break;
        case 0x30:    /* D-TLB Tag Access Registers */
          /* updates tag access reg and tag target and tsb pointer regs */
          if (access->type == mmu_access_load) {
            DBG_( Verb, ( << "ASI_DMMU tag access read: " << std::hex << mmu->d_regs.tag_access));
            access->val = mmu->d_regs.tag_access;
          } else {
            DBG_( Verb, ( << "ASI_DMMU tag access write: " << std::hex << access->val));
            mmu->d_regs.tag_access = access->val;
            //mmu->d_regs.sfar = access->val;
          }
          break;
        case 0x18:    /* D-SFSR */
        case 0x20:    /* D-SFAR (read-only) */
        case 0x38:    /* Virtual Watchpoint Address */
        case 0x40:    /* Physical Watchpoint Address */
        case 0x48:    /* dTSB Primary Extension Register */
        case 0x50:    /* dTSB Secondary Extension Register */
        case 0x58:    /* dTSB Nucleus Extension Registers */
        default:
          //DBG_Assert( false, ( << "ERROR: unhandled D-MMU access " << access->va));
          DBG_( Crit, ( << "ERROR: unhandled D-MMU access 0x" << std::hex << access->va));
          break;
      }
      break;

    case 0x59:  /* D-MMU TSB_8KB_PTR_REG */
      DBG_( Verb, ( << "DTLB_TSB_8KB_PTR_REG tsbp64k: " << std::hex << mmu->d_regs.tsbp8k << " val: " << access->val) );
      switch (access->va) {
        case 0x0:
          access->val = mmu->d_regs.tsbp8k;
          break;
        default:
          DBG_( Crit, ( << "ERROR: unhandled D-MMU access " << access->va));
          //DBG_Assert( false, ( << "ERROR: unhandled D-MMU access " << access->va));
          break;
      }
      break;

    case 0x5A:  /* D-MMU TSB_64KB_PTR_REG */
      DBG_( Verb, ( << "DTLB_TSB_64KB_PTR_REG tsbp64k: " << std::hex << mmu->d_regs.tsbp64k << " val: " << access->val) );
      switch (access->va) {
        case 0x0:
          access->val = mmu->d_regs.tsbp64k;
          break;
        default:
          DBG_( Crit, ( << "ERROR: unhandled D-MMU access " << access->va));
          //DBG_Assert( false, ( << "ERROR: unhandled D-MMU access " << access->va));
          break;
      }
      break;

    case 0x5C:  /* DTLB_DATA_IN_REG */
      /* store value */
      DBG_( Verb, ( << "DTLB_DATA_IN_REG tag: " << std::hex << mmu->d_regs.tag_access << " data: " << access->val) );
      DBG_Assert(access->type == mmu_access_store);
      DBG_Assert(access->va == 0);
      /* printf("Data in: tag=0x%016llx data=0x%016llx\n", mmu->d_regs.tag_access, access->val); */

      size = (access->val >> 61ULL) & 3;

      if ( ((access->val >> 63ULL) & 1) &&   /* valid */
           ((access->val >> 5ULL)  & 1) &&   /* CP */
           !((access->val >> 59ULL) & 1) &&  /* !IE */
           !((access->val >> 60ULL) & 1) &&  /* !NFO */
           (size == 0)) {                    /* 8KB page */
        access->val |= U_BIT_MASK;
      }

      /* choose victim, replace */
      if (((access->val >> 6) & 1) || size != 0) {
        /* fill in FA table if locked page or size !0 */

        int fa_idx = mmu_fa_choose(mmu);
        DBG_( Verb, ( << "DTLB fa replacement in " << fa_idx << std::hex
                      << " tag was: "  << mmu->dt16_tag[fa_idx]
                      << " tag is: "   << (mmu->d_regs.tag_access & ( ~page_mask[size] | CTXT_MASK))
                      << " data was: " << mmu->dt16_data[fa_idx]
                      << " data is: "  << access->val));
        mmu->dt16_tag[fa_idx] = ( mmu->d_regs.tag_access & ( ~page_mask[size] | CTXT_MASK) );
        mmu->dt16_data[fa_idx] = access->val;
      } else {
        unsigned idx, idx0, idx1;

        /* fill in dt512 */
        //idx0 = ((mmu->d_regs.sfar >> 13ULL) % 256);
        //idx1 = 256 + ((mmu->d_regs.sfar >> 13ULL) % 256);
        idx0 = ((mmu->d_regs.tag_access >> 13ULL) % 256);
        idx1 = 256 + ((mmu->d_regs.tag_access >> 13ULL) % 256);
        if (mmu->dt512_data[idx0] >> 63 == 0) {
          /* invalid, fill here */
          idx = idx0;
        } else if (mmu->dt512_data[idx1] >> 63 == 0) {
          /* invalid, fill here */
          idx = idx1;
        } else {
          //Change LFSR only if its used
          mmu->lfsr = (mmu->lfsr + 1) % 4;
          /* both valid, choose victim from lfsr */
          idx = mmu->lfsr & 0x1 ? idx1 : idx0;
        }
        /* printf("filling in idx:%d\n", idx); */
        DBG_( Verb, ( << "DTLB 2w replacement in " << idx << std::hex
                      << " tag was: "  << mmu->dt512_tag[idx]
                      << " tag is: "   << (mmu->d_regs.tag_access & ( ~page_mask[size] | CTXT_MASK))
                      << " data was: " << mmu->dt512_data[idx]
                      << " data is: "  << access->val
                      << " bug tag: " << mmu->d_regs.tag_access));
        DBG_( Verb, ( << "filling in idx: " << idx ) );
        //mmu->dt512_tag[idx] = mmu->d_regs.tag_access;
        mmu->dt512_tag[idx] = ( mmu->d_regs.tag_access & ( ~page_mask[size] | CTXT_MASK));
        mmu->dt512_data[idx] = access->val;
      }
      break;

    case 0x5D:  /* DTLB_DATA_ACCESS_REG */
      DBG_( Verb, ( << "DTLB_DATA_ACCESS_REG tag: " << std::hex << mmu->d_regs.tag_access << " data: " << access->val) );
      i = (access->va >> 3) & 0x1FF;
      tlb = (access->va >> 16) & 0x3;
      size = (access->val >> 61ULL) & 3;
      if (tlb == 0) {  /* full-associative */
        DBG_Assert(i < 16, ( << "access out of range: i=" << i));
        if (access->type == mmu_access_store) {
          //mmu->dt16_tag[i] = mmu->d_regs.tag_access;
          DBG_( Verb, ( << "DTLB fa replacement in " << i << std::hex
                        << " tag was: "  << mmu->dt16_tag[i]
                        << " tag is: "   << (mmu->d_regs.tag_access & ( ~page_mask[size] | CTXT_MASK))
                        << " data was: " << mmu->dt16_data[i]
                        << " data is: "  << access->val
                        << " bug tag: " << mmu->d_regs.tag_access));
          mmu->dt16_tag[i] = ( mmu->d_regs.tag_access & ( ~page_mask[size] | CTXT_MASK) );
          mmu->dt16_data[i] = access->val;
        } else {
          access->val = mmu->dt16_data[i];
        }
      } else if (tlb == 1) {  /* 2-way */
        DBG_Assert( false, ( << "not sure if this case will work i=" << i));
      } else {
        DBG_Assert( false, ( << "invalid tlb value in data access op: " << tlb));
      }
      break;

    case 0x5F:  /* DMMU_DEMAP */
      DBG_( Verb, ( << "DMMU_DEMAP: " << std::hex << " va: " << access->va << " type: " << access->type));
      assert(access->type == mmu_access_store);
      ctxt = (access->va >> 4) & 0x3;
      type = (access->va >> 6) & 0x3;
      va = access->va & ~(0x1FFF);

      switch (ctxt) {
        case CLASS_ASI_PRIMARY:
          context = mmu->primary_context;
          break;
        case CLASS_ASI_SECONDARY:
          context = mmu->secondary_context;
          break;
        case CLASS_ASI_NUCLEUS:
          context = 0;  /* hard-wired */
          break;
      }

      switch (type) {
        case 0:  /* demap page */
          mmu_translate(mmu, 0, va, ctxt, 0, 0, 0, 0, &except, MMU_DEMAP_PAGE);
          break;
        case 1:  /* demap all from context */
          /** CAUTION: untested **/
          /* walk 2-way table */
          DBG_( Crit, ( << "DMMU_DEMAP UNTESTED: " << std::hex << " context: " << context));
          for (i = 0; i < 512; i++) {
            if ((mmu->dt512_tag[i] & 0x1FFF) == context) {
              mmu->dt512_tag[i] = 0;
              mmu->dt512_data[i] &= ~(1ULL << 63);
            }
          }
          /* walk FA table */
          for (i = 0; i < 16; i++) {
            if ((mmu->dt16_tag[i] & 0x1FFF) == context) {
              mmu->dt16_tag[i] = 0;
              mmu->dt16_data[i] &= ~(1ULL << 63);
            }
          }
          break;

        case 2:  /* demap all unlocked entries */
        default:
          DBG_( Crit, ( << "ERROR: unhandled demap type: " << type));
          break;
      }

      break;

    case 0x50:  /* I-MMU */
      switch (access->va) {
        case 0x0:     /* TSB Tag Target Registers (read-only) */
          assert(access->type == mmu_access_load);
          access->val = mmu->i_regs.tsb_tag_target;
          break;
        case 0x28:    /* dTSB Base Registers */
          assert(access->type == mmu_access_store);
          mmu->i_regs.tsb = access->val;
          break;
        case 0x30:    /* D-TLB Tag Access Registers */
          if (access->type == mmu_access_load) {
            access->val = mmu->i_regs.tag_access;
          } else {
            mmu->i_regs.tag_access = access->val;
          }
          break;
        default:
          //DBG_Assert( false, ( << "unhandled unhandled I-MMU access : " << access->va));
          DBG_( Crit, ( << "unhandled unhandled I-MMU access : " << access->va));
          /* FIXME: proper assert */
          break;
      }
      break;

    case 0x51:  /* I-MMU TSB_8KB_PTR_REG */
      switch (access->va) {
        case 0x0:
          access->val = mmu->i_regs.tsbp8k;
          break;
        default:
          //DBG_Assert( false, ( << "unhandled unhandled I-MMU access : " << access->va));
          DBG_( Crit, ( << "unhandled unhandled I-MMU access : " << access->va));
          break;
      }
      break;

    case 0x52:  /* I-MMU TSB_64KB_PTR_REG */
      switch (access->va) {
        case 0x0:
          access->val = mmu->i_regs.tsbp64k;
          break;
        default:
          DBG_( Crit, ( << "unhandled unhandled I-MMU access : " << access->va));
          //DBG_Assert( false, ( << "unhandled unhandled I-MMU access : " << access->va));
          break;
      }
      break;

    case 0x54:  /* ITLB_DATA_IN_REG */
      /* store value */
      assert(access->type == mmu_access_store);
      assert(access->va == 0);
      /* printf("Data in: tag=0x%016llx data=0x%016llx\n", mmu->i_regs.tag_access, access->val); */

      // set use bit now (simics does)
      access->val |= U_BIT_MASK;

      /* choose victim, replace */
      if (0  /* locked page */) {
        /* fill in FA table */
        DBG_Assert( false, ( << "fill in fA table -- locked page"));
      } else if (size !=  0 /* TTE size != PgSz0 */) {
        //TWENISCH - I am temporarily assuming that PgSz0 is always 0, even though this isn't true.
        //I can't figure out where PgSz0 really comes from.
        int fa_idx = mmu_fa_choose(mmu);
        DBG_( Verb, ( << "ITLB fa replacement in " << fa_idx ) );
        mmu->it16_tag[fa_idx] = ( mmu->i_regs.tag_access & ( ~page_mask[size] | CTXT_MASK) ) ;
        mmu->it16_data[fa_idx] = access->val;

      } else {
        unsigned idx, idx0, idx1;

        /* fill in it512 */
        idx0 = ((mmu->i_regs.tag_access >> 13ULL) % 64);
        idx1 = 64 + ((mmu->i_regs.tag_access >> 13ULL) % 64);
        if (mmu->it128_data[idx0] >> 63 == 0) {
          /* invalid, fill here */
          idx = idx0;
        } else if (mmu->it128_data[idx1] >> 63 == 0) {
          /* invalid, fill here */
          idx = idx1;
        } else {
          /* Change lfsr only if its used */
          mmu->lfsr = (mmu->lfsr + 1) % 4;

          /* both valid, choose victim from lfsr */
          idx = mmu->lfsr & 0x1 ? idx1 : idx0;
        }
        DBG_( Trace, ( << "filling in idx: " << idx));
        mmu->it128_tag[idx] = mmu->i_regs.tag_access;
        mmu->it128_data[idx] = access->val;
      }

      break;

    case 0x57:  /* IMMU_DEMAP */
      DBG_( Verb, ( << "IMMU_DEMAP: " << std::hex << " va: " << access->va << " type: " << access->type));
      assert(access->type == mmu_access_store);
      ctxt = (access->va >> 4) & 0x3;
      type = (access->va >> 6) & 0x3;
      va = access->va & ~(0x1FFF);

      switch (ctxt) {
        case CLASS_ASI_PRIMARY:
          context = mmu->primary_context;
          break;
        case CLASS_ASI_SECONDARY:
          context = mmu->secondary_context;
          break;
        case CLASS_ASI_NUCLEUS:
          context = 0;  /* hard-wired */
          break;
      }

      switch (type) {
        case 0:  /* demap page */
          mmu_translate(mmu, 1, va, ctxt, 0, 0, 0, 0, &except, MMU_DEMAP_PAGE);
          break;
        case 1:  /* demap all from context */
          DBG_( Crit, ( << "IMMU_DEMAP UNTESTED: " << std::hex << " context: " << context));
          /** CAUTION: untested **/
          /* walk 2-way table */
          for (i = 0; i < 128; i++) {
            if ((mmu->it128_tag[i] & 0x1FFF) == context) {
              mmu->it128_tag[i] = 0;
              mmu->it128_data[i] &= ~(1ULL << 63);
            }
          }
          /* walk FA table */
          for (i = 0; i < 16; i++) {
            if ((mmu->it16_tag[i] & 0x1FFF) == context) {
              mmu->it16_tag[i] = 0;
              mmu->it16_data[i] &= ~(1ULL << 63);
            }
          }
          break;

        case 2:  /* demap all unlocked entries */
        default:
          DBG_( Crit, ( << "ERROR: unhandled demap type: " << type));
          break;
      }
      break;

    default:
      DBG_( Crit, ( << "unhandled MMU ASI: " << access->asi));
      //DBG_Assert( false, ( << "unhandled MMU ASI: " << access->asi));
      /* FIXME: proper assert */
      break;
  }

}

#define FM_GET_BITS(val, msb, lsb) ((val >> lsb) & ((1ULL<<(msb-lsb+1)) - 1))

void
fm_print_mmu_regs(mmu_t * mmu) {
  tlb_regs_t * d_regs = &(mmu->d_regs);
  tlb_regs_t * i_regs = &(mmu->i_regs);

  printf("Context registers:\n");
  printf("  Primary ctxt:   0x%04llx ", mmu->primary_context);
  printf("  Secondary ctxt: 0x%04llx ", mmu->secondary_context);
  printf("  Nucleus ctxt:   0x0000\n");

  printf("LSU control register: 0x000000000000000f\n");
  printf("  D-MMU enable:   true    D-cache enable: true\n");
  printf("  I-MMU enable:   true    I-cache enable: true\n");

  printf("D-MMU sync fault status register:\n");
  printf("  asi: 0x%02llx  ft: 0x%02llx  e: %1llx  ct: %1llx  pr: %1llx  w: %1llx ow: %1llx fv: %1llx\n",
         FM_GET_BITS(d_regs->sfsr, 23, 16), /* ASI */
         FM_GET_BITS(d_regs->sfsr, 11, 7), /* FT  */
         FM_GET_BITS(d_regs->sfsr, 6, 6),  /* E   */
         FM_GET_BITS(d_regs->sfsr, 5, 4),  /* CT  */
         FM_GET_BITS(d_regs->sfsr, 3, 3),  /* PR  */
         FM_GET_BITS(d_regs->sfsr, 2, 2),  /* W   */
         FM_GET_BITS(d_regs->sfsr, 1, 1),  /* OW  */
         FM_GET_BITS(d_regs->sfsr, 0, 0)); /* FV  */

  printf("I-MMU sync fault status register:\n");
  printf("  asi: 0x%02llx  ft: 0x%02llx  e: %1llx  ct: %1llx  pr: %1llx  w: %1llx ow: %1llx fv: %1llx\n",
         FM_GET_BITS(i_regs->sfsr, 23, 16), /* ASI */
         FM_GET_BITS(i_regs->sfsr, 11, 7), /* FT  */
         FM_GET_BITS(i_regs->sfsr, 6, 6),  /* E   */
         FM_GET_BITS(i_regs->sfsr, 5, 4),  /* CT  */
         FM_GET_BITS(i_regs->sfsr, 3, 3),  /* PR  */
         FM_GET_BITS(i_regs->sfsr, 2, 2),  /* W   */
         FM_GET_BITS(i_regs->sfsr, 1, 1),  /* OW  */
         FM_GET_BITS(i_regs->sfsr, 0, 0)); /* FV  */

  printf("D-MMU sync fault address register:\n");
  printf("  va: 0x%016llx\n", d_regs->sfar);

  printf("D-MMU tag access register:\n");
  printf("  va: 0x%016llx  0x%04llx\n",
         d_regs->tag_access & (~0x1FFFULL),
         FM_GET_BITS(d_regs->tag_access, 12, 0));

  printf("D-MMU tag access register:\n");
  printf("  va: 0x%016llx  0x%04llx\n",
         i_regs->tag_access & (~0x1FFFULL),
         FM_GET_BITS(i_regs->tag_access, 12, 0));

  printf("D tsbp8k     0x%016llx    I tsbp8k     0x%016llx\n", d_regs->tsbp8k,         i_regs->tsbp8k        );
  printf("D tsbp64k    0x%016llx    I tsbp64k    0x%016llx\n", d_regs->tsbp64k,        i_regs->tsbp64k       );
  printf("D tsbpd      0x%016llx                          \n", d_regs->tsbpd                                 );
  printf("D tsb        0x%016llx    I tsb        0x%016llx\n", d_regs->tsb,            i_regs->tsb           );
  printf("D tag_target 0x%016llx    I tag_target 0x%016llx\n", d_regs->tsb_tag_target, i_regs->tsb_tag_target);
  printf("D tsb_px     0x%016llx    I tsb_px     0x%016llx\n", d_regs->tsb_px,         i_regs->tsb_px        );
  printf("D tsb_sx     0x%016llx                          \n", d_regs->tsb_sx                                );
  printf("D tsb_nx     0x%016llx    I tsb_nx     0x%016llx\n", d_regs->tsb_nx,         i_regs->tsb_nx        );

}

//ALEX - FIXME: Pair the Flexus mmu with the QEMU mmu
/*
#define FM_COPY_FROM_SIMICS(ours, simics) \
do { attr_value_t val = SIM_get_attribute(chmmu, simics); \
     ours = val.u.integer; } while(0);

#define FM_COPY_LIST_FROM_SIMICS(ours, simics) \
do { attr_value_t val = SIM_get_attribute(chmmu, simics); \
     int i, size = val.u.list.size; \
     for (i = 0; i < size; i++) { \
       ours[i] = val.u.list.vector[i].u.integer; \
     } SIM_free_attribute(val); } while(0);
*/
/*
 * fm_init_mmu_from_simics: initializes an MMU to Simics MMU state
 */
/*
void
fm_init_mmu_from_simics(mmu_t * mmu, conf_object_t * chmmu) {
  tlb_regs_t * d_regs = &(mmu->d_regs);
  tlb_regs_t * i_regs = &(mmu->i_regs);

  FM_COPY_FROM_SIMICS(mmu->primary_context,      "ctxt_primary");
  FM_COPY_FROM_SIMICS(mmu->secondary_context,    "ctxt_secondary");

  FM_COPY_FROM_SIMICS(d_regs->sfar,              "dsfar");
  FM_COPY_FROM_SIMICS(d_regs->sfsr,              "dsfsr");
  FM_COPY_FROM_SIMICS(d_regs->tag_access,        "dtag_access");
  FM_COPY_FROM_SIMICS(d_regs->tsb_tag_target,    "dtag_target");
  FM_COPY_FROM_SIMICS(d_regs->tsb,               "dtsb");
  FM_COPY_FROM_SIMICS(d_regs->tsb_nx,            "dtsb_nx");
  FM_COPY_FROM_SIMICS(d_regs->tsb_px,            "dtsb_px");
  FM_COPY_FROM_SIMICS(d_regs->tsb_sx,            "dtsb_sx");
  FM_COPY_FROM_SIMICS(d_regs->tsbp64k,           "dtsbp64k");
  FM_COPY_FROM_SIMICS(d_regs->tsbp8k,            "dtsbp8k");
  FM_COPY_FROM_SIMICS(d_regs->tsbpd,             "dtsbpd");

  FM_COPY_FROM_SIMICS(i_regs->sfsr,              "isfsr");
  FM_COPY_FROM_SIMICS(i_regs->tag_access,        "itag_access");
  FM_COPY_FROM_SIMICS(i_regs->tsb_tag_target,    "itag_target");
  FM_COPY_FROM_SIMICS(i_regs->tsb,               "itsb");
  FM_COPY_FROM_SIMICS(i_regs->tsb_nx,            "itsb_nx");
  FM_COPY_FROM_SIMICS(i_regs->tsb_px,            "itsb_px");
  FM_COPY_FROM_SIMICS(i_regs->tsbp64k,           "itsbp64k");
  FM_COPY_FROM_SIMICS(i_regs->tsbp8k,            "itsbp8k");
  i_regs->sfar   = (mmu_reg_t)(-1);
  i_regs->tsb_sx = (mmu_reg_t)(-1);
  i_regs->tsbpd  = (mmu_reg_t)(-1);

  FM_COPY_FROM_SIMICS(mmu->lfsr,                 "lfsr");

  FM_COPY_FROM_SIMICS(mmu->pa_watchpoint,        "pa_watchpoint");
  FM_COPY_FROM_SIMICS(mmu->va_watchpoint,        "va_watchpoint");

  FM_COPY_LIST_FROM_SIMICS(mmu->dt16_tag,        "dtlb_fa_tagread");
  FM_COPY_LIST_FROM_SIMICS(mmu->dt16_data,       "dtlb_fa_daccess");
  FM_COPY_LIST_FROM_SIMICS(mmu->dt512_tag,       "dtlb_2w_tagread");
  FM_COPY_LIST_FROM_SIMICS(mmu->dt512_data,      "dtlb_2w_daccess");

  FM_COPY_LIST_FROM_SIMICS(mmu->it16_tag,        "itlb_fa_tagread");
  FM_COPY_LIST_FROM_SIMICS(mmu->it16_data,       "itlb_fa_daccess");
  FM_COPY_LIST_FROM_SIMICS(mmu->it128_tag,       "itlb_2w_tagread");
  FM_COPY_LIST_FROM_SIMICS(mmu->it128_data,      "itlb_2w_daccess");

}
*/
#define FM_COMPARE_FIELD(who, field) do { \
  if (a->field != b->field) { \
   DBG_(Crit, (<< "Mismatch in " << who << "->" << #field)); \
  DBG_(Crit, (<< " us:        " << std::hex << a->field)); \
  DBG_(Crit, (<< " simics:    " << std::hex << b->field)); \
    mismatch = 1; } } while(0);

#define FM_COMPARE_ARRAY(a, b, len, who) do { \
  int i; \
  for (i = 0; i < len; i++) { \
    if (a[i] != b[i]) { \
   DBG_(Crit, (<< "Mismatch in " << who << "[" << i << "]")); \
   DBG_(Crit, (<< " us:        " << std::hex << a[i])); \
   DBG_(Crit, (<< " simics:    " << std::hex << b[i])); \
      mismatch = 1; } } } while(0);

/*
 * fm_compare_regs: compare d- or i-tlb registers
 */
int
fm_compare_regs(tlb_regs_t * a, tlb_regs_t * b, const char * who) {
  int mismatch = 0;

  FM_COMPARE_FIELD(who, tsb_tag_target);
  //FM_COMPARE_FIELD(who, sfsr);
  FM_COMPARE_FIELD(who, sfar);
  FM_COMPARE_FIELD(who, tsb);
  FM_COMPARE_FIELD(who, tag_access);
  FM_COMPARE_FIELD(who, tsb_px);
  FM_COMPARE_FIELD(who, tsb_sx);
  FM_COMPARE_FIELD(who, tsb_nx);
  FM_COMPARE_FIELD(who, tsbp8k);
  FM_COMPARE_FIELD(who, tsbp64k);
  //FM_COMPARE_FIELD(who, tsbpd);
  /*FM_COMPARE_FIELD(who, data_in);  not used in simics */

  return mismatch;
}

/* fm_compare_mmus: compares two MMUs and prints any differences */
int
fm_compare_mmus(mmu_t * a, mmu_t * b) {
  int mismatch = 0;

  FM_COMPARE_FIELD("mmu", primary_context);
  FM_COMPARE_FIELD("mmu", secondary_context);
  FM_COMPARE_FIELD("mmu", pa_watchpoint);
  FM_COMPARE_FIELD("mmu", va_watchpoint);

  mismatch |= fm_compare_regs(&(a->d_regs), &(b->d_regs), "mmu->d_regs");
  mismatch |= fm_compare_regs(&(a->i_regs), &(b->i_regs), "mmu->i_regs");

  FM_COMPARE_FIELD("mmu", lfsr);

  FM_COMPARE_ARRAY(a->dt16_tag,   b->dt16_tag,   16,  "mmu->dt16_tag");
  FM_COMPARE_ARRAY(a->dt16_data,  b->dt16_data,  16,  "mmu->dt16_data");
  FM_COMPARE_ARRAY(a->dt512_tag,  b->dt512_tag,  512, "mmu->dt512_tag");
  FM_COMPARE_ARRAY(a->dt512_data, b->dt512_data, 512, "mmu->dt512_data");

  FM_COMPARE_ARRAY(a->it16_tag,   b->it16_tag,   16,  "mmu->it16_tag");
  FM_COMPARE_ARRAY(a->it16_data,  b->it16_data,  16,  "mmu->it16_data");
  FM_COMPARE_ARRAY(a->it128_tag,  b->it128_tag,  128, "mmu->it128_tag");
  FM_COMPARE_ARRAY(a->it128_data, b->it128_data, 128, "mmu->it128_data");

  return mismatch;
}

}

#endif //FLEXUS_TARGET_IS(v9)

} //end Namespace Simics
} //end namespace Flexus

#endif // IS_V9
