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

/* Line9 is one large ADDX/ADD/ADDA */

static uint32_t EMIT_ADD    (struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ADD_reg")));
static uint32_t EMIT_ADD_reg(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ADD_mem")));
static uint32_t EMIT_ADD_mem(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ADD_ext")));
static uint32_t EMIT_ADD_ext(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t direction = (opcode >> 8) & 1; // 0: Ea+Dn->Dn, 1: Ea+Dn->Ea
    uint8_t ext_words = 0;
    uint8_t tmp = 0xff;

    if (direction == 0)
    {
        uint8_t dest = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
        uint8_t src = 0xff;

        RA_SetDirtyM68kRegister(ctx, (opcode >> 9) & 7);
        if (size == 4 || update_mask == 0 || update_mask == SR_Z || update_mask == SR_N)
            EMIT_LoadFromEffectiveAddress(ctx, size, &src, opcode & 0x3f, &ext_words, 1, NULL);
        else
            EMIT_LoadFromEffectiveAddress(ctx, size, &src, opcode & 0x3f, &ext_words, 0, NULL);

        switch (size)
        {
        case 4:
            if (update_mask == 0)
                EMIT(ctx, add_reg(dest, dest, src, LSL, 0));
            else
                EMIT(ctx, adds_reg(dest, dest, src, LSL, 0));
            break;
        case 2:
            if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    add_reg(tmp, dest, src, LSL, 0),
                    bfxil(dest, tmp, 0, 16)
                );
                RA_FreeARMRegister(ctx, tmp);
            }
            else {
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    lsl(tmp, dest, 16),
                    adds_reg(src, tmp, src, LSL, 16),
                    bfxil(dest, src, 16, 16)
                );
                RA_FreeARMRegister(ctx, tmp);
            }
            break;
        case 1:
            if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    add_reg(tmp, dest, src, LSL, 0),
                    bfxil(dest, tmp, 0, 8)
                );
                RA_FreeARMRegister(ctx, tmp);
            }
            else {
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    lsl(tmp, dest, 24),
                    adds_reg(src, tmp, src, LSL, 24),
                    bfxil(dest, src, 24, 8)
                );
                RA_FreeARMRegister(ctx, tmp);
            }
            break;
        }

        if (size < 4 && update_mask == SR_Z)
        {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_ClearFlags(ctx, cc, SR_Z);

            switch (size) {
                case 2:
                    EMIT(ctx, ands_immed(31, dest, 16, 0));
                    break;
                case 1:
                    EMIT(ctx, ands_immed(31, dest, 8, 0));
                    break;
            }
            
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, A64_CC_EQ);
            update_mask = 0;
        }

        if (size < 4 && update_mask == SR_N)
        {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_ClearFlags(ctx, cc, SR_N);

            switch (size) {
                case 2:
                    EMIT(ctx, ands_immed(31, dest, 1, 32-15));
                    break;
                case 1:
                    EMIT(ctx, ands_immed(31, dest, 1, 32-7));
                    break;
            }
            
            EMIT_SetFlagsConditional(ctx, cc, SR_N, A64_CC_NE);
            update_mask = 0;
        }

        RA_FreeARMRegister(ctx, src);
    }
    else
    {
        uint8_t dest = 0xff;
        uint8_t src = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t mode = (opcode & 0x0038) >> 3;

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
                if (update_mask == 0)
                    EMIT(ctx, add_reg(tmp, tmp, src, LSL, 0));
                else
                    EMIT(ctx, adds_reg(tmp, tmp, src, LSL, 0));

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
                if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                    EMIT(ctx, add_reg(tmp, tmp, src, LSL, 0));
                }
                else 
                {
                    EMIT(ctx, 
                        lsl(tmp, tmp, 16),
                        adds_reg(tmp, tmp, src, LSL, 16),
                        lsr(tmp, tmp, 16)
                    );
                }

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
                if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                    EMIT(ctx, add_reg(tmp, tmp, src, LSL, 0));
                }
                else 
                {
                    EMIT(ctx, 
                        lsl(tmp, tmp, 24),
                        adds_reg(tmp, tmp, src, LSL, 24),
                        lsr(tmp, tmp, 24)
                    );
                }

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

        if (size < 4 && update_mask == SR_Z)
        {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_ClearFlags(ctx, cc, SR_Z);

            switch (size) {
                case 2:
                    EMIT(ctx, ands_immed(31, tmp, 16, 0));
                    break;
                case 1:
                    EMIT(ctx, ands_immed(31, tmp, 8, 0));
                    break;
            }
            
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, A64_CC_EQ);
            update_mask = 0;
        }

        if (size < 4 && update_mask == SR_N)
        {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_ClearFlags(ctx, cc, SR_N);

            switch (size) {
                case 2:
                    EMIT(ctx, ands_immed(31, tmp, 1, 32-15));
                    break;
                case 1:
                    EMIT(ctx, ands_immed(31, tmp, 1, 32-7));
                    break;
            }
            
            EMIT_SetFlagsConditional(ctx, cc, SR_N, A64_CC_NE);
            update_mask = 0;
        }

        RA_FreeARMRegister(ctx, dest);
        RA_FreeARMRegister(ctx, tmp);
    }

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);
        if (update_mask & SR_X)
            EMIT_GetNZCVX(ctx, cc, &update_mask);
        else
            EMIT_GetNZCV(ctx, cc, &update_mask);

        if (update_mask & SR_Z)
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
        if (update_mask & SR_V)
            EMIT_SetFlagsConditional(ctx, cc, SR_Valt, ARM_CC_VS);
        if (update_mask & (SR_X | SR_C)) {
            if ((update_mask & (SR_X | SR_C)) == SR_X)
                EMIT_SetFlagsConditional(ctx, cc, SR_X, ARM_CC_CS);
            else if ((update_mask & (SR_X | SR_C)) == SR_C)
                EMIT_SetFlagsConditional(ctx, cc, SR_Calt, ARM_CC_CS);
            else
                EMIT_SetFlagsConditional(ctx, cc, SR_Calt | SR_X, ARM_CC_CS);
        }
    }
    return 1;
}

static uint32_t EMIT_ADDA    (struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ADDA_reg")));
static uint32_t EMIT_ADDA_reg(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ADDA_mem")));
static uint32_t EMIT_ADDA_mem(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ADDA_ext")));
static uint32_t EMIT_ADDA_ext(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t ext_words = 0;
    uint8_t size = (opcode & 0x0100) == 0x0100 ? 4 : 2;
    uint8_t reg = RA_MapM68kRegister(ctx, ((opcode >> 9) & 7) + 8);
    uint8_t tmp = 0xff;
    uint8_t immed = (opcode & 0x3f) == 0x3c;

    if (size == 2)
    {
        if (immed)
        {
            int16_t offset = (int16_t)cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);

            if (offset >= 0 && offset < 4096)
            {
                EMIT(ctx, add_immed(reg, reg, offset));
                ext_words = 1;
            }
            else if (offset > -4096 && offset < 0)
            {
                EMIT(ctx, sub_immed(reg, reg, -offset));
                ext_words = 1;
            }
            else immed = 0;
        }

        if (immed == 0)
            EMIT_LoadFromEffectiveAddress(ctx, 0x80 | size, &tmp, opcode & 0x3f, &ext_words, 0, NULL);
    }    
    else
    {
        int32_t offset;
        if (immed)
        {
            offset = ((int16_t)cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr) << 16) | (uint16_t)cache_read_16(ICACHE, (uintptr_t)(ctx->tc_M68kCodePtr + 1));
            
            if (offset >= 0 && offset < 4096)
            {
                EMIT(ctx, add_immed(reg, reg, offset));
                ext_words = 2;
            }
            else if ((offset & 0xff000fff) == 0)
            {
                EMIT(ctx, add_immed_lsl12(reg, reg, offset >> 12));
                ext_words = 2;
            }
            else
            {
                immed = 0;
            }
        }

        if (immed == 0)
            EMIT_LoadFromEffectiveAddress(ctx, size, &tmp, opcode & 0x3f, &ext_words, 1, NULL);
    }
    
    if (tmp != 0xff)
        EMIT(ctx, add_reg(reg, reg, tmp, LSL, 0));

    RA_SetDirtyM68kRegister(ctx, ((opcode >> 9) & 7) + 8);

    RA_FreeARMRegister(ctx, tmp);

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

// BROKEN!!!
static uint32_t EMIT_ADDX    (struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ADDX_reg")));
static uint32_t EMIT_ADDX_reg(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ADDX_mem")));
static uint32_t EMIT_ADDX_mem(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t size = (opcode >> 6) & 3;
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);

    uint8_t cc = RA_GetCC(ctx);
    if (size == 2) {
        uint8_t tmp = RA_AllocARMRegister(ctx);

        EMIT(ctx, 
            and_immed(tmp, cc, 1, 31 & (32 - 4)),
            subs_immed(31, tmp, 0x10)
        );

        RA_FreeARMRegister(ctx, tmp);
    } else {
        EMIT(ctx, tst_immed(cc, 1, 31 & (32 - SRB_X)));
    }

    /* Register to register */
    if ((opcode & 0x0008) == 0)
    {
        uint8_t regx = RA_MapM68kRegister(ctx, opcode & 7);
        uint8_t regy = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
        uint8_t tmp = 0;
        uint8_t tmp_2 = 0;
        uint8_t tmp_cc_1 = RA_AllocARMRegister(ctx);
        uint8_t tmp_cc_2 = RA_AllocARMRegister(ctx);

        RA_SetDirtyM68kRegister(ctx, (opcode >> 9) & 7);

        switch (size)
        {
            case 0: /* Byte */
                tmp = RA_AllocARMRegister(ctx);
                tmp_2 = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    and_immed(tmp, regx, 8, 0),
                    and_immed(tmp_2, regy, 8, 0),
                    add_reg(tmp, tmp, tmp_2, LSL, 0),
                    csinc(tmp, tmp, tmp, A64_CC_EQ)
                );

                if (update_mask & SR_XVC) {
                    uint8_t tmp_3 = RA_AllocARMRegister(ctx);

                    EMIT(ctx, 
                        eor_reg(tmp_3, tmp_2, tmp, LSL, 0), // D ^ R -> tmp_3
                        eor_reg(tmp_2, regx, tmp, LSL, 0),  // S ^ R -> tmp_2
                        and_reg(tmp_3, tmp_2, tmp_3, LSL, 0), // V = (D^R) & (S^R), bit 7
                        mov_reg(tmp_2, tmp),                // tmp_2 <-- tmp, C at position 8
                        bfi(tmp_2, tmp_3, 0, 8),            // C at position 8, V at position 7
                        bfxil(cc, tmp_2, 7, 2)
                    );

                    if (update_mask & SR_X) {
                        EMIT(ctx, 
                            ror(0, cc, 1),
                            bfi(cc, 0, 4, 1)
                        );
                    }

                    RA_FreeARMRegister(ctx, tmp_3);

                    update_mask &= ~SR_XVC;
                }

                if (update_mask & SR_NZ) {
                    EMIT(ctx, adds_reg(31, 31, tmp, LSL, 24));
                }

                EMIT(ctx, bfxil(regy, tmp, 0, 8));
                RA_FreeARMRegister(ctx, tmp);
                RA_FreeARMRegister(ctx, tmp_2);
                break;

            case 1: /* Word */
                tmp = RA_AllocARMRegister(ctx);
                tmp_2 = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    and_immed(tmp, regx, 16, 0),
                    and_immed(tmp_2, regy, 16, 0),
                    add_reg(tmp, tmp, tmp_2, LSL, 0),
                    csinc(tmp, tmp, tmp, A64_CC_EQ)
                );

                if (update_mask & SR_XVC) {
                    uint8_t tmp_3 = RA_AllocARMRegister(ctx);

                    EMIT(ctx, 
                        eor_reg(tmp_3, tmp_2, tmp, LSL, 0), // D ^ R -> tmp_3
                        eor_reg(tmp_2, regx, tmp, LSL, 0),  // S ^ R -> tmp_2
                        and_reg(tmp_3, tmp_2, tmp_3, LSL, 0), // V = (D^R) & (S^R), bit 15
                        mov_reg(tmp_2, tmp),                 // tmp_2 <-- tmp. C at position 16
                        bfi(tmp_2, tmp_3, 0, 16),            // C at position 16, V at position 15
                        bfxil(cc, tmp_2, 15, 2)
                    );

                    if (update_mask & SR_X) {
                        EMIT(ctx, 
                            ror(0, cc, 1),
                            bfi(cc, 0, 4, 1)
                        );
                    }

                    RA_FreeARMRegister(ctx, tmp_3);

                    update_mask &= ~SR_XVC;
                }

                if (update_mask & SR_NZ) {
                    EMIT(ctx, adds_reg(31, 31, tmp, LSL, 16));
                }

                EMIT(ctx, bfxil(regy, tmp, 0, 16));
                RA_FreeARMRegister(ctx, tmp);
                RA_FreeARMRegister(ctx, tmp_2);
                break;

            case 2: /* Long */
                EMIT(ctx, adcs(regy, regy, regx));
                break;
        }
        RA_FreeARMRegister(ctx, tmp_cc_1);
        RA_FreeARMRegister(ctx, tmp_cc_2);
    }
    /* memory to memory */
    else
    {
        uint8_t regx = RA_MapM68kRegister(ctx, 8 + (opcode & 7));
        uint8_t regy = RA_MapM68kRegister(ctx, 8 + ((opcode >> 9) & 7));
        uint8_t dest = RA_AllocARMRegister(ctx);
        uint8_t src = RA_AllocARMRegister(ctx);
        uint8_t tmp_cc_1 = RA_AllocARMRegister(ctx);
        uint8_t tmp_cc_2 = RA_AllocARMRegister(ctx);
        uint8_t tmp = 0xff;

        RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
        RA_SetDirtyM68kRegister(ctx, 8 + ((opcode >> 9) & 7));

        switch (size)
        {
            case 0: /* Byte */
                EMIT(ctx, 
                    ldrb_offset_preindex(regx, src, (opcode & 7) == 7 ? -2 : -1),
                    ldrb_offset_preindex(regy, dest, ((opcode >> 9) & 7) == 7 ? -2 : -1)
                );

                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    add_reg(tmp, dest, src, LSL, 0),
                    csinc(tmp, tmp, tmp, A64_CC_EQ)
                );

                if (update_mask & SR_XVC) {
                    uint8_t tmp_3 = RA_AllocARMRegister(ctx);
                    uint8_t tmp_2 = RA_AllocARMRegister(ctx);

                    EMIT(ctx, 
                        eor_reg(tmp_3, dest, tmp, LSL, 0), // D ^ R -> tmp_3
                        eor_reg(tmp_2, src, tmp, LSL, 0),  // S ^ R -> tmp_2
                        and_reg(tmp_3, tmp_2, tmp_3, LSL, 0), // V = (D^R) & (S^R), bit 7
                        mov_reg(tmp_2, tmp),               // tmp_2 <-- tmp. C at position 8
                        bfi(tmp_2, tmp_3, 0, 8),           // C at position 8, V at position 7
                        bfxil(cc, tmp_2, 7, 2)
                    );

                    if (update_mask & SR_X) {
                        EMIT(ctx, 
                            ror(0, cc, 1),
                            bfi(cc, 0, 4, 1)
                        );
                    }

                    RA_FreeARMRegister(ctx, tmp_3);
                    RA_FreeARMRegister(ctx, tmp_2);

                    update_mask &= ~SR_XVC;
                }

                if (update_mask & SR_NZ) {
                    EMIT(ctx, adds_reg(31, 31, tmp, LSL, 24));
                }

                EMIT(ctx, strb_offset(regy, tmp, 0));
                RA_FreeARMRegister(ctx, tmp);
                break;

            case 1: /* Word */
                EMIT(ctx, 
                    ldrh_offset_preindex(regx, src, -2),
                    ldrh_offset_preindex(regy, dest, -2)
                );

                tmp = RA_AllocARMRegister(ctx);

                EMIT(ctx, 
                    add_reg(tmp, src, dest, LSL, 0),
                    csinc(tmp, tmp, tmp, A64_CC_EQ)
                );

                if (update_mask & SR_XVC) {
                    uint8_t tmp_3 = RA_AllocARMRegister(ctx);
                    uint8_t tmp_2 = RA_AllocARMRegister(ctx);

                    EMIT(ctx, 
                        eor_reg(tmp_3, dest, tmp, LSL, 0), // D ^ R -> tmp_3
                        eor_reg(tmp_2, src, tmp, LSL, 0),  // S ^ R -> tmp_2
                        and_reg(tmp_3, tmp_2, tmp_3, LSL, 0), // V = (D^R) & (S^R), bit 15
                        mov_reg(tmp_2, tmp),               // tmp_2 <-- tmp. C at position 16
                        bfi(tmp_2, tmp_3, 0, 16),          // C at position 16, V at position 15
                        bfxil(cc, tmp_2, 15, 2)
                    );

                    if (update_mask & SR_X) {
                        EMIT(ctx, 
                            ror(0, cc, 1),
                            bfi(cc, 0, 4, 1)
                        );
                    }

                    RA_FreeARMRegister(ctx, tmp_3);
                    RA_FreeARMRegister(ctx, tmp_2);

                    update_mask &= ~SR_XVC;
                }

                if (update_mask & SR_NZ) {
                    EMIT(ctx, adds_reg(31, 31, tmp, LSL, 16));
                }

                EMIT(ctx, strh_offset(regy, tmp, 0));
                RA_FreeARMRegister(ctx, tmp);
                break;

            case 2: /* Long */
                EMIT(ctx, 
                    ldr_offset_preindex(regx, src, -4),
                    ldr_offset_preindex(regy, dest, -4)
                );

                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    adcs(dest, dest, src),
                    str_offset(regy, dest, 0)
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
        }

        RA_FreeARMRegister(ctx, dest);
        RA_FreeARMRegister(ctx, src);
        RA_FreeARMRegister(ctx, tmp_cc_1);
        RA_FreeARMRegister(ctx, tmp_cc_2);
    }

    EMIT_AdvancePC(ctx, 2);

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);

        /* No chance of getting NZVC in quick way. The Z flags is unchanged if result is zero */

        if (update_mask & SR_Z) {
            EMIT(ctx, 
                b_cc(ARM_CC_EQ, 2),
                bic_immed(cc, cc, 1, 30)
            );
            update_mask &= ~SR_Z;
        }
        uint8_t alt_flags = update_mask;
        if ((alt_flags & 3) != 0 && (alt_flags & 3) < 3)
            alt_flags ^= 3;
        EMIT_ClearFlags(ctx, cc, alt_flags);
        if (update_mask & SR_N)
            EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
        if (update_mask & SR_V)
            EMIT_SetFlagsConditional(ctx, cc, SR_Valt, ARM_CC_VS);
        if (update_mask & (SR_X | SR_C)) {
            if ((update_mask & (SR_X | SR_C)) == SR_X)
                EMIT_SetFlagsConditional(ctx, cc, SR_X, ARM_CC_CS);
            else if ((update_mask & (SR_X | SR_C)) == SR_C)
                EMIT_SetFlagsConditional(ctx, cc, SR_Calt, ARM_CC_CS);
            else
                EMIT_SetFlagsConditional(ctx, cc, SR_Calt | SR_X, ARM_CC_CS);
        }
    }

    return 1;
}

static struct OpcodeDef InsnTable[4096] = {
    [0000 ... 0007] = { EMIT_ADD_reg, NULL, 0, SR_CCR, 1, 0, 1 },        //Dn Destination, Byte
    [0020 ... 0047] = { EMIT_ADD_mem, NULL, 0, SR_CCR, 1, 0, 1 },
    [0050 ... 0074] = { EMIT_ADD_ext, NULL, 0, SR_CCR, 1, 1, 1 },
    [0100 ... 0117] = { EMIT_ADD_reg, NULL, 0, SR_CCR, 1, 0, 2 },        //Word
    [0120 ... 0147] = { EMIT_ADD_mem, NULL, 0, SR_CCR, 1, 0, 2 },
    [0150 ... 0174] = { EMIT_ADD_ext, NULL, 0, SR_CCR, 1, 1, 2 },
    [0200 ... 0217] = { EMIT_ADD_reg, NULL, 0, SR_CCR, 1, 0, 4 },        //Long
    [0220 ... 0247] = { EMIT_ADD_mem, NULL, 0, SR_CCR, 1, 0, 4 },
    [0250 ... 0274] = { EMIT_ADD_ext, NULL, 0, SR_CCR, 1, 1, 4 },
    [0300 ... 0317] = { EMIT_ADDA_reg, NULL, 0, 0, 1, 0, 2 },            //Word
    [0320 ... 0347] = { EMIT_ADDA_mem, NULL, 0, 0, 1, 0, 2 },
    [0350 ... 0374] = { EMIT_ADDA_ext, NULL, 0, 0, 1, 1, 2 },
    [0400 ... 0407] = { EMIT_ADDX_reg, NULL, SR_XZ, SR_CCR, 1, 0, 1 },   //Byte
    [0410 ... 0417] = { EMIT_ADDX_mem, NULL, SR_XZ, SR_CCR, 1, 0, 1 }, 
    [0500 ... 0507] = { EMIT_ADDX_reg, NULL, SR_XZ, SR_CCR, 1, 0, 2 },   //Word
    [0510 ... 0517] = { EMIT_ADDX_mem, NULL, SR_XZ, SR_CCR, 1, 0, 2 },
    [0600 ... 0607] = { EMIT_ADDX_reg, NULL, SR_XZ, SR_CCR, 1, 0, 4 },   //Long
    [0610 ... 0617] = { EMIT_ADDX_mem, NULL, SR_XZ, SR_CCR, 1, 0, 4 },
    [0420 ... 0447] = { EMIT_ADD_mem, NULL, 0, SR_CCR, 1, 0, 1 },        //Dn Source, Byte
    [0450 ... 0471] = { EMIT_ADD_ext, NULL, 0, SR_CCR, 1, 1, 1 },
    [0520 ... 0547] = { EMIT_ADD_mem, NULL, 0, SR_CCR, 1, 0, 2 },        //Word
    [0550 ... 0571] = { EMIT_ADD_ext, NULL, 0, SR_CCR, 1, 1, 2 },
    [0620 ... 0647] = { EMIT_ADD_mem, NULL, 0, SR_CCR, 1, 0, 4 },        //Long
    [0650 ... 0671] = { EMIT_ADD_ext, NULL, 0, SR_CCR, 1, 1, 4 },
    [0700 ... 0717] = { EMIT_ADDA_reg, NULL, 0, 0, 1, 0, 4 },
    [0720 ... 0747] = { EMIT_ADDA_mem, NULL, 0, 0, 1, 0, 4 },
    [0750 ... 0774] = { EMIT_ADDA_ext, NULL, 0, 0, 1, 1, 4 },            //Long
};

uint32_t EMIT_lineD(struct TranslatorContext *ctx)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr++);

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

uint32_t GetSR_LineD(uint16_t opcode)
{
    /* If instruction is in the table, return what flags it needs (shifted 16 bits left) and flags it sets */
    if (InsnTable[opcode & 00777].od_Emit) {
        return (InsnTable[opcode & 00777].od_SRNeeds << 16) | InsnTable[opcode & 00777].od_SRSets;
    }
    /* Instruction not found, i.e. it needs all flags and sets none (ILLEGAL INSTRUCTION exception) */
    else {
        kprintf("Undefined LineD\n");
        return SR_CCR << 16;
    }
}


int M68K_GetLineDLength(uint16_t *insn_stream)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)insn_stream);
    
    int length = 0;
    int need_ea = 0;
    int opsize = 0;

    if (InsnTable[opcode & 0777].od_Emit) {
        length = InsnTable[opcode & 0777].od_BaseLength;
        need_ea = InsnTable[opcode & 0777].od_HasEA;
        opsize = InsnTable[opcode & 0777].od_OpSize;
    }

    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize);
    }

    return length;
}