/*
    Copyright © 2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _PPC_H
#define _PPC_H

#include <stdint.h>
#include <stdarg.h>
#include <cpp/Node>

#include "A64.h"

#include "TranslatorContext.hpp"

namespace Emu68::PPC
{

struct ExitBlock : public Emu68::Node
{
    uint32_t    eb_FixupCount;
    uint32_t    eb_InstructionCount;
    struct {
        uint32_t    type;
        uint32_t *  location;
    } *         eb_Fixup;
    uint32_t *  eb_ARMCode;
};

struct PPCState
{
    /* UISA - integer part */
    uint32_t GPR[32];
    uint32_t CR;    /* Must follow GPR immediately! */
    uint32_t XER;   /* Must follow CR immediately! */
    uint32_t LR;
    uint32_t CTR;   /* Must follow LR immediately! */
    uint32_t PC;

    /* UISA - FPU Part */
    uint32_t FPSCR;
    union {
        double FPR[32];
        uint64_t FPR_u64[32];
    } __attribute__((aligned(8)));

    /* VEA */
    uint64_t TB_offset;

    /* Async IRQ part */
    union {
        struct {
            volatile uint8_t ARM;
            volatile uint8_t ARM_err;
            volatile uint8_t EXT;
            volatile uint8_t RESET;
            volatile uint8_t DEC;
        } INT;
        volatile uint64_t INT64;
    };

    /* OEA */
    uint32_t MSR;
    uint32_t SRR0;
    uint32_t SRR1;
    uint32_t SPRG[8];
    uint32_t DAR;
    uint32_t DSISR;

    uint64_t INSN_COUNT;
    uint32_t JIT_CACHE_MISS;
    uint32_t JIT_UNIT_COUNT;
    uint32_t JIT_CACHE_TOTAL;
    uint32_t JIT_CACHE_FREE;
    uint32_t JIT_CONTROL;
    uint32_t JIT_CONTROL2;
    uint32_t BASEREG;
    uint8_t * M68K_FLAG;
};

#define MSR_LE      0x000001
#define MSR_RI      0x000002
#define MSR_DR      0x000010
#define MSR_IR      0x000020
#define MSR_IP      0x000040
#define MSR_FE1     0x000100
#define MSR_BE      0x000200
#define MSR_SE      0x000400
#define MSR_FE0     0x000800
#define MSR_ME      0x001000
#define MSR_FP      0x002000
#define MSR_PR      0x004000
#define MSR_EE      0x008000
#define MSR_ILE     0x010000
#define MSR_POW     0x040000

#define REG_PC      18
#define REG_LR      28
#define REG_CTR     29
#define REG_GPR0    13
#define REG_GPR1    14
#define REG_GPR2    15
#define REG_GPR3    16
#define REG_GPR4    17
#define REG_GPR5    19
#define REG_GPR6    20
#define REG_GPR7    21
#define REG_GPR8    22
#define REG_GPR9    23
#define REG_GPR10   24
#define REG_GPR11   25
#define REG_GPR12   26
#define REG_GPR13   27

#define REG_FPR0    8
#define REG_FPR1    9
#define REG_FPR2    10
#define REG_FPR3    11
#define REG_FPR4    12
#define REG_FPR5    13
#define REG_FPR6    14
#define REG_FPR7    15
#define REG_FPR8    16
#define REG_FPR9    17
#define REG_FPR10   18
#define REG_FPR11   19
#define REG_FPR12   27
#define REG_FPR13   28
#define REG_FPR29   29
#define REG_FPR30   30
#define REG_FPR31   31

#define REG_FPSCR_VN 20
#define REG_FPSCR_SIZE TS_S
#define REG_FPSCR_POS 0
#define REG_FPSCR_ASM "v20.s[0]"

#define REG_FPSCR    REG_FPSCR_VN,REG_FPSCR_SIZE,REG_FPSCR_POS

#define EPOCH_VN 20
#define EPOCH_SIZE TS_S
#define EPOCH_POS 1
#define EPOCH_ASM "v20.s[1]"

#define EPOCH       EPOCH_VN,EPOCH_SIZE,EPOCH_POS

#undef CTX_LAST_PC_VN
#undef CTX_LAST_PC_SIZE
#undef CTX_LAST_PC_POS
#undef CTX_LAST_PC_ASM

#define CTX_LAST_PC_VN 20
#define CTX_LAST_PC_SIZE TS_S
#define CTX_LAST_PC_POS 3
#define CTX_LAST_PC_ASM "v20.s[3]"

#undef CTX_POINTER_VN
#undef CTX_POINTER_SIZE
#undef CTX_POINTER_POS
#undef CTX_POINTER_ASM

#define CTX_POINTER_VN 21
#define CTX_POINTER_SIZE TS_D
#define CTX_POINTER_POS 1
#define CTX_POINTER_ASM "v21.d[1]"

#undef CTX_INSN_COUNT_VN
#undef CTX_INSN_COUNT_ASM
#undef CTX_INSN_COUNT_SIZE
#undef CTX_INSN_COUNT_POS

#define CTX_INSN_COUNT_VN 21
#define CTX_INSN_COUNT_SIZE TS_D
#define CTX_INSN_COUNT_POS 0
#define CTX_INSN_COUNT_ASM "v21.d[0]"

#define CTX_POINTER     CTX_POINTER_VN,CTX_POINTER_SIZE,CTX_POINTER_POS
#define CTX_INSN_COUNT  CTX_INSN_COUNT_VN,CTX_INSN_COUNT_SIZE,CTX_INSN_COUNT_POS
#define CTX_LAST_PC     CTX_LAST_PC_VN,CTX_LAST_PC_SIZE,CTX_LAST_PC_POS

#define GPR14_VN 22
#define GPR14_SIZE TS_S
#define GPR14_POS 0
#define GPR14_ASM "v22.s[0]"

#define GPR15_VN 22
#define GPR15_SIZE TS_S
#define GPR15_POS 1
#define GPR15_ASM "v22.s[1]"

#define GPR16_VN 22
#define GPR16_SIZE TS_S
#define GPR16_POS 2
#define GPR16_ASM "v22.s[2]"

#define GPR17_VN 22
#define GPR17_SIZE TS_S
#define GPR17_POS 3
#define GPR17_ASM "v22.s[3]"

#define GPR18_VN 23
#define GPR18_SIZE TS_S
#define GPR18_POS 0
#define GPR18_ASM "v23.s[0]"

#define GPR19_VN 23
#define GPR19_SIZE TS_S
#define GPR19_POS 1
#define GPR19_ASM "v23.s[1]"

#define GPR20_VN 23
#define GPR20_SIZE TS_S
#define GPR20_POS 2
#define GPR20_ASM "v23.s[2]"

#define GPR21_VN 23
#define GPR21_SIZE TS_S
#define GPR21_POS 3
#define GPR21_ASM "v23.s[3]"

#define GPR22_VN 24
#define GPR22_SIZE TS_S
#define GPR22_POS 0
#define GPR22_ASM "v24.s[0]"

#define GPR23_VN 24
#define GPR23_SIZE TS_S
#define GPR23_POS 1
#define GPR23_ASM "v24.s[1]"

#define GPR24_VN 24
#define GPR24_SIZE TS_S
#define GPR24_POS 2
#define GPR24_ASM "v24.s[2]"

#define GPR25_VN 24
#define GPR25_SIZE TS_S
#define GPR25_POS 3
#define GPR25_ASM "v24.s[3]"

#define GPR26_VN 25
#define GPR26_SIZE TS_S
#define GPR26_POS 0
#define GPR26_ASM "v25.s[0]"

#define GPR27_VN 25
#define GPR27_SIZE TS_S
#define GPR27_POS 1
#define GPR27_ASM "v25.s[1]"

#define GPR28_VN 25
#define GPR28_SIZE TS_S
#define GPR28_POS 2
#define GPR28_ASM "v25.s[2]"

#define GPR29_VN 25
#define GPR29_SIZE TS_S
#define GPR29_POS 3
#define GPR29_ASM "v25.s[3]"

#define GPR30_VN 26
#define GPR30_SIZE TS_S
#define GPR30_POS 0
#define GPR30_ASM "v26.s[0]"

#define GPR31_VN 26
#define GPR31_SIZE TS_S
#define GPR31_POS 1
#define GPR31_ASM "v26.s[1]"

#define REG_CR_VN 26
#define REG_CR_SIZE TS_S
#define REG_CR_POS 2
#define REG_CR_ASM "v26.s[2]"

#define REG_XER_VN 26
#define REG_XER_SIZE TS_S
#define REG_XER_POS 3
#define REG_XER_ASM "v26.s[3]"

#define GPR14       GPR14_VN,GPR14_SIZE,GPR14_POS
#define GPR15       GPR15_VN,GPR15_SIZE,GPR15_POS
#define GPR16       GPR16_VN,GPR16_SIZE,GPR16_POS
#define GPR17       GPR17_VN,GPR17_SIZE,GPR17_POS
#define GPR18       GPR18_VN,GPR18_SIZE,GPR18_POS
#define GPR19       GPR19_VN,GPR19_SIZE,GPR19_POS
#define GPR20       GPR20_VN,GPR20_SIZE,GPR20_POS
#define GPR21       GPR21_VN,GPR21_SIZE,GPR21_POS
#define GPR22       GPR22_VN,GPR22_SIZE,GPR22_POS
#define GPR23       GPR23_VN,GPR23_SIZE,GPR23_POS
#define GPR24       GPR24_VN,GPR24_SIZE,GPR24_POS
#define GPR25       GPR25_VN,GPR25_SIZE,GPR25_POS
#define GPR26       GPR26_VN,GPR26_SIZE,GPR26_POS
#define GPR27       GPR27_VN,GPR27_SIZE,GPR27_POS
#define GPR28       GPR28_VN,GPR28_SIZE,GPR28_POS
#define GPR29       GPR29_VN,GPR29_SIZE,GPR29_POS
#define GPR30       GPR30_VN,GPR30_SIZE,GPR30_POS
#define GPR31       GPR31_VN,GPR31_SIZE,GPR31_POS
#define REG_CR      REG_CR_VN,REG_CR_SIZE,REG_CR_POS
#define REG_XER     REG_XER_VN,REG_XER_SIZE,REG_XER_POS

struct PPCLocalState
{
    void *          pls_PPCPtr;
    uint32_t        pls_ARMOffset;
    uint8_t         pls_RegMap[38];
    int32_t         pls_PCRel;
};

struct RegisterNode : public Emu68::Node {
    uint8_t rn_RegNum;
    uint8_t rn_ARM;
    uint8_t rn_Dirty;
};

struct PPCTranslationUnit;
struct TranslationUnitLRU : public Emu68::Node {
    PPCTranslationUnit *unit;
    TranslationUnitLRU(PPCTranslationUnit *node = nullptr) : unit(node) {}
};

struct PPCTranslationUnit : public Emu68::Node
{
    /* Hot part of the structure shall preferably reside in one or at most two cache lines */
    //struct Node         ptu_HashNode;       /* 00: 2 x 8 bytes - prev and next pointer in the has table bucket */
    union {
        struct {
            uint32_t    ptu_Epoch;          /* 16: 2 x 4 bytes - first 32-bit epoch incremented after every cache flush */
            uint32_t    ptu_PPCAddress;     /*                   followed by 32-bit PPC entry address */
        };
        uint64_t        ptu_Key;            /*     1 x 8 bytes - match key, the two above combined */
    };
    void *              ptu_ARMEntryPoint;  /* 24: 1 x 8 bytes - entry point for AArch64 code */

    /* Less hot part - in case cache line is 32 bytes long, only */
    uint32_t            ptu_CRC32;          /* 32: 1 x 4 bytes - CRC32 of the whole block*/
    uint32_t            ptu_Fingerprint;    /* 36: 1 x 4 bytes - *mt_M68kAddress ^ *(mt_M68kAddress + 4) */
    uint32_t            ptu_PPCLow;        /* 40: 1 x 4 bytes - lowest m68k address in this block */
    uint32_t            ptu_PPCHigh;       /* 44: 1 x 4 bytes - highest m68k address in this block */
    TranslationUnitLRU  ptu_LRU;

    /* Cold part of the structure */
    uint32_t            ptu_PPCInsnCnt;
    uint32_t            ptu_ARMInsnCnt;
    uint32_t            ptu_CompileTime;
    uint32_t            ptu_VerifyTime;
    #if 0
    uint32_t        mt_PrologueSize;
    uint32_t        mt_EpilogueSize;
    uint32_t        mt_Conditionals;
    uint64_t        mt_UseCount;
    uint64_t        mt_FetchCount;
    #endif
    struct PPCLocalState * ptu_LocalState;

    uint32_t            ptu_ARMCode[0] __attribute__((aligned(64)));
};

struct PPCTranslatorContext : public TranslatorContext {
    uint32_t *      tc_PPCCodeStart;
    uint32_t *      tc_PPCCodePtr;

    int32_t _pc_rel;

    PPCTranslatorContext() : _pc_rel(0) {}

    void getOffsetPC(int8_t *offset);
    void advancePC(uint8_t offset);
    void flushPC();
    void resetOffsetPC() { _pc_rel = 0; }
    void emitException(uint16_t type);
    void emitLocalExit(uint32_t insn_fixup);
};

uint8_t AllocARMRegister(TranslatorContext *tc);
void FreeARMRegister(TranslatorContext *, uint8_t arm_reg);
uint8_t AllocFPRegister(PPCTranslatorContext *tc);
void FreeFPRegister(PPCTranslatorContext *, uint8_t arm_reg);
void SetDirtyGPR(struct TranslatorContext *, uint8_t reg);
void SetDirtyFPR(struct TranslatorContext *, uint8_t reg);

uint8_t GetCTX(PPCTranslatorContext *tc);
uint8_t TryCTX(struct TranslatorContext *);
void StoreDirtyGPRs(PPCTranslatorContext *tc);
void StoreDirtyFPRs(PPCTranslatorContext *tc);
uint8_t MapGPRForRead(PPCTranslatorContext *tc, uint8_t reg);
uint8_t MapGPRForReadAndWrite(PPCTranslatorContext *tc, uint8_t reg);
uint8_t MapGPRForWrite(PPCTranslatorContext *tc, uint8_t reg);
uint8_t IsGPRMapped(PPCTranslatorContext *, uint8_t reg);
uint8_t MapFPRForRead(PPCTranslatorContext *tc, uint8_t reg);
uint8_t MapFPRForReadAndWrite(PPCTranslatorContext *tc, uint8_t reg);
uint8_t MapFPRForWrite(PPCTranslatorContext *tc, uint8_t reg);
uint8_t IsFPRMapped(PPCTranslatorContext *, uint8_t reg);

void ResetReturnStack();
uint32_t *PopReturnAddress(uint8_t *success);
void PushReturnAddress(uint32_t *ret_addr);

inline void PPCTranslatorContext::getOffsetPC(int8_t *offset) {
    // Calculate new PC relative offset
    int new_offset = _pc_rel + *offset;

    // If overflow would occur then compute PC and get new offset
    if (new_offset > 127 || new_offset < -127)
    {
        if (_pc_rel > 0)
            emit(add_immed(REG_PC, REG_PC, _pc_rel));
        else
            emit(sub_immed(REG_PC, REG_PC, -_pc_rel));

        _pc_rel = 0;
        new_offset = *offset;
    }

    *offset = new_offset;
}

inline void PPCTranslatorContext::advancePC(uint8_t offset)
{
    // Calculate new PC relative offset
    _pc_rel += (int)offset;

    // If overflow would occur then compute PC and get new offset
    if (_pc_rel > 120 || _pc_rel < -120)
    {
        if (_pc_rel > 0)
            emit(add_immed(REG_PC, REG_PC, _pc_rel));
        else
            emit(sub_immed(REG_PC, REG_PC, -_pc_rel));

        _pc_rel = 0;
    }
}

inline void PPCTranslatorContext::flushPC()
{
    if (_pc_rel > 0)
        emit(add_immed(REG_PC, REG_PC, _pc_rel));
    else if (_pc_rel < 0)
        emit(sub_immed(REG_PC, REG_PC, -_pc_rel));

    _pc_rel = 0;
}

struct Opcode {
    uint32_t opcode;

    Opcode(uint32_t o) : opcode(o) {}

    bool illegal(uint32_t mask) { return (opcode & mask) != 0; }

    uint32_t u32() { return opcode; }
    uint32_t i32() { return (int32_t)opcode; }
    constexpr uint32_t u32(int s, int e) {
        return (opcode >> (31 - e)) & ((1 << (1 + e - s)) - 1);
    }
    constexpr uint32_t i32(int s, int e) {
        return (int32_t)((opcode >> (31 - e)) & ((1 << (1 + e - s)) - 1));
    }
    constexpr uint32_t u16(int s, int e) {
        return (uint16_t)((opcode >> (31 - e)) & ((1 << (1 + e - s)) - 1));
    }
    constexpr uint32_t i16(int s, int e) {
        return (int16_t)((opcode >> (31 - e)) & ((1 << (1 + e - s)) - 1));
    }
    constexpr uint32_t u8(int s, int e) {
        return (uint8_t)((opcode >> (31 - e)) & ((1 << (1 + e - s)) - 1));
    }
    constexpr uint32_t i8(int s, int e) {
        return (int8_t)((opcode >> (31 - e)) & ((1 << (1 + e - s)) - 1));
    }
};

/* Utility inlines */
static inline uint32_t getEPOCH()
{
    uint32_t epoch;
    __asm__ volatile("mov %w0, " EPOCH_ASM :"=r"(epoch));
    return epoch;
}

static inline struct PPCState *getHostCTX()
{
    struct PPCState *ctx;
    __asm__ volatile("mov %0, " CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}

/* Map between PPC integer/special purpose registers and AArch64 registers */

#define GPR(n)  (n)
#define FPR(n)  (n)
#define CRn     32
#define XERn    33
#define LRn     34
#define CTRn    35
#define FPSCRn  36
#define PCn     37

// Mapping for fixed PPC registers, all -1 regs are dynamically allocated
constexpr uint8_t INT_REG_MAPPING[] = {
     REG_GPR0,  REG_GPR1,  REG_GPR2,  REG_GPR3,  REG_GPR4,  REG_GPR5,  REG_GPR6,  REG_GPR7, // GPR00 .. GPR07
     REG_GPR8,  REG_GPR9, REG_GPR10, REG_GPR11, REG_GPR12, REG_GPR13,       255,       255, // GPR08 .. GPR15
          255,       255,       255,       255,       255,       255,       255,       255, // GPR16 .. GPR23
          255,       255,       255,       255,       255,       255,       255,       255, // GPR24 .. GPR31
          255,       255,    REG_LR,   REG_CTR,       255,    REG_PC                        // CR, XER, LR, CTR, FPSCR, PC
};

constexpr uint8_t FP_REG_MAPPING[] = {
     REG_FPR0,  REG_FPR1,  REG_FPR2,  REG_FPR3,  REG_FPR4,  REG_FPR5,  REG_FPR6,  REG_FPR7, // FPR00 .. FPR07
     REG_FPR8,  REG_FPR9, REG_FPR10, REG_FPR11, REG_FPR12, REG_FPR13,       255,       255, // FPR08 .. FPR15
          255,       255,       255,       255,       255,       255,       255,       255, // FPR16 .. FPR23
          255,       255,       255,       255,       255, REG_FPR29, REG_FPR30, REG_FPR31, // FPR24 .. FPR31
};

void EMIT_set_crn_logic(PPCTranslatorContext *tc, uint8_t cr);
void EMIT_set_crn_logic_no_minus(PPCTranslatorContext *tc, uint8_t cr);
void EMIT_set_crn_unsigned(PPCTranslatorContext *tc, uint8_t cr);
void EMIT_set_crn_signed(PPCTranslatorContext *tc, uint8_t cr);

int EMIT_lbz(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_lbzx(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_lbzu(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_lbzux(PPCTranslatorContext *tc, uint32_t opcode);

int EMIT_lha(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_lhax(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_lhau(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_lhaux(PPCTranslatorContext *tc, uint32_t opcode);

int EMIT_lhz(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_lhzx(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_lhzu(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_lhzux(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_lhbrx(PPCTranslatorContext *tc, uint32_t opcode);

int EMIT_lwz(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_lwzx(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_lwzu(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_lwzux(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_lwbrx(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_lwarx(PPCTranslatorContext *tc, uint32_t opcode);

int EMIT_stb(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_stbx(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_stbu(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_stbux(PPCTranslatorContext *tc, uint32_t opcode);

int EMIT_sth(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_sthx(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_sthu(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_sthux(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_sthbrx(PPCTranslatorContext *tc, uint32_t opcode);

int EMIT_stw(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_stwx(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_stwu(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_stwux(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_stwcx_dot(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_stwbrx(PPCTranslatorContext *tc, uint32_t opcode);

int EMIT_stfd(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_stfs(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_stfiwx(PPCTranslatorContext *tc, uint32_t opcode);

int EMIT_lfd(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_lfs(PPCTranslatorContext *tc, uint32_t opcode);

int EMIT_fmadd(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_fcmpu(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_fmrx(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_fctiwzx(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_fdivx(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_fmulx(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_faddx(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_fsubx(PPCTranslatorContext *tc, uint32_t opcode);

int EMIT_mftb(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_mtspr(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_mfspr(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_mtmsr(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_mfmsr(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_tw(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_twi(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_sync(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_icbi(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_eieio(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_dcbi(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_dcba(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_dcbz(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_dcbf(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_dcbt(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_dcbtst(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_dcbst(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_sc(PPCTranslatorContext *tc, uint32_t opcode);
int EMIT_rfi(PPCTranslatorContext *tc, uint32_t opcode);

} // Emu68::PPC

#endif /* _PPC_H */
