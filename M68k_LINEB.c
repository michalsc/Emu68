/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>
#include "ARM.h"
#include "M68k.h"
#include "RegisterAllocator.h"

uint32_t *EMIT_lineB(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    /* 1011xxx1xx001xxx - CMPM */
    if ((opcode & 0xf138) == 0xb108)
    {
        uint8_t size = 1 << ((opcode >> 6) & 3);
        uint8_t src = 0xff;
        uint8_t dst = 0xff;
        uint8_t ext_words = 0;
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, 0x18 | (opcode & 7), *m68k_ptr, &ext_words);
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &dst, 0x18 | ((opcode >> 9) & 7), *m68k_ptr, &ext_words);

        switch (size)
        {
        case 4:
            *ptr++ = rsbs_reg(tmp, src, dst, 0);
            break;
        case 2:
            *ptr++ = lsl_immed(tmp, src, 16);
            *ptr++ = rsbs_reg(tmp, tmp, dst, 16);
            break;
        case 1:
            *ptr++ = lsl_immed(tmp, src, 24);
            *ptr++ = rsbs_reg(tmp, tmp, dst, 24);
            break;
        }

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, src);
        RA_FreeARMRegister(&ptr, dst);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
        uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            if (update_mask & SR_V)
                *ptr++ = orr_cc_immed(ARM_CC_VS, REG_SR, REG_SR, SR_V);
            if (update_mask & SR_C) /* Note - after sub/rsb C flag on ARM is inverted! */
                *ptr++ = orr_cc_immed(ARM_CC_CC, REG_SR, REG_SR, SR_C);
        }
    }
    /* 1011xxxx11xxxxxx - CMPA */
    else if ((opcode & 0xf0c0) == 0xb0c0)
    {
        uint8_t size = ((opcode >> 8) & 1) ? 4 : 2;
        uint8_t src = 0xff;
        uint8_t dst = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
        uint8_t ext_words = 0;
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words);

        switch (size)
        {
        case 4:
            *ptr++ = rsbs_reg(tmp, src, dst, 0);
            break;
        case 2:
            *ptr++ = lsl_immed(tmp, src, 16);
            *ptr++ = rsbs_reg(tmp, tmp, dst, 16);
            break;
        case 1:
            *ptr++ = lsl_immed(tmp, src, 24);
            *ptr++ = rsbs_reg(tmp, tmp, dst, 24);
            break;
        }

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
        uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            if (update_mask & SR_V)
                *ptr++ = orr_cc_immed(ARM_CC_VS, REG_SR, REG_SR, SR_V);
            if (update_mask & SR_C) /* Note - after sub/rsb C flag on ARM is inverted! */
                *ptr++ = orr_cc_immed(ARM_CC_CC, REG_SR, REG_SR, SR_C);
        }
    }
    /* 1011xxx0xxxxxxxx - CMP */
    else if ((opcode & 0xf100) == 0xb000)
    {
        uint8_t size = 1 << ((opcode >> 6) & 3);
        uint8_t src = 0xff;
        uint8_t dst = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
        uint8_t ext_words = 0;
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words);

        switch(size)
        {
            case 4:
                *ptr++ = rsbs_reg(tmp, src, dst, 0);
                break;
            case 2:
                *ptr++ = lsl_immed(tmp, src, 16);
                *ptr++ = rsbs_reg(tmp, tmp, dst, 16);
                break;
            case 1:
                *ptr++ = lsl_immed(tmp, src, 24);
                *ptr++ = rsbs_reg(tmp, tmp, dst, 24);
                break;
        }

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
        uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            if (update_mask & SR_V)
                *ptr++ = orr_cc_immed(ARM_CC_VS, REG_SR, REG_SR, SR_V);
            if (update_mask & SR_C) /* Note - after sub/rsb C flag on ARM is inverted! */
                *ptr++ = orr_cc_immed(ARM_CC_CC, REG_SR, REG_SR, SR_C);
        }
    }
    /* 1011xxxxxxxxxxxx - EOR */
    else if ((opcode & 0xf000) == 0xb000)
    {
        uint8_t size = 1 << ((opcode >> 6) & 3);
        uint8_t direction = (opcode >> 8) & 1; // 0: Ea+Dn->Dn, 1: Ea+Dn->Ea
        uint8_t ext_words = 0;

        if (direction == 0)
        {
            uint8_t dest = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            uint8_t src = 0;

            RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
            ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words);

            switch (size)
            {
            case 4:
                *ptr++ = eors_reg(dest, dest, src, 0);
                break;
            case 2:
                *ptr++ = lsl_immed(src, src, 16);
                *ptr++ = eors_reg(src, src, dest, 16);
                *ptr++ = lsr_immed(src, src, 16);
                *ptr++ = bfi(dest, src, 0, 16);
                break;
            case 1:
                *ptr++ = lsl_immed(src, src, 24);
                *ptr++ = eors_reg(src, src, dest, 24);
                *ptr++ = lsr_immed(src, src, 24);
                *ptr++ = bfi(dest, src, 0, 8);
                break;
            }

            RA_FreeARMRegister(&ptr, src);
        }
        else
        {
            uint8_t dest = 0xff;
            uint8_t src = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t mode = (opcode & 0x0038) >> 3;

            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words);

            /* Fetch data into temporary register, perform add, store it back */
            switch (size)
            {
            case 4:
                if (mode == 4)
                {
                    *ptr++ = ldr_offset_preindex(dest, tmp, -4);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = ldr_offset(dest, tmp, 0);

                /* Perform calcualtion */
                *ptr++ = eors_reg(tmp, tmp, src, 0);

                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = str_offset_postindex(dest, tmp, 4);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = str_offset(dest, tmp, 0);
                break;
            case 2:
                if (mode == 4)
                {
                    *ptr++ = ldrh_offset_preindex(dest, tmp, -2);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = ldrh_offset(dest, tmp, 0);
                /* Perform calcualtion */
                *ptr++ = lsl_immed(tmp, tmp, 16);
                *ptr++ = eors_reg(tmp, tmp, src, 16);
                *ptr++ = lsr_immed(tmp, tmp, 16);
                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = strh_offset_postindex(dest, tmp, 2);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = strh_offset(dest, tmp, 0);
                break;
            case 1:
                if (mode == 4)
                {
                    *ptr++ = ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = ldrb_offset(dest, tmp, 0);

                /* Perform calcualtion */
                *ptr++ = lsl_immed(tmp, tmp, 24);
                *ptr++ = eors_reg(tmp, tmp, src, 24);
                *ptr++ = lsr_immed(tmp, tmp, 24);

                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = strb_offset(dest, tmp, 0);
                break;
            }

            RA_FreeARMRegister(&ptr, dest);
            RA_FreeARMRegister(&ptr, tmp);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
        uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        }
    }

    return ptr;
}
