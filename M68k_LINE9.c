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

/* Line9 is one large SUBX/SUB/SUBA */

uint32_t *EMIT_line9(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    /* SUBA */
    if ((opcode & 0xf0c0) == 0x90c0)
    {
        uint8_t ext_words = 0;
        uint8_t size = (opcode & 0x0100) == 0x0100 ? 4 : 2;
        uint8_t reg = RA_MapM68kRegister(&ptr, ((opcode >> 9) & 7) + 8);
        uint8_t tmp;
        RA_SetDirtyM68kRegister(&ptr, ((opcode >> 9) & 7) + 8);

        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp, opcode & 0x3f, *m68k_ptr, &ext_words, 0);

        *ptr++ = sub_reg(reg, reg, tmp, 0);

        RA_FreeARMRegister(&ptr, tmp);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;
    }
    /* SUBX */
    else if ((opcode & 0xf130) == 0x9100)
    {
        /* Move negated C flag to ARM flags */
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        M68K_GetCC(&ptr);
        *ptr++ = mov_immed_u8(tmp, 0);
        *ptr++ = tst_immed(REG_SR, SR_X);
        *ptr++ = orr_cc_immed(ARM_CC_EQ, tmp, tmp, 0x202);  /* Set bit 29: 0x20000000 */
        *ptr++ = msr(tmp, 8);
        RA_FreeARMRegister(&ptr, tmp);

        /* Register to register */
        if ((opcode & 0x0008) == 0)
        {
            uint8_t size = (opcode >> 6) & 3;
            uint8_t regx = RA_MapM68kRegister(&ptr, opcode & 7);
            uint8_t regy = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            uint8_t tmp = 0;

            RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);

            switch (size)
            {
                case 0: /* Byte */
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = lsl_immed(tmp, regx, 24);
                    *ptr++ = add_cc_immed(ARM_CC_CC, tmp, tmp, 0x401);
                    *ptr++ = rsbs_reg(tmp, tmp, regy, 24);
                    *ptr++ = lsr_immed(tmp, tmp, 24);
                    *ptr++ = bfi(regy, tmp, 0, 8);
                    RA_FreeARMRegister(&ptr, tmp);
                    break;
                case 1: /* Word */
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = lsl_immed(tmp, regx, 16);
                    *ptr++ = add_cc_immed(ARM_CC_CC, tmp, tmp, 0x801);
                    *ptr++ = rsbs_reg(tmp, tmp, regy, 16);
                    *ptr++ = lsr_immed(tmp, tmp, 16);
                    *ptr++ = bfi(regy, tmp, 0, 16);
                    RA_FreeARMRegister(&ptr, tmp);
                    break;
                case 2: /* Long */
                    *ptr++ = sbcs_reg(regy, regy, regx, 0);
                    break;
            }
        }
        /* memory to memory */
        else
        {
            uint8_t size = (opcode >> 6) & 3;
            uint8_t regx = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
            uint8_t regy = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
            uint8_t dest = RA_AllocARMRegister(&ptr);
            uint8_t src = RA_AllocARMRegister(&ptr);

            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            RA_SetDirtyM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));

            switch (size)
            {
                case 0: /* Byte */
                    *ptr++ = ldrb_offset_preindex(regx, src, (opcode & 7) == 7 ? -2 : -1);
                    *ptr++ = ldrb_offset_preindex(regy, dest, ((opcode >> 9) & 7) == 7 ? -2 : -1);
                    *ptr++ = lsl_immed(src, src, 24);
                    *ptr++ = add_cc_immed(ARM_CC_CC, src, src, 0x401);
                    *ptr++ = rsbs_reg(dest, src, dest, 24);
                    *ptr++ = lsr_immed(dest, dest, 24);
                    *ptr++ = strb_offset(regy, dest, 0);
                    break;
                case 1: /* Word */
                    *ptr++ = ldrh_offset_preindex(regx, src, -2);
                    *ptr++ = ldrh_offset_preindex(regy, dest, -2);
                    *ptr++ = lsl_immed(src, src, 16);
                    *ptr++ = add_cc_immed(ARM_CC_CC, src, src, 0x801);
                    *ptr++ = rsbs_reg(dest, src, dest, 16);
                    *ptr++ = lsr_immed(dest, dest, 16);
                    *ptr++ = strh_offset(regy, dest, 0);
                    break;
                case 2: /* Long */
                    *ptr++ = ldr_offset_preindex(regx, src, -4);
                    *ptr++ = ldr_offset_preindex(regy, dest, -4);
                    *ptr++ = sbcs_reg(dest, dest, src, 0);
                    *ptr++ = str_offset(regy, dest, 0);
                    break;
            }
        }

        ptr = EMIT_AdvancePC(ptr, 2);

        uint8_t mask = M68K_GetSRMask(*m68k_ptr);
        uint8_t update_mask = (SR_X | SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
            M68K_ModifyCC(&ptr);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            if (update_mask & SR_V)
                *ptr++ = orr_cc_immed(ARM_CC_VS, REG_SR, REG_SR, SR_V);
            if (update_mask & (SR_X | SR_C))
                *ptr++ = orr_cc_immed(ARM_CC_CC, REG_SR, REG_SR, SR_X | SR_C);
        }
    }
    /* SUB */
    else if ((opcode & 0xf000) == 0x9000)
    {
        uint8_t size = 1 << ((opcode >> 6) & 3);
        uint8_t direction = (opcode >> 8) & 1; // 0: Ea+Dn->Dn, 1: Ea+Dn->Ea
        uint8_t ext_words = 0;

        if (direction == 0)
        {
            uint8_t dest = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            uint8_t src = 0;

            RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
            ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0);

            switch(size)
            {
                case 4:
                    *ptr++ = subs_reg(dest, dest, src, 0);
                    break;
                case 2:
                    *ptr++ = lsl_immed(src, src, 16);
                    *ptr++ = rsbs_reg(src, src, dest, 16);
                    *ptr++ = lsr_immed(src, src, 16);
                    *ptr++ = bfi(dest, src, 0, 16);
                    break;
                case 1:
                    *ptr++ = lsl_immed(src, src, 24);
                    *ptr++ = rsbs_reg(src, src, dest, 24);
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

            if (mode == 4 || mode == 3)
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0);
            else
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 1);

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
                *ptr++ = subs_reg(tmp, tmp, src, 0);

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
                *ptr++ = subs_reg(tmp, tmp, src, 16);
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
                *ptr++ = subs_reg(tmp, tmp, src, 24);
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

        uint8_t mask = M68K_GetSRMask(*m68k_ptr);
        uint8_t update_mask = (SR_X | SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
            M68K_ModifyCC(&ptr);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            if (update_mask & SR_V)
                *ptr++ = orr_cc_immed(ARM_CC_VS, REG_SR, REG_SR, SR_V);
            if (update_mask & (SR_X | SR_C))
                *ptr++ = orr_cc_immed(ARM_CC_CC, REG_SR, REG_SR, SR_X | SR_C);
        }
    }
    else
        *ptr++ = udf(opcode);


    return ptr;
}
