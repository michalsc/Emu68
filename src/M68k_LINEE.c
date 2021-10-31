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

#ifdef __aarch64__
    if (update_mask & (SR_C | SR_X)) {
        if (direction) {
            *ptr++ = tst_immed(tmp, 1, 32 - 15);
        }
        else {
            *ptr++ = tst_immed(tmp, 1, 0);
        }
    }
#endif

    if (direction)
    {
#ifdef __aarch64__
        *ptr++ = lsl(tmp, tmp, 1);

#else
        *ptr++ = lsls_immed(tmp, tmp, 17);
        *ptr++ = lsr_immed(tmp, tmp, 16);
#endif
    }
    else
    {
#ifdef __aarch64__
        *ptr++ = asr(tmp, tmp, 1);
#else
        *ptr++ = asrs_immed(tmp, tmp, 1);
#endif
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
#ifdef __aarch64__
        uint8_t cc = RA_ModifyCC(&ptr);
        uint8_t tmp2 = RA_AllocARMRegister(&ptr);
        
        *ptr++ = mov_immed_u16(tmp2, update_mask, 0);
        *ptr++ = bic_reg(cc, cc, tmp2, LSL, 0);

        if (update_mask & (SR_C | SR_X)) {
            *ptr++ = b_cc(A64_CC_EQ, 3);
            *ptr++ = mov_immed_u16(tmp2, SR_C | SR_X, 0);
            *ptr++ = orr_reg(cc, cc, tmp2, LSL, 0);
        }

        RA_FreeARMRegister(&ptr, tmp2);

        if (update_mask & (SR_Z | SR_N))
        {
            *ptr++ = cmn_reg(31, tmp, LSL, 16);
            *ptr++ = mov_immed_u16(tmp, update_mask, 0);
        
            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
        }
#else
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        if (update_mask & (SR_X | SR_C))
            *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_X | SR_C);
#endif
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

#ifdef __aarch64__
    if (update_mask & (SR_C | SR_X)) {
        if (direction) {
            *ptr++ = tst_immed(tmp, 1, 32 - 15);
        }
        else {
            *ptr++ = tst_immed(tmp, 1, 0);
        }
    }
#endif

    if (direction)
    {
#ifdef __aarch64__
        *ptr++ = lsl(tmp, tmp, 1);
#else
        *ptr++ = lsls_immed(tmp, tmp, 17);
        *ptr++ = lsr_immed(tmp, tmp, 16);
#endif
    }
    else
    {
#ifdef __aarch64__
        *ptr++ = lsr(tmp, tmp, 1);

#else
        *ptr++ = lsrs_immed(tmp, tmp, 1);
#endif
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
#ifdef __aarch64__
        uint8_t cc = RA_ModifyCC(&ptr);
        uint8_t tmp2 = RA_AllocARMRegister(&ptr);
        
        *ptr++ = mov_immed_u16(tmp2, update_mask, 0);
        *ptr++ = bic_reg(cc, cc, tmp2, LSL, 0);

        if (update_mask & (SR_C | SR_X)) {
            *ptr++ = b_cc(A64_CC_EQ, 3);
            *ptr++ = mov_immed_u16(tmp2, SR_C | SR_X, 0);
            *ptr++ = orr_reg(cc, cc, tmp2, LSL, 0);
        }

        RA_FreeARMRegister(&ptr, tmp2);

        if (update_mask & (SR_Z | SR_N))
        {
            *ptr++ = cmn_reg(31, tmp, LSL, 16);
            *ptr++ = mov_immed_u16(tmp, update_mask, 0);
        
            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
        }
#else
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        if (update_mask & (SR_X | SR_C))
            *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_X | SR_C);
#endif
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
    uint8_t cc = RA_ModifyCC(&ptr);
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

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

        if (update_mask_copy & SR_XC) {
            if (direction) {
                *ptr++ = bfxil(cc, tmp, 16, 1);
            }
            else {
                *ptr++ = bfxil(cc, tmp, 31, 1);
            }
            if (update_mask_copy & SR_X) {
                *ptr++ = bfi(cc, cc, 4, 1);
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
#ifdef __aarch64__
        *ptr++ = ror(tmp, tmp, 32 - 1);
#else
        *ptr++ = rors_immed(tmp, tmp, 32 - 1);
#endif
    }
    else
    {
#ifdef __aarch64__
        *ptr++ = ror(tmp, tmp, 1);
#else
        *ptr++ = rors_immed(tmp, tmp, 1);
#endif
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
#ifdef __aarch64__
        uint8_t cc = RA_ModifyCC(&ptr);
        if (update_mask & (SR_Z | SR_N))
        {
            *ptr++ = cmn_reg(31, tmp, LSL, 16);
        }
        *ptr++ = mov_immed_u16(tmp, update_mask, 0);
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
            if (direction) {
                *ptr++ = tst_immed(tmp, 1, 0);
            }
            else {
                *ptr++ = tst_immed(tmp, 1, 1);
            }
            *ptr++ = b_cc(A64_CC_EQ, 3);
            *ptr++ = mov_immed_u16(tmp, SR_C | SR_X, 0);
            *ptr++ = orr_reg(cc, cc, tmp, LSL, 0);
        }
#else
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        if (update_mask & (SR_X | SR_C))
            *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_C);
#endif
    }
    RA_FreeARMRegister(&ptr, tmp);
    RA_FreeARMRegister(&ptr, dest);

    return ptr;
}

static uint32_t *EMIT_ASR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_ASL")));
static uint32_t *EMIT_ASL(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t shift = (opcode >> 9) & 7;
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t regshift = (opcode >> 5) & 1;
    uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    uint8_t direction = (opcode >> 8) & 1;
    RA_SetDirtyM68kRegister(&ptr, opcode & 7);

    if (regshift)
    {
        uint8_t shiftreg = RA_MapM68kRegister(&ptr, shift);

        if (direction)
        {
            switch(size)
            {
                case 4:
#ifdef __aarch64__
                    *ptr++ = lslv64(tmp, reg, shiftreg);
                    *ptr++ = mov_reg(reg, tmp);
                    if (update_mask & (SR_C | SR_X)) {
                        *ptr++ = tst64_immed(tmp, 1, 32, 1);
                    }
#else
                    *ptr++ = lsls_reg(reg, reg, shiftreg);
#endif
                    break;
                case 2:
#ifdef __aarch64__
                    *ptr++ = lslv64(tmp, reg, shiftreg);
                    if (update_mask & (SR_C | SR_X)) {
                        *ptr++ = tst_immed(tmp, 1, 16);
                    }
#else
                    *ptr++ = mov_reg_shift(tmp, reg, 16);
                    *ptr++ = lsls_reg(tmp, tmp, shiftreg);
                    *ptr++ = lsr_immed(tmp, tmp, 16);
#endif
                    *ptr++ = bfi(reg, tmp, 0, 16);
                    break;
                case 1:
#ifdef __aarch64__
                    *ptr++ = lslv64(tmp, reg, shiftreg);
                    if (update_mask & (SR_C | SR_X)) {
                        *ptr++ = tst_immed(tmp, 1, 24);
                    }
#else
                    *ptr++ = mov_reg_shift(tmp, reg, 24);
                    *ptr++ = lsls_reg(tmp, tmp, shiftreg);
                    *ptr++ = lsr_immed(tmp, tmp, 24);
#endif
                    *ptr++ = bfi(reg, tmp, 0, 8);
                    break;
            }
        }
        else
        {
            uint8_t mask = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
            if (update_mask & (SR_C | SR_X))
            {
                uint8_t t = RA_AllocARMRegister(&ptr);
                *ptr++ = sub_immed(t, shiftreg, 1);
                *ptr++ = mov_immed_u16(mask, 1, 0);
                *ptr++ = lslv64(mask, mask, t);
                RA_FreeARMRegister(&ptr, t);
            }
#endif
            switch (size)
            {
                case 4:
#ifdef __aarch64__
                    *ptr++ = sxtw64(tmp, reg);
                    if (update_mask & (SR_C | SR_X))
                    {
                        *ptr++ = ands64_reg(31, tmp, mask, LSL, 0);
                    }
                    *ptr++ = asrv64(tmp, tmp, shiftreg);
                    *ptr++ = mov_reg(reg, tmp);
#else
                    *ptr++ = asrs_reg(reg, reg, shiftreg);
#endif
                    break;
                case 2:
#ifdef __aarch64__
                    *ptr++ = sxth64(tmp, reg);
                    if (update_mask & (SR_C | SR_X))
                    {
                        *ptr++ = ands64_reg(31, tmp, mask, LSL, 0);
                    }
                    *ptr++ = asrv64(tmp, tmp, shiftreg);
#else
                    *ptr++ = sxth(tmp, reg, 0);
                    *ptr++ = asrs_reg(tmp, tmp, shiftreg);
#endif
                    *ptr++ = bfi(reg, tmp, 0, 16);
                    break;
                case 1:
#ifdef __aarch64__
                    *ptr++ = sxtb64(tmp, reg);
                    if (update_mask & (SR_C | SR_X))
                    {
                        *ptr++ = ands64_reg(31, tmp, mask, LSL, 0);
                    }
                    *ptr++ = asrv64(tmp, tmp, shiftreg);
#else
                    *ptr++ = sxtb(tmp, reg, 0);
                    *ptr++ = asrs_reg(tmp, tmp, shiftreg);
#endif
                    *ptr++ = bfi(reg, tmp, 0, 8);
                    break;
            }

            RA_FreeARMRegister(&ptr, mask);
        }
    }
    else
    {
        if (!shift) shift = 8;

#ifdef __aarch64__
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
#endif

        if (direction)
        {
            switch (size)
            {
                case 4:
#ifdef __aarch64__
                    *ptr++ = lsl(reg, reg, shift);
#else
                    *ptr++ = lsls_immed(reg, reg, shift);
#endif
                    break;
                case 2:
#ifdef __aarch64__
                    *ptr++ = lsl(tmp, reg, shift);
#else
                    *ptr++ = mov_reg_shift(tmp, reg, 16);
                    *ptr++ = lsls_immed(tmp, tmp, shift);
                    *ptr++ = lsr_immed(tmp, tmp, 16);
#endif
                    *ptr++ = bfi(reg, tmp, 0, 16);
                    break;
                case 1:
#ifdef __aarch64__
                    *ptr++ = lsl(tmp, reg, shift);
#else
                    *ptr++ = mov_reg_shift(tmp, reg, 24);
                    *ptr++ = lsls_immed(tmp, tmp, shift);
                    *ptr++ = lsr_immed(tmp, tmp, 24);
#endif
                    *ptr++ = bfi(reg, tmp, 0, 8);
                    break;
            }
        }
        else
        {
            switch (size)
            {
            case 4:
#ifdef __aarch64__
                *ptr++ = asr(reg, reg, shift);
#else
                *ptr++ = asrs_immed(reg, reg, shift);
#endif
                break;
            case 2:
#ifdef __aarch64__
                *ptr++ = sxth(tmp, reg);
                *ptr++ = asr(tmp, tmp, shift);
#else
                *ptr++ = sxth(tmp, reg, 0);
                *ptr++ = asrs_immed(tmp, tmp, shift);
#endif
                *ptr++ = bfi(reg, tmp, 0, 16);
                break;
            case 1:
#ifdef __aarch64__
                *ptr++ = sxtb(tmp, reg);
                *ptr++ = asr(tmp, tmp, shift);
#else
                *ptr++ = sxtb(tmp, reg, 0);
                *ptr++ = asrs_immed(tmp, tmp, shift);
#endif
                *ptr++ = bfi(reg, tmp, 0, 8);
                break;
            }
        }
    }

    ptr = EMIT_AdvancePC(ptr, 2);

    if (update_mask)
    {
#ifdef __aarch64__
        uint8_t cc = RA_ModifyCC(&ptr);
        uint8_t tmp2 = RA_AllocARMRegister(&ptr);

        *ptr++ = mov_immed_u16(tmp2, update_mask, 0);
        *ptr++ = bic_reg(cc, cc, tmp2, LSL, 0);

        if (update_mask & (SR_C | SR_X)) {
            *ptr++ = b_cc(A64_CC_EQ, 3);
            *ptr++ = mov_immed_u16(tmp2, SR_C | SR_X, 0);
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
#else
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        if (update_mask & (SR_X | SR_C))
            *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_X | SR_C);
#endif
    }

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
    uint8_t regshift = (opcode >> 5) & 1;
    uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
    uint8_t tmp = RA_AllocARMRegister(&ptr);

    RA_SetDirtyM68kRegister(&ptr, opcode & 7);

    if (regshift)
    {
        uint8_t shiftreg = RA_MapM68kRegister(&ptr, shift);

        if (direction)
        {
            switch (size)
            {
            case 4:
#ifdef __aarch64__
                *ptr++ = lslv64(tmp, reg, shiftreg);
                *ptr++ = mov_reg(reg, tmp);
                if (update_mask & (SR_C | SR_X)) {
                    *ptr++ = tst64_immed(tmp, 1, 32, 1);
                }
#else
                *ptr++ = lsls_reg(reg, reg, shiftreg);
#endif
                break;
            case 2:
#ifdef __aarch64__
                *ptr++ = lslv64(tmp, reg, shiftreg);
                if (update_mask & (SR_C | SR_X)) {
                    *ptr++ = tst_immed(tmp, 1, 16);
                }
#else
                *ptr++ = mov_reg_shift(tmp, reg, 16);
                *ptr++ = lsls_reg(tmp, tmp, shiftreg);
                *ptr++ = lsr_immed(tmp, tmp, 16);
#endif
                *ptr++ = bfi(reg, tmp, 0, 16);
                break;
            case 1:
#ifdef __aarch64__
                *ptr++ = lslv64(tmp, reg, shiftreg);
                if (update_mask & (SR_C | SR_X)) {
                    *ptr++ = tst_immed(tmp, 1, 24);
                }
#else
                *ptr++ = mov_reg_shift(tmp, reg, 24);
                *ptr++ = lsls_reg(tmp, tmp, shiftreg);
                *ptr++ = lsr_immed(tmp, tmp, 24);
#endif
                *ptr++ = bfi(reg, tmp, 0, 8);
                break;
            }
        }
        else
        {
            uint8_t mask = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
            if (update_mask & (SR_C | SR_X))
            {
                uint8_t t = RA_AllocARMRegister(&ptr);
                *ptr++ = sub_immed(t, shiftreg, 1);
                *ptr++ = mov_immed_u16(mask, 1, 0);
                *ptr++ = lslv64(mask, mask, t);
                RA_FreeARMRegister(&ptr, t);
            }
#endif
            switch (size)
            {
            case 4:
#ifdef __aarch64__
                *ptr++ = mov_reg(tmp, reg);
                if (update_mask & (SR_C | SR_X))
                {
                    *ptr++ = ands_reg(31, tmp, mask, LSL, 0);
                }
                *ptr++ = lsrv64(tmp, tmp, shiftreg);
                *ptr++ = mov_reg(reg, tmp);
#else
                *ptr++ = lsrs_reg(reg, reg, shiftreg);
#endif
                break;
            case 2:
#ifdef __aarch64__
                *ptr++ = uxth(tmp, reg);
                if (update_mask & (SR_C | SR_X))
                {
                    *ptr++ = ands_reg(31, tmp, mask, LSL, 0);
                }
                *ptr++ = lsrv64(tmp, tmp, shiftreg);
#else
                *ptr++ = uxth(tmp, reg, 0);
                *ptr++ = lsrs_reg(tmp, tmp, shiftreg);
#endif
                *ptr++ = bfi(reg, tmp, 0, 16);
                break;
            case 1:
#ifdef __aarch64__
                *ptr++ = uxtb(tmp, reg);
                if (update_mask & (SR_C | SR_X))
                {
                    *ptr++ = ands_reg(31, tmp, mask, LSL, 0);
                }
                *ptr++ = lsrv64(tmp, tmp, shiftreg);
#else
                *ptr++ = uxtb(tmp, reg, 0);
                *ptr++ = lsrs_reg(tmp, tmp, shiftreg);
#endif
                *ptr++ = bfi(reg, tmp, 0, 8);
                break;
            }
            RA_FreeARMRegister(&ptr, mask);
        }
    }
    else
    {
        if (!shift)
            shift = 8;

#ifdef __aarch64__
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
#endif

        if (direction)
        {
            switch (size)
            {
            case 4:
#ifdef __aarch64__
                *ptr++ = lsl(reg, reg, shift);
#else
                *ptr++ = lsls_immed(reg, reg, shift);
#endif
                break;
            case 2:
#ifdef __aarch64__
                *ptr++ = lsl(tmp, reg, shift);
#else
                *ptr++ = mov_reg_shift(tmp, reg, 16);
                *ptr++ = lsls_immed(tmp, tmp, shift);
                *ptr++ = lsr_immed(tmp, tmp, 16);
#endif
                *ptr++ = bfi(reg, tmp, 0, 16);
                break;
            case 1:
#ifdef __aarch64__
                *ptr++ = lsl(tmp, reg, shift);
#else
                *ptr++ = mov_reg_shift(tmp, reg, 24);
                *ptr++ = lsls_immed(tmp, tmp, shift);
                *ptr++ = lsr_immed(tmp, tmp, 24);
#endif
                *ptr++ = bfi(reg, tmp, 0, 8);
                break;
            }
        }
        else
        {
            switch (size)
            {
            case 4:
#ifdef __aarch64__
                *ptr++ = lsr(reg, reg, shift);
#else
                *ptr++ = lsrs_immed(reg, reg, shift);
#endif
                break;
            case 2:
#ifdef __aarch64__
                *ptr++ = uxth(tmp, reg);
                *ptr++ = lsr(tmp, tmp, shift);
#else
                *ptr++ = uxth(tmp, reg, 0);
                *ptr++ = lsrs_immed(tmp, tmp, shift);
#endif
                *ptr++ = bfi(reg, tmp, 0, 16);
                break;
            case 1:
#ifdef __aarch64__
                *ptr++ = uxtb(tmp, reg);
                *ptr++ = lsr(tmp, tmp, shift);
#else
                *ptr++ = uxtb(tmp, reg, 0);
                *ptr++ = lsrs_immed(tmp, tmp, shift);
#endif
                *ptr++ = bfi(reg, tmp, 0, 8);
                break;
            }
        }
    }

    ptr = EMIT_AdvancePC(ptr, 2);

    if (update_mask)
    {
#ifdef __aarch64__
        uint8_t cc = RA_ModifyCC(&ptr);
        uint8_t tmp2 = RA_AllocARMRegister(&ptr);

        *ptr++ = mov_immed_u16(tmp2, update_mask, 0);
        *ptr++ = bic_reg(cc, cc, tmp2, LSL, 0);

        if (update_mask & (SR_C | SR_X)) {
            *ptr++ = b_cc(A64_CC_EQ, 3);
            *ptr++ = mov_immed_u16(tmp2, SR_C | SR_X, 0);
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
#else
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        if (update_mask & (SR_X | SR_C))
            *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_X | SR_C);
#endif
    }
    RA_FreeARMRegister(&ptr, tmp);

    return ptr;
}

static uint32_t *EMIT_ROR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_ROL")));
static uint32_t *EMIT_ROL(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t direction = (opcode >> 8) & 1;
    uint8_t shift = (opcode >> 9) & 7;
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t regshift = (opcode >> 5) & 1;
    uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
    uint8_t tmp = RA_AllocARMRegister(&ptr);

    RA_SetDirtyM68kRegister(&ptr, opcode & 7);

    if (regshift)
    {
        shift = RA_CopyFromM68kRegister(&ptr, shift);

        if (direction)
        {
#ifdef __aarch64__
            *ptr++ = neg_reg(shift, shift, LSL, 0);
            *ptr++ = add_immed(shift, shift, 32);
#else
            *ptr++ = rsb_immed(shift, shift, 32);
#endif
        }

        switch (size)
        {
            case 4:
#ifdef __aarch64__
                *ptr++ = rorv(reg, reg, shift);
#else
                *ptr++ = rors_reg(reg, reg, shift);
#endif
                break;
            case 2:
                *ptr++ = mov_reg(tmp, reg);
                *ptr++ = bfi(tmp, tmp, 16, 16);
#ifdef __aarch64__
                *ptr++ = rorv(tmp, tmp, shift);
#else
                *ptr++ = rors_reg(tmp, tmp, shift);
#endif
                *ptr++ = bfi(reg, tmp, 0, 16);
                break;
            case 1:
                *ptr++ = mov_reg(tmp, reg);
                *ptr++ = bfi(tmp, tmp, 8, 8);
                *ptr++ = bfi(tmp, tmp, 16, 16);
#ifdef __aarch64__
                *ptr++ = rorv(tmp, tmp, shift);
#else
                *ptr++ = rors_reg(tmp, tmp, shift);
#endif
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
#ifdef __aarch64__
            *ptr++ = ror(reg, reg, shift);
#else
            *ptr++ = rors_immed(reg, reg, shift);
#endif
            break;
        case 2:
            *ptr++ = mov_reg(tmp, reg);
            *ptr++ = bfi(tmp, tmp, 16, 16);
#ifdef __aarch64__
            *ptr++ = ror(tmp, tmp, shift);
#else
            *ptr++ = rors_immed(tmp, tmp, shift);
#endif
            *ptr++ = bfi(reg, tmp, 0, 16);
            break;
        case 1:
            *ptr++ = mov_reg(tmp, reg);
            *ptr++ = bfi(tmp, tmp, 8, 8);
            *ptr++ = bfi(tmp, tmp, 16, 16);
#ifdef __aarch64__
            *ptr++ = ror(tmp, tmp, shift);
#else
            *ptr++ = rors_immed(tmp, tmp, shift);
#endif
            *ptr++ = bfi(reg, tmp, 0, 8);
            break;
        }
    }

    ptr = EMIT_AdvancePC(ptr, 2);

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
#ifdef __aarch64__
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
#endif
        uint8_t old_mask = update_mask & SR_C;
        ptr = EMIT_GetNZxx(ptr, cc, &update_mask);
        update_mask |= old_mask;

        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
        if (update_mask & SR_C) {
#ifdef __aarch64__
            if (!direction) {
                switch(size) {
                    case 4:
                        *ptr++ = bfxil(cc, reg, 31, 1);
                        break;
                    case 2:
                        *ptr++ = bfxil(cc, reg, 15, 1);
                        break;
                    case 1:
                        *ptr++ = bfxil(cc, reg, 7, 1);
                        break;
                }
            }
            else {
                *ptr++ = bfi(cc, reg, 0, 1);
            }
#else
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C, ARM_CC_CS);
#endif
        }

    }
    RA_FreeARMRegister(&ptr, tmp);

    return ptr;
}

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

        if (update_mask & SR_X) {
            *ptr++ = bfxil(cc, cc, 4, 1);
        }

        *ptr++ = 0;

        *tmp_ptr = b_cc(A64_CC_NE, ptr - tmp_ptr);
        tmp_ptr = ptr - 1;

        // Continue calculating modulo
        *ptr++ = mov_immed_u16(tmp2, size == 0 ? 9 : size == 1 ? 17 : 33, 0);
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
                    *ptr++ = neg_reg(amount, amount, LSR, 0);
                    *ptr++ = add_immed(amount, amount, 64);
                    *ptr++ = b_cc(A64_CC_EQ, 2);
                    *ptr++ = orr64_immed(tmp, tmp, 1, 32, 1);
                    *ptr++ = lsl64(tmp, tmp, 31);
                    *ptr++ = bfxil64(tmp, tmp, 31, 32);
                    *ptr++ = rorv64(tmp, tmp, amount);
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
                    *ptr++ = bfi64(tmp, tmp, 33, 31);
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
                    *ptr++ = bfxil(cc, tmp, 8, 1);
                    break;
                case 1:
                    *ptr++ = bfxil(cc, tmp, 16, 1);
                    break;
                case 2:
                    *ptr++ = bfxil64(cc, tmp, 32, 1);
                    break;
            }
            
            if (update_mask & SR_X) {
                *ptr++ = bfi(cc, cc, 4, 1);
            }

            *tmp_ptr = b(ptr - tmp_ptr);
        }

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
                        *ptr++ = bfxil(cc, tmp, 31, 1);
                        break;
                    case 1:
                        *ptr++ = bfxil(cc, tmp, 31, 1);
                        break;
                    case 2:
                        *ptr++ = bfxil64(cc, tmp, 63, 1);
                        break;
                }
            }
            else {
                switch(size)
                {
                    case 0:
                        *ptr++ = bfxil(cc, tmp, 8, 1);
                        break;
                    case 1:
                        *ptr++ = bfxil(cc, tmp, 16, 1);
                        break;
                    case 2:
                        *ptr++ = bfxil64(cc, tmp, 32, 1);
                        break;
                }
            }
            
            if (update_mask & SR_X) {
                *ptr++ = bfi(cc, cc, 4, 1);
            }
        }

        RA_FreeARMRegister(&ptr, tmp);
    }

    ptr = EMIT_AdvancePC(ptr, 2);
    return ptr;
}

static uint32_t *EMIT_BFTST(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = BE16((*m68k_ptr)[0]);
    uint8_t tmp = RA_AllocARMRegister(&ptr);

    ptr = EMIT_InjectDebugString(ptr, "BFTST at %08x\n", *m68k_ptr - 1);

    /* Special case: Source is Dn */
    if ((opcode & 0x0038) == 0)
    {
        uint8_t src = RA_MapM68kRegister(&ptr, opcode & 7);
        
        RA_FreeARMRegister(&ptr, src);

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
                /* width == width - 1 */
                width = (width == 0) ? 31 : width-1;
                offset = 31 - (offset + width);
                *ptr++ = sbfx(tmp, src, offset, width+1);
            }
            else
            {
                *ptr++ = mov_reg(tmp, src);
            }
        }
        /* Direct offset, width in reg */
        else if ((opcode2 & 0x0820) == 0x0020)
        {
            uint8_t offset = (opcode2 >> 6) & 0x1f;
            uint8_t width_reg_t = RA_MapM68kRegister(&ptr, opcode2 & 7);
            uint8_t width_reg = RA_AllocARMRegister(&ptr);
            *ptr++ = mov_reg(tmp, src);
#ifdef __aarch64__
            *ptr++ = neg_reg(width_reg, width_reg_t, LSL, 0);
            *ptr++ = add_immed(width_reg, width_reg, 32);
            *ptr++ = and_immed(width_reg, width_reg, 5, 0);
#else
            *ptr++ = rsb_immed(width_reg, width_reg_t, 32);
            *ptr++ = and_immed(width_reg, width_reg, 31);
#endif
            RA_FreeARMRegister(&ptr, width_reg_t);

#ifdef __aarch64__
            if (offset > 0)
            {
                *ptr++ = lsl(tmp, tmp, offset);
            }
            *ptr++ = asrv(tmp, tmp, width_reg);
#else
            if (offset > 0)
            {
                *ptr++ = lsl_immed(tmp, tmp, offset);
            }
            *ptr++ = asr_reg(tmp, tmp, width_reg);
#endif

            RA_FreeARMRegister(&ptr, width_reg);
        }
        /* Offset in reg, direct width */
        else if ((opcode2 & 0x0820) == 0x0800)
        {
            uint8_t width = (opcode2) & 0x1f;
            uint8_t offset_reg = RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);

            width = 32 - width;
#ifdef __aarch64__
            *ptr++ = lsrv(tmp, tmp, offset_reg);
#else
            *ptr++ = lsr_reg(tmp, tmp, offset_reg);
#endif
            *ptr++ = sbfx(tmp, tmp, 0, width);

            RA_FreeARMRegister(&ptr, offset_reg);
        }
        /* Both offset and width in regs */
        else
        {
            uint8_t width_reg_t = RA_MapM68kRegister(&ptr, opcode2 & 7);
            uint8_t width_reg = RA_AllocARMRegister(&ptr);
            uint8_t offset_reg = RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);

#ifdef __aarch64__
            *ptr++ = neg_reg(width_reg, width_reg_t, LSL, 0);
            *ptr++ = add_immed(width_reg, width_reg, 32);
            *ptr++ = and_immed(width_reg, width_reg, 5, 0);
#else
            *ptr++ = rsb_immed(width_reg, width_reg_t, 32);
            *ptr++ = and_immed(width_reg, width_reg, 31);
#endif
            RA_FreeARMRegister(&ptr, width_reg_t);

#ifdef __aarch64__
            *ptr++ = lslv(tmp, tmp, offset_reg);
            *ptr++ = asrv(tmp, tmp, width_reg);
#else
            *ptr++ = lsl_reg(tmp, tmp, offset_reg);
            *ptr++ = asr_reg(tmp, tmp, width_reg);
#endif
            RA_FreeARMRegister(&ptr, width_reg);
            RA_FreeARMRegister(&ptr, offset_reg);
        }
    }
    else
    {
        uint8_t dest;
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

        ptr = EMIT_InjectDebugString(ptr, "[JIT] BFTST at %08x partially not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);

        RA_FreeARMRegister(&ptr, dest);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    /* At this point extracted bitfield is in tmp register, compare it against 0, set zero and  */
    if (update_mask)
    {
#ifdef __aarch64__
        uint8_t cc = RA_ModifyCC(&ptr);
        *ptr++ = mov_immed_u16(tmp, update_mask, 0);
        *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);
        *ptr++ = cmn_reg(31, tmp, LSL, 0);

        if (update_mask & SR_Z) {
            *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
            *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
        }
        if (update_mask & SR_N) {
            *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
            *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
        }
#else
        M68K_ModifyCC(&ptr);
        *ptr++ = cmp_immed(tmp, 0);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
#endif
    }

    RA_FreeARMRegister(&ptr, tmp);

    return ptr;
}


static uint32_t *EMIT_BFEXTU(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = BE16((*m68k_ptr)[0]);
    uint8_t tmp = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);

    ptr = EMIT_InjectDebugString(ptr, "BFEXTU %04x-%04x at %08x\n", opcode, opcode2, *m68k_ptr - 1);

    /* Special case: Source is Dn */
    if ((opcode & 0x0038) == 0)
    {
        /* Direct offset and width */
        if ((opcode2 & 0x0820) == 0)
        {
            uint8_t src = RA_MapM68kRegister(&ptr, opcode & 7);
            uint8_t offset = (opcode2 >> 6) & 0x1f;
            uint8_t width = (opcode2) & 0x1f;

            RA_FreeARMRegister(&ptr, src);
            /*
                If offset == 0 and width == 0 the register value from Dn is already extracted bitfield,
                otherwise extract bitfield
            */
            if (offset != 0 || width != 0)
            {
                /* width == width - 1 */
                width = (width == 0) ? 31 : width-1;
                offset = 31 - (offset + width);
                *ptr++ = ubfx(tmp, src, offset, width+1);
            }
            else
            {
                *ptr++ = mov_reg(tmp, src);
            }
        }
    }
    else
    {
        uint8_t dest = 0xff;
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

        ptr = EMIT_InjectDebugString(ptr, "[JIT] BFEXTU at %08x partially not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);

        RA_FreeARMRegister(&ptr, dest);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    /* At this point extracted bitfield is in tmp register, compare it against 0, set zero and  */
    if (update_mask)
    {
#ifdef __aarch64__
        uint8_t cc = RA_ModifyCC(&ptr);
        *ptr++ = mov_immed_u16(tmp, update_mask, 0);
        *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);
        *ptr++ = cmn_reg(31, tmp, LSL, 0);

        if (update_mask & SR_Z) {
            *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
            *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
        }
        if (update_mask & SR_N) {
            *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
            *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
        }
#else
        M68K_ModifyCC(&ptr);
        *ptr++ = cmp_immed(tmp, 0);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
#endif
    }

    RA_FreeARMRegister(&ptr, tmp);

    return ptr;
}

static uint32_t *EMIT_BFEXTS(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = BE16((*m68k_ptr)[0]);
    uint8_t tmp = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);

    ptr = EMIT_InjectDebugString(ptr, "BFEXTS at %08x\n", *m68k_ptr - 1);

    /* Special case: Source is Dn */
    if ((opcode & 0x0038) == 0)
    {
        /* Direct offset and width */
        if ((opcode2 & 0x0820) == 0)
        {
            uint8_t src = RA_MapM68kRegister(&ptr, opcode & 7);
            uint8_t offset = (opcode2 >> 6) & 0x1f;
            uint8_t width = (opcode2) & 0x1f;

            RA_FreeARMRegister(&ptr, src);
            /*
                If offset == 0 and width == 0 the register value from Dn is already extracted bitfield,
                otherwise extract bitfield
            */
            if (offset != 0 || width != 0)
            {
                /* width == width - 1 */
                width = (width == 0) ? 31 : width-1;
                offset = 31 - (offset + width);
                *ptr++ = sbfx(tmp, src, offset, width+1);
            }
            else
            {
                *ptr++ = mov_reg(tmp, src);
            }
        }
    }
    else
    {
        uint8_t dest = 0xff;
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

        ptr = EMIT_InjectDebugString(ptr, "[JIT] BFEXTS at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);

        RA_FreeARMRegister(&ptr, dest);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    /* At this point extracted bitfield is in tmp register, compare it against 0, set zero and  */
    if (update_mask)
    {
#ifdef __aarch64__
        uint8_t cc = RA_ModifyCC(&ptr);
        *ptr++ = mov_immed_u16(tmp, update_mask, 0);
        *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);
        *ptr++ = cmn_reg(31, tmp, LSL, 0);

        if (update_mask & SR_Z) {
            *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
            *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
        }
        if (update_mask & SR_N) {
            *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
            *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
        }
#else
        M68K_ModifyCC(&ptr);
        *ptr++ = cmp_immed(tmp, 0);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
#endif
    }

    RA_FreeARMRegister(&ptr, tmp);
    return ptr;
}

static uint32_t *EMIT_BFCLR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = BE16((*m68k_ptr)[0]);
    uint8_t tmp = RA_AllocARMRegister(&ptr);

    ptr = EMIT_InjectDebugString(ptr, "BFCLR at %08x\n", *m68k_ptr - 1);

    /* Special case: Target is Dn */
    if ((opcode & 0x0038) == 0)
    {
        /* Direct offset and width */
        if ((opcode2 & 0x0820) == 0)
        {
            uint8_t dst = RA_MapM68kRegister(&ptr, opcode & 7);
            uint8_t offset = (opcode2 >> 6) & 0x1f;
            uint8_t width = (opcode2) & 0x1f;

            /* Insert bitfield into destination register */
            width = (width == 0) ? 31 : width-1;
            offset = 31 - (offset + width);
            *ptr++ = sbfx(tmp, dst, offset, width+1);
#ifdef __aarch64__
            *ptr++ = bfi(dst, 31, offset, width + 1);
#else
            *ptr++ = bfc(dst, offset, width + 1);
#endif
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);
        }
    }
    else
    {
        uint8_t dest = 0xff;
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

        ptr = EMIT_InjectDebugString(ptr, "[JIT] BFCLR at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);

        RA_FreeARMRegister(&ptr, dest);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    /* At this point extracted bitfield is in tmp register, compare it against 0, set zero and  */
    if (update_mask)
    {
#ifdef __aarch64__
        uint8_t cc = RA_ModifyCC(&ptr);
        *ptr++ = mov_immed_u16(tmp, update_mask, 0);
        *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);
        *ptr++ = cmn_reg(31, tmp, LSL, 0);

        if (update_mask & SR_Z) {
            *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
            *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
        }
        if (update_mask & SR_N) {
            *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
            *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
        }
#else
        M68K_ModifyCC(&ptr);
        *ptr++ = cmp_immed(tmp, 0);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
#endif
    }

    RA_FreeARMRegister(&ptr, tmp);

    return ptr;
}

static uint32_t *EMIT_BFFFO(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = BE16((*m68k_ptr)[0]);
    uint8_t dst = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    uint8_t src = 0xff;

    ptr = EMIT_InjectDebugString(ptr, "BFFFO at %08x\n", *m68k_ptr - 1);

    /* Special case: Source is Dn */
    if ((opcode & 0x0038) == 0)
    {
        /* Direct offset and width */
        if ((opcode2 & 0x0820) == 0)
        {
            uint8_t offset = (opcode2 >> 6) & 0x1f;
            uint8_t width = opcode2 & 0x1f;

            /* Another special case - full width and zero offset */
            if (offset == 0 && width == 0)
            {
                src = RA_MapM68kRegister(&ptr, opcode & 7);

                *ptr++ = clz(dst, src);
            }
            else
            {
                src = RA_CopyFromM68kRegister(&ptr, opcode & 7);

                if (offset)
                {
                    *ptr++ = bic_immed(src, src, offset, offset);
                }
                if (width)
                {
                    *ptr++ = orr_immed(src, src, width, 0);
                }

                *ptr++ = clz(dst, src);
            }

            ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
            (*m68k_ptr) += ext_words;

            if (update_mask)
            {
                uint8_t cc = RA_ModifyCC(&ptr);
                if (width)
                {
                    *ptr++ = bic_immed(src, src, width, 0);
                }
                *ptr++ = cmn_reg(31, src, LSL, offset);
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
            }
        }
        else
        {
            ptr = EMIT_InjectDebugString(ptr, "[JIT] BFFFO at %08x not implemented\n", *m68k_ptr - 1);
            ptr = EMIT_FlushPC(ptr);
            ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
            *ptr++ = INSN_TO_LE(0xffffffff);
        }
    }
    else
    {
        ptr = EMIT_InjectDebugString(ptr, "[JIT] BFFFO at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_FlushPC(ptr);
        ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
    }

    RA_FreeARMRegister(&ptr, tmp);
    RA_FreeARMRegister(&ptr, src);
    RA_FreeARMRegister(&ptr, dst);

    return ptr;
}

static uint32_t *EMIT_BFINS(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    uint8_t ext_words = 1;
    uint16_t opcode2 = BE16((*m68k_ptr)[0]);
    uint8_t src = RA_MapM68kRegister(&ptr, (opcode2 >> 12) & 7);
    uint8_t tmp = RA_AllocARMRegister(&ptr);

    ptr = EMIT_InjectDebugString(ptr, "BFINS at %08x\n", *m68k_ptr - 1);

    /* Special case: Target is Dn */
    if ((opcode & 0x0038) == 0)
    {
        /* Direct offset and width */
        if ((opcode2 & 0x0820) == 0)
        {
            uint8_t dst = RA_MapM68kRegister(&ptr, opcode & 7);

            uint8_t offset = (opcode2 >> 6) & 0x1f;
            uint8_t width = (opcode2) & 0x1f;

            /* Sign-extract bitfield into temporary register. Will be used later to set condition codes */
            *ptr++ = sbfx(tmp, src, 0, width);

            /* Insert bitfield into destination register */
            width = (width == 0) ? 31 : width-1;
            offset = 31 - (offset + width);
            *ptr++ = bfi(dst, tmp, offset, width + 1);

            RA_SetDirtyM68kRegister(&ptr, opcode & 7);
        }
        else
        {
            ptr = EMIT_InjectDebugString(ptr, "[JIT] BFINS at %08x not implemented\n", *m68k_ptr - 1);
            ptr = EMIT_InjectPrintContext(ptr);
        }
    }
    else
    {
        uint8_t dest = 0xff;
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

        ptr = EMIT_InjectDebugString(ptr, "[JIT] BFINS at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);

        RA_FreeARMRegister(&ptr, dest);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    /* At this point extracted bitfield is in tmp register, compare it against 0, set zero and  */
    if (update_mask)
    {
#ifdef __aarch64__
        uint8_t cc = RA_ModifyCC(&ptr);
        *ptr++ = mov_immed_u16(tmp, update_mask, 0);
        *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);
        *ptr++ = cmn_reg(31, tmp, LSL, 0);

        if (update_mask & SR_Z) {
            *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
            *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
        }
        if (update_mask & SR_N) {
            *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
            *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
        }
#else
        M68K_ModifyCC(&ptr);
        *ptr++ = cmp_immed(tmp, 0);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
#endif
    }

    RA_FreeARMRegister(&ptr, tmp);

    return ptr;
}

static uint32_t *EMIT_BFCHG(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    (void)update_mask;

    ptr = EMIT_InjectDebugString(ptr, "BFCHG at %08x\n", *m68k_ptr - 1);

    ptr = EMIT_InjectDebugString(ptr, "[JIT] BFCHG at %08x (%04x) not implemented\n", *m68k_ptr - 1, opcode);
    ptr = EMIT_FlushPC(ptr);
    ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
    *ptr++ = INSN_TO_LE(0xffffffff);
    return ptr;
}

static uint32_t *EMIT_BFSET(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(&(*m68k_ptr)[-1]);
    (void)update_mask;
    ptr = EMIT_InjectDebugString(ptr, "[JIT] BFCHG at %08x (%04x) not implemented\n", *m68k_ptr - 1, opcode);
    ptr = EMIT_FlushPC(ptr);
    ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
    *ptr++ = INSN_TO_LE(0xffffffff);
    return ptr;
}

static struct OpcodeDef InsnTable[4096] = {
	[00000 ... 00007] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 8, Byte, Dn
	[00010 ... 00017] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00020 ... 00027] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00030 ... 00037] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[00040 ... 00047] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D0
	[00050 ... 00057] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00060 ... 00067] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00070 ... 00077] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[00100 ... 00107] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 8, Word, Dn
	[00110 ... 00117] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00120 ... 00127] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00130 ... 00137] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[00140 ... 00147] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D0
	[00150 ... 00157] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00160 ... 00167] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00170 ... 00177] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[00200 ... 00207] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 8, Long, Dn
	[00210 ... 00217] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00220 ... 00227] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00230 ... 00237] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[00240 ... 00247] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D0
	[00250 ... 00257] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00260 ... 00267] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00270 ... 00277] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[01000 ... 01007] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 1, Byte, Dn
	[01010 ... 01017] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01020 ... 01027] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01030 ... 01037] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[01040 ... 01047] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D1
	[01050 ... 01057] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01060 ... 01067] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01070 ... 01077] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[01100 ... 01107] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 1, Word, Dn
	[01110 ... 01117] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01120 ... 01127] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01130 ... 01137] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[01140 ... 01147] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D1
	[01150 ... 01157] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01160 ... 01167] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01170 ... 01177] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[01200 ... 01207] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 1, Long, Dn
	[01210 ... 01217] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01220 ... 01227] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01230 ... 01237] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[01240 ... 01247] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D1
	[01250 ... 01257] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01260 ... 01267] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01270 ... 01277] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[02000 ... 02007] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 2, Byte, Dn
	[02010 ... 02017] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02020 ... 02027] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02030 ... 02037] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[02040 ... 02047] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D2
	[02050 ... 02057] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02060 ... 02067] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02070 ... 02077] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[02100 ... 02107] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 2, Word, Dn
	[02110 ... 02117] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02120 ... 02127] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02130 ... 02137] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[02140 ... 02147] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D2
	[02150 ... 02157] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02160 ... 02167] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02170 ... 02177] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[02200 ... 02207] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 2, Long, Dn
	[02210 ... 02217] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02220 ... 02227] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02230 ... 02237] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[02240 ... 02247] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D2
	[02250 ... 02257] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02260 ... 02267] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02270 ... 02277] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[03000 ... 03007] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 3, Byte, Dn
	[03010 ... 03017] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03020 ... 03027] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03030 ... 03037] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[03040 ... 03047] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D3
	[03050 ... 03057] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03060 ... 03067] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03070 ... 03077] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[03100 ... 03107] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 3, Word, Dn
	[03110 ... 03117] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03120 ... 03127] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03130 ... 03137] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[03140 ... 03147] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D3
	[03150 ... 03157] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03160 ... 03167] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03170 ... 03177] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[03200 ... 03207] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 3, Long, Dn
	[03210 ... 03217] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03220 ... 03227] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03230 ... 03237] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[03240 ... 03247] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D3
	[03250 ... 03257] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03260 ... 03267] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03270 ... 03277] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[04000 ... 04007] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 4, Byte, Dn
	[04010 ... 04017] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04020 ... 04027] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04030 ... 04037] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[04040 ... 04047] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D4
	[04050 ... 04057] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04060 ... 04067] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04070 ... 04077] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[04100 ... 04107] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 4, Word, Dn
	[04110 ... 04117] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04120 ... 04127] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04130 ... 04137] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[04140 ... 04147] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D4
	[04150 ... 04157] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04160 ... 04167] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04170 ... 04177] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[04200 ... 04207] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 4, Long, Dn
	[04210 ... 04217] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04220 ... 04227] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04230 ... 04237] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[04240 ... 04247] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D4
	[04250 ... 04257] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04260 ... 04267] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04270 ... 04277] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[05000 ... 05007] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 5, Byte, Dn
	[05010 ... 05017] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05020 ... 05027] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05030 ... 05037] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[05040 ... 05047] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D5
	[05050 ... 05057] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05060 ... 05067] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05070 ... 05077] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[05100 ... 05107] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 5, Word, Dn
	[05110 ... 05117] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05120 ... 05127] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05130 ... 05137] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[05140 ... 05147] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D5
	[05150 ... 05157] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05160 ... 05167] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05170 ... 05177] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[05200 ... 05207] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 5, Long, Dn
	[05210 ... 05217] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05220 ... 05227] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05230 ... 05237] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[05240 ... 05247] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D5
	[05250 ... 05257] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05260 ... 05267] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05270 ... 05277] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[06000 ... 06007] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 6, Byte, Dn
	[06010 ... 06017] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06020 ... 06027] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06030 ... 06037] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[06040 ... 06047] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D6
	[06050 ... 06057] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06060 ... 06067] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06070 ... 06077] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[06100 ... 06107] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 6, Word, Dn
	[06110 ... 06117] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06120 ... 06127] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06130 ... 06137] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[06140 ... 06147] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D6
	[06150 ... 06157] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06160 ... 06167] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06170 ... 06177] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[06200 ... 06207] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 6, Long, Dn
	[06210 ... 06217] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06220 ... 06227] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06230 ... 06237] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[06240 ... 06247] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D6
	[06250 ... 06257] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06260 ... 06267] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06270 ... 06277] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[07000 ... 07007] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 7, Byte, Dn
	[07010 ... 07017] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07020 ... 07027] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07030 ... 07037] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[07040 ... 07047] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D7
	[07050 ... 07057] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07060 ... 07067] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07070 ... 07077] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[07100 ... 07107] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 7, Word, Dn
	[07110 ... 07117] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07120 ... 07127] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07130 ... 07137] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[07140 ... 07147] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D7
	[07150 ... 07157] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07160 ... 07167] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR , 1, 0, 2},
	[07170 ... 07177] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[07200 ... 07207] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 7, Long, Dn
	[07210 ... 07217] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07220 ... 07227] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07230 ... 07237] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[07240 ... 07247] = { { EMIT_ASR }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D7
	[07250 ... 07257] = { { EMIT_LSR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07260 ... 07267] = { { EMIT_ROXR }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07270 ... 07277] = { { EMIT_ROR }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[00320 ... 00371] = { { EMIT_ASR_mem }, NULL, SR_X, SR_CCR, 1, 1, 2 },  //Shift #1, <ea> (memory only)
	[01320 ... 01371] = { { EMIT_LSR_mem }, NULL, SR_X, SR_CCR, 1, 1, 2 },
	[02320 ... 02371] = { { EMIT_ROXR_mem }, NULL, SR_X, SR_CCR, 1, 1, 2 },
	[03320 ... 03371] = { { EMIT_ROR_mem }, NULL, 0, SR_NZVC, 1, 1, 2 },

	[00400 ... 00407] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 8, Byte, Dn
	[00410 ... 00417] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00420 ... 00427] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00430 ... 00437] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[00440 ... 00447] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D0
	[00450 ... 00457] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00460 ... 00467] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[00470 ... 00477] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[00500 ... 00507] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 8, Word, Dn
	[00510 ... 00517] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00520 ... 00527] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00530 ... 00537] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[00540 ... 00547] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D0
	[00550 ... 00557] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00560 ... 00567] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[00570 ... 00577] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[00600 ... 00607] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 8, Long, Dn
	[00610 ... 00617] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00620 ... 00627] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00630 ... 00637] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[00640 ... 00647] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D0
	[00650 ... 00657] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00660 ... 00667] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[00670 ... 00677] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[01400 ... 01407] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 1, Byte, Dn
	[01410 ... 01417] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01420 ... 01427] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01430 ... 01437] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[01440 ... 01447] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D1
	[01450 ... 01457] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01460 ... 01467] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[01470 ... 01477] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[01500 ... 01507] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 1, Word, Dn
	[01510 ... 01517] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01520 ... 01527] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01530 ... 01537] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[01540 ... 01547] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D1
	[01550 ... 01557] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01560 ... 01567] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[01570 ... 01577] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[01600 ... 01607] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 1, Long, Dn
	[01610 ... 01617] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01620 ... 01627] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01630 ... 01637] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[01640 ... 01647] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D1
	[01650 ... 01657] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01660 ... 01667] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[01670 ... 01677] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[02400 ... 02407] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 2, Byte, Dn
	[02410 ... 02417] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02420 ... 02427] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02430 ... 02437] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[02440 ... 02447] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D2
	[02450 ... 02457] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02460 ... 02467] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[02470 ... 02477] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[02500 ... 02507] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 2, Word, Dn
	[02510 ... 02517] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02520 ... 02527] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02530 ... 02537] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[02540 ... 02547] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D2
	[02550 ... 02557] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02560 ... 02567] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[02570 ... 02577] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[02600 ... 02607] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 2, Long, Dn
	[02610 ... 02617] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02620 ... 02627] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02630 ... 02637] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[02640 ... 02647] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D2
	[02650 ... 02657] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02660 ... 02667] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[02670 ... 02677] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[03400 ... 03407] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 3, Byte, Dn
	[03410 ... 03417] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03420 ... 03427] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03430 ... 03437] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[03440 ... 03447] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D3
	[03450 ... 03457] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03460 ... 03467] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[03470 ... 03477] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[03500 ... 03507] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 3, Word, Dn
	[03510 ... 03517] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03520 ... 03527] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03530 ... 03537] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[03540 ... 03547] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D3
	[03550 ... 03557] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03560 ... 03567] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[03570 ... 03577] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[03600 ... 03607] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 3, Long, Dn
	[03610 ... 03617] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03620 ... 03627] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03630 ... 03637] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[03640 ... 03647] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D3
	[03650 ... 03657] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03660 ... 03667] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[03670 ... 03677] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[04400 ... 04407] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 4, Byte, Dn
	[04410 ... 04417] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04420 ... 04427] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04430 ... 04437] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[04440 ... 04447] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D4
	[04450 ... 04457] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04460 ... 04467] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[04470 ... 04477] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[04500 ... 04507] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 4, Word, Dn
	[04510 ... 04517] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04520 ... 04527] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04530 ... 04537] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[04540 ... 04547] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D4
	[04550 ... 04557] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04560 ... 04567] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[04570 ... 04577] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[04600 ... 04607] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 4, Long, Dn
	[04610 ... 04617] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04620 ... 04627] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04630 ... 04637] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[04640 ... 04647] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D4
	[04650 ... 04657] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04660 ... 04667] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[04670 ... 04677] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[05400 ... 05407] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 5, Byte, Dn
	[05410 ... 05417] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05420 ... 05427] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05430 ... 05437] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[05440 ... 05447] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D5
	[05450 ... 05457] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05460 ... 05467] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[05470 ... 05477] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[05500 ... 05507] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 5, Word, Dn
	[05510 ... 05517] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05520 ... 05527] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05530 ... 05537] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[05540 ... 05547] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D5
	[05550 ... 05557] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05560 ... 05567] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[05570 ... 05577] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[05600 ... 05607] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 5, Long, Dn
	[05610 ... 05617] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05620 ... 05627] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05630 ... 05637] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[05640 ... 05647] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D5
	[05650 ... 05657] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05660 ... 05667] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[05670 ... 05677] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[06400 ... 06407] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 6, Byte, Dn
	[06410 ... 06417] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06420 ... 06427] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06430 ... 06437] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[06440 ... 06447] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D6
	[06450 ... 06457] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06460 ... 06467] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[06470 ... 06477] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[06500 ... 06507] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 6, Word, Dn
	[06510 ... 06517] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06520 ... 06527] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06530 ... 06537] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[06540 ... 06547] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D6
	[06550 ... 06557] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06560 ... 06567] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[06570 ... 06577] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[06600 ... 06607] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 6, Long, Dn
	[06610 ... 06617] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06620 ... 06627] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06630 ... 06637] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[06640 ... 06647] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D6
	[06650 ... 06657] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06660 ... 06667] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[06670 ... 06677] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[07400 ... 07407] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //immediate 7, Byte, Dn
	[07410 ... 07417] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07420 ... 07427] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07430 ... 07437] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[07440 ... 07447] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 1 },  //D7
	[07450 ... 07457] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07460 ... 07467] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 1 },
	[07470 ... 07477] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 1 },
	[07500 ... 07507] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //immediate 7, Word, Dn
	[07510 ... 07517] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07520 ... 07527] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07530 ... 07537] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[07540 ... 07547] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 2 },  //D7
	[07550 ... 07557] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07560 ... 07567] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 2 },
	[07570 ... 07577] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 2 },
	[07600 ... 07607] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //immediate 7, Long, Dn
	[07610 ... 07617] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07620 ... 07627] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07630 ... 07637] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },
	[07640 ... 07647] = { { EMIT_ASL }, NULL, SR_X, SR_CCR, 1, 0, 4 },  //D7
	[07650 ... 07657] = { { EMIT_LSL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07660 ... 07667] = { { EMIT_ROXL }, NULL, SR_X, SR_CCR, 1, 0, 4 },
	[07670 ... 07677] = { { EMIT_ROL }, NULL, 0, SR_NZVC, 1, 0, 4 },

	[00720 ... 00771] = { { EMIT_ASL_mem }, NULL, SR_X, SR_CCR, 1, 1, 2 },  //Shift #1, <ea> (memory only)
	[01720 ... 01771] = { { EMIT_LSL_mem }, NULL, SR_X, SR_CCR, 1, 1, 2 },
	[02720 ... 02771] = { { EMIT_ROXL_mem }, NULL, SR_X, SR_CCR, 1, 1, 2 },
	[03720 ... 03771] = { { EMIT_ROL_mem }, NULL, 0, SR_NZVC, 1, 1, 2 },

	[04300 ... 04307] = { { EMIT_BFTST }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[04320 ... 04327] = { { EMIT_BFTST }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[04350 ... 04371] = { { EMIT_BFTST }, NULL, 0, SR_NZVC, 2, 1, 0 },

	[05300 ... 05307] = { { EMIT_BFCHG }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[05320 ... 05327] = { { EMIT_BFCHG }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[05350 ... 05371] = { { EMIT_BFCHG }, NULL, 0, SR_NZVC, 2, 1, 0 },

	[06300 ... 06307] = { { EMIT_BFCLR }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[06320 ... 06327] = { { EMIT_BFCLR }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[06350 ... 06371] = { { EMIT_BFCLR }, NULL, 0, SR_NZVC, 2, 1, 0 },

	[07300 ... 07307] = { { EMIT_BFSET }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[07320 ... 07327] = { { EMIT_BFSET }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[07350 ... 07371] = { { EMIT_BFSET }, NULL, 0, SR_NZVC, 2, 1, 0 },

	[04700 ... 04707] = { { EMIT_BFEXTU }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[04720 ... 04727] = { { EMIT_BFEXTU }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[04750 ... 04771] = { { EMIT_BFEXTU }, NULL, 0, SR_NZVC, 2, 1, 0 },

	[05700 ... 05707] = { { EMIT_BFEXTS }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[05720 ... 05727] = { { EMIT_BFEXTS }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[05750 ... 05771] = { { EMIT_BFEXTS }, NULL, 0, SR_NZVC, 2, 1, 0 },

	[06700 ... 06707] = { { EMIT_BFFFO }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[06720 ... 06727] = { { EMIT_BFFFO }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[06750 ... 06771] = { { EMIT_BFFFO }, NULL, 0, SR_NZVC, 2, 1, 0 },

	[07700 ... 07707] = { { EMIT_BFINS }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[07720 ... 07727] = { { EMIT_BFINS }, NULL, 0, SR_NZVC, 2, 0, 0 },
	[07750 ... 07771] = { { EMIT_BFINS }, NULL, 0, SR_NZVC, 2, 1, 0 },
};


uint32_t *EMIT_lineE(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;
    *insn_consumed = 1;

    /* Special case: the combination of RO(R/L).W #8, Dn; SWAP Dn; RO(R/L).W, Dn
        this is replaced by REV instruction */
    if (((opcode & 0xfef8) == 0xe058) &&
        BE16((*m68k_ptr)[0]) == (0x4840 | (opcode & 7)) &&
        (BE16((*m68k_ptr)[1]) & 0xfeff) == (opcode & 0xfeff))
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
            *ptr++ = mov_immed_u16(tmp, update_mask, 0);
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
                *ptr++ = mov_immed_u16(tmp, SR_C | SR_X, 0);
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
    uint16_t opcode = BE16(*insn_stream);
    
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