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

uint32_t *EMIT_ADDQ(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    
    /* ADDQ */
    uint8_t update_cc = 1;
    uint8_t ext_count = 0;
    uint8_t data = (opcode >> 9) & 7;
    if (data == 0) data = 8;

    if ((opcode & 0x30) == 0)
    {
        /* Dx or Ax case */
        uint8_t dx = (opcode & 0x38) == 0;
        uint8_t tmp;

        if (dx)
        {
            /* Fetch m68k register */
            uint8_t dest = RA_MapM68kRegister(&ptr, (opcode & 7) + (dx ? 0 : 8));
            RA_SetDirtyM68kRegister(&ptr, (opcode & 7) + (dx ? 0 : 8));

            switch ((opcode >> 6) & 3)
            {
            case 0:
                tmp = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
                *ptr++ = mov_immed_u16(tmp, data << 8, 1);
                *ptr++ = adds_reg(tmp, tmp, dest, LSL, 24);
                *ptr++ = bfxil(dest, tmp, 24, 8);
#else
                *ptr++ = lsl_immed(tmp, dest, 24);
                *ptr++ = adds_immed(tmp, tmp, 0x400 | data);
                *ptr++ = lsr_immed(tmp, tmp, 24);
                *ptr++ = bfi(dest, tmp, 0, 8);
#endif
                RA_FreeARMRegister(&ptr, tmp);
                break;

            case 1:
                tmp = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
                *ptr++ = mov_immed_u16(tmp, data, 1);
                *ptr++ = adds_reg(tmp, tmp, dest, LSL, 16);
                *ptr++ = bfxil(dest, tmp, 16, 16);
#else
                *ptr++ = lsl_immed(tmp, dest, 16);
                *ptr++ = adds_immed(tmp, tmp, 0x800 | data);
                *ptr++ = lsr_immed(tmp, tmp, 16);
                *ptr++ = bfi(dest, tmp, 0, 16);
#endif
                RA_FreeARMRegister(&ptr, tmp);
                break;

            case 2:
                *ptr++ = adds_immed(dest, dest, data);
                break;
            }
        }
        else
        {
            /* Fetch m68k register */
            uint8_t dest = RA_MapM68kRegister(&ptr, (opcode & 7) + 8);
            RA_SetDirtyM68kRegister(&ptr, (opcode & 7) + 8);

            update_cc = 0;

            *ptr++ = adds_immed(dest, dest, data);
        }
    }
    else
    {
        /* Load effective address */
        uint8_t dest = 0xff;
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;

        if (mode == 4 || mode == 3)
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
        else
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);

        switch ((opcode >> 6) & 3)
        {
            case 0: /* 8-bit */
                if (mode == 4)
                {
                    *ptr++ = ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = ldrb_offset(dest, tmp, 0);
                /* Perform calcualtion */
#ifdef __aarch64__
                uint8_t immed = RA_AllocARMRegister(&ptr);
                *ptr++ = mov_immed_u16(immed, data << 8, 1);
                *ptr++ = adds_reg(tmp, immed, tmp, LSL, 24);
                *ptr++ = lsr(tmp, tmp, 24);
                RA_FreeARMRegister(&ptr, immed);
#else
                *ptr++ = lsl_immed(tmp, tmp, 24);
                *ptr++ = adds_immed(tmp, tmp, 0x400 | data);
                *ptr++ = lsr_immed(tmp, tmp, 24);
#endif
                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = strb_offset(dest, tmp, 0);
                break;
            case 1: /* 16-bit */
                if (mode == 4)
                {
                    *ptr++ = ldrh_offset_preindex(dest, tmp, -2);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = ldrh_offset(dest, tmp, 0);

                /* Perform calcualtion */
#ifdef __aarch64__
                immed = RA_AllocARMRegister(&ptr);
                *ptr++ = mov_immed_u16(immed, data, 1);
                *ptr++ = adds_reg(tmp, immed, tmp, LSL, 16);
                *ptr++ = lsr(tmp, tmp, 16);
                RA_FreeARMRegister(&ptr, immed);
#else
                *ptr++ = lsl_immed(tmp, tmp, 16);
                *ptr++ = adds_immed(tmp, tmp, 0x800 | data);
                *ptr++ = lsr_immed(tmp, tmp, 16);
#endif
                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = strh_offset_postindex(dest, tmp, 2);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = strh_offset(dest, tmp, 0);
                break;

            case 2: /* 32-bit */
                if (mode == 4)
                {
                    *ptr++ = ldr_offset_preindex(dest, tmp, -4);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = ldr_offset(dest, tmp, 0);

                /* Perform calcualtion */
                *ptr++ = adds_immed(tmp, tmp, data);

                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = str_offset_postindex(dest, tmp, 4);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = str_offset(dest, tmp, 0);
                break;
        }

        RA_FreeARMRegister(&ptr, dest);
        RA_FreeARMRegister(&ptr, tmp);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));

    (*m68k_ptr) += ext_count;

    if (update_cc)
    {
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            if (update_mask & SR_X)
                ptr = EMIT_GetNZVCX(ptr, cc, &update_mask);
            else
                ptr = EMIT_GetNZVC(ptr, cc, &update_mask);

            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_VS);
            if (update_mask & (SR_X | SR_C)) {
                if ((update_mask & (SR_X | SR_C)) == SR_X)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_X, ARM_CC_CS);
                else if ((update_mask & (SR_X | SR_C)) == SR_C)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C, ARM_CC_CS);
                else
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C | SR_X, ARM_CC_CS);
            }
        }
    }

    return ptr;
}

uint32_t *EMIT_SUBQ(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);

    /* SUBQ */
    uint8_t update_cc = 1;
    uint8_t ext_count = 0;
    uint8_t data = (opcode >> 9) & 7;
    if (data == 0)
        data = 8;

    if ((opcode & 0x30) == 0)
    {
        /* Dx or Ax case */
        uint8_t dx = (opcode & 0x38) == 0;
        uint8_t tmp;

        if (dx)
        {
            /* Fetch m68k register */
            uint8_t dest = RA_MapM68kRegister(&ptr, (opcode & 7));
            RA_SetDirtyM68kRegister(&ptr, (opcode & 7));
            uint8_t tmp2 = RA_AllocARMRegister(&ptr);

            switch ((opcode >> 6) & 3)
            {
            case 0:
                tmp = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
                *ptr++ = lsl(tmp2, dest, 24);
                *ptr++ = mov_immed_u16(tmp, data << 8, 1);
                *ptr++ = subs_reg(tmp, tmp2, tmp, LSL, 0);
                *ptr++ = bfxil(dest, tmp, 24, 8);
#else
                *ptr++ = lsl_immed(tmp, dest, 24);
                *ptr++ = subs_immed(tmp, tmp, 0x400 | data);
                *ptr++ = lsr_immed(tmp, tmp, 24);
                *ptr++ = bfi(dest, tmp, 0, 8);
#endif
                RA_FreeARMRegister(&ptr, tmp);
                break;

            case 1:
                tmp = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
                *ptr++ = lsl(tmp2, dest, 16);
                *ptr++ = mov_immed_u16(tmp, data, 1);
                *ptr++ = subs_reg(tmp, tmp2, tmp, LSL, 0);
                *ptr++ = bfxil(dest, tmp, 16, 16);
#else
                *ptr++ = lsl_immed(tmp, dest, 16);
                *ptr++ = subs_immed(tmp, tmp, 0x800 | data);
                *ptr++ = lsr_immed(tmp, tmp, 16);
                *ptr++ = bfi(dest, tmp, 0, 16);
#endif
                RA_FreeARMRegister(&ptr, tmp);
                break;

            case 2:
                *ptr++ = subs_immed(dest, dest, data);
                break;
            }

            RA_FreeARMRegister(&ptr, tmp2);
        }
        else
        {
            /* Fetch m68k register */
            uint8_t dest = RA_MapM68kRegister(&ptr, (opcode & 7) + 8);
            RA_SetDirtyM68kRegister(&ptr, (opcode & 7) + 8);

            update_cc = 0;

            *ptr++ = subs_immed(dest, dest, data);
        }
    }
    else
    {
        /* Load effective address */
        uint8_t dest = 0xff;
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;

        if (mode == 4 || mode == 3)
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
        else
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);

        switch ((opcode >> 6) & 3)
        {
        case 0: /* 8-bit */
            if (mode == 4)
            {
                *ptr++ = ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldrb_offset(dest, tmp, 0);
            /* Perform calcualtion */
#ifdef __aarch64__
            uint8_t immed = RA_AllocARMRegister(&ptr);
            *ptr++ = lsl(tmp, tmp, 24);
            *ptr++ = mov_immed_u16(immed, data << 8, 1);
            *ptr++ = subs_reg(tmp, tmp, immed, LSL, 0);
            *ptr++ = lsr(tmp, tmp, 24);
            RA_FreeARMRegister(&ptr, immed);
#else
            *ptr++ = lsl_immed(tmp, tmp, 24);
            *ptr++ = subs_immed(tmp, tmp, 0x400 | data);
            *ptr++ = lsr_immed(tmp, tmp, 24);
#endif
            /* Store back */
            if (mode == 3)
            {
                *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strb_offset(dest, tmp, 0);
            break;
        case 1: /* 16-bit */
            if (mode == 4)
            {
                *ptr++ = ldrh_offset_preindex(dest, tmp, -2);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldrh_offset(dest, tmp, 0);

            /* Perform calcualtion */
#ifdef __aarch64__
            immed = RA_AllocARMRegister(&ptr);
            *ptr++ = lsl(tmp, tmp, 16);
            *ptr++ = mov_immed_u16(immed, data, 1);
            *ptr++ = subs_reg(tmp, tmp, immed, LSL, 0);
            *ptr++ = lsr(tmp, tmp, 16);
            RA_FreeARMRegister(&ptr, immed);
#else
            *ptr++ = lsl_immed(tmp, tmp, 16);
            *ptr++ = subs_immed(tmp, tmp, 0x800 | data);
            *ptr++ = lsr_immed(tmp, tmp, 16);
#endif
            /* Store back */
            if (mode == 3)
            {
                *ptr++ = strh_offset_postindex(dest, tmp, 2);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strh_offset(dest, tmp, 0);
            break;

        case 2: /* 32-bit */
            if (mode == 4)
            {
                *ptr++ = ldr_offset_preindex(dest, tmp, -4);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldr_offset(dest, tmp, 0);

            /* Perform calcualtion */
            *ptr++ = subs_immed(tmp, tmp, data);

            /* Store back */
            if (mode == 3)
            {
                *ptr++ = str_offset_postindex(dest, tmp, 4);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = str_offset(dest, tmp, 0);
            break;
        }

        RA_FreeARMRegister(&ptr, dest);
        RA_FreeARMRegister(&ptr, tmp);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));

    (*m68k_ptr) += ext_count;

    if (update_cc)
    {
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            if (update_mask & SR_X)
                ptr = EMIT_GetNZVnCX(ptr, cc, &update_mask);
            else
                ptr = EMIT_GetNZVnC(ptr, cc, &update_mask);

            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_VS);
            if (update_mask & (SR_X | SR_C)) {
                if ((update_mask & (SR_X | SR_C)) == SR_X)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_X, ARM_CC_CC);
                else if ((update_mask & (SR_X | SR_C)) == SR_C)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C, ARM_CC_CC);
                else
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C | SR_X, ARM_CC_CC);
            }
        }
    }

    return ptr;
}

uint32_t *EMIT_Scc(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t m68k_condition = (opcode >> 8) & 0x0f;
    uint8_t arm_condition = 0;
    uint8_t ext_count = 0;

    if ((opcode & 0x38) == 0)
    {
        /* Scc Dx case */
        uint8_t dest = RA_MapM68kRegister(&ptr, opcode & 7);
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        /* T condition always sets lowest 8 bis, F condition always clears them */
        if ((opcode & 0x0f00) == 0x0100)
        {
#ifdef __aarch64__
            *ptr++ = bic_immed(dest, dest, 8, 0);
#else
            *ptr++ = bfc(dest, 0, 8);
#endif
        }
        else if ((opcode & 0x0f00) == 0x0000)
        {
#ifdef __aarch64__
            *ptr++ = orr_immed(dest, dest, 8, 0);
#else
            *ptr++ = orr_immed(dest, dest, 0xff);
#endif
        }
        else
        {
            arm_condition = EMIT_TestCondition(&ptr, m68k_condition);

#ifdef __aarch64__
            uint8_t c_yes = RA_AllocARMRegister(&ptr);
            uint8_t c_no = RA_AllocARMRegister(&ptr);

            *ptr++ = orr_immed(c_yes, dest, 8, 0);
            *ptr++ = bic_immed(c_no, dest, 8, 0);
            *ptr++ = csel(dest, c_yes, c_no, arm_condition);

            RA_FreeARMRegister(&ptr, c_yes);
            RA_FreeARMRegister(&ptr, c_no);
#else
            *ptr++ = orr_cc_immed(arm_condition, dest, dest, 0xff);
            *ptr++ = bfc_cc(arm_condition^1, dest, 0, 8);
#endif
        }
    }
    else
    {
        uint8_t dest = RA_AllocARMRegister(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        /* T condition always sets lowest 8 bis, F condition always clears them */
        if ((opcode & 0x0f00) == 0x0100)
        {
#ifdef __aarch64__
            *ptr++ = mov_immed_u16(dest, 0, 0);
#else
            *ptr++ = bfc(tmp, 0, 8);
#endif
        }
        else if ((opcode & 0x0f00) == 0x0000)
        {
#ifdef __aarch64__
            *ptr++ = mov_immed_u16(dest, 0xff, 0);
#else
            *ptr++ = orr_immed(tmp, tmp, 0xff);
#endif
        }
        else
        {
            arm_condition = EMIT_TestCondition(&ptr, m68k_condition);
#ifdef __aarch64__
            uint8_t c_yes = RA_AllocARMRegister(&ptr);
            uint8_t c_no = RA_AllocARMRegister(&ptr);

            *ptr++ = orr_immed(c_yes, dest, 8, 0);
            *ptr++ = bic_immed(c_no, dest, 8, 0);
            *ptr++ = csel(dest, c_yes, c_no, arm_condition);

            RA_FreeARMRegister(&ptr, c_yes);
            RA_FreeARMRegister(&ptr, c_no);
#else
            *ptr++ = orr_cc_immed(arm_condition, tmp, tmp, 0xff);
            *ptr++ = bfc_cc(arm_condition^1, tmp, 0, 8);
#endif
        }

        ptr = EMIT_StoreToEffectiveAddress(ptr, 1, &dest, opcode & 0x3f, *m68k_ptr, &ext_count);

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, dest);
    }

    (*m68k_ptr) += ext_count;
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));

    return ptr;
}

uint32_t *EMIT_TRAPcc(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint32_t source = (uint32_t)(uintptr_t)(*m68k_ptr - 1);
    uint8_t arm_condition = 0xff;
    uint8_t m68k_condition = (opcode >> 8) & 15;
    switch (opcode & 7)
    {
        case 4:
            ptr = EMIT_AdvancePC(ptr, 2);
            break;
        case 2:
            ptr = EMIT_AdvancePC(ptr, 4);
            (*m68k_ptr)++;
            break;
        case 3:
            ptr = EMIT_AdvancePC(ptr, 6);
            (*m68k_ptr)+=2;
            break;
        default:
            ptr = EMIT_InjectDebugString(ptr, "[JIT] Illegal OPMODE %d in TRAPcc at %08x. Opcode %04x\n", opcode & 7, source, opcode);
            ptr = EMIT_InjectPrintContext(ptr);
            *ptr++ = udf(opcode);
            break;
    }
    ptr = EMIT_FlushPC(ptr);

    /* If condition is TRUE, always generate exception */
    if (m68k_condition == M_CC_T)
    {
        ptr = EMIT_Exception(ptr, VECTOR_TRAPcc, 2, source);
        *ptr++ = INSN_TO_LE(0xffffffff);
    }
    /* If condition is FALSE, never generate exception, otherwise test CC */
    else if (m68k_condition != M_CC_F)
    {
        uint32_t *tmpptr;
        arm_condition = EMIT_TestCondition(&ptr, m68k_condition);

        tmpptr = ptr;
        *ptr++ = b_cc(arm_condition ^ 1, 0);
        ptr = EMIT_Exception(ptr, VECTOR_TRAPcc, 2, source);
        *tmpptr = b_cc(arm_condition ^ 1, ptr - tmpptr);
        *ptr++ = (uint32_t)(uintptr_t)tmpptr;
        *ptr++ = 1;
        *ptr++ = 0;
        *ptr++ = INSN_TO_LE(0xfffffffe);
    }
    
    return ptr;
}

uint32_t *EMIT_DBcc(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t counter_reg = RA_MapM68kRegister(&ptr, opcode & 7);
    uint8_t m68k_condition = (opcode >> 8) & 0x0f;
    uint8_t arm_condition = 0;
    uint32_t *branch_1 = NULL;
    uint32_t *branch_2 = NULL;
    int32_t branch_offset = 2 + (int16_t)BE16(*(*m68k_ptr)++);
    uint16_t *bra_rel_ptr = *m68k_ptr - 2;

    //*ptr++ = b(0);

    /* Selcom case of DBT which does nothing */
    if (m68k_condition == M_CC_T)
    {
        ptr = EMIT_AdvancePC(ptr, 4);
    }
    else
    {
        uint8_t c_true = RA_AllocARMRegister(&ptr);
        uint8_t c_false = RA_AllocARMRegister(&ptr);
        int8_t off8 = 0;
        int32_t off = 4;
        ptr = EMIT_GetOffsetPC(ptr, &off8);
        off += off8;
        ptr = EMIT_ResetOffsetPC(ptr);

        *ptr++ = add_immed(c_true, REG_PC, off);

        off = branch_offset + off8;

        if (off > -4096 && off < 0)
        {
            *ptr++ = sub_immed(c_false, REG_PC, -off);
        }
        else if (off > 0 && off < 4096)
        {
            *ptr++ = add_immed(c_false, REG_PC, off);
        }
        else if (off != 0)
        {
            uint8_t reg = RA_AllocARMRegister(&ptr);
            *ptr++ = movw_immed_u16(reg, off & 0xffff);
            *ptr++ = movt_immed_u16(reg, (off >> 16) & 0xffff);
            *ptr++ = add_reg(c_false, REG_PC, reg, LSL, 0);
            RA_FreeARMRegister(&ptr, reg);
        } else /* branch_offset == 0 */
        {
            *ptr++ = mov_reg(c_false, REG_PC);
        }

        /* If condition was not false check the condition and eventually break the loop */
        if (m68k_condition != M_CC_F)
        {
            arm_condition = EMIT_TestCondition(&ptr, m68k_condition);

            /* Adjust PC, negated CC is loop condition, CC is loop break condition */
#ifdef __aarch64__
            *ptr++ = csel(REG_PC, c_true, c_false, arm_condition);
#else
            *ptr++ = add_cc_immed(arm_condition^1, REG_PC, REG_PC, 2);
            *ptr++ = add_cc_immed(arm_condition, REG_PC, REG_PC, 4);
#endif
            /* conditionally exit loop */
            branch_1 = ptr;
            *ptr++ = b_cc(arm_condition, 0);
        }

        /* Copy register to temporary, shift 16 bits left */
        uint8_t reg = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
        *ptr++ = uxth(reg, counter_reg);
#else
        *ptr++ = mov_reg_shift(reg, counter_reg, 16);
#endif
        /* Substract 0x10000 from temporary, compare with 0xffff0000 */
#ifdef __aarch64__
        *ptr++ = subs_immed(reg, reg, 1);
#else
        *ptr++ = sub_immed(reg, reg, 0x801);
        *ptr++ = cmn_immed(reg, 0x801);

        /* Bit shift result back and copy it into counter register */
        *ptr++ = lsr_immed(reg, reg, 16);
#endif
        *ptr++ = bfi(counter_reg, reg, 0, 16);
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        /* If counter was 0xffff (temprary reg 0xffff0000) break the loop */
#ifdef __aarch64__
        *ptr++ = csel(REG_PC, c_true, c_false, A64_CC_MI);
        branch_2 = ptr;
        *ptr++ = b_cc(A64_CC_PL, 3);
        if (branch_1) {
#ifdef __aarch64__
            *branch_1 = b_cc(arm_condition, ptr - branch_1);
#else
            *branch_1 = INSN_TO_LE(INSN_TO_LE(*branch_1) + ((int)(branch_2 - branch_1) << 5));
#endif
        }
#else
        *ptr++ = add_cc_immed(ARM_CC_EQ, REG_PC, REG_PC, 4);
        branch_2 = ptr;
        *ptr++ = b_cc(ARM_CC_EQ, 2);
#endif

#if 0
        *ptr++ = add_immed(REG_PC, REG_PC, 2);
        /* Load PC-relative offset */
        *ptr++ = ldrsh_offset(REG_PC, reg, 0);
#ifdef __aarch64__
        *ptr++ = add_reg(REG_PC, REG_PC, reg, LSL, 0);
#else
        *ptr++ = add_reg(REG_PC, REG_PC, reg, 0);
#endif
#endif
        RA_FreeARMRegister(&ptr, reg);

        *branch_2 = b_cc(A64_CC_PL, ptr - branch_2);
        
        *m68k_ptr = (void *)((uintptr_t)bra_rel_ptr + branch_offset);

        *ptr++ = (uint32_t)(uintptr_t)branch_2;
        *ptr++ = 1;
        *ptr++ = 0;
        *ptr++ = INSN_TO_LE(0xfffffffe);
        *ptr++ = INSN_TO_LE(0xfffffff1);

        RA_FreeARMRegister(&ptr, c_true);
        RA_FreeARMRegister(&ptr, c_false);
        RA_FreeARMRegister(&ptr, counter_reg);
    }

    return ptr;
}

static EMIT_Function JumpTable[4096] = {
    [0x000 ... 0x007] = EMIT_ADDQ, 
    [0x010 ... 0x039] = EMIT_ADDQ,  
    [0x040 ... 0x079] = EMIT_ADDQ,  
    [0x080 ... 0x0b9] = EMIT_ADDQ,  

    [0x200 ... 0x207] = EMIT_ADDQ,
    [0x210 ... 0x239] = EMIT_ADDQ,
    [0x240 ... 0x279] = EMIT_ADDQ,
    [0x280 ... 0x2b9] = EMIT_ADDQ,

    [0x400 ... 0x407] = EMIT_ADDQ,
    [0x410 ... 0x439] = EMIT_ADDQ,
    [0x440 ... 0x479] = EMIT_ADDQ,
    [0x480 ... 0x4b9] = EMIT_ADDQ,

    [0x600 ... 0x607] = EMIT_ADDQ,
    [0x610 ... 0x639] = EMIT_ADDQ,
    [0x640 ... 0x679] = EMIT_ADDQ,
    [0x680 ... 0x6b9] = EMIT_ADDQ,

    [0x800 ... 0x807] = EMIT_ADDQ,
    [0x810 ... 0x839] = EMIT_ADDQ,
    [0x840 ... 0x879] = EMIT_ADDQ,
    [0x880 ... 0x8b9] = EMIT_ADDQ,

    [0xa00 ... 0xa07] = EMIT_ADDQ,
    [0xa10 ... 0xa39] = EMIT_ADDQ,
    [0xa40 ... 0xa79] = EMIT_ADDQ,
    [0xa80 ... 0xab9] = EMIT_ADDQ,

    [0xc00 ... 0xc07] = EMIT_ADDQ,
    [0xc10 ... 0xc39] = EMIT_ADDQ,
    [0xc40 ... 0xc79] = EMIT_ADDQ,
    [0xc80 ... 0xcb9] = EMIT_ADDQ,

    [0xe00 ... 0xe07] = EMIT_ADDQ,
    [0xe10 ... 0xe39] = EMIT_ADDQ,
    [0xe40 ... 0xe79] = EMIT_ADDQ,
    [0xe80 ... 0xeb9] = EMIT_ADDQ,


    [0x100 ... 0x107] = EMIT_SUBQ,
    [0x110 ... 0x139] = EMIT_SUBQ,
    [0x140 ... 0x179] = EMIT_SUBQ,
    [0x180 ... 0x1b9] = EMIT_SUBQ,

    [0x300 ... 0x307] = EMIT_SUBQ,
    [0x310 ... 0x339] = EMIT_SUBQ,
    [0x340 ... 0x379] = EMIT_SUBQ,
    [0x380 ... 0x3b9] = EMIT_SUBQ,

    [0x500 ... 0x507] = EMIT_SUBQ,
    [0x510 ... 0x539] = EMIT_SUBQ,
    [0x540 ... 0x579] = EMIT_SUBQ,
    [0x580 ... 0x5b9] = EMIT_SUBQ,

    [0x700 ... 0x707] = EMIT_SUBQ,
    [0x710 ... 0x739] = EMIT_SUBQ,
    [0x740 ... 0x779] = EMIT_SUBQ,
    [0x780 ... 0x7b9] = EMIT_SUBQ,

    [0x900 ... 0x907] = EMIT_SUBQ,
    [0x910 ... 0x939] = EMIT_SUBQ,
    [0x940 ... 0x979] = EMIT_SUBQ,
    [0x980 ... 0x9b9] = EMIT_SUBQ,

    [0xb00 ... 0xb07] = EMIT_SUBQ,
    [0xb10 ... 0xb39] = EMIT_SUBQ,
    [0xb40 ... 0xb79] = EMIT_SUBQ,
    [0xb80 ... 0xbb9] = EMIT_SUBQ,

    [0xd00 ... 0xd07] = EMIT_SUBQ,
    [0xd10 ... 0xd39] = EMIT_SUBQ,
    [0xd40 ... 0xd79] = EMIT_SUBQ,
    [0xd80 ... 0xdb9] = EMIT_SUBQ,

    [0xf00 ... 0xf07] = EMIT_SUBQ,
    [0xf10 ... 0xf39] = EMIT_SUBQ,
    [0xf40 ... 0xf79] = EMIT_SUBQ,
    [0xf80 ... 0xfb9] = EMIT_SUBQ,


    [0x0c0 ... 0x0c7] = EMIT_Scc,
    [0x0d0 ... 0x0f9] = EMIT_Scc,
    
    [0x1c0 ... 0x1c7] = EMIT_Scc,
    [0x1d0 ... 0x1f9] = EMIT_Scc,

    [0x2c0 ... 0x2c7] = EMIT_Scc,
    [0x2d0 ... 0x2f9] = EMIT_Scc,

    [0x3c0 ... 0x3c7] = EMIT_Scc,
    [0x3d0 ... 0x3f9] = EMIT_Scc,

    [0x4c0 ... 0x4c7] = EMIT_Scc,
    [0x4d0 ... 0x4f9] = EMIT_Scc,
    
    [0x5c0 ... 0x5c7] = EMIT_Scc,
    [0x5d0 ... 0x5f9] = EMIT_Scc,

    [0x6c0 ... 0x6c7] = EMIT_Scc,
    [0x6d0 ... 0x6f9] = EMIT_Scc,

    [0x7c0 ... 0x7c7] = EMIT_Scc,
    [0x7d0 ... 0x7f9] = EMIT_Scc,

    [0x8c0 ... 0x8c7] = EMIT_Scc,
    [0x8d0 ... 0x8f9] = EMIT_Scc,
    
    [0x9c0 ... 0x9c7] = EMIT_Scc,
    [0x9d0 ... 0x9f9] = EMIT_Scc,

    [0xac0 ... 0xac7] = EMIT_Scc,
    [0xad0 ... 0xaf9] = EMIT_Scc,

    [0xbc0 ... 0xbc7] = EMIT_Scc,
    [0xbd0 ... 0xbf9] = EMIT_Scc,

    [0xcc0 ... 0xcc7] = EMIT_Scc,
    [0xcd0 ... 0xcf9] = EMIT_Scc,
    
    [0xdc0 ... 0xdc7] = EMIT_Scc,
    [0xdd0 ... 0xdf9] = EMIT_Scc,

    [0xec0 ... 0xec7] = EMIT_Scc,
    [0xed0 ... 0xef9] = EMIT_Scc,

    [0xfc0 ... 0xfc7] = EMIT_Scc,
    [0xfd0 ... 0xff9] = EMIT_Scc,


    [0x0fa ... 0x0fc] = EMIT_TRAPcc,
    [0x1fa ... 0x1fc] = EMIT_TRAPcc,
    [0x2fa ... 0x2fc] = EMIT_TRAPcc,
    [0x3fa ... 0x3fc] = EMIT_TRAPcc,
    [0x4fa ... 0x4fc] = EMIT_TRAPcc,
    [0x5fa ... 0x5fc] = EMIT_TRAPcc,
    [0x6fa ... 0x6fc] = EMIT_TRAPcc,
    [0x7fa ... 0x7fc] = EMIT_TRAPcc,
    [0x8fa ... 0x8fc] = EMIT_TRAPcc,
    [0x9fa ... 0x9fc] = EMIT_TRAPcc,
    [0xafa ... 0xafc] = EMIT_TRAPcc,
    [0xbfa ... 0xbfc] = EMIT_TRAPcc,
    [0xcfa ... 0xcfc] = EMIT_TRAPcc,
    [0xdfa ... 0xdfc] = EMIT_TRAPcc,
    [0xefa ... 0xefc] = EMIT_TRAPcc,
    [0xffa ... 0xffc] = EMIT_TRAPcc,

    [0x0c8 ... 0x0cf] = EMIT_DBcc,
    [0x1c8 ... 0x1cf] = EMIT_DBcc,
    [0x2c8 ... 0x2cf] = EMIT_DBcc,
    [0x3c8 ... 0x3cf] = EMIT_DBcc,
    [0x4c8 ... 0x4cf] = EMIT_DBcc,
    [0x5c8 ... 0x5cf] = EMIT_DBcc,
    [0x6c8 ... 0x6cf] = EMIT_DBcc,
    [0x7c8 ... 0x7cf] = EMIT_DBcc,
    [0x8c8 ... 0x8cf] = EMIT_DBcc,
    [0x9c8 ... 0x9cf] = EMIT_DBcc,
    [0xac8 ... 0xacf] = EMIT_DBcc,
    [0xbc8 ... 0xbcf] = EMIT_DBcc,
    [0xcc8 ... 0xccf] = EMIT_DBcc,
    [0xdc8 ... 0xdcf] = EMIT_DBcc,
    [0xec8 ... 0xecf] = EMIT_DBcc,
    [0xfc8 ... 0xfcf] = EMIT_DBcc,
};

uint32_t *EMIT_line5(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;
    *insn_consumed = 1;

    if (JumpTable[opcode & 0xfff]) {
        ptr = JumpTable[opcode & 0xfff](ptr, opcode, m68k_ptr);
    }
    else
    {
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
    }

    return ptr;
}
