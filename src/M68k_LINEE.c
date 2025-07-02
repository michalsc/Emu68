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

static uint32_t EMIT_ASR_mem(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ASL_mem")));
static uint32_t EMIT_ASL_mem(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t dest = 0xff;
    uint8_t tmp = RA_AllocARMRegister(ctx);
    uint8_t ext_words = 0;
    EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_words, 1, NULL);

    /* Pre-decrement mode */
    if ((opcode & 0x38) == 0x20) {
        EMIT(ctx, ldrsh_offset_preindex(dest, tmp, -2));
    }
    else {
        EMIT(ctx, ldrsh_offset(dest, tmp, 0));
    }

    if (update_mask & (SR_C | SR_X)) {
        if (direction) {
            EMIT(ctx, tst_immed(tmp, 1, 32 - 15));
        }
        else {
            EMIT(ctx, tst_immed(tmp, 1, 0));
        }
    }

    if (direction)
    {
        EMIT(ctx, lsl(tmp, tmp, 1));
    }
    else
    {
        EMIT(ctx, asr(tmp, tmp, 1));
    }

    if ((opcode & 0x38) == 0x18) {
        EMIT(ctx, strh_offset_postindex(dest, tmp, 2));
    }
    else {
        EMIT(ctx, strh_offset(dest, tmp, 0));
    }

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);
        uint8_t tmp2 = RA_AllocARMRegister(ctx);
        
        uint8_t alt_mask = update_mask;
        if ((alt_mask & 3) != 0 && (alt_mask & 3) < 3)
            alt_mask ^= 3;
        EMIT_ClearFlags(ctx, cc, alt_mask);

        if (update_mask & (SR_C | SR_X)) {
            EMIT(ctx, 
                b_cc(A64_CC_EQ, 3),
                mov_immed_u16(tmp2, SR_Calt | SR_X, 0),
                orr_reg(cc, cc, tmp2, LSL, 0)
            );
        }

        if (update_mask & (SR_Z | SR_N))
        {
            EMIT(ctx, cmn_reg(31, tmp, LSL, 16));
        
            if (update_mask & SR_Z) {
                EMIT(ctx, 
                    b_cc(A64_CC_EQ ^ 1, 2),
                    orr_immed(cc, cc, 1, (32 - SRB_Z) & 31)
                );
            }
            if (update_mask & SR_N) {
                EMIT(ctx, 
                    b_cc(A64_CC_MI ^ 1, 2),
                    orr_immed(cc, cc, 1, (32 - SRB_N) & 31)
                );
            }
        }

        RA_FreeARMRegister(ctx, tmp2);

        // V flag can have non-zero value only for left shifting
        if ((update_mask & SR_V) && direction)
        {
            EMIT(ctx, 
                eor_reg(tmp, tmp, tmp, LSL, 1),
                tbz(tmp, 16, 2),
                orr_immed(cc, cc, 1, (32 - SRB_Valt) & 31)
            );
        }
    }
    RA_FreeARMRegister(ctx, tmp);
    RA_FreeARMRegister(ctx, dest);
    
    return 1;
}

static uint32_t EMIT_LSR_mem(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_LSL_mem")));
static uint32_t EMIT_LSL_mem(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t dest = 0xff;
    uint8_t tmp = RA_AllocARMRegister(ctx);
    uint8_t ext_words = 0;
    
    EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_words, 1, NULL);

    /* Pre-decrement mode */
    if ((opcode & 0x38) == 0x20) {
        EMIT(ctx, ldrh_offset_preindex(dest, tmp, -2));
    }
    else {
        EMIT(ctx, ldrh_offset(dest, tmp, 0));
    }

    if (update_mask & (SR_C | SR_X)) {
        if (direction) {
            EMIT(ctx, tst_immed(tmp, 1, 32 - 15));
        }
        else {
            EMIT(ctx, tst_immed(tmp, 1, 0));
        }
    }

    if (direction)
    {
        EMIT(ctx, lsl(tmp, tmp, 1));
    }
    else
    {
        EMIT(ctx, lsr(tmp, tmp, 1));
    }

    if ((opcode & 0x38) == 0x18) {
        EMIT(ctx, strh_offset_postindex(dest, tmp, 2));
    }
    else {
        EMIT(ctx, strh_offset(dest, tmp, 0));
    }
        
    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);
        uint8_t tmp2 = RA_AllocARMRegister(ctx);
        
        uint8_t alt_mask = update_mask;
        if ((alt_mask & 3) != 0 && (alt_mask & 3) < 3)
            alt_mask ^= 3;
        EMIT_ClearFlags(ctx, cc, alt_mask);

        if (update_mask & (SR_C | SR_X)) {
            EMIT(ctx, 
                b_cc(A64_CC_EQ, 3),
                mov_immed_u16(tmp2, SR_Calt | SR_X, 0),
                orr_reg(cc, cc, tmp2, LSL, 0)
            );
        }

        RA_FreeARMRegister(ctx, tmp2);

        if (update_mask & (SR_Z | SR_N))
        {
            EMIT(ctx, cmn_reg(31, tmp, LSL, 16));
        
            if (update_mask & SR_Z) {
                EMIT(ctx, 
                    b_cc(A64_CC_EQ ^ 1, 2),
                    orr_immed(cc, cc, 1, (32 - SRB_Z) & 31)
                );
            }
            if (update_mask & SR_N) {
                EMIT(ctx, 
                    b_cc(A64_CC_MI ^ 1, 2),
                    orr_immed(cc, cc, 1, (32 - SRB_N) & 31)
                );
            }
        }
    }
    RA_FreeARMRegister(ctx, tmp);
    RA_FreeARMRegister(ctx, dest);

    return 1;
}

static uint32_t EMIT_ROXR_mem(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ROXL_mem")));
static uint32_t EMIT_ROXL_mem(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t dest = 0xff;
    uint8_t ext_words = 0;    
    uint8_t tmp = RA_AllocARMRegister(ctx);
    EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_words, 1, NULL);

    uint8_t cc = RA_ModifyCC(ctx);

    if ((opcode & 0x38) == 0x20) {
        EMIT(ctx, ldrh_offset_preindex(dest, tmp, -2));
    }
    else {
        EMIT(ctx, ldrh_offset(dest, tmp, 0));
    }

    /* Test X flag, push the flag value into tmp register */
    EMIT(ctx, 
        tst_immed(cc, 1, 32 - SRB_X),
        b_cc(A64_CC_EQ, 2)
    );

    if (direction) {
        EMIT(ctx, 
            orr_immed(tmp, tmp, 1, 1),
            ror(tmp, tmp, 31)
        );
    }
    else {
        EMIT(ctx, 
            orr_immed(tmp, tmp, 1, 16),
            ror(tmp, tmp, 1)
        );
    }

    if ((opcode & 0x38) == 0x18) {
        EMIT(ctx, strh_offset_postindex(dest, tmp, 2));
    }
    else {
        EMIT(ctx, strh_offset(dest, tmp, 0));
    }

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    if (update_mask)
    {
        uint8_t tmp2 = RA_AllocARMRegister(ctx);
        uint8_t update_mask_copy = update_mask;

        if (update_mask & (SR_Z | SR_N))
        {
            EMIT(ctx, cmn_reg(31, tmp, LSL, 16));
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }
        else if (update_mask & SR_V) {
            EMIT(ctx, bic_immed(cc, cc, 1, 32 - SRB_Valt));
        }

        if (update_mask_copy & SR_XC) {
            if (direction) {
                EMIT(ctx, 
                    bfxil(tmp, tmp, 16, 1),
                    bfi(cc, tmp, 1, 1)
                );
            }
            else {
                EMIT(ctx, 
                    bfxil(tmp, tmp, 31, 1),
                    bfi(cc, tmp, 1, 1)
                );
            }
            if (update_mask_copy & SR_X) {
                EMIT(ctx, 
                    ror(0, cc, 1),
                    bfi(cc, 0, 4, 1)
                );
            }
        }
      
        RA_FreeARMRegister(ctx, tmp2);
    }

    RA_FreeARMRegister(ctx, tmp);
    RA_FreeARMRegister(ctx, dest);

    return 1;
}

static uint32_t EMIT_ROR_mem(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ROL_mem")));
static uint32_t EMIT_ROL_mem(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t dest = 0xff;
    uint8_t tmp = RA_AllocARMRegister(ctx);
    uint8_t ext_words = 0;
    EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_words, 1, NULL);

    if ((opcode & 0x38) == 0x20) {
        EMIT(ctx, ldrh_offset_preindex(dest, tmp, -2));
    }
    else {
        EMIT(ctx, ldrh_offset(dest, tmp, 0));
    }
    EMIT(ctx, bfi(tmp, tmp, 16, 16));

    if (direction)
    {
        EMIT(ctx, ror(tmp, tmp, 32 - 1));
    }
    else
    {
        EMIT(ctx, ror(tmp, tmp, 1));
    }

    if ((opcode & 0x38) == 0x18) {
        EMIT(ctx, strh_offset_postindex(dest, tmp, 2));
    }
    else {
        EMIT(ctx, strh_offset(dest, tmp, 0));
    }

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    if (update_mask)
    {
        uint8_t tmp2 = RA_AllocARMRegister(ctx);
        uint8_t cc = RA_ModifyCC(ctx);

        if (update_mask & (SR_Z | SR_N))
        {
            EMIT(ctx, cmn_reg(31, tmp, LSL, 16));
        }
        uint8_t alt_flags = update_mask;
        if ((alt_flags & 3) != 0 && (alt_flags & 3) < 3)
            alt_flags ^= 3;
        EMIT_ClearFlags(ctx, cc, alt_flags);

        if (update_mask & SR_Z) {
            EMIT(ctx, 
                b_cc(A64_CC_EQ ^ 1, 2),
                orr_immed(cc, cc, 1, (32 - SRB_Z) & 31)
            );
        }
        if (update_mask & SR_N) {
            EMIT(ctx, 
                b_cc(A64_CC_MI ^ 1, 2),
                orr_immed(cc, cc, 1, (32 - SRB_N) & 31)
            );
        }

        if (update_mask & (SR_C)) {
            if (direction) {
                EMIT(ctx, tst_immed(tmp, 1, 16));
            }
            else {
                EMIT(ctx, tst_immed(tmp, 1, 1));
            }
            EMIT(ctx, 
                b_cc(A64_CC_EQ, 2),
                orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt))
            );
        }
        RA_FreeARMRegister(ctx, tmp2);
    }
    RA_FreeARMRegister(ctx, tmp);
    RA_FreeARMRegister(ctx, dest);

    return 1;
}

static uint32_t EMIT_ASR_reg(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ASL_reg")));
static uint32_t EMIT_ASL_reg(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t cc = RA_ModifyCC(ctx);
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t shift = (opcode >> 9) & 7;
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t reg = RA_MapM68kRegister(ctx, opcode & 7);
    uint8_t tmp = RA_AllocARMRegister(ctx);
    uint8_t direction = (opcode >> 8) & 1;
    uint32_t *tmpptr_1;
    uint32_t *tmpptr_2;
    uint8_t shiftreg_orig = RA_AllocARMRegister(ctx);
    uint8_t reg_orig = RA_AllocARMRegister(ctx);
    uint8_t mask = RA_AllocARMRegister(ctx);

    RA_SetDirtyM68kRegister(ctx, opcode & 7);

    uint8_t shiftreg = RA_MapM68kRegister(ctx, shift);

    // Check shift size 0 - in that case no bit shifting is necessary, clear VC flags, update NZ, leave X
    EMIT(ctx, ands_immed(shiftreg_orig, shiftreg, 6, 0));
    tmpptr_1 = ctx->tc_CodePtr++;

    // If N and/or Z need to be updated, do it and clear CV. No further actions are necessary
    if (update_mask & SR_NZ) {
        uint8_t update_mask_copy = update_mask;
        switch (size) {
            case 4:
                EMIT(ctx, cmn_reg(31, reg, LSL, 0));
                break;
            case 2:
                EMIT(ctx, cmn_reg(31, reg, LSL, 16));
                break;
            case 1:
                EMIT(ctx, cmn_reg(31, reg, LSL, 24));
                break;
        }
        EMIT_GetNZ00(ctx, cc, &update_mask_copy);
    }
    else if (update_mask & SR_VC) {
        // Only V or C need to be updated. Clear both
        EMIT(ctx, bic_immed(cc, cc, 2, 0));
    }

    // Skip further bit shifting totally
    tmpptr_2 = ctx->tc_CodePtr++;

    if ((ctx->tc_CodePtr - tmpptr_1) != 2) {
        *tmpptr_1 = b_cc(A64_CC_NE, ctx->tc_CodePtr - tmpptr_1);
    }
    else {
        ctx->tc_CodePtr--;
        tmpptr_2--;
        tmpptr_1 = NULL;
    }

    if (direction)
    {
        if (update_mask & SR_V)
        {
            EMIT(ctx, 
                mov64_immed_u16(mask, 0x8000, 3),
                asrv64(mask, mask, shiftreg_orig)
            );
            switch (size)
            {
                case 4:
                    EMIT(ctx, 
                        lsr64(mask, mask, 32),
                        mov_reg(reg_orig, reg)
                    );
                    break;
                case 2:
                    EMIT(ctx, 
                        lsr64(mask, mask, 32+16),
                        and_immed(reg_orig, reg, 16, 0)
                    );
                    break;
                case 1:
                    EMIT(ctx, 
                        lsr64(mask, mask, 32 + 24),
                        and_immed(reg_orig, reg, 8, 0)
                    );
                    break;
            }
        }

        switch(size)
        {
            case 4:
                EMIT(ctx, 
                    lslv64(tmp, reg, shiftreg),
                    mov_reg(reg, tmp)
                );
                if (update_mask & (SR_C | SR_X)) {
                    EMIT(ctx, tst64_immed(tmp, 1, 32, 1));
                }
                break;
            case 2:
                EMIT(ctx, lslv64(tmp, reg, shiftreg));
                if (update_mask & (SR_C | SR_X)) {
                    EMIT(ctx, tst_immed(tmp, 1, 16));
                }
                EMIT(ctx, bfi(reg, tmp, 0, 16));
                break;
            case 1:
                EMIT(ctx, lslv64(tmp, reg, shiftreg));
                if (update_mask & (SR_C | SR_X)) {
                    EMIT(ctx, tst_immed(tmp, 1, 24));
                }
                EMIT(ctx, bfi(reg, tmp, 0, 8));
                break;
        }
    }
    else
    {
        uint8_t mask = RA_AllocARMRegister(ctx);

        if (update_mask & (SR_C | SR_X))
        {
            uint8_t t = RA_AllocARMRegister(ctx);
            EMIT(ctx, 
                sub_immed(t, shiftreg, 1),
                mov_immed_u16(mask, 1, 0),
                lslv64(mask, mask, t)
            );
            RA_FreeARMRegister(ctx, t);
        }

        switch (size)
        {
            case 4:
                EMIT(ctx, sxtw64(tmp, reg));
                if (update_mask & (SR_C | SR_X))
                {
                    EMIT(ctx, ands64_reg(31, tmp, mask, LSL, 0));
                }
                EMIT(ctx, 
                    asrv64(tmp, tmp, shiftreg),
                    mov_reg(reg, tmp)
                );
                break;
            case 2:
                EMIT(ctx, sxth64(tmp, reg));
                if (update_mask & (SR_C | SR_X))
                {
                    EMIT(ctx, ands64_reg(31, tmp, mask, LSL, 0));
                }
                EMIT(ctx, 
                    asrv64(tmp, tmp, shiftreg),
                    bfi(reg, tmp, 0, 16)
                );
                break;
            case 1:
                EMIT(ctx, sxtb64(tmp, reg));
                if (update_mask & (SR_C | SR_X))
                {
                    EMIT(ctx, ands64_reg(31, tmp, mask, LSL, 0));
                }
                EMIT(ctx, 
                    asrv64(tmp, tmp, shiftreg),
                    bfi(reg, tmp, 0, 8)
                );
                break;
        }

        RA_FreeARMRegister(ctx, mask);
    }

    if (update_mask)
    {
        uint8_t tmp2 = RA_AllocARMRegister(ctx);
        uint8_t alt_flags = update_mask;
        if ((alt_flags & 3) != 0 && (alt_flags & 3) < 3)
            alt_flags ^= 3;
        EMIT_ClearFlags(ctx, cc, alt_flags);

        if (update_mask & (SR_C | SR_X)) {
            EMIT(ctx, 
                b_cc(A64_CC_EQ, 3),
                mov_immed_u16(tmp2, SR_Calt | SR_X, 0),
                orr_reg(cc, cc, tmp2, LSL, 0)
            );
        }

        RA_FreeARMRegister(ctx, tmp2);

        if (direction && (update_mask & SR_V)) {
            EMIT(ctx, 
                ands_reg(31, reg_orig, mask, LSL, 0),
                b_cc(A64_CC_EQ, 7),
                cmp_immed(shiftreg_orig, size == 4 ? 32 : (size == 2 ? 16 : 8)),
                b_cc(A64_CC_GE, 4),
                eor_reg(reg_orig, reg_orig, mask, LSL, 0),
                ands_reg(31, reg_orig, mask, LSL, 0),
                b_cc(A64_CC_EQ, 2),
                orr_immed(cc, cc, 1, 31 & (32 - SRB_Valt))
            );
        }

        if (update_mask & (SR_Z | SR_N))
        {
            switch(size)
            {
                case 4:
                    EMIT(ctx, cmn_reg(31, reg, LSL, 0));
                    break;
                case 2:
                    EMIT(ctx, cmn_reg(31, tmp, LSL, 16));
                    break;
                case 1:
                    EMIT(ctx, cmn_reg(31, tmp, LSL, 24));
                    break;
            }

            if (update_mask & SR_Z) {
                EMIT(ctx, 
                    b_cc(A64_CC_EQ ^ 1, 2),
                    orr_immed(cc, cc, 1, (32 - SRB_Z) & 31)
                );
            }
            if (update_mask & SR_N) {
                EMIT(ctx, 
                    b_cc(A64_CC_MI ^ 1, 2),
                    orr_immed(cc, cc, 1, (32 - SRB_N) & 31)
                );
            }
        }
    }

    if (tmpptr_1 != NULL) {
        *tmpptr_2 = b_cc(A64_CC_AL, ctx->tc_CodePtr - tmpptr_2);
    }
    else {
        *tmpptr_2 = b_cc(A64_CC_EQ, ctx->tc_CodePtr - tmpptr_2);
    }

    EMIT_AdvancePC(ctx, 2);

    RA_FreeARMRegister(ctx, tmp);
    RA_FreeARMRegister(ctx, mask);
    RA_FreeARMRegister(ctx, shiftreg_orig);
    RA_FreeARMRegister(ctx, reg_orig);

    return 1;

}

static uint32_t EMIT_ASR(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ASL")));
static uint32_t EMIT_ASL(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t shift = (opcode >> 9) & 7;
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t reg = RA_MapM68kRegister(ctx, opcode & 7);
    uint8_t tmp = RA_AllocARMRegister(ctx);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t cc = RA_ModifyCC(ctx);
    RA_SetDirtyM68kRegister(ctx, opcode & 7);

    if (!shift) shift = 8;

    if (direction && (update_mask & SR_V))
    {
        uint8_t tmp_reg = RA_AllocARMRegister(ctx);
        int rot = (size == 4) ? 0 : (size == 2) ? 16 : 24;
        int width = shift + 1;

        if (size == 1 && width > 8)
            width = 8;

        EMIT(ctx, 
            bic_immed(cc, cc, 1, 31 & (32 - SRB_Valt)),
            ands_immed(tmp_reg, reg, width, width + rot),
            b_cc(A64_CC_EQ, (size == 1 && shift == 8) ? 2 : 5)
        );
        if (!(size == 1 && shift == 8)) {
            EMIT(ctx, 
                eor_immed(tmp_reg, tmp_reg, width, width + rot),
                ands_immed(tmp_reg, tmp_reg, width, width + rot),
                b_cc(A64_CC_EQ, 2)
            );
        }
        EMIT(ctx, orr_immed(cc, cc, 1, 31 & (32 - SRB_Valt)));
        
        update_mask &= ~SR_V;
        RA_FreeARMRegister(ctx, tmp_reg);
    }

    if (update_mask & (SR_C | SR_X)) {
        if (direction) {
            switch (size) {
                case 4:
                    EMIT(ctx, tst_immed(reg, 1, shift));
                    break;
                case 2:
                    EMIT(ctx, tst_immed(reg, 1, 16 + shift));
                    break;
                case 1:
                    EMIT(ctx, tst_immed(reg, 1, 31 & (24 + shift)));
                    break;
            }
        }
        else {
            EMIT(ctx, tst_immed(reg, 1, 31 & (33 - shift)));
        }
    }

    if (direction)
    {
        switch (size)
        {
            case 4:
                EMIT(ctx, lsl(reg, reg, shift));
                break;
            case 2:
                EMIT(ctx, 
                    lsl(tmp, reg, shift),
                    bfi(reg, tmp, 0, 16)
                );
                break;
            case 1:
                EMIT(ctx, 
                    lsl(tmp, reg, shift),
                    bfi(reg, tmp, 0, 8)
                );
                break;
        }
    }
    else
    {
        switch (size)
        {
        case 4:
            EMIT(ctx, asr(reg, reg, shift));
            break;
        case 2:
            EMIT(ctx, 
                sxth(tmp, reg),
                asr(tmp, tmp, shift),
                bfi(reg, tmp, 0, 16)
            );
            break;
        case 1:
            EMIT(ctx, 
                sxtb(tmp, reg),
                asr(tmp, tmp, shift),
                bfi(reg, tmp, 0, 8)
            );
            break;
        }
    }

    EMIT_AdvancePC(ctx, 2);

    if (update_mask)
    {
        uint8_t tmp2 = RA_AllocARMRegister(ctx);
        uint8_t clear_mask = update_mask;

        /* Swap C and V flags in immediate */
        if ((clear_mask & 3) != 0 && (clear_mask & 3) < 3)
            clear_mask ^= 3;

        EMIT_ClearFlags(ctx, cc, clear_mask);

        if (update_mask & (SR_C | SR_X)) {
            EMIT(ctx, 
                b_cc(A64_CC_EQ, 3),
                mov_immed_u16(tmp2, SR_Calt | SR_X, 0),
                orr_reg(cc, cc, tmp2, LSL, 0)
            );
        }

        RA_FreeARMRegister(ctx, tmp2);

        if (update_mask & (SR_Z | SR_N))
        {
            switch(size)
            {
                case 4:
                    EMIT(ctx, cmn_reg(31, reg, LSL, 0));
                    break;
                case 2:
                    EMIT(ctx, cmn_reg(31, tmp, LSL, 16));
                    break;
                case 1:
                    EMIT(ctx, cmn_reg(31, tmp, LSL, 24));
                    break;
            }

            if (update_mask & SR_Z) {
                EMIT(ctx, b_cc(A64_CC_EQ ^ 1, 2));
                EMIT(ctx, orr_immed(cc, cc, 1, (32 - SRB_Z) & 31));
            }
            if (update_mask & SR_N) {
                EMIT(ctx, b_cc(A64_CC_MI ^ 1, 2));
                EMIT(ctx, orr_immed(cc, cc, 1, (32 - SRB_N) & 31));
            }
        }
    }

    RA_FreeARMRegister(ctx, tmp);

    return 1;
}

static uint32_t EMIT_LSR_reg(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_LSL_reg")));
static uint32_t EMIT_LSL_reg(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t cc = RA_ModifyCC(ctx);
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t shift = (opcode >> 9) & 7;
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t reg = RA_MapM68kRegister(ctx, opcode & 7);
    uint8_t tmp = RA_AllocARMRegister(ctx);
    uint32_t *tmpptr_1;
    uint32_t *tmpptr_2;

    RA_SetDirtyM68kRegister(ctx, opcode & 7);

    uint8_t shiftreg = RA_MapM68kRegister(ctx, shift);

    // Check shift size 0 - in that case no bit shifting is necessary, clear VC flags, update NZ, leave X
    EMIT(ctx, ands_immed(31, shiftreg, 6, 0));
    tmpptr_1 = ctx->tc_CodePtr++;

    // If V and/or Z need to be updated, do it and clear CV. No further actions are necessary
    if (update_mask & SR_NZ) {
        uint8_t update_mask_copy = update_mask;
        switch (size) {
            case 4:
                EMIT(ctx, cmn_reg(31, reg, LSL, 0));
                break;
            case 2:
                EMIT(ctx, cmn_reg(31, reg, LSL, 16));
                break;
            case 1:
                EMIT(ctx, cmn_reg(31, reg, LSL, 24));
                break;
        }
        EMIT_GetNZ00(ctx, cc, &update_mask_copy);
    }
    else if (update_mask & SR_VC) {
        // Only V or C need to be updated. Clear both
        EMIT(ctx, bic_immed(cc, cc, 2, 0));
    }

    // Skip further bit shifting totally
    tmpptr_2 = ctx->tc_CodePtr++;

    if ((ctx->tc_CodePtr - tmpptr_1) != 2) {
        *tmpptr_1 = b_cc(A64_CC_NE, ctx->tc_CodePtr - tmpptr_1);
    }
    else {
        ctx->tc_CodePtr--;
        tmpptr_2--;
        tmpptr_1 = NULL;
    }

    if (direction)
    {
        switch (size)
        {
        case 4:
            EMIT(ctx, 
                lslv64(tmp, reg, shiftreg),
                mov_reg(reg, tmp)
            );
            if (update_mask & (SR_C | SR_X)) {
                EMIT(ctx, tst64_immed(tmp, 1, 32, 1));
            }
            break;
        case 2:
            EMIT(ctx, lslv64(tmp, reg, shiftreg));
            if (update_mask & (SR_C | SR_X)) {
                EMIT(ctx, tst_immed(tmp, 1, 16));
            }
            EMIT(ctx, bfi(reg, tmp, 0, 16));
            break;
        case 1:
            EMIT(ctx, lslv64(tmp, reg, shiftreg));
            if (update_mask & (SR_C | SR_X)) {
                EMIT(ctx, tst_immed(tmp, 1, 24));
            }
            EMIT(ctx, bfi(reg, tmp, 0, 8));
            break;
        }
    }
    else
    {
        uint8_t mask = RA_AllocARMRegister(ctx);
        if (update_mask & (SR_C | SR_X))
        {
            uint8_t t = RA_AllocARMRegister(ctx);
/*
            *ptr++ = sub_immed(t, shiftreg, 1);
            *ptr++ = mov_immed_u16(mask, 1, 0);
            *ptr++ = lslv64(mask, mask, t);
*/
            RA_FreeARMRegister(ctx, t);
        }
        switch (size)
        {
        case 4:
            EMIT(ctx, mov_reg(tmp, reg));
            if (update_mask & (SR_C | SR_X))
            {
                //*ptr++ = ands_reg(31, tmp, mask, LSL, 0);
                EMIT(ctx, 
                    rorv64(0, tmp, shiftreg),
                    tst64_immed(0, 1, 1, 1)
                );
            }
            EMIT(ctx, 
                lsrv64(tmp, tmp, shiftreg),
                mov_reg(reg, tmp)
            );
            break;
        case 2:
            EMIT(ctx, uxth(tmp, reg));
            if (update_mask & (SR_C | SR_X))
            {
                //*ptr++ = ands_reg(31, tmp, mask, LSL, 0);
                EMIT(ctx, 
                    rorv64(0, tmp, shiftreg),
                    tst64_immed(0, 1, 1, 1)
                );
            }
            EMIT(ctx, 
                lsrv64(tmp, tmp, shiftreg),
                bfi(reg, tmp, 0, 16)
            );
            break;
        case 1:
            EMIT(ctx, uxtb(tmp, reg));
            if (update_mask & (SR_C | SR_X))
            {
                //*ptr++ = ands_reg(31, tmp, mask, LSL, 0);
                EMIT(ctx, 
                    rorv64(0, tmp, shiftreg),
                    tst64_immed(0, 1, 1, 1)
                );
            }
            EMIT(ctx, 
                lsrv64(tmp, tmp, shiftreg),
                bfi(reg, tmp, 0, 8)
            );
            break;
        }
        RA_FreeARMRegister(ctx, mask);
    }

    if (update_mask)
    {
        uint8_t tmp2 = RA_AllocARMRegister(ctx);

        /* C/X condition is already pre-computed. Insert the flags now! */       
        if (update_mask & (SR_C | SR_X)) {
            if ((update_mask & SR_XC) == SR_XC)
            {
                EMIT(ctx, 
                    mov_immed_u16(tmp2, SR_Calt | SR_X, 0),
                    bic_reg(0, cc, tmp2, LSL, 0),
                    orr_reg(tmp2, cc, tmp2, LSL, 0),
                    csel(cc, 0, tmp2, A64_CC_EQ)
                );
            }
            else if ((update_mask & SR_XC) == SR_X)
            {
                EMIT(ctx, 
                    cset(0, A64_CC_NE),
                    bfi(cc, 0, SRB_X, 1)
                );
            }
            else
            {
                EMIT(ctx, 
                    cset(0, A64_CC_NE),
                    bfi(cc, 0, SRB_Calt, 1)
                );
            }

            /* Done with C and/or X */
            update_mask &= ~(SR_XC);
        }

        RA_FreeARMRegister(ctx, tmp2);

        uint8_t clear_mask = update_mask;

        /* Swap C and V flags in immediate */
        if ((clear_mask & 3) != 0 && (clear_mask & 3) < 3)
            clear_mask ^= 3;

        EMIT_ClearFlags(ctx, cc, clear_mask);

        if (update_mask & (SR_Z | SR_N))
        {
            switch(size)
            {
                case 4:
                    EMIT(ctx, cmn_reg(31, reg, LSL, 0));
                    break;
                case 2:
                    EMIT(ctx, cmn_reg(31, tmp, LSL, 16));
                    break;
                case 1:
                    EMIT(ctx, cmn_reg(31, tmp, LSL, 24));
                    break;
            }

            if (update_mask & SR_Z) {
                EMIT(ctx, 
                    b_cc(A64_CC_EQ ^ 1, 2),
                    orr_immed(cc, cc, 1, (32 - SRB_Z) & 31)
                );
            }
            if (update_mask & SR_N) {
                EMIT(ctx, 
                    b_cc(A64_CC_MI ^ 1, 2),
                    orr_immed(cc, cc, 1, (32 - SRB_N) & 31)
                );
            }
        }
    }

    if (tmpptr_1 != NULL) {
        *tmpptr_2 = b_cc(A64_CC_AL, ctx->tc_CodePtr - tmpptr_2);
    }
    else {
        *tmpptr_2 = b_cc(A64_CC_EQ, ctx->tc_CodePtr - tmpptr_2);
    }

    EMIT_AdvancePC(ctx, 2);

    RA_FreeARMRegister(ctx, tmp);

    return 1;
}


static uint32_t EMIT_LSR(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_LSL")));
static uint32_t EMIT_LSL(struct TranslatorContext *ctx, uint16_t opcode)
{

    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t shift = (opcode >> 9) & 7;
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t reg = RA_MapM68kRegister(ctx, opcode & 7);
    uint8_t tmp = RA_AllocARMRegister(ctx);

    RA_SetDirtyM68kRegister(ctx, opcode & 7);

    if (!shift)
        shift = 8;

    if (update_mask & (SR_C | SR_X)) {
        if (direction) {
            switch (size) {
                case 4:
                    EMIT(ctx, tst_immed(reg, 1, shift));
                    break;
                case 2:
                    EMIT(ctx, tst_immed(reg, 1, 16 + shift));
                    break;
                case 1:
                    EMIT(ctx, tst_immed(reg, 1, 31 & (24 + shift)));
                    break;
            }
        }
        else {
            EMIT(ctx, tst_immed(reg, 1, 31 & (33 - shift)));
        }
    }

    if (direction)
    {
        switch (size)
        {
        case 4:
            EMIT(ctx, lsl(reg, reg, shift));
            break;
        case 2:
            EMIT(ctx, 
                lsl(tmp, reg, shift),
                bfi(reg, tmp, 0, 16)
            );
            break;
        case 1:
            EMIT(ctx, 
                lsl(tmp, reg, shift),
                bfi(reg, tmp, 0, 8)
            );
            break;
        }
    }
    else
    {
        switch (size)
        {
        case 4:
            EMIT(ctx, lsr(reg, reg, shift));
            break;
        case 2:
            EMIT(ctx, 
                uxth(tmp, reg),
                lsr(tmp, tmp, shift),
                bfi(reg, tmp, 0, 16)
            );
            break;
        case 1:
            EMIT(ctx, 
                uxtb(tmp, reg),
                lsr(tmp, tmp, shift),
                bfi(reg, tmp, 0, 8)
            );
            break;
        }
    }

    EMIT_AdvancePC(ctx, 2);

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);

        uint8_t alt_flags = update_mask;
        if ((alt_flags & 3) != 0 && (alt_flags & 3) < 3)
            alt_flags ^= 3;

        EMIT_ClearFlags(ctx, cc, alt_flags);

        uint8_t tmp2 = RA_AllocARMRegister(ctx);

        /* C/X condition is already pre-computed. Insert the flags now! */       
        if (update_mask & (SR_C | SR_X)) {
            if ((update_mask & SR_XC) == SR_XC)
            {
                EMIT(ctx, 
                    mov_immed_u16(tmp2, SR_Calt | SR_X, 0),
                    orr_reg(tmp2, cc, tmp2, LSL, 0),
                    csel(cc, cc, tmp2, A64_CC_EQ)
                );
            }
            else if ((update_mask & SR_XC) == SR_X)
            {
                EMIT(ctx, 
                    cset(0, A64_CC_NE),
                    bfi(cc, 0, SRB_X, 1)
                );
            }
            else
            {
                EMIT(ctx, 
                    cset(0, A64_CC_NE),
                    bfi(cc, 0, SRB_Calt, 1)
                );
            }

            /* Done with C and/or X */
            update_mask &= ~(SR_XC);
        }

        RA_FreeARMRegister(ctx, tmp2);

        if (update_mask & (SR_Z | SR_N))
        {
            switch(size)
            {
                case 4:
                    EMIT(ctx, cmn_reg(31, reg, LSL, 0));
                    break;
                case 2:
                    EMIT(ctx, cmn_reg(31, tmp, LSL, 16));
                    break;
                case 1:
                    EMIT(ctx, cmn_reg(31, tmp, LSL, 24));
                    break;
            }

            if (update_mask & SR_Z) {
                EMIT(ctx, 
                    orr_immed(0, cc, 1, (32 - SRB_Z) & 31),
                    csel(cc, 0, cc, A64_CC_EQ)
                );
            }
            if (update_mask & SR_N) {
                EMIT(ctx, 
                    orr_immed(0, cc, 1, (32 - SRB_N) & 31),
                    csel(cc, 0, cc, A64_CC_MI)
                );
            }
        }
    }
    RA_FreeARMRegister(ctx, tmp);

    return 1;
}

static uint32_t EMIT_ROR_reg(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ROL")));
static uint32_t EMIT_ROL_reg(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ROL")));
static uint32_t EMIT_ROR(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ROL")));
static uint32_t EMIT_ROL(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t shift = (opcode >> 9) & 7;
    uint8_t shift_orig = 0xff;
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t regshift = (opcode >> 5) & 1;
    uint8_t reg = RA_MapM68kRegister(ctx, opcode & 7);
    uint8_t tmp = RA_AllocARMRegister(ctx);

    RA_SetDirtyM68kRegister(ctx, opcode & 7);

    if (regshift)
    {
        if (direction)
        {
            uint8_t shift_mod = RA_AllocARMRegister(ctx);
            shift = RA_MapM68kRegister(ctx, shift);

            if (update_mask & SR_C) {
                shift_orig = RA_AllocARMRegister(ctx);
                EMIT(ctx, and_immed(shift_orig, shift, 6, 0));
            }

#if 1
            EMIT(ctx, 
                mov_immed_u16(shift_mod, 32, 0),
                sub_reg(shift_mod, shift_mod, shift, LSL, 0)
            );
#else
            EMIT(ctx, 
                neg_reg(shift_mod, shift, LSL, 0),
                add_immed(shift_mod, shift_mod, 32)
            );
#endif
            shift = shift_mod;
        }
        else
        {
            shift = RA_CopyFromM68kRegister(ctx, shift);
            if (update_mask & SR_C) {
                shift_orig = RA_AllocARMRegister(ctx);
                EMIT(ctx, and_immed(shift_orig, shift, 6, 0));
            }
        }

        switch (size)
        {
            case 4:
                EMIT(ctx, rorv(reg, reg, shift));
                break;
            case 2:
                EMIT(ctx, 
                    mov_reg(tmp, reg),
                    bfi(tmp, reg, 16, 16),
                    rorv(tmp, tmp, shift),
                    bfi(reg, tmp, 0, 16)
                );
                break;
            case 1:
                EMIT(ctx, 
                    mov_reg(tmp, reg),
                    bfi(tmp, reg, 8, 8),
                    bfi(tmp, tmp, 16, 16),
                    rorv(tmp, tmp, shift),
                    bfi(reg, tmp, 0, 8)
                );
                break;
        }

        RA_FreeARMRegister(ctx, shift);
    }
    else
    {
        if (!shift)
            shift = 8;

        if (direction)
        {
            shift = 32 - shift;
        }

        switch (size)
        {
        case 4:
            EMIT(ctx, ror(reg, reg, shift));
            break;
        case 2:
            EMIT(ctx, 
                mov_reg(tmp, reg),
                bfi(tmp, reg, 16, 16),
                ror(tmp, tmp, shift),
                bfi(reg, tmp, 0, 16)
            );
            break;
        case 1:
            EMIT(ctx, 
                mov_reg(tmp, reg),
                bfi(tmp, reg, 8, 8),
                bfi(tmp, tmp, 16, 16),
                ror(tmp, tmp, shift),
                bfi(reg, tmp, 0, 8)
            );
            break;
        }
    }

    EMIT_AdvancePC(ctx, 2);

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);
        switch(size)
        {
            case 4:
                EMIT(ctx, cmn_reg(31, reg, LSL, 0));
                break;
            case 2:
                EMIT(ctx, cmn_reg(31, tmp, LSL, 16));
                break;
            case 1:
                EMIT(ctx, cmn_reg(31, tmp, LSL, 24));
                break;
        }
        uint8_t old_mask = update_mask & SR_C;
        EMIT_GetNZ00(ctx, cc, &update_mask);
        update_mask |= old_mask;

        if (update_mask & SR_Z)
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
        if (update_mask & SR_C) {
            if (regshift) {
                if (!direction)
                    EMIT(ctx, cbz(shift_orig, 3));
                else
                    EMIT(ctx, cbz(shift_orig, 2));
                RA_FreeARMRegister(ctx, shift_orig);
            }
            if (!direction) {
                switch(size) {
                    case 4:
                        EMIT(ctx, 
                            bfxil(tmp, reg, 31, 1),
                            bfi(cc, tmp, 1, 1)
                        );
                        break;
                    case 2:
                        EMIT(ctx, 
                            bfxil(tmp, reg, 15, 1),
                            bfi(cc, tmp, 1, 1)
                        );
                        break;
                    case 1:
                        EMIT(ctx, 
                            bfxil(tmp, reg, 7, 1),
                            bfi(cc, tmp, 1, 1)
                        );
                        break;
                }
            }
            else {
                EMIT(ctx, bfi(cc, reg, 1, 1));
            }
        }

    }
    RA_FreeARMRegister(ctx, tmp);

    return 1;
}

static uint32_t EMIT_ROXR_reg(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ROXL")));
static uint32_t EMIT_ROXL_reg(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ROXL")));
static uint32_t EMIT_ROXR(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_ROXL")));
static uint32_t EMIT_ROXL(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    int dir = opcode & 0x100;
    uint8_t cc = RA_ModifyCC(ctx);

    int size = (opcode >> 6) & 3;
    uint8_t dest = RA_MapM68kRegister(ctx, opcode & 7);
    RA_SetDirtyM68kRegister(ctx, opcode & 7);

    if (opcode & 0x20)
    {
        // REG/REG mode
        uint8_t amount_reg = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
        uint8_t amount = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t tmp2 = RA_AllocARMRegister(ctx);
        uint32_t *tmp_ptr;
        
        // Limit rotate amount to 0..63, depending on size calculate modulo 9, 17, 33, depending on size
        EMIT(ctx, ands_immed(tmp, amount_reg, 6, 0));

        // If Z flag is set, don't bother with further ROXL/ROXR - size 0, no reg change
        // Only update CPU flags in that case
        tmp_ptr = ctx->tc_CodePtr++;

        if (update_mask & SR_NZV) {
            switch (size)
            {
                case 0:
                    EMIT(ctx, cmn_reg(31, dest, LSL, 24));
                    break;
                case 1:
                    EMIT(ctx, cmn_reg(31, dest, LSL, 16));
                    break;
                case 2:
                    EMIT(ctx, cmn_reg(31, dest, LSL, 0));
                    break;
            }

            uint8_t tmp_mask = update_mask;
            EMIT_GetNZ00(ctx, cc, &tmp_mask);
        }

        if (update_mask & SR_C) {
            EMIT(ctx, 
                lsr(0, cc, 4),
                bfi(cc, 0, 1, 1)
            );
        }

        *tmp_ptr = b_cc(A64_CC_NE, ctx->tc_CodePtr - tmp_ptr);
        tmp_ptr = ctx->tc_CodePtr++;

        // Continue calculating modulo
        EMIT(ctx, 
            mov_immed_u16(tmp2, size == 0 ? 9 : (size == 1 ? 17 : 33), 0),
            udiv(amount, tmp, tmp2),
            msub(amount, tmp, amount, tmp2)
        );

        // Copy data from dest register
        switch (size)
        {
            case 0:
                EMIT(ctx, and_immed(tmp, dest, 8, 0));
                break;
            case 1:
                EMIT(ctx, and_immed(tmp, dest, 16, 0));
                break;
            case 2:
                EMIT(ctx, mov_reg(tmp, dest));
                break;
        }
        
        // Fill the temporary register with repetitions of X and dest
        EMIT(ctx, tst_immed(cc, 1, 32 - SRB_X));
        if (dir)
        {
            // Rotate left
            switch (size)
            {
                case 0: // byte
                    EMIT(ctx, 
                        neg_reg(amount, amount, LSR, 0),
                        add_immed(amount, amount, 32),
                        b_cc(A64_CC_EQ, 2),
                        orr_immed(tmp, tmp, 1, 32 - 8),
                        bfi(tmp, tmp, 32 - 9, 9),
                        rorv(tmp, tmp, amount),
                        bfi(dest, tmp, 0, 8)
                    );
                    break;
                
                case 1: // word
                    EMIT(ctx, 
                        neg_reg(amount, amount, LSR, 0),
                        add_immed(amount, amount, 64),
                        b_cc(A64_CC_EQ, 2),
                        orr_immed(tmp, tmp, 1, 32 - 16),
                        bfi64(tmp, tmp, 64 - 17, 17),
                        rorv64(tmp, tmp, amount),
                        bfi(dest, tmp, 0, 16)
                    );
                    break;

                case 2: // long
                    EMIT(ctx, 
                        b_cc(A64_CC_EQ, 2),
                        orr64_immed(tmp, tmp, 1, 32, 1),
                        cbz(amount, 13),
                        neg_reg(amount, amount, LSR, 0),
                        add_immed(amount, amount, 64),
                        cmp_immed(amount, 32),
                        b_cc(A64_CC_EQ, 6),
                        lsl64(tmp, tmp, 31),
                        bfxil64(tmp, tmp, 31, 32),
                        rorv64(tmp, tmp, amount),
                        mov_reg(dest, tmp),
                        b(4),
                        bfi64(tmp, tmp, 33, 10),
                        ror64(tmp, tmp, 1),
                        mov_reg(dest, tmp)
                    );
                    break;

                default:
                    break;
            }
        }
        else
        {
            // Rotate right - pattern in temp register: ...... X | DEST | X | DEST
            switch (size)
            {
                case 0: // byte
                    EMIT(ctx, 
                        b_cc(A64_CC_EQ, 2),
                        orr_immed(tmp, tmp, 1, 32 - 8),
                        bfi(tmp, tmp, 9, 9),
                        rorv(tmp, tmp, amount),
                        bfi(dest, tmp, 0, 8)
                    );
                    break;
                
                case 1: // word
                    EMIT(ctx, 
                        b_cc(A64_CC_EQ, 2),
                        orr_immed(tmp, tmp, 1, 32 - 16),
                        bfi64(tmp, tmp, 17, 17),
                        rorv64(tmp, tmp, amount),
                        bfi(dest, tmp, 0, 16)
                    );
                    break;

                case 2: // long
                    EMIT(ctx, 
                        b_cc(A64_CC_EQ, 2),
                        orr64_immed(tmp, tmp, 1, 64 - 32, 1),
                        cmp_immed(amount, 31),
                        b_cc(A64_CC_HI, 5),
                        bfi64(tmp, tmp, 33, 31),
                        rorv64(tmp, tmp, amount),
                        mov_reg(dest, tmp),
                        b(6),
                        lsr64(tmp, tmp, 32),
                        sub_immed(amount, amount, 32),
                        bfi64(tmp, dest, 1, 32),
                        rorv64(tmp, tmp, amount),
                        mov_reg(dest, tmp)
                    );
                    break;

                default:
                    break;
            }
        }
        
        if (update_mask & SR_NZV) {
            switch (size)
            {
                case 0:
                    EMIT(ctx, cmn_reg(31, tmp, LSL, 24));
                    break;
                case 1:
                    EMIT(ctx, cmn_reg(31, tmp, LSL, 16));
                    break;
                case 2:
                    EMIT(ctx, cmn_reg(31, tmp, LSL, 0));
                    break;
            }

            uint8_t tmp_mask = update_mask;
            EMIT_GetNZ00(ctx, cc, &tmp_mask);
        }

        if (update_mask & SR_XC) {
            switch(size)
            {
                case 0:
                    EMIT(ctx, 
                        bfxil(tmp, tmp, 8, 1),
                        bfi(cc, tmp, 1, 1)
                    );
                    break;
                case 1:
                    EMIT(ctx, 
                        bfxil(tmp, tmp, 16, 1),
                        bfi(cc, tmp, 1, 1)
                    );
                    break;
                case 2:
                    EMIT(ctx, 
                        bfxil64(tmp, tmp, 32, 1),
                        bfi(cc, tmp, 1, 1)
                    );
                    break;
            }
            
            if (update_mask & SR_X) {
                EMIT(ctx, 
                    ror(0, cc, 1),
                    bfi(cc, 0, 4, 1)
                );
            }
        }

        *tmp_ptr = b(ctx->tc_CodePtr - tmp_ptr);

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, tmp2);
        RA_FreeARMRegister(ctx, amount);
    }
    else {
        uint8_t tmp = RA_AllocARMRegister(ctx);
        int amount = (opcode >> 9) & 7;
        if (amount == 0)
            amount = 8;

        if (dir)
        {
            // rotate left
            switch (size)
            {
                // Rotate left byte, 1 to 8 positions
                // temporary register layout
                // X7654321 0....... ........ 76543210
                // After rotation copy the 31th bit into X and C

                case 0: // byte
                    EMIT(ctx, 
                        mov_reg(tmp, dest),
                        bic_immed(tmp, tmp, 1, 1),
                        tbz(cc, SRB_X, 2),
                        orr_immed(tmp, tmp, 1, 1),
                        bfi(tmp, tmp, 31-8, 8),
                        ror(tmp, tmp, 32 - amount),
                        bfi(dest, tmp, 0, 8)
                    );
                    break;
                
                // Rotate left word, 1 to 8 positions
                // temporary register layout
                // Xfedcba9 87654321 fedcba98 76543210
                // After rotation copy the 31th bit into X and C

                case 1: // word
                    EMIT(ctx, 
                        mov_reg(tmp, dest),
                        bic_immed(tmp, tmp, 1, 1),
                        tbz(cc, SRB_X, 2),
                        orr_immed(tmp, tmp, 1, 1),
                        bfi64(tmp, tmp, 31-16, 16),
                        ror(tmp, tmp, 32 - amount),
                        bfi(dest, tmp, 0, 16)
                    );
                    break;

                // Rotate left long, 1 to 8 positions
                // Use 64bit temporary register for the operation
                // bits 64-32: X(1f)(1e)...(00)
                // bits 31-0: source register
                case 2: // long
                    EMIT(ctx, 
                        lsl64(tmp, dest, 31),
                        bic64_immed(tmp, tmp, 1, 1, 1),
                        tbz(cc, SRB_X, 2),
                        orr64_immed(tmp, tmp, 1, 1, 1),
                        bfxil64(tmp, tmp, 31, 32),
                        ror64(tmp, tmp, 64 - amount),
                        mov_reg(dest, tmp)
                    );
                    break;
            }
        }
        else
        {
            // rotate right
            switch (size)
            {
                case 0: // byte
                    EMIT(ctx, 
                        mov_reg(tmp, dest),
                        bic_immed(tmp, tmp, 1, 32 - 8),
                        tbz(cc, SRB_X, 2),
                        orr_immed(tmp, tmp, 1, 32 - 8),
                        bfi(tmp, tmp, 9, 9),
                        ror(tmp, tmp, amount),
                        bfi(dest, tmp, 0, 8)
                    );
                    break;
                case 1: // word
                    EMIT(ctx, 
                        mov_reg(tmp, dest),
                        bic_immed(tmp, tmp, 1, 32 - 16),
                        tbz(cc, SRB_X, 2),
                        orr_immed(tmp, tmp, 1, 32 - 16),
                        bfi64(tmp, tmp, 17, 17),
                        ror64(tmp, tmp, amount),
                        bfi(dest, tmp, 0, 16)
                    );
                    break;
                case 2: // long
                    EMIT(ctx, 
                        lsl64(tmp, dest, 33),
                        bfi64(tmp, dest, 0, 32),
                        tbz(cc, SRB_X, 4),
                        orr64_immed(tmp, tmp, 1, 32, 1),
                        b(2),
                        bic64_immed(tmp, tmp, 1, 32, 1),
                        ror64(tmp, tmp, amount),
                        mov_reg(dest, tmp)
                    );
                    break;
            }
        }

        if (update_mask & SR_NZV) {
            switch (size)
            {
                case 0:
                    EMIT(ctx, cmn_reg(31, tmp, LSL, 24));
                    break;
                case 1:
                    EMIT(ctx, cmn_reg(31, tmp, LSL, 16));
                    break;
                case 2:
                    EMIT(ctx, cmn_reg(31, tmp, LSL, 0));
                    break;
            }

            uint8_t tmp_mask = update_mask;
            EMIT_GetNZ00(ctx, cc, &tmp_mask);
        }

        if (update_mask & SR_XC) {
            if (dir) {
                switch(size)
                {
                    case 0:
                        EMIT(ctx, 
                            bfxil(tmp, tmp, 31, 1),
                            bfi(cc, tmp, 1, 1)
                        );
                        break;
                    case 1:
                        EMIT(ctx, 
                            bfxil(tmp, tmp, 31, 1),
                            bfi(cc, tmp, 1, 1)
                        );
                        break;
                    case 2:
                        EMIT(ctx, 
                            bfxil64(tmp, tmp, 63, 1),
                            bfi(cc, tmp, 1, 1)
                        );
                        break;
                }
            }
            else {
                switch(size)
                {
                    case 0:
                        EMIT(ctx, 
                            bfxil(tmp, tmp, 8, 1),
                            bfi(cc, tmp, 1, 1)
                        );
                        break;
                    case 1:
                        EMIT(ctx, 
                            bfxil(tmp, tmp, 16, 1),
                            bfi(cc, tmp, 1, 1)
                        );
                        break;
                    case 2:
                        EMIT(ctx, 
                            bfxil64(tmp, tmp, 32, 1),
                            bfi(cc, tmp, 1, 1)
                        );
                        break;
                }
            }
            
            if (update_mask & SR_X) {
                EMIT(ctx, 
                    ror(0, cc, 1),
                    bfi(cc, 0, 4, 1)
                );
            }
        }

        RA_FreeARMRegister(ctx, tmp);
    }

    EMIT_AdvancePC(ctx, 2);
    
    return 1;
}

enum BF_OP {
    OP_EOR,
    OP_SET,
    OP_CLR,
    OP_TST,
    OP_INS,
    OP_EXTS,
    OP_EXTU,
    OP_FFO
};

static inline uint32_t EMIT_BFxxx_II(struct TranslatorContext *ctx, uint8_t base, enum BF_OP op, uint8_t Do, uint8_t Dw, uint8_t update_mask, uint8_t data)
{
    uint8_t width = (Dw == 0) ? 32 : Dw;
    uint8_t base_offset = Do >> 3;
    uint8_t bit_offset = Do & 7;
    uint8_t data_reg = 0;
    uint8_t test_reg = 1;
    uint8_t fetched_size = 0;
    uint8_t data_offset = 0;

    /* IF bit offset + width <= 8, fetch a byte */
    if ((bit_offset + width) <= 8)
    {
        EMIT(ctx, ldurb_offset(base, data_reg, base_offset));
        fetched_size = 1;
        data_offset = 56;
    }
    /* bit offset + width <= 16: fetch a word */
    else if ((bit_offset + width) <= 16)
    {
        EMIT(ctx, ldurh_offset(base, data_reg, base_offset));
        fetched_size = 2;
        data_offset = 48;
    }
    /* bit offset + width <= 32: fetch a long */
    else if ((bit_offset + width) <= 32)
    {
        EMIT(ctx, ldur_offset(base, data_reg, base_offset));
        fetched_size = 4;
        data_offset = 32;
    }
    /* Worst case otherwise - fetch 64bit */
    else
    {
        EMIT(ctx, ldur64_offset(base, data_reg, base_offset));
        fetched_size = 8;
    }

    /* For insert mode the inserted data is checked for NZ flags, otherwise existing bitfield */
    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);
        if (op == OP_INS)
        {
            /* Test inserted data */
            EMIT(ctx, cmn_reg(31, data, LSL, 32 - width));
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }
        else if (op != OP_EXTS && op != OP_EXTU && op != OP_FFO)
        {
            /* Test bitfield */
            EMIT(ctx, 
                lsl64(test_reg, data_reg, data_offset + bit_offset),
                ands64_immed(test_reg, test_reg, width, width, 1)
            );
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }
    }

    if (op != OP_TST)
    {
        switch (op)
        {
            case OP_EOR:
                // Exclusive-or all bits
                EMIT(ctx, eor64_immed(data_reg, data_reg, width, width + data_offset + bit_offset, 1));
                break;
                
            case OP_SET:
                // Set all bits
                EMIT(ctx, orr64_immed(data_reg, data_reg, width, width + data_offset + bit_offset, 1));
                break;

            case OP_CLR:
                // Clear all bits
                EMIT(ctx, bic64_immed(data_reg, data_reg, width, width + data_offset + bit_offset, 1));
                break;
            
            case OP_INS:
                switch (fetched_size)
                {
                    case 1:
                        EMIT(ctx, bfi(data_reg, data, 8 - (width + bit_offset), width));
                        break;
                    case 2:
                        EMIT(ctx, bfi(data_reg, data, 16 - (width + bit_offset), width));
                        break;
                    case 4:
                        EMIT(ctx, bfi(data_reg, data, 32 - (width + bit_offset), width));
                        break;
                    case 8:
                        EMIT(ctx, bfi64(data_reg, data, 64 - (width + bit_offset), width));
                        break;
                }
                break;

            case OP_EXTS:
            case OP_EXTU:
                {
                    EMIT(ctx, lsl64(test_reg, data_reg, data_offset + bit_offset));
                    if (update_mask)
                    {
                        uint8_t cc = RA_ModifyCC(ctx);
                        EMIT(ctx, ands64_immed(test_reg, test_reg, width, width, 1));
                        EMIT_GetNZ00(ctx, cc, &update_mask);
                    }
                    if (op == OP_EXTU) {
                        EMIT(ctx, lsr64(data, test_reg, 64 - width));
                    } else {
                        EMIT(ctx, asr64(data, test_reg, 64 - width));
                    }
                }
                break;

            case OP_FFO:
                {
                    /* Shift bitfield to the left */
                    EMIT(ctx, lsl64(test_reg, data_reg, data_offset + bit_offset));
                    if (update_mask)
                    {
                        /* Test bitfield if necessary */
                        uint8_t cc = RA_ModifyCC(ctx);
                        EMIT(ctx, ands64_immed(test_reg, test_reg, width, width, 1));
                        EMIT_GetNZ00(ctx, cc, &update_mask);
                    }

                    /* Set lower bits to 1, so that CLZ can catch such cases */
                    EMIT(ctx, 
                        orr64_immed(test_reg, test_reg, 64 - width, 0, 1),
                        clz64(data, test_reg),
                        add_immed(data, data, Do)
                    );
                }
                break;

            default:
                break;
        }

        if (op != OP_EXTS && op != OP_EXTU && op != OP_FFO)
        {
            /* Store data back... */
            switch (fetched_size)
            {
                case 1:
                    EMIT(ctx, sturb_offset(base, data_reg, base_offset));
                    break;
                case 2:
                    EMIT(ctx, sturh_offset(base, data_reg, base_offset));
                    break;
                case 4:
                    EMIT(ctx, stur_offset(base, data_reg, base_offset));
                    break;
                case 8:
                    EMIT(ctx, stur64_offset(base, data_reg, base_offset));
                    break;
            }
        }
    }

    return 1;
}

static inline uint32_t EMIT_BFxxx_IR(struct TranslatorContext *ctx, uint8_t base, enum BF_OP op, uint8_t Do, uint8_t Dw, uint8_t update_mask, uint8_t data)
{
    uint8_t mask_reg = RA_AllocARMRegister(ctx);
    uint8_t width_reg_orig = Dw;
    uint8_t width_reg = 3;
    uint8_t data_reg = 0;
    uint8_t test_reg = 1;
    uint8_t insert_reg = test_reg;

    uint8_t base_offset = Do >> 3;
    uint8_t bit_offset = Do & 7;
    
    EMIT(ctx, 
        /* Build up the mask from reg value */
        and_immed(width_reg, width_reg_orig, 5, 0),
        cbnz(width_reg, 2),
        mov_immed_u16(width_reg, 32, 0),
        mov_immed_u16(mask_reg, 1, 0),
        lslv64(mask_reg, mask_reg, width_reg),
        sub64_immed(mask_reg, mask_reg, 1),
        
        /* Move mask to the topmost bits of 64-bit mask_reg */
        rbit64(mask_reg, mask_reg),

        /* Fetch the data */
        /* Width == 1? Fetch byte */
        cmp_immed(width_reg, 1),
        b_cc(A64_CC_NE, 4),
        ldurb_offset(base, data_reg, base_offset),
        ror64(data_reg, data_reg, 8),
        b(12),
        /* Width <= 8? Fetch half word */
        cmp_immed(width_reg, 8),
        b_cc(A64_CC_GT, 4),
        ldurh_offset(base, data_reg, base_offset),
        ror64(data_reg, data_reg, 16),
        b(7),
        /* Width <= 24? Fetch long word */
        cmp_immed(width_reg, 24),
        b_cc(A64_CC_GT, 4),
        ldur_offset(base, data_reg, base_offset),
        ror64(data_reg, data_reg, 32),
        b(2),
        ldur64_offset(base, data_reg, base_offset)
    );

    /* In case of INS, prepare the source data accordingly */
    if (op == OP_INS)
    {
        EMIT(ctx, 
            /* Put inserted value to topmost bits of 64bit reg */
            rorv64(insert_reg, data, width_reg),
            /* CLear with insert mask, set condition codes */
            ands64_reg(insert_reg, insert_reg, mask_reg, LSL, 0)
        );
        
        /* If XNZVC needs to be set, do it now */
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }
    }
    else if (op != OP_EXTS && op != OP_EXTU && op != OP_FFO)
    {
        /* Shall bitfield be investigated before? */
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(ctx);

            EMIT(ctx, 
                lsl64(test_reg, data_reg, bit_offset),
                ands64_reg(31, mask_reg, test_reg, LSL, 0)
            );
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }
    }

    /* For all operations other than TST perform the action */
    if (op != OP_TST)
    {
        switch (op)
        {
            case OP_EOR:
                // Exclusive-or all bits
                EMIT(ctx, eor64_reg(data_reg, data_reg, mask_reg, LSR, bit_offset));
                break;
                
            case OP_SET:
                // Set all bits
                EMIT(ctx, orr64_reg(data_reg, data_reg, mask_reg, LSR, bit_offset));
                break;

            case OP_CLR:
                // Clear all bits
                EMIT(ctx, bic64_reg(data_reg, data_reg, mask_reg, LSR, bit_offset));
                break;
            
            case OP_INS:
                EMIT(ctx, 
                    // Clear all bits
                    bic64_reg(data_reg, data_reg, mask_reg, LSR, bit_offset),
                    // Insert data
                    orr64_reg(data_reg, data_reg, insert_reg, LSR, bit_offset)
                );
                break;

            case OP_EXTU:
            case OP_EXTS:
                {
                    /* Shift data left as much as possible */
                    EMIT(ctx, lsl64(test_reg, data_reg, bit_offset));

                    /* If mask is to be updated do it now*/
                    if (update_mask)
                    {
                        uint8_t cc = RA_ModifyCC(ctx);
                        EMIT(ctx, ands64_reg(31, mask_reg, test_reg, LSL, 0));
                        EMIT_GetNZ00(ctx, cc, &update_mask);
                    }

                    /* Compute how far do we shift right: 64 - width */
                    EMIT(ctx, 
                        neg_reg(width_reg, width_reg, LSL, 0),
                        add_immed(width_reg, width_reg, 64)
                    );
                    if (op == OP_EXTS) {
                        EMIT(ctx, asrv64(data, test_reg, width_reg));
                    } else {
                        EMIT(ctx, lsrv64(data, test_reg, width_reg));
                    }
                }
                break;
            
            case OP_FFO:
                {
                    EMIT(ctx, lsl64(test_reg, data_reg, bit_offset));

                    if (update_mask)
                    {
                        uint8_t cc = RA_ModifyCC(ctx);
                        
                        EMIT(ctx, ands64_reg(31, mask_reg, test_reg, LSL, 0));
                        EMIT_GetNZ00(ctx, cc, &update_mask);
                    }

                    // invert mask
                    EMIT(ctx, 
                        mvn64_reg(mask_reg, mask_reg, LSL, 0),
                        orr64_reg(test_reg, test_reg, mask_reg, LSL, 0),
                        clz64(data, test_reg),
                        add_immed(data, data, Do)
                    );
                }
                break;

            default:
                break;
        }

        if (op != OP_EXTS && op != OP_EXTU && op != OP_FFO)
        {
            /* Store the data back */
            EMIT(ctx, 
                /* Width == 1? Fetch byte */
                cmp_immed(width_reg, 1),
                b_cc(A64_CC_NE, 4),
                ror64(data_reg, data_reg, 64 - 8),
                sturb_offset(base, data_reg, base_offset),
                b(12),
                /* Width <= 8? Fetch half word */
                cmp_immed(width_reg, 8),
                b_cc(A64_CC_GT, 4),
                ror64(data_reg, data_reg, 64 - 16),
                sturh_offset(base, data_reg, base_offset),
                b(7),
                /* Width <= 24? Fetch long word */
                cmp_immed(width_reg, 24),
                b_cc(A64_CC_GT, 4),
                ror64(data_reg, data_reg, 32),
                stur_offset(base, data_reg, base_offset),
                b(2),
                stur64_offset(base, data_reg, base_offset)
            );
        }
    }

    RA_FreeARMRegister(ctx, mask_reg);

    return 1;
}

static inline uint32_t EMIT_BFxxx_RI(struct TranslatorContext *ctx, uint8_t base, enum BF_OP op, uint8_t Do, uint8_t Dw, uint8_t update_mask, uint8_t data)
{
    uint8_t mask_reg = RA_AllocARMRegister(ctx);
    uint8_t tmp = 2;
    uint8_t off_reg = RA_AllocARMRegister(ctx);
    uint8_t csel_1 = 0;
    uint8_t csel_2 = 1;
    //uint8_t insert_reg = 3;
    uint8_t width = Dw;
    uint8_t off_reg_orig = Do;
    uint8_t base_orig = base;
    int base_allocated = 0;

    if (width == 0)
        width = 32;

    // Adjust base register according to the offset
    // If base register is m68k register (directly), make a copy and do adjustment there!
    if (RA_IsM68kRegister(base))
    {
        base = RA_AllocARMRegister(ctx);
        base_allocated = 1;
    }

    /* If base was allocated, it will differ from base_orig so the adjustment will be correct in *any* case */
    EMIT(ctx, 
        add_reg(base, base_orig, off_reg_orig, ASR, 3),
        and_immed(off_reg, off_reg_orig, 3, 0)
    );

    if (width == 1)
    {
        EMIT(ctx, 
            // Build up a mask
            orr_immed(mask_reg, 31, width, 24 + width),

            // Load data 
            ldrb_offset(base, tmp, 0),

            // Shift mask to correct position
            lsrv(mask_reg, mask_reg, off_reg)
        );

        /* In case of INS, prepare the source data accordingly */
        if (op == OP_INS)
        {
            /* If XNZVC needs to be set, do it now */
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(ctx);
                uint8_t alt_flags = update_mask;
                if ((alt_flags & 3) != 0 && (alt_flags & 3) < 3)
                    alt_flags ^= 3;

                EMIT_ClearFlags(ctx, cc, alt_flags);

                EMIT(ctx, 
                    ands_immed(31, data, 1, 0),

                    orr_immed(csel_1, cc, 1, 32 - SRB_N),
                    orr_immed(csel_2, cc, 1, 32 - SRB_Z),
                    csel(cc, csel_2, csel_1, A64_CC_EQ)
                );
            }
        }
        else if (op != OP_EXTS && op != OP_EXTU && op != OP_FFO)
        {
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(ctx);
                
                uint8_t alt_flags = update_mask;
                if ((alt_flags & 3) != 0 && (alt_flags & 3) < 3)
                    alt_flags ^= 3;

                EMIT_ClearFlags(ctx, cc, alt_flags);

                EMIT(ctx, 
                    ands_reg(31, tmp, mask_reg, LSL, 0),

                    orr_immed(csel_1, cc, 1, 32 - SRB_N),
                    orr_immed(csel_2, cc, 1, 32 - SRB_Z),
                    csel(cc, csel_2, csel_1, A64_CC_EQ)
                );
            }
        }

        if (op != OP_TST)
        {
            switch(op)
            {
                case OP_EOR:
                    // Exclusive-or all bits
                    EMIT(ctx, eor_reg(tmp, tmp, mask_reg, LSL, 0));
                    break;
                
                case OP_SET:
                    // Set all bits
                    EMIT(ctx, orr_reg(tmp, tmp, mask_reg, LSL, 0));
                    break;

                case OP_CLR:
                    // Clear all bits
                    EMIT(ctx, bic_reg(tmp, tmp, mask_reg, LSL, 0));
                    break;
                
                case OP_INS:
                    /* If update mask was not tested, test data, bit 0 now */
                    if (!update_mask)
                    {
                        EMIT(ctx, ands_immed(31, data, 1, 0));
                    }
                    EMIT(ctx, 
                        bic_reg(csel_1, tmp, mask_reg, LSL, 0),
                        orr_reg(csel_2, tmp, mask_reg, LSL, 0),
                        csel(tmp, csel_1, csel_2, A64_CC_EQ)
                    );
                    break;

                case OP_EXTU:
                case OP_EXTS:
                    {
                        EMIT(ctx, ands_reg(31, tmp, mask_reg, LSL, 0));
                        if (update_mask)
                        {
                            uint8_t cc = RA_ModifyCC(ctx);
                            
                            uint8_t alt_flags = update_mask;
                            if ((alt_flags & 3) != 0 && (alt_flags & 3) < 3)
                                alt_flags ^= 3;

                            EMIT_ClearFlags(ctx, cc, alt_flags);

                            EMIT(ctx, 
                                orr_immed(csel_1, cc, 1, 32 - SRB_N),
                                orr_immed(csel_2, cc, 1, 32 - SRB_Z),
                                csel(cc, csel_2, csel_1, A64_CC_EQ)
                            );
                        }
                        if (op == OP_EXTU) {
                            EMIT(ctx, csinc(data, 31, 31, A64_CC_EQ));
                        } else {
                            EMIT(ctx, csinv(data, 31, 31, A64_CC_EQ));
                        }
                    }
                    break;
                
                case OP_FFO:
                    {
                        EMIT(ctx, ands_reg(31, tmp, mask_reg, LSL, 0));

                        if (update_mask)
                        {
                            uint8_t cc = RA_ModifyCC(ctx);
                            
                            uint8_t alt_flags = update_mask;
                            if ((alt_flags & 3) != 0 && (alt_flags & 3) < 3)
                                alt_flags ^= 3;

                            EMIT_ClearFlags(ctx, cc, alt_flags);

                            EMIT(ctx, 
                                orr_immed(csel_1, cc, 1, 32 - SRB_N),
                                orr_immed(csel_2, cc, 1, 32 - SRB_Z),
                                csel(cc, csel_2, csel_1, A64_CC_EQ)
                            );
                        }

                        EMIT(ctx, 
                            add_immed(csel_2, off_reg_orig, 1),
                            csel(data, csel_2, off_reg_orig, A64_CC_EQ)
                        );
                    }
                    break;

                default:
                    break;
            }

            if (op != OP_EXTS && op != OP_EXTU && op != OP_FFO)
            {
                // Store back
                EMIT(ctx, strb_offset(base, tmp, 0));
            }
        }
    }
    else if (width <= 8)
    {
        EMIT(ctx, 
            // Build up a mask
            orr_immed(mask_reg, 31, width, 16 + width),

            // Load data 
            ldrh_offset(base, tmp, 0)
        );

        if (op == OP_INS)
        {
            if (update_mask)
            {
                uint8_t testreg = RA_AllocARMRegister(ctx);
                uint8_t cc = RA_ModifyCC(ctx);

                // Test input data
                EMIT(ctx, 
                    ror(testreg, data, width),
                    ands_immed(31, testreg, width, width)
                );

                EMIT_GetNZ00(ctx, cc, &update_mask);

                RA_FreeARMRegister(ctx, testreg);
            }
        }
        else if (op != OP_EXTS && op != OP_EXTU && op != OP_FFO)
        {
            if (update_mask)
            {
                uint8_t testreg = RA_AllocARMRegister(ctx);
                uint8_t cc = RA_ModifyCC(ctx);

                EMIT(ctx, 
                    // Shift source to correct position
                    lslv(testreg, tmp, off_reg),
                    lsl(testreg, testreg, 16),

                    // Mask the bitfield, update condition codes
                    ands_reg(31, testreg, mask_reg, LSL, 16)
                );

                EMIT_GetNZ00(ctx, cc, &update_mask);

                RA_FreeARMRegister(ctx, testreg);
            }
        }

        if (op != OP_TST)
        {
            // Shift mask to correct position
            if (op != OP_EXTU && op != OP_EXTS && op != OP_FFO)
            {
                EMIT(ctx, lsrv64(mask_reg, mask_reg, off_reg));
            }

            switch(op)
            {
                case OP_EOR:
                    // Exclusive-or all bits
                    EMIT(ctx, eor_reg(tmp, tmp, mask_reg, LSL, 0));
                    break;
                
                case OP_SET:
                    // Set all bits
                    EMIT(ctx, orr_reg(tmp, tmp, mask_reg, LSL, 0));
                    break;
                
                case OP_CLR:
                    // Clear all bits
                    EMIT(ctx, bic_reg(tmp, tmp, mask_reg, LSL, 0));
                    break;
                
                case OP_INS:
                    EMIT(ctx, 
                        // Rotate source data
                        ror(csel_1, data, 16 + width),
                        lsrv(csel_1, csel_1, off_reg),
                        // Mask non-insert bits
                        and_reg(csel_1, csel_1, mask_reg, LSL, 0),
                        // Clear all bits in target
                        bic_reg(tmp, tmp, mask_reg, LSL, 0),
                        // Merge fields
                        orr_reg(tmp, tmp, csel_1, LSL, 0)
                    );
                    break;

                case OP_EXTU:
                case OP_EXTS:
                    {
                        uint8_t testreg = RA_AllocARMRegister(ctx);
                        
                        // Shift source to correct position
                        EMIT(ctx, 
                            lslv(testreg, tmp, off_reg),
                            lsl(testreg, testreg, 16)
                        );
                        
                        if (update_mask)
                        {
                            uint8_t cc = RA_ModifyCC(ctx);

                            // Mask the bitfield, update condition codes
                            EMIT(ctx, ands_reg(31, testreg, mask_reg, LSL, 16));
                            EMIT_GetNZ00(ctx, cc, &update_mask);
                        }
                        if (op == OP_EXTU) {
                            EMIT(ctx, lsr(data, testreg, 32 - width));
                        } else {
                            EMIT(ctx, asr(data, testreg, 32 - width));
                        }

                        RA_FreeARMRegister(ctx, testreg);
                    }
                    break;
                
                case OP_FFO:
                    {
                        uint8_t testreg = RA_AllocARMRegister(ctx);

                        // Shift source to correct position
                        EMIT(ctx, 
                            lslv(testreg, tmp, off_reg),
                            lsl(testreg, testreg, 16)
                        );

                        if (update_mask)
                        {   
                            uint8_t cc = RA_ModifyCC(ctx);

                            // Mask the bitfield, update condition codes
                            EMIT(ctx, ands_reg(31, testreg, mask_reg, LSL, 16));

                            EMIT_GetNZ00(ctx, cc, &update_mask);   
                        }

                        EMIT(ctx, 
                            orr_immed(testreg, testreg, 32 - width, 0),
                            clz(testreg, testreg),
                            add_reg(data, testreg, off_reg_orig, LSL, 0)
                        );

                        RA_FreeARMRegister(ctx, testreg);
                    }
                    break;

                default:
                    break;
            }

            if (op != OP_EXTS && op != OP_EXTU && op != OP_FFO)
            {
                // Store back
                EMIT(ctx, strh_offset(base, tmp, 0));
            }
        }
    }
    else if (width <= 24)
    {
        EMIT(ctx, 
            // Build up a mask
            orr_immed(mask_reg, 31, width, width),

            // Load data 
            ldr_offset(base, tmp, 0)
        );

        if (op == OP_INS)
        {
            if (update_mask)
            {
                uint8_t testreg = RA_AllocARMRegister(ctx);
                uint8_t cc = RA_ModifyCC(ctx);

                // Test input data
                EMIT(ctx, 
                    ror(testreg, data, width),
                    ands_immed(31, testreg, width, width)
                );

                EMIT_GetNZ00(ctx, cc, &update_mask);

                RA_FreeARMRegister(ctx, testreg);
            }
        }
        else if (op != OP_EXTS && op != OP_EXTU && op != OP_FFO)
        {
            if (update_mask)
            {
                uint8_t testreg = RA_AllocARMRegister(ctx);
                uint8_t cc = RA_ModifyCC(ctx);

                EMIT(ctx, 
                    // Shift source to correct position
                    lslv(testreg, tmp, off_reg),

                    // Mask the bitfield, update condition codes
                    ands_reg(31, testreg, mask_reg, LSL, 0)
                );

                EMIT_GetNZ00(ctx, cc, &update_mask);

                RA_FreeARMRegister(ctx, testreg);
            }
        }

        if (op != OP_TST)
        {
            // Shift mask to correct position
            if (op != OP_EXTU && op != OP_EXTS && op != OP_FFO)
            {
                EMIT(ctx, lsrv64(mask_reg, mask_reg, off_reg));
            }

            switch(op)
            {
                case OP_EOR:
                    // Exclusive-or all bits
                    EMIT(ctx, eor_reg(tmp, tmp, mask_reg, LSL, 0));
                    break;
                
                case OP_SET:
                    // Set all bits
                    EMIT(ctx, orr_reg(tmp, tmp, mask_reg, LSL, 0));
                    break;
                
                case OP_CLR:
                    // Clear all bits
                    EMIT(ctx, bic_reg(tmp, tmp, mask_reg, LSL, 0));
                    break;
                
                case OP_INS:
                    EMIT(ctx, 
                        // Rotate source data
                        ror(csel_1, data, width),
                        lsrv(csel_1, csel_1, off_reg),
                        // Mask non-insert bits
                        and_reg(csel_1, csel_1, mask_reg, LSL, 0),
                        // Clear all bits in target
                        bic_reg(tmp, tmp, mask_reg, LSL, 0),
                        // Merge fields
                        orr_reg(tmp, tmp, csel_1, LSL, 0)
                    );
                    break;
                
                case OP_EXTU:
                case OP_EXTS:
                    {
                        uint8_t testreg = RA_AllocARMRegister(ctx);
                        
                        // Shift source to correct position
                        EMIT(ctx, lslv(testreg, tmp, off_reg));
                        
                        if (update_mask)
                        {
                            uint8_t cc = RA_ModifyCC(ctx);

                            // Mask the bitfield, update condition codes
                            EMIT(ctx, ands_reg(31, testreg, mask_reg, LSL, 0));
                            EMIT_GetNZ00(ctx, cc, &update_mask);
                        }
                        if (op == OP_EXTU) {
                            EMIT(ctx, lsr(data, testreg, 32 - width));
                        } else {
                            EMIT(ctx, asr(data, testreg, 32 - width));
                        }

                        RA_FreeARMRegister(ctx, testreg);
                    }
                    break;
                
                case OP_FFO:
                    {
                        uint8_t testreg = RA_AllocARMRegister(ctx);

                        // Shift source to correct position
                        EMIT(ctx, lslv(testreg, tmp, off_reg));

                        if (update_mask)
                        {    
                            uint8_t cc = RA_ModifyCC(ctx);

                            // Mask the bitfield, update condition codes
                            EMIT(ctx, ands_reg(31, testreg, mask_reg, LSL, 0));

                            EMIT_GetNZ00(ctx, cc, &update_mask);
                        }

                        EMIT(ctx, 
                            orr_immed(testreg, testreg, 32 - width, 0),
                            clz(testreg, testreg),
                            add_reg(data, testreg, off_reg_orig, LSL, 0)
                        );

                        RA_FreeARMRegister(ctx, testreg);
                    }
                    break;

                default:
                    break;
            }

            if (op != OP_EXTS && op != OP_EXTU && op != OP_FFO)
            {
                // Store back
                EMIT(ctx, str_offset(base, tmp, 0));
            }
        }
    }
    else
    {
        EMIT(ctx, 
            // Build up a mask
            orr64_immed(mask_reg, 31, width, width, 1),

            // Load data and shift it left according to reminder in offset reg
            ldr64_offset(base, tmp, 0)
        );

        if (op == OP_INS)
        {
            if (update_mask)
            {
                uint8_t testreg = RA_AllocARMRegister(ctx);
                uint8_t cc = RA_ModifyCC(ctx);

                // Test input data
                if (width != 32)
                {
                    EMIT(ctx, 
                        ror(testreg, data, width),
                        ands_immed(31, testreg, width, width)
                    );
                }
                else
                {
                    EMIT(ctx, cmn_immed(data, 0));
                }

                EMIT_GetNZ00(ctx, cc, &update_mask);

                RA_FreeARMRegister(ctx, testreg);
            }
        }
        else if (op != OP_EXTS && op != OP_EXTU && op != OP_FFO)
        {
            if (update_mask)
            {
                uint8_t testreg = RA_AllocARMRegister(ctx);
                uint8_t cc = RA_ModifyCC(ctx);

                EMIT(ctx, 
                    // Shift source to correct position
                    lslv64(testreg, tmp, off_reg),

                    // Mask the bitfield, update condition codes
                    ands64_reg(31, testreg, mask_reg, LSL, 0)
                );

                EMIT_GetNZ00(ctx, cc, &update_mask);

                RA_FreeARMRegister(ctx, testreg);
            }
        }

        if (op != OP_TST)
        {
            // Shift mask to correct position
            if (op != OP_EXTU && op != OP_EXTS && op != OP_FFO)
            {
                EMIT(ctx, lsrv64(mask_reg, mask_reg, off_reg));
            }

            switch(op)
            {
                case OP_EOR:
                    // Exclusive-or all bits
                    EMIT(ctx, eor64_reg(tmp, tmp, mask_reg, LSL, 0));
                    break;
                
                case OP_SET:
                    // Set all bits
                    EMIT(ctx, orr64_reg(tmp, tmp, mask_reg, LSL, 0));
                    break;

                case OP_CLR:
                    // Clear all bits
                    EMIT(ctx, bic64_reg(tmp, tmp, mask_reg, LSL, 0));
                    break;
                
                case OP_INS:
                    EMIT(ctx, 
                        // Rotate source data
                        ror64(csel_1, data, width),
                        lsrv64(csel_1, csel_1, off_reg),
                        // Mask non-insert bits
                        and64_reg(csel_1, csel_1, mask_reg, LSL, 0),
                        // Clear all bits in target
                        bic64_reg(tmp, tmp, mask_reg, LSL, 0),
                        // Merge fields
                        orr64_reg(tmp, tmp, csel_1, LSL, 0)
                    );
                    break;

                case OP_EXTU:
                case OP_EXTS:
                    {
                        uint8_t testreg = RA_AllocARMRegister(ctx);
                        
                        // Shift source to correct position
                        EMIT(ctx, lslv64(testreg, tmp, off_reg));
                        
                        if (update_mask)
                        {
                            uint8_t cc = RA_ModifyCC(ctx);

                            // Mask the bitfield, update condition codes
                            EMIT(ctx, ands64_reg(31, testreg, mask_reg, LSL, 0));
                            EMIT_GetNZ00(ctx, cc, &update_mask);
                        }
                        if (op == OP_EXTU) {
                            EMIT(ctx, lsr64(data, testreg, 64 - width));
                        } else {
                            EMIT(ctx, asr64(data, testreg, 64 - width));
                        }

                        RA_FreeARMRegister(ctx, testreg);
                    }
                    break;

                case OP_FFO:
                    {
                        uint8_t testreg = RA_AllocARMRegister(ctx);

                        // Shift source to correct position
                        EMIT(ctx, lslv64(testreg, tmp, off_reg));

                        if (update_mask)
                        {    
                            uint8_t cc = RA_ModifyCC(ctx);

                            // Mask the bitfield, update condition codes
                            EMIT(ctx, ands64_reg(31, testreg, mask_reg, LSL, 0));

                            EMIT_GetNZ00(ctx, cc, &update_mask);
                        }

                        EMIT(ctx, 
                            orr64_immed(testreg, testreg, 64 - width, 0, 1),
                            clz64(testreg, testreg),
                            add_reg(data, testreg, off_reg_orig, LSL, 0)
                        );

                        RA_FreeARMRegister(ctx, testreg);
                    }
                    break;

                default:
                    break;
            }

            if (op != OP_EXTS && op != OP_EXTU && op != OP_FFO)
            {
                // Store back
                EMIT(ctx, str64_offset(base, tmp, 0));
            }
        }
    }

    if (base_allocated)
    {
        RA_FreeARMRegister(ctx, base);
    }
    RA_FreeARMRegister(ctx, tmp);
    RA_FreeARMRegister(ctx, mask_reg);
    RA_FreeARMRegister(ctx, off_reg);

    return 1;
}

static inline uint32_t EMIT_BFxxx_RR(struct TranslatorContext *ctx, uint8_t base, enum BF_OP op, uint8_t Do, uint8_t Dw, uint8_t update_mask, uint8_t data)
{
    uint8_t width_reg_orig = Dw;
    uint8_t off_reg_orig = Do;
    uint8_t width_reg = 3;
    uint8_t mask_reg = 2;
    uint8_t test_reg = 1;
    uint8_t insert_reg = test_reg;
    uint8_t off_reg = RA_AllocARMRegister(ctx);
    uint8_t data_reg = 0;
    uint8_t base_orig = base;
    int base_allocated = 0;

    EMIT(ctx, 
        /* Build up the mask from reg value */
        and_immed(width_reg, width_reg_orig, 5, 0),
        cbnz(width_reg, 2),
        mov_immed_u16(width_reg, 32, 0),
        mov_immed_u16(mask_reg, 1, 0),
        lslv64(mask_reg, mask_reg, width_reg),
        sub64_immed(mask_reg, mask_reg, 1),
        /* Move mask to the topmost bits of 64-bit mask_reg */
        rbit64(mask_reg, mask_reg)
    );

    // Adjust base register according to the offset
    // If base register is m68k register (directly), make a copy and do adjustment there!
    if (RA_IsM68kRegister(base))
    {
        base = RA_AllocARMRegister(ctx);
        base_allocated = 1;
    }

    EMIT(ctx, 
        /* If base was allocated, it will differ from base_orig so the adjustment will be correct in *any* case */
        add_reg(base, base_orig, off_reg_orig, ASR, 3),
        and_immed(off_reg, off_reg_orig, 3, 0),
        
        /* Fetch the data */
        /* Width == 1? Fetch byte */
        cmp_immed(width_reg, 1),
        b_cc(A64_CC_NE, 4),
        ldrb_offset(base, data_reg, 0),
        ror64(data_reg, data_reg, 8),
        b(12),
        /* Width <= 8? Fetch half word */
        cmp_immed(width_reg, 8),
        b_cc(A64_CC_GT, 4),
        ldrh_offset(base, data_reg, 0),
        ror64(data_reg, data_reg, 16),
        b(7),
        /* Width <= 24? Fetch long word */
        cmp_immed(width_reg, 24),
        b_cc(A64_CC_GT, 4),
        ldr_offset(base, data_reg, 0),
        ror64(data_reg, data_reg, 32),
        b(2),
        ldr64_offset(base, data_reg, 0)
    );

    /* In case of INS, prepare the source data accordingly */
    if (op == OP_INS)
    {
        EMIT(ctx, 
            /* Put inserted value to topmost bits of 64bit reg */
            rorv64(insert_reg, data, width_reg),
            /* CLear with insert mask, set condition codes */
            ands64_reg(insert_reg, insert_reg, mask_reg, LSL, 0)
        );
        
        /* If XNZVC needs to be set, do it now */
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }
    }
    else if (op != OP_EXTS && op != OP_EXTU && op != OP_FFO)
    {
        /* Shall bitfield be investigated before? */
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(ctx);

            EMIT(ctx, 
                lslv64(test_reg, data_reg, off_reg),
                ands64_reg(31, mask_reg, test_reg, LSL, 0)
            );

            EMIT_GetNZ00(ctx, cc, &update_mask);
        }
    }

    /* For all operations other than TST perform the action */
    if (op != OP_TST)
    {
        // Shift mask to correct position
        if (op != OP_EXTU && op != OP_EXTS && op != OP_FFO)
        {
            EMIT(ctx, lsrv64(mask_reg, mask_reg, off_reg));
        }

        switch (op)
        {
            case OP_EOR:
                // Exclusive-or all bits
                EMIT(ctx, eor64_reg(data_reg, data_reg, mask_reg, LSL, 0));
                break;
                
            case OP_SET:
                // Set all bits
                EMIT(ctx, orr64_reg(data_reg, data_reg, mask_reg, LSL, 0));
                break;

            case OP_CLR:
                // Clear all bits
                EMIT(ctx, bic64_reg(data_reg, data_reg, mask_reg, LSL, 0));
                break;

            case OP_INS:
                EMIT(ctx, 
                    // Move inserted value to location
                    lsrv64(insert_reg, insert_reg, off_reg),
                    // Clear all bits
                    bic64_reg(data_reg, data_reg, mask_reg, LSL, 0),
                    // Insert data
                    orr64_reg(data_reg, data_reg, insert_reg, LSL, 0)
                );
                break;
            
            case OP_EXTU:
            case OP_EXTS:
                {
                    /* Shift data left as much as possible */
                    EMIT(ctx, lslv64(test_reg, data_reg, off_reg));

                    /* If mask is to be updated do it now*/
                    if (update_mask)
                    {
                        uint8_t cc = RA_ModifyCC(ctx);
                        EMIT(ctx, ands64_reg(31, mask_reg, test_reg, LSL, 0));
                        EMIT_GetNZ00(ctx, cc, &update_mask);
                    }

                    /* Compute how far do we shift right: 64 - width */
                    EMIT(ctx, 
                        neg_reg(width_reg, width_reg, LSL, 0),
                        add_immed(width_reg, width_reg, 64)
                    );
                    if (op == OP_EXTS) {
                        EMIT(ctx, asrv64(data, test_reg, width_reg));
                    } else {
                        EMIT(ctx, lsrv64(data, test_reg, width_reg));
                    }
                }
                break;
            
            case OP_FFO:
                {
                    EMIT(ctx, lslv64(test_reg, data_reg, off_reg));

                    if (update_mask)
                    {
                        uint8_t cc = RA_ModifyCC(ctx);
                        
                        EMIT(ctx, ands64_reg(31, mask_reg, test_reg, LSL, 0));
                        EMIT_GetNZ00(ctx, cc, &update_mask);
                    }

                    // invert mask
                    EMIT(ctx, 
                        mvn64_reg(mask_reg, mask_reg, LSL, 0),
                        orr64_reg(test_reg, test_reg, mask_reg, LSL, 0),
                        clz64(test_reg, test_reg),
                        add_reg(data, test_reg, off_reg_orig, LSL, 0)
                    );
                }
                break;

            default:
                break;
        }

        if (op != OP_EXTS && op != OP_EXTU && op != OP_FFO)
        {
            EMIT(ctx, 
                /* Store the data back */
                /* Width == 1? Fetch byte */
                cmp_immed(width_reg, 1),
                b_cc(A64_CC_NE, 4),
                ror64(data_reg, data_reg, 64 - 8),
                strb_offset(base, data_reg, 0),
                b(12),
                /* Width <= 8? Fetch half word */
                cmp_immed(width_reg, 8),
                b_cc(A64_CC_GT, 4),
                ror64(data_reg, data_reg, 64 - 16),
                strh_offset(base, data_reg, 0),
                b(7),
                /* Width <= 24? Fetch long word */
                cmp_immed(width_reg, 24),
                b_cc(A64_CC_GT, 4),
                ror64(data_reg, data_reg, 32),
                str_offset(base, data_reg, 0),
                b(2),
                str64_offset(base, data_reg, 0)
            );
        }
    }

    if (base_allocated)
    {
        RA_FreeARMRegister(ctx, base);
    }
    RA_FreeARMRegister(ctx, width_reg);
    RA_FreeARMRegister(ctx, mask_reg);
    RA_FreeARMRegister(ctx, off_reg);

    return 1;
}

static uint32_t EMIT_BFTST_reg(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t src = RA_MapM68kRegister(ctx, opcode & 7);

    /* Direct offset and width */
    if ((opcode2 & 0x0820) == 0)
    {    
        uint8_t offset = (opcode2 >> 6) & 0x1f;
        uint8_t width = (opcode2) & 0x1f;

        /*
            If offset == 0 and width == 0 the register value from Dn is already extracted bitfield,
            otherwise extract bitfield
        */
        if (offset != 0 || width != 0)
        {
            uint8_t tmp = RA_AllocARMRegister(ctx);

            // Get the source, expand to 64 bit to allow rotating
            EMIT(ctx, 
                lsl64(tmp, src, 32),
                orr64_reg(tmp, tmp, src, LSL, 0)
            );

            // Get width
            if (width == 0) width = 32;

            // Extract bitfield
            EMIT(ctx, sbfx64(tmp, tmp, 64 - (offset + width), width));
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(ctx);
                EMIT(ctx, cmn_reg(31, tmp, LSL, 0));
                EMIT_GetNZ00(ctx, cc, &update_mask);
            }

            RA_FreeARMRegister(ctx, tmp);
        }
        else
        {
            /* Emit empty bftst just in case no flags need to be tested */
            EMIT(ctx, cmn_reg(31, src, LSL, 0));
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(ctx);
                EMIT_GetNZ00(ctx, cc, &update_mask);
            }
        }
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t width_reg = RA_CopyFromM68kRegister(ctx, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t offset = (opcode2 >> 6) & 31;

        EMIT(ctx, 
            // Shift left by offset + 32 bits
            lsl64(tmp, src, 32 + offset),
            orr64_reg(tmp, tmp, src, LSL, offset),

            // Build up a mask
            and_immed(width_reg, width_reg, 5, 0),
            cbnz(width_reg, 2),
            mov_immed_u16(width_reg, 32, 0),
            mov_immed_u16(mask_reg, 1, 0),
            lslv64(mask_reg, mask_reg, width_reg),
            sub64_immed(mask_reg, mask_reg, 1),
            rorv64(mask_reg, mask_reg, width_reg),

            // Mask the bitfield, update condition codes
            ands64_reg(tmp, tmp, mask_reg, LSL, 0)
        );

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, width_reg);
        RA_FreeARMRegister(ctx, mask_reg);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t width = opcode2 & 31;

        EMIT(ctx, and_immed(off_reg, off_reg, 5, 0));

        if (width == 0)
            width = 32;

        EMIT(ctx, 
            // Build up a mask
            orr64_immed(mask_reg, 31, width, width, 1),

            // Load data and shift it left according to reminder in offset reg
            lsl64(tmp, src, 32),
            orr64_reg(tmp, tmp, src, LSL, 0),
            lslv64(tmp, tmp, off_reg),

            // Mask the bitfield, update condition codes
            ands64_reg(tmp, tmp, mask_reg, LSL, 0)
        );

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, mask_reg);
        RA_FreeARMRegister(ctx, off_reg);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_CopyFromM68kRegister(ctx, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);

        EMIT(ctx, 
            and_immed(off_reg, off_reg, 5, 0),

            // Build up a mask
            and_immed(width_reg, width_reg, 5, 0),
            cbnz(width_reg, 2),
            mov_immed_u16(width_reg, 32, 0),
            mov_immed_u16(mask_reg, 1, 0),
            lslv64(mask_reg, mask_reg, width_reg),
            sub64_immed(mask_reg, mask_reg, 1),
            rorv64(mask_reg, mask_reg, width_reg),

            // Load data and shift it left according to reminder in offset reg
            lsl64(tmp, src, 32),
            orr64_reg(tmp, tmp, src, LSL, 0),
            lslv64(tmp, tmp, off_reg),

            // Mask the bitfield, update condition codes
            ands64_reg(tmp, tmp, mask_reg, LSL, 0)
        );
        
        if (update_mask) {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, mask_reg);
        RA_FreeARMRegister(ctx, width_reg);
        RA_FreeARMRegister(ctx, off_reg);
    }

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_BFTST(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t base = 0xff;

    // Get EA address into a temporary register
    EMIT_LoadFromEffectiveAddress(ctx, 0, &base, opcode & 0x3f, &ext_words, 1, NULL);

    // Do == Immed, Dw == immed
    if (!(opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width = opcode2 & 31;
        
        EMIT_BFxxx_II(ctx, base, OP_TST, offset, width, update_mask, -1);
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width_reg = RA_MapM68kRegister(ctx, opcode2 & 7);

        EMIT_BFxxx_IR(ctx, base, OP_TST, offset, width_reg, update_mask, -1);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_MapM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width = opcode2 & 31;

        EMIT_BFxxx_RI(ctx, base, OP_TST, off_reg, width, update_mask, -1);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_MapM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_MapM68kRegister(ctx, opcode2 & 7);

        EMIT_BFxxx_RR(ctx, base, OP_TST, off_reg, width_reg, update_mask, -1);
    }

    RA_FreeARMRegister(ctx, base);

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_BFEXTU_reg(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);

    /*
        IMPORTANT: Although it is not mentioned in 68000 PRM, the bitfield operations on
            source register are in fact rotation instructions. Bitfield is eventually masked,
            but the temporary contents of source operand for the Dn addressing mode are
            actually rotated.
    */
    uint8_t src = RA_MapM68kRegister(ctx, opcode & 7);

    /* Direct offset and width */
    if ((opcode2 & 0x0820) == 0)
    {
        uint8_t dest = RA_MapM68kRegister(ctx, (opcode2 >> 12) & 7);
        uint8_t offset = (opcode2 >> 6) & 0x1f;
        uint8_t width = (opcode2) & 0x1f;
        RA_SetDirtyM68kRegister(ctx, (opcode2 >> 12) & 7);

        // Get width
        if (width == 0) width = 32;

        /*
            If offset == 0 and width == 0 the register value from Dn is already extracted bitfield,
            otherwise extract bitfield
        */
        if (offset != 0 || width != 32)
        {
            uint8_t tmp = RA_AllocARMRegister(ctx);

            EMIT(ctx, 
                // Get the source, expand to 64 bit to allow rotating
                lsl64(tmp, src, 32),
                orr64_reg(tmp, tmp, src, LSL, 0),

                // Extract bitfield
                ubfx64(dest, tmp, 64 - (offset + width), width)
            );

            RA_FreeARMRegister(ctx, tmp);
        }
        else
        {
            EMIT(ctx, mov_reg(dest, src));
        }

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT(ctx, cmn_reg(31, dest, LSL, 32 - width));
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t width_reg = RA_CopyFromM68kRegister(ctx, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t offset = (opcode2 >> 6) & 31;

        EMIT(ctx, 
            // Shift left by offset + 32 bits
            lsl64(tmp, src, 32 + offset),
            orr64_reg(tmp, tmp, src, LSL, offset),

            // Build up a mask
            and_immed(width_reg, width_reg, 5, 0),
            cbnz(width_reg, 2),
            mov_immed_u16(width_reg, 32, 0),
            mov_immed_u16(mask_reg, 1, 0),
            lslv64(mask_reg, mask_reg, width_reg),
            sub64_immed(mask_reg, mask_reg, 1),
            rorv64(mask_reg, mask_reg, width_reg),

            // Mask the bitfield, update condition codes
            ands64_reg(tmp, tmp, mask_reg, LSL, 0),

            // Arithmetic shift right 64-width bits
            mov_immed_u16(mask_reg, 64, 0),
            sub_reg(width_reg, mask_reg, width_reg, LSL, 0),
            lsrv64(tmp, tmp, width_reg),
            
            // Move to destination register
            mov_reg(dest, tmp)
        );

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, width_reg);
        RA_FreeARMRegister(ctx, mask_reg);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t off_reg = RA_CopyFromM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t width = opcode2 & 31;

        EMIT(ctx, and_immed(off_reg, off_reg, 5, 0));
        
        if (width == 0)
            width = 32;

        EMIT(ctx, 
            // Build up a mask
            orr64_immed(mask_reg, 31, width, width, 1),

            // Load data and shift it left according to reminder in offset reg
            lsl64(tmp, src, 32),
            orr64_reg(tmp, tmp, src, LSL, 0),
            lslv64(tmp, tmp, off_reg),

            // Mask the bitfield, update condition codes
            ands64_reg(tmp, tmp, mask_reg, LSL, 0),

            // Arithmetic shift right 64-width bits
            lsr64(tmp, tmp, 64 - width),
            
            // Move to destination register
            mov_reg(dest, tmp)
        );

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, mask_reg);
        RA_FreeARMRegister(ctx, off_reg);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t off_reg = RA_CopyFromM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_CopyFromM68kRegister(ctx, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);

        EMIT(ctx, 
            and_immed(off_reg, off_reg, 5, 0),

            // Build up a mask
            and_immed(width_reg, width_reg, 5, 0),
            cbnz(width_reg, 2),
            mov_immed_u16(width_reg, 32, 0),
            mov_immed_u16(mask_reg, 1, 0),
            lslv64(mask_reg, mask_reg, width_reg),
            sub64_immed(mask_reg, mask_reg, 1),
            rorv64(mask_reg, mask_reg, width_reg),

            // Load data and shift it left according to reminder in offset reg
            lsl64(tmp, src, 32),
            orr64_reg(tmp, tmp, src, LSL, 0),
            lslv64(tmp, tmp, off_reg),

            // Mask the bitfield, update condition codes
            ands64_reg(tmp, tmp, mask_reg, LSL, 0),
            
            // Arithmetic shift right 64-width bits
            mov_immed_u16(off_reg, 64, 0),
            sub_reg(width_reg, off_reg, width_reg, LSL, 0),
            lsrv64(tmp, tmp, width_reg),
            
            // Move to destination register
            mov_reg(dest, tmp)
        );

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, mask_reg);
        RA_FreeARMRegister(ctx, width_reg);
        RA_FreeARMRegister(ctx, off_reg);
    }

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_BFEXTU(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t base = 0xff;

    // Get EA address into a temporary register
    EMIT_LoadFromEffectiveAddress(ctx, 0, &base, opcode & 0x3f, &ext_words, 1, NULL);

    // Do == Immed, Dw == immed
    if (!(opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width = opcode2 & 31;
        
        EMIT_BFxxx_II(ctx, base, OP_EXTU, offset, width, update_mask, dest);
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t width_reg = RA_MapM68kRegisterForWrite(ctx, opcode2 & 7);
        uint8_t offset = (opcode2 >> 6) & 31;

        EMIT_BFxxx_IR(ctx, base, OP_EXTU, offset, width_reg, update_mask, dest);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t off_reg = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 6) & 7);
        uint8_t width = opcode2 & 31;

        EMIT_BFxxx_RI(ctx, base, OP_EXTU, off_reg, width, update_mask, dest);
    }

    // Do == REG, Dw == REG
    if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t off_reg = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_MapM68kRegisterForWrite(ctx, opcode2 & 7);

        EMIT_BFxxx_RR(ctx, base, OP_EXTU, off_reg, width_reg, update_mask, dest);
    }

    RA_FreeARMRegister(ctx, base);

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_BFEXTS_reg(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t src = RA_MapM68kRegister(ctx, opcode & 7);

    /* Direct offset and width */
    if ((opcode2 & 0x0820) == 0)
    {    
        uint8_t dest = RA_MapM68kRegister(ctx, (opcode2 >> 12) & 7);
        uint8_t offset = (opcode2 >> 6) & 0x1f;
        uint8_t width = (opcode2) & 0x1f;

        RA_SetDirtyM68kRegister(ctx, (opcode2 >> 12) & 7);
        /*
            If offset == 0 and width == 0 the register value from Dn is already extracted bitfield,
            otherwise extract bitfield
        */
        if (offset != 0 || width != 0)
        {
            uint8_t tmp = RA_AllocARMRegister(ctx);

            // Get the source, expand to 64 bit to allow rotating
            EMIT(ctx, 
                lsl64(tmp, src, 32),
                orr64_reg(tmp, tmp, src, LSL, 0)
            );

            // Get width
            if (width == 0) width = 32;

            // Extract bitfield
            EMIT(ctx, 
                sbfx64(tmp, tmp, 64 - (offset + width), width),
                mov_reg(dest, tmp)
            );

            RA_FreeARMRegister(ctx, tmp);
        }
        else
        {
            EMIT(ctx, mov_reg(dest, src));
        }

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT(ctx, cmn_reg(31, dest, LSL, 0));
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t width_reg = RA_CopyFromM68kRegister(ctx, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t offset = (opcode2 >> 6) & 31;

        EMIT(ctx, 
            // Shift left by offset + 32 bits
            lsl64(tmp, src, 32 + offset),
            orr64_reg(tmp, tmp, src, LSL, offset),

            // Build up a mask
            and_immed(width_reg, width_reg, 5, 0),
            cbnz(width_reg, 2),
            mov_immed_u16(width_reg, 32, 0),
            mov_immed_u16(mask_reg, 1, 0),
            lslv64(mask_reg, mask_reg, width_reg),
            sub64_immed(mask_reg, mask_reg, 1),
            rorv64(mask_reg, mask_reg, width_reg),

            // Mask the bitfield, update condition codes
            ands64_reg(tmp, tmp, mask_reg, LSL, 0),

            // Arithmetic shift right 64-width bits
            mov_immed_u16(mask_reg, 64, 0),
            sub_reg(width_reg, mask_reg, width_reg, LSL, 0),
            asrv64(tmp, tmp, width_reg),
            
            // Move to destination register
            mov_reg(dest, tmp)
        );

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, width_reg);
        RA_FreeARMRegister(ctx, mask_reg);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t off_reg = RA_CopyFromM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t width = opcode2 & 31;

        EMIT(ctx, and_immed(off_reg, off_reg, 5, 0));

        if (width == 0)
            width = 32;

        EMIT(ctx, 
            // Build up a mask
            orr64_immed(mask_reg, 31, width, width, 1),

            // Load data and shift it left according to reminder in offset reg
            lsl64(tmp, src, 32),
            orr64_reg(tmp, tmp, src, LSL, 0),
            lslv64(tmp, tmp, off_reg),

            // Mask the bitfield, update condition codes
            ands64_reg(tmp, tmp, mask_reg, LSL, 0),

            // Arithmetic shift right 64-width bits
            asr64(tmp, tmp, 64 - width),
            
            // Move to destination register
            mov_reg(dest, tmp)
        );

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, mask_reg);
        RA_FreeARMRegister(ctx, off_reg);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t off_reg = RA_CopyFromM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_CopyFromM68kRegister(ctx, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);

        EMIT(ctx,
            and_immed(off_reg, off_reg, 5, 0),

            // Build up a mask
            and_immed(width_reg, width_reg, 5, 0),
            cbnz(width_reg, 2),
            mov_immed_u16(width_reg, 32, 0),
            mov_immed_u16(mask_reg, 1, 0),
            lslv64(mask_reg, mask_reg, width_reg),
            sub64_immed(mask_reg, mask_reg, 1),
            rorv64(mask_reg, mask_reg, width_reg),

            // Load data and shift it left according to reminder in offset reg
            lsl64(tmp, src, 32),
            orr64_reg(tmp, tmp, src, LSL, 0),
            lslv64(tmp, tmp, off_reg),

            // Mask the bitfield, update condition code
            ands64_reg(tmp, tmp, mask_reg, LSL, 0),
            
            // Arithmetic shift right 64-width bits
            mov_immed_u16(off_reg, 64, 0),
            sub_reg(width_reg, off_reg, width_reg, LSL, 0),
            asrv64(tmp, tmp, width_reg),
            
            // Move to destination register
            mov_reg(dest, tmp)
        );

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, mask_reg);
        RA_FreeARMRegister(ctx, width_reg);
        RA_FreeARMRegister(ctx, off_reg);
    }

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_BFEXTS(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t base = 0xff;

    // Get EA address into a temporary register
    EMIT_LoadFromEffectiveAddress(ctx, 0, &base, opcode & 0x3f, &ext_words, 1, NULL);

    // Do == Immed, Dw == immed
    if (!(opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width = opcode2 & 31;
        
        EMIT_BFxxx_II(ctx, base, OP_EXTS, offset, width, update_mask, dest);
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t width_reg = RA_MapM68kRegisterForWrite(ctx, opcode2 & 7);
        uint8_t offset = (opcode2 >> 6) & 31;

        EMIT_BFxxx_IR(ctx, base, OP_EXTS, offset, width_reg, update_mask, dest);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t off_reg = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 6) & 7);
        uint8_t width = opcode2 & 31;

        EMIT_BFxxx_RI(ctx, base, OP_EXTS, off_reg, width, update_mask, dest);
    }

    // Do == REG, Dw == REG
    if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t off_reg = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_MapM68kRegisterForWrite(ctx, opcode2 & 7);

        EMIT_BFxxx_RR(ctx, base, OP_EXTS, off_reg, width_reg, update_mask, dest);
    }

    RA_FreeARMRegister(ctx, base);

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_BFFFO_reg(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);

    uint8_t src = RA_MapM68kRegister(ctx, opcode & 7);

    /* Direct offset and width */
    if ((opcode2 & 0x0820) == 0)
    {    
        uint8_t offset = (opcode2 >> 6) & 0x1f;
        uint8_t width = (opcode2) & 0x1f;
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);

        /*
            If offset == 0 and width == 0 the register value from Dn is already extracted bitfield,
            otherwise extract bitfield
        */
        if (offset != 0 || width != 0)
        {
            uint8_t tmp = RA_AllocARMRegister(ctx);

            // Get the source, expand to 64 bit to allow rotating
            EMIT(ctx, 
                lsl64(tmp, src, 32 + offset),
                orr64_reg(tmp, tmp, src, LSL, offset)
            );

            // Get width
            if (width == 0) width = 32;

            EMIT(ctx, 
                // Test bitfield and count zeros
                ands64_immed(tmp, tmp, width, width, 1),
                orr64_immed(tmp, tmp, 64 - width, 0, 1),

                // Perform BFFFO counting now
                clz64(dest, tmp),
            
                // Add offset
                add_immed(dest, dest, offset)
            );

            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(ctx);
                EMIT_GetNZ00(ctx, cc, &update_mask);
            }

            RA_FreeARMRegister(ctx, tmp);
        }
        else
        {
            EMIT(ctx, clz(dest, src));
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(ctx);
                EMIT(ctx, cmn_reg(31, src, LSL, 0));
                EMIT_GetNZ00(ctx, cc, &update_mask);
            }
        }
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t width_reg = RA_CopyFromM68kRegister(ctx, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t offset = (opcode2 >> 6) & 31;

        EMIT(ctx, 
            // Shift left by offset + 32 bits
            lsl64(tmp, src, 32 + offset),
            orr64_reg(tmp, tmp, src, LSL, offset),

            // Build up a mask
            and_immed(width_reg, width_reg, 5, 0),
            cbnz(width_reg, 2),
            mov_immed_u16(width_reg, 32, 0),
            mov_immed_u16(mask_reg, 1, 0),
            lslv64(mask_reg, mask_reg, width_reg),
            sub64_immed(mask_reg, mask_reg, 1),
            rorv64(mask_reg, mask_reg, width_reg),

            // Mask the bitfield, update condition codes
            ands64_reg(tmp, tmp, mask_reg, LSL, 0),

            // Invert the mask and orr it with tmp reg
            orn64_reg(tmp, tmp, mask_reg, LSL, 0),
            
            // Perform BFFFO counting now
            clz64(dest, tmp),
            
            // Add offset
            add_immed(dest, dest, offset)
        );

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, width_reg);
        RA_FreeARMRegister(ctx, mask_reg);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_AllocARMRegister(ctx);
        uint8_t off_orig = ((opcode2 >> 6) & 7) == ((opcode2 >> 12) & 7) ? RA_CopyFromM68kRegister(ctx, (opcode2 >> 6) & 7):RA_MapM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t width = opcode2 & 31;

        EMIT(ctx, and_immed(off_reg, off_orig, 5, 0));

        if (width == 0)
            width = 32;

        EMIT(ctx, 
            // Build up a mask
            orr64_immed(mask_reg, 31, width, width, 1),

            // Load data and shift it left according to reminder in offset reg
            lsl64(tmp, src, 32),
            orr64_reg(tmp, tmp, src, LSL, 0),
            lslv64(tmp, tmp, off_reg),

            // Mask the bitfield, update condition codes
            ands64_reg(tmp, tmp, mask_reg, LSL, 0),

            // Invert the mask and orr it with tmp reg
            orn64_reg(tmp, tmp, mask_reg, LSL, 0),
            
            // Perform BFFFO counting now
            clz64(dest, tmp),
            
            // Add offset
            add_reg(dest, dest, off_orig, LSL, 0)
        );

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, mask_reg);
        RA_FreeARMRegister(ctx, off_reg);
        RA_FreeARMRegister(ctx, off_orig);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_AllocARMRegister(ctx);
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t off_orig = ((opcode2 >> 6) & 7) == ((opcode2 >> 12) & 7) ? RA_CopyFromM68kRegister(ctx, (opcode2 >> 6) & 7):RA_MapM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_CopyFromM68kRegister(ctx, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);

        EMIT(ctx, 
            and_immed(off_reg, off_orig, 5, 0),

            // Build up a mask
            and_immed(width_reg, width_reg, 5, 0),
            cbnz(width_reg, 2),
            mov_immed_u16(width_reg, 32, 0),
            mov_immed_u16(mask_reg, 1, 0),
            lslv64(mask_reg, mask_reg, width_reg),
            sub64_immed(mask_reg, mask_reg, 1),
            rorv64(mask_reg, mask_reg, width_reg),

            // Load data and shift it left according to reminder in offset reg
            lsl64(tmp, src, 32),
            orr64_reg(tmp, tmp, src, LSL, 0),
            lslv64(tmp, tmp, off_reg),

            // Mask the bitfield, update condition codes
            ands64_reg(tmp, tmp, mask_reg, LSL, 0),
            
            // Invert the mask and orr it with tmp reg
            orn64_reg(tmp, tmp, mask_reg, LSL, 0),
            
            // Perform BFFFO counting now
            clz64(dest, tmp),
            
            // Add offset
            add_reg(dest, dest, off_orig, LSL, 0)
        );

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_GetNZ00(ctx, cc, &update_mask);
        }

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, mask_reg);
        RA_FreeARMRegister(ctx, width_reg);
        RA_FreeARMRegister(ctx, off_reg);
        RA_FreeARMRegister(ctx, off_orig);
    }

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_BFFFO(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t base = 0xff;

    // Get EA address into a temporary register
    EMIT_LoadFromEffectiveAddress(ctx, 0, &base, opcode & 0x3f, &ext_words, 1, NULL);

    // Do == Immed, Dw == immed
    if (!(opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width = opcode2 & 31;
        
        EMIT_BFxxx_II(ctx, base, OP_FFO, offset, width, update_mask, dest);
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t width_reg = RA_MapM68kRegister(ctx, opcode2 & 7);
        uint8_t offset = (opcode2 >> 6) & 31;

        EMIT_BFxxx_IR(ctx, base, OP_FFO, offset, width_reg, update_mask, dest);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t width = opcode2 & 31;
        uint8_t off_reg = RA_MapM68kRegister(ctx, (opcode2 >> 6) & 7);

        EMIT_BFxxx_RI(ctx, base, OP_FFO, off_reg, width, update_mask, dest);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t dest = RA_MapM68kRegisterForWrite(ctx, (opcode2 >> 12) & 7);
        uint8_t width_reg = RA_MapM68kRegister(ctx, opcode2 & 7);
        uint8_t off_reg = RA_MapM68kRegister(ctx, (opcode2 >> 6) & 7);

        EMIT_BFxxx_RR(ctx, base, OP_FFO, off_reg, width_reg, update_mask, dest);
    }

    RA_FreeARMRegister(ctx, base);

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_BFCHG_reg(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr + 1);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t src = RA_MapM68kRegister(ctx, opcode & 7);

    RA_SetDirtyM68kRegister(ctx, opcode & 7);

    /* Direct offset and width */
    if ((opcode2 & 0x0820) == 0)
    {    
        uint8_t offset = (opcode2 >> 6) & 0x1f;
        uint8_t width = (opcode2) & 0x1f;

        /*
            If offset == 0 and width == 0 the register value from Dn is already extracted bitfield,
            otherwise extract bitfield
        */

        if (offset != 0 || width != 0)
        {
            uint8_t tmp = RA_AllocARMRegister(ctx);

            // Get width
            if (width == 0) width = 32;

            // If condition codes needs to be updated, do it now
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(ctx);
                    
                EMIT(ctx, ror(tmp, src, 31 & (32 - offset)));
                if (width != 32)
                    EMIT(ctx, ands_immed(31, tmp, width, width));
                else
                    EMIT(ctx, cmn_reg(31, tmp, LSL, 0));

                EMIT_GetNZ00(ctx, cc, &update_mask);
            }
            if (width != 32) {
                EMIT(ctx, eor_immed(src, src, width, 31 & (width + offset)));
            }
            else {
                EMIT(ctx, mvn_reg(src, src, LSL, 0));
            }

            RA_FreeARMRegister(ctx, tmp);
        }
        else
        {
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(ctx);
                EMIT(ctx, cmn_reg(31, src, LSL, 0));

                EMIT_GetNZ00(ctx, cc, &update_mask);
            }

            EMIT(ctx, mvn_reg(src, src, LSL, 0));
        }
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t width_reg = RA_CopyFromM68kRegister(ctx, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t offset = (opcode2 >> 6) & 31;

        EMIT(ctx, 
            // Build up a mask
            and_immed(width_reg, width_reg, 5, 0),
            cbnz(width_reg, 2),
            mov_immed_u16(width_reg, 32, 0),
            mov_immed_u16(mask_reg, 1, 0),
            lslv64(mask_reg, mask_reg, width_reg),
            sub64_immed(mask_reg, mask_reg, 1),
            rorv(mask_reg, mask_reg, width_reg)
        );

        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(ctx);
            uint8_t cc = RA_ModifyCC(ctx);

            // If offset != 0, shift left by offset bits
            if (offset != 0)
            {
                EMIT(ctx, 
                    ror(testreg, src, 31 & (32 - offset)),
                    // Mask the bitfield, update condition codes
                    ands_reg(31, testreg, mask_reg, LSL, 0)
                );
            }
            else
            {
                EMIT(ctx, ands_reg(31, src, mask_reg, LSL, 0));
            }
            EMIT_GetNZ00(ctx, cc, &update_mask);

            RA_FreeARMRegister(ctx, testreg);
        }

        // Set the mask bits to zero
        if (offset != 0)
            EMIT(ctx, eor_reg(src, src, mask_reg, ROR, offset));
        else
            EMIT(ctx, eor_reg(src, src, mask_reg, LSL, 0));
       
        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, width_reg);
        RA_FreeARMRegister(ctx, mask_reg);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t width = opcode2 & 31;

        EMIT(ctx, and_immed(off_reg, off_reg, 5, 0));

        if (width == 0)
            width = 32;

        // Build mask
        if (width != 32) {
            EMIT(ctx, orr_immed(mask_reg, 31, width, width));
        }

        // Update condition codes if necessary
        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(ctx);
            uint8_t cc = RA_ModifyCC(ctx);

            EMIT(ctx, 
                mov_immed_u16(testreg, 32, 0),
                sub_reg(testreg, testreg, off_reg, LSL, 0),
                rorv(testreg, src, testreg)
            );

            if (width != 32) {
                EMIT(ctx, ands_immed(31, testreg, width, width));
            }
            else {
                EMIT(ctx, cmn_reg(31, testreg, LSL, 0));
            }

            EMIT_GetNZ00(ctx, cc, &update_mask);

            RA_FreeARMRegister(ctx, testreg);
        }

        if (width != 32) {
            EMIT(ctx, 
                // Shift mask
                rorv(mask_reg, mask_reg, off_reg),

                // Clear bitfield
                eor_reg(src, src, mask_reg, LSL, 0)
            );
        }
        else {
            EMIT(ctx, mvn_reg(src, src, LSL, 0));
        }

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, mask_reg);
        RA_FreeARMRegister(ctx, off_reg);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_CopyFromM68kRegister(ctx, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);

        EMIT(ctx, 
            and_immed(off_reg, off_reg, 5, 0),

            // Build up a mask
            and_immed(width_reg, width_reg, 5, 0),
            cbnz(width_reg, 2),
            mov_immed_u16(width_reg, 32, 0),
            mov_immed_u16(mask_reg, 1, 0),
            lslv64(mask_reg, mask_reg, width_reg),
            sub64_immed(mask_reg, mask_reg, 1),
            rorv(mask_reg, mask_reg, width_reg)
        );

        // Update condition codes if necessary
        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(ctx);
            uint8_t cc = RA_ModifyCC(ctx);

            EMIT(ctx, 
                mov_immed_u16(testreg, 32, 0),
                sub_reg(testreg, testreg, off_reg, LSL, 0),
                rorv(testreg, src, testreg),

                ands_reg(31, testreg, mask_reg, LSL, 0)
            );

            EMIT_GetNZ00(ctx, cc, &update_mask);

            RA_FreeARMRegister(ctx, testreg);
        }

        EMIT(ctx, 
            // Rotate mask to correct position
            rorv(mask_reg, mask_reg, off_reg),

            // Set bits in field to zeros
            eor_reg(src, src, mask_reg, LSL, 0)
        );

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, mask_reg);
        RA_FreeARMRegister(ctx, width_reg);
        RA_FreeARMRegister(ctx, off_reg);
    }

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_BFCHG(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t base = 0xff;

    // Get EA address into a temporary register
    EMIT_LoadFromEffectiveAddress(ctx, 0, &base, opcode & 0x3f, &ext_words, 1, NULL);

    // Do == Immed, Dw == immed
    if (!(opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width = opcode2 & 31;

        EMIT_BFxxx_II(ctx, base, OP_EOR, offset, width, update_mask, -1);
    }
    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width_reg = RA_MapM68kRegister(ctx, opcode2 & 7);

        EMIT_BFxxx_IR(ctx, base, OP_EOR, offset, width_reg, update_mask, -1);
    }
    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_MapM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width = opcode2 & 31;

        EMIT_BFxxx_RI(ctx, base, OP_EOR, off_reg, width, update_mask, -1);
    }
    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_MapM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_MapM68kRegister(ctx, opcode2 & 7);

        EMIT_BFxxx_RR(ctx, base, OP_EOR, off_reg, width_reg, update_mask, -1);
    }

    RA_FreeARMRegister(ctx, base);

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_BFSET_reg(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t src = RA_MapM68kRegister(ctx, opcode & 7);

    RA_SetDirtyM68kRegister(ctx, opcode & 7);

    /* Direct offset and width */
    if ((opcode2 & 0x0820) == 0)
    {    
        uint8_t offset = (opcode2 >> 6) & 0x1f;
        uint8_t width = (opcode2) & 0x1f;

        /*
            If offset == 0 and width == 0 the register value from Dn is already extracted bitfield,
            otherwise extract bitfield
        */

        if (offset != 0 || width != 0)
        {
            uint8_t tmp = RA_AllocARMRegister(ctx);

            // Get width
            if (width == 0) width = 32;

            // If condition codes needs to be updated, do it now
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(ctx);
                    
                EMIT(ctx, ror(tmp, src, 31 & (32 - offset)));
                if (width != 32)
                    EMIT(ctx, ands_immed(31, tmp, width, width));
                else
                    EMIT(ctx, cmn_reg(31, tmp, LSL, 0));

                EMIT_GetNZ00(ctx, cc, &update_mask);
            }
            if (width != 32) {
                EMIT(ctx, orr_immed(src, src, width, 31 & (width + offset)));
            }
            else {
                EMIT(ctx, movn_immed_u16(src, 0, 0));
            }

            RA_FreeARMRegister(ctx, tmp);
        }
        else
        {
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(ctx);
                EMIT(ctx, cmn_reg(31, src, LSL, 0));

                EMIT_GetNZ00(ctx, cc, &update_mask);
            }

            EMIT(ctx, movn_immed_u16(src, 0, 0));
        }
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t width_reg = RA_CopyFromM68kRegister(ctx, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t offset = (opcode2 >> 6) & 31;

        // Build up a mask
        EMIT(ctx, 
            and_immed(width_reg, width_reg, 5, 0),
            cbnz(width_reg, 2),
            mov_immed_u16(width_reg, 32, 0),
            mov_immed_u16(mask_reg, 1, 0),
            lslv64(mask_reg, mask_reg, width_reg),
            sub64_immed(mask_reg, mask_reg, 1),
            rorv(mask_reg, mask_reg, width_reg)
        );

        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(ctx);
            uint8_t cc = RA_ModifyCC(ctx);

            // If offset != 0, shift left by offset bits
            if (offset != 0)
            {
                EMIT(ctx, 
                    ror(testreg, src, 31 & (32 - offset)),
                    // Mask the bitfield, update condition codes
                    ands_reg(31, testreg, mask_reg, LSL, 0)
                );
            }
            else
            {
                EMIT(ctx, ands_reg(31, src, mask_reg, LSL, 0));
            }
            EMIT_GetNZ00(ctx, cc, &update_mask);

            RA_FreeARMRegister(ctx, testreg);
        }

        // Set the mask bits to one
        if (offset != 0)
            EMIT(ctx, orr_reg(src, src, mask_reg, ROR, offset));
        else
            EMIT(ctx, orr_reg(src, src, mask_reg, LSL, 0));
       
        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, width_reg);
        RA_FreeARMRegister(ctx, mask_reg);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t width = opcode2 & 31;

        EMIT(ctx, and_immed(off_reg, off_reg, 5, 0));

        if (width == 0)
            width = 32;

        // Build mask
        if (width != 32) {
            EMIT(ctx, orr_immed(mask_reg, 31, width, width));
        }

        // Update condition codes if necessary
        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(ctx);
            uint8_t cc = RA_ModifyCC(ctx);

            EMIT(ctx, 
                mov_immed_u16(testreg, 32, 0),
                sub_reg(testreg, testreg, off_reg, LSL, 0),
                rorv(testreg, src, testreg)
            );

            if (width != 32) {
                EMIT(ctx, ands_immed(31, testreg, width, width));
            }
            else {
                EMIT(ctx, cmn_reg(31, testreg, LSL, 0));
            }

            EMIT_GetNZ00(ctx, cc, &update_mask);

            RA_FreeARMRegister(ctx, testreg);
        }

        if (width != 32) {
            EMIT(ctx, 
                // Shift mask
                rorv(mask_reg, mask_reg, off_reg),

                // Or with source
                orr_reg(src, src, mask_reg, LSL, 0)
            );
        }
        else {
            EMIT(ctx, movn_immed_u16(src, 0, 0));
        }

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, mask_reg);
        RA_FreeARMRegister(ctx, off_reg);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_CopyFromM68kRegister(ctx, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);

        EMIT(ctx, 
            and_immed(off_reg, off_reg, 5, 0),

            // Build up a mask
            and_immed(width_reg, width_reg, 5, 0),
            cbnz(width_reg, 2),
            mov_immed_u16(width_reg, 32, 0),
            mov_immed_u16(mask_reg, 1, 0),
            lslv64(mask_reg, mask_reg, width_reg),
            sub64_immed(mask_reg, mask_reg, 1),
            rorv(mask_reg, mask_reg, width_reg)
        );

        // Update condition codes if necessary
        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(ctx);
            uint8_t cc = RA_ModifyCC(ctx);

            EMIT(ctx, 
                mov_immed_u16(testreg, 32, 0),
                sub_reg(testreg, testreg, off_reg, LSL, 0),
                rorv(testreg, src, testreg),

                ands_reg(31, testreg, mask_reg, LSL, 0)
            );

            EMIT_GetNZ00(ctx, cc, &update_mask);

            RA_FreeARMRegister(ctx, testreg);
        }

        EMIT(ctx, 
            // Rotate mask to correct position
            rorv(mask_reg, mask_reg, off_reg),

            // Set bits in field to ones
            orr_reg(src, src, mask_reg, LSL, 0)
        );

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, mask_reg);
        RA_FreeARMRegister(ctx, width_reg);
        RA_FreeARMRegister(ctx, off_reg);
    }

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_BFSET(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t base = 0xff;

    // Get EA address into a temporary register
    EMIT_LoadFromEffectiveAddress(ctx, 0, &base, opcode & 0x3f, &ext_words, 1, NULL);

    // Do == Immed, Dw == immed
    if (!(opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width = opcode2 & 31;

        EMIT_BFxxx_II(ctx, base, OP_SET, offset, width, update_mask, -1);
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width_reg = RA_MapM68kRegister(ctx, opcode2 & 7);

        EMIT_BFxxx_IR(ctx, base, OP_SET, offset, width_reg, update_mask, -1);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_MapM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width = opcode2 & 31;

        EMIT_BFxxx_RI(ctx, base, OP_SET, off_reg, width, update_mask, -1);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_MapM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_MapM68kRegister(ctx, opcode2 & 7);

        EMIT_BFxxx_RR(ctx, base, OP_SET, off_reg, width_reg, update_mask, -1);
    }

    RA_FreeARMRegister(ctx, base);

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_BFCLR_reg(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t src = RA_MapM68kRegister(ctx, opcode & 7);

    RA_SetDirtyM68kRegister(ctx, opcode & 7);

    /* Direct offset and width */
    if ((opcode2 & 0x0820) == 0)
    {    
        uint8_t offset = (opcode2 >> 6) & 0x1f;
        uint8_t width = (opcode2) & 0x1f;

        /*
            If offset == 0 and width == 0 the register value from Dn is already extracted bitfield,
            otherwise extract bitfield
        */

        if (offset != 0 || width != 0)
        {
            uint8_t tmp = RA_AllocARMRegister(ctx);

            // Get width
            if (width == 0) width = 32;

            // If condition codes needs to be updated, do it now
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(ctx);
                    
                EMIT(ctx, ror(tmp, src, 31 & (32 - offset)));
                if (width != 32)
                    EMIT(ctx, ands_immed(31, tmp, width, width));
                else
                    EMIT(ctx, cmn_reg(31, tmp, LSL, 0));

                EMIT_GetNZ00(ctx, cc, &update_mask);
            }
            if (width != 32) {
                EMIT(ctx, bic_immed(src, src, width, 31 & (width + offset)));
            }
            else {
                EMIT(ctx, mov_immed_u16(src, 0, 0));
            }

            RA_FreeARMRegister(ctx, tmp);
        }
        else
        {
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(ctx);
                EMIT(ctx, cmn_reg(31, src, LSL, 0));

                EMIT_GetNZ00(ctx, cc, &update_mask);
            }

            EMIT(ctx, mov_immed_u16(src, 0, 0));
        }
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t width_reg = RA_CopyFromM68kRegister(ctx, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t offset = (opcode2 >> 6) & 31;

        // Build up a mask
        EMIT(ctx, 
            and_immed(width_reg, width_reg, 5, 0),
            cbnz(width_reg, 2),
            mov_immed_u16(width_reg, 32, 0),
            mov_immed_u16(mask_reg, 1, 0),
            lslv64(mask_reg, mask_reg, width_reg),
            sub64_immed(mask_reg, mask_reg, 1),
            rorv(mask_reg, mask_reg, width_reg)
        );

        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(ctx);
            uint8_t cc = RA_ModifyCC(ctx);

            // If offset != 0, shift left by offset bits
            if (offset != 0)
            {
                EMIT(ctx, 
                    ror(testreg, src, 31 & (32 - offset)),
                    // Mask the bitfield, update condition codes
                    ands_reg(31, testreg, mask_reg, LSL, 0)
                );
            }
            else
            {
                EMIT(ctx, ands_reg(31, src, mask_reg, LSL, 0));
            }
            EMIT_GetNZ00(ctx, cc, &update_mask);

            RA_FreeARMRegister(ctx, testreg);
        }

        // Set the mask bits to zero
        if (offset != 0)
            EMIT(ctx, bic_reg(src, src, mask_reg, ROR, offset));
        else
            EMIT(ctx, bic_reg(src, src, mask_reg, LSL, 0));
       
        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, width_reg);
        RA_FreeARMRegister(ctx, mask_reg);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t width = opcode2 & 31;

        EMIT(ctx, and_immed(off_reg, off_reg, 5, 0));

        if (width == 0)
            width = 32;

        // Build mask
        if (width != 32) {
            EMIT(ctx, orr_immed(mask_reg, 31, width, width));
        }

        // Update condition codes if necessary
        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(ctx);
            uint8_t cc = RA_ModifyCC(ctx);

            EMIT(ctx, 
                mov_immed_u16(testreg, 32, 0),
                sub_reg(testreg, testreg, off_reg, LSL, 0),
                rorv(testreg, src, testreg)
            );

            if (width != 32) {
                EMIT(ctx, ands_immed(31, testreg, width, width));
            }
            else {
                EMIT(ctx, cmn_reg(31, testreg, LSL, 0));
            }

            EMIT_GetNZ00(ctx, cc, &update_mask);

            RA_FreeARMRegister(ctx, testreg);
        }

        if (width != 32) {
            EMIT(ctx, 
                // Shift mask
                rorv(mask_reg, mask_reg, off_reg),

                // Clear bitfield
                bic_reg(src, src, mask_reg, LSL, 0)
            );
        }
        else {
            EMIT(ctx, mov_immed_u16(src, 0, 0));
        }

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, mask_reg);
        RA_FreeARMRegister(ctx, off_reg);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_CopyFromM68kRegister(ctx, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);

        EMIT(ctx, 
            and_immed(off_reg, off_reg, 5, 0),

            // Build up a mask
            and_immed(width_reg, width_reg, 5, 0),
            cbnz(width_reg, 2),
            mov_immed_u16(width_reg, 32, 0),
            mov_immed_u16(mask_reg, 1, 0),
            lslv64(mask_reg, mask_reg, width_reg),
            sub64_immed(mask_reg, mask_reg, 1),
            rorv(mask_reg, mask_reg, width_reg)
        );

        // Update condition codes if necessary
        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(ctx);
            uint8_t cc = RA_ModifyCC(ctx);

            EMIT(ctx, 
                mov_immed_u16(testreg, 32, 0),
                sub_reg(testreg, testreg, off_reg, LSL, 0),
                rorv(testreg, src, testreg),

                ands_reg(31, testreg, mask_reg, LSL, 0)
            );

            EMIT_GetNZ00(ctx, cc, &update_mask);

            RA_FreeARMRegister(ctx, testreg);
        }

        EMIT(ctx, 
            // Rotate mask to correct position
            rorv(mask_reg, mask_reg, off_reg),

            // Set bits in field to zeros
            bic_reg(src, src, mask_reg, LSL, 0)
        );

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, mask_reg);
        RA_FreeARMRegister(ctx, width_reg);
        RA_FreeARMRegister(ctx, off_reg);
    }

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_BFCLR(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t base = 0xff;

    // Get EA address into a temporary register
    EMIT_LoadFromEffectiveAddress(ctx, 0, &base, opcode & 0x3f, &ext_words, 1, NULL);

    // Do == Immed, Dw == immed
    if (!(opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width = opcode2 & 31;

        EMIT_BFxxx_II(ctx, base, OP_CLR, offset, width, update_mask, -1);
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width_reg = RA_MapM68kRegister(ctx, opcode2 & 7);

        EMIT_BFxxx_IR(ctx, base, OP_CLR, offset, width_reg, update_mask, -1);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_MapM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width = opcode2 & 31;

        EMIT_BFxxx_RI(ctx, base, OP_CLR, off_reg, width, update_mask, -1);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_MapM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_MapM68kRegister(ctx, opcode2 & 7);

        EMIT_BFxxx_RR(ctx, base, OP_CLR, off_reg, width_reg, update_mask, -1);
    }

    RA_FreeARMRegister(ctx, base);

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_BFINS_reg(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t dest = RA_MapM68kRegister(ctx, opcode & 7);
    uint8_t src = RA_MapM68kRegister(ctx, (opcode2 >> 12) & 7);

    RA_SetDirtyM68kRegister(ctx, opcode & 7);

    /* Direct offset and width */
    if ((opcode2 & 0x0820) == 0)
    {    
        uint8_t offset = (opcode2 >> 6) & 0x1f;
        uint8_t width = (opcode2) & 0x1f;

        /*
            If offset == 0 and width == 0 the register value from Dn is already extracted bitfield,
            otherwise extract bitfield
        */

        if (offset != 0 || width != 0)
        {
            uint8_t tmp = RA_AllocARMRegister(ctx);
            uint8_t masked_src = RA_AllocARMRegister(ctx);

            // Get width
            if (width == 0) width = 32;

            // Get source bitfield, clip to requested size
            if (width != 32) {
                EMIT(ctx, ands_immed(masked_src, src, width, 0));
            }
            else {
                EMIT(ctx, mov_reg(masked_src, src));
            }

            // Rotate source bitfield so that the MSB is at offset
            if (((offset + width) & 31) != 0)
                EMIT(ctx, ror(masked_src, masked_src, 31 & (offset + width)));

            // If condition codes needs to be updated, do it now
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(ctx);
                uint8_t testreg = RA_AllocARMRegister(ctx);
                
                if (offset != 0) {
                    EMIT(ctx, 
                        ror(testreg, masked_src, 31 & (32 - offset)),
                        cmn_reg(31, testreg, LSL, 0)
                    );
                }
                else {
                    EMIT(ctx, cmn_reg(31, masked_src, LSL, 0));
                }

                EMIT_GetNZ00(ctx, cc, &update_mask);

                RA_FreeARMRegister(ctx, testreg);
            }

            // Clear destination
            if (width != 32) {
                EMIT(ctx, 
                    bic_immed(dest, dest, width, 31 & (width + offset)),
                    // Insert bitfield
                    orr_reg(dest, dest, masked_src, LSL, 0)
                );
            }
            else {
                EMIT(ctx, mov_reg(dest, masked_src));
            }

            RA_FreeARMRegister(ctx, tmp);
            RA_FreeARMRegister(ctx, masked_src);
        }
        else
        {
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(ctx);
                EMIT(ctx, cmn_reg(31, src, LSL, 0));

                EMIT_GetNZ00(ctx, cc, &update_mask);
            }

            EMIT(ctx, mov_reg(dest, src));
        }
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t masked_src = RA_AllocARMRegister(ctx);
        uint8_t width_reg = RA_CopyFromM68kRegister(ctx, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t offset = (opcode2 >> 6) & 31;

        // Build up a mask and mask out source bitfield
        EMIT(ctx, 
            and_immed(width_reg, width_reg, 5, 0),
            cbnz(width_reg, 2),
            mov_immed_u16(width_reg, 32, 0),
            mov_immed_u16(mask_reg, 1, 0),
            lslv64(mask_reg, mask_reg, width_reg),
            sub64_immed(mask_reg, mask_reg, 1),
            ands_reg(masked_src, src, mask_reg, LSL, 0),
            rorv(mask_reg, mask_reg, width_reg),
            rorv(masked_src, masked_src, width_reg)
        );

        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(ctx);

            EMIT(ctx, cmn_reg(31, masked_src, LSL, 0));

            EMIT_GetNZ00(ctx, cc, &update_mask);
        }

        // Set the mask bits to zero and insert sorce
        if (offset != 0) {
            EMIT(ctx, 
                bic_reg(dest, dest, mask_reg, ROR, offset),
                orr_reg(dest, dest, masked_src, ROR, offset)
            );
        }
        else {
            EMIT(ctx, 
                bic_reg(dest, dest, mask_reg, LSL, 0),
                orr_reg(dest, dest, masked_src, LSL, 0)
            );
        }
       
        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, width_reg);
        RA_FreeARMRegister(ctx, mask_reg);
        RA_FreeARMRegister(ctx, masked_src);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t width = opcode2 & 31;
        uint8_t masked_src = RA_AllocARMRegister(ctx);

        EMIT(ctx, and_immed(off_reg, off_reg, 5, 0));

        if (width == 0)
            width = 32;

        // Get source bitfield, clip to requested size
        if (width != 32) {
            EMIT(ctx, 
                ands_immed(masked_src, src, width, 0),
                ror(masked_src, masked_src, width)
            );
        }
        else {
            EMIT(ctx, mov_reg(masked_src, src));
        }

        // Build mask
        if (width != 32) {
            EMIT(ctx, orr_immed(mask_reg, 31, width, width));
        }

        // Update condition codes if necessary
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(ctx);

            EMIT(ctx, cmn_reg(31, masked_src, LSL, 0));

            EMIT_GetNZ00(ctx, cc, &update_mask);
        }

        if (width != 32) {
            EMIT(ctx, 
                // Shift mask
                rorv(mask_reg, mask_reg, off_reg),
                // Shift source
                rorv(masked_src, masked_src, off_reg),
                // Clear bitfield
                bic_reg(dest, dest, mask_reg, LSL, 0),
                // Insert
                orr_reg(dest, dest, masked_src, LSL, 0)
            );
        }
        else {
            // Width == 32. Just rotate the source into destination
            EMIT(ctx, rorv(dest, masked_src, off_reg));
        }

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, mask_reg);
        RA_FreeARMRegister(ctx, off_reg);
        RA_FreeARMRegister(ctx, masked_src);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_CopyFromM68kRegister(ctx, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(ctx);
        uint8_t masked_src = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);

        EMIT(ctx, 
            and_immed(off_reg, off_reg, 5, 0),

            // Build up a mask and mask out source bitfield
            and_immed(width_reg, width_reg, 5, 0),
            cbnz(width_reg, 2),
            mov_immed_u16(width_reg, 32, 0),
            mov_immed_u16(mask_reg, 1, 0),
            lslv64(mask_reg, mask_reg, width_reg),
            sub64_immed(mask_reg, mask_reg, 1),
            ands_reg(masked_src, src, mask_reg, LSL, 0),
            rorv(mask_reg, mask_reg, width_reg),
            rorv(masked_src, masked_src, width_reg)
        );

        // Update condition codes if necessary
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(ctx);

            EMIT(ctx, cmn_reg(31, masked_src, LSL, 0));

            EMIT_GetNZ00(ctx, cc, &update_mask);
        }

        EMIT(ctx, 
            // Rotate mask to correct position
            rorv(mask_reg, mask_reg, off_reg),
            
            // Rotate source to correct position
            rorv(masked_src, masked_src, off_reg),

            // Set bits in field to zeros
            bic_reg(dest, dest, mask_reg, LSL, 0),

            // Insert field
            orr_reg(dest, dest, masked_src, LSL, 0)
        );

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, mask_reg);
        RA_FreeARMRegister(ctx, width_reg);
        RA_FreeARMRegister(ctx, off_reg);
        RA_FreeARMRegister(ctx, masked_src);
    }

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_BFINS(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t base = 0xff;

    uint8_t src = RA_MapM68kRegister(ctx, (opcode2 >> 12) & 7);

    // Get EA address into a temporary register
    EMIT_LoadFromEffectiveAddress(ctx, 0, &base, opcode & 0x3f, &ext_words, 1, NULL);

    // Do == Immed, Dw == immed
    if (!(opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width = opcode2 & 31;
        
        EMIT_BFxxx_II(ctx, base, OP_INS, offset, width, update_mask, src);
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width_reg = RA_MapM68kRegister(ctx, opcode2 & 7);

        EMIT_BFxxx_IR(ctx, base, OP_INS, offset, width_reg, update_mask, src);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_MapM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width = opcode2 & 31;

        EMIT_BFxxx_RI(ctx, base, OP_INS, off_reg, width, update_mask, src);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_MapM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_MapM68kRegister(ctx, opcode2 & 7);

        EMIT_BFxxx_RR(ctx, base, OP_INS, off_reg, width_reg, update_mask, src);
    }

    RA_FreeARMRegister(ctx, base);

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static struct OpcodeDef InsnTable[4096] = {
	[00000 ... 00007] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 1 },  //immediate 8, Byte, Dn
	[00010 ... 00017] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 1 },
	[00020 ... 00027] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00030 ... 00037] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 1 },
	[00040 ... 00047] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D0
	[00050 ... 00057] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00060 ... 00067] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00070 ... 00077] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 1 },
	[00100 ... 00107] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 2 },  //immediate 8, Word, Dn
	[00110 ... 00117] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 2 },
	[00120 ... 00127] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00130 ... 00137] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 2 },
	[00140 ... 00147] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D0
	[00150 ... 00157] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00160 ... 00167] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00170 ... 00177] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 2 },
	[00200 ... 00207] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 4 },  //immediate 8, Long, Dn
	[00210 ... 00217] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 4 },
	[00220 ... 00227] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00230 ... 00237] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 4 },
	[00240 ... 00247] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D0
	[00250 ... 00257] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00260 ... 00267] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00270 ... 00277] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 4 },

	[01000 ... 01007] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 1 },  //immediate 1, Byte, Dn
	[01010 ... 01017] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 1 },
	[01020 ... 01027] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01030 ... 01037] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 1 },
	[01040 ... 01047] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D1
	[01050 ... 01057] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01060 ... 01067] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01070 ... 01077] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 1 },
	[01100 ... 01107] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 2 },  //immediate 1, Word, Dn
	[01110 ... 01117] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 2 },
	[01120 ... 01127] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01130 ... 01137] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 2 },
	[01140 ... 01147] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D1
	[01150 ... 01157] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01160 ... 01167] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01170 ... 01177] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 2 },
	[01200 ... 01207] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 4 },  //immediate 1, Long, Dn
	[01210 ... 01217] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 4 },
	[01220 ... 01227] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01230 ... 01237] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 4 },
	[01240 ... 01247] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D1
	[01250 ... 01257] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01260 ... 01267] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01270 ... 01277] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 4 },

	[02000 ... 02007] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 1 },  //immediate 2, Byte, Dn
	[02010 ... 02017] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 1 },
	[02020 ... 02027] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02030 ... 02037] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 1 },
	[02040 ... 02047] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D2
	[02050 ... 02057] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02060 ... 02067] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02070 ... 02077] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 1 },
	[02100 ... 02107] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 2 },  //immediate 2, Word, Dn
	[02110 ... 02117] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 2 },
	[02120 ... 02127] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02130 ... 02137] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 2 },
	[02140 ... 02147] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D2
	[02150 ... 02157] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02160 ... 02167] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02170 ... 02177] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 2 },
	[02200 ... 02207] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 4 },  //immediate 2, Long, Dn
	[02210 ... 02217] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 4 },
	[02220 ... 02227] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02230 ... 02237] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 4 },
	[02240 ... 02247] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D2
	[02250 ... 02257] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02260 ... 02267] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02270 ... 02277] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 4 },

	[03000 ... 03007] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 1 },  //immediate 3, Byte, Dn
	[03010 ... 03017] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 1 },
	[03020 ... 03027] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03030 ... 03037] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 1 },
	[03040 ... 03047] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D3
	[03050 ... 03057] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03060 ... 03067] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03070 ... 03077] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 1 },
	[03100 ... 03107] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 2 },  //immediate 3, Word, Dn
	[03110 ... 03117] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 2 },
	[03120 ... 03127] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03130 ... 03137] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 2 },
	[03140 ... 03147] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D3
	[03150 ... 03157] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03160 ... 03167] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03170 ... 03177] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 2 },
	[03200 ... 03207] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 4 },  //immediate 3, Long, Dn
	[03210 ... 03217] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 4 },
	[03220 ... 03227] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03230 ... 03237] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 4 },
	[03240 ... 03247] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D3
	[03250 ... 03257] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03260 ... 03267] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03270 ... 03277] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 4 },

	[04000 ... 04007] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 1 },  //immediate 4, Byte, Dn
	[04010 ... 04017] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 1 },
	[04020 ... 04027] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04030 ... 04037] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 1 },
	[04040 ... 04047] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D4
	[04050 ... 04057] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04060 ... 04067] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04070 ... 04077] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 1 },
	[04100 ... 04107] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 2 },  //immediate 4, Word, Dn
	[04110 ... 04117] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 2 },
	[04120 ... 04127] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04130 ... 04137] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 2 },
	[04140 ... 04147] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D4
	[04150 ... 04157] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04160 ... 04167] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04170 ... 04177] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 2 },
	[04200 ... 04207] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 4 },  //immediate 4, Long, Dn
	[04210 ... 04217] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 4 },
	[04220 ... 04227] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04230 ... 04237] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 4 },
	[04240 ... 04247] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D4
	[04250 ... 04257] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04260 ... 04267] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04270 ... 04277] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 4 },

	[05000 ... 05007] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 1 },  //immediate 5, Byte, Dn
	[05010 ... 05017] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 1 },
	[05020 ... 05027] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05030 ... 05037] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 1 },
	[05040 ... 05047] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D5
	[05050 ... 05057] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05060 ... 05067] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05070 ... 05077] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 1 },
	[05100 ... 05107] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 2 },  //immediate 5, Word, Dn
	[05110 ... 05117] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 2 },
	[05120 ... 05127] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05130 ... 05137] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 2 },
	[05140 ... 05147] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D5
	[05150 ... 05157] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05160 ... 05167] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05170 ... 05177] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 2 },
	[05200 ... 05207] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 4 },  //immediate 5, Long, Dn
	[05210 ... 05217] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 4 },
	[05220 ... 05227] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05230 ... 05237] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 4 },
	[05240 ... 05247] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D5
	[05250 ... 05257] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05260 ... 05267] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05270 ... 05277] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 4 },

	[06000 ... 06007] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 1 },  //immediate 6, Byte, Dn
	[06010 ... 06017] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 1 },
	[06020 ... 06027] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06030 ... 06037] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 1 },
	[06040 ... 06047] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D6
	[06050 ... 06057] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06060 ... 06067] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06070 ... 06077] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 1 },
	[06100 ... 06107] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 2 },  //immediate 6, Word, Dn
	[06110 ... 06117] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 2 },
	[06120 ... 06127] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06130 ... 06137] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 2 },
	[06140 ... 06147] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D6
	[06150 ... 06157] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06160 ... 06167] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06170 ... 06177] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 2 },
	[06200 ... 06207] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 4 },  //immediate 6, Long, Dn
	[06210 ... 06217] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 4 },
	[06220 ... 06227] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06230 ... 06237] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 4 },
	[06240 ... 06247] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D6
	[06250 ... 06257] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06260 ... 06267] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06270 ... 06277] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 4 },

	[07000 ... 07007] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 1 },  //immediate 7, Byte, Dn
	[07010 ... 07017] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 1 },
	[07020 ... 07027] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07030 ... 07037] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 1 },
	[07040 ... 07047] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D7
	[07050 ... 07057] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07060 ... 07067] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07070 ... 07077] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 1 },
	[07100 ... 07107] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 2 },  //immediate 7, Word, Dn
	[07110 ... 07117] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 2 },
	[07120 ... 07127] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07130 ... 07137] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 2 },
	[07140 ... 07147] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D7
	[07150 ... 07157] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07160 ... 07167] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR , 1, 0, 2},
	[07170 ... 07177] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 2 },
	[07200 ... 07207] = { EMIT_ASR, NULL, 0, SR_CCR, 1, 0, 4 },  //immediate 7, Long, Dn
	[07210 ... 07217] = { EMIT_LSR, NULL, 0, SR_CCR, 1, 0, 4 },
	[07220 ... 07227] = { EMIT_ROXR, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07230 ... 07237] = { EMIT_ROR, NULL, 0, SR_NZVC, 1, 0, 4 },
	[07240 ... 07247] = { EMIT_ASR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D7
	[07250 ... 07257] = { EMIT_LSR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07260 ... 07267] = { EMIT_ROXR_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07270 ... 07277] = { EMIT_ROR_reg, NULL, 0, SR_NZVC, 1, 0, 4 },

	[00320 ... 00371] = { EMIT_ASR_mem, NULL, 0, SR_CCR, 1, 1, 2 },  //Shift #1, <ea> (memory only)
	[01320 ... 01371] = { EMIT_LSR_mem, NULL, 0, SR_CCR, 1, 1, 2 },
	[02320 ... 02371] = { EMIT_ROXR_mem, NULL, SR_X, SR_CCR, 1, 1, 2 },
	[03320 ... 03371] = { EMIT_ROR_mem, NULL, 0, SR_NZVC, 1, 1, 2 },

	[00400 ... 00407] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 1 },  //immediate 8, Byte, Dn
	[00410 ... 00417] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 1 },
	[00420 ... 00427] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00430 ... 00437] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 1 },
	[00440 ... 00447] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D0
	[00450 ... 00457] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00460 ... 00467] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00470 ... 00477] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 1 },
	[00500 ... 00507] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 2 },  //immediate 8, Word, Dn
	[00510 ... 00517] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 2 },
	[00520 ... 00527] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00530 ... 00537] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 2 },
	[00540 ... 00547] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D0
	[00550 ... 00557] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00560 ... 00567] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00570 ... 00577] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 2 },
	[00600 ... 00607] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 4 },  //immediate 8, Long, Dn
	[00610 ... 00617] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 4 },
	[00620 ... 00627] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00630 ... 00637] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 4 },
	[00640 ... 00647] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D0
	[00650 ... 00657] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00660 ... 00667] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00670 ... 00677] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 4 },

	[01400 ... 01407] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 1 },  //immediate 1, Byte, Dn
	[01410 ... 01417] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 1 },
	[01420 ... 01427] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01430 ... 01437] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 1 },
	[01440 ... 01447] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D1
	[01450 ... 01457] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01460 ... 01467] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01470 ... 01477] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 1 },
	[01500 ... 01507] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 2 },  //immediate 1, Word, Dn
	[01510 ... 01517] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 2 },
	[01520 ... 01527] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01530 ... 01537] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 2 },
	[01540 ... 01547] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D1
	[01550 ... 01557] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01560 ... 01567] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01570 ... 01577] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 2 },
	[01600 ... 01607] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 4 },  //immediate 1, Long, Dn
	[01610 ... 01617] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 4 },
	[01620 ... 01627] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01630 ... 01637] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 4 },
	[01640 ... 01647] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D1
	[01650 ... 01657] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01660 ... 01667] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01670 ... 01677] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 4 },

	[02400 ... 02407] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 1 },  //immediate 2, Byte, Dn
	[02410 ... 02417] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 1 },
	[02420 ... 02427] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02430 ... 02437] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 1 },
	[02440 ... 02447] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D2
	[02450 ... 02457] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02460 ... 02467] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02470 ... 02477] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 1 },
	[02500 ... 02507] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 2 },  //immediate 2, Word, Dn
	[02510 ... 02517] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 2 },
	[02520 ... 02527] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02530 ... 02537] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 2 },
	[02540 ... 02547] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D2
	[02550 ... 02557] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02560 ... 02567] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02570 ... 02577] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 2 },
	[02600 ... 02607] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 4 },  //immediate 2, Long, Dn
	[02610 ... 02617] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 4 },
	[02620 ... 02627] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02630 ... 02637] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 4 },
	[02640 ... 02647] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D2
	[02650 ... 02657] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02660 ... 02667] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02670 ... 02677] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 4 },

	[03400 ... 03407] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 1 },  //immediate 3, Byte, Dn
	[03410 ... 03417] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 1 },
	[03420 ... 03427] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03430 ... 03437] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 1 },
	[03440 ... 03447] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D3
	[03450 ... 03457] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03460 ... 03467] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03470 ... 03477] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 1 },
	[03500 ... 03507] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 2 },  //immediate 3, Word, Dn
	[03510 ... 03517] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 2 },
	[03520 ... 03527] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03530 ... 03537] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 2 },
	[03540 ... 03547] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D3
	[03550 ... 03557] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03560 ... 03567] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03570 ... 03577] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 2 },
	[03600 ... 03607] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 4 },  //immediate 3, Long, Dn
	[03610 ... 03617] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 4 },
	[03620 ... 03627] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03630 ... 03637] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 4 },
	[03640 ... 03647] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D3
	[03650 ... 03657] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03660 ... 03667] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03670 ... 03677] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 4 },

	[04400 ... 04407] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 1 },  //immediate 4, Byte, Dn
	[04410 ... 04417] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 1 },
	[04420 ... 04427] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04430 ... 04437] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 1 },
	[04440 ... 04447] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D4
	[04450 ... 04457] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04460 ... 04467] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04470 ... 04477] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 1 },
	[04500 ... 04507] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 2 },  //immediate 4, Word, Dn
	[04510 ... 04517] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 2 },
	[04520 ... 04527] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04530 ... 04537] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 2 },
	[04540 ... 04547] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D4
	[04550 ... 04557] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04560 ... 04567] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04570 ... 04577] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 2 },
	[04600 ... 04607] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 4 },  //immediate 4, Long, Dn
	[04610 ... 04617] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 4 },
	[04620 ... 04627] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04630 ... 04637] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 4 },
	[04640 ... 04647] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D4
	[04650 ... 04657] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04660 ... 04667] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04670 ... 04677] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 4 },

	[05400 ... 05407] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 1 },  //immediate 5, Byte, Dn
	[05410 ... 05417] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 1 },
	[05420 ... 05427] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05430 ... 05437] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 1 },
	[05440 ... 05447] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D5
	[05450 ... 05457] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05460 ... 05467] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05470 ... 05477] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 1 },
	[05500 ... 05507] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 2 },  //immediate 5, Word, Dn
	[05510 ... 05517] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 2 },
	[05520 ... 05527] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05530 ... 05537] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 2 },
	[05540 ... 05547] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D5
	[05550 ... 05557] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05560 ... 05567] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05570 ... 05577] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 2 },
	[05600 ... 05607] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 4 },  //immediate 5, Long, Dn
	[05610 ... 05617] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 4 },
	[05620 ... 05627] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05630 ... 05637] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 4 },
	[05640 ... 05647] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D5
	[05650 ... 05657] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05660 ... 05667] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05670 ... 05677] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 4 },

	[06400 ... 06407] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 1 },  //immediate 6, Byte, Dn
	[06410 ... 06417] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 1 },
	[06420 ... 06427] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06430 ... 06437] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 1 },
	[06440 ... 06447] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D6
	[06450 ... 06457] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06460 ... 06467] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06470 ... 06477] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 1 },
	[06500 ... 06507] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 2 },  //immediate 6, Word, Dn
	[06510 ... 06517] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 2 },
	[06520 ... 06527] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06530 ... 06537] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 2 },
	[06540 ... 06547] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D6
	[06550 ... 06557] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06560 ... 06567] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06570 ... 06577] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 2 },
	[06600 ... 06607] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 4 },  //immediate 6, Long, Dn
	[06610 ... 06617] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 4 },
	[06620 ... 06627] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06630 ... 06637] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 4 },
	[06640 ... 06647] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D6
	[06650 ... 06657] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06660 ... 06667] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06670 ... 06677] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 4 },

	[07400 ... 07407] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 1 },  //immediate 7, Byte, Dn
	[07410 ... 07417] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 1 },
	[07420 ... 07427] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07430 ... 07437] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 1 },
	[07440 ... 07447] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D7
	[07450 ... 07457] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07460 ... 07467] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07470 ... 07477] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 1 },
	[07500 ... 07507] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 2 },  //immediate 7, Word, Dn
	[07510 ... 07517] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 2 },
	[07520 ... 07527] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07530 ... 07537] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 2 },
	[07540 ... 07547] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D7
	[07550 ... 07557] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07560 ... 07567] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07570 ... 07577] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 2 },
	[07600 ... 07607] = { EMIT_ASL, NULL, 0, SR_CCR, 1, 0, 4 },  //immediate 7, Long, Dn
	[07610 ... 07617] = { EMIT_LSL, NULL, 0, SR_CCR, 1, 0, 4 },
	[07620 ... 07627] = { EMIT_ROXL, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07630 ... 07637] = { EMIT_ROL, NULL, 0, SR_NZVC, 1, 0, 4 },
	[07640 ... 07647] = { EMIT_ASL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D7
	[07650 ... 07657] = { EMIT_LSL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07660 ... 07667] = { EMIT_ROXL_reg, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07670 ... 07677] = { EMIT_ROL_reg, NULL, 0, SR_NZVC, 1, 0, 4 },

	[00720 ... 00771] = { EMIT_ASL_mem, NULL, 0, SR_CCR, 1, 1, 2 },  //Shift #1, <ea> (memory only)
	[01720 ... 01771] = { EMIT_LSL_mem, NULL, 0, SR_CCR, 1, 1, 2 },
	[02720 ... 02771] = { EMIT_ROXL_mem, NULL, SR_X, SR_CCR, 1, 1, 2 },
	[03720 ... 03771] = { EMIT_ROL_mem, NULL, 0, SR_NZVC, 1, 1, 2 },

	[04300 ... 04307] = { EMIT_BFTST_reg, NULL, 0, SR_NZVC, 2, 0, 0 },
	[04320 ... 04327] = { EMIT_BFTST, NULL, 0, SR_NZVC, 2, 0, 0 },
	[04350 ... 04373] = { EMIT_BFTST, NULL, 0, SR_NZVC, 2, 1, 0 },

	[05300 ... 05307] = { EMIT_BFCHG_reg, NULL, 0, SR_NZVC, 2, 0, 0 },
	[05320 ... 05327] = { EMIT_BFCHG, NULL, 0, SR_NZVC, 2, 0, 0 },
	[05350 ... 05371] = { EMIT_BFCHG, NULL, 0, SR_NZVC, 2, 1, 0 },

	[06300 ... 06307] = { EMIT_BFCLR_reg, NULL, 0, SR_NZVC, 2, 0, 0 },
	[06320 ... 06327] = { EMIT_BFCLR, NULL, 0, SR_NZVC, 2, 0, 0 },
	[06350 ... 06371] = { EMIT_BFCLR, NULL, 0, SR_NZVC, 2, 1, 0 },

	[07300 ... 07307] = { EMIT_BFSET_reg, NULL, 0, SR_NZVC, 2, 0, 0 },
	[07320 ... 07327] = { EMIT_BFSET, NULL, 0, SR_NZVC, 2, 0, 0 },
	[07350 ... 07371] = { EMIT_BFSET, NULL, 0, SR_NZVC, 2, 1, 0 },

	[04700 ... 04707] = { EMIT_BFEXTU_reg, NULL, 0, SR_NZVC, 2, 0, 0 },
	[04720 ... 04727] = { EMIT_BFEXTU, NULL, 0, SR_NZVC, 2, 0, 0 },
	[04750 ... 04773] = { EMIT_BFEXTU, NULL, 0, SR_NZVC, 2, 1, 0 },

	[05700 ... 05707] = { EMIT_BFEXTS_reg, NULL, 0, SR_NZVC, 2, 0, 0 },
	[05720 ... 05727] = { EMIT_BFEXTS, NULL, 0, SR_NZVC, 2, 0, 0 },
	[05750 ... 05773] = { EMIT_BFEXTS, NULL, 0, SR_NZVC, 2, 1, 0 },

	[06700 ... 06707] = { EMIT_BFFFO_reg, NULL, 0, SR_NZVC, 2, 0, 0 },
	[06720 ... 06727] = { EMIT_BFFFO, NULL, 0, SR_NZVC, 2, 0, 0 },
	[06750 ... 06773] = { EMIT_BFFFO, NULL, 0, SR_NZVC, 2, 1, 0 },

	[07700 ... 07707] = { EMIT_BFINS_reg, NULL, 0, SR_NZVC, 2, 0, 0 },
	[07720 ... 07727] = { EMIT_BFINS, NULL, 0, SR_NZVC, 2, 0, 0 },
	[07750 ... 07771] = { EMIT_BFINS, NULL, 0, SR_NZVC, 2, 1, 0 },
};

uint32_t EMIT_lineE(struct TranslatorContext *ctx)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr++);

    /* Special case: the combination of RO(R/L).W #8, Dn; SWAP Dn; RO(R/L).W, Dn
        this is replaced by REV instruction */
    if (((opcode & 0xfef8) == 0xe058) &&
        cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[0]) == (0x4840 | (opcode & 7)) &&
        (cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[1]) & 0xfeff) == (opcode & 0xfeff))
    {
        uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
        uint8_t reg = RA_MapM68kRegister(ctx, opcode & 7);
        RA_SetDirtyM68kRegister(ctx, opcode & 7);

        EMIT(ctx, rev(reg, reg));

        EMIT_AdvancePC(ctx, 6);
        ctx->tc_M68kCodePtr += 2;

        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(ctx);
            uint8_t tmp = RA_AllocARMRegister(ctx);
            EMIT(ctx, cmn_reg(31, reg, LSL, 0));
            uint8_t alt_flags = update_mask;
            if ((alt_flags & 3) != 0 && (alt_flags & 3) < 3)
                alt_flags ^= 3;
            EMIT(ctx, 
                mov_immed_u16(tmp, alt_flags, 0),
                bic_reg(cc, cc, tmp, LSL, 0)
            );

            if (update_mask & SR_Z) {
                EMIT(ctx, 
                    b_cc(A64_CC_EQ ^ 1, 2),
                    orr_immed(cc, cc, 1, (32 - SRB_Z) & 31)
                );
            }
            if (update_mask & SR_N) {
                EMIT(ctx, 
                    b_cc(A64_CC_MI ^ 1, 2),
                    orr_immed(cc, cc, 1, (32 - SRB_N) & 31)
                );
            }
            if (update_mask & (SR_C | SR_X)) {
                EMIT(ctx, 
                    b_cc(A64_CC_CS ^ 1, 3),
                    mov_immed_u16(tmp, SR_Calt | SR_X, 0),
                    orr_reg(cc, cc, tmp, LSL, 0)
                );
            }
            RA_FreeARMRegister(ctx, tmp);
        }

        return 3;
    }
    else if (InsnTable[opcode & 0xfff].od_Emit) {
        return InsnTable[opcode & 0xfff].od_Emit(ctx, opcode);
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

uint32_t GetSR_LineE(uint16_t opcode)
{
    /* If instruction is in the table, return what flags it needs (shifted 16 bits left) and flags it sets */
    if (InsnTable[opcode & 0xfff].od_Emit) {
        return (InsnTable[opcode & 0xfff].od_SRNeeds << 16) | InsnTable[opcode & 0xfff].od_SRSets;
    }
    /* Instruction not found, i.e. it needs all flags and sets none (ILLEGAL INSTRUCTION exception) */
    else {
        kprintf("Undefined LineE\n");
        return SR_CCR << 16;
    }
}

int M68K_GetLineELength(uint16_t *insn_stream)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)insn_stream);
    
    int length = 0;
    int need_ea = 0;
    int opsize = 0;

    if (InsnTable[opcode & 0xfff].od_Emit) {
        length = InsnTable[opcode & 0xfff].od_BaseLength;
        need_ea = InsnTable[opcode & 0xfff].od_HasEA;
        opsize = InsnTable[opcode & 0xfff].od_OpSize;
    }

    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize);
    }

    return length;
}
