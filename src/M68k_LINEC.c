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

uint32_t EMIT_MULU(struct TranslatorContext *ctx, uint16_t opcode);
uint32_t EMIT_MULS(struct TranslatorContext *ctx, uint16_t opcode);

uint32_t EMIT_AND(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_AND_reg")));
static uint32_t EMIT_AND_reg(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_AND_mem")));
static uint32_t EMIT_AND_mem(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_AND_ext")));
static uint32_t EMIT_AND_ext(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t ext_words = 0;
    uint8_t test_register = 0xff;

    if (direction == 0)
    {
        uint8_t dest = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
        uint8_t src = 0xff;
        uint8_t tmp_reg = RA_AllocARMRegister(ctx);

        test_register = dest;

        RA_SetDirtyM68kRegister(ctx, (opcode >> 9) & 7);
        
        EMIT_LoadFromEffectiveAddress(ctx, size, &src, opcode & 0x3f, &ext_words, 1, NULL);

        switch (size)
        {
            case 4:
                EMIT(ctx,
                    update_mask ? 
                        ands_reg(dest, dest, src, LSL, 0) : 
                        and_reg(dest, dest, src, LSL, 0)
                );
                break;
            case 2:
                EMIT(ctx, 
                    and_reg(tmp_reg, src, dest, LSL, 0),
                    bfi(dest, tmp_reg, 0, 16)
                );
                break;
            case 1:
                EMIT(ctx, 
                    and_reg(tmp_reg, src, dest, LSL, 0),
                    bfi(dest, tmp_reg, 0, 8)
                );
                break;
        }

        RA_FreeARMRegister(ctx, src);
        RA_FreeARMRegister(ctx, tmp_reg);
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
                EMIT(ctx, 
                    update_mask ? 
                        ands_reg(tmp, tmp, src, LSL, 0) : 
                        and_reg(tmp, tmp, src, LSL, 0)
                );

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
                EMIT(ctx, and_reg(tmp, tmp, src, LSL, 0));

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
                EMIT(ctx, and_reg(tmp, tmp, src, LSL, 0));

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

uint32_t EMIT_EXG(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t reg1 = 0xff;
    uint8_t reg2 = 0xff;
    uint8_t tmp = RA_AllocARMRegister(ctx);

    switch ((opcode >> 3) & 0x1f)
    {
        case 0x08: /* Data registers */
            reg1 = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
            reg2 = RA_MapM68kRegister(ctx, opcode & 7);
            EMIT(ctx, 
                mov_reg(tmp, reg1),
                mov_reg(reg1, reg2),
                mov_reg(reg2, tmp)
            );
            RA_SetDirtyM68kRegister(ctx, (opcode >> 9) & 7);
            RA_SetDirtyM68kRegister(ctx, opcode & 7);
            break;

        case 0x09:
            reg1 = RA_MapM68kRegister(ctx, 8 + ((opcode >> 9) & 7));
            reg2 = RA_MapM68kRegister(ctx, 8 + (opcode & 7));
            EMIT(ctx, 
                mov_reg(tmp, reg1),
                mov_reg(reg1, reg2),
                mov_reg(reg2, tmp)
            );
            RA_SetDirtyM68kRegister(ctx, 8 + ((opcode >> 9) & 7));
            RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            break;

        case 0x11:
            reg1 = RA_MapM68kRegister(ctx, ((opcode >> 9) & 7));
            reg2 = RA_MapM68kRegister(ctx, 8 + (opcode & 7));
            EMIT(ctx, 
                mov_reg(tmp, reg1),
                mov_reg(reg1, reg2),
                mov_reg(reg2, tmp)
            );
            RA_SetDirtyM68kRegister(ctx, ((opcode >> 9) & 7));
            RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            break;
    }

    EMIT_AdvancePC(ctx, 2);

    RA_FreeARMRegister(ctx, tmp);

    return 1;
}

static uint32_t EMIT_ABCD_reg(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t src = RA_MapM68kRegister(ctx, opcode & 7);
    uint8_t dst = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
    uint8_t cc = RA_ModifyCC(ctx);

    uint8_t tmp_a = RA_AllocARMRegister(ctx);
    uint8_t tmp_b = RA_AllocARMRegister(ctx);
    uint8_t tmp_c = RA_AllocARMRegister(ctx);
    uint8_t tmp_d = RA_AllocARMRegister(ctx);

    RA_SetDirtyM68kRegister(ctx, (opcode >> 9) & 7);

    EMIT(ctx, 
        // Mask higher and lower nibbles, perform calculation. 
        and_immed(tmp_a, src, 4, 0),
        and_immed(tmp_b, dst, 4, 0),

        and_immed(tmp_c, src, 4, 28),
        and_immed(tmp_d, dst, 4, 28),

        // Perform calculations, separate for high and low nibbles
        add_reg(tmp_a, tmp_a, tmp_b, LSL, 0),
        add_reg(tmp_c, tmp_c, tmp_d, LSL, 0),

        // Add X
        tst_immed(cc, 1, 31 & (32 - SRB_X)),
        csinc(tmp_a, tmp_a, tmp_a, A64_CC_EQ),

        // Compute sum of high and low nibbles into tmp_b
        add_reg(tmp_b, tmp_a, tmp_c, LSL, 0),

        // if lower nibble higher than 9 make radix adjustment to result
        cmp_immed(tmp_a, 9),
        b_cc(A64_CC_LS, 2),
        add_immed(tmp_b, tmp_b, 6),

        // Mask higher nibble of result and detect carry
        and_immed(tmp_d, tmp_b, 6, 28),
        cmp_immed(tmp_d, 0x90)
    );

    if (update_mask & SR_XC)
    {
        EMIT(ctx, 
            bic_immed(cc, cc, 1, 32 - SRB_Calt),
            b_cc(A64_CC_LS, 3),
            orr_immed(cc, cc, 1, 32 - SRB_Calt)
        );
    }
    else
    {
        EMIT(ctx, b_cc(A64_CC_LS, 2));
    }
    // Make radix adjustment of upper nibble
    EMIT(ctx, add_immed(tmp_b, tmp_b, 0x60));
    if (update_mask & SR_X)
    {
        // Copy C flag to X
        EMIT(ctx, 
            ror(0, cc, 1),
            bfi(cc, 0, 4, 1)
        );
    }
    
    // Insert into result
    EMIT(ctx, bfi(dst, tmp_b, 0, 8));

    if (update_mask & SR_Z)
    {
        // Z flag updating. AND the result and clear Z if non-zero, leave unchanged otherwise
        EMIT(ctx, 
            ands_immed(31, tmp_b, 8, 0),
            b_cc(A64_CC_EQ, 2),
            bic_immed(cc, cc, 1, 32 - SRB_Z)
        );
    }

    RA_FreeARMRegister(ctx, tmp_a);
    RA_FreeARMRegister(ctx, tmp_b);
    RA_FreeARMRegister(ctx, tmp_c);
    RA_FreeARMRegister(ctx, tmp_d);
    RA_FreeARMRegister(ctx, src);
    RA_FreeARMRegister(ctx, dst);
    EMIT_AdvancePC(ctx, 2);

    return 1;
}

static uint32_t EMIT_ABCD_mem(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t cc = RA_ModifyCC(ctx);

    uint8_t tmp_a = RA_AllocARMRegister(ctx);
    uint8_t tmp_b = RA_AllocARMRegister(ctx);
    uint8_t tmp_c = RA_AllocARMRegister(ctx);
    uint8_t tmp_d = RA_AllocARMRegister(ctx);

    uint8_t an_src = RA_MapM68kRegister(ctx, 8 + (opcode & 7));
    uint8_t an_dst = RA_MapM68kRegister(ctx, 8 + ((opcode >> 9) & 7));

    // Fetch initial data into regs tmp_a and tmp_b
    if ((opcode & 7) == 7) {
        EMIT(ctx, ldrb_offset_preindex(an_src, tmp_a, -2));
    }
    else {
        EMIT(ctx, ldrb_offset_preindex(an_src, tmp_a, -1));
    }

    if (((opcode >> 9) & 7) == 7) {
        EMIT(ctx, ldrb_offset_preindex(an_dst, tmp_b, -2));
    }
    else {
        EMIT(ctx, ldrb_offset_preindex(an_dst, tmp_b, -1));
    }

    RA_SetDirtyM68kRegister(ctx, 8 + ((opcode >> 9) & 7));
    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));

    EMIT(ctx, 
        // Mask higher and lower nibbles, perform calculation. 
        and_immed(tmp_c, tmp_a, 4, 28),
        and_immed(tmp_d, tmp_b, 4, 28),
        and_immed(tmp_a, tmp_a, 4, 0),
        and_immed(tmp_b, tmp_b, 4, 0),

        // Perform calculations, separate for high and low nibbles
        add_reg(tmp_a, tmp_a, tmp_b, LSL, 0),
        add_reg(tmp_c, tmp_c, tmp_d, LSL, 0),

        // Add X
        tst_immed(cc, 1, 31 & (32 - SRB_X)),
        csinc(tmp_a, tmp_a, tmp_a, A64_CC_EQ),

        // Compute sum of high and low nibbles into tmp_b
        add_reg(tmp_b, tmp_a, tmp_c, LSL, 0),

        // if lower nibble higher than 9 make radix adjustment to result
        cmp_immed(tmp_a, 9),
        b_cc(A64_CC_LS, 2),
        add_immed(tmp_b, tmp_b, 6),

        // Mask higher nibble of result and detect carry
        and_immed(tmp_d, tmp_b, 6, 28),
        cmp_immed(tmp_d, 0x90)
    );

    if (update_mask & SR_XC)
    {
        EMIT(ctx, 
            bic_immed(cc, cc, 1, 32 - SRB_Calt),
            b_cc(A64_CC_LS, 3),
            orr_immed(cc, cc, 1, 32 - SRB_Calt)
        );
    }
    else
    {
        EMIT(ctx, b_cc(A64_CC_LS, 2));
    }
    // Make radix adjustment of upper nibble
    EMIT(ctx, add_immed(tmp_b, tmp_b, 0x60));
    if (update_mask & SR_X)
    {
        // Copy C flag to X
        EMIT(ctx, 
            ror(0, cc, 1),
            bfi(cc, 0, 4, 1)
        );
    }
    
    // Insert into result
    EMIT(ctx, strb_offset(an_dst, tmp_b, 0));

    if (update_mask & SR_Z)
    {
        // Z flag updating. AND the result and clear Z if non-zero, leave unchanged otherwise
        EMIT(ctx, 
            ands_immed(31, tmp_b, 8, 0),
            b_cc(A64_CC_EQ, 2),
            bic_immed(cc, cc, 1, 32 - SRB_Z)
        );
    }

    RA_FreeARMRegister(ctx, tmp_a);
    RA_FreeARMRegister(ctx, tmp_b);
    RA_FreeARMRegister(ctx, tmp_c);
    RA_FreeARMRegister(ctx, tmp_d);
    
    EMIT_AdvancePC(ctx, 2);

    return 1;
}

static struct OpcodeDef InsnTable[512] = {
    [0000 ... 0007] = { EMIT_AND_reg, NULL, 0, SR_NZVC, 1, 0, 1 },  //D0 Destination, Byte
    [0020 ... 0047] = { EMIT_AND_mem, NULL, 0, SR_NZVC, 1, 0, 1 },
    [0050 ... 0074] = { EMIT_AND_ext, NULL, 0, SR_NZVC, 1, 1, 1 },
    [0100 ... 0107] = { EMIT_AND_reg, NULL, 0, SR_NZVC, 1, 0, 2 }, //Word
    [0120 ... 0147] = { EMIT_AND_mem, NULL, 0, SR_NZVC, 1, 0, 2 },
    [0150 ... 0174] = { EMIT_AND_ext, NULL, 0, SR_NZVC, 1, 1, 2 },
    [0200 ... 0207] = { EMIT_AND_reg, NULL, 0, SR_NZVC, 1, 0, 4 }, //Long
    [0220 ... 0247] = { EMIT_AND_mem, NULL, 0, SR_NZVC, 1, 0, 4 },
    [0250 ... 0274] = { EMIT_AND_ext, NULL, 0, SR_NZVC, 1, 1, 4 },
 
    [0300 ... 0307] = { EMIT_MULU, NULL, 0, SR_NZVC, 1, 0, 2}, //_reg, //D0 Destination
    [0320 ... 0347] = { EMIT_MULU, NULL, 0, SR_NZVC, 1, 0, 2 }, //_mem,
    [0350 ... 0374] = { EMIT_MULU, NULL, 0, SR_NZVC, 1, 1, 2 }, //_ext,
 
    [0400 ... 0407] = { EMIT_ABCD_reg, NULL, SR_XZ, SR_XZC, 1, 0, 1 }, //D0 Destination
    [0410 ... 0417] = { EMIT_ABCD_mem, NULL, SR_XZ, SR_XZC, 1, 0, 1 }, //-Ax),-(Ay)
    [0420 ... 0447] = { EMIT_AND_mem, NULL, 0, SR_NZVC, 1, 0, 1 }, //Byte
    [0450 ... 0471] = { EMIT_AND_ext, NULL, 0, SR_NZVC, 1, 1, 1 }, //D0 Source
 
    [0500 ... 0517] = { EMIT_EXG, NULL, 0, 0, 1, 0, 4 }, //R0 Source, unsized always the full register
    [0520 ... 0547] = { EMIT_AND_mem, NULL, 0, SR_NZVC, 1, 0, 2 }, //Word
    [0550 ... 0571] = { EMIT_AND_ext, NULL, 0, SR_NZVC, 1, 1, 2 }, 

    [0610 ... 0617] = { EMIT_EXG, NULL, 0, 0, 1, 0, 4 },  //D0 Source
    [0620 ... 0647] = { EMIT_AND_mem, NULL, 0, SR_NZVC, 1, 0, 4 }, //Long
    [0650 ... 0671] = { EMIT_AND_ext, NULL, 0, SR_NZVC, 1, 1, 4 },

    [0700 ... 0707] = { EMIT_MULS, NULL, 0, SR_NZVC, 1, 0, 2 }, //_reg, //D0 Destination
    [0720 ... 0747] = { EMIT_MULS, NULL, 0, SR_NZVC, 1, 0, 2 }, //_mem,
    [0750 ... 0774] = { EMIT_MULS, NULL, 0, SR_NZVC, 1, 1, 2 }, //_ext,
};


uint32_t EMIT_lineC(struct TranslatorContext *ctx)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr++);

    /* 1100xxx011xxxxxx - MULU */
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

uint32_t GetSR_LineC(uint16_t opcode)
{
    /* If instruction is in the table, return what flags it needs (shifted 16 bits left) and flags it sets */
    if (InsnTable[opcode & 00777].od_Emit) {
        return (InsnTable[opcode & 00777].od_SRNeeds << 16) | InsnTable[opcode & 00777].od_SRSets;
    }
    /* Instruction not found, i.e. it needs all flags and sets none (ILLEGAL INSTRUCTION exception) */
    else {
        kprintf("Undefined LineC\n");
        return SR_CCR << 16;
    }
}


int M68K_GetLineCLength(uint16_t *insn_stream)
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
