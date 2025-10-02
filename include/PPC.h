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

#include "nodes.h"
#include "md5.h"
#include "lists.h"

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
    union {
        double FPR[32];
        uint64_t FPR_u64[32];
    };
    uint32_t FPSCR;

    /* VEA */
    uint64_t TB_offset;

    /* Async IRQ part */
    union {
        struct {
            uint8_t ARM;
            uint8_t ARM_err;
            uint8_t EXT;
            uint8_t RESET;
        } INT;
        uint32_t INT32;
    };

    /* OEA */
    uint64_t INSN_COUNT;
    uint32_t JIT_CACHE_MISS;
    uint32_t JIT_UNIT_COUNT;
    uint32_t JIT_CACHE_TOTAL;
    uint32_t JIT_CACHE_FREE;
    uint32_t JIT_SOFTFLUSH_THRESH;
    uint32_t JIT_CONTROL;
    uint32_t JIT_CONTROL2;
};

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

#define REG_FPSCR_VN 19
#define REG_FPSCR_SIZE TS_S
#define REG_FPSCR_POS 0
#define REG_FPSCR_ASM "v19.s[0]"

#define REG_FPSCR    REG_FPSCR_VN,REG_FPSCR_SIZE,REG_FPSCR_POS

#define CTX_LAST_PC_VN 19
#define CTX_LAST_PC_SIZE TS_S
#define CTX_LAST_PC_POS 3
#define CTX_LAST_PC_ASM "v19.s[3]"

#define CTX_POINTER_VN 20
#define CTX_POINTER_SIZE TS_D
#define CTX_POINTER_POS 1
#define CTX_POINTER_ASM "v20.d[1]"

#define CTX_INSN_COUNT_VN 20
#define CTX_INSN_COUNT_SIZE TS_D
#define CTX_INSN_COUNT_POS 0
#define CTX_INSN_COUNT_ASM "v20.d[0]"

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

struct PPCTranslationUnit
{
    /* Hot part of the structure shall preferably reside in one or at most two cache lines */
    struct Node         ptu_HashNode;       /* 00: 2 x 8 bytes - prev and next pointer in the has table bucket */
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
    struct Node         ptu_LRUNode;        /* 48: 2 x 8 bytes - LRU node */

    /* Cold part of the structure */
    uint32_t            ptu_PPCInsnCnt;
    uint32_t            ptu_ARMInsnCnt;
    #if 0
    uint32_t        mt_PrologueSize;
    uint32_t        mt_EpilogueSize;
    uint32_t        mt_Conditionals;
    uint64_t        mt_UseCount;
    uint64_t        mt_FetchCount;
    #endif
    struct PPCLocalState * ptu_LocalState;

    uint32_t            ptu_ARMCode[] __attribute__((aligned(64)));
};

#endif /* _PPC_H */
