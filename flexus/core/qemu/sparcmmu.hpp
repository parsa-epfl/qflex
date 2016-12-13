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
#ifndef FLEXUS_SIMICS_SPARC_MMU_HPP_INCLUDED
#define FLEXUS_SIMICS_SPARC_MMU_HPP_INCLUDED

#include "sparcmmu_defines.h"

namespace Flexus {
namespace Qemu {

#if FLEXUS_TARGET_IS(v9)
namespace MMU {

typedef unsigned long long address_t;
typedef unsigned long long mmu_reg_t;
typedef unsigned char asi_t;

typedef struct tlb_regs {        /* D-ASI  I-ASI  VA       */
  /* ---------------------- */
  mmu_reg_t tsb_tag_target;        /* 0x58          0x0      */
  /* primary_context    */  /* 0x58   ---    0x8      */
  /* secondary_context  */  /* 0x58   ---    0x10     */
  /* nucleus_context    */  /*     wired to 0         */
  mmu_reg_t sfsr;                  /* 0x58          0x18     */
  mmu_reg_t sfar;    /* D only */  /* 0x58   ---    0x20     */
  mmu_reg_t tsb;                   /* 0x58          0x28     */
  mmu_reg_t tag_access;            /* 0x58          0x30     */
  /* va_watchpoint      */  /* 0x58   ---    0x38     */
  /* pa_watchpoint      */  /* 0x58   ---    0x40     */
  mmu_reg_t tsb_px;                /* 0x58          0x48     */
  mmu_reg_t tsb_sx;  /* D only */  /* 0x58   ---    0x50     */
  mmu_reg_t tsb_nx;                /* 0x58          0x58     */
  mmu_reg_t tsbp8k;                /* 0x59          0x0      */
  mmu_reg_t tsbp64k;               /* 0x5A          0x0      */
  mmu_reg_t tsbpd;   /* D only */  /* 0x5B   ---    0x0      */
  mmu_reg_t data_in;               /* 0x5C          0x0      */
  /* data_access        */  /* 0x5D          0x0+     */
  /* cam_diagnostic     */  /* 0x5D          0x40000+ */
  /* tag_read           */  /* 0x5E          0x0+     */
  /* Demap operations   */  /* 0x5F                   */

} tlb_regs_t;

/* TTE tag format:
 *
 * [ G  |  -  | Context | --- | VA_tag ]
 *   63  62 61 60     48 47 42 41     0
 */
typedef unsigned long long tte_tag;

/* TTE data format:
 *
 * [ V  | Siz | NFO | IE  | Soft2 | --- | PA<42:13> | Soft | L, CP, CV, E, P, W, G ]
 *   63  62 61  60    59   58   50 49 43 42       13 12   7  6  5   4   3  2  1  0
 */
typedef unsigned long long tte_data;

typedef struct mmu {

  /* general MMU registers */
  mmu_reg_t primary_context;
  mmu_reg_t secondary_context;
  mmu_reg_t pa_watchpoint;
  mmu_reg_t va_watchpoint;

  /* D- and I-TLB registers */
  tlb_regs_t d_regs;
  tlb_regs_t i_regs;

  unsigned long long lfsr;   /* LFSR bits for replacement */

  /* D-TLB tags and data */
  tte_tag  dt16_tag[16];
  tte_data dt16_data[16];
  tte_tag  dt512_tag[512];
  tte_data dt512_data[512];

  /* I-TLB tags and data */
  tte_tag  it16_tag[16];
  tte_data it16_data[16];
  tte_tag  it128_tag[128];
  tte_data it128_data[128];

} mmu_t;

typedef enum {
  CLASS_ASI_PRIMARY        = 0,
  CLASS_ASI_SECONDARY      = 1,
  CLASS_ASI_NUCLEUS        = 2,
  CLASS_ASI_NONTRANSLATING = 4
} asi_class_t;

typedef enum {
  MMU_TRANSLATE = 0,  /* for 'normal' translations */
  MMU_TRANSLATE_PF,   /* translate but don't trap */
  MMU_DEMAP_PAGE
} mmu_translation_type_t;

typedef enum {
  MMU_TSB_8K_PTR = 0,
  MMU_TSB_64K_PTR
} mmu_ptr_type_t;

/* taken from simics v9_exception_type */
typedef enum {
  no_exception                     = 0x000,
  /* */
  instruction_access_exception     = 0x008,
  instruction_access_mmu_miss      = 0x009,
  instruction_access_error         = 0x00a,
  /* */
  data_access_exception            = 0x030,
  data_access_mmu_miss             = 0x031,
  data_access_error                = 0x032,
  data_access_protection           = 0x033,
  mem_address_not_aligned          = 0x034,
  /* */
  priveledged_action               = 0x037,
  /* */
  fast_instruction_access_MMU_miss = 0x064,
  fast_data_access_MMU_miss        = 0x068,
  fast_data_access_protection      = 0x06c
  /* */
} mmu_exception_t;

typedef enum {
  mmu_access_load  = 1,
  mmu_access_store = 2
} mmu_access_type_t;

typedef struct mmu_access {
  address_t va;     /* virtual address */
  asi_t asi;         /* ASI */
  mmu_access_type_t type;        /* load or store */
  mmu_reg_t val;  /* store value or load result */
} mmu_access_t;

/* prototypes */
bool mmu_is_cacheable(tte_data data);
bool mmu_is_sideeffect(tte_data data);
bool mmu_is_xendian(tte_data data);
address_t mmu_make_paddr(tte_data data, address_t va);

address_t mmu_translate(mmu_t * mmu,
                        unsigned int is_fetch,
                        address_t va,
                        unsigned int asi_class,
                        unsigned int asi,
                        unsigned int nofault,
                        unsigned int priv,
                        unsigned int access_type,
                        mmu_exception_t * except,
                        mmu_translation_type_t trans_type);

tte_data mmu_lookup(mmu_t * mmu,
                    unsigned int is_fetch,
                    address_t va,
                    unsigned int asi_class,
                    unsigned int asi,
                    unsigned int nofault,
                    unsigned int priv,
                    unsigned int access_type,
                    mmu_exception_t * except,
                    mmu_translation_type_t trans_type);

address_t mmu_generate_tsb_ptr(address_t va,
                               mmu_ptr_type_t type,
                               address_t tsb_base_in,
                               unsigned int tsb_split,
                               unsigned int tsb_size,
                               mmu_reg_t tsb_ext);

void mmu_access(mmu_t * mmu, mmu_access_t * access);

void fm_print_mmu_regs(mmu_t * mmu);
int fm_compare_regs(tlb_regs_t * a, tlb_regs_t * b, const char * who);
int fm_compare_mmus(mmu_t * a, mmu_t * b);

} //end namespace MMU
#endif
} //end Namespace Qemu
} //end namespace Flexus

#if FLEXUS_TARGET_IS(v9)
#include <core/qemu/mai_api.hpp>
namespace Flexus {
namespace Qemu {
namespace MMU {
//void fm_init_mmu_from_simics(mmu_t * mmu, Flexus::Qemu::API::conf_object_t * chmmu); //FIXME
}
}
}
#endif

#endif //FLEXUS_SIMICS_MAI_API_HPP_INCLUDED

