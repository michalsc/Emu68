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
		uint64_t u64;
		uint32_t u32[2];
    } FP[8];   // Double precision! Extended is "emulated" in load/store only

    /* More control registers.. */
    uint8_t  SFC;
    uint8_t  DFC;
    uint16_t TCR;
    uint32_t URP;
    uint32_t SRP;
    uint32_t MMUSR;
    uint32_t ITT0;
    uint32_t ITT1;
    uint32_t DTT0;
    uint32_t DTT1;

    /* Async IRQ part */
    union {
        struct {
            uint8_t ARM;
            uint8_t ARM_err;
            uint8_t IPL;
            uint8_t RESET;
        } INT;
        uint32_t INT32;
    };
    uint64_t INSN_COUNT;

    uint32_t JIT_CACHE_MISS;
    uint32_t JIT_UNIT_COUNT;
    uint32_t JIT_CACHE_TOTAL;
    uint32_t JIT_CACHE_FREE;
    uint32_t JIT_SOFTFLUSH_THRESH;
    uint32_t JIT_CONTROL;
    uint32_t JIT_CONTROL2;
};

#define JCCB_SOFT               0
#define JCCF_SOFT               0x00000001
#define JCCB_SOFT_LIMIT         1
#define JCCB_INSN_DEPTH         24
#define JCCB_INSN_DEPTH_MASK    0xff
#define JCCB_INLINE_RANGE       8
#define JCCB_INLINE_RANGE_MASK  0xffff
#define JCCB_LOOP_COUNT         4
#define JCCB_LOOP_COUNT_MASK    0xf

#define JC2B_CHIP_SLOWDOWN              0
#define JC2F_CHIP_SLOWDOWN              (1 << JC2B_CHIP_SLOWDOWN)
#define JC2B_DBF_SLOWDOWN               1
#define JC2F_DBF_SLOWDOWN               (1 << JC2B_DBF_SLOWDOWN)
#define JC2B_CCR_SCAN_DEPTH             3
#define JC2_CCR_SCAN_MASK               0x1f
#define JC2B_CHIP_SLOWDOWN_RATIO        8
#define JC2_CHIP_SLOWDOWN_RATIO_MASK    0x07
#define JC2B_BLITWAIT                   11
#define JC2F_BLITWAIT                   (1 << JC2B_BLITWAIT)

#define DCB_VERBOSE 0
#define DCB_VERBOSE_MASK 0x3
#define DCB_DISASM  2
#define DCF_DISASM  0x00000004

#define CACR_DE 0x80000000
#define CACR_IE 0x00008000

#define CACRB_DE 31
#define CACRB_IE 15

//SR
#define SR_Calt 0x0002
#define SR_Valt 0x0001
#define SR_ZCalt 0x0006
#define SR_ZValt 0x0005
#define SR_NCalt 0x000a
#define SR_NValt 0x0009
#define SR_NZCalt 0x000e
#define SR_NZValt 0x000d
#define SR_XCalt 0x0012
#define SR_XValt 0x0011
#define SR_XZCalt 0x0016
#define SR_XZValt 0x0015
#define SR_XNCalt 0x001a
#define SR_XNValt 0x0019
#define SR_XNZCalt 0x001e
#define SR_XNZValt 0x001d

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

#define SR_ALL  0xf71f

#define SRB_Calt 1
#define SRB_Valt 0

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

//MMUSR
#define MMUSR_B 0x8000
#define MMUSR_L 0x4000
#define MMUSR_S 0x2000
#define MMUSR_A 0x1000
#define MMUSR_W 0x0800
#define MMUSR_I 0x0400
#define MMUSR_M 0x0200
#define MMUSR_G 0x0100
#define MMUSR_C 0x0080
#define MMUSR_N 0x0007

//FPCR
#define FPCR_RND	0x00000030
#define FPCR_PREC	0x000000c0
#define FPCR_INEX1	0x00000100
#define FPCR_INEX2	0x00000200
#define FPCR_DZ		0x00000400
#define FPCR_UNFL	0x00000800
#define FPCR_OVFL	0x00001000
#define FPCR_OPERR	0x00002000
#define FPCR_SNAN	0x00004000
#define FPCR_BSUN	0x00008000

#define FPEE		0x0000ff00
#define FPMC		0x000000f0

#define FPCR_ALL	0x0000fff0

#define FPCRB_RND	4
#define FPCRB_PREC	6
#define FPCRB_INEX1	8
#define FPCRB_INEX2	9
#define FPCRB_DZ	10
#define FPCRB_UNFL	11
#define	FPCRB_OVFL	12
#define FPCRB_OPERR	13
#define FPCRB_SNAN	14
#define FPCRB_BSUN	15

//FPSR
#define FPSR_INEX	0x00000008
#define FPSR_IOP	0x00000080
#define FPSR_INEX1	0x00000100
#define FPSR_INEX2	0x00000200
#define FPSR_DZ		0x00000410	//these also appear in AEXC, this should be split for obvious reasons!
#define FPSR_UNFL	0x00000820	//these also appear in AEXC, this should be split for obvious reasons!
#define FPSR_OVFL	0x00001040	//these also appear in AEXC, this should be split for obvious reasons!
#define FPSR_OPERR	0x00002000
#define FPSR_SNAN	0x00004000
#define FPSR_BSUN	0x00008000
#define FPSR_Q		0x007F0000
#define FPSR_S		0x00800000
#define FPSR_N      0x08000000
#define FPSR_Z      0x04000000
#define FPSR_NZ		0x0c000000
#define FPSR_I      0x02000000
#define FPSR_NI		0x0a000000
#define FPSR_NAN    0x01000000
#define FPSR_NNAN	0x09000000

#define FPCC		0x0f000000
#define FPQB		0x00ff0000
#define FPEB		0x0000ff00
#define FPAEB		0x000000f8
#define FPEW		0x0000fff8  //Exception word; to be used when an expection is expected

#define FPSR_ALL	0x0ffffff8

#define FPSRB_INEX1	8
#define FPSRB_INEX2	9
#define FPSRB_DZ	10
#define FPSRB_UNFL	11
#define FPSRB_OVFL	12
#define FPSRB_OPERR	13
#define FPSRB_SNAN	14
#define FPSRB_BSUN	15
#define	FPSRB_S		23
#define FPSRB_N     27
#define FPSRB_Z     26
#define FPSRB_I     25
#define FPSRB_NAN   24

//Condition Codes
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

#define P_CC_BS 000
#define P_CC_BC 001
#define P_CC_LS 002
#define P_CC_LC 003
#define P_CC_SS 004
#define P_CC_SC 005
#define P_CC_AS 006
#define P_CC_AC 007
#define P_CC_WS 010
#define P_CC_WC 011
#define P_CC_IS 012
#define P_CC_IC 013
#define P_CC_GS 014
#define P_CC_GC 015
#define P_CC_CS 016
#define P_CC_CC 017

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

//Co-processors, not defined USER coprocessors for obvious reasons
#define cp_MMU      00
#define cp_FPU      01

//Vectors Exception handling
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
//FPU Vector Exception
#define VECTOR_UNORDERED_CONDITION  0xC0
#define VECTOR_INEXACT_RESULT       0xC4
#define VECTOR_FP_DIVIDE_BY_ZERO    0xC8
#define VECTOR_UNDERFLOW            0xCC
#define VECTOR_OPERAND_ERROR        0xD0
#define VECTOR_OVERFLOW             0xD4
#define VECTOR_SIGNALING_NAN        0xD8

uint32_t *EMIT_GetOffsetPC(uint32_t *ptr, int8_t *offset);
uint32_t *EMIT_AdvancePC(uint32_t *ptr, uint8_t offset);
uint32_t *EMIT_FlushPC(uint32_t *ptr);
uint32_t *EMIT_ResetOffsetPC(uint32_t *ptr);
uint32_t *EMIT_LoadFromEffectiveAddress(uint32_t *ptr, uint8_t size, uint8_t *arm_reg, uint8_t ea, uint16_t *m68k_ptr, uint8_t *ext_words, uint8_t read_only, int32_t *imm_offset);
uint32_t *EMIT_StoreToEffectiveAddress(uint32_t *ptr, uint8_t size, uint8_t *arm_reg, uint8_t ea, uint16_t *m68k_ptr, uint8_t *ext_words, int sign_extend);
uint32_t *EMIT_Exception(uint32_t *ptr, uint16_t exception, uint8_t format, ...);
uint32_t *EMIT_LocalExit(uint32_t *ptr, uint32_t insn_count_fixup);
uint32_t *EMIT_JumpOnCondition(uint32_t *ptr, uint8_t m68k_condition, uint32_t distance);

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
uint32_t *EMIT_line7(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed);
uint32_t *EMIT_line1(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed);
uint32_t *EMIT_line2(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed);
uint32_t *EMIT_line3(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed);

uint32_t GetSR_Line0(uint16_t opcode);
uint32_t GetSR_Line1(uint16_t opcode);
uint32_t GetSR_Line2(uint16_t opcode);
uint32_t GetSR_Line3(uint16_t opcode);
uint32_t GetSR_Line4(uint16_t opcode);
uint32_t GetSR_Line5(uint16_t opcode);
uint32_t GetSR_Line6(uint16_t opcode);
uint32_t GetSR_Line7(uint16_t opcode);
uint32_t GetSR_Line8(uint16_t opcode);
uint32_t GetSR_Line9(uint16_t opcode);
uint32_t GetSR_LineB(uint16_t opcode);
uint32_t GetSR_LineC(uint16_t opcode);
uint32_t GetSR_LineD(uint16_t opcode);
uint32_t GetSR_LineE(uint16_t opcode);

int M68K_GetLine0Length(uint16_t *insn_stream);
int M68K_GetLine1Length(uint16_t *insn_stream);
int M68K_GetLine2Length(uint16_t *insn_stream);
int M68K_GetLine3Length(uint16_t *insn_stream);
int M68K_GetLine4Length(uint16_t *insn_stream);
int M68K_GetLine5Length(uint16_t *insn_stream);
int M68K_GetLine6Length(uint16_t *insn_stream);
int M68K_GetLine7Length(uint16_t *insn_stream);
int M68K_GetLine8Length(uint16_t *insn_stream);
int M68K_GetLine9Length(uint16_t *insn_stream);
int M68K_GetLineBLength(uint16_t *insn_stream);
int M68K_GetLineCLength(uint16_t *insn_stream);
int M68K_GetLineDLength(uint16_t *insn_stream);
int M68K_GetLineELength(uint16_t *insn_stream);
int M68K_GetLineFLength(uint16_t *insn_stream);
uint8_t SR_GetEALength(uint16_t *insn_stream, uint8_t ea, uint8_t imm_size);

typedef uint32_t * (*EMIT_Function)(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr);
typedef uint32_t * (*EMIT_MultiFunction)(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed);

struct OpcodeDef {
    union {
        EMIT_Function       od_Emit;
        EMIT_MultiFunction  od_EmitMulti;
    };
    void *          od_Interpret;   // Not used yet.
    uint16_t        od_SRNeeds;
    uint16_t        od_SRSets;
    uint8_t         od_BaseLength;
    uint8_t         od_HasEA;
    uint8_t         od_OpSize;
};

struct FPUOpcodeDef {
    EMIT_Function   od_Emit;
    void *          od_Interpret;   // Not used yet.
    uint32_t        od_FPSRNeeds;
    uint32_t        od_FPSRSets;
    uint8_t         od_BaseLength;
    uint8_t         od_HasEA;
    uint8_t         od_OpSize;
};

uint32_t *EMIT_InjectPrintContext(uint32_t *ptr);
uint32_t *EMIT_InjectDebugStringV(uint32_t *ptr, const char * restrict format, va_list args);
uint32_t *EMIT_InjectDebugString(uint32_t *ptr, const char * restrict format, ...);

void M68K_PushReturnAddress(uint16_t *ret_addr);
uint16_t *M68K_PopReturnAddress(uint8_t *success);
void M68K_ResetReturnStack();
int M68K_GetINSNLength(uint16_t *insn_stream);
int M68K_IsBranch(uint16_t *insn_stream);

uint8_t EMIT_TestCondition(uint32_t **pptr, uint8_t m68k_condition);
uint8_t EMIT_TestFPUCondition(uint32_t **pptr, uint8_t m68k_condition);
uint8_t M68K_GetSRMask(uint16_t *m68k_stream);
void M68K_InitializeCache();
struct M68KTranslationUnit *M68K_GetTranslationUnit(uint16_t *ptr);
void *M68K_TranslateNoCache(uint16_t *m68kcodeptr);
struct M68KTranslationUnit *M68K_VerifyUnit(struct M68KTranslationUnit *unit);
void M68K_DumpStats();
uint8_t M68K_GetCC(uint32_t **ptr);
uint8_t M68K_ModifyCC(uint32_t **ptr);
void M68K_FlushCC(uint32_t **ptr);

#endif /* _M68K_H */
