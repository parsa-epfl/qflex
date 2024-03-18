ARM LEGACY CPU API
==================
      1 theCPU->advance
      1 theCPU->breakSimulation
      2 theCPU->getPC
      1 theCPU->getPendingInterrupt
     16 theCPU->id
      1 theCPU->readAARCH64
      1 theCPU->readDCZID_EL0
      1 theCPU->readException
      1 theCPU->readFPCR
      2 theCPU->readFPSR
      1 theCPU->readHCREL2
      1 theCPU->readPhysicalAddress
      1 theCPU->readPSTATE
      1 theCPU->readSCTLR
      1 theCPU->readSP_el
      2 theCPU->readVirtualAddress
      1 theCPU->readVRegister
      1 theCPU->readXRegister


ARM LEGACY CPU API location
===========================
      components/uArch/microArch.cpp:26
      components/MMU/PageWalk.cpp:2

OLD QEMU_... api call
=====================

      1 API::QEMU_BRANCH_TYPE_COUNT
      3 API::QEMU_break_simulation
      1 API::QEMU_callback_event_t
      1 API::QEMU_Class_Kind_Pseudo
      1 API::QEMU_Class_Kind_Session
      1 API::QEMU_Class_Kind_Vanilla
      1 API::QEMU_Clean_Cache
      1 API::QEMU_clear_exception
      1 API::QEMU_config_ready
      1 API::QEMU_cpu_execute
      1 API::QEMU_cpu_has_work
      1 API::QEMU_cpu_mem_trans
      1 API::QEMU_CPU_Mode_Supervisor
      2 API::QEMU_Data_Cache
      4 API::QEMU_DI_Data
      8 API::QEMU_DI_Instruction
      1 API::QEMU_disassemble
      1 API::QEMU_dma_mem_trans
      1 API::QEMU_dump_state
      2 API::QEMU_ethernet_frame
      1 API::QEMU_flush_all_caches
      1 API::QEMU_get_all_cpus
      5 API::QEMU_get_cpu_by_index
      6 API::QEMU_get_cpu_index
      1 API::QEMU_get_current_el
      1 API::QEMU_get_ethernet
      2 API::QEMU_get_instruction_count
      2 API::QEMU_get_num_cores
     12 API::QEMU_get_object_by_name
      1 API::QEMU_get_pending_interrupt
      1 API::QEMU_get_phys_mem
      8 API::QEMU_get_program_counter
      1 API::QEMU_getSimulationTime
     18 API::QEMU_insert_callback
      2 API::QEMU_Instruction_Cache
      1 API::QEMU_Invalidate_Cache
     10 API::QEMU_logical_to_physical
      7 API::QEMU_magic_instruction
      1 API::QEMU_mem_op_is_data
      2 API::QEMU_mem_op_is_write
      1 API::QEMU_PE_No_Exception
      1 API::QEMU_periodic_event
      1 API::QEMU_read_AARCH64
      1 API::QEMU_read_DCZID_EL0
      1 API::QEMU_read_exception
      1 API::QEMU_read_fpcr
      1 API::QEMU_read_fpsr
      1 API::QEMU_read_hcr_el2
      5 API::QEMU_read_phys_memory
      1 API::QEMU_read_pstate
     16 API::QEMU_read_register
      1 API::QEMU_read_sctlr
      1 API::QEMU_read_sp_el
      1 API::QEMU_read_tpidr
      1 API::QEMU_read_unhashed_sysreg
      1 API::QEMU_set_tick_frequency
      2 API::QEMU_trace_mem_hier
      1 API::QEMU_Trans_Cachecomponents/uArch/microArch.cpp:26
components/MMU/PageWalk.cpp:2
      2 API::QEMU_Trans_Instr_Fetch
      2 API::QEMU_Trans_Load
      2 API::QEMU_Trans_Prefetch
      2 API::QEMU_Trans_Store
      2 API::QEMU_xterm_break_string

OLD QEMU_... api location
=========================

      components/MagicBreakQEMU/breakpoint_tracker.cpp
      components/uArch/ValueTracker.hpp
      components/DecoupledFeederQEMU/QemuTracer.cpp
      components/TraceTrackerQEMU/SharingTracker.hpp
      components/MMU/MMUImpl.cpp
      core/qemu/mai_api.hpp
      core/qemu/startup.cpp
      core/qemu/configuration_api.hpp
      components/MagicBreakQEMU/breakpoint_tracker.cpp:6
      components/uArch/ValueTracker.hpp:8
      components/DecoupledFeederQEMU/QemuTracer.cpp:10
      components/TraceTrackerQEMU/SharingTracker.hpp:1
      components/MMU/MMUImpl.cpp:4
      core/qemu/startup.cpp:1
      core/qemu/mai_api.hpp:5
      core/qemu/configuration_api.hpp:3
