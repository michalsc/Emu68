/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "support.h"
#include "M68k.h"
#include "RegisterAllocator.h"
#include "cache.h"

static uint32_t EMIT_CMPA(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_CMPA_reg")));
static uint32_t EMIT_CMPA_reg(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_CMPA_mem")));
static uint32_t EMIT_CMPA_mem(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_CMPA_ext")));
static uint32_t EMIT_CMPA_ext(struct TranslatorContext *ctx, uint16_t opcode)
 {
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t size = ((opcode >> 8) & 1) ? 4 : 2;
    uint8_t src = 0xff;
    uint8_t dst = RA_MapM68kRegister(ctx, 8 + ((opcode >> 9) & 7));
    uint8_t ext_words = 0;

    if (size == 4)
        EMIT_LoadFromEffectiveAddress(ctx, size, &src, opcode & 0x3f, &ext_words, 1, NULL);
    else
        EMIT_LoadFromEffectiveAddress(ctx, 0x80 | size, &src, opcode & 0x3f, &ext_words, 0, NULL);

    EMIT(ctx, cmp_reg(dst, src, LSL, 0));

    RA_FreeARMRegister(ctx, src);

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);
        
        if (__builtin_popcount(update_mask) != 0)
        {
            EMIT_GetNZnCV(ctx, cc, &update_mask);
            
            if (update_mask & SR_Z)
                EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                EMIT_SetFlagsConditional(ctx, cc, SR_Valt, ARM_CC_VS);
            if (update_mask & SR_C)
                EMIT_SetFlagsConditional(ctx, cc, SR_Calt, ARM_CC_CC);
        }
    }

    return 1;
}


static uint32_t EMIT_CMPM(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t src = 0xff;
    uint8_t dst = 0xff;
    uint8_t ext_words = 0;
    uint8_t tmp = RA_AllocARMRegister(ctx);

    EMIT_LoadFromEffectiveAddress(ctx, size, &src, 0x18 | (opcode & 7), &ext_words, 1, NULL);
    EMIT_LoadFromEffectiveAddress(ctx, size, &dst, 0x18 | ((opcode >> 9) & 7), &ext_words, 1, NULL);

    switch (size)
    {
        case 4:
            EMIT(ctx, subs_reg(tmp, dst, src, LSL, 0));
            break;
        case 2:
            EMIT(ctx, 
                lsl(tmp, dst, 16),
                subs_reg(tmp, tmp, src, LSL, 16)
            );
            break;
        case 1:
            EMIT(ctx, 
                lsl(tmp, dst, 24),
                subs_reg(tmp, tmp, src, LSL, 24)
            );
            break;
    }

    RA_FreeARMRegister(ctx, tmp);
    RA_FreeARMRegister(ctx, src);
    RA_FreeARMRegister(ctx, dst);

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);
        
        if (__builtin_popcount(update_mask) != 0)
        {
            EMIT_GetNZnCV(ctx, cc, &update_mask);
            
            if (update_mask & SR_Z)
                EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                EMIT_SetFlagsConditional(ctx, cc, SR_Valt, ARM_CC_VS);
            if (update_mask & SR_C)
                EMIT_SetFlagsConditional(ctx, cc, SR_Calt, ARM_CC_CC);
        }
    }

    return 1;
}


static uint32_t EMIT_CMP(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_CMP_reg")));
static uint32_t EMIT_CMP_reg(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_CMP_mem")));
static uint32_t EMIT_CMP_mem(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_CMP_ext")));
static uint32_t EMIT_CMP_ext(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t src = 0xff;
    uint8_t dst = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
    uint8_t ext_words = 0;
    uint8_t tmp = RA_AllocARMRegister(ctx);

    EMIT_LoadFromEffectiveAddress(ctx, size, &src, opcode & 0x3f, &ext_words, 1, NULL);

    switch(size)
    {
        case 4:
            EMIT(ctx, subs_reg(31, dst, src, LSL, 0));
            break;
        case 2:
            EMIT(ctx, 
                lsl(tmp, dst, 16),
                subs_reg(31, tmp, src, LSL, 16)
            );
            break;
        case 1:
            EMIT(ctx, 
                lsl(tmp, dst, 24),
                subs_reg(31, tmp, src, LSL, 24)
            );
            break;
    }

    RA_FreeARMRegister(ctx, tmp);
    RA_FreeARMRegister(ctx, src);

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);

        if (__builtin_popcount(update_mask) != 0)
        {
            EMIT_GetNZnCV(ctx, cc, &update_mask);

            if (update_mask & SR_Z)
                EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                EMIT_SetFlagsConditional(ctx, cc, SR_Valt, ARM_CC_VS);
            if (update_mask & SR_C)
                EMIT_SetFlagsConditional(ctx, cc, SR_Calt, ARM_CC_CC);
        }
    }

    return 1;
}


static uint32_t EMIT_EOR(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_EOR_reg")));
static uint32_t EMIT_EOR_reg(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_EOR_mem")));
static uint32_t EMIT_EOR_mem(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_EOR_ext")));
static uint32_t EMIT_EOR_ext(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t ext_words = 0;
    uint8_t test_register;

    if ((opcode & 0x38) == 0)
    {
        uint8_t src = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
        uint8_t dest = RA_MapM68kRegister(ctx, (opcode) & 7);
        uint8_t tmp = 0xff;

        test_register = dest;

        RA_SetDirtyM68kRegister(ctx, opcode & 7);

        switch (size)
        {
            case 4:
                EMIT(ctx, eor_reg(dest, dest, src, LSL, 0));
                break;
            case 2:
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    eor_reg(tmp, src, dest, LSL, 0),
                    bfi(dest, tmp, 0, 16)
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 1:
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    eor_reg(tmp, src, dest, LSL, 0),
                    bfi(dest, tmp, 0, 8)
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
        }

        RA_FreeARMRegister(ctx, src);
    }
    else
    {
        uint8_t dest = 0xff;
        uint8_t src = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t mode = (opcode & 0x0038) >> 3;

        test_register = tmp;

        if (mode == 4 || mode == 3)
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_words, 0, NULL);
        else
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_words, 1, NULL);

        /* Fetch data into temporary register, perform add, store it back */
        switch (size)
        {
        case 4:
            if (mode == 4)
            {
                EMIT(ctx, ldr_offset_preindex(dest, tmp, -4));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, ldr_offset(dest, tmp, 0));

            /* Perform calcualtion */
            EMIT(ctx, eor_reg(tmp, tmp, src, LSL, 0));

            /* Store back */
            if (mode == 3)
            {
                EMIT(ctx, str_offset_postindex(dest, tmp, 4));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, str_offset(dest, tmp, 0));
            break;
        
        case 2:
            if (mode == 4)
            {
                EMIT(ctx, ldrh_offset_preindex(dest, tmp, -2));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, ldrh_offset(dest, tmp, 0));
            
            /* Perform calcualtion */
            EMIT(ctx, eor_reg(tmp, tmp, src, LSL, 0));

            /* Store back */
            if (mode == 3)
            {
                EMIT(ctx, strh_offset_postindex(dest, tmp, 2));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, strh_offset(dest, tmp, 0));
            break;
        case 1:
            if (mode == 4)
            {
                EMIT(ctx, ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, ldrb_offset(dest, tmp, 0));

            /* Perform calcualtion */
            EMIT(ctx, eor_reg(tmp, tmp, src, LSL, 0));

            /* Store back */
            if (mode == 3)
            {
                EMIT(ctx, strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, strb_offset(dest, tmp, 0));
            break;
        }

        RA_FreeARMRegister(ctx, dest);
    }

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    if (update_mask)
    {
        switch(size)
        {
            case 4:
                EMIT(ctx, cmn_reg(31, test_register, LSL, 0));
                break;
            case 2:
                EMIT(ctx, cmn_reg(31, test_register, LSL, 16));
                break;
            case 1:
                EMIT(ctx, cmn_reg(31, test_register, LSL, 24));
                break;
        }
        
        uint8_t cc = RA_ModifyCC(ctx);
        EMIT_GetNZ00(ctx, cc, &update_mask);

        if (update_mask & SR_Z)
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
    }
    RA_FreeARMRegister(ctx, test_register);

    return 1;
}

static struct OpcodeDef InsnTable[512] = {
    [0000 ... 0007] = { EMIT_CMP_reg, NULL, 0, SR_NZVC, 1, 0, 1 }, //D0 destination, Byte
    [0020 ... 0047] = { EMIT_CMP_mem, NULL, 0, SR_NZVC, 1, 0, 1 }, //(An)
    [0050 ... 0074] = { EMIT_CMP_ext, NULL, 0, SR_NZVC, 1, 1, 1 }, //memory indirect
    [0100 ... 0117] = { EMIT_CMP_reg, NULL, 0, SR_NZVC, 1, 0, 2 }, //register, Word
    [0120 ... 0147] = { EMIT_CMP_mem, NULL, 0, SR_NZVC, 1, 0, 2 }, //(An)
    [0150 ... 0174] = { EMIT_CMP_ext, NULL, 0, SR_NZVC, 1, 1, 2 }, //memory indirect
    [0200 ... 0217] = { EMIT_CMP_reg, NULL, 0, SR_NZVC, 1, 0, 4 }, //register Long
    [0220 ... 0247] = { EMIT_CMP_mem, NULL, 0, SR_NZVC, 1, 0, 4 }, //(An)
    [0250 ... 0274] = { EMIT_CMP_ext, NULL, 0, SR_NZVC, 1, 1, 4 }, //memory indirect

    [0300 ... 0317] = { EMIT_CMPA_reg, NULL, 0, SR_NZVC, 1, 0, 2 }, //A0, Word
    [0320 ... 0347] = { EMIT_CMPA_mem, NULL, 0, SR_NZVC, 1, 0, 2 }, //(An)
    [0350 ... 0374] = { EMIT_CMPA_ext, NULL, 0, SR_NZVC, 1, 1, 2 }, //memory indirect
 
    [0400 ... 0407] = { EMIT_EOR_reg, NULL, 0, SR_NZVC, 1, 0, 1 }, //D0, Byte
    [0410 ... 0417] = { EMIT_CMPM, NULL, 0, SR_NZVC, 1, 0, 1 },
    [0420 ... 0447] = { EMIT_EOR_mem, NULL, 0, SR_NZVC, 1, 0, 1 },
    [0450 ... 0471] = { EMIT_EOR_ext, NULL, 0, SR_NZVC, 1, 1, 1 },
    [0500 ... 0507] = { EMIT_EOR_reg, NULL, 0, SR_NZVC, 1, 0, 2 }, //D0, Word
    [0510 ... 0517] = { EMIT_CMPM, NULL, 0, SR_NZVC, 1, 0, 2 },
    [0520 ... 0547] = { EMIT_EOR_mem, NULL, 0, SR_NZVC, 1, 0, 2 },
    [0550 ... 0571] = { EMIT_EOR_ext, NULL, 0, SR_NZVC, 1, 1, 2 },
    [0600 ... 0607] = { EMIT_EOR_reg, NULL, 0, SR_NZVC, 1, 0, 4 }, //D0, Long
    [0610 ... 0617] = { EMIT_CMPM, NULL, 0, SR_NZVC, 1, 0, 4 },
    [0620 ... 0647] = { EMIT_EOR_mem, NULL, 0, SR_NZVC, 1, 0, 4 },
    [0650 ... 0671] = { EMIT_EOR_ext, NULL, 0, SR_NZVC, 1, 1, 4 },

    [0700 ... 0717] = { EMIT_CMPA_reg, NULL, 0, SR_NZVC, 1, 0, 4 }, //A0, Long
    [0720 ... 0747] = { EMIT_CMPA_mem, NULL, 0, SR_NZVC, 1, 0, 4 }, //(An)
    [0750 ... 0774] = { EMIT_CMPA_ext, NULL, 0, SR_NZVC, 1, 1, 4 }, //memory indirect
};

uint32_t EMIT_lineB(struct TranslatorContext *ctx)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr++);

    /* 1011xxxx11xxxxxx - CMPA */
    if (InsnTable[opcode & 00777].od_Emit)
    {
        return InsnTable[opcode & 00777].od_Emit(ctx, opcode);
    }
    else
    {
        EMIT_FlushPC(ctx);
        EMIT_InjectDebugString(ctx, "[JIT] opcode %04x at %08x not implemented\n", opcode, ctx->tc_M68kCodePtr - 1);
        EMIT(ctx, 
            svc(0x100),
            svc(0x101),
            svc(0x103),
            (uint32_t)(uintptr_t)(ctx->tc_M68kCodePtr - 8),
            48
        );
        EMIT_Exception(ctx, VECTOR_ILLEGAL_INSTRUCTION, 0);
        EMIT(ctx, INSN_TO_LE(0xffffffff));
    }

    return 1;
}

uint32_t GetSR_LineB(uint16_t opcode)
{
    /* If instruction is in the table, return what flags it needs (shifted 16 bits left) and flags it sets */
    if (InsnTable[opcode & 00777].od_Emit) {
        return (InsnTable[opcode & 00777].od_SRNeeds << 16) | InsnTable[opcode & 00777].od_SRSets;
    }
    /* Instruction not found, i.e. it needs all flags and sets none (ILLEGAL INSTRUCTION exception) */
    else {
        kprintf("Undefined LineB\n");
        return SR_CCR << 16;
    }
}


int M68K_GetLineBLength(uint16_t *insn_stream)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)insn_stream);
    
    int length = 0;
    int need_ea = 0;
    int opsize = 0;

    if (InsnTable[opcode & 00777].od_Emit) {
        length = InsnTable[opcode & 00777].od_BaseLength;
        need_ea = InsnTable[opcode & 00777].od_HasEA;
        opsize = InsnTable[opcode & 00777].od_OpSize;
    }

    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize);
    }

    return length;
}