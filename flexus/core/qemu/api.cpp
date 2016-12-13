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
namespace Flexus{
namespace Qemu{
namespace API{
#include "api.h"

CPU_READ_REGISTER_PROC cpu_read_register= nullptr;
READREG_PROC readReg= nullptr;
MMU_LOGICAL_TO_PHYSICAL_PROC mmu_logical_to_physical= nullptr;
CPU_GET_PROGRAM_COUNTER_PROC cpu_get_program_counter= nullptr;
CPU_GET_ADDRESS_SPACE_PROC cpu_get_address_space= nullptr;
CPU_PROC_NUM_PROC cpu_proc_num= nullptr;
CPU_POP_INDEXES_PROC cpu_pop_indexes= nullptr;
QEMU_GET_PHYS_MEMORY_PROC QEMU_get_phys_memory= nullptr;
QEMU_GET_ETHERNET_PROC QEMU_get_ethernet= nullptr;
QEMU_CLEAR_EXCEPTION_PROC QEMU_clear_exception= nullptr;
QEMU_READ_REGISTER_PROC QEMU_read_register= nullptr;
QEMU_READ_REGISTER_BY_TYPE_PROC QEMU_read_register_by_type= nullptr;
QEMU_READ_PHYS_MEMORY_PROC QEMU_read_phys_memory= nullptr;
QEMU_GET_PHYS_MEM_PROC QEMU_get_phys_mem= nullptr;
QEMU_GET_CPU_BY_INDEX_PROC QEMU_get_cpu_by_index= nullptr;
QEMU_GET_PROCESSOR_NUMBER_PROC QEMU_get_processor_number= nullptr;
QEMU_STEP_COUNT_PROC QEMU_step_count= nullptr;
QEMU_GET_NUM_CPUS_PROC QEMU_get_num_cpus= nullptr;

// return the number of sockets on he motherboard
QEMU_GET_NUM_SOCKETS_PROC QEMU_get_num_sockets= nullptr;

// returns the number of cores per CPU socket
QEMU_GET_NUM_CORES_PROC QEMU_get_num_cores= nullptr;

// return the number of native threads per core
QEMU_GET_NUM_THREADS_PER_CORE_PROC QEMU_get_num_threads_per_core= nullptr;

// return the id of the socket of the processor
QEMU_CPU_GET_SOCKET_ID_PROC QEMU_cpu_get_socket_id= nullptr;

// return the core id of the processor
QEMU_CPU_GET_CORE_ID_PROC QEMU_cpu_get_core_id= nullptr;

// return the hread id of the processor
QEMU_CPU_GET_THREAD_ID_PROC QEMU_cpu_get_thread_id= nullptr;

// set quantum value for all vcpus
QEMU_CPU_SET_QUANTUM QEMU_cpu_set_quantum = nullptr;

// return an array of all processors
// (numSockets * numCores * numthreads CPUs)
QEMU_GET_ALL_PROCESSORS_PROC QEMU_get_all_processors= nullptr;

// set the frequency of a given cpu.
QEMU_SET_TICK_FREQUENCY_PROC QEMU_set_tick_frequency= nullptr;

// get freq of given cpu
QEMU_GET_TICK_FREQUENCY_PROC QEMU_get_tick_frequency= nullptr;

// get the program counter of a given cpu.
QEMU_GET_PROGRAM_COUNTER_PROC QEMU_get_program_counter= nullptr;

// convert a logical address to a physical address.
QEMU_LOGICAL_TO_PHYSICAL_PROC QEMU_logical_to_physical= nullptr;

QEMU_BREAK_SIMULATION_PROC QEMU_break_simulation= nullptr;

// dummy function at the moment. should flush the translation cache.
QEMU_FLUSH_ALL_CACHES_PROC QEMU_flush_all_caches= nullptr;

// determine the memory operation type by the transaction struct.
//[???]I assume return true if it is data, false otherwise
QEMU_MEM_OP_IS_DATA_PROC QEMU_mem_op_is_data= nullptr;

//[???]I assume return true if it is write, false otherwise
QEMU_MEM_OP_IS_WRITE_PROC QEMU_mem_op_is_write= nullptr;

//[???]I assume return true if it is read, false otherwise
QEMU_MEM_OP_IS_READ_PROC QEMU_mem_op_is_read= nullptr;

QEMU_INSTRUCTION_HANDLE_INTERRUPT_PROC QEMU_instruction_handle_interrupt = nullptr;
QEMU_GET_PENDING_EXCEPTION_PROC QEMU_get_pending_exception = nullptr;
QEMU_ADVANCE_PROC QEMU_advance = nullptr;
QEMU_GET_OBJECT_PROC QEMU_get_object = nullptr;

//NOOSHIN: begin
QEMU_CPU_EXEC_PROC QEMU_cpu_exec_proc = nullptr;
//NOOSHIN: end

QEMU_IS_IN_SIMULATION_PROC QEMU_is_in_simulation = nullptr;
QEMU_TOGGLE_SIMULATION_PROC QEMU_toggle_simulation = nullptr;
QEMU_FLUSH_TB_CACHE_PROC QEMU_flush_tb_cache = nullptr;

QEMU_GET_INSTRUCTION_COUNT_PROC QEMU_get_instruction_count = nullptr;

// insert a callback specific for the given cpu or -1 for a generic callback
QEMU_INSERT_CALLBACK_PROC QEMU_insert_callback= nullptr;

// delete a callback specific for the given cpu or -1 for a generic callback
QEMU_DELETE_CALLBACK_PROC QEMU_delete_callback= nullptr;

void QFLEX_API_set_interface_hooks( const QFLEX_API_Interface_Hooks_t* hooks ) {
  cpu_read_register= hooks->cpu_read_register;
  readReg= hooks->readReg;
  mmu_logical_to_physical= hooks->mmu_logical_to_physical;
  cpu_get_program_counter= hooks->cpu_get_program_counter;
  cpu_get_address_space= hooks->cpu_get_address_space;
  cpu_proc_num= hooks->cpu_proc_num;
  cpu_pop_indexes= hooks->cpu_pop_indexes;
  QEMU_get_phys_memory= hooks->QEMU_get_phys_memory;
  QEMU_get_ethernet= hooks->QEMU_get_ethernet;
  QEMU_clear_exception= hooks->QEMU_clear_exception;
  QEMU_read_register= hooks->QEMU_read_register;
  QEMU_read_register_by_type= hooks->QEMU_read_register_by_type;
  QEMU_read_phys_memory= hooks->QEMU_read_phys_memory;
  QEMU_get_phys_mem= hooks->QEMU_get_phys_mem;
  QEMU_get_cpu_by_index= hooks->QEMU_get_cpu_by_index;
  QEMU_get_processor_number= hooks->QEMU_get_processor_number;
  QEMU_step_count= hooks->QEMU_step_count;
  QEMU_get_num_cpus= hooks->QEMU_get_num_cpus;
  QEMU_get_num_sockets= hooks->QEMU_get_num_sockets;
  QEMU_get_num_cores= hooks->QEMU_get_num_cores;
  QEMU_get_num_threads_per_core= hooks->QEMU_get_num_threads_per_core;
  QEMU_cpu_get_socket_id= hooks->QEMU_cpu_get_socket_id;
  QEMU_cpu_get_core_id= hooks->QEMU_cpu_get_core_id;
  QEMU_cpu_get_thread_id= hooks->QEMU_cpu_get_thread_id;
  QEMU_cpu_set_quantum= hooks->QEMU_cpu_set_quantum;
  QEMU_get_all_processors= hooks->QEMU_get_all_processors;
  QEMU_set_tick_frequency= hooks->QEMU_set_tick_frequency;
  QEMU_get_tick_frequency= hooks->QEMU_get_tick_frequency;
  QEMU_get_program_counter= hooks->QEMU_get_program_counter;
  QEMU_logical_to_physical= hooks->QEMU_logical_to_physical;
  QEMU_break_simulation= hooks->QEMU_break_simulation;
  QEMU_flush_all_caches= hooks->QEMU_flush_all_caches;
  QEMU_mem_op_is_data= hooks->QEMU_mem_op_is_data;
  QEMU_mem_op_is_write= hooks->QEMU_mem_op_is_write;
  QEMU_mem_op_is_read= hooks->QEMU_mem_op_is_read;
  QEMU_instruction_handle_interrupt = hooks->QEMU_instruction_handle_interrupt;
  QEMU_get_pending_exception = hooks->QEMU_get_pending_exception;
  QEMU_advance = hooks->QEMU_advance;
  QEMU_get_object = hooks->QEMU_get_object;
  QEMU_is_in_simulation = hooks->QEMU_is_in_simulation;
  QEMU_toggle_simulation = hooks->QEMU_toggle_simulation;
  QEMU_flush_tb_cache = hooks->QEMU_flush_tb_cache;
  QEMU_get_instruction_count = hooks->QEMU_get_instruction_count;
  QEMU_insert_callback= hooks->QEMU_insert_callback;
  QEMU_delete_callback= hooks->QEMU_delete_callback;
  QEMU_cpu_exec_proc = hooks->QEMU_cpu_exec_proc;///NOOSHIN
}

} // namespace API
} // namespace Qemu
} // namespace Flexus
