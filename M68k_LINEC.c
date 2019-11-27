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


uint32_t *EMIT_MUL_DIV(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr);

uint32_t *EMIT_lineC(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    /* 1100xxx011xxxxxx - MULU */
    if ((opcode & 0xf1c0) == 0xc0c0)
    {
        ptr = EMIT_MUL_DIV(ptr, opcode, m68k_ptr);
    }
    /* 1100xxx10000xxxx - ABCD */
    else if ((opcode & 0xf1f0) == 0xc100)
    {
        *ptr++ = udf(opcode);
    }
    /* 1100xxx111xxxxxx - MULS */
    else if ((opcode & 0xf1c0) == 0xc1c0)
    {
        ptr = EMIT_MUL_DIV(ptr, opcode, m68k_ptr);
    }
    /* 1100xxx1xx00xxxx - EXG */
    else if ((opcode & 0xf130) == 0xc100)
    {
        uint8_t reg1 = 0xff;
        uint8_t reg2 = 0xff;
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        switch ((opcode >> 3) & 0x1f)
        {
            case 0x08: /* Data registers */
                reg1 = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
                reg2 = RA_MapM68kRegister(&ptr, opcode & 7);
                *ptr++ = mov_reg(tmp, reg1);
                *ptr++ = mov_reg(reg1, reg2);
                *ptr++ = mov_reg(reg2, tmp);
                RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
                RA_SetDirtyM68kRegister(&ptr, opcode & 7);
                break;

            case 0x09:
                reg1 = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
                reg2 = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
                *ptr++ = mov_reg(tmp, reg1);
                *ptr++ = mov_reg(reg1, reg2);
                *ptr++ = mov_reg(reg2, tmp);
                RA_SetDirtyM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                break;

            case 0x11:
                reg1 = RA_MapM68kRegister(&ptr, ((opcode >> 9) & 7));
                reg2 = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
                *ptr++ = mov_reg(tmp, reg1);
                *ptr++ = mov_reg(reg1, reg2);
                *ptr++ = mov_reg(reg2, tmp);
                RA_SetDirtyM68kRegister(&ptr, ((opcode >> 9) & 7));
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                break;
        }

        ptr = EMIT_AdvancePC(ptr, 2);

        RA_FreeARMRegister(&ptr, tmp);
    }
    /* 1100xxxxxxxxxxxx - AND */
    else if ((opcode & 0xf000) == 0xc000)
    {
        uint8_t size = 1 << ((opcode >> 6) & 3);
        uint8_t direction = (opcode >> 8) & 1;
        uint8_t ext_words = 0;

        if (direction == 0)
        {
            uint8_t dest = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            uint8_t src = 0xff;

            RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
            ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0);

            switch (size)
            {
            case 4:
                *ptr++ = ands_reg(dest, dest, src, 0);
                break;
            case 2:
                *ptr++ = lsl_immed(src, src, 16);
                *ptr++ = ands_reg(src, src, dest, 16);
                *ptr++ = lsr_immed(src, src, 16);
                *ptr++ = bfi(dest, src, 0, 16);
                break;
            case 1:
                *ptr++ = lsl_immed(src, src, 24);
                *ptr++ = ands_reg(src, src, dest, 24);
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
                *ptr++ = ands_reg(tmp, tmp, src, 0);

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
                *ptr++ = ands_reg(tmp, tmp, src, 16);
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
                *ptr++ = ands_reg(tmp, tmp, src, 24);
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
        uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
            M68K_ModifyCC(&ptr);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        }
    }
    else
        *ptr++ = udf(opcode);

    return ptr;
}
