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
//FIXME: note commented out all uses of WhiteBox

#include <set>

// For debug only
#include <iostream>

#include <core/boost_extensions/padded_string_cast.hpp>

#include <core/types.hpp>
#include <core/debug/debug.hpp>

#include <core/qemu/configuration_api.hpp>
#include <core/qemu/api_wrappers.hpp>

#include <components/DecoupledFeederQEMU/QemuTracer.hpp>
#include <components/uFetch/uFetchTypes.hpp>

#include <core/stats.hpp>
namespace Stat = Flexus::Stat;
//was only in namespace nDecoupledFeeder

namespace nDecoupledFeeder {

using namespace Flexus::SharedTypes;

namespace Qemu = Flexus::Qemu;
namespace API = Qemu::API;

std::set< API::conf_object_t *> theTimingModels;
const uint32_t kLargeMemoryQueue = 16000;
extern "C"{
        void TraceMemHierOperate(
				void *obj
			  , API::conf_object_t * space
			  , API::memory_transaction_t * mem_trans
			  );
	    void DMAMemHierOperate(
				void *obj
		      , API::conf_object_t * space
			  , API::memory_transaction_t * mem_trans
			  );
    
}

struct TracerStats {
  Stat::StatCounter theMemOps_stat;
  Stat::StatCounter theIOOps_stat;
  Stat::StatCounter theUncacheableOps_stat;
  Stat::StatCounter theFetches_stat;
  Stat::StatCounter theLoadExOps_stat;
  Stat::StatCounter theStoreExOps_stat;
  Stat::StatCounter theRMWOps_stat;
  Stat::StatCounter theCASOps_stat;
  Stat::StatCounter theLoadOps_stat;
  Stat::StatCounter theStoreOps_stat;
  Stat::StatCounter thePrefetchOps_stat;

  int64_t theIOOps;
  int64_t theUncacheableOps;
  int64_t theFetches;
  int64_t theLoadExOps;
  int64_t theStoreExOps;
  int64_t theRMWOps;
  int64_t theCASOps;
  int64_t theLoadOps;
  int64_t theStoreOps;
  int64_t thePrefetchOps;

  TracerStats(std::string const & aName)
    : theMemOps_stat(aName + "MemOps")
    , theIOOps_stat(aName + "IOOps")
    , theUncacheableOps_stat(aName + "UncacheableOps")
    , theFetches_stat(aName + "Fetches")
    , theLoadExOps_stat(aName + "LoadExOps")
    , theStoreExOps_stat(aName + "StoreExOps")
    , theRMWOps_stat(aName + "RMWOps")
    , theCASOps_stat(aName + "CASOps")
    , theLoadOps_stat(aName + "LoadOps")
    , theStoreOps_stat(aName + "StoreOps")
    , thePrefetchOps_stat(aName + "PrefetchOps")

    , theIOOps(0)
    , theUncacheableOps(0)
    , theFetches(0)
    , theLoadExOps(0)
    , theStoreExOps(0)
    , theRMWOps(0)
    , theCASOps(0)
    , theLoadOps(0)
    , theStoreOps(0)
    , thePrefetchOps(0)
  {}

  void update() {
    theMemOps_stat += theLoadOps + theStoreOps + theCASOps + theLoadExOps + theStoreExOps +
                      theRMWOps + thePrefetchOps;
    theIOOps_stat += theIOOps;
    theUncacheableOps_stat += theUncacheableOps;
    theFetches_stat += theFetches;
    theLoadExOps_stat += theLoadExOps;
    theStoreExOps_stat += theStoreExOps;
    theRMWOps_stat += theRMWOps;
    theCASOps_stat += theCASOps;
    theLoadOps_stat += theLoadOps;
    theStoreOps_stat += theStoreOps;
    thePrefetchOps_stat += thePrefetchOps;
    theIOOps = 0;
    theUncacheableOps = 0;
    theFetches = 0;
    theLoadExOps = 0;
    theStoreExOps = 0;
    theRMWOps = 0;
    theCASOps = 0;
    theLoadOps = 0;
    theStoreOps = 0;
    thePrefetchOps = 0;
  }
};

#if FLEXUS_TARGET_IS(v9)
#define IS_PRIV(mem_trans) (mem_trans->sparc_specific.priv)
#elif FLEXUS_TARGET_IS(ARM)
#define IS_PRIV(mem_trans) (!mem_trans->arm_specific.user)
#elif FLEXUS_TARGET_IS(X86)
#define IS_PRIV(mem_trans) (mem_trans->i386_specific.mode == API::QEMU_CPU_Mode_Supervisor)
#else // !FLEXUS_TARGET_IS(v9) && !FLEXUS_TARGET(ARM) && !FLEXUS_TARGET(X86)
#error Unknown target
#endif //FLEXUS_TARGET_IS(v9) || FLEXUS_TARGET(ARM) || FLEXUS_TARGET(X86)

class QemuTracerImpl {
  API::conf_object_t * theUnderlyingObject;
  API::conf_object_t * theCPU;
  int32_t theIndex;
  API::conf_object_t * thePhysMemory;
  API::conf_object_t * thePhysIO;

  TracerStats * theUserStats;
  TracerStats * theOSStats;
  TracerStats * theBothStats;
  MemoryMessage theMemoryMessage;
    public:
  std::function< void(int, MemoryMessage &) > toL1D;
  std::function< void(int, MemoryMessage &, uint32_t) > toL1I;
  std::function< void(int, MemoryMessage &) > toNAW;

//  bool theWhiteBoxDebug;
//  int32_t  theWhiteBoxPeriod;
  bool theSendNonAllocatingStores;

public:
  QemuTracerImpl(API::conf_object_t * anUnderlyingObject)
    : theUnderlyingObject(anUnderlyingObject)
    , theMemoryMessage(MemoryMessage::LoadReq)
  {}

  API::conf_object_t * cpu() const {
    return theCPU;
  }

  // Initialize the tracer to the desired CPU
  void init(API::conf_object_t * aCPU
            , index_t anIndex
            , std::function< void(int, MemoryMessage &) > aToL1D
            , std::function< void(int, MemoryMessage &, uint32_t) > aToL1I
            , std::function< void(int, MemoryMessage &) > aToNAW
  //          , bool aWhiteBoxDebug
    //        , int32_t aWhiteBoxPeriod
            , bool aSendNonAllocatingStores
 	
           ) {
    theCPU = aCPU;
    theIndex = anIndex;
    toL1D = aToL1D;
    toL1I = aToL1I;
    toNAW = aToNAW;
 //   theWhiteBoxDebug = false;
 //   theWhiteBoxPeriod = aWhiteBoxPeriod;
    theSendNonAllocatingStores = aSendNonAllocatingStores;
    theUserStats = new TracerStats(boost::padded_string_cast < 2, '0' > (theIndex) + "-feeder-User:");
    theOSStats = new TracerStats(boost::padded_string_cast < 2, '0' > (theIndex) + "-feeder-OS:");
    theBothStats = new TracerStats(boost::padded_string_cast < 2, '0' > (theIndex) + "-feeder-");
	API::conf_object_t *thePhysMemory = API::QEMU_get_phys_mem(theCPU);
    theTimingModels.insert(thePhysMemory);

#if FLEXUS_TARGET_IS(v9)
	thePhysIO = 0;
#else
    thePhysIO = 0;
#endif
  }

  void updateStats() {
    theUserStats->update();
    theOSStats->update();
    theBothStats->update();
  }

  API::cycles_t insn_fetch(Qemu::API::memory_transaction_t * mem_trans) {
    const int32_t k_no_stall = 0;
    theMemoryMessage.address() = 
		PhysicalMemoryAddress( mem_trans->s.physical_address );
    theMemoryMessage.pc() = 
		VirtualMemoryAddress( mem_trans->s.logical_address );
    theMemoryMessage.type() = MemoryMessage::FetchReq;
    theMemoryMessage.priv() = IS_PRIV(mem_trans);
    theMemoryMessage.reqSize() = 4;

    eBranchType branchTypeTable[ API::QEMU_BRANCH_TYPE_COUNT ] = {
      kNonBranch,
      kConditional,
      kUnconditional,
      kCall,
      kReturn,
      kLastBranchType
    };

    theMemoryMessage.branchType() = branchTypeTable[ mem_trans->s.branch_type ];
    theMemoryMessage.branchAnnul() = (mem_trans->s.annul != 0);

#if FLEXUS_TARGET_IS(v9) & 0
    uint64_t reg_content;
    API::QEMU_read_register(theCPU, 46 /* kTL */, nullptr, &reg_content );
    theMemoryMessage.tl() = reg_content;
#else
    theMemoryMessage.tl() = 0;
#endif

#if FLEXUS_TARGET_IS(v9) & 0
    uint32_t opcode = 
		API::QEMU_read_phys_memory(theCPU, mem_trans->s.physical_address, 4);
#else
    uint32_t opcode = 0;
#endif
    IS_PRIV(mem_trans) ?  
		theOSStats->theFetches++ : theUserStats->theFetches++ ;
    theBothStats->theFetches++;

// TODO FIXME
    DBG_ (VVerb, ( 
				<< "sending fetch[" 
				<< theIndex 
				<< "] op: " 
				<< opcode 
				<< " message: " 
				<< theMemoryMessage) 
			);
    toL1I(theIndex, theMemoryMessage, opcode);
    return k_no_stall; //Never stalls
  }

  API::cycles_t trace_mem_hier_operate(
				       API::conf_object_t * space,
				       API::memory_transaction_t * mem_trans ) {
    int mn = API::QEMU_get_processor_number(theCPU);
    //debugTransaction(mem_trans); // ustiugov: uncomment to track every memory op
    //Flexus::SharedTypes::MemoryMessage msg(MemoryMessage::LoadReq);
    //toL1D((int32_t) 0, msg); 
    const int32_t k_no_stall = 0;
#if FLEXUS_TARGET_IS(x86) || FLEXUS_TARGET_IS(v9) || FLEXUS_TARGET_IS(ARM)
    if (mem_trans->io ) {
      //Count data accesses
      IS_PRIV(mem_trans) ?  theOSStats->theIOOps++ : theUserStats->theIOOps++ ;
      theBothStats->theIOOps++;
      return k_no_stall; //Not a memory operation
    }
#endif

#if FLEXUS_TARGET_IS(v9)
    if (! mem_trans->sparc_specific.cache_physical ) {
      //Count data accesses
      IS_PRIV(mem_trans) ?  theOSStats->theUncacheableOps++ : theUserStats->theUncacheableOps++ ;
      theBothStats->theUncacheableOps++;
      return k_no_stall; //Not a memory operation
    }
#endif
   
#if FLEXUS_TARGET_IS(v9)
    if (mem_trans->sparc_specific.address_space == 0x71 ) {
      //BLK stores to ASI 71.
      //These stores update the data caches on hit, but do not allocate on
      //miss.  The best way to model these in
      theMemoryMessage.address() = PhysicalMemoryAddress( mem_trans->s.physical_address );
      theMemoryMessage.pc() = VirtualMemoryAddress( API::QEMU_get_program_counter(theCPU) );
      theMemoryMessage.priv() = IS_PRIV(mem_trans);
      theMemoryMessage.reqSize() = mem_trans->s.size;
      theMemoryMessage.type() = MemoryMessage::NonAllocatingStoreReq;
      toNAW(theIndex, theMemoryMessage);
      // NOTE: with the current implementation the LRU position of a block is not updated upon a NAW hit. (NIKOS)
      // We may want to fix the NAW hits, but I don't expect it'll make a measurable difference.
      return k_no_stall; //Not a memory operation
    }
#endif
    if (mem_trans->s.type == API::QEMU_Trans_Instr_Fetch) {
      return insn_fetch(mem_trans);
    }

    //Minimal memory message implementation
    theMemoryMessage.address() = PhysicalMemoryAddress( mem_trans->s.physical_address );
    theMemoryMessage.pc() = VirtualMemoryAddress( mem_trans->s.pc );
    theMemoryMessage.priv() = IS_PRIV(mem_trans);

    //Set the type field of the memory operation
    if (mem_trans->s.atomic) {

#if FLEXUS_TARGET_IS(v9)
      // Need to determine opcode, as this may be an RMW or CAS
      // record the opcode
      API::physical_address_t pc = API::QEMU_logical_to_physical(theCPU, API::QEMU_DI_Instruction, mem_trans->s.pc);
      uint32_t op_code = API::QEMU_read_phys_memory(theCPU, pc, 4);

      //LDD(a)            is 11-- ---0 -001 1--- ---- ---- ---- ----
      //STD(a)            is 11-- ---0 -011 1--- ---- ---- ---- ----

      //LDSTUB(a)/SWAP(a) is 11-- ---0 -11- 1--- ---- ---- ---- ----
      //CAS(x)A           is 11-- ---1 111- 0--- ---- ---- ---- ----

      const uint32_t kLDD_mask = 0xC1780000;
      const uint32_t kLDD_pattern = 0xC0180000;

      const uint32_t kSTD_mask = 0xC1780000;
      const uint32_t kSTD_pattern = 0xC038000;

      const uint32_t kRMW_mask = 0xC1680000;
      const uint32_t kRMW_pattern = 0xC0680000;

      const uint32_t kCAS_mask = 0xC1E80000;
      const uint32_t kCAS_pattern = 0xC1E00000;

      if ((op_code & kLDD_mask) == kLDD_pattern) {
        theMemoryMessage.type() = MemoryMessage::LoadReq;
        IS_PRIV(mem_trans) ?  theOSStats->theLoadOps++ : theUserStats->theLoadOps++ ;
        theBothStats->theLoadOps++;
      } else if ((op_code & kSTD_mask) == kSTD_pattern) {
        theMemoryMessage.type() = MemoryMessage::StoreReq;
        IS_PRIV(mem_trans) ?  theOSStats->theStoreOps++ : theUserStats->theStoreOps++ ;
        theBothStats->theStoreOps++;
      } else if ((op_code & kCAS_mask) == kCAS_pattern)  {
        theMemoryMessage.type() = MemoryMessage::CmpxReq;
        IS_PRIV(mem_trans) ?  theOSStats->theCASOps++ : theUserStats->theCASOps++ ;
        theBothStats->theCASOps++;
      } else if ((op_code & kRMW_mask) == kRMW_pattern) {
        theMemoryMessage.type() = MemoryMessage::RMWReq;
        IS_PRIV(mem_trans) ?  theOSStats->theRMWOps++ : theUserStats->theRMWOps++ ;
        theBothStats->theRMWOps++;
      } else {
          //FIXME getting an error with swap, says the opcode is 7cc2, which means the first 2 bytes are all 0s
          //this might not be possible in sparc.
//        printf("LDD: %x, STD: %x, CAS: %x, RMW: %x\n", kLDD_mask, kSTD_mask, kRMW_mask, kCAS_mask);
        DBG_Assert( false, ( << "Unknown atomic operation. Opcode: " << std::hex << op_code << " pc: " << pc << std::dec ) );
      }
#elif FLEXUS_TARGET_IS(ARM)
      theMemoryMessage.type() = MemoryMessage::RMWReq;
      switch (mem_trans->s.type) {
          case API::QEMU_Trans_Load:
              IS_PRIV(mem_trans) ? theOSStats->theLoadExOps++ : theUserStats->theLoadExOps++ ;
              theBothStats->theLoadExOps++;
              break;
          case API::QEMU_Trans_Store:
              IS_PRIV(mem_trans) ?  theOSStats->theStoreExOps++ : theUserStats->theStoreExOps++ ;
              theBothStats->theStoreExOps++;
              break;
          default:
              DBG_(Crit, ( << "unhandled transaction type.  Transaction follows:"));
              debugTransaction(mem_trans);
              DBG_Assert(false);
              break;
      }
#else //Assume all atomic x86 atomic operations are RMWs.
      theMemoryMessage.type() = MemoryMessage::RMWReq;
      IS_PRIV(mem_trans) ?  theOSStats->theRMWOps++ : theUserStats->theRMWOps++ ;
      theBothStats->theRMWOps++;
#endif //v9

    } else {
      switch (mem_trans->s.type) {
        case API::QEMU_Trans_Load:
          theMemoryMessage.type() = MemoryMessage::LoadReq;
          if(IS_PRIV(mem_trans)){
              theOSStats->theLoadOps++;
          }else{
              theUserStats->theLoadOps++ ;
          }
          theBothStats->theLoadOps++;
          break;
        case API::QEMU_Trans_Store:
          theMemoryMessage.type() = MemoryMessage::StoreReq;
          IS_PRIV(mem_trans) ?  theOSStats->theStoreOps++ : theUserStats->theStoreOps++ ;
          theBothStats->theStoreOps++;
          break;
        case API::QEMU_Trans_Prefetch:
          //Prefetches are currently mapped as loads
          theMemoryMessage.type() = MemoryMessage::LoadReq;
          IS_PRIV(mem_trans) ?  theOSStats->thePrefetchOps++ : theUserStats->thePrefetchOps++ ;
          theBothStats->thePrefetchOps++;
          break;
        case API::QEMU_Trans_Instr_Fetch:
          DBG_Assert(false);
        case API::QEMU_Trans_Cache:
	  // We don't really support these operations
	  return k_no_stall;
	  /*
	  if( mem_trans->cache_op == API::QEMU_Invalidate_Cache ) {
	    theMemoryMesage.type() = MemoryMessage::Invalidate;

	    if( mem_trans->cache == API::QEMU_Instruction_Cache ) {
	      toL1I( theIndex, theMemoryMessage);
	    } else if( mem_trans->cache == API::QEMU_Data_Cache ) {
	      toL1D( theIndex, theMemoryMessage );
	    }
	    return k_no_stall;
	  } else if( mem_trans->cache_op == API::QEMU_Clean_Cache ) {
	    theMemoryMesage.type() = MemoryMessage::Invalidate;

	    if( mem_trans->cache == API::QEMU_Instruction_Cache ) {
	      toL1I( theIndex, theMemoryMessage);
	    } else if( mem_trans->cache == API::QEMU_Data_Cache ) {
	      toL1D( theIndex, theMemoryMessage );
	    }
	    return k_no_stall;
	  }
	  break;
	  */
        default:
          DBG_(Crit, ( << "unhandled transaction type.  Transaction follows:"));
          debugTransaction(mem_trans);
          DBG_Assert(false);
          break;
      }
    }
#if FLEXUS_TARGET_IS(v9)
    if (mem_trans->sparc_specific.address_space == 0x71 ) {
      //BLK stores to ASI 71.
      //These stores update the data caches on hit, but do not allocate on
      //miss.  The best way to model these in
      theMemoryMessage.reqSize() = mem_trans->s.size;
      if(theSendNonAllocatingStores) theMemoryMessage.type() = MemoryMessage::NonAllocatingStoreReq;
      else {
       DBG_Assert(theMemoryMessage.type() = MemoryMessage::StoreReq);
      }
    }
#endif
    
    toL1D(theIndex, theMemoryMessage);
    if( theIndex != 0){
      //std::cout<<"index : "<<theIndex<<std::endl;
    }
    return k_no_stall; //Never stalls
  }

  //Useful debugging stuff for tracing every instruction
  void debugTransaction(Qemu::API::memory_transaction_t * mem_trans) {
    API::logical_address_t pc_logical = API::QEMU_get_program_counter(theCPU);
    API::physical_address_t pc = API::QEMU_logical_to_physical(theCPU, API::QEMU_DI_Instruction, pc_logical);
	unsigned opcode = 0;

    DBG_(Tmp, SetNumeric( (ScaffoldIdx) theIndex)
         ( << "Mem Hier Instr: " << opcode << " logical pc: " << &std::hex << pc_logical << " pc: " << pc)
        );

    if (API::QEMU_mem_op_is_data(&mem_trans->s)) {
      if (API::QEMU_mem_op_is_write(&mem_trans->s)) {
        DBG_(Tmp, SetNumeric( (ScaffoldIdx) theIndex)
             ( << "  Write v@" << &std::hex << mem_trans->s.logical_address << " p@" << mem_trans->s.physical_address << &std::dec << " size=" << mem_trans->s.size
               << " type=" << mem_trans->s.type
               << (  mem_trans->s.atomic ? " atomic" : "" )
               << (  mem_trans->s.may_stall ? "" : " no-stall" )
               << (  mem_trans->s.inquiry ? " inquiry" : "")
               << (  mem_trans->s.speculative ? " speculative" : "")
               << (  mem_trans->s.ignore ? " ignore" : "")
               << (  IS_PRIV(mem_trans) ? " priv" : " user")
             )
            );
      } else {
        if ( mem_trans->s.type == API::QEMU_Trans_Prefetch) {
          DBG_(Tmp, SetNumeric( (ScaffoldIdx) theIndex)
               ( << "  Prefetch v@" << &std::hex << mem_trans->s.logical_address << " p@" << mem_trans->s.physical_address << &std::dec << " size=" << mem_trans->s.size
                 << " type=" << mem_trans->s.type
                 << (  mem_trans->s.atomic ? " atomic" : "" )
                 << (  mem_trans->s.may_stall ? "" : " no-stall" )
                 << (  mem_trans->s.inquiry ? " inquiry" : "")
                 << (  mem_trans->s.speculative ? " speculative" : "")
                 << (  mem_trans->s.ignore ? " ignore" : "")
                 << (  IS_PRIV(mem_trans) ? " priv" : " user")
               )
              );
        } else {
          DBG_(Tmp, SetNumeric( (ScaffoldIdx) theIndex)
               ( << "  Read v@" << &std::hex << mem_trans->s.logical_address << " p@" << mem_trans->s.physical_address << &std::dec << " size=" << mem_trans->s.size
                 << " type=" << mem_trans->s.type
                 << (  mem_trans->s.atomic ? " atomic" : "" )
                 << (  mem_trans->s.may_stall ? "" : " no-stall" )
                 << (  mem_trans->s.inquiry ? " inquiry" : "")
                 << (  mem_trans->s.speculative ? " speculative" : "")
                 << (  mem_trans->s.ignore ? " ignore" : "")
                 << (  IS_PRIV(mem_trans) ? " priv" : " user")
               )
              );
        }
      }
    }
  }

};  // class QemuTracerImpl

class QemuTracer : public Qemu::AddInObject <QemuTracerImpl> {
  typedef Qemu::AddInObject<QemuTracerImpl> base;
public:
  static const Qemu::Persistence  class_persistence = Qemu::Session;
  static std::string className() {
    return "DecoupledFeeder";
  }
  static std::string classDescription() {
    return "Flexus's decoupled feeder.";
  }

  QemuTracer() : base() { }
  QemuTracer(Qemu::API::conf_object_t * aQemuObject) : base(aQemuObject) {}
  QemuTracer(QemuTracerImpl * anImpl) : base(anImpl) {}
};

class DMATracerImpl {
  API::conf_object_t * theUnderlyingObject;

  MemoryMessage theMemoryMessage;
  std::function< void(MemoryMessage &) > toDMA;

public:
  DMATracerImpl(API::conf_object_t * anUnderlyingObjec)
    : theUnderlyingObject(anUnderlyingObjec)
    , theMemoryMessage(MemoryMessage::WriteReq)
  {}

  // Initialize the tracer to the desired CPU
  void init(std::function< void(MemoryMessage &) > aToDMA) {
    toDMA = aToDMA;
  }

  void updateStats() {
  }

  API::cycles_t dma_mem_hier_operate(
		    API::conf_object_t * space
		  , API::memory_transaction_t * mem_trans
		  ) 
  {

    const int32_t k_no_stall = 0;
    //debugTransaction(mem_trans);
    if (API::QEMU_mem_op_is_write(&mem_trans->s)) {
      DBG_( Verb, ( << "DMA To Mem: " << std::hex << mem_trans->s.physical_address  << std::dec << " Size: " << mem_trans->s.size ) );
      theMemoryMessage.type() = MemoryMessage::WriteReq;
    } else {
      DBG_( Verb, ( << "DMA From Mem: " << std::hex << mem_trans->s.physical_address  << std::dec << " Size: " << mem_trans->s.size ) );
      theMemoryMessage.type() = MemoryMessage::ReadReq;
    }

    theMemoryMessage.address() = PhysicalMemoryAddress( mem_trans->s.physical_address );
    theMemoryMessage.reqSize() = mem_trans->s.size;
    toDMA(theMemoryMessage);
    return k_no_stall; //Never stalls
  }

};  // class DMATracerImpl

class DMATracer : public Qemu::AddInObject <DMATracerImpl> {
  typedef Qemu::AddInObject<DMATracerImpl> base;
public:
  static const Qemu::Persistence  class_persistence = Qemu::Session;
  static std::string className() {
    return "DMATracer";
  }
  static std::string classDescription() {
    return "Flexus's DMA tracer.";
  }

  DMATracer() : base() { }
  DMATracer(Qemu::API::conf_object_t * aQemuObject) : base(aQemuObject) {}
  DMATracer(DMATracerImpl * anImpl) : base(anImpl) {}
};

class QemuTracerManagerImpl : public QemuTracerManager {
  int32_t theNumCPUs;
  bool theClientServer;
  QemuTracer * theTracers;
  DMATracer theDMATracer;
  std::function< void(int, MemoryMessage &) > toL1D;
  std::function< void(int, MemoryMessage &, uint32_t) > toL1I;
  std::function< void(MemoryMessage &) > toDMA;
  std::function< void(int, MemoryMessage &) > toNAW;

//  bool theWhiteBoxDebug;
//  int32_t  theWhiteBoxPeriod;
  bool theSendNonAllocatingStores;
public:
  QemuTracerManagerImpl(
		int32_t aNumCPUs
	  , std::function< void(int, MemoryMessage &) > aToL1D
	  , std::function< void(int, MemoryMessage &, uint32_t) > aToL1I
	  , std::function< void(MemoryMessage &) > aToDMA
	  , std::function< void(int, MemoryMessage &) > aToNAW
//	  , bool aWhiteBoxDebug
//	  , int32_t aWhiteBoxPeriod
	  , bool aSendNonAllocatingStores
	 )
    : theNumCPUs(aNumCPUs)
    , theClientServer(false)
    , toL1D(aToL1D)
    , toL1I(aToL1I)
    , toDMA(aToDMA)
    , toNAW(aToNAW)
  //  , theWhiteBoxDebug(aWhiteBoxDebug)
  //  , theWhiteBoxPeriod(aWhiteBoxPeriod)
    , theSendNonAllocatingStores(aSendNonAllocatingStores) {
    DBG_( Dev, ( << "Initializing QemuTracerManager."  ) );
    theNumCPUs = aNumCPUs;

    //Dump translation caches
    Qemu::API::QEMU_flush_all_caches();
    
    //Flexus::SharedTypes::MemoryMessage msg(MemoryMessage::LoadReq);
    //toL1D((int32_t) 0, msg); 
    detectClientServer();
    createTracers();
    createDMATracer();
    DBG_( Dev, ( << "Done initializing QemuTracerManager."  ) );
  }

  virtual ~QemuTracerManagerImpl() {}

  void setQemuQuantum(int64_t aSwitchTime) {
  }

  void setSystemTick(double aTickFreq) {
    for (int32_t i = 0; i < theNumCPUs; ++i) {
		API::conf_object_t *cpu = Qemu::API::QEMU_get_cpu_by_index(i);
		API::QEMU_set_tick_frequency(cpu, aTickFreq);
    }
  }

  void updateStats() {
    for (int32_t i = 0; i < theNumCPUs; ++i) {
      theTracers[i]->updateStats();
    }
  }

  void enableInstructionTracing() {
  }

private:
  void detectClientServer() {
    theClientServer = false;
  }

  void createTracers() {
    theTracers = new QemuTracer [ theNumCPUs ];

    //Create QemuTracer Factory
    Qemu::Factory<QemuTracer> tracer_factory;
    //registerTimingInterface();


    //Create QemuTracer Objects
    //FIXME I believe this had been used incorrectly as the end point of the inner for loop.
    //In the simics it was being used, but not sure why or how it works.
    for ( int32_t ii = 0; ii < theNumCPUs ; ++ii ) {
      std::string feeder_name("flexus-feeder");
      if (theNumCPUs > 1) {
        feeder_name += '-' + boost::padded_string_cast < 2, '0' > (ii);
      }
      theTracers[ii] = tracer_factory.create(feeder_name);

      API::conf_object_t * cpu = API::QEMU_get_cpu_by_index(ii);
        theTracers[ii]->init(
			    cpu
			  , ii
			  , toL1D
			  , toL1I
			  , toNAW
//			  , theWhiteBoxDebug
//			  , theWhiteBoxPeriod
			  , theSendNonAllocatingStores
			  );
    }
    //Flexus::SharedTypes::MemoryMessage msg(MemoryMessage::LoadReq);
    //theTracers[0]->toL1D((int32_t) 0, msg); 
    //Should not be here, need to also change the function
    //    registerTimingInterface(&theTracers[ii]);
    registerTimingInterface();
  }

	void createDMATracer(void) {
		bool client_server = false;
		//Create QemuTracer Factory
		Qemu::Factory<DMATracer> tracer_factory;
		API::conf_class_t * trace_class = tracer_factory.getQemuClass();

		std::string tracer_name("dma-tracer");
		theDMATracer = tracer_factory.create(tracer_name);
		DBG_( Crit, ( << "Connecting to DMA memory map" ) );
		theDMATracer->init(toDMA);

		registerDMAInterface();
	}

  void registerTimingInterface(/*QemuTracerImpl * p*/) { 
	  // XXX: Pass addr of each individual tracer object depending on number 
	  // of CPUs. Now need to revamp the callback interface to integrate 
	  // conf_object_t, so that callbacks can be called per configuration
	  // object and not in a global context (because that's not how it's 
	  // supposed to work).
	  /*API::QEMU_insert_callback(
			    Flexus::Qemu::API::QEMU_trace_mem_hier
			  , callback
			  );*/
      //TODO figure out how to get the cpuNum
      QemuTracer * p = (theTracers);
      if(p){

   // Flexus::SharedTypes::MemoryMessage msg(MemoryMessage::LoadReq);
 //   (p[0])->toL1D((int32_t) 0, msg); 
      }
    //FIXME: I am not sure how best to pass the object to the callback function. This way could possibly cause problems with multiple cpus
      for(int i = 0; i < theNumCPUs; i++ ){
        API::conf_object_t * cpu = API::QEMU_get_cpu_by_index(i);
	API::QEMU_insert_callback(
				  API::QEMU_get_processor_number(cpu),
				  API::QEMU_cpu_mem_trans,
				  reinterpret_cast<void*>(theTracers+i),
				  reinterpret_cast<void*>(&TraceMemHierOperate)
				  );
      }
  }

  void registerDMAInterface() {
	  // XXX: See above
	  /*
	  void *callback = (&this->dma_mem_hier_operate);
	  API::QEMU_insert_callback(
			    Flexus::Qemu::API::QEMU_trace_mem_hier
			  , callback
			  );*/
    //FIXME: I am not sure how best to pass the object to the callback function. This way could possibly cause problems with multiple cpus
    DMATracerImpl * p = (&theDMATracer);
	  API::QEMU_insert_callback(
			    QEMUFLEX_GENERIC_CALLBACK,
			    API::QEMU_dma_mem_trans
			    , ((void*)p)
			  , (void*)&DMAMemHierOperate
			  );
  }

};

QemuTracerManager * QemuTracerManager::construct(int32_t aNumCPUs
    , std::function< void(int, MemoryMessage &) > toL1D
    , std::function< void(int, MemoryMessage &, uint32_t) > toL1I
    , std::function< void(MemoryMessage &) > toDMA
    , std::function< void(int, MemoryMessage &) > toNAW
//    , bool aWhiteBoxDebug
//    , int32_t aWhiteBoxPeriod
    , bool aSendNonAllocatingStores
                                                    ) {
  return new QemuTracerManagerImpl(aNumCPUs, toL1D, toL1I, toDMA, toNAW,/* aWhiteBoxDebug, aWhiteBoxPeriod,*/ aSendNonAllocatingStores);
}
extern "C"{ 
    void TraceMemHierOperate(
				void *obj
			  , API::conf_object_t * space
			  , API::memory_transaction_t * mem_trans
			  )
	{
	  // Thread safe as is, no need to add muteces
          QemuTracer* tracer = reinterpret_cast<QemuTracer*>(obj);
	  (*tracer)->trace_mem_hier_operate(space, mem_trans);
	};
    void DMAMemHierOperate(
				void *obj
			  , API::conf_object_t * space
			  , API::memory_transaction_t * mem_trans
			  )
	{
	  DMATracerImpl* tracer = reinterpret_cast<DMATracerImpl*>(obj);
	  tracer->dma_mem_hier_operate(space, mem_trans);
	};
}



} //end nDecoupledFeeder
