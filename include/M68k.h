/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _M68K_H
#define _M68K_H

#include <stdint.h>
#include <stdarg.h>

#include "nodes.h"
#include "md5.h"
#include "lists.h"

struct M68KLocalState {
    void *          mls_M68kPtr;
    uint32_t        mls_ARMOffset;
    uint8_t         mls_RegMap[16];
    int32_t         mls_PCRel;
};

struct M68KTranslationUnit {
    struct Node     mt_HashNode;
    struct Node     mt_LRUNode;
    uint16_t *      mt_M68kAddress;
    uint16_t *      mt_M68kLow;
    uint16_t *      mt_M68kHigh;
    uint32_t        mt_PrologueSize;
    uint32_t        mt_EpilogueSize;
    uint32_t        mt_Conditionals;
    uint32_t        mt_M68kInsnCnt;
    uint32_t        mt_ARMInsnCnt;
    uint64_t        mt_UseCount;
    uint64_t        mt_FetchCount;
    void *          mt_ARMEntryPoint;
    struct M68KLocalState *  mt_LocalState;
    uint32_t        mt_CRC32;
    uint32_t        mt_ARMCode[]
#ifdef __aarch64__
    __attribute__((aligned(64)));
#else
     __attribute__((aligned(32)));
#endif
};

struct M68KState
{
    /* Integer part */

    union {
        uint8_t u8[4];
        int8_t s8[4];
        uint16_t u16[2];
        int16_t s16[2];
        uint32_t u32;
        int32_t s32;
    } D[8];
    union {
        uint16_t u16[2];
        int16_t s16[2];
        uint32_t u32;
        int32_t s32;
    } A[8];
    union {
        uint16_t u16[2];
        int16_t s16[2];
        uint32_t u32;
        int32_t s32;
    } USP, MSP, ISP;

    uint32_t PC;
    uint16_t SR;
    uint32_t VBR;
    uint32_t CACR;

    /* FPU Part */
    uint32_t FPSR;
    uint32_t FPIAR;
    uint16_t FPCR;
    union {
		uint8_t B;
		uint16_t W;
		uint32_t L;
		float S;
		double d;
		long double X; //place holded dependant on FMT setting compiler will default to double is rounding is set to default.
		long double P; //DO NOT USE!!!
		uint64_t u64;
		uint32_t u32[2];
		};
    } FP[8];   // Double precision! Extended is "emulated" in load/store only

    /* Async IRQ part */
    uint32_t PINT;
    uint32_t IPL0;
    uint64_t INSN_COUNT;
};

#define CACR_DE 0x80000000
#define CACR_IE 0x00008000

#define CACRB_DE 31
#define CACRB_IE 15

#define SR_C    0x0001
#define SR_V    0x0002
#define SR_VC   0x0003
#define SR_Z    0x0004
#define SR_ZC   0x0005
#define SR_ZV   0x0006
#define SR_ZVC  0x0007
#define SR_N    0x0008
#define SR_NC   0x0009
#define SR_NV   0x000a
#define SR_NVC  0x000b
#define SR_NZ   0x000c
#define SR_NZC  0x000d
#define SR_NZV  0x000e
#define SR_NZVC 0x000f
#define SR_X    0x0010
#define SR_XC   0x0011
#define SR_XV   0x0012
#define SR_XVC  0x0013
#define SR_XZ   0x0014
#define SR_XZC  0x0015
#define SR_XZV  0x0016
#define SR_XZVC 0x0017
#define SR_XN   0x0018
#define SR_XNC  0x0019
#define SR_XNV  0x001a
#define SR_XNVC 0x001b
#define SR_XNZ  0x001c
#define SR_XNZC 0x001d
#define SR_XNZV 0x001e
#define SR_CCR  0x001f
#define SR_IPL  0x0700
#define SR_M    0x1000
#define SR_S    0x2000
#define SR_T0   0x4000
#define SR_T1   0x8000

#define SRB_C    0
#define SRB_V    1
#define SRB_Z    2
#define SRB_N    3
#define SRB_X    4
#define SRB_IPL  8
#define SRB_M    12
#define SRB_S    13
#define SRB_T0   14
#define SRB_T1   15

#define FPSR_INEX	0x00000008
#define FPSR_IOP	0x00000080
#define FPSR_INEX1	0x00000100
#define FPSR_INEX2	0x00000200
#define FPSR_DZ		0x00000410	//these also appear in AEXC
#define FPSR_UNFL	0x00000820	//these also appear in AEXC
#define FPSR_OVFL	0x00001040	//these also appear in AEXC
#define FPSR_OPERR	0x00002000
#define FPSR_SNAN	0x00004000
#define FPSR_BSUN	0x00008000
#define FPSR_Q		0x007F0000
#define FPSR_S		0x00800000
#define FPSR_N      0x08000000
#define FPSR_Z      0x04000000
#define FPSR_I      0x02000000
#define FPSR_NAN    0x01000000

#define	FPSRB_S		23
#define FPSRB_N     27
#define FPSRB_Z     26
#define FPSRB_I     25
#define FPSRB_NAN   24

#define M_CC_T  0x00
#define M_CC_F  0x01
#define M_CC_HI 0x02
#define M_CC_LS 0x03
#define M_CC_CC 0x04
#define M_CC_CS 0x05
#define M_CC_NE 0x06
#define M_CC_EQ 0x07
#define M_CC_VC 0x08
#define M_CC_VS 0x09
#define M_CC_PL 0x0a
#define M_CC_MI 0x0b
#define M_CC_GE 0x0c
#define M_CC_LT 0x0d
#define M_CC_GT 0x0e
#define M_CC_LE 0x0f

#define F_CC_EQ     0x01
#define F_CC_NE     0x0e
#define F_CC_GT     0x12
#define F_CC_NGT    0x1d
#define F_CC_GE     0x13
#define F_CC_NGE    0x1c
#define F_CC_LT     0x14
#define F_CC_NLT    0x1b
#define F_CC_LE     0x15
#define F_CC_NLE    0x1a
#define F_CC_GL     0x16
#define F_CC_NGL    0x19
#define F_CC_GLE    0x17
#define F_CC_NGLE   0x18
#define F_CC_OGT    0x02
#define F_CC_ULE    0x0d
#define F_CC_OGE    0x03
#define F_CC_ULT    0x0c
#define F_CC_OLT    0x04
#define F_CC_UGE    0x0b
#define F_CC_OLE    0x05
#define F_CC_UGT    0x0a
#define F_CC_OGL    0x06
#define F_CC_UEQ    0x09
#define F_CC_OR     0x07
#define F_CC_UN     0x08
#define F_CC_F      0x00
#define F_CC_T      0x0f
#define F_CC_SF     0x10
#define F_CC_ST     0x1f
#define F_CC_SEQ    0x11
#define F_CC_SNE    0x1e

#define VECTOR_RESET_ISP            0x000
#define VECTOR_RESET_PC             0x004
#define VECTOR_ACCESS_FAULT         0x008
#define VECTOR_ADDRESS_ERROR        0x00c
#define VECTOR_ILLEGAL_INSTRUCTION  0x010
#define VECTOR_DIVIDE_BY_ZERO       0x014
#define VECTOR_CHK                  0x018
#define VECTOR_TRAPcc               0x01c
#define VECTOR_PRIVILEGE_VIOLATION  0x020
#define VECTOR_TRACE                0x024
#define VECTOR_LINE_A               0x028
#define VECTOR_LINE_F               0x02c
#define VECTOR_FORMAT_ERROR         0x038
#define VECTOR_UNINITIALIZED_INT    0x03c
#define VECTOR_INT_SPURIOUS         0x060
#define VECTOR_INT_LEVEL1           0x064
#define VECTOR_INT_LEVEL2           0x068
#define VECTOR_INT_LEVEL3           0x06c
#define VECTOR_INT_LEVEL4           0x070
#define VECTOR_INT_LEVEL5           0x074
#define VECTOR_INT_LEVEL6           0x078
#define VECTOR_INT_LEVEL7           0x07c

#define VECTOR_INT_TRAP(n)          (0x080 + ((n) & 15)*4)

uint32_t *EMIT_GetOffsetPC(uint32_t *ptr, int8_t *offset);
uint32_t *EMIT_AdvancePC(uint32_t *ptr, uint8_t offset);
uint32_t *EMIT_FlushPC(uint32_t *ptr);
uint32_t *EMIT_ResetOffsetPC(uint32_t *ptr);
uint32_t *EMIT_LoadFromEffectiveAddress(uint32_t *ptr, uint8_t size, uint8_t *arm_reg, uint8_t ea, uint16_t *m68k_ptr, uint8_t *ext_words, uint8_t read_only, int32_t *imm_offset);
uint32_t *EMIT_StoreToEffectiveAddress(uint32_t *ptr, uint8_t size, uint8_t *arm_reg, uint8_t ea, uint16_t *m68k_ptr, uint8_t *ext_words);
uint32_t *EMIT_Exception(uint32_t *ptr, uint16_t exception, uint8_t format, ...);

uint32_t *EMIT_line0(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed);
uint32_t *EMIT_line4(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed);
uint32_t *EMIT_line5(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed);
uint32_t *EMIT_line6(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed);
uint32_t *EMIT_moveq(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed);
uint32_t *EMIT_line8(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed);
uint32_t *EMIT_line9(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed);
uint32_t *EMIT_lineB(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed);
uint32_t *EMIT_lineC(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed);
uint32_t *EMIT_lineD(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed);
uint32_t *EMIT_lineE(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed);
uint32_t *EMIT_lineF(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed);
uint32_t *EMIT_move(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed);

typedef uint32_t * (*EMIT_Function)(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr);
typedef uint32_t * (*EMIT_MultiFunction)(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed);

uint32_t *EMIT_InjectPrintContext(uint32_t *ptr);
uint32_t *EMIT_InjectDebugStringV(uint32_t *ptr, const char * restrict format, va_list args);
uint32_t *EMIT_InjectDebugString(uint32_t *ptr, const char * restrict format, ...);

void M68K_PushReturnAddress(uint16_t *ret_addr);
uint16_t *M68K_PopReturnAddress(uint8_t *success);
void M68K_ResetReturnStack();
int M68K_GetINSNLength(uint16_t *insn_stream);
int M68K_IsBranch(uint16_t *insn_stream);

uint8_t EMIT_TestCondition(uint32_t **pptr, uint8_t m68k_condition);
uint8_t M68K_GetSRMask(uint16_t *m68k_stream);
void M68K_InitializeCache();
struct M68KTranslationUnit *M68K_GetTranslationUnit(uint16_t *ptr);
void *M68K_TranslateNoCache(uint16_t *m68kcodeptr);
struct M68KTranslationUnit *M68K_VerifyUnit(struct M68KTranslationUnit *unit);
void M68K_DumpStats();
uint8_t M68K_GetCC(uint32_t **ptr);
uint8_t M68K_ModifyCC(uint32_t **ptr);
void M68K_FlushCC(uint32_t **ptr);

static inline __attribute__((always_inline)) struct M68KTranslationUnit *M68K_FindTranslationUnit(uint16_t *ptr)
{
    struct M68KTranslationUnit *unit = NULL, *n;
    uintptr_t hash = (uintptr_t)ptr;
    extern struct List *ICache;
    extern struct List LRU;

    /* Get 16-bit has from the pointer to m68k code */
    hash = (hash ^ (hash >> 16)) & 0xffff;

    /* Find entry with correct address */
    ForeachNode(&ICache[hash], n)
    {
        if (n->mt_M68kAddress == ptr)
        {
            /* Unit found? Move it to the front of LRU list */
            unit = n;

            struct Node *this = &unit->mt_LRUNode;

#ifdef __aarch64__
            /* Correct unit found. Preload ICache */
            //asm volatile ("prfm plil1keep, [%0]"::"r"(unit->mt_ARMEntryPoint));
#endif
            if (1)
            {
                // Update LRU for least *frequently* used strategy
                if (this->ln_Pred->ln_Pred) {
                    struct Node *pred = this->ln_Pred;
                    struct Node *succ = this->ln_Succ;

                    this->ln_Pred = pred->ln_Pred;
                    this->ln_Succ = pred;
                    this->ln_Pred->ln_Succ = this;
                    pred->ln_Pred = this;
                    pred->ln_Succ = succ;
                    succ->ln_Pred = pred;
                }
            }
            else
            {
                // Update LRU for least *recently* used strategy
                REMOVE(&unit->mt_LRUNode);
                ADDHEAD(&LRU, &unit->mt_LRUNode);
            }

            return unit;
        }
    }

    return NULL;
}

#endif /* _M68K_H */
