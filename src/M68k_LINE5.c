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

uint32_t *EMIT_line5(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    if ((opcode & 0xf0c0) == 0x50c0)
    {
        /* Scc/TRAPcc/DBcc */
        if ((opcode & 0x38) == 0x08)
        {
            /* DBcc */
            uint8_t counter_reg = RA_MapM68kRegister(&ptr, opcode & 7);
            uint8_t m68k_condition = (opcode >> 8) & 0x0f;
            uint8_t arm_condition = 0;
            uint32_t *branch_1 = NULL;
            uint32_t *branch_2 = NULL;

            (*m68k_ptr)++;

            /* Selcom case of DBT which does nothing */
            if (m68k_condition == M_CC_T)
            {
                ptr = EMIT_AdvancePC(ptr, 4);
            }
            else
            {
                ptr = EMIT_FlushPC(ptr);

                /* If condition was not false check the condition and eventually break the loop */
                if (m68k_condition != M_CC_F)
                {
                    arm_condition = EMIT_TestCondition(&ptr, m68k_condition);

                    /* Adjust PC, negated CC is loop condition, CC is loop break condition */
#ifdef __aarch64__
                    uint8_t c_true = RA_AllocARMRegister(&ptr);
                    uint8_t c_false = RA_AllocARMRegister(&ptr);

                    *ptr++ = add_immed(c_false, REG_PC, 2);
                    *ptr++ = add_immed(c_true, REG_PC, 4);
                    *ptr++ = csel(REG_PC, c_true, c_false, arm_condition);

                    RA_FreeARMRegister(&ptr, c_true);
                    RA_FreeARMRegister(&ptr, c_false);
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
                /* Substract 0x10000 from temporary, copare with 0xffff0000 */
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
                *ptr++ = add_immed(reg, REG_PC, 4);
                *ptr++ = csel(REG_PC, reg, REG_PC, A64_CC_MI);
                branch_2 = ptr;
                *ptr++ = b_cc(A64_CC_MI, 2);
#else
                *ptr++ = add_cc_immed(ARM_CC_EQ, REG_PC, REG_PC, 4);
                branch_2 = ptr;
                *ptr++ = b_cc(ARM_CC_EQ, 2);
#endif

                *ptr++ = add_immed(REG_PC, REG_PC, 2);
                /* Load PC-relative offset */
                *ptr++ = ldrsh_offset(REG_PC, reg, 0);
#ifdef __aarch64__
                *ptr++ = add_reg(REG_PC, REG_PC, reg, LSL, 0);
#else
                *ptr++ = add_reg(REG_PC, REG_PC, reg, 0);
#endif
                RA_FreeARMRegister(&ptr, reg);

                if (branch_1) {
#ifdef __aarch64__
#else
                    *branch_1 = INSN_TO_LE(INSN_TO_LE(*branch_1) + ((int)(branch_2 - branch_1) << 5));
#endif
                    *ptr++ = (uint32_t)(uintptr_t)branch_1;
                }

                *ptr++ = (uint32_t)(uintptr_t)branch_2;
                *ptr++ = branch_1 == NULL ? 1 : 2;
                *ptr++ = 0;
                *ptr++ = INSN_TO_LE(0xfffffffe);

                RA_FreeARMRegister(&ptr, counter_reg);
            }
        }
        else if ((opcode & 0x38) == 0x38)
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
                    ptr = EMIT_InjectDebugString(ptr, "[JIT] Illegal OPMODE in TRAPcc at %08x\n", source);
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
        }
        else
        {
            /* Scc */
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
        }

    }
    else if ((opcode & 0xf100) == 0x5100)
    {
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

                switch ((opcode >> 6) & 3)
                {
                case 0:
                    tmp = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
                    *ptr++ = mov_immed_u16(tmp, (-data) << 8, 1);
                    *ptr++ = adds_reg(tmp, tmp, dest, LSL, 24);
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
                    *ptr++ = mov_immed_u16(tmp, -data, 1);
                    *ptr++ = adds_reg(tmp, tmp, dest, LSL, 16);
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
                *ptr++ = mov_immed_u16(immed, (-data) << 8, 1);
                *ptr++ = adds_reg(tmp, immed, tmp, LSL, 24);
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
                *ptr++ = mov_immed_u16(immed, -data, 1);
                *ptr++ = adds_reg(tmp, immed, tmp, LSL, 16);
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
            uint8_t mask = M68K_GetSRMask(*m68k_ptr);
            uint8_t update_mask = (SR_X | SR_C | SR_V | SR_Z | SR_N) & ~mask;

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
    }
    else if ((opcode & 0xf100) == 0x5000)
    {
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
            uint8_t mask = M68K_GetSRMask(*m68k_ptr);
            uint8_t update_mask = (SR_X | SR_C | SR_V | SR_Z | SR_N) & ~mask;

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
    }
    else
    {
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
    }

    return ptr;
}
