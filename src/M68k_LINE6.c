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

uint32_t *EMIT_line6(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    /* 01100000xxxxxxxx - BRA */
    if ((opcode & 0xfe00) == 0x6000)
    {
        uint8_t bsr = 0;
        uint8_t reg = RA_AllocARMRegister(&ptr);
        int8_t addend = 0;
        uint16_t *bra_rel_ptr = *m68k_ptr;
        int32_t bra_off = 0;

        int8_t current_pc_off = 2;
        int32_t abs_off = 0;
        ptr = EMIT_GetOffsetPC(ptr, &current_pc_off);
        ptr = EMIT_ResetOffsetPC(ptr);
        abs_off = current_pc_off;

        /* use 16-bit offset */
        if ((opcode & 0x00ff) == 0x00)
        {
            addend = 2;
            bra_off = (int16_t)(BE16((*m68k_ptr)[0]));
            (*m68k_ptr)++;
        }
        /* use 32-bit offset */
        else if ((opcode & 0x00ff) == 0xff)
        {
            addend = 4;
            bra_off = (int32_t)(BE32(*(uint32_t*)*m68k_ptr));
            (*m68k_ptr) += 2;
        }
        else
        /* otherwise use 8-bit offset */
        {
            bra_off = (int8_t)(opcode & 0xff);
        }

        /* Check if INSN is BSR */
        if (opcode & 0x0100)
        {
            uint8_t sp = RA_MapM68kRegister(&ptr, 15);
            RA_SetDirtyM68kRegister(&ptr, 15);

            uint8_t tmp = RA_AllocARMRegister(&ptr);
            if ((addend + abs_off) > 0 && (addend + abs_off) < 256)
                *ptr++ = add_immed(tmp, REG_PC, (addend + abs_off));
            else if ((addend + abs_off) > -256 && (addend + abs_off) < 0)
                *ptr++ = sub_immed(tmp, REG_PC, -(addend + abs_off));
            else if ((addend + abs_off) != 0) {
                int32_t v = addend + abs_off;
                *ptr++ = movw_immed_u16(tmp, v & 0xffff);
                if ((v >> 16) & 0xffff)
                    *ptr++ = movt_immed_u16(tmp, v >> 16);
#ifdef __aarch64__
                *ptr++ = add_reg(tmp, REG_PC, tmp, LSL, 0);
#else
                *ptr++ = add_reg(tmp, REG_PC, tmp, 0);
#endif
            }

            if ((addend + abs_off))
                *ptr++ = str_offset_preindex(sp, tmp, -4);
            else
                *ptr++ = str_offset_preindex(sp, REG_PC, -4);

            bsr = 1;

            RA_FreeARMRegister(&ptr, tmp);
        }

        abs_off += bra_off;

#ifdef __aarch64__
        if (abs_off > -4096 && abs_off < 4096)
        {
            if (abs_off > 0 && abs_off < 4096)
                *ptr++ = add_immed(REG_PC, REG_PC, abs_off);
            else if (abs_off > -4096 && abs_off < 0)
                *ptr++ = sub_immed(REG_PC, REG_PC, -abs_off);
        }
        else
        {
            *ptr++ = movw_immed_u16(reg, abs_off & 0xffff);
            if ((abs_off >> 16) & 0xffff)
                *ptr++ = movt_immed_u16(reg, abs_off >> 16);
            *ptr++ = add_reg(REG_PC, REG_PC, reg, LSL, 0);
        }
#else
        if (abs_off > -256 && abs_off < 256)
        {
            if (abs_off > 0 && abs_off < 256)
                *ptr++ = add_immed(REG_PC, REG_PC, abs_off);
            else if (abs_off > -256 && abs_off < 0)
                *ptr++ = sub_immed(REG_PC, REG_PC, -abs_off);
        }
        else
        {
            *ptr++ = movw_immed_u16(reg, abs_off & 0xffff);
            if ((abs_off >> 16) & 0xffff)
                *ptr++ = movt_immed_u16(reg, abs_off >> 16);
            *ptr++ = add_reg(REG_PC, REG_PC, reg, 0);
        }
#endif
        RA_FreeARMRegister(&ptr, reg);

        /* If branch is done within +- 4KB, try to inline it instead of breaking up the translation unit */
        if (bra_off >= -4096 && bra_off <= 4096) {
            if (bsr) {
                M68K_PushReturnAddress(*m68k_ptr);
            }

            *m68k_ptr = (void *)((uintptr_t)bra_rel_ptr + bra_off);
        }
        else
            *ptr++ = INSN_TO_LE(0xffffffff);
    }
    /* 0110ccccxxxxxxxx - Bcc */
    else
    {
        uint32_t *tmpptr;
        uint8_t m68k_condition = (opcode >> 8) & 15;
        uint8_t success_condition = 0;

#ifndef __aarch64__
        success_condition = EMIT_TestCondition(&ptr, m68k_condition);
#endif

        int8_t local_pc_off = 2;

        ptr = EMIT_GetOffsetPC(ptr, &local_pc_off);
        ptr = EMIT_ResetOffsetPC(ptr);

        uint8_t reg = RA_AllocARMRegister(&ptr);

        intptr_t branch_target = (intptr_t)(*m68k_ptr);
        intptr_t branch_offset = 0;

        /* use 16-bit offset */
        if ((opcode & 0x00ff) == 0x00)
        {
            branch_offset = (int16_t)BE16(*(*m68k_ptr)++);
        }
        /* use 32-bit offset */
        else if ((opcode & 0x00ff) == 0xff)
        {
            uint16_t lo16, hi16;
            hi16 = BE16(*(*m68k_ptr)++);
            lo16 = BE16(*(*m68k_ptr)++);
            branch_offset = lo16 | (hi16 << 16);
        }
        else
        /* otherwise use 8-bit offset */
        {
            branch_offset = (int8_t)(opcode & 0xff);
        }

        branch_offset += local_pc_off;

#ifdef __aarch64__
        uint8_t pc_yes = RA_AllocARMRegister(&ptr);
        uint8_t pc_no = RA_AllocARMRegister(&ptr);

        if (branch_offset > 0 && branch_offset < 4096)
            *ptr++ = add_immed(pc_yes, REG_PC, branch_offset);
        else if (branch_offset > -4096 && branch_offset < 0)
            *ptr++ = sub_immed(pc_yes, REG_PC, -branch_offset);
        else if (branch_offset != 0) {
            *ptr++ = movw_immed_u16(reg, branch_offset);
            if ((branch_offset >> 16) & 0xffff)
                *ptr++ = movt_immed_u16(reg, (branch_offset >> 16) & 0xffff);
            *ptr++ = add_reg(pc_yes, REG_PC, reg, LSL, 0);
        }
        else { *ptr++ = mov_reg(pc_yes, REG_PC); }
#else
        if (branch_offset > 0 && branch_offset < 255)
            *ptr++ = add_cc_immed(success_condition, REG_PC, REG_PC, branch_offset);
        else if (branch_offset > -256 && branch_offset < 0)
            *ptr++ = sub_cc_immed(success_condition, REG_PC, REG_PC, -branch_offset);
        else if (branch_offset != 0) {
            *ptr++ = movw_cc_immed_u16(success_condition, reg, branch_offset);
            if ((branch_offset >> 16) & 0xffff)
                *ptr++ = movt_cc_immed_u16(success_condition, reg, (branch_offset >> 16) & 0xffff);
            *ptr++ = add_cc_reg(success_condition, REG_PC, REG_PC, reg, 0);
        }

        /* Next jump to skip the condition - invert bit 0 of the condition code here! */
        tmpptr = ptr;

        *ptr++ = 0; // Here a b_cc(success_condition ^ 1, 2); will be put, but with right offset

#endif
        branch_target += branch_offset - local_pc_off;

        int16_t local_pc_off_16 = local_pc_off - 2;

        /* Adjust PC accordingly */
        if ((opcode & 0x00ff) == 0x00)
        {
            local_pc_off_16 += 4;
        }
        /* use 32-bit offset */
        else if ((opcode & 0x00ff) == 0xff)
        {
            local_pc_off_16 += 6;
        }
        else
        /* otherwise use 8-bit offset */
        {
            local_pc_off_16 += 2;
        }

#ifdef __aarch64__
        if (local_pc_off_16 > 0 && local_pc_off_16 < 255)
            *ptr++ = add_immed(pc_no, REG_PC, local_pc_off_16);
        else if (local_pc_off_16 > -256 && local_pc_off_16 < 0)
            *ptr++ = sub_immed(pc_no, REG_PC, -local_pc_off_16);
        else if (local_pc_off_16 != 0) {
            *ptr++ = movw_immed_u16(reg, local_pc_off_16);
            if ((local_pc_off_16 >> 16) & 0xffff)
                *ptr++ = movt_immed_u16(reg, local_pc_off_16 >> 16);
            *ptr++ = add_reg(pc_no, REG_PC, reg, LSL, 0);
        }
        success_condition = EMIT_TestCondition(&ptr, m68k_condition);
        *ptr++ = csel(REG_PC, pc_yes, pc_no, success_condition);
        RA_FreeARMRegister(&ptr, pc_yes);
        RA_FreeARMRegister(&ptr, pc_no);
        tmpptr = ptr;
#if EMU68_DEF_BRANCH_AUTO
        if (branch_target < (intptr_t)*m68k_ptr)
            *ptr++ = b_cc(success_condition, 1);
        else
            *ptr++ = b_cc(success_condition^1, 1);
#else
#if EMU68_DEF_BRANCH_TAKEN
        *ptr++ = b_cc(success_condition, 1);
#else
        *ptr++ = b_cc(success_condition^1, 1);
#endif
#endif
#else
        if (local_pc_off_16 > 0 && local_pc_off_16 < 255)
            *ptr++ = add_immed(REG_PC, REG_PC, local_pc_off_16);
        else if (local_pc_off_16 > -256 && local_pc_off_16 < 0)
            *ptr++ = sub_immed(REG_PC, REG_PC, -local_pc_off_16);
        else if (local_pc_off_16 != 0) {
            *ptr++ = movw_immed_u16(reg, local_pc_off_16);
            if ((local_pc_off_16 >> 16) & 0xffff)
                *ptr++ = movt_immed_u16(reg, local_pc_off_16 >> 16);
            *ptr++ = add_reg(REG_PC, REG_PC, reg, 0);
        }
        /* Now we now how far we jump. put the branch in place */
        *tmpptr = b_cc(success_condition, ptr-tmpptr-2);
#endif

#if EMU68_DEF_BRANCH_AUTO
        if (branch_target < (intptr_t)*m68k_ptr)
            *m68k_ptr = (uint16_t *)branch_target;
#else
#if EMU68_DEF_BRANCH_TAKEN
        *m68k_ptr = (uint16_t *)branch_target;
#endif
#endif

        RA_FreeARMRegister(&ptr, reg);
        *ptr++ = (uint32_t)(uintptr_t)tmpptr;
        *ptr++ = 1;
        *ptr++ = branch_target;
        *ptr++ = INSN_TO_LE(0xfffffffe);
    }

    return ptr;
}
