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

static uint8_t M68K_ccTo_ARM[] = {
    ARM_CC_AL,      // M_CC_T
    0x0f,           // M_CC_F
    ARM_CC_HI,      // M_CC_HI
    ARM_CC_LS,      // M_CC_LS
    ARM_CC_CC,      // M_CC_CC
    ARM_CC_CS,      // M_CC_CS
    ARM_CC_NE,      // M_CC_NE
    ARM_CC_EQ,      // M_CC_EQ
    ARM_CC_VC,      // M_CC_VC
    ARM_CC_VS,      // M_CC_VS
    ARM_CC_PL,      // M_CC_PL
    ARM_CC_MI,      // M_CC_MI
    ARM_CC_GE,      // M_CC_GE
    ARM_CC_LT,      // M_CC_LT
    ARM_CC_GT,      // M_CC_GT
    ARM_CC_LE       // M_CC_LE
};

static uint32_t *EMIT_LoadARMCC(uint32_t *ptr, uint8_t m68k_cc)
{
    uint8_t tmp = RA_AllocARMRegister(&ptr);

    *ptr++ = mov_reg_shift(tmp, m68k_cc, 28);     /* Copy m68k_cc */
    *ptr++ = bic_immed(tmp, tmp, 0x203); /* Clear bits 0 and 1 */
    *ptr++ = tst_immed(tmp, 2);
    *ptr++ = orr_cc_immed(ARM_CC_NE, tmp, tmp, 0x201);
    *ptr++ = tst_immed(tmp, 1);
    *ptr++ = orr_cc_immed(ARM_CC_NE, tmp, tmp, 0x202);
    *ptr++ = msr(tmp, 8);

    RA_FreeARMRegister(&ptr, tmp);

    return ptr;
}

uint32_t *EMIT_line6(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    /* 01100000xxxxxxxx - BRA */
    if ((opcode & 0xfe00) == 0x6000)
    {
        uint8_t reg = RA_AllocARMRegister(&ptr);
        uint8_t addend = 0;

        ptr = EMIT_AdvancePC(ptr, 2);

        /* use 16-bit offset */
        if ((opcode & 0x00ff) == 0x00)
        {
            int8_t pc_off = 2;
            ptr = EMIT_GetOffsetPC(ptr, &pc_off);
            *ptr++ = ldrsh_offset(REG_PC, reg, pc_off);
            addend = 2;
            (*m68k_ptr)++;
        }
        /* use 32-bit offset */
        else if ((opcode & 0x00ff) == 0xff)
        {
            int8_t pc_off = 2;
            ptr = EMIT_GetOffsetPC(ptr, &pc_off);
            *ptr++ = ldr_offset(REG_PC, reg, pc_off);
            addend = 4;
            (*m68k_ptr) += 2;
        }
        else
        /* otherwise use 8-bit offset */
        {
            *ptr++ = mov_immed_s8(reg, opcode & 0xff);
        }
        
        ptr = EMIT_FlushPC(ptr);

        /* Check if INSN is BSR */
        if (opcode & 0x0100)
        {
            uint8_t sp = RA_MapM68kRegister(&ptr, 15);
            RA_SetDirtyM68kRegister(&ptr, 15);
            if (addend)
            {
                uint8_t tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = add_immed(tmp, REG_PC, addend);
                *ptr++ = str_offset_preindex(sp, tmp, -4);
                RA_FreeARMRegister(&ptr, tmp);
            }
            else
            {
                *ptr++ = str_offset_preindex(sp, REG_PC, -4);
            }
        }

        *ptr++ = add_reg(REG_PC, REG_PC, reg, 0);
        RA_FreeARMRegister(&ptr, reg);
        *ptr++ = INSN_TO_LE(0xffffffff);
    }
    /* 0110ccccxxxxxxxx - Bcc */
    else
    {
        uint32_t *tmpptr;
        uint8_t m68k_condition = (opcode >> 8) & 15;
        uint8_t arm_condition = M68K_ccTo_ARM[m68k_condition];
        /* First convert m68k condition code to ARM */
        ptr = EMIT_LoadARMCC(ptr, REG_SR);

        ptr = EMIT_FlushPC(ptr);

        /* Adjust PC accordingly */
        if ((opcode & 0x00ff) == 0x00)
        {
            *ptr++ = add_cc_immed(arm_condition^1, REG_PC, REG_PC, 4);
        }
        /* use 32-bit offset */
        else if ((opcode & 0x00ff) == 0xff)
        {
            *ptr++ = add_cc_immed(arm_condition ^ 1, REG_PC, REG_PC, 6);
        }
        else
        /* otherwise use 8-bit offset */
        {
            *ptr++ = add_cc_immed(arm_condition ^ 1, REG_PC, REG_PC, 2);
        }

        /* Next jump to skip the condition - invert bit 0 of the condition code here! */
        tmpptr = ptr;        
        *ptr++ = b_cc(arm_condition ^ 1, 2);

        uint8_t reg = RA_AllocARMRegister(&ptr);

        *ptr++ = add_immed(REG_PC, REG_PC, 2);

        /* use 16-bit offset */
        if ((opcode & 0x00ff) == 0x00)
        {
            *ptr++ = ldrsh_offset(REG_PC, reg, 0);
            (*m68k_ptr)++;
        }
        /* use 32-bit offset */
        else if ((opcode & 0x00ff) == 0xff)
        {
            *ptr++ = ldr_offset(REG_PC, reg, 0);
            (*m68k_ptr) += 2;
        }
        else
        /* otherwise use 8-bit offset */
        {
            *ptr++ = mov_immed_s8(reg, opcode & 0xff);
        }

        *ptr++ = add_reg(REG_PC, REG_PC, reg, 0);
        RA_FreeARMRegister(&ptr, reg);
        *ptr++ = (uint32_t)tmpptr;
        *ptr++ = 1;
        *ptr++ = INSN_TO_LE(0xfffffffe);
    }


    return ptr;
}
