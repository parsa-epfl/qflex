#ifndef _SPARCMMU_DEFINES_H
#define _SPARCMMU_DEFINES_H

#ifndef TURBO_SIMICS

#if defined(TARGET_SPARC_V9)
/*
 * SPARC V9 architecture defined ASIs
 */
#define ASI_NUCLEUS                         (0x04)
#define ASI_NUCLEUS_LITTLE                  (0x0c)
#define ASI_AS_IF_USER_PRIMARY              (0x10)
#define ASI_AS_IF_USER_SECONDARY            (0x11)
#define ASI_AS_IF_USER_PRIMARY_LITTLE       (0x18)
#define ASI_AS_IF_USER_SECONDARY_LITTLE     (0x19)
#define ASI_PRIMARY                         (0x80)
#define ASI_SECONDARY                       (0x81)
#define ASI_PRIMARY_NOFAULT                 (0x82)
#define ASI_SECONDARY_NOFAULT               (0x83)
#define ASI_PRIMARY_LITTLE                  (0x88)
#define ASI_SECONDARY_LITTLE                (0x89)
#define ASI_PRIMARY_NOFAULT_LITTLE          (0x8a)
#define ASI_SECONDARY_NOFAULT_LITTLE        (0x8b)
#endif /* TARGET_SPARC_V9 */

#if defined(TARGET_ULTRA)
/*
 * Ultra ASIs
 */
#define ASI_PHYS_USE_EC                     (0x14)
#define ASI_PHYS_BYPASS_EC_WITH_EBIT        (0x15)
#define ASI_PHYS_USE_EC_LITTLE              (0x1c)
#define ASI_PHYS_BYPASS_EC_WITH_EBIT_LITTLE (0x1d)

/* Atomic quad load ASIs. Can only be used with ldda */
#define ASI_NUCLEUS_QUAD_LDD                (0x24)
#define ASI_NUCLEUS_QUAD_LDD_L              (0x2c)

#if defined(TARGET_NIAGARA)
#define ASI_SCRATCHPAD                       0x20
#define ASI_MMU_CONTEXTID                    0x21

#define ASI_QUEUE                            0x25

#define ASI_QUAD_LDD_REAL                    0x26
#define ASI_QUAD_LDD_REAL_L                  0x2e

#define ASI_DMMU_CTXT_ZERO_TSB_BASE_PS0      0x31
#define ASI_DMMU_CTXT_ZERO_TSB_BASE_PS1      0x32
#define ASI_DMMU_CTXT_ZERO_CONFIG            0x33
#define ASI_IMMU_CTXT_ZERO_TSB_BASE_PS0      0x35
#define ASI_IMMU_CTXT_ZERO_TSB_BASE_PS1      0x36
#define ASI_IMMU_CTXT_ZERO_CONFIG            0x37
#define ASI_DMMU_CTXT_NONZERO_TSB_BASE_PS0   0x39
#define ASI_DMMU_CTXT_NONZERO_TSB_BASE_PS1   0x3a
#define ASI_DMMU_CTXT_NONZERO_CONFIG         0x3b
#define ASI_IMMU_CTXT_NONZERO_TSB_BASE_PS0   0x3d
#define ASI_IMMU_CTXT_NONZERO_TSB_BASE_PS1   0x3e
#define ASI_IMMU_CTXT_NONZERO_CONFIG         0x3f
#define ASI_STREAM_MA                        0x40
#define ASI_SPARC_BIST                       0x42
#define ASI_ERROR_INJECT_REG                 0x43
#define ASI_STM_CTL_REG                      0x44

#define ASI_HV_SCRATCHPAD                    0x4f
#define ASI_TLB_INVALIDATE_ALL               0x60

#define ASI_AIUP_BLK_INIT_QUAD_LDD           0x22
#define ASI_AIUS_BLK_INIT_QUAD_LDD           0x23
#define ASI_NUCL_BLK_INIT_QUAD_LDD           0x27
#define ASI_AIUP_BLK_INIT_QUAD_LDD_L         0x2a
#define ASI_AIUS_BLK_INIT_QUAD_LDD_L         0x2b
#define ASI_NUCL_BLK_INIT_QUAD_LDD_L         0x2f
#define ASI_PRIM_BLK_INIT_QUAD_LDD           0xe2
#define ASI_SCND_BLK_INIT_QUAD_LDD           0xe3
#define ASI_PRIM_BLK_INIT_QUAD_LDD_L         0xea
#define ASI_SCND_BLK_INIT_QUAD_LDD_L         0xeb
#endif   /* TARGET_NIAGARA */

#if defined(ULTRA_HAS_LDD_PHYS)
#define ASI_QUAD_LDD_PHYS                   (0x34)
#define ASI_QUAD_LDD_PHYS_L                 (0x3c)
#endif /* ULTRA_HAS_LDD_PHYS */

#if defined(TARGET_ULTRAIII) || defined(TARGET_ULTRAIV)
#define ASI_PCACHE_DATA_STATUS              (0x30) /* NI in Simics */
#define ASI_PCACHE_DATA                     (0x31) /* NI in Simics */
#define ASI_PCACHE_TAG                      (0x32) /* NI in Simics */
#define ASI_PCACHE_SNOOP_TAG                (0x33) /* NI in Simics */

#define ASI_SRAM_FAST_INIT                  (0x40)

#define ASI_WCACHE_VALID_BITS               (0x38) /* NI in Simics */
#define ASI_WCACHE_DATA                     (0x39) /* NI in Simics */
#define ASI_WCACHE_TAG                      (0x3a) /* NI in Simics */
#define ASI_WCACHE_SNOOP_TAG                (0x3b) /* NI in Simics */

#define ASI_DCACHE_INVALIDATE               (0x42) /* NI in Simics */
#define ASI_DCACHE_UTAG                     (0x43) /* NI in Simics */
#define ASI_DCACHE_SNOOP_TAG                (0x44) /* NI in Simics */
#endif /* TARGET_ULTRAIII || TARGET_ULTRAIV */

#if defined(TARGET_NIAGARA)
#define ASI_INSTRUCTION_BREAKPOINT           0x42
#define ASI_LSU_CONTROL_REG                  0x45
#endif   /* TARGET_NIAGARA */

#if defined(TARGET_ULTRAI) || defined(TARGET_ULTRAII)
#define ASI_LSU_CONTROL_REG                 (0x45)
#endif
#if defined(TARGET_ULTRAIII) || defined(TARGET_ULTRAIV)
#define ASI_DCU_CONTROL_REG                 (0x45)
#endif

#define ASI_DCACHE_DATA                     (0x46)
#define ASI_DCACHE_TAG                      (0x47)

#define ASI_INTR_DISPATCH_STATUS 	    (0x48)

#if defined(ULTRA_HAS_UPA_CONFIG)
#define ASI_UPA_CONFIG			    (0x4a)
#endif

#if defined(ULTRA_HAS_SAFARI_CONFIG)
#define ASI_SAFARI_CONFIG		    (0x4a)
#endif

#if defined(ULTRA_HAS_EEE)
#define ASI_ESTATE_ERROR_EN_REG             (0x4b)
#endif

#if defined(ULTRA_HAS_AFAR_AFSR)
#define ASI_ASYNC_FAULT_STATUS              (0x4c)
#define ASI_ASYNC_FAULT_ADDRESS             (0x4d)
#endif

#if defined(TARGET_ULTRAI) || defined(TARGET_ULTRAII) || defined(TARGET_ULTRAIII) || defined(TARGET_ULTRAIV)
#define ASI_ECACHE_TAG                      (0x4e)
#endif

#define ASI_IMMU                            (0x50)
#define ASI_IMMU_TSB_8KB_PTR_REG            (0x51)
#define ASI_IMMU_TSB_64KB_PTR_REG           (0x52)

#if defined(ULTRA_HAS_SERIAL)
#define ASI_SERIAL_ID                       (0x53)
#endif

#define ASI_ITLB_DATA_IN_REG                (0x54)
#define ASI_ITLB_DATA_ACCESS_REG            (0x55)
#define ASI_ITLB_TAG_READ_REG               (0x56)
#define ASI_IMMU_DEMAP                      (0x57)

#define ASI_DMMU                            (0x58)
#define ASI_DMMU_TSB_8KB_PTR_REG            (0x59)
#define ASI_DMMU_TSB_64KB_PTR_REG           (0x5a)
#define ASI_DMMU_TSB_DIRECT_PTR_REG         (0x5b)
#define ASI_DTLB_DATA_IN_REG                (0x5c)
#define ASI_DTLB_DATA_ACCESS_REG            (0x5d)
#define ASI_DTLB_TAG_READ_REG               (0x5e)
#define ASI_DMMU_DEMAP                      (0x5f)

#define ASI_ICACHE_INSTR                    (0x66)
#define ASI_ICACHE_TAG                      (0x67)

#if defined(TARGET_ULTRAIII) || defined(TARGET_ULTRAIV)
#define ASI_IMMU_SNOOP_TAG                  (0x68) /* NI in Simics */
#endif

#if defined(TARGET_ULTRAI) || defined(TARGET_ULTRAII)
#define ASI_ICACHE_PRE_DECODE               (0x6e)
#define ASI_ICACHE_NEXT_FIELD               (0x6f) /* NI in Simics */
#endif
#if defined(TARGET_ULTRAIII) || defined(TARGET_ULTRAIV)
#define ASI_BRANCH_PREDICTION_ARRAY         (0x6f) /* NI in Simics */
#endif

#if defined(TARGET_NIAGARA)
#define ASI_BLK_AIUP                         0x16
#define ASI_BLK_AIUS                         0x17
#else/* !TARGET_NIAGARA */
#define ASI_BLK_AIUP                         0x70
#define ASI_BLK_AIUS                         0x71
#endif /* !TARGET_NIAGARA */

#if defined(TARGET_ULTRAIII) || defined(TARGET_ULTRAIV)
#define ASI_MCU_CTRL_REG                    (0x72) /* NI in Simics */
#if defined(TARGET_ULTRAIV)
#define ASI_ECACHE_CFG_TIMING_CTRL           0x73
#endif   /* TARGET_ULTRAIV */
#define ASI_ECACHE_DATA                     (0x74) /* NI in Simics */
#define ASI_ECACHE_CTRL                     (0x75) /* NI in Simics */
#endif /* TARGET_ULTRAIII || TARGET_ULTRAIV */

#if defined(TARGET_NIAGARA)
#define ASI_INTR_RECEIVE                     0x72
#define ASI_UDB_INTR_W                       0x73
#define ASI_UDB_INTR_R                       0x74
#else
#define ASI_INTR_RECEIVE		     0x49
#define ASI_UDB_INTR_W 			     0x77
#define ASI_UDB_INTR_R 			     0x7f
#endif

#define ASI_ECACHE_W                        (0x76)


#if defined(TARGET_NIAGARA)
#define ASI_BLK_AIUPL                        0x1e
#define ASI_BLK_AIUSL                        0x1f
#else/* !TARGET_NIAGARA */
#define ASI_BLK_AIUPL                        0x78
#define ASI_BLK_AIUSL                        0x79
#endif /* !TARGET_NIAGARA */

#define ASI_ECACHE_R                        (0x7e)

#define ASI_PST8_P                          (0xc0)
#define ASI_PST8_S                          (0xc1)
#define ASI_PST16_P                         (0xc2)
#define ASI_PST16_S                         (0xc3)
#define ASI_PST32_P                         (0xc4)
#define ASI_PST32_S                         (0xc5)
#define ASI_PST8_PL                         (0xc8)
#define ASI_PST8_SL                         (0xc9)
#define ASI_PST16_PL                        (0xca)
#define ASI_PST16_SL                        (0xcb)
#define ASI_PST32_PL                        (0xcc)
#define ASI_PST32_SL                        (0xcd)

#define ASI_FL8_P                           (0xd0)
#define ASI_FL8_S                           (0xd1)
#define ASI_FL16_P                          (0xd2)
#define ASI_FL16_S                          (0xd3)
#define ASI_FL8_PL                          (0xd8)
#define ASI_FL8_SL                          (0xd9)
#define ASI_FL16_PL                         (0xda)
#define ASI_FL16_SL                         (0xdb)

#define ASI_BLK_COMMIT_P                    (0xe0)
#define ASI_BLK_COMMIT_S                    (0xe1)

#define ASI_BLK_P                           (0xf0)
#define ASI_BLK_S                           (0xf1)
#define ASI_BLK_PL                          (0xf8)
#define ASI_BLK_SL                          (0xf9)

#define IMPLICIT_D_ASI_CURRENT()  (IMPLICIT_D_ASI(current_processor))
#define IMPLICIT_I_ASI_CURRENT()  (IMPLICIT_I_ASI(current_processor))

#define IMPLICIT_D_ASI(cpu)     (REG_TL_R(cpu) ? (REG_PSTATE_CLE_R(cpu) ? ASI_NUCLEUS_LITTLE : ASI_NUCLEUS) \
                                               : (REG_PSTATE_CLE_R(cpu) ? ASI_PRIMARY_LITTLE : ASI_PRIMARY))

#define IMPLICIT_I_ASI(cpu)     (REG_TL_R(cpu) ? ASI_NUCLEUS : ASI_PRIMARY)

#endif /* TARGET_ULTRA */

/* macro defining table of ASI properties */
#define ASI_INFO_DEFINITION(table)                                              \
do {                                                                            \
	int i;                                                                  \
	table[ASI_NUCLEUS] =                                                    \
                Sim_ASI_Translating | Sim_ASI_Nucleus | Sim_ASI_Prefetch        \
                | Sim_ASI_Atomic;                                               \
	table[ASI_NUCLEUS_LITTLE] =                                             \
		Sim_ASI_Translating | Sim_ASI_Nucleus                           \
		| Sim_ASI_Little_Endian | Sim_ASI_Prefetch | Sim_ASI_Atomic;    \
	table[ASI_AS_IF_USER_PRIMARY] =                                         \
		Sim_ASI_Translating | Sim_ASI_As_If_User | Sim_ASI_Primary      \
                | Sim_ASI_Prefetch | Sim_ASI_Atomic;                            \
	table[ASI_AS_IF_USER_SECONDARY] =                                       \
		Sim_ASI_Translating | Sim_ASI_As_If_User | Sim_ASI_Secondary    \
                | Sim_ASI_Prefetch | Sim_ASI_Atomic;                            \
	table[ASI_AS_IF_USER_PRIMARY_LITTLE] =                                  \
		Sim_ASI_Translating | Sim_ASI_As_If_User | Sim_ASI_Primary      \
		| Sim_ASI_Prefetch | Sim_ASI_Little_Endian | Sim_ASI_Atomic;    \
	table[ASI_AS_IF_USER_SECONDARY_LITTLE] =                                \
		Sim_ASI_Translating | Sim_ASI_As_If_User | Sim_ASI_Secondary    \
		| Sim_ASI_Prefetch | Sim_ASI_Little_Endian | Sim_ASI_Atomic;    \
	table[ASI_PRIMARY] =                                                    \
                Sim_ASI_Translating | Sim_ASI_Primary | Sim_ASI_Prefetch        \
                | Sim_ASI_Atomic;                                               \
	table[ASI_SECONDARY] =                                                  \
                Sim_ASI_Translating | Sim_ASI_Secondary | Sim_ASI_Prefetch      \
                | Sim_ASI_Atomic;                                               \
	table[ASI_PRIMARY_NOFAULT] =                                            \
		Sim_ASI_Translating | Sim_ASI_Primary | Sim_ASI_Prefetch        \
                | Sim_ASI_Non_Fault;                                            \
	table[ASI_SECONDARY_NOFAULT] =                                          \
		Sim_ASI_Translating | Sim_ASI_Secondary | Sim_ASI_Prefetch      \
                | Sim_ASI_Non_Fault;                                            \
	table[ASI_PRIMARY_LITTLE] =                                             \
		Sim_ASI_Translating | Sim_ASI_Primary | Sim_ASI_Prefetch        \
		| Sim_ASI_Little_Endian | Sim_ASI_Atomic;                       \
	table[ASI_SECONDARY_LITTLE] =                                           \
		Sim_ASI_Translating | Sim_ASI_Secondary	| Sim_ASI_Prefetch      \
		| Sim_ASI_Little_Endian | Sim_ASI_Atomic;                       \
	table[ASI_PRIMARY_NOFAULT_LITTLE] =                                     \
		Sim_ASI_Translating | Sim_ASI_Primary | Sim_ASI_Prefetch        \
                | Sim_ASI_Non_Fault | Sim_ASI_Little_Endian;                    \
	table[ASI_SECONDARY_NOFAULT_LITTLE] =                                   \
		Sim_ASI_Translating | Sim_ASI_Secondary | Sim_ASI_Prefetch      \
                | Sim_ASI_Non_Fault | Sim_ASI_Little_Endian;                    \
	ASI_INFO_DEFINITION_ULTRA(table);                                       \
	for(i = 0; i < 0x80; i++)                                               \
		table[i] |= Sim_ASI_Restricted;                                 \
} while(0)

#if defined(TARGET_ULTRA)
#define ASI_INFO_DEFINITION_ULTRA(table)                                                        \
do {                                                                                            \
	table[ASI_PHYS_USE_EC] = Sim_ASI_Bypass | Sim_ASI_Atomic;                               \
	table[ASI_PHYS_BYPASS_EC_WITH_EBIT] =                                                   \
		Sim_ASI_Bypass | Sim_ASI_Side_Effects                                           \
		| Sim_ASI_Non_Cachable;                                                         \
	table[ASI_PHYS_USE_EC_LITTLE] =                                                         \
		Sim_ASI_Bypass | Sim_ASI_Little_Endian | Sim_ASI_Atomic;                        \
	table[ASI_PHYS_BYPASS_EC_WITH_EBIT_LITTLE] =                                            \
		Sim_ASI_Bypass | Sim_ASI_Side_Effects | Sim_ASI_Non_Cachable                    \
		| Sim_ASI_Little_Endian;                                                        \
	table[ASI_NUCLEUS_QUAD_LDD] =                                                           \
                Sim_ASI_Translating | Sim_ASI_Nucleus | Sim_ASI_Quad_Ldd;                       \
	table[ASI_NUCLEUS_QUAD_LDD_L] =                                                         \
		Sim_ASI_Translating | Sim_ASI_Nucleus                                           \
                | Sim_ASI_Quad_Ldd  | Sim_ASI_Little_Endian;                                    \
	table[ASI_BLK_AIUP] =                                                                   \
		Sim_ASI_Translating | Sim_ASI_Block | Sim_ASI_As_If_User                        \
		| Sim_ASI_Primary;                                                              \
	table[ASI_BLK_AIUS] =                                                                   \
		Sim_ASI_Translating | Sim_ASI_Block | Sim_ASI_As_If_User                        \
		| Sim_ASI_Secondary;                                                            \
	table[ASI_BLK_AIUPL] =                                                                  \
		Sim_ASI_Translating | Sim_ASI_Block | Sim_ASI_As_If_User                        \
		| Sim_ASI_Primary | Sim_ASI_Little_Endian;                                      \
	table[ASI_BLK_AIUSL] =                                                                  \
		Sim_ASI_Translating | Sim_ASI_Block | Sim_ASI_As_If_User                        \
		| Sim_ASI_Secondary | Sim_ASI_Little_Endian;                                    \
        ASI_INFO_DEFINITION_ULTRA_HAS_PARTIAL_STORE(table);                                     \
        ASI_INFO_DEFINITION_ULTRA_HAS_SHORT_FP(table);                                          \
        ASI_INFO_DEFINITION_ULTRA_HAS_BLOCK_COMMIT(table);                                      \
	table[ASI_BLK_P] =                                                                      \
		Sim_ASI_Translating | Sim_ASI_Block | Sim_ASI_Primary;                          \
	table[ASI_BLK_S] =                                                                      \
		Sim_ASI_Translating | Sim_ASI_Block | Sim_ASI_Secondary;                        \
	table[ASI_BLK_PL] =                                                                     \
		Sim_ASI_Translating | Sim_ASI_Block | Sim_ASI_Primary                           \
		| Sim_ASI_Little_Endian;                                                        \
	table[ASI_BLK_SL] =                                                                     \
		Sim_ASI_Translating | Sim_ASI_Block | Sim_ASI_Secondary                         \
		| Sim_ASI_Little_Endian;                                                        \
	ASI_INFO_DEFINITION_ULTRA_HAS_LDD_PHYS(table);                                          \
} while(0)
#else  /* not TARGET_ULTRA */
#define ASI_INFO_DEFINITION_ULTRA(table)
#endif /* not TARGET_ULTRA */

#if defined(ULTRA_HAS_PARTIAL_STORE)
#define ASI_INFO_DEFINITION_ULTRA_HAS_PARTIAL_STORE(table)                                      \
do {                                                                                            \
	table[ASI_PST8_P] = Sim_ASI_Translating | Sim_ASI_Primary;                              \
	table[ASI_PST8_S] = Sim_ASI_Translating | Sim_ASI_Secondary;                            \
	table[ASI_PST16_P] = Sim_ASI_Translating | Sim_ASI_Primary;                             \
	table[ASI_PST16_S] = Sim_ASI_Translating | Sim_ASI_Secondary;                           \
	table[ASI_PST32_P] = Sim_ASI_Translating | Sim_ASI_Primary;                             \
	table[ASI_PST32_S] = Sim_ASI_Translating | Sim_ASI_Secondary;                           \
	table[ASI_PST8_PL] =                                                                    \
		Sim_ASI_Translating | Sim_ASI_Primary                                           \
		| Sim_ASI_Little_Endian;                                                        \
	table[ASI_PST8_SL] =                                                                    \
		Sim_ASI_Translating | Sim_ASI_Secondary                                         \
		| Sim_ASI_Little_Endian;                                                        \
	table[ASI_PST16_PL] =                                                                   \
		Sim_ASI_Translating | Sim_ASI_Primary                                           \
		| Sim_ASI_Little_Endian;                                                        \
	table[ASI_PST16_SL] =                                                                   \
		Sim_ASI_Translating | Sim_ASI_Secondary                                         \
		| Sim_ASI_Little_Endian;                                                        \
	table[ASI_PST32_PL] =                                                                   \
		Sim_ASI_Translating | Sim_ASI_Primary                                           \
		| Sim_ASI_Little_Endian;                                                        \
	table[ASI_PST32_SL] =                                                                   \
		Sim_ASI_Translating | Sim_ASI_Secondary                                         \
		| Sim_ASI_Little_Endian;                                                        \
} while (0)
#else    /* ULTRA_HAS_PARTIAL_STORE */
#define ASI_INFO_DEFINITION_ULTRA_HAS_PARTIAL_STORE(table)
#endif   /* ULTRA_HAS_PARTIAL_STORE */

#if defined(ULTRA_HAS_SHORT_FP)
#define ASI_INFO_DEFINITION_ULTRA_HAS_SHORT_FP(table)                                           \
do {                                                                                            \
	table[ASI_FL8_P] = Sim_ASI_Translating | Sim_ASI_Primary | Sim_ASI_Short_FP;            \
	table[ASI_FL8_S] = Sim_ASI_Translating | Sim_ASI_Secondary | Sim_ASI_Short_FP;          \
	table[ASI_FL16_P] = Sim_ASI_Translating | Sim_ASI_Primary  | Sim_ASI_Short_FP;          \
	table[ASI_FL16_S] = Sim_ASI_Translating | Sim_ASI_Secondary  | Sim_ASI_Short_FP;        \
	table[ASI_FL8_PL] =                                                                     \
		Sim_ASI_Translating | Sim_ASI_Primary                                           \
		| Sim_ASI_Little_Endian | Sim_ASI_Short_FP;                                     \
	table[ASI_FL8_SL] =                                                                     \
		Sim_ASI_Translating | Sim_ASI_Secondary                                         \
		| Sim_ASI_Little_Endian | Sim_ASI_Short_FP;                                     \
	table[ASI_FL16_PL] =                                                                    \
		Sim_ASI_Translating | Sim_ASI_Primary                                           \
		| Sim_ASI_Little_Endian | Sim_ASI_Short_FP;                                     \
	table[ASI_FL16_SL] =                                                                    \
		Sim_ASI_Translating | Sim_ASI_Secondary                                         \
		| Sim_ASI_Little_Endian | Sim_ASI_Short_FP;                                     \
} while(0)
#else    /* ULTRA_HAS_SHORT_FP */
#define ASI_INFO_DEFINITION_ULTRA_HAS_SHORT_FP(table)
#endif   /* ULTRA_HAS_SHORT_FP */

#if defined(ULTRA_HAS_BLOCK_COMMIT)
#define ASI_INFO_DEFINITION_ULTRA_HAS_BLOCK_COMMIT(table)                                       \
do {                                                                                            \
	table[ASI_BLK_COMMIT_P] =                                                               \
		Sim_ASI_Translating | Sim_ASI_Block | Sim_ASI_Primary | Sim_ASI_Commit;         \
	table[ASI_BLK_COMMIT_S] =                                                               \
		Sim_ASI_Translating | Sim_ASI_Block | Sim_ASI_Secondary | Sim_ASI_Commit;       \
} while(0)
#else    /* ULTRA_HAS_BLOCK_COMMIT */
#define ASI_INFO_DEFINITION_ULTRA_HAS_BLOCK_COMMIT(table)
#endif   /* ULTRA_HAS_BLOCK_COMMIT */

#if defined(ULTRA_HAS_LDD_PHYS)
#define ASI_INFO_DEFINITION_ULTRA_HAS_LDD_PHYS(table)			                        \
do {									                        \
	table[ASI_QUAD_LDD_PHYS] = Sim_ASI_Bypass | Sim_ASI_Quad_Ldd;	                        \
	table[ASI_QUAD_LDD_PHYS_L] = Sim_ASI_Bypass | Sim_ASI_Quad_Ldd | Sim_ASI_Little_Endian; \
} while(0)
#else  /* not ULTRA_HAS_LDD_PHYS */
#define ASI_INFO_DEFINITION_ULTRA_HAS_LDD_PHYS(table)
#endif /* not ULTRA_HAS_LDD_PHYS */


#if defined(TARGET_NIAGARA)
#define ASI_INFO_DEFINITION_NIAGARA(table)                                      \
do {                                                                            \
        table[ASI_QUAD_LDD_REAL]    = Sim_ASI_Bypass | Sim_ASI_Quad_Ldd;        \
        table[ASI_QUAD_LDD_REAL_L]  =                                           \
                Sim_ASI_Bypass | Sim_ASI_Quad_Ldd                               \
                | Sim_ASI_Little_Endian;                                        \
        table[ASI_AIUP_BLK_INIT_QUAD_LDD]  =                                    \
                Sim_ASI_Translating | Sim_ASI_As_If_User | Sim_ASI_Primary      \
                | Sim_ASI_Quad_Ldd;                                             \
        table[ASI_AIUS_BLK_INIT_QUAD_LDD]   =                                   \
                Sim_ASI_Translating | Sim_ASI_As_If_User | Sim_ASI_Secondary    \
                | Sim_ASI_Quad_Ldd;                                             \
        table[ASI_NUCL_BLK_INIT_QUAD_LDD]   =                                   \
                Sim_ASI_Translating | Sim_ASI_Nucleus | Sim_ASI_Quad_Ldd;       \
        table[ASI_AIUP_BLK_INIT_QUAD_LDD_L] =                                   \
                Sim_ASI_Translating | Sim_ASI_As_If_User | Sim_ASI_Primary      \
                | Sim_ASI_Quad_Ldd | Sim_ASI_Little_Endian;                     \
        table[ASI_AIUS_BLK_INIT_QUAD_LDD_L] =                                   \
                Sim_ASI_Translating | Sim_ASI_As_If_User | Sim_ASI_Secondary    \
                | Sim_ASI_Quad_Ldd | Sim_ASI_Little_Endian;                     \
        table[ASI_NUCL_BLK_INIT_QUAD_LDD_L] =                                   \
		Sim_ASI_Translating | Sim_ASI_Nucleus                           \
                | Sim_ASI_Quad_Ldd  | Sim_ASI_Little_Endian;                    \
        table[ASI_PRIM_BLK_INIT_QUAD_LDD]   =                                   \
                Sim_ASI_Translating | Sim_ASI_Primary | Sim_ASI_Quad_Ldd;       \
        table[ASI_SCND_BLK_INIT_QUAD_LDD]   =                                   \
                Sim_ASI_Translating | Sim_ASI_Secondary | Sim_ASI_Quad_Ldd;     \
        table[ASI_PRIM_BLK_INIT_QUAD_LDD_L] =                                   \
                Sim_ASI_Translating | Sim_ASI_Primary | Sim_ASI_Quad_Ldd        \
                | Sim_ASI_Little_Endian;                                        \
        table[ASI_SCND_BLK_INIT_QUAD_LDD_L] =                                   \
                Sim_ASI_Translating | Sim_ASI_Secondary | Sim_ASI_Quad_Ldd      \
                | Sim_ASI_Little_Endian;                                        \
} while (0)
#endif /* TARGET_NIAGARA */
#endif /* TURBO_SIMICS */

typedef enum asi_info {
        Sim_ASI_Bypass        = 1 <<  0,
        Sim_ASI_Restricted    = 1 <<  1,
        Sim_ASI_Translating   = 1 <<  2,
        Sim_ASI_Little_Endian = 1 <<  3,
        Sim_ASI_Nucleus       = 1 <<  4,
        Sim_ASI_Primary       = 1 <<  5,
        Sim_ASI_Secondary     = 1 <<  6,
        Sim_ASI_Non_Fault     = 1 <<  7,
        Sim_ASI_As_If_User    = 1 <<  8,
        Sim_ASI_Non_Cachable  = 1 <<  9,
        Sim_ASI_Block         = 1 << 10,
        Sim_ASI_Side_Effects  = 1 << 11,
        Sim_ASI_Commit        = 1 << 12,
        Sim_ASI_HRestricted   = 1 << 13,
        Sim_ASI_Quad_Ldd      = 1 << 14,
        Sim_ASI_Prefetch      = 1 << 15,
        Sim_ASI_Atomic        = 1 << 16,
        Sim_ASI_Short_FP      = 1 << 17,
        Sim_ASI_Sync1         = 1 << 18,
        Sim_ASI_Sync2         = 1 << 19
} asi_info_t;

#endif
