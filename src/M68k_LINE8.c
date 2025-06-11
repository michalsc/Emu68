/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "A64.h"
#include "support.h"
#include "M68k.h"
#include "RegisterAllocator.h"
#include "cache.h"

#define EMIT_DIVU_reg EMIT_MUL_DIV
#define EMIT_DIVU_mem EMIT_MUL_DIV
#define EMIT_DIVU_ext EMIT_MUL_DIV
#define EMIT_DIVS_reg EMIT_MUL_DIV
#define EMIT_DIVS_mem EMIT_MUL_DIV
#define EMIT_DIVS_ext EMIT_MUL_DIV

uint32_t EMIT_PACK_mem(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_PACK_reg")));
uint32_t EMIT_PACK_reg(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint16_t addend = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t tmp = -1;

    if (opcode & 8)
    {
        uint8_t an_src = RA_MapM68kRegister(ctx, 8 + (opcode & 7));
        tmp = RA_AllocARMRegister(ctx);
        
        EMIT(ctx, ldrsh_offset_preindex(an_src, tmp, -2));

        RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
    }
    else
    {
        tmp = RA_CopyFromM68kRegister(ctx, opcode & 7);
    }

    if (addend & 0xfff) {
        EMIT(ctx, add_immed(tmp, tmp, addend & 0xfff));
    }
    if (addend & 0xf000) {
        EMIT(ctx,  add_immed_lsl12(tmp, tmp, addend >> 12));
    }

    EMIT(ctx, bfi(tmp, tmp, 4, 4));

    if (opcode & 8)
    {
        uint8_t dst = RA_MapM68kRegister(ctx, 8 + ((opcode >> 9) & 7));
        RA_SetDirtyM68kRegister(ctx, 8 + ((opcode >> 9) & 7));

        EMIT(ctx, lsr(tmp, tmp, 4));
        if (((opcode >> 9) & 7) == 7) {
            EMIT(ctx, strb_offset_preindex(dst, tmp, -2));
        }
        else {
            EMIT(ctx, strb_offset_preindex(dst, tmp, -1));
        }
    }
    else
    {
        uint8_t dst = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
        RA_SetDirtyM68kRegister(ctx, (opcode >> 9) & 7);
        EMIT(ctx, bfxil(dst, tmp, 4, 8));
    }

    ctx->tc_M68kCodePtr++;
    EMIT_AdvancePC(ctx, 4);

    RA_FreeARMRegister(ctx, tmp);

    return 1;
}

uint32_t EMIT_UNPK_mem(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_UNPK_reg")));
uint32_t EMIT_UNPK_reg(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint16_t addend = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t tmp = -1;

    if (opcode & 8)
    {
        uint8_t an_src = RA_MapM68kRegister(ctx, 8 + (opcode & 7));
        tmp = RA_AllocARMRegister(ctx);

        if ((opcode & 7) == 7) {
            EMIT(ctx, ldrb_offset_preindex(an_src, tmp, -2));
        }
        else {
            EMIT(ctx, ldrb_offset_preindex(an_src, tmp, -1));
        }

        RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
    }
    else
    {
        uint8_t src_reg = RA_MapM68kRegister(ctx, opcode & 7);
        tmp = RA_AllocARMRegister(ctx);
        EMIT(ctx, and_immed(tmp, src_reg, 8, 0));
    }

    EMIT(ctx, 
        orr_reg(tmp, tmp, tmp, LSL, 4),
        and_immed(tmp, tmp, 28, 24)
    );

    if (addend & 0xfff) {
        EMIT(ctx, add_immed(tmp, tmp, addend & 0xfff));
    }
    if (addend & 0xf000) {
        EMIT(ctx, add_immed_lsl12(tmp, tmp, addend >> 12));
    }

    if (opcode & 8)
    {
        uint8_t dst = RA_MapM68kRegister(ctx, 8 + ((opcode >> 9) & 7));
        RA_SetDirtyM68kRegister(ctx, 8 + ((opcode >> 9) & 7));
        EMIT(ctx, strh_offset_preindex(dst, tmp, -2));
    }
    else
    {
        uint8_t dst = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
        RA_SetDirtyM68kRegister(ctx, (opcode >> 9) & 7);
        EMIT(ctx, bfi(dst, tmp, 0, 16));
    }

    ctx->tc_M68kCodePtr++;
    EMIT_AdvancePC(ctx, 4);

    RA_FreeARMRegister(ctx, tmp);

    return 1;
}

uint32_t EMIT_OR_ext(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_OR_reg")));
uint32_t EMIT_OR_mem(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_OR_reg")));
uint32_t EMIT_OR_reg(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t direction = (opcode >> 8) & 1; // 0: Ea+Dn->Dn, 1: Ea+Dn->Ea
    uint8_t ext_words = 0;
    uint8_t test_register = 0xff;

    if (direction == 0)
    {
        uint8_t dest = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
        uint8_t src = 0xff;
        uint8_t temp_reg = RA_AllocARMRegister(ctx);

        test_register = dest;

        RA_SetDirtyM68kRegister(ctx, (opcode >> 9) & 7);
        EMIT_LoadFromEffectiveAddress(ctx, size, &src, opcode & 0x3f, &ext_words, 1, NULL);
        
        switch (size)
        {
            case 4:
                EMIT(ctx, orr_reg(dest, dest, src, LSL, 0));
                break;
            case 2:
                EMIT(ctx, 
                    orr_reg(temp_reg, src, dest, LSL, 0),
                    bfi(dest, temp_reg, 0, 16)
                );
                break;
            case 1:
                EMIT(ctx, 
                    orr_reg(temp_reg, src, dest, LSL, 0),
                    bfi(dest, temp_reg, 0, 8)
                );
                break;
        }

        RA_FreeARMRegister(ctx, temp_reg);
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
            EMIT(ctx, orr_reg(tmp, tmp, src, LSL, 0));

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
            EMIT(ctx, orr_reg(tmp, tmp, src, LSL, 0));

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
            EMIT(ctx, orr_reg(tmp, tmp, src, LSL, 0));

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

// BROKEN!!!!
uint32_t EMIT_SBCD_reg(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);

    uint8_t src = RA_MapM68kRegister(ctx, opcode & 7);
    uint8_t dest = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);

    uint8_t tmp_a = RA_AllocARMRegister(ctx);
    uint8_t tmp_b = RA_AllocARMRegister(ctx);
    uint8_t tmp_c = RA_AllocARMRegister(ctx);
    uint8_t tmp_d = RA_AllocARMRegister(ctx);
    uint8_t tmp_n = RA_AllocARMRegister(ctx);
    uint8_t cc = RA_ModifyCC(ctx);

kprintf("[ERROR] SBCD is not yet fixed!!!\n");

    EMIT(ctx, 
    /* Extract dest into further temp register (used to check overflow and flags) */
        and_immed(tmp_n, dest, 8, 0),

        and_immed(tmp_a, src, 4, 0),   // Fetch low nibbles
        and_immed(tmp_b, dest, 4, 0),

        and_immed(tmp_c, src, 4, 28),  // Fetch high nibbles
        and_immed(tmp_d, dest, 4, 28),

        // Test X flag
        tst_immed(cc, 1, 31 & (32 - SRB_X)),   // Sub X

        // Subtract nibbles
        sub_reg(tmp_a, tmp_b, tmp_a, LSL, 0),
        sub_reg(tmp_c, tmp_d, tmp_c, LSL, 0),
        
        // Extract 8-bit src into tmp_b and perform subtraction on tmp_n test reg
        and_immed(tmp_b, src, 8, 0),
        sub_reg(tmp_n, tmp_n, tmp_b, LSL, 0),

        // if X was set (NE), decrease lower nibble result by one
        b_cc(A64_CC_EQ, 3),
        sub_immed(tmp_a, tmp_a, 1),
        sub_immed(tmp_n, tmp_n, 1)
    );

    if (update_mask & SR_XC)
    {
        EMIT(ctx, mov_reg(tmp_d, tmp_n));
    }

    // Join nibbles together in tmp_b register
    EMIT(ctx, add_reg(tmp_b, tmp_a, tmp_c, LSL, 0));

    // If lower libble overflowed, do radix correction
    EMIT(ctx, ands_immed(31, tmp_a, 4, 28));

    if (update_mask & SR_XC) {
        EMIT(ctx, 
            b_cc(A64_CC_EQ, 3),
            sub_immed(tmp_b, tmp_b, 6),
            sub_immed(tmp_d, tmp_d, 6)
        );
    }
    else {
        EMIT(ctx, 
            b_cc(A64_CC_EQ, 2),
            sub_immed(tmp_b, tmp_b, 6)
        );
    }


    // Check if result overflowed
    EMIT(ctx, 
        ands_immed(31, tmp_n, 1, 24),
        b_cc(A64_CC_EQ, 2),
        sub_immed(tmp_b, tmp_b, 0x60)
    );

    if (update_mask & SR_XC) {
        EMIT(ctx, 
            bic_immed(cc, cc, 1, 31),
            ands_immed(31, tmp_d, 2, 24),
            b_cc(A64_CC_EQ, 2),
            orr_immed(cc, cc, 1, 31)
        );

        if (update_mask & SR_X) {
            EMIT(ctx, 
                ror(0, cc, 1),
                bfi(cc, 0, 4, 1)
            );
        }
    }

    // Insert result into target register
    EMIT(ctx, bfi(dest, tmp_b, 0, 8));

    if (update_mask & SR_Z) {
        EMIT(ctx, 
            ands_immed(31, tmp_b, 8, 0),
            b_cc(A64_CC_EQ, 2),
            bic_immed(cc, cc, 1, 31 & (32 - SRB_Z))
        );
    }

    RA_FreeARMRegister(ctx, tmp_a);
    RA_FreeARMRegister(ctx, tmp_b);
    RA_FreeARMRegister(ctx, tmp_c);
    RA_FreeARMRegister(ctx, tmp_d);
    RA_FreeARMRegister(ctx, tmp_n);

    EMIT_AdvancePC(ctx, 2);

    return 1;
}

uint32_t EMIT_SBCD_mem(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t tmp_a = RA_AllocARMRegister(ctx);
    uint8_t tmp_b = RA_AllocARMRegister(ctx);
    uint8_t tmp_c = RA_AllocARMRegister(ctx);
    uint8_t tmp_d = RA_AllocARMRegister(ctx);
    uint8_t tmp_n = RA_AllocARMRegister(ctx);
    uint8_t src = RA_AllocARMRegister(ctx);
    uint8_t cc = RA_ModifyCC(ctx);

kprintf("[ERROR] SBCD mem is not yet fixed!\n");

    uint8_t an_src = RA_MapM68kRegister(ctx, 8 + (opcode & 7));
    uint8_t an_dest = RA_MapM68kRegister(ctx, 8 + ((opcode >> 9) & 7));

    /* predecremented address, special case if SP */
    if ((opcode & 7) == 7)
        EMIT(ctx, ldrb_offset_preindex(an_src, src, -2));
    else
        EMIT(ctx, ldrb_offset_preindex(an_src, src, -1));
    if (((opcode >> 9) & 7) == 7)
        EMIT(ctx, ldrb_offset_preindex(an_dest, tmp_n, -2));
    else
        EMIT(ctx, ldrb_offset_preindex(an_dest, tmp_n, -1));

    EMIT(ctx, 
        and_immed(tmp_a, src, 4, 0),   // Fetch low nibbles
        and_immed(tmp_b, tmp_n, 4, 0),

        and_immed(tmp_c, src, 4, 28),  // Fetch high nibbles
        and_immed(tmp_d, tmp_n, 4, 28),

        // Test X flag
        tst_immed(cc, 1, 31 & (32 - SRB_X)),   // Sub X

        // Subtract nibbles
        sub_reg(tmp_a, tmp_b, tmp_a, LSL, 0),
        sub_reg(tmp_c, tmp_d, tmp_c, LSL, 0),
        
        // Perform subtraction on tmp_n test reg
        sub_reg(tmp_n, tmp_n, src, LSL, 0),

        // if X was set (NE), decrease lower nibble result by one
        b_cc(A64_CC_EQ, 3),
        sub_immed(tmp_a, tmp_a, 1),
        sub_immed(tmp_n, tmp_n, 1)
    );

    if (update_mask & SR_XC)
    {
        EMIT(ctx, mov_reg(tmp_d, tmp_n));
    }

    EMIT(ctx, 
        // Join nibbles together in tmp_b register
        add_reg(tmp_b, tmp_a, tmp_c, LSL, 0),

        // If lower libble overflowed, do radix correction
        ands_immed(31, tmp_a, 4, 28)
    );

    if (update_mask & SR_XC) {
        EMIT(ctx, 
            b_cc(A64_CC_EQ, 3),
            sub_immed(tmp_b, tmp_b, 6),
            sub_immed(tmp_d, tmp_d, 6)
        );
    }
    else {
        EMIT(ctx, 
            b_cc(A64_CC_EQ, 2),
            sub_immed(tmp_b, tmp_b, 6)
        );
    }

    // Check if result overflowed
    EMIT(ctx, 
        ands_immed(31, tmp_n, 1, 24),
        b_cc(A64_CC_EQ, 2),
        sub_immed(tmp_b, tmp_b, 0x60)
    );

    if (update_mask & SR_XC) {
        EMIT(ctx, 
            bic_immed(cc, cc, 1, 31),
            ands_immed(31, tmp_d, 2, 24),
            b_cc(A64_CC_EQ, 2),
            orr_immed(cc, cc, 1, 31)
        );

        if (update_mask & SR_X) {
            EMIT(ctx, 
                ror(0, cc, 1),
                bfi(cc, 0, 4, 1)
            );
        }
    }

    // Insert result into target register
    EMIT(ctx, strb_offset(an_dest, tmp_b, 0));

    if (update_mask & SR_Z) {
        EMIT(ctx, 
            ands_immed(31, tmp_b, 8, 0),
            b_cc(A64_CC_EQ, 2),
            bic_immed(cc, cc, 1, 31 & (32 - SRB_Z))
        );
    }

    RA_FreeARMRegister(ctx, tmp_a);
    RA_FreeARMRegister(ctx, tmp_b);
    RA_FreeARMRegister(ctx, tmp_c);
    RA_FreeARMRegister(ctx, tmp_d);
    RA_FreeARMRegister(ctx, tmp_n);
    RA_FreeARMRegister(ctx, src);

    EMIT_AdvancePC(ctx, 2);

    return 1;
}

static struct OpcodeDef InsnTable[512] = {
    [0000 ... 0007] = { EMIT_OR_reg, NULL, 0, SR_NZVC, 1, 0, 1 },    //D0 Destination
    [0020 ... 0047] = { EMIT_OR_mem, NULL, 0, SR_NZVC, 1, 0, 1 },
    [0050 ... 0074] = { EMIT_OR_ext, NULL, 0, SR_NZVC, 1, 1, 1 },
    [0100 ... 0107] = { EMIT_OR_reg, NULL, 0, SR_NZVC, 1, 0, 2 },
    [0120 ... 0147] = { EMIT_OR_mem, NULL, 0, SR_NZVC, 1, 0, 2 },
    [0150 ... 0174] = { EMIT_OR_ext, NULL, 0, SR_NZVC, 1, 1, 2 },
    [0200 ... 0207] = { EMIT_OR_reg, NULL, 0, SR_NZVC, 1, 0, 4 },
    [0220 ... 0247] = { EMIT_OR_mem, NULL, 0, SR_NZVC, 1, 0, 4 },
    [0250 ... 0274] = { EMIT_OR_ext, NULL, 0, SR_NZVC, 1, 1, 4 },
 
    [0300 ... 0307] = { EMIT_DIVU_reg, NULL, 0, SR_NZVC, 1, 0, 2 },  //D0 Destination, DIVU.W
    [0320 ... 0347] = { EMIT_DIVU_mem, NULL, 0, SR_NZVC, 1, 0, 2 },
    [0350 ... 0374] = { EMIT_DIVU_ext, NULL, 0, SR_NZVC, 1, 1, 2 },
 
    [0400 ... 0407] = { EMIT_SBCD_reg, NULL, SR_XZ, SR_XZC, 1, 0, 1 },
    [0410 ... 0417] = { EMIT_SBCD_mem, NULL, SR_XZ, SR_XZC, 1, 0, 1 },  //R0 Destination
    [0420 ... 0447] = { EMIT_OR_mem, NULL, 0, SR_NZVC, 1, 0, 1 },
    [0450 ... 0471] = { EMIT_OR_ext, NULL, 0, SR_NZVC, 1, 1, 1 },    //D0 Source
 
    [0500 ... 0507] = { EMIT_PACK_reg, NULL, 0, 0, 2, 0, 2 },
    [0510 ... 0517] = { EMIT_PACK_mem, NULL, 0, 0, 2, 0, 2 },  //_ext,//R0 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
    [0520 ... 0547] = { EMIT_OR_mem, NULL, 0, SR_NZVC, 1, 0, 2 }, 
    [0550 ... 0571] = { EMIT_OR_ext, NULL, 0, SR_NZVC, 1, 1, 2 },
 
    [0600 ... 0607] = { EMIT_UNPK_reg, NULL, 0, 0, 2, 0, 2 },
    [0610 ... 0617] = { EMIT_UNPK_mem, NULL, 0, 0, 2, 0, 2 },  //_ext,//R0 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
    [0620 ... 0647] = { EMIT_OR_mem, NULL, 0, SR_NZVC, 1, 0, 4 }, 
    [0650 ... 0671] = { EMIT_OR_ext, NULL, 0, SR_NZVC, 1, 1, 4 },

    [0700 ... 0707] = { EMIT_DIVS_reg, NULL, 0, SR_NZVC, 1, 0, 2 },  //D0 Destination, DIVS.W
    [0720 ... 0747] = { EMIT_DIVS_mem, NULL, 0, SR_NZVC, 1, 0, 2 },
    [0750 ... 0774] = { EMIT_DIVS_ext, NULL, 0, SR_NZVC, 1, 1, 2 },
};

uint32_t EMIT_line8(struct TranslatorContext *ctx)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr++);

    if (InsnTable[opcode & 0x1ff].od_Emit) {
        return InsnTable[opcode & 0x1ff].od_Emit(ctx, opcode);
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

uint32_t GetSR_Line8(uint16_t opcode)
{
    /* If instruction is in the table, return what flags it needs (shifted 16 bits left) and flags it sets */
    if (InsnTable[opcode & 0x1ff].od_Emit) {
        return (InsnTable[opcode & 0x1ff].od_SRNeeds << 16) | InsnTable[opcode & 0x1ff].od_SRSets;
    }
    /* Instruction not found, i.e. it needs all flags and sets none (ILLEGAL INSTRUCTION exception) */
    else {
        kprintf("Undefined Line8\n");
        return SR_CCR << 16;
    }
}

int M68K_GetLine8Length(uint16_t *insn_stream)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)insn_stream);
    
    int length = 0;
    int need_ea = 0;
    int opsize = 0;

    if (InsnTable[opcode & 0x1ff].od_Emit) {
        length = InsnTable[opcode & 0x1ff].od_BaseLength;
        need_ea = InsnTable[opcode & 0x1ff].od_HasEA;
        opsize = InsnTable[opcode & 0x1ff].od_OpSize;
    }

    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize);
    }

    return length;
}
