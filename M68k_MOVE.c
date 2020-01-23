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

uint32_t *EMIT_moveq(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    int8_t value = opcode & 0xff;
    uint8_t reg = (opcode >> 9) & 7;
    uint8_t tmp_reg = RA_MapM68kRegisterForWrite(&ptr, reg);

    (*m68k_ptr)++;

    *ptr++ = mov_immed_s8(tmp_reg, value);
    ptr = EMIT_AdvancePC(ptr, 2);

    uint8_t mask = M68K_GetSRMask(*m68k_ptr);
    uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

    if (update_mask)
    {
#ifdef __aarch64__
        uint8_t cc = RA_ModifyCC(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_immed_u16(tmp, update_mask, 0);
        *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);
        if (value <= 0) {
            if (value < 0)
                *ptr++ = mov_immed_u16(tmp, SR_N, 0);
            else
                *ptr++ = mov_immed_u16(tmp, SR_Z, 0);
            *ptr++ = orr_reg(cc, cc, tmp, LSL, 0);
        }
        RA_FreeARMRegister(&ptr, tmp);
#else
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (value == 0)
            *ptr++ = orr_immed(REG_SR, REG_SR, SR_Z);
        else if (value & 0x80)
            *ptr++ = orr_immed(REG_SR, REG_SR, SR_N);
#endif
    }

    return ptr;
}

uint32_t *EMIT_move(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    uint8_t ext_count = 0;
    uint8_t tmp_reg = 0xff;
    uint8_t size = 1;
    uint8_t tmp = 0;
    uint8_t is_movea = (opcode & 0x01c0) == 0x0040;
    int is_load_immediate = 0;
    uint32_t immediate_value = 0;

    (*m68k_ptr)++;

    if ((opcode & 0x3000) == 0x1000)
        size = 1;
    else if ((opcode & 0x3000) == 0x2000)
        size = 4;
    else
        size = 2;

    if (is_movea && size == 2) {
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp_reg, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
    } else {
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp_reg, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);
    }

    if ((opcode & 0x3f) == 0x3c) {
        is_load_immediate = 1;
        switch (size) {
            case 4:
                immediate_value = BE32(*(uint32_t*)(*m68k_ptr));
                break;
            case 2:
                immediate_value = BE16(**m68k_ptr);
                break;
            case 1:
                immediate_value = ((uint8_t*)*m68k_ptr)[1];
                break;
        }
    }

    /* Reverse destination mode, since this one is reversed in MOVE instruction */
    tmp = (opcode >> 6) & 0x3f;
    tmp = ((tmp & 7) << 3) | (tmp >> 3);

    /* In case of movea the value is *always* sign-extended to 32 bits */
    if (is_movea && size == 2) {
#ifdef __aarch64__
        *ptr++ = sxth(tmp_reg, tmp_reg);
#else
        *ptr++ = sxth(tmp_reg, tmp_reg, 0);
#endif
        size = 4;
    }

    ptr = EMIT_StoreToEffectiveAddress(ptr, size, &tmp_reg, tmp, *m68k_ptr, &ext_count);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));

    (*m68k_ptr) += ext_count;

    if (!is_movea)
    {
        uint8_t mask = M68K_GetSRMask(*m68k_ptr);
        uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
#ifdef __aarch64__
            uint8_t cc = RA_ModifyCC(&ptr);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = mov_immed_u16(tmp, update_mask, 0);
            *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);

            if (is_load_immediate) {
                int32_t tmp_immediate = 0;

                switch (size)
                {
                    case 4:
                        tmp_immediate = (int32_t)immediate_value;
                        break;
                    case 2:
                        tmp_immediate = (int16_t)immediate_value;
                        break;
                    case 1:
                        tmp_immediate = (int8_t)immediate_value;
                        break;
                }

                if (tmp_immediate <= 0) {
                    if (tmp_immediate < 0)
                        *ptr++ = mov_immed_u16(tmp, SR_N, 0);
                    else
                        *ptr++ = mov_immed_u16(tmp, SR_Z, 0);
                    *ptr++ = orr_reg(cc, cc, tmp, LSL, 0);
                }
            } else {
                switch (size)
                {
                    case 4:
                        *ptr++ = cmn_reg(31, tmp_reg, LSL, 0);
                        break;
                    case 2:
                        *ptr++ = cmn_reg(31, tmp_reg, LSL, 16);
                        break;
                    case 1:
                        *ptr++ = cmn_reg(31, tmp_reg, LSL, 24);
                        break;
                }
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }

            RA_FreeARMRegister(&ptr, tmp);
#else

            M68K_ModifyCC(&ptr);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (is_load_immediate) {
                if (update_mask & SR_N) {
                    switch (size)
                    {
                    case 4:
                        if (immediate_value & 0x80000000)
                            *ptr++ = orr_immed(REG_SR, REG_SR, SR_N);
                        break;

                    case 2:
                        if (immediate_value & 0x8000)
                            *ptr++ = orr_immed(REG_SR, REG_SR, SR_N);
                        break;

                    case 1:
                        if (immediate_value & 0x80)
                            *ptr++ = orr_immed(REG_SR, REG_SR, SR_N);
                        break;

                    default:
                        break;
                    }
                }

                if ((update_mask & SR_Z) && (immediate_value == 0))
                    *ptr++ = orr_immed(REG_SR, REG_SR, SR_Z);
            }
            else {
                *ptr++ = cmp_immed(tmp_reg, 0);
                if (update_mask & SR_N)
                    *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
                if (update_mask & SR_Z)
                    *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            }
#endif
        }
    }

    RA_FreeARMRegister(&ptr, tmp_reg);
    return ptr;
}
