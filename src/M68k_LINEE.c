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

static uint32_t *EMIT_ASR_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_ASL_mem")));
static uint32_t *EMIT_ASL_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t dest = 0xff;
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    uint8_t ext_words = 0;
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

    /* Pre-decrement mode */
    if ((opcode & 0x38) == 0x20) {
        *ptr++ = ldrsh_offset_preindex(dest, tmp, -2);
    }
    else {
        *ptr++ = ldrsh_offset(dest, tmp, 0);
    }

    if (update_mask & (SR_C | SR_X)) {
        if (direction) {
            *ptr++ = tst_immed(tmp, 1, 32 - 15);
        }
        else {
            *ptr++ = tst_immed(tmp, 1, 0);
        }
    }

    if (direction)
    {
        *ptr++ = lsl(tmp, tmp, 1);
    }
    else
    {
        *ptr++ = asr(tmp, tmp, 1);
    }

    if ((opcode & 0x38) == 0x18) {
        *ptr++ = strh_offset_postindex(dest, tmp, 2);
    }
    else {
        *ptr++ = strh_offset(dest, tmp, 0);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        uint8_t tmp2 = RA_AllocARMRegister(&ptr);
        
        uint8_t alt_mask = update_mask;
        if ((alt_mask & 3) != 0 && (alt_mask & 3) < 3)
            alt_mask ^= 3;
        ptr = EMIT_ClearFlags(ptr, cc, alt_mask);

        if (update_mask & (SR_C | SR_X)) {
            *ptr++ = b_cc(A64_CC_EQ, 3);
            *ptr++ = mov_immed_u16(tmp2, SR_Calt | SR_X, 0);
            *ptr++ = orr_reg(cc, cc, tmp2, LSL, 0);
        }

        if (update_mask & (SR_Z | SR_N))
        {
            *ptr++ = cmn_reg(31, tmp, LSL, 16);
        
            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
        }

        RA_FreeARMRegister(&ptr, tmp2);

        // V flag can have non-zero value only for left shifting
        if ((update_mask & SR_V) && direction)
        {
            *ptr++ = eor_reg(tmp, tmp, tmp, LSL, 1);
            *ptr++ = tbz(tmp, 16, 2);
            *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Valt) & 31);
        }
    }
    RA_FreeARMRegister(&ptr, tmp);
    RA_FreeARMRegister(&ptr, dest);
    
    return ptr;
}

static uint32_t *EMIT_LSR_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_LSL_mem")));
static uint32_t *EMIT_LSL_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t dest = 0xff;
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    uint8_t ext_words = 0;
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

    /* Pre-decrement mode */
    if ((opcode & 0x38) == 0x20) {
        *ptr++ = ldrh_offset_preindex(dest, tmp, -2);
    }
    else {
        *ptr++ = ldrh_offset(dest, tmp, 0);
    }

    if (update_mask & (SR_C | SR_X)) {
        if (direction) {
            *ptr++ = tst_immed(tmp, 1, 32 - 15);
        }
        else {
            *ptr++ = tst_immed(tmp, 1, 0);
        }
    }

    if (direction)
    {
        *ptr++ = lsl(tmp, tmp, 1);
    }
    else
    {
        *ptr++ = lsr(tmp, tmp, 1);
    }

    if ((opcode & 0x38) == 0x18) {
        *ptr++ = strh_offset_postindex(dest, tmp, 2);
    }
    else {
        *ptr++ = strh_offset(dest, tmp, 0);
    }
        
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        uint8_t tmp2 = RA_AllocARMRegister(&ptr);
        
        uint8_t alt_mask = update_mask;
        if ((alt_mask & 3) != 0 && (alt_mask & 3) < 3)
            alt_mask ^= 3;
        ptr = EMIT_ClearFlags(ptr, cc, alt_mask);

        if (update_mask & (SR_C | SR_X)) {
            *ptr++ = b_cc(A64_CC_EQ, 3);
            *ptr++ = mov_immed_u16(tmp2, SR_Calt | SR_X, 0);
            *ptr++ = orr_reg(cc, cc, tmp2, LSL, 0);
        }

        RA_FreeARMRegister(&ptr, tmp2);

        if (update_mask & (SR_Z | SR_N))
        {
            *ptr++ = cmn_reg(31, tmp, LSL, 16);
        
            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
        }
    }
    RA_FreeARMRegister(&ptr, tmp);
    RA_FreeARMRegister(&ptr, dest);

    return ptr;
}

static uint32_t *EMIT_ROXR_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_ROXL_mem")));
static uint32_t *EMIT_ROXL_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t dest = 0xff;
    uint8_t ext_words = 0;    
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

    uint8_t cc = RA_ModifyCC(&ptr);

    if ((opcode & 0x38) == 0x20) {
        *ptr++ = ldrh_offset_preindex(dest, tmp, -2);
    }
    else {
        *ptr++ = ldrh_offset(dest, tmp, 0);
    }

    /* Test X flag, push the flag value into tmp register */
    *ptr++ = tst_immed(cc, 1, 32 - SRB_X);
    *ptr++ = b_cc(A64_CC_EQ, 2);

    if (direction) {
        *ptr++ = orr_immed(tmp, tmp, 1, 1);
        *ptr++ = ror(tmp, tmp, 31);
    }
    else {
        *ptr++ = orr_immed(tmp, tmp, 1, 16);
        *ptr++ = ror(tmp, tmp, 1);
    }

    if ((opcode & 0x38) == 0x18) {
        *ptr++ = strh_offset_postindex(dest, tmp, 2);
    }
    else {
        *ptr++ = strh_offset(dest, tmp, 0);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t tmp2 = RA_AllocARMRegister(&ptr);
        uint8_t update_mask_copy = update_mask;

        if (update_mask & (SR_Z | SR_N))
        {
            *ptr++ = cmn_reg(31, tmp, LSL, 16);
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        }
        else if (update_mask & SR_V) {
            *ptr++ = bic_immed(cc, cc, 1, 32 - SRB_Valt);
        }
kprintf("[ERROR] ROXL mem not yet fixed!\n");
        if (update_mask_copy & SR_XC) {
            if (direction) {
                *ptr++ = bfxil(tmp, tmp, 16, 1);
                *ptr++ = bfi(cc, tmp, 1, 1);
            }
            else {
                *ptr++ = bfxil(tmp, tmp, 31, 1);
                *ptr++ = bfi(cc, tmp, 1, 1);
            }
            if (update_mask_copy & SR_X) {
                *ptr++ = ror(0, cc, 1);
                *ptr++ = bfi(cc, 0, 4, 1);
            }
        }
      
        RA_FreeARMRegister(&ptr, tmp2);
    }

    RA_FreeARMRegister(&ptr, tmp);
    RA_FreeARMRegister(&ptr, dest);

    return ptr;
}

static uint32_t *EMIT_ROR_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_ROL_mem")));
static uint32_t *EMIT_ROL_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t dest = 0xff;
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    uint8_t ext_words = 0;
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

    if ((opcode & 0x38) == 0x20) {
        *ptr++ = ldrh_offset_preindex(dest, tmp, -2);
    }
    else {
        *ptr++ = ldrh_offset(dest, tmp, 0);
    }
    *ptr++ = bfi(tmp, tmp, 16, 16);

    if (direction)
    {
        *ptr++ = ror(tmp, tmp, 32 - 1);
    }
    else
    {
        *ptr++ = ror(tmp, tmp, 1);
    }

    if ((opcode & 0x38) == 0x18) {
        *ptr++ = strh_offset_postindex(dest, tmp, 2);
    }
    else {
        *ptr++ = strh_offset(dest, tmp, 0);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t tmp2 = RA_AllocARMRegister(&ptr);
        uint8_t cc = RA_ModifyCC(&ptr);

        if (update_mask & (SR_Z | SR_N))
        {
            *ptr++ = cmn_reg(31, tmp, LSL, 16);
        }
        uint8_t alt_flags = update_mask;
        if ((alt_flags & 3) != 0 && (alt_flags & 3) < 3)
            alt_flags ^= 3;
        ptr = EMIT_ClearFlags(ptr, cc, alt_flags);

        if (update_mask & SR_Z) {
            *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
            *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
        }
        if (update_mask & SR_N) {
            *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
            *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
        }

        if (update_mask & (SR_C)) {
            if (direction) {
                *ptr++ = tst_immed(tmp, 1, 16);
            }
            else {
                *ptr++ = tst_immed(tmp, 1, 1);
            }
            *ptr++ = b_cc(A64_CC_EQ, 2);
            *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt));
        }
        RA_FreeARMRegister(&ptr, tmp2);
    }
    RA_FreeARMRegister(&ptr, tmp);
    RA_FreeARMRegister(&ptr, dest);

    return ptr;
}

static uint32_t *EMIT_ASR_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_ASL_reg")));
static uint32_t *EMIT_ASL_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t cc = RA_ModifyCC(&ptr);
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t shift = (opcode >> 9) & 7;
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    uint8_t direction = (opcode >> 8) & 1;
    uint32_t *tmpptr_1;
    uint32_t *tmpptr_2;
    uint8_t shiftreg_orig = RA_AllocARMRegister(&ptr);
    uint8_t reg_orig = RA_AllocARMRegister(&ptr);
    uint8_t mask = RA_AllocARMRegister(&ptr);

    RA_SetDirtyM68kRegister(&ptr, opcode & 7);

    uint8_t shiftreg = RA_MapM68kRegister(&ptr, shift);

    // Check shift size 0 - in that case no bit shifting is necessary, clear VC flags, update NZ, leave X
    *ptr++ = ands_immed(shiftreg_orig, shiftreg, 6, 0);
    tmpptr_1 = ptr;
    *ptr++ = b_cc(A64_CC_NE, 0);

    // If N and/or Z need to be updated, do it and clear CV. No further actions are necessary
    if (update_mask & SR_NZ) {
        uint8_t update_mask_copy = update_mask;
        switch (size) {
            case 4:
                *ptr++ = cmn_reg(31, reg, LSL, 0);
                break;
            case 2:
                *ptr++ = cmn_reg(31, reg, LSL, 16);
                break;
            case 1:
                *ptr++ = cmn_reg(31, reg, LSL, 24);
                break;
        }
        ptr = EMIT_GetNZ00(ptr, cc, &update_mask_copy);
    }
    else if (update_mask & SR_VC) {
        // Only V or C need to be updated. Clear both
        *ptr++ = bic_immed(cc, cc, 2, 0);
    }

    tmpptr_2 = ptr;
    // Skip further bit shifting totally
    *ptr++ = b_cc(A64_CC_AL, 0);

    if ((ptr - tmpptr_1) != 2) {
        *tmpptr_1 = b_cc(A64_CC_NE, ptr - tmpptr_1);
    }
    else {
        ptr--;
        tmpptr_2--;
        tmpptr_1 = NULL;
    }

    if (direction)
    {
        if (update_mask & SR_V)
        {
            *ptr++ = mov64_immed_u16(mask, 0x8000, 3);
            *ptr++ = asrv64(mask, mask, shiftreg_orig);
            switch (size)
            {
                case 4:
                    *ptr++ = lsr64(mask, mask, 32);
                    *ptr++ = mov_reg(reg_orig, reg);
                    break;
                case 2:
                    *ptr++ = lsr64(mask, mask, 32+16);
                    *ptr++ = and_immed(reg_orig, reg, 16, 0);
                    break;
                case 1:
                    *ptr++ = lsr64(mask, mask, 32 + 24);
                    *ptr++ = and_immed(reg_orig, reg, 8, 0);
                    break;
            }
        }

        switch(size)
        {
            case 4:
                *ptr++ = lslv64(tmp, reg, shiftreg);
                *ptr++ = mov_reg(reg, tmp);
                if (update_mask & (SR_C | SR_X)) {
                    *ptr++ = tst64_immed(tmp, 1, 32, 1);
                }
                break;
            case 2:
                *ptr++ = lslv64(tmp, reg, shiftreg);
                if (update_mask & (SR_C | SR_X)) {
                    *ptr++ = tst_immed(tmp, 1, 16);
                }
                *ptr++ = bfi(reg, tmp, 0, 16);
                break;
            case 1:
                *ptr++ = lslv64(tmp, reg, shiftreg);
                if (update_mask & (SR_C | SR_X)) {
                    *ptr++ = tst_immed(tmp, 1, 24);
                }
                *ptr++ = bfi(reg, tmp, 0, 8);
                break;
        }
    }
    else
    {
        uint8_t mask = RA_AllocARMRegister(&ptr);

        if (update_mask & (SR_C | SR_X))
        {
            uint8_t t = RA_AllocARMRegister(&ptr);
            *ptr++ = sub_immed(t, shiftreg, 1);
            *ptr++ = mov_immed_u16(mask, 1, 0);
            *ptr++ = lslv64(mask, mask, t);
            RA_FreeARMRegister(&ptr, t);
        }

        switch (size)
        {
            case 4:
                *ptr++ = sxtw64(tmp, reg);
                if (update_mask & (SR_C | SR_X))
                {
                    *ptr++ = ands64_reg(31, tmp, mask, LSL, 0);
                }
                *ptr++ = asrv64(tmp, tmp, shiftreg);
                *ptr++ = mov_reg(reg, tmp);
                break;
            case 2:
                *ptr++ = sxth64(tmp, reg);
                if (update_mask & (SR_C | SR_X))
                {
                    *ptr++ = ands64_reg(31, tmp, mask, LSL, 0);
                }
                *ptr++ = asrv64(tmp, tmp, shiftreg);
                *ptr++ = bfi(reg, tmp, 0, 16);
                break;
            case 1:
                *ptr++ = sxtb64(tmp, reg);
                if (update_mask & (SR_C | SR_X))
                {
                    *ptr++ = ands64_reg(31, tmp, mask, LSL, 0);
                }
                *ptr++ = asrv64(tmp, tmp, shiftreg);
                *ptr++ = bfi(reg, tmp, 0, 8);
                break;
        }

        RA_FreeARMRegister(&ptr, mask);
    }

    if (update_mask)
    {
        uint8_t tmp2 = RA_AllocARMRegister(&ptr);
        uint8_t alt_flags = update_mask;
        if ((alt_flags & 3) != 0 && (alt_flags & 3) < 3)
            alt_flags ^= 3;
        ptr = EMIT_ClearFlags(ptr, cc, alt_flags);

        if (update_mask & (SR_C | SR_X)) {
            *ptr++ = b_cc(A64_CC_EQ, 3);
            *ptr++ = mov_immed_u16(tmp2, SR_Calt | SR_X, 0);
            *ptr++ = orr_reg(cc, cc, tmp2, LSL, 0);
        }

        RA_FreeARMRegister(&ptr, tmp2);

        if (direction && (update_mask & SR_V)) {
            *ptr++ = ands_reg(31, reg_orig, mask, LSL, 0);
            *ptr++ = b_cc(A64_CC_EQ, 7);
            *ptr++ = cmp_immed(shiftreg_orig, size == 4 ? 32 : (size == 2 ? 16 : 8));
            *ptr++ = b_cc(A64_CC_GE, 4);
            *ptr++ = eor_reg(reg_orig, reg_orig, mask, LSL, 0);
            *ptr++ = ands_reg(31, reg_orig, mask, LSL, 0);
            *ptr++ = b_cc(A64_CC_EQ, 2);
            *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_Valt));
        }

        if (update_mask & (SR_Z | SR_N))
        {
            switch(size)
            {
                case 4:
                    *ptr++ = cmn_reg(31, reg, LSL, 0);
                    break;
                case 2:
                    *ptr++ = cmn_reg(31, tmp, LSL, 16);
                    break;
                case 1:
                    *ptr++ = cmn_reg(31, tmp, LSL, 24);
                    break;
            }

            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
        }
    }

    if (tmpptr_1 != NULL) {
        *tmpptr_2 = b_cc(A64_CC_AL, ptr - tmpptr_2);
    }
    else {
        *tmpptr_2 = b_cc(A64_CC_EQ, ptr - tmpptr_2);
    }

    ptr = EMIT_AdvancePC(ptr, 2);

    RA_FreeARMRegister(&ptr, tmp);
    RA_FreeARMRegister(&ptr, mask);
    RA_FreeARMRegister(&ptr, shiftreg_orig);
    RA_FreeARMRegister(&ptr, reg_orig);

    return ptr;

}

static uint32_t *EMIT_ASR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_ASL")));
static uint32_t *EMIT_ASL(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t shift = (opcode >> 9) & 7;
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t cc = RA_ModifyCC(&ptr);
    RA_SetDirtyM68kRegister(&ptr, opcode & 7);

    if (!shift) shift = 8;

    if (direction && (update_mask & SR_V))
    {
        uint8_t tmp_reg = RA_AllocARMRegister(&ptr);
        int rot = (size == 4) ? 0 : (size == 2) ? 16 : 24;
        int width = shift + 1;

        if (size == 1 && width > 8)
            width = 8;

        *ptr++ = bic_immed(cc, cc, 1, 31 & (32 - SRB_Valt));
        *ptr++ = ands_immed(tmp_reg, reg, width, width + rot);
        *ptr++ = b_cc(A64_CC_EQ, (size == 1 && shift == 8) ? 2 : 5);
        if (!(size == 1 && shift == 8)) {
            *ptr++ = eor_immed(tmp_reg, tmp_reg, width, width + rot);
            *ptr++ = ands_immed(tmp_reg, tmp_reg, width, width + rot);
            *ptr++ = b_cc(A64_CC_EQ, 2);
        }
        *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_Valt));
        
        update_mask &= ~SR_V;
        RA_FreeARMRegister(&ptr, tmp_reg);
    }

    if (update_mask & (SR_C | SR_X)) {
        if (direction) {
            switch (size) {
                case 4:
                    *ptr++ = tst_immed(reg, 1, shift);
                    break;
                case 2:
                    *ptr++ = tst_immed(reg, 1, 16 + shift);
                    break;
                case 1:
                    *ptr++ = tst_immed(reg, 1, 31 & (24 + shift));
                    break;
            }
        }
        else {
            *ptr++ = tst_immed(reg, 1, 31 & (33 - shift));
        }
    }

    if (direction)
    {
        switch (size)
        {
            case 4:
                *ptr++ = lsl(reg, reg, shift);
                break;
            case 2:
                *ptr++ = lsl(tmp, reg, shift);
                *ptr++ = bfi(reg, tmp, 0, 16);
                break;
            case 1:
                *ptr++ = lsl(tmp, reg, shift);
                *ptr++ = bfi(reg, tmp, 0, 8);
                break;
        }
    }
    else
    {
        switch (size)
        {
        case 4:
            *ptr++ = asr(reg, reg, shift);
            break;
        case 2:
            *ptr++ = sxth(tmp, reg);
            *ptr++ = asr(tmp, tmp, shift);
            *ptr++ = bfi(reg, tmp, 0, 16);
            break;
        case 1:
            *ptr++ = sxtb(tmp, reg);
            *ptr++ = asr(tmp, tmp, shift);
            *ptr++ = bfi(reg, tmp, 0, 8);
            break;
        }
    }

    ptr = EMIT_AdvancePC(ptr, 2);

    if (update_mask)
    {
        uint8_t tmp2 = RA_AllocARMRegister(&ptr);
        uint8_t clear_mask = update_mask;

        /* Swap C and V flags in immediate */
        if ((clear_mask & 3) != 0 && (clear_mask & 3) < 3)
            clear_mask ^= 3;

        ptr = EMIT_ClearFlags(ptr, cc, clear_mask);

        if (update_mask & (SR_C | SR_X)) {
            *ptr++ = b_cc(A64_CC_EQ, 3);
            *ptr++ = mov_immed_u16(tmp2, SR_Calt | SR_X, 0);
            *ptr++ = orr_reg(cc, cc, tmp2, LSL, 0);
        }

        RA_FreeARMRegister(&ptr, tmp2);

        if (update_mask & (SR_Z | SR_N))
        {
            switch(size)
            {
                case 4:
                    *ptr++ = cmn_reg(31, reg, LSL, 0);
                    break;
                case 2:
                    *ptr++ = cmn_reg(31, tmp, LSL, 16);
                    break;
                case 1:
                    *ptr++ = cmn_reg(31, tmp, LSL, 24);
                    break;
            }

            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
        }
    }

    RA_FreeARMRegister(&ptr, tmp);

    return ptr;
}

static uint32_t *EMIT_LSR_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_LSL_reg")));
static uint32_t *EMIT_LSL_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t cc = RA_ModifyCC(&ptr);
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t shift = (opcode >> 9) & 7;
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    uint32_t *tmpptr_1;
    uint32_t *tmpptr_2;

    RA_SetDirtyM68kRegister(&ptr, opcode & 7);

    uint8_t shiftreg = RA_MapM68kRegister(&ptr, shift);

    // Check shift size 0 - in that case no bit shifting is necessary, clear VC flags, update NZ, leave X
    *ptr++ = ands_immed(31, shiftreg, 6, 0);
    tmpptr_1 = ptr;
    *ptr++ = b_cc(A64_CC_NE, 0);

    // If V and/or Z need to be updated, do it and clear CV. No further actions are necessary
    if (update_mask & SR_NZ) {
        uint8_t update_mask_copy = update_mask;
        switch (size) {
            case 4:
                *ptr++ = cmn_reg(31, reg, LSL, 0);
                break;
            case 2:
                *ptr++ = cmn_reg(31, reg, LSL, 16);
                break;
            case 1:
                *ptr++ = cmn_reg(31, reg, LSL, 24);
                break;
        }
        ptr = EMIT_GetNZ00(ptr, cc, &update_mask_copy);
    }
    else if (update_mask & SR_VC) {
        // Only V or C need to be updated. Clear both
        *ptr++ = bic_immed(cc, cc, 2, 0);
    }

    tmpptr_2 = ptr;
    // Skip further bit shifting totally
    *ptr++ = b_cc(A64_CC_AL, 0);

    if ((ptr - tmpptr_1) != 2) {
        *tmpptr_1 = b_cc(A64_CC_NE, ptr - tmpptr_1);
    }
    else {
        ptr--;
        tmpptr_2--;
        tmpptr_1 = NULL;
    }

    if (direction)
    {
        switch (size)
        {
        case 4:
            *ptr++ = lslv64(tmp, reg, shiftreg);
            *ptr++ = mov_reg(reg, tmp);
            if (update_mask & (SR_C | SR_X)) {
                *ptr++ = tst64_immed(tmp, 1, 32, 1);
            }
            break;
        case 2:
            *ptr++ = lslv64(tmp, reg, shiftreg);
            if (update_mask & (SR_C | SR_X)) {
                *ptr++ = tst_immed(tmp, 1, 16);
            }
            *ptr++ = bfi(reg, tmp, 0, 16);
            break;
        case 1:
            *ptr++ = lslv64(tmp, reg, shiftreg);
            if (update_mask & (SR_C | SR_X)) {
                *ptr++ = tst_immed(tmp, 1, 24);
            }
            *ptr++ = bfi(reg, tmp, 0, 8);
            break;
        }
    }
    else
    {
        uint8_t mask = RA_AllocARMRegister(&ptr);
        if (update_mask & (SR_C | SR_X))
        {
            uint8_t t = RA_AllocARMRegister(&ptr);
/*
            *ptr++ = sub_immed(t, shiftreg, 1);
            *ptr++ = mov_immed_u16(mask, 1, 0);
            *ptr++ = lslv64(mask, mask, t);
*/
            RA_FreeARMRegister(&ptr, t);
        }
        switch (size)
        {
        case 4:
            *ptr++ = mov_reg(tmp, reg);
            if (update_mask & (SR_C | SR_X))
            {
                //*ptr++ = ands_reg(31, tmp, mask, LSL, 0);
                *ptr++ = rorv64(0, tmp, shiftreg);
                *ptr++ = tst64_immed(0, 1, 1, 1);
            }
            *ptr++ = lsrv64(tmp, tmp, shiftreg);
            *ptr++ = mov_reg(reg, tmp);
            break;
        case 2:
            *ptr++ = uxth(tmp, reg);
            if (update_mask & (SR_C | SR_X))
            {
                //*ptr++ = ands_reg(31, tmp, mask, LSL, 0);
                *ptr++ = rorv64(0, tmp, shiftreg);
                *ptr++ = tst64_immed(0, 1, 1, 1);
            }
            *ptr++ = lsrv64(tmp, tmp, shiftreg);
            *ptr++ = bfi(reg, tmp, 0, 16);
            break;
        case 1:
            *ptr++ = uxtb(tmp, reg);
            if (update_mask & (SR_C | SR_X))
            {
                //*ptr++ = ands_reg(31, tmp, mask, LSL, 0);
                *ptr++ = rorv64(0, tmp, shiftreg);
                *ptr++ = tst64_immed(0, 1, 1, 1);
            }
            *ptr++ = lsrv64(tmp, tmp, shiftreg);
            *ptr++ = bfi(reg, tmp, 0, 8);
            break;
        }
        RA_FreeARMRegister(&ptr, mask);
    }

    if (update_mask)
    {
        uint8_t tmp2 = RA_AllocARMRegister(&ptr);

        /* C/X condition is already pre-computed. Insert the flags now! */       
        if (update_mask & (SR_C | SR_X)) {
            if ((update_mask & SR_XC) == SR_XC)
            {
                *ptr++ = mov_immed_u16(tmp2, SR_Calt | SR_X, 0);
                *ptr++ = bic_reg(0, cc, tmp2, LSL, 0);
                *ptr++ = orr_reg(tmp2, cc, tmp2, LSL, 0);
                *ptr++ = csel(cc, 0, tmp2, A64_CC_EQ);
            }
            else if ((update_mask & SR_XC) == SR_X)
            {
                *ptr++ = cset(0, A64_CC_NE);
                *ptr++ = bfi(cc, 0, SRB_X, 1);
            }
            else
            {
                *ptr++ = cset(0, A64_CC_NE);
                *ptr++ = bfi(cc, 0, SRB_Calt, 1);
            }

            /* Done with C and/or X */
            update_mask &= ~(SR_XC);
        }

        RA_FreeARMRegister(&ptr, tmp2);

        uint8_t clear_mask = update_mask;

        /* Swap C and V flags in immediate */
        if ((clear_mask & 3) != 0 && (clear_mask & 3) < 3)
            clear_mask ^= 3;

        ptr = EMIT_ClearFlags(ptr, cc, clear_mask);

        if (update_mask & (SR_Z | SR_N))
        {
            switch(size)
            {
                case 4:
                    *ptr++ = cmn_reg(31, reg, LSL, 0);
                    break;
                case 2:
                    *ptr++ = cmn_reg(31, tmp, LSL, 16);
                    break;
                case 1:
                    *ptr++ = cmn_reg(31, tmp, LSL, 24);
                    break;
            }

            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
        }
    }

    if (tmpptr_1 != NULL) {
        *tmpptr_2 = b_cc(A64_CC_AL, ptr - tmpptr_2);
    }
    else {
        *tmpptr_2 = b_cc(A64_CC_EQ, ptr - tmpptr_2);
    }

    ptr = EMIT_AdvancePC(ptr, 2);

    RA_FreeARMRegister(&ptr, tmp);

    return ptr;
}


static uint32_t *EMIT_LSR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_LSL")));
static uint32_t *EMIT_LSL(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{

    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t shift = (opcode >> 9) & 7;
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
    uint8_t tmp = RA_AllocARMRegister(&ptr);

    RA_SetDirtyM68kRegister(&ptr, opcode & 7);

    if (!shift)
        shift = 8;

    if (update_mask & (SR_C | SR_X)) {
        if (direction) {
            switch (size) {
                case 4:
                    *ptr++ = tst_immed(reg, 1, shift);
                    break;
                case 2:
                    *ptr++ = tst_immed(reg, 1, 16 + shift);
                    break;
                case 1:
                    *ptr++ = tst_immed(reg, 1, 31 & (24 + shift));
                    break;
            }
        }
        else {
            *ptr++ = tst_immed(reg, 1, 31 & (33 - shift));
        }
    }

    if (direction)
    {
        switch (size)
        {
        case 4:
            *ptr++ = lsl(reg, reg, shift);
            break;
        case 2:
            *ptr++ = lsl(tmp, reg, shift);
            *ptr++ = bfi(reg, tmp, 0, 16);
            break;
        case 1:
            *ptr++ = lsl(tmp, reg, shift);
            *ptr++ = bfi(reg, tmp, 0, 8);
            break;
        }
    }
    else
    {
        switch (size)
        {
        case 4:
            *ptr++ = lsr(reg, reg, shift);
            break;
        case 2:
            *ptr++ = uxth(tmp, reg);
            *ptr++ = lsr(tmp, tmp, shift);
            *ptr++ = bfi(reg, tmp, 0, 16);
            break;
        case 1:
            *ptr++ = uxtb(tmp, reg);
            *ptr++ = lsr(tmp, tmp, shift);
            *ptr++ = bfi(reg, tmp, 0, 8);
            break;
        }
    }

    ptr = EMIT_AdvancePC(ptr, 2);

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);

        uint8_t alt_flags = update_mask;
        if ((alt_flags & 3) != 0 && (alt_flags & 3) < 3)
            alt_flags ^= 3;

        ptr = EMIT_ClearFlags(ptr, cc, alt_flags);

        uint8_t tmp2 = RA_AllocARMRegister(&ptr);

        /* C/X condition is already pre-computed. Insert the flags now! */       
        if (update_mask & (SR_C | SR_X)) {
            if ((update_mask & SR_XC) == SR_XC)
            {
                *ptr++ = mov_immed_u16(tmp2, SR_Calt | SR_X, 0);
                *ptr++ = orr_reg(tmp2, cc, tmp2, LSL, 0);
                *ptr++ = csel(cc, cc, tmp2, A64_CC_EQ);
            }
            else if ((update_mask & SR_XC) == SR_X)
            {
                *ptr++ = cset(0, A64_CC_NE);
                *ptr++ = bfi(cc, 0, SRB_X, 1);
            }
            else
            {
                *ptr++ = cset(0, A64_CC_NE);
                *ptr++ = bfi(cc, 0, SRB_Calt, 1);
            }

            /* Done with C and/or X */
            update_mask &= ~(SR_XC);
        }

        RA_FreeARMRegister(&ptr, tmp2);

        if (update_mask & (SR_Z | SR_N))
        {
            switch(size)
            {
                case 4:
                    *ptr++ = cmn_reg(31, reg, LSL, 0);
                    break;
                case 2:
                    *ptr++ = cmn_reg(31, tmp, LSL, 16);
                    break;
                case 1:
                    *ptr++ = cmn_reg(31, tmp, LSL, 24);
                    break;
            }

            if (update_mask & SR_Z) {
                *ptr++ = orr_immed(0, cc, 1, (32 - SRB_Z) & 31);
                *ptr++ = csel(cc, 0, cc, A64_CC_EQ);
            }
            if (update_mask & SR_N) {
                *ptr++ = orr_immed(0, cc, 1, (32 - SRB_N) & 31);
                *ptr++ = csel(cc, 0, cc, A64_CC_MI);
            }
        }
    }
    RA_FreeARMRegister(&ptr, tmp);

    return ptr;
}

static uint32_t *EMIT_ROR_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_ROL")));
static uint32_t *EMIT_ROL_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_ROL")));
static uint32_t *EMIT_ROR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_ROL")));
static uint32_t *EMIT_ROL(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t shift = (opcode >> 9) & 7;
    uint8_t shift_orig = 0xff;
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t regshift = (opcode >> 5) & 1;
    uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
    uint8_t tmp = RA_AllocARMRegister(&ptr);

    RA_SetDirtyM68kRegister(&ptr, opcode & 7);

    if (regshift)
    {
        if (direction)
        {
            uint8_t shift_mod = RA_AllocARMRegister(&ptr);
            shift = RA_MapM68kRegister(&ptr, shift);

            if (update_mask & SR_C) {
                shift_orig = RA_AllocARMRegister(&ptr);
                *ptr++ = and_immed(shift_orig, shift, 6, 0);
            }

            *ptr++ = neg_reg(shift_mod, shift, LSL, 0);
            *ptr++ = add_immed(shift_mod, shift_mod, 32);

            shift = shift_mod;
        }
        else
        {
            shift = RA_CopyFromM68kRegister(&ptr, shift);
            if (update_mask & SR_C) {
                shift_orig = RA_AllocARMRegister(&ptr);
                *ptr++ = and_immed(shift_orig, shift, 6, 0);
            }
        }

        switch (size)
        {
            case 4:
                *ptr++ = rorv(reg, reg, shift);
                break;
            case 2:
                *ptr++ = mov_reg(tmp, reg);
                *ptr++ = bfi(tmp, tmp, 16, 16);
                *ptr++ = rorv(tmp, tmp, shift);
                *ptr++ = bfi(reg, tmp, 0, 16);
                break;
            case 1:
                *ptr++ = mov_reg(tmp, reg);
                *ptr++ = bfi(tmp, tmp, 8, 8);
                *ptr++ = bfi(tmp, tmp, 16, 16);
                *ptr++ = rorv(tmp, tmp, shift);
                *ptr++ = bfi(reg, tmp, 0, 8);
                break;
        }

        RA_FreeARMRegister(&ptr, shift);
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
            *ptr++ = ror(reg, reg, shift);
            break;
        case 2:
            *ptr++ = mov_reg(tmp, reg);
            *ptr++ = bfi(tmp, tmp, 16, 16);
            *ptr++ = ror(tmp, tmp, shift);
            *ptr++ = bfi(reg, tmp, 0, 16);
            break;
        case 1:
            *ptr++ = mov_reg(tmp, reg);
            *ptr++ = bfi(tmp, tmp, 8, 8);
            *ptr++ = bfi(tmp, tmp, 16, 16);
            *ptr++ = ror(tmp, tmp, shift);
            *ptr++ = bfi(reg, tmp, 0, 8);
            break;
        }
    }

    ptr = EMIT_AdvancePC(ptr, 2);

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        switch(size)
        {
            case 4:
                *ptr++ = cmn_reg(31, reg, LSL, 0);
                break;
            case 2:
                *ptr++ = cmn_reg(31, tmp, LSL, 16);
                break;
            case 1:
                *ptr++ = cmn_reg(31, tmp, LSL, 24);
                break;
        }
        uint8_t old_mask = update_mask & SR_C;
        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        update_mask |= old_mask;

        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
        if (update_mask & SR_C) {
            if (regshift) {
                if (!direction)
                    *ptr++ = cbz(shift_orig, 3);
                else
                    *ptr++ = cbz(shift_orig, 2);
                RA_FreeARMRegister(&ptr, shift_orig);
            }
            if (!direction) {
                switch(size) {
                    case 4:
                        *ptr++ = bfxil(tmp, reg, 31, 1);
                        *ptr++ = bfi(cc, tmp, 1, 1);
                        break;
                    case 2:
                        *ptr++ = bfxil(tmp, reg, 15, 1);
                        *ptr++ = bfi(cc, tmp, 1, 1);
                        break;
                    case 1:
                        *ptr++ = bfxil(tmp, reg, 7, 1);
                        *ptr++ = bfi(cc, tmp, 1, 1);
                        break;
                }
            }
            else {
                *ptr++ = bfi(cc, reg, 1, 1);
            }
        }

    }
    RA_FreeARMRegister(&ptr, tmp);

    return ptr;
}

static uint32_t *EMIT_ROXR_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_ROXL")));
static uint32_t *EMIT_ROXL_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_ROXL")));
static uint32_t *EMIT_ROXR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_ROXL")));
static uint32_t *EMIT_ROXL(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    int dir = opcode & 0x100;
    uint8_t cc = RA_ModifyCC(&ptr);

    int size = (opcode >> 6) & 3;
    uint8_t dest = RA_MapM68kRegister(&ptr, opcode & 7);
    RA_SetDirtyM68kRegister(&ptr, opcode & 7);

    if (opcode & 0x20)
    {
        // REG/REG mode
        uint8_t amount_reg = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
        uint8_t amount = RA_AllocARMRegister(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t tmp2 = RA_AllocARMRegister(&ptr);
        uint32_t *tmp_ptr;
        
        // Limit rotate amount to 0..63, depending on size calculate modulo 9, 17, 33, depending on size
        *ptr++ = ands_immed(tmp, amount_reg, 6, 0);

        // If Z flag is set, don't bother with further ROXL/ROXR - size 0, no reg change
        // Only update CPU flags in that case
        tmp_ptr = ptr;
        *ptr++ = 0;

        if (update_mask & SR_NZV) {
            switch (size)
            {
                case 0:
                    *ptr++ = cmn_reg(31, dest, LSL, 24);
                    break;
                case 1:
                    *ptr++ = cmn_reg(31, dest, LSL, 16);
                    break;
                case 2:
                    *ptr++ = cmn_reg(31, dest, LSL, 0);
                    break;
            }

            uint8_t tmp_mask = update_mask;
            ptr = EMIT_GetNZ00(ptr, cc, &tmp_mask);
        }

        if (update_mask & SR_C) {
            *ptr++ = lsr(0, cc, 4);
            *ptr++ = bfi(cc, 0, 1, 1);
        }

        *ptr++ = 0;

        *tmp_ptr = b_cc(A64_CC_NE, ptr - tmp_ptr);
        tmp_ptr = ptr - 1;

        // Continue calculating modulo
        *ptr++ = mov_immed_u16(tmp2, size == 0 ? 9 : (size == 1 ? 17 : 33), 0);
        *ptr++ = udiv(amount, tmp, tmp2);
        *ptr++ = msub(amount, tmp, amount, tmp2);

        // Copy data from dest register
        switch (size)
        {
            case 0:
                *ptr++ = and_immed(tmp, dest, 8, 0);
                break;
            case 1:
                *ptr++ = and_immed(tmp, dest, 16, 0);
                break;
            case 2:
                *ptr++ = mov_reg(tmp, dest);
                break;
        }
        
kprintf("[ERROR] ROXL not yet fixed!\n");
        // Fill the temporary register with repetitions of X and dest
        *ptr++ = tst_immed(cc, 1, 32 - SRB_X);
        if (dir)
        {
            // Rotate left
            switch (size)
            {
                case 0: // byte
                    *ptr++ = neg_reg(amount, amount, LSR, 0);
                    *ptr++ = add_immed(amount, amount, 32);
                    *ptr++ = b_cc(A64_CC_EQ, 2);
                    *ptr++ = orr_immed(tmp, tmp, 1, 32 - 8);
                    *ptr++ = bfi(tmp, tmp, 32 - 9, 9);
                    *ptr++ = rorv(tmp, tmp, amount);
                    *ptr++ = bfi(dest, tmp, 0, 8);
                    break;
                
                case 1: // word
                    *ptr++ = neg_reg(amount, amount, LSR, 0);
                    *ptr++ = add_immed(amount, amount, 64);
                    *ptr++ = b_cc(A64_CC_EQ, 2);
                    *ptr++ = orr_immed(tmp, tmp, 1, 32 - 16);
                    *ptr++ = bfi64(tmp, tmp, 64 - 17, 17);
                    *ptr++ = rorv64(tmp, tmp, amount);
                    *ptr++ = bfi(dest, tmp, 0, 16);
                    break;

                case 2: // long
                    *ptr++ = b_cc(A64_CC_EQ, 2);
                    *ptr++ = orr64_immed(tmp, tmp, 1, 32, 1);
                    *ptr++ = cbz(amount, 13);
                    *ptr++ = neg_reg(amount, amount, LSR, 0);
                    *ptr++ = add_immed(amount, amount, 64);
                    *ptr++ = cmp_immed(amount, 32);
                    *ptr++ = b_cc(A64_CC_EQ, 6);
                    *ptr++ = lsl64(tmp, tmp, 31);
                    *ptr++ = bfxil64(tmp, tmp, 31, 32);
                    *ptr++ = rorv64(tmp, tmp, amount);
                    *ptr++ = mov_reg(dest, tmp);
                    *ptr++ = b(4);
                    *ptr++ = bfi64(tmp, tmp, 33, 10);
                    *ptr++ = ror64(tmp, tmp, 1);
                    *ptr++ = mov_reg(dest, tmp);
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
                    *ptr++ = b_cc(A64_CC_EQ, 2);
                    *ptr++ = orr_immed(tmp, tmp, 1, 32 - 8);
                    *ptr++ = bfi(tmp, tmp, 9, 9);
                    *ptr++ = rorv(tmp, tmp, amount);
                    *ptr++ = bfi(dest, tmp, 0, 8);
                    break;
                
                case 1: // word
                    *ptr++ = b_cc(A64_CC_EQ, 2);
                    *ptr++ = orr_immed(tmp, tmp, 1, 32 - 16);
                    *ptr++ = bfi64(tmp, tmp, 17, 17);
                    *ptr++ = rorv64(tmp, tmp, amount);
                    *ptr++ = bfi(dest, tmp, 0, 16);
                    break;

                case 2: // long
                    *ptr++ = b_cc(A64_CC_EQ, 2);
                    *ptr++ = orr64_immed(tmp, tmp, 1, 64 - 32, 1);
                    *ptr++ = cmp_immed(amount, 31);
                    *ptr++ = b_cc(A64_CC_HI, 5);
                    *ptr++ = bfi64(tmp, tmp, 33, 31);
                    *ptr++ = rorv64(tmp, tmp, amount);
                    *ptr++ = mov_reg(dest, tmp);
                    *ptr++ = b(6);
                    *ptr++ = lsr64(tmp, tmp, 32);
                    *ptr++ = sub_immed(amount, amount, 32);
                    *ptr++ = bfi64(tmp, dest, 1, 32);
                    *ptr++ = rorv64(tmp, tmp, amount);
                    *ptr++ = mov_reg(dest, tmp);
                    break;

                default:
                    break;
            }
        }
        
        if (update_mask & SR_NZV) {
            switch (size)
            {
                case 0:
                    *ptr++ = cmn_reg(31, tmp, LSL, 24);
                    break;
                case 1:
                    *ptr++ = cmn_reg(31, tmp, LSL, 16);
                    break;
                case 2:
                    *ptr++ = cmn_reg(31, tmp, LSL, 0);
                    break;
            }

            uint8_t tmp_mask = update_mask;
            ptr = EMIT_GetNZ00(ptr, cc, &tmp_mask);
        }

        if (update_mask & SR_XC) {
            switch(size)
            {
                case 0:
                    *ptr++ = bfxil(tmp, tmp, 8, 1);
                    *ptr++ = bfi(cc, tmp, 1, 1);
                    break;
                case 1:
                    *ptr++ = bfxil(tmp, tmp, 16, 1);
                    *ptr++ = bfi(cc, tmp, 1, 1);
                    break;
                case 2:
                    *ptr++ = bfxil64(tmp, tmp, 32, 1);
                    *ptr++ = bfi(cc, tmp, 1, 1);
                    break;
            }
            
            if (update_mask & SR_X) {
                *ptr++ = ror(0, cc, 1);
                *ptr++ = bfi(cc, 0, 4, 1);
            }
        }

        *tmp_ptr = b(ptr - tmp_ptr);

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, tmp2);
        RA_FreeARMRegister(&ptr, amount);
    }
    else {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
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
                    *ptr++ = mov_reg(tmp, dest);
                    *ptr++ = bic_immed(tmp, tmp, 1, 1);
                    *ptr++ = tbz(cc, SRB_X, 2);
                    *ptr++ = orr_immed(tmp, tmp, 1, 1);
                    *ptr++ = bfi(tmp, tmp, 31-8, 8);
                    *ptr++ = ror(tmp, tmp, 32 - amount);
                    *ptr++ = bfi(dest, tmp, 0, 8);
                    break;
                
                // Rotate left word, 1 to 8 positions
                // temporary register layout
                // Xfedcba9 87654321 fedcba98 76543210
                // After rotation copy the 31th bit into X and C

                case 1: // word
                    *ptr++ = mov_reg(tmp, dest);
                    *ptr++ = bic_immed(tmp, tmp, 1, 1);
                    *ptr++ = tbz(cc, SRB_X, 2);
                    *ptr++ = orr_immed(tmp, tmp, 1, 1);
                    *ptr++ = bfi64(tmp, tmp, 31-16, 16);
                    *ptr++ = ror(tmp, tmp, 32 - amount);
                    *ptr++ = bfi(dest, tmp, 0, 16);
                    break;

                // Rotate left long, 1 to 8 positions
                // Use 64bit temporary register for the operation
                // bits 64-32: X(1f)(1e)...(00)
                // bits 31-0: source register
                case 2: // long
                    *ptr++ = lsl64(tmp, dest, 31);
                    *ptr++ = bic64_immed(tmp, tmp, 1, 1, 1);
                    *ptr++ = tbz(cc, SRB_X, 2);
                    *ptr++ = orr64_immed(tmp, tmp, 1, 1, 1);
                    *ptr++ = bfxil64(tmp, tmp, 31, 32);
                    *ptr++ = ror64(tmp, tmp, 64 - amount);
                    *ptr++ = mov_reg(dest, tmp);
                    break;
            }
        }
        else
        {
            // rotate right
            switch (size)
            {
                case 0: // byte
                    *ptr++ = mov_reg(tmp, dest);
                    *ptr++ = bic_immed(tmp, tmp, 1, 32 - 8);
                    *ptr++ = tbz(cc, SRB_X, 2);
                    *ptr++ = orr_immed(tmp, tmp, 1, 32 - 8);
                    *ptr++ = bfi(tmp, tmp, 9, 9);
                    *ptr++ = ror(tmp, tmp, amount);
                    *ptr++ = bfi(dest, tmp, 0, 8);
                    break;
                case 1: // word
                    *ptr++ = mov_reg(tmp, dest);
                    *ptr++ = bic_immed(tmp, tmp, 1, 32 - 16);
                    *ptr++ = tbz(cc, SRB_X, 2);
                    *ptr++ = orr_immed(tmp, tmp, 1, 32 - 16);
                    *ptr++ = bfi64(tmp, tmp, 17, 17);
                    *ptr++ = ror64(tmp, tmp, amount);
                    *ptr++ = bfi(dest, tmp, 0, 16);
                    break;
                case 2: // long
                    *ptr++ = lsl64(tmp, dest, 33);
                    *ptr++ = bfi64(tmp, dest, 0, 32);
                    *ptr++ = tbz(cc, SRB_X, 4);
                    *ptr++ = orr64_immed(tmp, tmp, 1, 32, 1);
                    *ptr++ = b(2);
                    *ptr++ = bic64_immed(tmp, tmp, 1, 32, 1);
                    *ptr++ = ror64(tmp, tmp, amount);
                    *ptr++ = mov_reg(dest, tmp);
                    break;
            }
        }

        if (update_mask & SR_NZV) {
            switch (size)
            {
                case 0:
                    *ptr++ = cmn_reg(31, tmp, LSL, 24);
                    break;
                case 1:
                    *ptr++ = cmn_reg(31, tmp, LSL, 16);
                    break;
                case 2:
                    *ptr++ = cmn_reg(31, tmp, LSL, 0);
                    break;
            }

            uint8_t tmp_mask = update_mask;
            ptr = EMIT_GetNZ00(ptr, cc, &tmp_mask);
        }

        if (update_mask & SR_XC) {
            if (dir) {
                switch(size)
                {
                    case 0:
                        *ptr++ = bfxil(tmp, tmp, 31, 1);
                        *ptr++ = bfi(cc, tmp, 1, 1);
                        break;
                    case 1:
                        *ptr++ = bfxil(tmp, tmp, 31, 1);
                        *ptr++ = bfi(cc, tmp, 1, 1);
                        break;
                    case 2:
                        *ptr++ = bfxil64(tmp, tmp, 63, 1);
                        *ptr++ = bfi(cc, tmp, 1, 1);
                        break;
                }
            }
            else {
                switch(size)
                {
                    case 0:
                        *ptr++ = bfxil(tmp, tmp, 8, 1);
                        *ptr++ = bfi(cc, tmp, 1, 1);
                        break;
                    case 1:
                        *ptr++ = bfxil(tmp, tmp, 16, 1);
                        *ptr++ = bfi(cc, tmp, 1, 1);
                        break;
                    case 2:
                        *ptr++ = bfxil64(tmp, tmp, 32, 1);
                        *ptr++ = bfi(cc, tmp, 1, 1);
                        break;
                }
            }
            
            if (update_mask & SR_X) {
                *ptr++ = ror(0, cc, 1);
                *ptr++ = bfi(cc, 0, 4, 1);
            }
        }

        RA_FreeARMRegister(&ptr, tmp);
    }

    ptr = EMIT_AdvancePC(ptr, 2);
    return ptr;
}

enum BF_OP {
    OP_EOR,
    OP_SET,
    OP_CLR,
    OP_TST,
    OP_INS,
};

static inline uint32_t *EMIT_BFxxx_IR(uint32_t *ptr, uint8_t base, enum BF_OP op, uint8_t Do, uint8_t Dw, uint8_t update_mask, uint8_t data)
{
    uint8_t mask_reg = RA_AllocARMRegister(&ptr);
    uint8_t width_reg_orig = Dw;
    uint8_t width_reg = 3;
    uint8_t data_reg = 0;
    uint8_t test_reg = 1;
    uint8_t insert_reg = test_reg;

    uint8_t base_offset = Do >> 3;
    uint8_t bit_offset = Do & 7;
    
    /* Build up the mask from reg value */
    *ptr++ = and_immed(width_reg, width_reg_orig, 5, 0);
    *ptr++ = cbnz(width_reg, 2);
    *ptr++ = mov_immed_u16(width_reg, 32, 0);
    *ptr++ = mov_immed_u16(mask_reg, 1, 0);
    *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
    *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
    /* Move mask to the topmost bits of 64-bit mask_reg */
    *ptr++ = rbit64(mask_reg, mask_reg);

    /* Fetch the data */
    /* Width == 1? Fetch byte */
    *ptr++ = cmp_immed(width_reg, 1);
    *ptr++ = b_cc(A64_CC_NE, 4);
    *ptr++ = ldurb_offset(base, data_reg, base_offset);
    *ptr++ = ror64(data_reg, data_reg, 8);
    *ptr++ = b(12);
    /* Width <= 8? Fetch half word */
    *ptr++ = cmp_immed(width_reg, 8);
    *ptr++ = b_cc(A64_CC_GT, 4);
    *ptr++ = ldurh_offset(base, data_reg, base_offset);
    *ptr++ = ror64(data_reg, data_reg, 16);
    *ptr++ = b(7);
    /* Width <= 24? Fetch long word */
    *ptr++ = cmp_immed(width_reg, 24);
    *ptr++ = b_cc(A64_CC_GT, 4);
    *ptr++ = ldur_offset(base, data_reg, base_offset);
    *ptr++ = ror64(data_reg, data_reg, 32);
    *ptr++ = b(2);
    *ptr++ = ldur64_offset(base, data_reg, base_offset);

    /* In case of INS, prepare the source data accordingly */
    if (op == OP_INS)
    {
        /* Put inserted value to topmost bits of 64bit reg */
        *ptr++ = rorv64(insert_reg, data, width_reg);
        /* CLear with insert mask, set condition codes */
        *ptr++ = ands64_reg(insert_reg, insert_reg, mask_reg, LSL, 0);
        
        /* If XNZVC needs to be set, do it now */
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        }
    }
    else
    {
        /* Shall bitfield be investigated before? */
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);

            *ptr++ = lslv64(test_reg, data_reg, bit_offset);
            *ptr++ = ands64_reg(31, mask_reg, test_reg, LSL, 0);
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        }
    }

    /* For all operations other than TST perform the action */
    if (op != OP_TST)
    {
        switch (op)
        {
            case OP_EOR:
                // Exclusive-or all bits
                *ptr++ = eor64_reg(data_reg, data_reg, mask_reg, LSR, bit_offset);
                break;
                
            case OP_SET:
                // Set all bits
                *ptr++ = orr64_reg(data_reg, data_reg, mask_reg, LSR, bit_offset);
                break;

            case OP_CLR:
                // Clear all bits
                *ptr++ = bic64_reg(data_reg, data_reg, mask_reg, LSR, bit_offset);
                break;
            
            case OP_INS:
                // Clear all bits
                *ptr++ = bic64_reg(data_reg, data_reg, mask_reg, LSR, bit_offset);
                // Insert data
                *ptr++ = orr64_reg(data_reg, data_reg, insert_reg, LSR, bit_offset);
                break;

            default:
                break;
        }

        /* Store the data back */
        /* Width == 1? Fetch byte */
        *ptr++ = cmp_immed(width_reg, 1);
        *ptr++ = b_cc(A64_CC_NE, 4);
        *ptr++ = ror64(data_reg, data_reg, 64 - 8);
        *ptr++ = sturb_offset(base, data_reg, base_offset);
        *ptr++ = b(12);
        /* Width <= 8? Fetch half word */
        *ptr++ = cmp_immed(width_reg, 8);
        *ptr++ = b_cc(A64_CC_GT, 4);
        *ptr++ = ror64(data_reg, data_reg, 64 - 16);
        *ptr++ = sturh_offset(base, data_reg, base_offset);
        *ptr++ = b(7);
        /* Width <= 24? Fetch long word */
        *ptr++ = cmp_immed(width_reg, 24);
        *ptr++ = b_cc(A64_CC_GT, 4);
        *ptr++ = ror64(data_reg, data_reg, 32);
        *ptr++ = stur_offset(base, data_reg, base_offset);
        *ptr++ = b(2);
        *ptr++ = stur64_offset(base, data_reg, base_offset);
    }

    RA_FreeARMRegister(&ptr, mask_reg);

    return ptr;
}

static inline uint32_t *EMIT_BFxxx_RI(uint32_t *ptr, uint8_t base, enum BF_OP op, uint8_t Do, uint8_t Dw, uint8_t update_mask, uint8_t data)
{
    uint8_t mask_reg = RA_AllocARMRegister(&ptr);
    uint8_t tmp = 2;
    uint8_t off_reg = RA_AllocARMRegister(&ptr);
    uint8_t csel_1 = 0;
    uint8_t csel_2 = 1;
    //uint8_t insert_reg = 3;
    uint8_t width = Dw;
    uint8_t off_reg_orig = Do;

(void)data;

    if (width == 0)
        width = 32;

    // Adjust base register according to the offset
    *ptr++ = add_reg(base, base, off_reg_orig, ASR, 3);
    *ptr++ = and_immed(off_reg, off_reg_orig, 3, 0);

    if (width == 1)
    {
        // Build up a mask
        *ptr++ = orr_immed(mask_reg, 31, width, 24 + width);

        // Load data 
        *ptr++ = ldrb_offset(base, tmp, 0);

        // Shift mask to correct position
        *ptr++ = lsrv(mask_reg, mask_reg, off_reg);

        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            
            uint8_t alt_flags = update_mask;
            if ((alt_flags & 3) != 0 && (alt_flags & 3) < 3)
                alt_flags ^= 3;

            ptr = EMIT_ClearFlags(ptr, cc, alt_flags);

            *ptr++ = ands_reg(31, tmp, mask_reg, LSL, 0);

            *ptr++ = orr_immed(csel_1, cc, 1, 32 - SRB_N);
            *ptr++ = orr_immed(csel_2, cc, 1, 32 - SRB_Z);
            *ptr++ = csel(cc, csel_2, csel_1, A64_CC_EQ);
        }

        if (op != OP_TST)
        {
            switch(op)
            {
                case OP_EOR:
                    // Exclusive-or all bits
                    *ptr++ = eor_reg(tmp, tmp, mask_reg, LSL, 0);
                    break;
                
                case OP_SET:
                    // Exclusive-or all bits
                    *ptr++ = orr_reg(tmp, tmp, mask_reg, LSL, 0);
                    break;

                case OP_CLR:
                    // Exclusive-or all bits
                    *ptr++ = bic_reg(tmp, tmp, mask_reg, LSL, 0);
                    break;

                default:
                    break;
            }

            // Store back
            *ptr++ = strb_offset(base, tmp, 0);
        }
    }
    else if (width <= 8)
    {
        // Build up a mask
        *ptr++ = orr_immed(mask_reg, 31, width, 16 + width);

        // Load data 
        *ptr++ = ldrh_offset(base, tmp, 0);

        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(&ptr);
            uint8_t cc = RA_ModifyCC(&ptr);

            // Shift source to correct position
            *ptr++ = lslv(testreg, tmp, off_reg);
            *ptr++ = lsl(testreg, testreg, 16);

            // Mask the bitfield, update condition codes
            *ptr++ = ands_reg(31, testreg, mask_reg, LSL, 16);

            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

            RA_FreeARMRegister(&ptr, testreg);
        }

        if (op != OP_TST)
        {
            // Shift mask to correct position
            *ptr++ = lsrv(mask_reg, mask_reg, off_reg);

            switch(op)
            {
                case OP_EOR:
                    // Exclusive-or all bits
                    *ptr++ = eor_reg(tmp, tmp, mask_reg, LSL, 0);
                    break;
                
                case OP_SET:
                    // Exclusive-or all bits
                    *ptr++ = orr_reg(tmp, tmp, mask_reg, LSL, 0);
                    break;
                
                case OP_CLR:
                    // Exclusive-or all bits
                    *ptr++ = bic_reg(tmp, tmp, mask_reg, LSL, 0);
                    break;

                default:
                    break;
            }

            // Store back
            *ptr++ = strh_offset(base, tmp, 0);
        }
    }
    else if (width <= 24)
    {
        // Build up a mask
        *ptr++ = orr_immed(mask_reg, 31, width, width);

        // Load data 
        *ptr++ = ldr_offset(base, tmp, 0);

        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(&ptr);
            uint8_t cc = RA_ModifyCC(&ptr);

            // Shift source to correct position
            *ptr++ = lslv(testreg, tmp, off_reg);

            // Mask the bitfield, update condition codes
            *ptr++ = ands_reg(31, testreg, mask_reg, LSL, 0);

            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

            RA_FreeARMRegister(&ptr, testreg);
        }

        if (op != OP_TST)
        {
            // Shift mask to correct position
            *ptr++ = lsrv(mask_reg, mask_reg, off_reg);

            switch(op)
            {
                case OP_EOR:
                    // Exclusive-or all bits
                    *ptr++ = eor_reg(tmp, tmp, mask_reg, LSL, 0);
                    break;
                
                case OP_SET:
                    // Exclusive-or all bits
                    *ptr++ = orr_reg(tmp, tmp, mask_reg, LSL, 0);
                    break;
                
                case OP_CLR:
                    // Exclusive-or all bits
                    *ptr++ = bic_reg(tmp, tmp, mask_reg, LSL, 0);
                    break;

                default:
                    break;
            }

            // Store back
            *ptr++ = str_offset(base, tmp, 0);
        }
    }
    else
    {
        // Build up a mask
        *ptr++ = orr64_immed(mask_reg, 31, width, width, 1);

        // Load data and shift it left according to reminder in offset reg
        *ptr++ = ldr64_offset(base, tmp, 0);

        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(&ptr);
            uint8_t cc = RA_ModifyCC(&ptr);

            // Shift source to correct position
            *ptr++ = lslv64(testreg, tmp, off_reg);

            // Mask the bitfield, update condition codes
            *ptr++ = ands64_reg(31, testreg, mask_reg, LSL, 0);

            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

            RA_FreeARMRegister(&ptr, testreg);
        }

        if (op != OP_TST)
        {
            // Shift mask to correct position
            *ptr++ = lsrv64(mask_reg, mask_reg, off_reg);

            switch(op)
            {
                case OP_EOR:
                    // Exclusive-or all bits
                    *ptr++ = eor64_reg(tmp, tmp, mask_reg, LSL, 0);
                    break;
                
                case OP_SET:
                    // Exclusive-or all bits
                    *ptr++ = orr64_reg(tmp, tmp, mask_reg, LSL, 0);
                    break;

                case OP_CLR:
                    // Exclusive-or all bits
                    *ptr++ = bic64_reg(tmp, tmp, mask_reg, LSL, 0);
                    break;

                default:
                    break;
            }

            // Store back
            *ptr++ = str64_offset(base, tmp, 0);
        }
    }

    RA_FreeARMRegister(&ptr, tmp);
    RA_FreeARMRegister(&ptr, mask_reg);
    RA_FreeARMRegister(&ptr, off_reg);

    return ptr;
}

static inline uint32_t *EMIT_BFxxx_RR(uint32_t *ptr, uint8_t base, enum BF_OP op, uint8_t Do, uint8_t Dw, uint8_t update_mask, uint8_t data)
{
    uint8_t width_reg_orig = Dw;
    uint8_t off_reg_orig = Do;
    uint8_t width_reg = 3;
    uint8_t mask_reg = 2;
    uint8_t test_reg = 1;
    uint8_t insert_reg = test_reg;
    uint8_t off_reg = RA_AllocARMRegister(&ptr);
    uint8_t data_reg = 0;

    /* Build up the mask from reg value */
    *ptr++ = and_immed(width_reg, width_reg_orig, 5, 0);
    *ptr++ = cbnz(width_reg, 2);
    *ptr++ = mov_immed_u16(width_reg, 32, 0);
    *ptr++ = mov_immed_u16(mask_reg, 1, 0);
    *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
    *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
    /* Move mask to the topmost bits of 64-bit mask_reg */
    *ptr++ = rbit64(mask_reg, mask_reg);

    // Adjust base register according to the offset
    *ptr++ = add_reg(base, base, off_reg_orig, ASR, 3);
    *ptr++ = and_immed(off_reg, off_reg_orig, 3, 0);
    
    /* Fetch the data */
    /* Width == 1? Fetch byte */
    *ptr++ = cmp_immed(width_reg, 1);
    *ptr++ = b_cc(A64_CC_NE, 4);
    *ptr++ = ldrb_offset(base, data_reg, 0);
    *ptr++ = ror64(data_reg, data_reg, 8);
    *ptr++ = b(12);
    /* Width <= 8? Fetch half word */
    *ptr++ = cmp_immed(width_reg, 8);
    *ptr++ = b_cc(A64_CC_GT, 4);
    *ptr++ = ldrh_offset(base, data_reg, 0);
    *ptr++ = ror64(data_reg, data_reg, 16);
    *ptr++ = b(7);
    /* Width <= 24? Fetch long word */
    *ptr++ = cmp_immed(width_reg, 24);
    *ptr++ = b_cc(A64_CC_GT, 4);
    *ptr++ = ldr_offset(base, data_reg, 0);
    *ptr++ = ror64(data_reg, data_reg, 32);
    *ptr++ = b(2);
    *ptr++ = ldr64_offset(base, data_reg, 0);

    /* In case of INS, prepare the source data accordingly */
    if (op == OP_INS)
    {
        /* Put inserted value to topmost bits of 64bit reg */
        *ptr++ = rorv64(insert_reg, data, width_reg);
        /* CLear with insert mask, set condition codes */
        *ptr++ = ands64_reg(insert_reg, insert_reg, mask_reg, LSL, 0);
        
        /* If XNZVC needs to be set, do it now */
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        }
    }
    else
    {
        /* Shall bitfield be investigated before? */
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);

            *ptr++ = lslv64(test_reg, data_reg, off_reg);
            *ptr++ = ands64_reg(31, mask_reg, test_reg, LSL, 0);
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        }
    }

    /* For all operations other than TST perform the action */
    if (op != OP_TST)
    {
        // Shift mask to correct position
        *ptr++ = lsrv64(mask_reg, mask_reg, off_reg);

        switch (op)
        {
            case OP_EOR:
                // Exclusive-or all bits
                *ptr++ = eor64_reg(data_reg, data_reg, mask_reg, LSL, 0);
                break;
                
            case OP_SET:
                // Set all bits
                *ptr++ = orr64_reg(data_reg, data_reg, mask_reg, LSL, 0);
                break;

            case OP_CLR:
                // Clear all bits
                *ptr++ = bic64_reg(data_reg, data_reg, mask_reg, LSL, 0);
                break;

            case OP_INS:
                // Move inserted value to location
                *ptr++ = lsrv64(insert_reg, insert_reg, off_reg);
                // Clear all bits
                *ptr++ = bic64_reg(data_reg, data_reg, mask_reg, LSL, 0);
                // Insert data
                *ptr++ = orr64_reg(data_reg, data_reg, insert_reg, LSL, 0);
                break;

            default:
                break;
        }

        /* Store the data back */
        /* Width == 1? Fetch byte */
        *ptr++ = cmp_immed(width_reg, 1);
        *ptr++ = b_cc(A64_CC_NE, 4);
        *ptr++ = ror64(data_reg, data_reg, 64 - 8);
        *ptr++ = strb_offset(base, data_reg, 0);
        *ptr++ = b(12);
        /* Width <= 8? Fetch half word */
        *ptr++ = cmp_immed(width_reg, 8);
        *ptr++ = b_cc(A64_CC_GT, 4);
        *ptr++ = ror64(data_reg, data_reg, 64 - 16);
        *ptr++ = strh_offset(base, data_reg, 0);
        *ptr++ = b(7);
        /* Width <= 24? Fetch long word */
        *ptr++ = cmp_immed(width_reg, 24);
        *ptr++ = b_cc(A64_CC_GT, 4);
        *ptr++ = ror64(data_reg, data_reg, 32);
        *ptr++ = str_offset(base, data_reg, 0);
        *ptr++ = b(2);
        *ptr++ = str64_offset(base, data_reg, 0);
    }

    RA_FreeARMRegister(&ptr, width_reg);
    RA_FreeARMRegister(&ptr, mask_reg);
    RA_FreeARMRegister(&ptr, off_reg);

    return ptr;
}

static uint32_t *EMIT_BFTST(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    
    /* Special case: Source is Dn */
    if ((opcode & 0x0038) == 0)
    {
        uint8_t src = RA_MapM68kRegister(&ptr, opcode & 7);

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
                uint8_t tmp = RA_AllocARMRegister(&ptr);

                // Get the source, expand to 64 bit to allow rotating
                *ptr++ = lsl64(tmp, src, 32);
                *ptr++ = orr64_reg(tmp, tmp, src, LSL, 0);

                // Get width
                if (width == 0) width = 32;

                // Extract bitfield
                *ptr++ = sbfx64(tmp, tmp, 64 - (offset + width), width);
                if (update_mask)
                {
                    uint8_t cc = RA_ModifyCC(&ptr);
                    *ptr++ = cmn_reg(31, tmp, LSL, 0);
                    ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
                }

                RA_FreeARMRegister(&ptr, tmp);
            }
            else
            {
                /* Emit empty bftst just in case no flags need to be tested */
                *ptr++ = cmn_reg(31, src, LSL, 0);
                if (update_mask)
                {
                    uint8_t cc = RA_ModifyCC(&ptr);
                    ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
                }
            }
        }

        // Do == immed, Dw == reg
        else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
        {
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
            uint8_t mask_reg = RA_AllocARMRegister(&ptr);
            uint8_t offset = (opcode2 >> 6) & 31;

            // Shift left by offset + 32 bits
            *ptr++ = lsl64(tmp, src, 32 + offset);
            *ptr++ = orr64_reg(tmp, tmp, src, LSL, offset);

            // Build up a mask
            *ptr++ = and_immed(width_reg, width_reg, 5, 0);
            *ptr++ = cbnz(width_reg, 2);
            *ptr++ = mov_immed_u16(width_reg, 32, 0);
            *ptr++ = mov_immed_u16(mask_reg, 1, 0);
            *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
            *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
            *ptr++ = rorv64(mask_reg, mask_reg, width_reg);

            // Mask the bitfield, update condition codes
            *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
            RA_FreeARMRegister(&ptr, width_reg);
            RA_FreeARMRegister(&ptr, mask_reg);
        }

        // Do == REG, Dw == immed
        else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
        {
            uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
            uint8_t mask_reg = RA_AllocARMRegister(&ptr);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t width = opcode2 & 31;

            *ptr++ = and_immed(off_reg, off_reg, 5, 0);

            if (width == 0)
                width = 32;

            // Build up a mask
            *ptr++ = orr64_immed(mask_reg, 31, width, width, 1);

            // Load data and shift it left according to reminder in offset reg
            *ptr++ = lsl64(tmp, src, 32);
            *ptr++ = orr64_reg(tmp, tmp, src, LSL, 0);
            *ptr++ = lslv64(tmp, tmp, off_reg);

            // Mask the bitfield, update condition codes
            *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
            RA_FreeARMRegister(&ptr, mask_reg);
            RA_FreeARMRegister(&ptr, off_reg);
        }

        // Do == REG, Dw == REG
        else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
        {
            uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
            uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
            uint8_t mask_reg = RA_AllocARMRegister(&ptr);
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            *ptr++ = and_immed(off_reg, off_reg, 5, 0);

            // Build up a mask
            *ptr++ = and_immed(width_reg, width_reg, 5, 0);
            *ptr++ = cbnz(width_reg, 2);
            *ptr++ = mov_immed_u16(width_reg, 32, 0);
            *ptr++ = mov_immed_u16(mask_reg, 1, 0);
            *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
            *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
            *ptr++ = rorv64(mask_reg, mask_reg, width_reg);

            // Load data and shift it left according to reminder in offset reg
            *ptr++ = lsl64(tmp, src, 32);
            *ptr++ = orr64_reg(tmp, tmp, src, LSL, 0);
            *ptr++ = lslv64(tmp, tmp, off_reg);

            // Mask the bitfield, update condition codes
            *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);
            
            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
            RA_FreeARMRegister(&ptr, mask_reg);
            RA_FreeARMRegister(&ptr, width_reg);
            RA_FreeARMRegister(&ptr, off_reg);
        }
    }
    else
    {
        uint8_t base = 0xff;

        // Get EA address into a temporary register
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &base, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

        // Do == Immed, Dw == immed
        if (!(opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
        {
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t offset = (opcode2 >> 6) & 31;
            uint8_t width = opcode2 & 31;
            
            if (width == 0)
                width = 32;

            // No need to precalculate base address here, we are all good if we fetch full 64 bit now
            *ptr++ = ldr64_offset(base, tmp, 0);

            // Extract bitfield
            *ptr++ = sbfx64(tmp, tmp, 64 - offset - width, width);

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                *ptr++ = cmn64_reg(31, tmp, LSL, 64 - width);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
        }

        // Do == immed, Dw == reg
        else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
        {
            uint8_t offset = (opcode2 >> 6) & 31;
            uint8_t width_reg = RA_MapM68kRegister(&ptr, opcode2 & 7);

            ptr = EMIT_BFxxx_IR(ptr, base, OP_TST, offset, width_reg, update_mask, -1);
        }

        // Do == REG, Dw == immed
        else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
        {
            uint8_t off_reg = RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);
            uint8_t width = opcode2 & 31;

            ptr = EMIT_BFxxx_RI(ptr, base, OP_TST, off_reg, width, update_mask, -1);
        }

        // Do == REG, Dw == REG
        else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
        {
            uint8_t off_reg = RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);
            uint8_t width_reg = RA_MapM68kRegister(&ptr, opcode2 & 7);

            ptr = EMIT_BFxxx_RR(ptr, base, OP_TST, off_reg, width_reg, update_mask, -1);
        }

        RA_FreeARMRegister(&ptr, base);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    return ptr;
}

static uint32_t *EMIT_BFEXTU(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);

    /* Special case: Source is Dn */
    if ((opcode & 0x0038) == 0)
    {
        /*
            IMPORTANT: Although it is not mentioned in 68000 PRM, the bitfield operations on
                source register are in fact rotation instructions. Bitfield is eventually masked,
                but the temporary contents of source operand for the Dn addressing mode are
                actually rotated.
        */
        uint8_t src = RA_MapM68kRegister(&ptr, opcode & 7);

        /* Direct offset and width */
        if ((opcode2 & 0x0820) == 0)
        {
            uint8_t dest = RA_MapM68kRegister(&ptr, (opcode2 >> 12) & 7);
            uint8_t offset = (opcode2 >> 6) & 0x1f;
            uint8_t width = (opcode2) & 0x1f;
            RA_SetDirtyM68kRegister(&ptr, (opcode2 >> 12) & 7);

            // Get width
            if (width == 0) width = 32;

            /*
                If offset == 0 and width == 0 the register value from Dn is already extracted bitfield,
                otherwise extract bitfield
            */
            if (offset != 0 || width != 32)
            {
                uint8_t tmp = RA_AllocARMRegister(&ptr);

                // Get the source, expand to 64 bit to allow rotating
                *ptr++ = lsl64(tmp, src, 32);
                *ptr++ = orr64_reg(tmp, tmp, src, LSL, 0);

                // Extract bitfield
                *ptr++ = ubfx64(dest, tmp, 64 - (offset + width), width);

                RA_FreeARMRegister(&ptr, tmp);
            }
            else
            {
                *ptr++ = mov_reg(dest, src);
            }

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                *ptr++ = cmn_reg(31, dest, LSL, 32 - width);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }
        }

        // Do == immed, Dw == reg
        else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
        {
            uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
            uint8_t mask_reg = RA_AllocARMRegister(&ptr);
            uint8_t offset = (opcode2 >> 6) & 31;

            // Shift left by offset + 32 bits
            *ptr++ = lsl64(tmp, src, 32 + offset);
            *ptr++ = orr64_reg(tmp, tmp, src, LSL, offset);

            // Build up a mask
            *ptr++ = and_immed(width_reg, width_reg, 5, 0);
            *ptr++ = cbnz(width_reg, 2);
            *ptr++ = mov_immed_u16(width_reg, 32, 0);
            *ptr++ = mov_immed_u16(mask_reg, 1, 0);
            *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
            *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
            *ptr++ = rorv64(mask_reg, mask_reg, width_reg);

            // Mask the bitfield, update condition codes
            *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);

            // Arithmetic shift right 64-width bits
            *ptr++ = mov_immed_u16(mask_reg, 64, 0);
            *ptr++ = sub_reg(width_reg, mask_reg, width_reg, LSL, 0);
            *ptr++ = lsrv64(tmp, tmp, width_reg);
            
            // Move to destination register
            *ptr++ = mov_reg(dest, tmp);

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
            RA_FreeARMRegister(&ptr, width_reg);
            RA_FreeARMRegister(&ptr, mask_reg);
        }

        // Do == REG, Dw == immed
        else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
        {
            uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
            uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
            uint8_t mask_reg = RA_AllocARMRegister(&ptr);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t width = opcode2 & 31;

            *ptr++ = and_immed(off_reg, off_reg, 5, 0);
            
            if (width == 0)
                width = 32;

            // Build up a mask
            *ptr++ = orr64_immed(mask_reg, 31, width, width, 1);

            // Load data and shift it left according to reminder in offset reg
            *ptr++ = lsl64(tmp, src, 32);
            *ptr++ = orr64_reg(tmp, tmp, src, LSL, 0);
            *ptr++ = lslv64(tmp, tmp, off_reg);

            // Mask the bitfield, update condition codes
            *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);

            // Arithmetic shift right 64-width bits
            *ptr++ = lsr64(tmp, tmp, 64 - width);
            
            // Move to destination register
            *ptr++ = mov_reg(dest, tmp);

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
            RA_FreeARMRegister(&ptr, mask_reg);
            RA_FreeARMRegister(&ptr, off_reg);
        }

        // Do == REG, Dw == REG
        else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
        {
            uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
            uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
            uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
            uint8_t mask_reg = RA_AllocARMRegister(&ptr);
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            *ptr++ = and_immed(off_reg, off_reg, 5, 0);

            // Build up a mask
            *ptr++ = and_immed(width_reg, width_reg, 5, 0);
            *ptr++ = cbnz(width_reg, 2);
            *ptr++ = mov_immed_u16(width_reg, 32, 0);
            *ptr++ = mov_immed_u16(mask_reg, 1, 0);
            *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
            *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
            *ptr++ = rorv64(mask_reg, mask_reg, width_reg);

            // Load data and shift it left according to reminder in offset reg
            *ptr++ = lsl64(tmp, src, 32);
            *ptr++ = orr64_reg(tmp, tmp, src, LSL, 0);
            *ptr++ = lslv64(tmp, tmp, off_reg);

            // Mask the bitfield, update condition codes
            *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);
            
            // Arithmetic shift right 64-width bits
            *ptr++ = mov_immed_u16(off_reg, 64, 0);
            *ptr++ = sub_reg(width_reg, off_reg, width_reg, LSL, 0);
            *ptr++ = lsrv64(tmp, tmp, width_reg);
            
            // Move to destination register
            *ptr++ = mov_reg(dest, tmp);

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
            RA_FreeARMRegister(&ptr, mask_reg);
            RA_FreeARMRegister(&ptr, width_reg);
            RA_FreeARMRegister(&ptr, off_reg);
        }
    }
    else
    {
        uint8_t base = 0xff;

        // Get EA address into a temporary register
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &base, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

        // Do == Immed, Dw == immed
        if (!(opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
        {
            uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t offset = (opcode2 >> 6) & 31;
            uint8_t width = opcode2 & 31;
            
            if (width == 0)
                width = 32;

            // No need to precalculate base address here, we are all good if we fetch full 64 bit now
            *ptr++ = ldr64_offset(base, tmp, 0);

            // Extract bitfield
            *ptr++ = ubfx64(tmp, tmp, 64 - offset - width, width);

            // Copy to destination
            *ptr++ = mov_reg(dest, tmp);

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                *ptr++ = cmn64_reg(31, tmp, LSL, 64 - width);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
        }

        // Do == immed, Dw == reg
        else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
        {
            uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
            uint8_t mask_reg = RA_AllocARMRegister(&ptr);
            uint8_t offset = (opcode2 >> 6) & 31;

            // No need to precalculate base address here, we are all good if we fetch full 64 bit now
            *ptr++ = ldr64_offset(base, tmp, 0);

            // If offset != 0, shift left by offset bits
            if (offset != 0) {
                *ptr++ = lsl64(tmp, tmp, offset);
            }

            // Build up a mask
            *ptr++ = and_immed(width_reg, width_reg, 5, 0);
            *ptr++ = cbnz(width_reg, 2);
            *ptr++ = mov_immed_u16(width_reg, 32, 0);
            *ptr++ = mov_immed_u16(mask_reg, 1, 0);
            *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
            *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
            *ptr++ = rorv64(mask_reg, mask_reg, width_reg);

            // Mask the bitfield, update condition codes
            *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);

            // Arithmetic shift right 64-width bits
            *ptr++ = mov_immed_u16(mask_reg, 64, 0);
            *ptr++ = sub_reg(width_reg, mask_reg, width_reg, LSL, 0);
            *ptr++ = lsrv64(tmp, tmp, width_reg);
            
            // Move to destination register
            *ptr++ = mov_reg(dest, tmp);

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
            RA_FreeARMRegister(&ptr, width_reg);
            RA_FreeARMRegister(&ptr, mask_reg);
        }

        // Do == REG, Dw == immed
        else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
        {
            uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
            uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
            uint8_t mask_reg = RA_AllocARMRegister(&ptr);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t width = opcode2 & 31;

            if (width == 0)
                width = 32;

            // Adjust base register according to the offset
            *ptr++ = add_reg(base, base, off_reg, ASR, 3);
            *ptr++ = and_immed(off_reg, off_reg, 3, 0);

            // Build up a mask
            *ptr++ = orr64_immed(mask_reg, 31, width, width, 1);

            // Load data and shift it left according to reminder in offset reg
            *ptr++ = ldr64_offset(base, tmp, 0);
            *ptr++ = lslv64(tmp, tmp, off_reg);

            // Mask the bitfield, update condition codes
            *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);

            // Arithmetic shift right 64-width bits
            *ptr++ = lsr64(tmp, tmp, 64 - width);
            
            // Move to destination register
            *ptr++ = mov_reg(dest, tmp);

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
            RA_FreeARMRegister(&ptr, mask_reg);
            RA_FreeARMRegister(&ptr, off_reg);
        }

        // Do == REG, Dw == REG
        if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
        {
            uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
            uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
            uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
            uint8_t mask_reg = RA_AllocARMRegister(&ptr);
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            // Adjust base register according to the offset
            *ptr++ = add_reg(base, base, off_reg, ASR, 3);
            *ptr++ = and_immed(off_reg, off_reg, 3, 0);

            // Build up a mask
            *ptr++ = and_immed(width_reg, width_reg, 5, 0);
            *ptr++ = cbnz(width_reg, 2);
            *ptr++ = mov_immed_u16(width_reg, 32, 0);
            *ptr++ = mov_immed_u16(mask_reg, 1, 0);
            *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
            *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
            *ptr++ = rorv64(mask_reg, mask_reg, width_reg);

            // Load data and shift it left according to reminder in offset reg
            *ptr++ = ldr64_offset(base, tmp, 0);
            *ptr++ = lslv64(tmp, tmp, off_reg);

            // Mask the bitfield, update condition codes
            *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);
            
            // Arithmetic shift right 64-width bits
            *ptr++ = mov_immed_u16(off_reg, 64, 0);
            *ptr++ = sub_reg(width_reg, off_reg, width_reg, LSL, 0);
            *ptr++ = lsrv64(tmp, tmp, width_reg);
            
            // Move to destination register
            *ptr++ = mov_reg(dest, tmp);

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
            RA_FreeARMRegister(&ptr, mask_reg);
            RA_FreeARMRegister(&ptr, width_reg);
            RA_FreeARMRegister(&ptr, off_reg);
        }

        RA_FreeARMRegister(&ptr, base);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    return ptr;
}

static uint32_t *EMIT_BFEXTS(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    
    /* Special case: Source is Dn */
    if ((opcode & 0x0038) == 0)
    {
        uint8_t src = RA_MapM68kRegister(&ptr, opcode & 7);

        /* Direct offset and width */
        if ((opcode2 & 0x0820) == 0)
        {    
            uint8_t dest = RA_MapM68kRegister(&ptr, (opcode2 >> 12) & 7);
            uint8_t offset = (opcode2 >> 6) & 0x1f;
            uint8_t width = (opcode2) & 0x1f;

            RA_SetDirtyM68kRegister(&ptr, (opcode2 >> 12) & 7);
            /*
                If offset == 0 and width == 0 the register value from Dn is already extracted bitfield,
                otherwise extract bitfield
            */
            if (offset != 0 || width != 0)
            {
                uint8_t tmp = RA_AllocARMRegister(&ptr);

                // Get the source, expand to 64 bit to allow rotating
                *ptr++ = lsl64(tmp, src, 32);
                *ptr++ = orr64_reg(tmp, tmp, src, LSL, 0);

                // Get width
                if (width == 0) width = 32;

                // Extract bitfield
                *ptr++ = sbfx64(tmp, tmp, 64 - (offset + width), width);
                *ptr++ = mov_reg(dest, tmp);

                RA_FreeARMRegister(&ptr, tmp);
            }
            else
            {
                *ptr++ = mov_reg(dest, src);
            }

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                *ptr++ = cmn_reg(31, dest, LSL, 0);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }
        }

        // Do == immed, Dw == reg
        else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
        {
            uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
            uint8_t mask_reg = RA_AllocARMRegister(&ptr);
            uint8_t offset = (opcode2 >> 6) & 31;

            // Shift left by offset + 32 bits
            *ptr++ = lsl64(tmp, src, 32 + offset);
            *ptr++ = orr64_reg(tmp, tmp, src, LSL, offset);

            // Build up a mask
            *ptr++ = and_immed(width_reg, width_reg, 5, 0);
            *ptr++ = cbnz(width_reg, 2);
            *ptr++ = mov_immed_u16(width_reg, 32, 0);
            *ptr++ = mov_immed_u16(mask_reg, 1, 0);
            *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
            *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
            *ptr++ = rorv64(mask_reg, mask_reg, width_reg);

            // Mask the bitfield, update condition codes
            *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);

            // Arithmetic shift right 64-width bits
            *ptr++ = mov_immed_u16(mask_reg, 64, 0);
            *ptr++ = sub_reg(width_reg, mask_reg, width_reg, LSL, 0);
            *ptr++ = asrv64(tmp, tmp, width_reg);
            
            // Move to destination register
            *ptr++ = mov_reg(dest, tmp);

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
            RA_FreeARMRegister(&ptr, width_reg);
            RA_FreeARMRegister(&ptr, mask_reg);
        }

        // Do == REG, Dw == immed
        else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
        {
            uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
            uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
            uint8_t mask_reg = RA_AllocARMRegister(&ptr);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t width = opcode2 & 31;

            *ptr++ = and_immed(off_reg, off_reg, 5, 0);

            if (width == 0)
                width = 32;

            // Build up a mask
            *ptr++ = orr64_immed(mask_reg, 31, width, width, 1);

            // Load data and shift it left according to reminder in offset reg
            *ptr++ = lsl64(tmp, src, 32);
            *ptr++ = orr64_reg(tmp, tmp, src, LSL, 0);
            *ptr++ = lslv64(tmp, tmp, off_reg);

            // Mask the bitfield, update condition codes
            *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);

            // Arithmetic shift right 64-width bits
            *ptr++ = asr64(tmp, tmp, 64 - width);
            
            // Move to destination register
            *ptr++ = mov_reg(dest, tmp);

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
            RA_FreeARMRegister(&ptr, mask_reg);
            RA_FreeARMRegister(&ptr, off_reg);
        }

        // Do == REG, Dw == REG
        else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
        {
            uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
            uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
            uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
            uint8_t mask_reg = RA_AllocARMRegister(&ptr);
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            *ptr++ = and_immed(off_reg, off_reg, 5, 0);

            // Build up a mask
            *ptr++ = and_immed(width_reg, width_reg, 5, 0);
            *ptr++ = cbnz(width_reg, 2);
            *ptr++ = mov_immed_u16(width_reg, 32, 0);
            *ptr++ = mov_immed_u16(mask_reg, 1, 0);
            *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
            *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
            *ptr++ = rorv64(mask_reg, mask_reg, width_reg);

            // Load data and shift it left according to reminder in offset reg
            *ptr++ = lsl64(tmp, src, 32);
            *ptr++ = orr64_reg(tmp, tmp, src, LSL, 0);
            *ptr++ = lslv64(tmp, tmp, off_reg);

            // Mask the bitfield, update condition codes
            *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);
            
            // Arithmetic shift right 64-width bits
            *ptr++ = mov_immed_u16(off_reg, 64, 0);
            *ptr++ = sub_reg(width_reg, off_reg, width_reg, LSL, 0);
            *ptr++ = asrv64(tmp, tmp, width_reg);
            
            // Move to destination register
            *ptr++ = mov_reg(dest, tmp);

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
            RA_FreeARMRegister(&ptr, mask_reg);
            RA_FreeARMRegister(&ptr, width_reg);
            RA_FreeARMRegister(&ptr, off_reg);
        }
    }
    else
    {
        uint8_t base = 0xff;

        // Get EA address into a temporary register
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &base, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

        // Do == Immed, Dw == immed
        if (!(opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
        {
            uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t offset = (opcode2 >> 6) & 31;
            uint8_t width = opcode2 & 31;
            
            if (width == 0)
                width = 32;

            // No need to precalculate base address here, we are all good if we fetch full 64 bit now
            *ptr++ = ldr64_offset(base, tmp, 0);

            // Extract bitfield
            *ptr++ = sbfx64(tmp, tmp, 64 - offset - width, width);

            // Copy to destination
            *ptr++ = mov_reg(dest, tmp);

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                *ptr++ = cmn64_reg(31, tmp, LSL, 64 - width);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
        }

        // Do == immed, Dw == reg
        else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
        {
            uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
            uint8_t mask_reg = RA_AllocARMRegister(&ptr);
            uint8_t offset = (opcode2 >> 6) & 31;

            // No need to precalculate base address here, we are all good if we fetch full 64 bit now
            *ptr++ = ldr64_offset(base, tmp, 0);

            // If offset != 0, shift left by offset bits
            if (offset != 0) {
                *ptr++ = lsl64(tmp, tmp, offset);
            }

            // Build up a mask
            *ptr++ = and_immed(width_reg, width_reg, 5, 0);
            *ptr++ = cbnz(width_reg, 2);
            *ptr++ = mov_immed_u16(width_reg, 32, 0);
            *ptr++ = mov_immed_u16(mask_reg, 1, 0);
            *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
            *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
            *ptr++ = rorv64(mask_reg, mask_reg, width_reg);

            // Mask the bitfield, update condition codes
            *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);

            // Arithmetic shift right 64-width bits
            *ptr++ = mov_immed_u16(mask_reg, 64, 0);
            *ptr++ = sub_reg(width_reg, mask_reg, width_reg, LSL, 0);
            *ptr++ = asrv64(tmp, tmp, width_reg);
            
            // Move to destination register
            *ptr++ = mov_reg(dest, tmp);

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
            RA_FreeARMRegister(&ptr, width_reg);
            RA_FreeARMRegister(&ptr, mask_reg);
        }

        // Do == REG, Dw == immed
        else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
        {
            uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
            uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
            uint8_t mask_reg = RA_AllocARMRegister(&ptr);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t width = opcode2 & 31;

            if (width == 0)
                width = 32;

            // Adjust base register according to the offset
            *ptr++ = add_reg(base, base, off_reg, ASR, 3);
            *ptr++ = and_immed(off_reg, off_reg, 3, 0);

            // Build up a mask
            *ptr++ = orr64_immed(mask_reg, 31, width, width, 1);

            // Load data and shift it left according to reminder in offset reg
            *ptr++ = ldr64_offset(base, tmp, 0);
            *ptr++ = lslv64(tmp, tmp, off_reg);

            // Mask the bitfield, update condition codes
            *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);

            // Arithmetic shift right 64-width bits
            *ptr++ = asr64(tmp, tmp, 64 - width);
            
            // Move to destination register
            *ptr++ = mov_reg(dest, tmp);

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
            RA_FreeARMRegister(&ptr, mask_reg);
            RA_FreeARMRegister(&ptr, off_reg);
        }

        // Do == REG, Dw == REG
        else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
        {
            uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
            uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
            uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
            uint8_t mask_reg = RA_AllocARMRegister(&ptr);
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            // Adjust base register according to the offset
            *ptr++ = add_reg(base, base, off_reg, ASR, 3);
            *ptr++ = and_immed(off_reg, off_reg, 3, 0);

            // Build up a mask
            *ptr++ = and_immed(width_reg, width_reg, 5, 0);
            *ptr++ = cbnz(width_reg, 2);
            *ptr++ = mov_immed_u16(width_reg, 32, 0);
            *ptr++ = mov_immed_u16(mask_reg, 1, 0);
            *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
            *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
            *ptr++ = rorv64(mask_reg, mask_reg, width_reg);

            // Load data and shift it left according to reminder in offset reg
            *ptr++ = ldr64_offset(base, tmp, 0);
            *ptr++ = lslv64(tmp, tmp, off_reg);

            // Mask the bitfield, update condition codes
            *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);
            
            // Arithmetic shift right 64-width bits
            *ptr++ = mov_immed_u16(off_reg, 64, 0);
            *ptr++ = sub_reg(width_reg, off_reg, width_reg, LSL, 0);
            *ptr++ = asrv64(tmp, tmp, width_reg);
            
            // Move to destination register
            *ptr++ = mov_reg(dest, tmp);

            if (update_mask) {
                uint8_t cc = RA_ModifyCC(&ptr);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
            RA_FreeARMRegister(&ptr, mask_reg);
            RA_FreeARMRegister(&ptr, width_reg);
            RA_FreeARMRegister(&ptr, off_reg);
        }

        RA_FreeARMRegister(&ptr, base);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    return ptr;
}

static uint32_t *EMIT_BFFFO_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);

    uint8_t src = RA_MapM68kRegister(&ptr, opcode & 7);

    /* Direct offset and width */
    if ((opcode2 & 0x0820) == 0)
    {    
        uint8_t offset = (opcode2 >> 6) & 0x1f;
        uint8_t width = (opcode2) & 0x1f;
        uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);

        /*
            If offset == 0 and width == 0 the register value from Dn is already extracted bitfield,
            otherwise extract bitfield
        */
        if (offset != 0 || width != 0)
        {
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            // Get the source, expand to 64 bit to allow rotating
            *ptr++ = lsl64(tmp, src, 32 + offset);
            *ptr++ = orr64_reg(tmp, tmp, src, LSL, offset);

            // Get width
            if (width == 0) width = 32;

            // Test bitfield and count zeros
            *ptr++ = ands64_immed(tmp, tmp, width, width, 1);
            *ptr++ = orr64_immed(tmp, tmp, 64 - width, 0, 1);

            // Perform BFFFO counting now
            *ptr++ = clz64(dest, tmp);
        
            // Add offset
            *ptr++ = add_immed(dest, dest, offset);

            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(&ptr);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            RA_FreeARMRegister(&ptr, tmp);
        }
        else
        {
            *ptr++ = clz(dest, src);
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(&ptr);
                *ptr++ = cmn_reg(31, src, LSL, 0);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }
        }
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
        uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t offset = (opcode2 >> 6) & 31;

        // Shift left by offset + 32 bits
        *ptr++ = lsl64(tmp, src, 32 + offset);
        *ptr++ = orr64_reg(tmp, tmp, src, LSL, offset);

        // Build up a mask
        *ptr++ = and_immed(width_reg, width_reg, 5, 0);
        *ptr++ = cbnz(width_reg, 2);
        *ptr++ = mov_immed_u16(width_reg, 32, 0);
        *ptr++ = mov_immed_u16(mask_reg, 1, 0);
        *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
        *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
        *ptr++ = rorv64(mask_reg, mask_reg, width_reg);

        // Mask the bitfield, update condition codes
        *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);

        // Invert the mask and orr it with tmp reg
        *ptr++ = orn64_reg(tmp, tmp, mask_reg, LSL, 0);
        
        // Perform BFFFO counting now
        *ptr++ = clz64(dest, tmp);
        
        // Add offset
        *ptr++ = add_immed(dest, dest, offset);

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        }

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, width_reg);
        RA_FreeARMRegister(&ptr, mask_reg);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_AllocARMRegister(&ptr);
        uint8_t off_orig = ((opcode2 >> 6) & 7) == ((opcode2 >> 12) & 7) ? RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7):RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t width = opcode2 & 31;

        *ptr++ = and_immed(off_reg, off_orig, 5, 0);

        if (width == 0)
            width = 32;

        // Build up a mask
        *ptr++ = orr64_immed(mask_reg, 31, width, width, 1);

        // Load data and shift it left according to reminder in offset reg
        *ptr++ = lsl64(tmp, src, 32);
        *ptr++ = orr64_reg(tmp, tmp, src, LSL, 0);
        *ptr++ = lslv64(tmp, tmp, off_reg);

        // Mask the bitfield, update condition codes
        *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);

        // Invert the mask and orr it with tmp reg
        *ptr++ = orn64_reg(tmp, tmp, mask_reg, LSL, 0);
        
        // Perform BFFFO counting now
        *ptr++ = clz64(dest, tmp);
        
        // Add offset
        *ptr++ = add_reg(dest, dest, off_orig, LSL, 0);

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        }

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, mask_reg);
        RA_FreeARMRegister(&ptr, off_reg);
        RA_FreeARMRegister(&ptr, off_orig);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_AllocARMRegister(&ptr);
        uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
        uint8_t off_orig = ((opcode2 >> 6) & 7) == ((opcode2 >> 12) & 7) ? RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7):RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        *ptr++ = and_immed(off_reg, off_orig, 5, 0);

        // Build up a mask
        *ptr++ = and_immed(width_reg, width_reg, 5, 0);
        *ptr++ = cbnz(width_reg, 2);
        *ptr++ = mov_immed_u16(width_reg, 32, 0);
        *ptr++ = mov_immed_u16(mask_reg, 1, 0);
        *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
        *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
        *ptr++ = rorv64(mask_reg, mask_reg, width_reg);

        // Load data and shift it left according to reminder in offset reg
        *ptr++ = lsl64(tmp, src, 32);
        *ptr++ = orr64_reg(tmp, tmp, src, LSL, 0);
        *ptr++ = lslv64(tmp, tmp, off_reg);

        // Mask the bitfield, update condition codes
        *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);
        
        // Invert the mask and orr it with tmp reg
        *ptr++ = orn64_reg(tmp, tmp, mask_reg, LSL, 0);
        
        // Perform BFFFO counting now
        *ptr++ = clz64(dest, tmp);
        
        // Add offset
        *ptr++ = add_reg(dest, dest, off_orig, LSL, 0);

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        }

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, mask_reg);
        RA_FreeARMRegister(&ptr, width_reg);
        RA_FreeARMRegister(&ptr, off_reg);
        RA_FreeARMRegister(&ptr, off_orig);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    return ptr;
}

static uint32_t *EMIT_BFFFO(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    uint8_t base = 0xff;

    // Get EA address into a temporary register
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &base, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

    // Do == Immed, Dw == immed
    if (!(opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width = opcode2 & 31;
        
        if (width == 0)
            width = 32;

        // No need to precalculate base address here, we are all good if we fetch full 64 bit now
        *ptr++ = ldr64_offset(base, tmp, 0);

        // If offset != 0, shift left by offset bits
        if (offset != 0) {
            *ptr++ = lsl64(tmp, tmp, offset);
        }

        // Mask the bitfield, update condition codes
        *ptr++ = ands64_immed(tmp, tmp, width, width, 1);

        // Invert the mask and orr it with tmp reg
        *ptr++ = orr64_immed(tmp, tmp, 64 - width, 0, 1);
        
        // Perform BFFFO counting now
        *ptr++ = clz64(dest, tmp);
        
        // Add offset
        *ptr++ = add_immed(dest, dest, offset);

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        }

        RA_FreeARMRegister(&ptr, tmp);
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
        uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t offset = (opcode2 >> 6) & 31;

        // No need to precalculate base address here, we are all good if we fetch full 64 bit now
        *ptr++ = ldr64_offset(base, tmp, 0);

        // If offset != 0, shift left by offset bits
        if (offset != 0) {
            *ptr++ = lsl64(tmp, tmp, offset);
        }

        // Build up a mask
        *ptr++ = and_immed(width_reg, width_reg, 5, 0);
        *ptr++ = cbnz(width_reg, 2);
        *ptr++ = mov_immed_u16(width_reg, 32, 0);
        *ptr++ = mov_immed_u16(mask_reg, 1, 0);
        *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
        *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
        *ptr++ = rorv64(mask_reg, mask_reg, width_reg);

        // Mask the bitfield, update condition codes
        *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);

        // Invert the mask and orr it with tmp reg
        *ptr++ = orn64_reg(tmp, tmp, mask_reg, LSL, 0);
        
        // Perform BFFFO counting now
        *ptr++ = clz64(dest, tmp);
        
        // Add offset
        *ptr++ = add_immed(dest, dest, offset);

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        }

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, width_reg);
        RA_FreeARMRegister(&ptr, mask_reg);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
        uint8_t off_orig = ((opcode2 >> 6) & 7) == ((opcode2 >> 12) & 7) ? RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7):RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t width = opcode2 & 31;

        if (width == 0)
            width = 32;

        // Adjust base register according to the offset
        *ptr++ = add_reg(base, base, off_reg, ASR, 3);
        *ptr++ = and_immed(off_reg, off_reg, 3, 0);

        // Build up a mask
        *ptr++ = orr64_immed(mask_reg, 31, width, width, 1);

        // Load data and shift it left according to reminder in offset reg
        *ptr++ = ldr64_offset(base, tmp, 0);
        *ptr++ = lslv64(tmp, tmp, off_reg);

        // Mask the bitfield, update condition codes
        *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);

        // Invert the mask and orr it with tmp reg
        *ptr++ = orn64_reg(tmp, tmp, mask_reg, LSL, 0);
        
        // Perform BFFFO counting now
        *ptr++ = clz64(dest, tmp);
        
        // Add offset
        *ptr++ = add_reg(dest, dest, off_orig, LSL, 0);

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        }

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, mask_reg);
        RA_FreeARMRegister(&ptr, off_reg);
        RA_FreeARMRegister(&ptr, off_orig);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t dest = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
        uint8_t off_orig = ((opcode2 >> 6) & 7) == ((opcode2 >> 12) & 7) ? RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7):RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        // Adjust base register according to the offset
        *ptr++ = add_reg(base, base, off_reg, ASR, 3);
        *ptr++ = and_immed(off_reg, off_reg, 3, 0);

        // Build up a mask
        *ptr++ = and_immed(width_reg, width_reg, 5, 0);
        *ptr++ = cbnz(width_reg, 2);
        *ptr++ = mov_immed_u16(width_reg, 32, 0);
        *ptr++ = mov_immed_u16(mask_reg, 1, 0);
        *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
        *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
        *ptr++ = rorv64(mask_reg, mask_reg, width_reg);

        // Load data and shift it left according to reminder in offset reg
        *ptr++ = ldr64_offset(base, tmp, 0);
        *ptr++ = lslv64(tmp, tmp, off_reg);

        // Mask the bitfield, update condition codes
        *ptr++ = ands64_reg(tmp, tmp, mask_reg, LSL, 0);
        
        // Invert the mask and orr it with tmp reg
        *ptr++ = orn64_reg(tmp, tmp, mask_reg, LSL, 0);
        
        // Perform BFFFO counting now
        *ptr++ = clz64(dest, tmp);
        
        // Add offset
        *ptr++ = add_reg(dest, dest, off_orig, LSL, 0);

        if (update_mask) {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        }

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, mask_reg);
        RA_FreeARMRegister(&ptr, width_reg);
        RA_FreeARMRegister(&ptr, off_reg);
        RA_FreeARMRegister(&ptr, off_orig);
    }

    RA_FreeARMRegister(&ptr, base);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    return ptr;
}

static uint32_t *EMIT_BFCHG_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    uint8_t src = RA_MapM68kRegister(&ptr, opcode & 7);

    RA_SetDirtyM68kRegister(&ptr, opcode & 7);

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
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            // Get width
            if (width == 0) width = 32;

            // If condition codes needs to be updated, do it now
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(&ptr);
                    
                *ptr++ = ror(tmp, src, 31 & (32 - offset));
                if (width != 32)
                    *ptr++ = ands_immed(31, tmp, width, width);
                else
                    *ptr++ = cmn_reg(31, tmp, LSL, 0);

                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }
            if (width != 32) {
                *ptr++ = eor_immed(src, src, width, 31 & (width + offset));
            }
            else {
                *ptr++ = mvn_reg(src, src, LSL, 0);
            }

            RA_FreeARMRegister(&ptr, tmp);
        }
        else
        {
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(&ptr);
                *ptr++ = cmn_reg(31, src, LSL, 0);

                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            *ptr++ = mvn_reg(src, src, LSL, 0);
        }
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t offset = (opcode2 >> 6) & 31;

        // Build up a mask
        *ptr++ = and_immed(width_reg, width_reg, 5, 0);
        *ptr++ = cbnz(width_reg, 2);
        *ptr++ = mov_immed_u16(width_reg, 32, 0);
        *ptr++ = mov_immed_u16(mask_reg, 1, 0);
        *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
        *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
        *ptr++ = rorv(mask_reg, mask_reg, width_reg);

        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(&ptr);
            uint8_t cc = RA_ModifyCC(&ptr);

            // If offset != 0, shift left by offset bits
            if (offset != 0)
            {
                *ptr++ = ror(testreg, src, 31 & (32 - offset));
                // Mask the bitfield, update condition codes
                *ptr++ = ands_reg(31, testreg, mask_reg, LSL, 0);
            }
            else
            {
                *ptr++ = ands_reg(31, src, mask_reg, LSL, 0);
            }
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

            RA_FreeARMRegister(&ptr, testreg);
        }

        // Set the mask bits to zero
        if (offset != 0)
            *ptr++ = eor_reg(src, src, mask_reg, ROR, offset);
        else
            *ptr++ = eor_reg(src, src, mask_reg, LSL, 0);
       
        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, width_reg);
        RA_FreeARMRegister(&ptr, mask_reg);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t width = opcode2 & 31;

        *ptr++ = and_immed(off_reg, off_reg, 5, 0);

        if (width == 0)
            width = 32;

        // Build mask
        if (width != 32) {
            *ptr++ = orr_immed(mask_reg, 31, width, width);
        }

        // Update condition codes if necessary
        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(&ptr);
            uint8_t cc = RA_ModifyCC(&ptr);

            *ptr++ = mov_immed_u16(testreg, 32, 0);
            *ptr++ = sub_reg(testreg, testreg, off_reg, LSL, 0);
            *ptr++ = rorv(testreg, src, testreg);

            if (width != 32) {
                *ptr++ = ands_immed(31, testreg, width, width);
            }
            else {
                *ptr++ = cmn_reg(31, testreg, LSL, 0);
            }

            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

            RA_FreeARMRegister(&ptr, testreg);
        }

        if (width != 32) {
            // Shift mask
            *ptr++ = rorv(mask_reg, mask_reg, off_reg);

            // Clear bitfield
            *ptr++ = eor_reg(src, src, mask_reg, LSL, 0);
        }
        else {
            *ptr++ = mvn_reg(src, src, LSL, 0);
        }

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, mask_reg);
        RA_FreeARMRegister(&ptr, off_reg);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        *ptr++ = and_immed(off_reg, off_reg, 5, 0);

        // Build up a mask
        *ptr++ = and_immed(width_reg, width_reg, 5, 0);
        *ptr++ = cbnz(width_reg, 2);
        *ptr++ = mov_immed_u16(width_reg, 32, 0);
        *ptr++ = mov_immed_u16(mask_reg, 1, 0);
        *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
        *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
        *ptr++ = rorv(mask_reg, mask_reg, width_reg);

        // Update condition codes if necessary
        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(&ptr);
            uint8_t cc = RA_ModifyCC(&ptr);

            *ptr++ = mov_immed_u16(testreg, 32, 0);
            *ptr++ = sub_reg(testreg, testreg, off_reg, LSL, 0);
            *ptr++ = rorv(testreg, src, testreg);

            *ptr++ = ands_reg(31, testreg, mask_reg, LSL, 0);

            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

            RA_FreeARMRegister(&ptr, testreg);
        }

        // Rotate mask to correct position
        *ptr++ = rorv(mask_reg, mask_reg, off_reg);

        // Set bits in field to zeros
        *ptr++ = eor_reg(src, src, mask_reg, LSL, 0);

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, mask_reg);
        RA_FreeARMRegister(&ptr, width_reg);
        RA_FreeARMRegister(&ptr, off_reg);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    return ptr;
}

static uint32_t *EMIT_BFCHG(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    uint8_t base = 0xff;

    // Get EA address into a temporary register
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &base, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

    // Do == Immed, Dw == immed
    if (!(opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width = opcode2 & 31;
        
        if (width == 0)
            width = 32;
        
        // No need to precalculate base address here, we are all good if we fetch full 64 bit now
        *ptr++ = ldr64_offset(base, tmp, 0);

        // If mask needs to be updated, extract the bitfield
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            uint8_t testreg = RA_AllocARMRegister(&ptr);

            // If offset != 0, shift left by offset bits
            if (offset != 0) {
                *ptr++ = lsl64(testreg, tmp, offset);
                *ptr++ = ands64_immed(31, testreg, width, width, 1);
            }
            else {
                *ptr++ = ands64_immed(31, tmp, width, width, 1);
            }
            
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            
            RA_FreeARMRegister(&ptr, testreg);
        }

        // Set entire bitfield to zeros
        *ptr++ = eor64_immed(tmp, tmp, width, width + offset, 1);

        // Store back
        *ptr++ = str64_offset(base, tmp, 0);
        
        RA_FreeARMRegister(&ptr, tmp);
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width_reg = RA_MapM68kRegister(&ptr, opcode2 & 7);

        ptr = EMIT_BFxxx_IR(ptr, base, OP_EOR, offset, width_reg, update_mask, -1);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t width = opcode2 & 31;

        ptr = EMIT_BFxxx_RI(ptr, base, OP_EOR, off_reg, width, update_mask, -1);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_MapM68kRegister(&ptr, opcode2 & 7);

        ptr = EMIT_BFxxx_RR(ptr, base, OP_EOR, off_reg, width_reg, update_mask, -1);
    }

    RA_FreeARMRegister(&ptr, base);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    return ptr;
}

static uint32_t *EMIT_BFSET_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    uint8_t src = RA_MapM68kRegister(&ptr, opcode & 7);

    RA_SetDirtyM68kRegister(&ptr, opcode & 7);

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
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            // Get width
            if (width == 0) width = 32;

            // If condition codes needs to be updated, do it now
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(&ptr);
                    
                *ptr++ = ror(tmp, src, 31 & (32 - offset));
                if (width != 32)
                    *ptr++ = ands_immed(31, tmp, width, width);
                else
                    *ptr++ = cmn_reg(31, tmp, LSL, 0);

                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }
            if (width != 32) {
                *ptr++ = orr_immed(src, src, width, 31 & (width + offset));
            }
            else {
                *ptr++ = movn_immed_u16(src, 0, 0);    
            }

            RA_FreeARMRegister(&ptr, tmp);
        }
        else
        {
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(&ptr);
                *ptr++ = cmn_reg(31, src, LSL, 0);

                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            *ptr++ = movn_immed_u16(src, 0, 0);
        }
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t offset = (opcode2 >> 6) & 31;

        // Build up a mask
        *ptr++ = and_immed(width_reg, width_reg, 5, 0);
        *ptr++ = cbnz(width_reg, 2);
        *ptr++ = mov_immed_u16(width_reg, 32, 0);
        *ptr++ = mov_immed_u16(mask_reg, 1, 0);
        *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
        *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
        *ptr++ = rorv(mask_reg, mask_reg, width_reg);

        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(&ptr);
            uint8_t cc = RA_ModifyCC(&ptr);

            // If offset != 0, shift left by offset bits
            if (offset != 0)
            {
                *ptr++ = ror(testreg, src, 31 & (32 - offset));
                // Mask the bitfield, update condition codes
                *ptr++ = ands_reg(31, testreg, mask_reg, LSL, 0);
            }
            else
            {
                *ptr++ = ands_reg(31, src, mask_reg, LSL, 0);
            }
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

            RA_FreeARMRegister(&ptr, testreg);
        }

        // Set the mask bits to one
        if (offset != 0)
            *ptr++ = orr_reg(src, src, mask_reg, ROR, offset);
        else
            *ptr++ = orr_reg(src, src, mask_reg, LSL, 0);
       
        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, width_reg);
        RA_FreeARMRegister(&ptr, mask_reg);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t width = opcode2 & 31;

        *ptr++ = and_immed(off_reg, off_reg, 5, 0);

        if (width == 0)
            width = 32;

        // Build mask
        if (width != 32) {
            *ptr++ = orr_immed(mask_reg, 31, width, width);
        }

        // Update condition codes if necessary
        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(&ptr);
            uint8_t cc = RA_ModifyCC(&ptr);

            *ptr++ = mov_immed_u16(testreg, 32, 0);
            *ptr++ = sub_reg(testreg, testreg, off_reg, LSL, 0);
            *ptr++ = rorv(testreg, src, testreg);

            if (width != 32) {
                *ptr++ = ands_immed(31, testreg, width, width);
            }
            else {
                *ptr++ = cmn_reg(31, testreg, LSL, 0);
            }

            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

            RA_FreeARMRegister(&ptr, testreg);
        }

        if (width != 32) {
            // Shift mask
            *ptr++ = rorv(mask_reg, mask_reg, off_reg);

            // Or with source
            *ptr++ = orr_reg(src, src, mask_reg, LSL, 0);
        }
        else {
            *ptr++ = movn_immed_u16(src, 0, 0);
        }

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, mask_reg);
        RA_FreeARMRegister(&ptr, off_reg);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        *ptr++ = and_immed(off_reg, off_reg, 5, 0);

        // Build up a mask
        *ptr++ = and_immed(width_reg, width_reg, 5, 0);
        *ptr++ = cbnz(width_reg, 2);
        *ptr++ = mov_immed_u16(width_reg, 32, 0);
        *ptr++ = mov_immed_u16(mask_reg, 1, 0);
        *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
        *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
        *ptr++ = rorv(mask_reg, mask_reg, width_reg);

        // Update condition codes if necessary
        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(&ptr);
            uint8_t cc = RA_ModifyCC(&ptr);

            *ptr++ = mov_immed_u16(testreg, 32, 0);
            *ptr++ = sub_reg(testreg, testreg, off_reg, LSL, 0);
            *ptr++ = rorv(testreg, src, testreg);

            *ptr++ = ands_reg(31, testreg, mask_reg, LSL, 0);

            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

            RA_FreeARMRegister(&ptr, testreg);
        }

        // Rotate mask to correct position
        *ptr++ = rorv(mask_reg, mask_reg, off_reg);

        // Set bits in field to ones
        *ptr++ = orr_reg(src, src, mask_reg, LSL, 0);

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, mask_reg);
        RA_FreeARMRegister(&ptr, width_reg);
        RA_FreeARMRegister(&ptr, off_reg);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    return ptr;
}

static uint32_t *EMIT_BFSET(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    uint8_t base = 0xff;

    // Get EA address into a temporary register
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &base, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

    // Do == Immed, Dw == immed
    if (!(opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width = opcode2 & 31;
        
        if (width == 0)
            width = 32;
        
        // No need to precalculate base address here, we are all good if we fetch full 64 bit now
        *ptr++ = ldr64_offset(base, tmp, 0);

        // If mask needs to be updated, extract the bitfield
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            uint8_t testreg = RA_AllocARMRegister(&ptr);

            // If offset != 0, shift left by offset bits
            if (offset != 0) {
                *ptr++ = lsl64(testreg, tmp, offset);
                *ptr++ = ands64_immed(31, testreg, width, width, 1);
            }
            else {
                *ptr++ = ands64_immed(31, tmp, width, width, 1);
            }
            
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            
            RA_FreeARMRegister(&ptr, testreg);
        }

        // Set entire bitfield to ones
        *ptr++ = orr64_immed(tmp, tmp, width, width + offset, 1);

        // Store back
        *ptr++ = str64_offset(base, tmp, 0);
        
        RA_FreeARMRegister(&ptr, tmp);
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width_reg = RA_MapM68kRegister(&ptr, opcode2 & 7);

        ptr = EMIT_BFxxx_IR(ptr, base, OP_SET, offset, width_reg, update_mask, -1);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t width = opcode2 & 31;

        ptr = EMIT_BFxxx_RI(ptr, base, OP_SET, off_reg, width, update_mask, -1);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_MapM68kRegister(&ptr, opcode2 & 7);

        ptr = EMIT_BFxxx_RR(ptr, base, OP_SET, off_reg, width_reg, update_mask, -1);
    }

    RA_FreeARMRegister(&ptr, base);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    return ptr;
}

static uint32_t *EMIT_BFCLR_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    uint8_t src = RA_MapM68kRegister(&ptr, opcode & 7);

    RA_SetDirtyM68kRegister(&ptr, opcode & 7);

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
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            // Get width
            if (width == 0) width = 32;

            // If condition codes needs to be updated, do it now
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(&ptr);
                    
                *ptr++ = ror(tmp, src, 31 & (32 - offset));
                if (width != 32)
                    *ptr++ = ands_immed(31, tmp, width, width);
                else
                    *ptr++ = cmn_reg(31, tmp, LSL, 0);

                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }
            if (width != 32) {
                *ptr++ = bic_immed(src, src, width, 31 & (width + offset));
            }
            else {
                *ptr++ = mov_immed_u16(src, 0, 0);    
            }

            RA_FreeARMRegister(&ptr, tmp);
        }
        else
        {
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(&ptr);
                *ptr++ = cmn_reg(31, src, LSL, 0);

                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            *ptr++ = mov_immed_u16(src, 0, 0);
        }
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t offset = (opcode2 >> 6) & 31;

        // Build up a mask
        *ptr++ = and_immed(width_reg, width_reg, 5, 0);
        *ptr++ = cbnz(width_reg, 2);
        *ptr++ = mov_immed_u16(width_reg, 32, 0);
        *ptr++ = mov_immed_u16(mask_reg, 1, 0);
        *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
        *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
        *ptr++ = rorv(mask_reg, mask_reg, width_reg);

        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(&ptr);
            uint8_t cc = RA_ModifyCC(&ptr);

            // If offset != 0, shift left by offset bits
            if (offset != 0)
            {
                *ptr++ = ror(testreg, src, 31 & (32 - offset));
                // Mask the bitfield, update condition codes
                *ptr++ = ands_reg(31, testreg, mask_reg, LSL, 0);
            }
            else
            {
                *ptr++ = ands_reg(31, src, mask_reg, LSL, 0);
            }
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

            RA_FreeARMRegister(&ptr, testreg);
        }

        // Set the mask bits to zero
        if (offset != 0)
            *ptr++ = bic_reg(src, src, mask_reg, ROR, offset);
        else
            *ptr++ = bic_reg(src, src, mask_reg, LSL, 0);
       
        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, width_reg);
        RA_FreeARMRegister(&ptr, mask_reg);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t width = opcode2 & 31;

        *ptr++ = and_immed(off_reg, off_reg, 5, 0);

        if (width == 0)
            width = 32;

        // Build mask
        if (width != 32) {
            *ptr++ = orr_immed(mask_reg, 31, width, width);
        }

        // Update condition codes if necessary
        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(&ptr);
            uint8_t cc = RA_ModifyCC(&ptr);

            *ptr++ = mov_immed_u16(testreg, 32, 0);
            *ptr++ = sub_reg(testreg, testreg, off_reg, LSL, 0);
            *ptr++ = rorv(testreg, src, testreg);

            if (width != 32) {
                *ptr++ = ands_immed(31, testreg, width, width);
            }
            else {
                *ptr++ = cmn_reg(31, testreg, LSL, 0);
            }

            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

            RA_FreeARMRegister(&ptr, testreg);
        }

        if (width != 32) {
            // Shift mask
            *ptr++ = rorv(mask_reg, mask_reg, off_reg);

            // Clear bitfield
            *ptr++ = bic_reg(src, src, mask_reg, LSL, 0);
        }
        else {
            *ptr++ = mov_immed_u16(src, 0, 0);
        }

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, mask_reg);
        RA_FreeARMRegister(&ptr, off_reg);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        *ptr++ = and_immed(off_reg, off_reg, 5, 0);

        // Build up a mask
        *ptr++ = and_immed(width_reg, width_reg, 5, 0);
        *ptr++ = cbnz(width_reg, 2);
        *ptr++ = mov_immed_u16(width_reg, 32, 0);
        *ptr++ = mov_immed_u16(mask_reg, 1, 0);
        *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
        *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
        *ptr++ = rorv(mask_reg, mask_reg, width_reg);

        // Update condition codes if necessary
        if (update_mask)
        {
            uint8_t testreg = RA_AllocARMRegister(&ptr);
            uint8_t cc = RA_ModifyCC(&ptr);

            *ptr++ = mov_immed_u16(testreg, 32, 0);
            *ptr++ = sub_reg(testreg, testreg, off_reg, LSL, 0);
            *ptr++ = rorv(testreg, src, testreg);

            *ptr++ = ands_reg(31, testreg, mask_reg, LSL, 0);

            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

            RA_FreeARMRegister(&ptr, testreg);
        }

        // Rotate mask to correct position
        *ptr++ = rorv(mask_reg, mask_reg, off_reg);

        // Set bits in field to zeros
        *ptr++ = bic_reg(src, src, mask_reg, LSL, 0);

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, mask_reg);
        RA_FreeARMRegister(&ptr, width_reg);
        RA_FreeARMRegister(&ptr, off_reg);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    return ptr;
}

static uint32_t *EMIT_BFCLR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    uint8_t base = 0xff;

    // Get EA address into a temporary register
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &base, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

    // Do == Immed, Dw == immed
    if (!(opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width = opcode2 & 31;
        
        if (width == 0)
            width = 32;
        
        // No need to precalculate base address here, we are all good if we fetch full 64 bit now
        *ptr++ = ldr64_offset(base, tmp, 0);

        // If mask needs to be updated, extract the bitfield
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            uint8_t testreg = RA_AllocARMRegister(&ptr);

            // If offset != 0, shift left by offset bits
            if (offset != 0) {
                *ptr++ = lsl64(testreg, tmp, offset);
                *ptr++ = ands64_immed(31, testreg, width, width, 1);
            }
            else {
                *ptr++ = ands64_immed(31, tmp, width, width, 1);
            }
            
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            
            RA_FreeARMRegister(&ptr, testreg);
        }

        // Set entire bitfield to zeros
        *ptr++ = bic64_immed(tmp, tmp, width, width + offset, 1);

        // Store back
        *ptr++ = str64_offset(base, tmp, 0);
        
        RA_FreeARMRegister(&ptr, tmp);
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width_reg = RA_MapM68kRegister(&ptr, opcode2 & 7);

        ptr = EMIT_BFxxx_IR(ptr, base, OP_CLR, offset, width_reg, update_mask, -1);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t width = opcode2 & 31;

        ptr = EMIT_BFxxx_RI(ptr, base, OP_CLR, off_reg, width, update_mask, -1);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_MapM68kRegister(&ptr, opcode2 & 7);

        ptr = EMIT_BFxxx_RR(ptr, base, OP_CLR, off_reg, width_reg, update_mask, -1);
    }

    RA_FreeARMRegister(&ptr, base);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    return ptr;
}


static uint32_t *EMIT_BFINS_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    uint8_t dest = RA_MapM68kRegister(&ptr, opcode & 7);
    uint8_t src = RA_MapM68kRegister(&ptr, (opcode2 >> 12) & 7);

    RA_SetDirtyM68kRegister(&ptr, opcode & 7);

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
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t masked_src = RA_AllocARMRegister(&ptr);

            // Get width
            if (width == 0) width = 32;

            // Get source bitfield, clip to requested size
            if (width != 32) {
                *ptr++ = ands_immed(masked_src, src, width, 0);
            }
            else {
                *ptr++ = mov_reg(masked_src, src);
            }

            // Rotate source bitfield so that the MSB is at offset
            if (((offset + width) & 31) != 0)
                *ptr++ = ror(masked_src, masked_src, 31 & (offset + width));

            // If condition codes needs to be updated, do it now
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(&ptr);
                uint8_t testreg = RA_AllocARMRegister(&ptr);
                
                if (offset != 0) {
                    *ptr++ = ror(testreg, masked_src, 31 & (32 - offset));
                    *ptr++ = cmn_reg(31, testreg, LSL, 0);
                }
                else {
                    *ptr++ = cmn_reg(31, masked_src, LSL, 0);
                }

                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

                RA_FreeARMRegister(&ptr, testreg);
            }

            // Clear destination
            if (width != 32) {
                *ptr++ = bic_immed(dest, dest, width, 31 & (width + offset));
                // Insert bitfield
                *ptr++ = orr_reg(dest, dest, masked_src, LSL, 0);
            }
            else {
                *ptr++ = mov_reg(dest, masked_src);
            }

            RA_FreeARMRegister(&ptr, tmp);
            RA_FreeARMRegister(&ptr, masked_src);
        }
        else
        {
            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(&ptr);
                *ptr++ = cmn_reg(31, src, LSL, 0);

                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }

            *ptr++ = mov_reg(dest, src);
        }
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t masked_src = RA_AllocARMRegister(&ptr);
        uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t offset = (opcode2 >> 6) & 31;

        // Build up a mask and mask out source bitfield
        *ptr++ = and_immed(width_reg, width_reg, 5, 0);
        *ptr++ = cbnz(width_reg, 2);
        *ptr++ = mov_immed_u16(width_reg, 32, 0);
        *ptr++ = mov_immed_u16(mask_reg, 1, 0);
        *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
        *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
        *ptr++ = ands_reg(masked_src, src, mask_reg, LSL, 0);
        *ptr++ = rorv(mask_reg, mask_reg, width_reg);
        *ptr++ = rorv(masked_src, masked_src, width_reg);

        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);

            *ptr++ = cmn_reg(31, masked_src, LSL, 0);

            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        }

        // Set the mask bits to zero and insert sorce
        if (offset != 0) {
            *ptr++ = bic_reg(dest, dest, mask_reg, ROR, offset);
            *ptr++ = orr_reg(dest, dest, masked_src, ROR, offset);
        }
        else {
            *ptr++ = bic_reg(dest, dest, mask_reg, LSL, 0);
            *ptr++ = orr_reg(dest, dest, masked_src, LSL, 0);
        }
       
        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, width_reg);
        RA_FreeARMRegister(&ptr, mask_reg);
        RA_FreeARMRegister(&ptr, masked_src);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t width = opcode2 & 31;
        uint8_t masked_src = RA_AllocARMRegister(&ptr);

        *ptr++ = and_immed(off_reg, off_reg, 5, 0);

        if (width == 0)
            width = 32;

        // Get source bitfield, clip to requested size
        if (width != 32) {
            *ptr++ = ands_immed(masked_src, src, width, 0);
            *ptr++ = ror(masked_src, masked_src, width);
        }
        else {
            *ptr++ = mov_reg(masked_src, src);
        }

        // Build mask
        if (width != 32) {
            *ptr++ = orr_immed(mask_reg, 31, width, width);
        }

        // Update condition codes if necessary
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);

            *ptr++ = cmn_reg(31, masked_src, LSL, 0);

            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        }

        if (width != 32) {
            // Shift mask
            *ptr++ = rorv(mask_reg, mask_reg, off_reg);
            // Shift source
            *ptr++ = rorv(masked_src, masked_src, off_reg);
            // Clear bitfield
            *ptr++ = bic_reg(dest, dest, mask_reg, LSL, 0);
            // Insert
            *ptr++ = orr_reg(dest, dest, masked_src, LSL, 0);
        }
        else {
            // Width == 32. Just rotate the source into destination
            *ptr++ = rorv(dest, masked_src, off_reg);
        }

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, mask_reg);
        RA_FreeARMRegister(&ptr, off_reg);
        RA_FreeARMRegister(&ptr, masked_src);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_CopyFromM68kRegister(&ptr, opcode2 & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t masked_src = RA_AllocARMRegister(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        *ptr++ = and_immed(off_reg, off_reg, 5, 0);

        // Build up a mask and mask out source bitfield
        *ptr++ = and_immed(width_reg, width_reg, 5, 0);
        *ptr++ = cbnz(width_reg, 2);
        *ptr++ = mov_immed_u16(width_reg, 32, 0);
        *ptr++ = mov_immed_u16(mask_reg, 1, 0);
        *ptr++ = lslv64(mask_reg, mask_reg, width_reg);
        *ptr++ = sub64_immed(mask_reg, mask_reg, 1);
        *ptr++ = ands_reg(masked_src, src, mask_reg, LSL, 0);
        *ptr++ = rorv(mask_reg, mask_reg, width_reg);
        *ptr++ = rorv(masked_src, masked_src, width_reg);

        // Update condition codes if necessary
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);

            *ptr++ = cmn_reg(31, masked_src, LSL, 0);

            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        }

        // Rotate mask to correct position
        *ptr++ = rorv(mask_reg, mask_reg, off_reg);
        
        // Rotate source to correct position
        *ptr++ = rorv(masked_src, masked_src, off_reg);

        // Set bits in field to zeros
        *ptr++ = bic_reg(dest, dest, mask_reg, LSL, 0);

        // Insert field
        *ptr++ = orr_reg(dest, dest, masked_src, LSL, 0);

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, mask_reg);
        RA_FreeARMRegister(&ptr, width_reg);
        RA_FreeARMRegister(&ptr, off_reg);
        RA_FreeARMRegister(&ptr, masked_src);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    return ptr;
}

static uint32_t *EMIT_BFINS(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    uint8_t base = 0xff;

    uint8_t src = RA_MapM68kRegister(&ptr, (opcode2 >> 12) & 7);

    // Get EA address into a temporary register
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &base, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

    // Do == Immed, Dw == immed
    if (!(opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width = opcode2 & 31;
        
        if (width == 0)
            width = 32;
        
        // No need to precalculate base address here, we are all good if we fetch full 64 bit now
        *ptr++ = ldr64_offset(base, tmp, 0);

        // If mask needs to be updated, extract the bitfield
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);

            *ptr++ = cmn64_reg(31, src, LSL, 64 - width);

            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        }

        // Insert source bitfield
        *ptr++ = bfi64(tmp, src, 64 - (offset + width), width); 

        // Store back
        *ptr++ = str64_offset(base, tmp, 0);
        
        RA_FreeARMRegister(&ptr, tmp);
    }

    // Do == immed, Dw == reg
    else if (!(opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t offset = (opcode2 >> 6) & 31;
        uint8_t width_reg = RA_MapM68kRegister(&ptr, opcode2 & 7);

        ptr = EMIT_BFxxx_IR(ptr, base, OP_INS, offset, width_reg, update_mask, src);
    }

    // Do == REG, Dw == immed
    else if ((opcode2 & (1 << 11)) && !(opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_CopyFromM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t mask_reg = RA_AllocARMRegister(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t width = opcode2 & 31;
        uint8_t masked_src = RA_AllocARMRegister(&ptr);

        if (width == 0)
            width = 32;

        // Get source bitfield, clip to requested size
        *ptr++ = ands64_immed(masked_src, src, width, 0, 1);
        *ptr++ = ror64(masked_src, masked_src, width);

        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);

            *ptr++ = cmn64_reg(31, masked_src, LSL, 0);

            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        }

        // Adjust base register according to the offset
        *ptr++ = add_reg(base, base, off_reg, ASR, 3);
        *ptr++ = and_immed(off_reg, off_reg, 3, 0);

        // Build up a mask
        *ptr++ = orr64_immed(mask_reg, 31, width, width, 1);

        // Load data and shift it left according to reminder in offset reg
        *ptr++ = ldr64_offset(base, tmp, 0);

        // Shift mask to correct position
        *ptr++ = lsrv64(mask_reg, mask_reg, off_reg);

        // Shift source
        *ptr++ = lsrv64(masked_src, masked_src, off_reg);

        // Set all bits to zeros
        *ptr++ = bic64_reg(tmp, tmp, mask_reg, LSL, 0);

        // Insert bitfield
        *ptr++ = orr64_reg(tmp, tmp, masked_src, LSL, 0);

        // Store back
        *ptr++ = str64_offset(base, tmp, 0);

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, mask_reg);
        RA_FreeARMRegister(&ptr, off_reg);
        RA_FreeARMRegister(&ptr, masked_src);
    }

    // Do == REG, Dw == REG
    else if ((opcode2 & (1 << 11)) && (opcode2 & (1 << 5)))
    {
        uint8_t off_reg = RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t width_reg = RA_MapM68kRegister(&ptr, opcode2 & 7);

        ptr = EMIT_BFxxx_RR(ptr, base, OP_INS, off_reg, width_reg, update_mask, src);
    }

    RA_FreeARMRegister(&ptr, base);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    return ptr;
}


static struct OpcodeDef InsnTable[4096] = {
	[00000 ... 00007] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 8, Byte, Dn
	[00010 ... 00017] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00020 ... 00027] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00030 ... 00037] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[00040 ... 00047] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D0
	[00050 ... 00057] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00060 ... 00067] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00070 ... 00077] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[00100 ... 00107] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 8, Word, Dn
	[00110 ... 00117] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00120 ... 00127] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00130 ... 00137] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[00140 ... 00147] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D0
	[00150 ... 00157] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00160 ... 00167] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00170 ... 00177] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[00200 ... 00207] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 8, Long, Dn
	[00210 ... 00217] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00220 ... 00227] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00230 ... 00237] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[00240 ... 00247] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D0
	[00250 ... 00257] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00260 ... 00267] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00270 ... 00277] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[01000 ... 01007] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 1, Byte, Dn
	[01010 ... 01017] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01020 ... 01027] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01030 ... 01037] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[01040 ... 01047] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D1
	[01050 ... 01057] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01060 ... 01067] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01070 ... 01077] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[01100 ... 01107] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 1, Word, Dn
	[01110 ... 01117] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01120 ... 01127] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01130 ... 01137] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[01140 ... 01147] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D1
	[01150 ... 01157] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01160 ... 01167] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01170 ... 01177] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[01200 ... 01207] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 1, Long, Dn
	[01210 ... 01217] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01220 ... 01227] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01230 ... 01237] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[01240 ... 01247] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D1
	[01250 ... 01257] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01260 ... 01267] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01270 ... 01277] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[02000 ... 02007] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 2, Byte, Dn
	[02010 ... 02017] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02020 ... 02027] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02030 ... 02037] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[02040 ... 02047] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D2
	[02050 ... 02057] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02060 ... 02067] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02070 ... 02077] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[02100 ... 02107] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 2, Word, Dn
	[02110 ... 02117] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02120 ... 02127] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02130 ... 02137] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[02140 ... 02147] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D2
	[02150 ... 02157] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02160 ... 02167] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02170 ... 02177] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[02200 ... 02207] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 2, Long, Dn
	[02210 ... 02217] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02220 ... 02227] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02230 ... 02237] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[02240 ... 02247] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D2
	[02250 ... 02257] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02260 ... 02267] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02270 ... 02277] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[03000 ... 03007] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 3, Byte, Dn
	[03010 ... 03017] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03020 ... 03027] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03030 ... 03037] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[03040 ... 03047] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D3
	[03050 ... 03057] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03060 ... 03067] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03070 ... 03077] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[03100 ... 03107] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 3, Word, Dn
	[03110 ... 03117] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03120 ... 03127] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03130 ... 03137] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[03140 ... 03147] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D3
	[03150 ... 03157] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03160 ... 03167] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03170 ... 03177] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[03200 ... 03207] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 3, Long, Dn
	[03210 ... 03217] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03220 ... 03227] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03230 ... 03237] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[03240 ... 03247] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D3
	[03250 ... 03257] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03260 ... 03267] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03270 ... 03277] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[04000 ... 04007] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 4, Byte, Dn
	[04010 ... 04017] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04020 ... 04027] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04030 ... 04037] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[04040 ... 04047] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D4
	[04050 ... 04057] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04060 ... 04067] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04070 ... 04077] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[04100 ... 04107] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 4, Word, Dn
	[04110 ... 04117] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04120 ... 04127] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04130 ... 04137] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[04140 ... 04147] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D4
	[04150 ... 04157] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04160 ... 04167] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04170 ... 04177] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[04200 ... 04207] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 4, Long, Dn
	[04210 ... 04217] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04220 ... 04227] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04230 ... 04237] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[04240 ... 04247] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D4
	[04250 ... 04257] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04260 ... 04267] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04270 ... 04277] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[05000 ... 05007] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 5, Byte, Dn
	[05010 ... 05017] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05020 ... 05027] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05030 ... 05037] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[05040 ... 05047] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D5
	[05050 ... 05057] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05060 ... 05067] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05070 ... 05077] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[05100 ... 05107] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 5, Word, Dn
	[05110 ... 05117] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05120 ... 05127] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05130 ... 05137] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[05140 ... 05147] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D5
	[05150 ... 05157] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05160 ... 05167] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05170 ... 05177] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[05200 ... 05207] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 5, Long, Dn
	[05210 ... 05217] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05220 ... 05227] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05230 ... 05237] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[05240 ... 05247] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D5
	[05250 ... 05257] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05260 ... 05267] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05270 ... 05277] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[06000 ... 06007] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 6, Byte, Dn
	[06010 ... 06017] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06020 ... 06027] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06030 ... 06037] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[06040 ... 06047] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D6
	[06050 ... 06057] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06060 ... 06067] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06070 ... 06077] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[06100 ... 06107] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 6, Word, Dn
	[06110 ... 06117] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06120 ... 06127] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06130 ... 06137] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[06140 ... 06147] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D6
	[06150 ... 06157] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06160 ... 06167] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06170 ... 06177] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[06200 ... 06207] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 6, Long, Dn
	[06210 ... 06217] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06220 ... 06227] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06230 ... 06237] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[06240 ... 06247] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D6
	[06250 ... 06257] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06260 ... 06267] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06270 ... 06277] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[07000 ... 07007] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 7, Byte, Dn
	[07010 ... 07017] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07020 ... 07027] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07030 ... 07037] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[07040 ... 07047] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D7
	[07050 ... 07057] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07060 ... 07067] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07070 ... 07077] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[07100 ... 07107] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 7, Word, Dn
	[07110 ... 07117] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07120 ... 07127] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07130 ... 07137] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[07140 ... 07147] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D7
	[07150 ... 07157] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07160 ... 07167] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR , 1, 0, 2},
	[07170 ... 07177] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[07200 ... 07207] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 7, Long, Dn
	[07210 ... 07217] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07220 ... 07227] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07230 ... 07237] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[07240 ... 07247] = { { EMIT_ASR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D7
	[07250 ... 07257] = { { EMIT_LSR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07260 ... 07267] = { { EMIT_ROXR_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07270 ... 07277] = { { EMIT_ROR_reg }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[00320 ... 00371] = { { EMIT_ASR_mem }, NULL, SR_X, SR_CCR, 1, 1, 2 },  //Shift #1, <ea> (memory only)
	[01320 ... 01371] = { { EMIT_LSR_mem }, NULL, SR_X, SR_CCR, 1, 1, 2 },
	[02320 ... 02371] = { { EMIT_ROXR_mem }, NULL, SR_X, SR_CCR, 1, 1, 2 },
	[03320 ... 03371] = { { EMIT_ROR_mem }, NULL, 0, SR_NZVC, 1, 1, 2 },

	[00400 ... 00407] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 8, Byte, Dn
	[00410 ... 00417] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00420 ... 00427] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00430 ... 00437] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[00440 ... 00447] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D0
	[00450 ... 00457] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00460 ... 00467] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00470 ... 00477] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[00500 ... 00507] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 8, Word, Dn
	[00510 ... 00517] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00520 ... 00527] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00530 ... 00537] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[00540 ... 00547] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D0
	[00550 ... 00557] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00560 ... 00567] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00570 ... 00577] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[00600 ... 00607] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 8, Long, Dn
	[00610 ... 00617] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00620 ... 00627] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00630 ... 00637] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[00640 ... 00647] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D0
	[00650 ... 00657] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00660 ... 00667] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00670 ... 00677] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[01400 ... 01407] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 1, Byte, Dn
	[01410 ... 01417] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01420 ... 01427] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01430 ... 01437] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[01440 ... 01447] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D1
	[01450 ... 01457] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01460 ... 01467] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01470 ... 01477] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[01500 ... 01507] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 1, Word, Dn
	[01510 ... 01517] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01520 ... 01527] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01530 ... 01537] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[01540 ... 01547] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D1
	[01550 ... 01557] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01560 ... 01567] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01570 ... 01577] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[01600 ... 01607] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 1, Long, Dn
	[01610 ... 01617] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01620 ... 01627] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01630 ... 01637] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[01640 ... 01647] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D1
	[01650 ... 01657] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01660 ... 01667] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01670 ... 01677] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[02400 ... 02407] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 2, Byte, Dn
	[02410 ... 02417] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02420 ... 02427] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02430 ... 02437] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[02440 ... 02447] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D2
	[02450 ... 02457] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02460 ... 02467] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02470 ... 02477] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[02500 ... 02507] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 2, Word, Dn
	[02510 ... 02517] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02520 ... 02527] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02530 ... 02537] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[02540 ... 02547] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D2
	[02550 ... 02557] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02560 ... 02567] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02570 ... 02577] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[02600 ... 02607] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 2, Long, Dn
	[02610 ... 02617] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02620 ... 02627] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02630 ... 02637] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[02640 ... 02647] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D2
	[02650 ... 02657] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02660 ... 02667] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02670 ... 02677] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[03400 ... 03407] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 3, Byte, Dn
	[03410 ... 03417] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03420 ... 03427] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03430 ... 03437] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[03440 ... 03447] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D3
	[03450 ... 03457] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03460 ... 03467] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03470 ... 03477] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[03500 ... 03507] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 3, Word, Dn
	[03510 ... 03517] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03520 ... 03527] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03530 ... 03537] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[03540 ... 03547] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D3
	[03550 ... 03557] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03560 ... 03567] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03570 ... 03577] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[03600 ... 03607] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 3, Long, Dn
	[03610 ... 03617] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03620 ... 03627] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03630 ... 03637] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[03640 ... 03647] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D3
	[03650 ... 03657] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03660 ... 03667] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03670 ... 03677] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[04400 ... 04407] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 4, Byte, Dn
	[04410 ... 04417] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04420 ... 04427] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04430 ... 04437] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[04440 ... 04447] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D4
	[04450 ... 04457] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04460 ... 04467] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04470 ... 04477] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[04500 ... 04507] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 4, Word, Dn
	[04510 ... 04517] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04520 ... 04527] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04530 ... 04537] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[04540 ... 04547] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D4
	[04550 ... 04557] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04560 ... 04567] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04570 ... 04577] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[04600 ... 04607] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 4, Long, Dn
	[04610 ... 04617] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04620 ... 04627] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04630 ... 04637] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[04640 ... 04647] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D4
	[04650 ... 04657] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04660 ... 04667] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04670 ... 04677] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[05400 ... 05407] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 5, Byte, Dn
	[05410 ... 05417] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05420 ... 05427] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05430 ... 05437] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[05440 ... 05447] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D5
	[05450 ... 05457] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05460 ... 05467] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05470 ... 05477] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[05500 ... 05507] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 5, Word, Dn
	[05510 ... 05517] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05520 ... 05527] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05530 ... 05537] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[05540 ... 05547] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D5
	[05550 ... 05557] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05560 ... 05567] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05570 ... 05577] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[05600 ... 05607] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 5, Long, Dn
	[05610 ... 05617] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05620 ... 05627] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05630 ... 05637] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[05640 ... 05647] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D5
	[05650 ... 05657] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05660 ... 05667] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05670 ... 05677] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[06400 ... 06407] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 6, Byte, Dn
	[06410 ... 06417] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06420 ... 06427] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06430 ... 06437] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[06440 ... 06447] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D6
	[06450 ... 06457] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06460 ... 06467] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06470 ... 06477] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[06500 ... 06507] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 6, Word, Dn
	[06510 ... 06517] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06520 ... 06527] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06530 ... 06537] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[06540 ... 06547] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D6
	[06550 ... 06557] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06560 ... 06567] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06570 ... 06577] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[06600 ... 06607] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 6, Long, Dn
	[06610 ... 06617] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06620 ... 06627] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06630 ... 06637] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[06640 ... 06647] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D6
	[06650 ... 06657] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06660 ... 06667] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06670 ... 06677] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[07400 ... 07407] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 7, Byte, Dn
	[07410 ... 07417] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07420 ... 07427] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07430 ... 07437] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[07440 ... 07447] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D7
	[07450 ... 07457] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07460 ... 07467] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07470 ... 07477] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[07500 ... 07507] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 7, Word, Dn
	[07510 ... 07517] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07520 ... 07527] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07530 ... 07537] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[07540 ... 07547] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D7
	[07550 ... 07557] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07560 ... 07567] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07570 ... 07577] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[07600 ... 07607] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 7, Long, Dn
	[07610 ... 07617] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07620 ... 07627] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07630 ... 07637] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[07640 ... 07647] = { { EMIT_ASL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D7
	[07650 ... 07657] = { { EMIT_LSL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07660 ... 07667] = { { EMIT_ROXL_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07670 ... 07677] = { { EMIT_ROL_reg }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[00720 ... 00771] = { { EMIT_ASL_mem }, NULL, SR_X, SR_CCR, 1, 1, 2 },  //Shift #1, <ea> (memory only)
	[01720 ... 01771] = { { EMIT_LSL_mem }, NULL, SR_X, SR_CCR, 1, 1, 2 },
	[02720 ... 02771] = { { EMIT_ROXL_mem }, NULL, SR_X, SR_CCR, 1, 1, 2 },
	[03720 ... 03771] = { { EMIT_ROL_mem }, NULL, 0, SR_NZVC, 1, 1, 2 },

	[04300 ... 04307] = { { EMIT_BFTST }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[04320 ... 04327] = { { EMIT_BFTST }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[04350 ... 04373] = { { EMIT_BFTST }, NULL, 0, SR_NZVC, 2, 1, 0 },

	[05300 ... 05307] = { { EMIT_BFCHG_reg }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[05320 ... 05327] = { { EMIT_BFCHG }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[05350 ... 05371] = { { EMIT_BFCHG }, NULL, 0, SR_NZVC, 2, 1, 0 },

	[06300 ... 06307] = { { EMIT_BFCLR_reg }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[06320 ... 06327] = { { EMIT_BFCLR }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[06350 ... 06371] = { { EMIT_BFCLR }, NULL, 0, SR_NZVC, 2, 1, 0 },

	[07300 ... 07307] = { { EMIT_BFSET_reg }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[07320 ... 07327] = { { EMIT_BFSET }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[07350 ... 07371] = { { EMIT_BFSET }, NULL, 0, SR_NZVC, 2, 1, 0 },

	[04700 ... 04707] = { { EMIT_BFEXTU }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[04720 ... 04727] = { { EMIT_BFEXTU }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[04750 ... 04773] = { { EMIT_BFEXTU }, NULL, 0, SR_NZVC, 2, 1, 0 },

	[05700 ... 05707] = { { EMIT_BFEXTS }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[05720 ... 05727] = { { EMIT_BFEXTS }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[05750 ... 05773] = { { EMIT_BFEXTS }, NULL, 0, SR_NZVC, 2, 1, 0 },

	[06700 ... 06707] = { { EMIT_BFFFO_reg }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[06720 ... 06727] = { { EMIT_BFFFO }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[06750 ... 06773] = { { EMIT_BFFFO }, NULL, 0, SR_NZVC, 2, 1, 0 },

	[07700 ... 07707] = { { EMIT_BFINS_reg }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[07720 ... 07727] = { { EMIT_BFINS }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[07750 ... 07771] = { { EMIT_BFINS }, NULL, 0, SR_NZVC, 2, 1, 0 },
};


uint32_t *EMIT_lineE(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    (*m68k_ptr)++;
    *insn_consumed = 1;

    /* Special case: the combination of RO(R/L).W #8, Dn; SWAP Dn; RO(R/L).W, Dn
        this is replaced by REV instruction */
    if (((opcode & 0xfef8) == 0xe058) &&
        cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]) == (0x4840 | (opcode & 7)) &&
        (cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[1]) & 0xfeff) == (opcode & 0xfeff))
    {
        uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
        uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        *insn_consumed = 3;

        *ptr++ = rev(reg, reg);

        ptr = EMIT_AdvancePC(ptr, 6);
        *m68k_ptr += 2;

        if (update_mask)
        {
#ifdef __aarch64__
            uint8_t cc = RA_ModifyCC(&ptr);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = cmn_reg(31, reg, LSL, 0);
            uint8_t alt_flags = update_mask;
            if ((alt_flags & 3) != 0 && (alt_flags & 3) < 3)
                alt_flags ^= 3;
            *ptr++ = mov_immed_u16(tmp, alt_flags, 0);
            *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);

            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
            if (update_mask & (SR_C | SR_X)) {
                *ptr++ = b_cc(A64_CC_CS ^ 1, 3);
                *ptr++ = mov_immed_u16(tmp, SR_Calt | SR_X, 0);
                *ptr++ = orr_reg(cc, cc, tmp, LSL, 0);
            }
            RA_FreeARMRegister(&ptr, tmp);
#else
            M68K_ModifyCC(&ptr);
            *ptr++ = cmp_immed(reg, 0);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            if (update_mask & (SR_C))
                *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_C);
#endif
        }

        return ptr;
    }
    else if (InsnTable[opcode & 0xfff].od_Emit) {
        ptr = InsnTable[opcode & 0xfff].od_Emit(ptr, opcode, m68k_ptr);
    }
    else
    {
        ptr = EMIT_FlushPC(ptr);
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        *ptr++ = svc(0x100);
        *ptr++ = svc(0x101);
        *ptr++ = svc(0x103);
        *ptr++ = (uint32_t)(uintptr_t)(*m68k_ptr - 8);
        *ptr++ = 48;
        ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
    }

    return ptr;
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