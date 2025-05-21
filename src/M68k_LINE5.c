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

                if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                    *ptr++ = add_immed(tmp, dest, data);
                    *ptr++ = bfxil(dest, tmp, 0, 8);

                    if (update_mask == SR_Z) {
                        *ptr++ = ands_immed(31, tmp, 8, 0);
                    }
                    else if (update_mask == SR_N) {
                        *ptr++ = ands_immed(31, tmp, 1, 32-7);
                    }
                }
                else 
                {
                    *ptr++ = mov_immed_u16(tmp, data << 8, 1);
                    *ptr++ = adds_reg(tmp, tmp, dest, LSL, 24);
                    *ptr++ = bfxil(dest, tmp, 24, 8);
                }

                RA_FreeARMRegister(&ptr, tmp);
                break;

            case 1:
                tmp = RA_AllocARMRegister(&ptr);

                if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                    *ptr++ = add_immed(tmp, dest, data);
                    *ptr++ = bfxil(dest, tmp, 0, 16);

                    if (update_mask == SR_Z) {
                        *ptr++ = ands_immed(31, tmp, 16, 0);
                    }
                    else if (update_mask == SR_N) {
                        *ptr++ = ands_immed(31, tmp, 1, 32-15);
                    }
                }
                else 
                {
                    *ptr++ = mov_immed_u16(tmp, data, 1);
                    *ptr++ = adds_reg(tmp, tmp, dest, LSL, 16);
                    *ptr++ = bfxil(dest, tmp, 16, 16);
                }

                RA_FreeARMRegister(&ptr, tmp);
                break;

            case 2:
                *ptr++ = update_mask ? adds_immed(dest, dest, data) : add_immed(dest, dest, data);
                break;
            }
        }
        else
        {
            /* Fetch m68k register */
            uint8_t dest = RA_MapM68kRegister(&ptr, (opcode & 7) + 8);
            RA_SetDirtyM68kRegister(&ptr, (opcode & 7) + 8);

            update_cc = 0;

            *ptr++ = add_immed(dest, dest, data);
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
                if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                    *ptr++ = add_immed(tmp, tmp, data);
                    
                    if (update_mask == SR_Z) {
                        *ptr++ = ands_immed(31, tmp, 8, 0);
                    }
                    else if (update_mask == SR_N) {
                        *ptr++ = ands_immed(31, tmp, 1, 32-7);
                    }
                }
                else 
                {
                    uint8_t immed = RA_AllocARMRegister(&ptr);
                    *ptr++ = mov_immed_u16(immed, data << 8, 1);
                    *ptr++ = adds_reg(tmp, immed, tmp, LSL, 24);
                    *ptr++ = lsr(tmp, tmp, 24);
                    RA_FreeARMRegister(&ptr, immed);
                }

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
                if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                    *ptr++ = add_immed(tmp, tmp, data);

                    if (update_mask == SR_Z) {
                        *ptr++ = ands_immed(31, tmp, 16, 0);
                    }
                    else if (update_mask == SR_N) {
                        *ptr++ = ands_immed(31, tmp, 1, 32-15);
                    }
                }
                else 
                {
                    uint8_t immed = RA_AllocARMRegister(&ptr);
                    *ptr++ = mov_immed_u16(immed, data, 1);
                    *ptr++ = adds_reg(tmp, immed, tmp, LSL, 16);
                    *ptr++ = lsr(tmp, tmp, 16);
                    RA_FreeARMRegister(&ptr, immed);
                }

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
                *ptr++ = update_mask ? adds_immed(tmp, tmp, data) : add_immed(tmp, tmp, data);

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

    if (((opcode >> 6) & 3) < 2)
    {
        if (update_mask == SR_Z) 
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_ClearFlags(ptr, cc, SR_Z);          
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, A64_CC_EQ);
            update_mask = 0;
        }
        else if (update_mask == SR_N) 
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_ClearFlags(ptr, cc, SR_N);          
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, A64_CC_NE);
            update_mask = 0;
        }
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));

    (*m68k_ptr) += ext_count;

    if (update_cc)
    {
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            if (update_mask & SR_X)
                ptr = EMIT_GetNZCVX(ptr, cc, &update_mask);
            else
                ptr = EMIT_GetNZCV(ptr, cc, &update_mask);

            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Valt, ARM_CC_VS);
            if (update_mask & (SR_X | SR_C)) {
                if ((update_mask & (SR_X | SR_C)) == SR_X)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_X, ARM_CC_CS);
                else if ((update_mask & (SR_X | SR_C)) == SR_C)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Calt, ARM_CC_CS);
                else
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Calt | SR_X, ARM_CC_CS);
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
                if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                    *ptr++ = sub_immed(tmp, dest, data);
                    *ptr++ = bfxil(dest, tmp, 0, 8);

                    if (update_mask == SR_Z) {
                        *ptr++ = ands_immed(31, tmp, 8, 0);
                    }
                    else if (update_mask == SR_N) {
                        *ptr++ = ands_immed(31, tmp, 1, 32-7);
                    }
                }
                else
                {
                    *ptr++ = lsl(tmp2, dest, 24);
                    *ptr++ = mov_immed_u16(tmp, data << 8, 1);
                    *ptr++ = subs_reg(tmp, tmp2, tmp, LSL, 0);
                    *ptr++ = bfxil(dest, tmp, 24, 8);
                }

                RA_FreeARMRegister(&ptr, tmp);
                break;

            case 1:
                tmp = RA_AllocARMRegister(&ptr);
                if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                    *ptr++ = sub_immed(tmp, dest, data);
                    *ptr++ = bfxil(dest, tmp, 0, 16);

                    if (update_mask == SR_Z) {
                        *ptr++ = ands_immed(31, tmp, 16, 0);
                    }
                    else if (update_mask == SR_N) {
                        *ptr++ = ands_immed(31, tmp, 1, 32-15);
                    }
                }
                else
                {
                    *ptr++ = lsl(tmp2, dest, 16);
                    *ptr++ = mov_immed_u16(tmp, data, 1);
                    *ptr++ = subs_reg(tmp, tmp2, tmp, LSL, 0);
                    *ptr++ = bfxil(dest, tmp, 16, 16);
                }

                RA_FreeARMRegister(&ptr, tmp);
                break;

            case 2:
                *ptr++ = update_mask ? subs_immed(dest, dest, data) : sub_immed(dest, dest, data);
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

            *ptr++ = sub_immed(dest, dest, data);
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
            if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                *ptr++ = sub_immed(tmp, tmp, data);

                if (update_mask == SR_Z) {
                    *ptr++ = ands_immed(31, tmp, 8, 0);
                }
                else if (update_mask == SR_N) {
                    *ptr++ = ands_immed(31, tmp, 1, 32-7);
                }
            }
            else
            {
                uint8_t immed = RA_AllocARMRegister(&ptr);
                *ptr++ = lsl(tmp, tmp, 24);
                *ptr++ = mov_immed_u16(immed, data << 8, 1);
                *ptr++ = subs_reg(tmp, tmp, immed, LSL, 0);
                *ptr++ = lsr(tmp, tmp, 24);
                RA_FreeARMRegister(&ptr, immed);
            }

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
            if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                *ptr++ = sub_immed(tmp, tmp, data);

                if (update_mask == SR_Z) {
                    *ptr++ = ands_immed(31, tmp, 16, 0);
                }
                else if (update_mask == SR_N) {
                    *ptr++ = ands_immed(31, tmp, 1, 32-15);
                }
            }
            else
            {
                uint8_t immed = RA_AllocARMRegister(&ptr);
                *ptr++ = lsl(tmp, tmp, 16);
                *ptr++ = mov_immed_u16(immed, data, 1);
                *ptr++ = subs_reg(tmp, tmp, immed, LSL, 0);
                *ptr++ = lsr(tmp, tmp, 16);
                RA_FreeARMRegister(&ptr, immed);
            }

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
            *ptr++ = update_mask ? subs_immed(tmp, tmp, data) : sub_immed(tmp, tmp, data);

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

    if (((opcode >> 6) & 3) < 2)
    {
        if (update_mask == SR_Z) 
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_ClearFlags(ptr, cc, SR_Z);          
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, A64_CC_EQ);
            update_mask = 0;
        }
        else if (update_mask == SR_N) 
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_ClearFlags(ptr, cc, SR_N);          
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, A64_CC_NE);
            update_mask = 0;
        }
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));

    (*m68k_ptr) += ext_count;

    if (update_cc)
    {
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            if (update_mask & SR_X)
                ptr = EMIT_GetNZnCVX(ptr, cc, &update_mask);
            else
                ptr = EMIT_GetNZnCV(ptr, cc, &update_mask);

            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Valt, ARM_CC_VS);
            if (update_mask & (SR_X | SR_C)) {
                if ((update_mask & (SR_X | SR_C)) == SR_X)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_X, ARM_CC_CC);
                else if ((update_mask & (SR_X | SR_C)) == SR_C)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Calt, ARM_CC_CC);
                else
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Calt | SR_X, ARM_CC_CC);
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
            *ptr++ = bic_immed(dest, dest, 8, 0);
        }
        else if ((opcode & 0x0f00) == 0x0000)
        {
            *ptr++ = orr_immed(dest, dest, 8, 0);
        }
        else
        {
            arm_condition = EMIT_TestCondition(&ptr, m68k_condition);

            uint8_t tmp = RA_AllocARMRegister(&ptr);

            *ptr++ = csetm(tmp, arm_condition);
            *ptr++ = bfi(dest, tmp, 0, 8);

            RA_FreeARMRegister(&ptr, tmp);
        }
    }
    else
    {
        uint8_t dest = RA_AllocARMRegister(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        /* T condition always sets lowest 8 bis, F condition always clears them */
        if ((opcode & 0x0f00) == 0x0100)
        {
            *ptr++ = mov_immed_u16(dest, 0, 0);
        }
        else if ((opcode & 0x0f00) == 0x0000)
        {
            *ptr++ = mov_immed_u16(dest, 0xff, 0);
        }
        else
        {
            arm_condition = EMIT_TestCondition(&ptr, m68k_condition);
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            *ptr++ = csetm(tmp, arm_condition);
            *ptr++ = bfi(dest, tmp, 0, 8);

            RA_FreeARMRegister(&ptr, tmp);
        }

        ptr = EMIT_StoreToEffectiveAddress(ptr, 1, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 0);

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
            ptr = EMIT_FlushPC(ptr);
            ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
            *ptr++ = INSN_TO_LE(0xffffffff);
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
    extern struct M68KState *__m68k_state;
    uint8_t counter_reg = RA_MapM68kRegister(&ptr, opcode & 7);
    uint8_t m68k_condition = (opcode >> 8) & 0x0f;
    //uint8_t arm_condition = 0;
    uint32_t *branch_1 = NULL;
    uint32_t *branch_2 = NULL;
    uint32_t branch_1_type = 0;
    uint32_t branch_2_type = 0;
    int32_t branch_offset = 2 + (int16_t)cache_read_16(ICACHE, (uintptr_t)&(*(*m68k_ptr)++));
    uint16_t *bra_rel_ptr = *m68k_ptr - 2;

    /* Seldom case of DBT which does nothing */
    if (m68k_condition == M_CC_T)
    {
        /* Emu68 needs to emit at least one aarch64 opcode, push nop */
        *ptr++ = nop();
        ptr = EMIT_AdvancePC(ptr, 4);
    }
    else
    {
        int8_t off8 = 0;
        int32_t off = 4;

        // Suggested by Paraj - a way to allow old code using DBF as busy loop work:
        // For busy loops (of the form l dbf dN,l) in chip mem add extra delay that is
        // at least 10 7MHz clocks (For old school replayer routines)
        if (__m68k_state->JIT_CONTROL2 & JC2F_DBF_SLOWDOWN)
        {
            if (m68k_condition == M_CC_F && branch_offset == 0 && (uintptr_t)*m68k_ptr < 0x200000)
            {
                uint8_t c_true = RA_AllocARMRegister(&ptr);
                uint8_t c_false = RA_AllocARMRegister(&ptr);
                *ptr++ = mov_immed_u16(c_true, 0, 0);
                *ptr++ = ldrb_offset(c_true, c_false, 0);
                *ptr++ = ldrb_offset(c_true, c_false, 0);
                *ptr++ = ldrb_offset(c_true, c_false, 0);
                RA_FreeARMRegister(&ptr, c_true);
                RA_FreeARMRegister(&ptr, c_false);
            }
        }

        ptr = EMIT_GetOffsetPC(ptr, &off8);
        off += off8;
        ptr = EMIT_ResetOffsetPC(ptr);

        int32_t true_pc_addend = off;

        //*ptr++ = add_immed(c_true, REG_PC, off);

        off = branch_offset + off8;

        int32_t false_pc_addend = off;
#if 0
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
            RA_FreeARMRegister(&ptr, c_false);
            c_false = REG_PC;
        }
#endif
        /* If condition was not false check the condition and eventually break the loop */
        if (m68k_condition != M_CC_F)
        {
            //arm_condition = EMIT_TestCondition(&ptr, m68k_condition);

            ///* Adjust PC, negated CC is loop condition, CC is loop break condition */
            //*ptr++ = csel(REG_PC, c_true, c_false, arm_condition);

            /* conditionally exit loop */
            ptr = EMIT_JumpOnCondition(ptr, m68k_condition, 0, &branch_1_type);
            branch_1 = ptr - 1;
        }

        /* Copy register to temporary, shift 16 bits left */
        uint8_t reg = RA_AllocARMRegister(&ptr);

        *ptr++ = uxth(reg, counter_reg);

        /* Substract 0x10000 from temporary, compare with 0xffff0000 */
        *ptr++ = subs_immed(reg, reg, 1);

        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        /* If counter was 0xffff (temprary reg 0xffff0000) break the loop */
        //*ptr++ = csel(REG_PC, c_true, c_false, A64_CC_MI);

        /* Insert register back */
        *ptr++ = bfi(counter_reg, reg, 0, 16);

        branch_2 = ptr;
        branch_2_type = FIXUP_BCC;
        *ptr++ = b_cc(A64_CC_MI, 0);
        
        RA_FreeARMRegister(&ptr, reg);

        off = false_pc_addend;
        if (off > -4096 && off < 0)
        {
            *ptr++ = sub_immed(REG_PC, REG_PC, -off);
        }
        else if (off > 0 && off < 4096)
        {
            *ptr++ = add_immed(REG_PC, REG_PC, off);
        }
        else if (off != 0)
        {
            uint8_t reg = RA_AllocARMRegister(&ptr);
            *ptr++ = movw_immed_u16(reg, off & 0xffff);
            *ptr++ = movt_immed_u16(reg, (off >> 16) & 0xffff);
            *ptr++ = add_reg(REG_PC, REG_PC, reg, LSL, 0);
            RA_FreeARMRegister(&ptr, reg);
        }

        *m68k_ptr = (void *)((uintptr_t)bra_rel_ptr + branch_offset);
#if 0
        /* Generate exit points */
        if (branch_1) {
            uint32_t *exit_code_start = ptr;

            *ptr++ = add_immed(REG_PC, REG_PC, true_pc_addend);

            /* Insert local exit */
            ptr = EMIT_LocalExit(ptr, 1);
            uint32_t *exit_code_end = ptr;

            /* Insert fixup location */
            *ptr++ = exit_code_end - branch_1;
            *ptr++ = branch_1_type;
            *ptr++ = exit_code_end - exit_code_start;
            *ptr++ = INSN_TO_LE(MARKER_EXIT_BLOCK);
        }
#endif
        uint32_t *exit_code_start = ptr;

        *ptr++ = add_immed(REG_PC, REG_PC, true_pc_addend);

        /* Insert local exit */
        ptr = EMIT_LocalExit(ptr, 1);
        uint32_t *exit_code_end = ptr;

        /* Insert fixup location - if branch_1 is not NULL, insert double exit, otherwise single one */
        if (branch_1) {
            /* Insert fixup location */
            *ptr++ = exit_code_end - branch_1;
            *ptr++ = branch_1_type;
            *ptr++ = exit_code_end - branch_2;
            *ptr++ = branch_2_type;
            *ptr++ = exit_code_end - exit_code_start;
            *ptr++ = INSN_TO_LE(MARKER_DOUBLE_EXIT);
        }
        else {
            *ptr++ = exit_code_end - branch_2;
            *ptr++ = branch_2_type;
            *ptr++ = exit_code_end - exit_code_start;
            *ptr++ = INSN_TO_LE(MARKER_EXIT_BLOCK);
        }

        RA_FreeARMRegister(&ptr, counter_reg);
    }

    return ptr;
}

static struct OpcodeDef InsnTable[512] = {
	[0000 ... 0007] = { { .od_Emit = EMIT_ADDQ }, NULL, 0, SR_CCR, 1, 0, 1 },
	[0020 ... 0047] = { { .od_Emit = EMIT_ADDQ }, NULL, 0, SR_CCR, 1, 0, 1 },
	[0050 ... 0071] = { { .od_Emit = EMIT_ADDQ }, NULL, 0, SR_CCR, 1, 1, 1 }, 
	[0100 ... 0107] = { { .od_Emit = EMIT_ADDQ }, NULL, 0, SR_CCR, 1, 0, 2 },
    [0110 ... 0117] = { { .od_Emit = EMIT_ADDQ }, NULL, 0, 0, 1, 0, 2 },
    [0120 ... 0147] = { { .od_Emit = EMIT_ADDQ }, NULL, 0, SR_CCR, 1, 0, 2 },
	[0150 ... 0171] = { { .od_Emit = EMIT_ADDQ }, NULL, 0, SR_CCR, 1, 1, 2 },
	[0200 ... 0207] = { { .od_Emit = EMIT_ADDQ }, NULL, 0, SR_CCR, 1, 0, 4 },
    [0210 ... 0217] = { { .od_Emit = EMIT_ADDQ }, NULL, 0, 0, 1, 0, 4 },
    [0220 ... 0247] = { { .od_Emit = EMIT_ADDQ }, NULL, 0, SR_CCR, 1, 0, 4 },
	[0250 ... 0271] = { { .od_Emit = EMIT_ADDQ }, NULL, 0, SR_CCR, 1, 1, 4 },

	[0300 ... 0307] = { { .od_Emit = EMIT_Scc }, NULL, SR_NZVC, 0, 1, 0, 1 },
	[0710 ... 0717] = { { .od_Emit = EMIT_DBcc }, NULL, SR_NZVC, 0, 2, 0, 0 },
	[0320 ... 0347] = { { .od_Emit = EMIT_Scc }, NULL, SR_NZVC, 0, 1, 0, 1 },
	[0350 ... 0371] = { { .od_Emit = EMIT_Scc }, NULL, SR_NZVC, 0, 1, 1, 1 },
	[0372]          = { { .od_Emit = EMIT_TRAPcc }, NULL, SR_CCR, 0, 2, 0, 0 },
    [0373]          = { { .od_Emit = EMIT_TRAPcc }, NULL, SR_CCR, 0, 3, 0, 0 },
    [0374]          = { { .od_Emit = EMIT_TRAPcc }, NULL, SR_CCR, 0, 1, 0, 0 },

	[0400 ... 0407] = { { .od_Emit = EMIT_SUBQ }, NULL, 0, SR_CCR, 1, 0, 1 },
	[0420 ... 0447] = { { .od_Emit = EMIT_SUBQ }, NULL, 0, SR_CCR, 1, 0, 1 },
	[0450 ... 0471] = { { .od_Emit = EMIT_SUBQ }, NULL, 0, SR_CCR, 1, 1, 1 },
	[0500 ... 0507] = { { .od_Emit = EMIT_SUBQ }, NULL, 0, SR_CCR, 1, 0, 2 },
    [0510 ... 0517] = { { .od_Emit = EMIT_SUBQ }, NULL, 0, 0, 1, 0, 2 },
    [0520 ... 0547] = { { .od_Emit = EMIT_SUBQ }, NULL, 0, SR_CCR, 1, 0, 2 },
	[0550 ... 0571] = { { .od_Emit = EMIT_SUBQ }, NULL, 0, SR_CCR, 1, 1, 2 },
	[0600 ... 0607] = { { .od_Emit = EMIT_SUBQ }, NULL, 0, SR_CCR, 1, 0, 4 },
    [0610 ... 0617] = { { .od_Emit = EMIT_SUBQ }, NULL, 0, 0, 1, 0, 4 },
    [0620 ... 0647] = { { .od_Emit = EMIT_SUBQ }, NULL, 0, SR_CCR, 1, 0, 4 },
	[0650 ... 0671] = { { .od_Emit = EMIT_SUBQ }, NULL, 0, SR_CCR, 1, 1, 4 },

	[0700 ... 0707] = { { .od_Emit = EMIT_Scc }, NULL, SR_NZVC, 0, 1, 0, 1  },
	[0310 ... 0317] = { { .od_Emit = EMIT_DBcc }, NULL, SR_NZVC, 0, 2, 0, 0 },
	[0720 ... 0747] = { { .od_Emit = EMIT_Scc }, NULL, SR_NZVC, 0, 1, 0, 1  },
	[0750 ... 0771] = { { .od_Emit = EMIT_Scc }, NULL, SR_NZVC, 0, 1, 1, 1  },
	[0772]          = { { .od_Emit = EMIT_TRAPcc }, NULL, SR_CCR, 0, 2, 0, 0},
    [0773]          = { { .od_Emit = EMIT_TRAPcc }, NULL, SR_CCR, 0, 3, 0, 0},
    [0774]          = { { .od_Emit = EMIT_TRAPcc }, NULL, SR_CCR, 0, 1, 0, 0},
};

uint32_t *EMIT_line5(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    (*m68k_ptr)++;
    *insn_consumed = 1;

    if (InsnTable[opcode & 0777].od_Emit) {
        ptr = InsnTable[opcode & 0777].od_Emit(ptr, opcode, m68k_ptr);
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


uint32_t GetSR_Line5(uint16_t opcode)
{
    /* If instruction is in the table, return what flags it needs (shifted 16 bits left) and flags it sets */
    if (InsnTable[opcode & 0777].od_Emit) {
        return (InsnTable[opcode & 0777].od_SRNeeds << 16) | InsnTable[opcode & 0777].od_SRSets;
    }
    /* Instruction not found, i.e. it needs all flags and sets none (ILLEGAL INSTRUCTION exception) */
    else {
        kprintf("Undefined Line5\n");
        return SR_CCR << 16;
    }
}

int M68K_GetLine5Length(uint16_t *insn_stream)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)&(*insn_stream));
    
    int length = 0;
    int need_ea = 0;
    int opsize = 0;

    if (InsnTable[opcode & 0777].od_Emit) {
        length = InsnTable[opcode & 0777].od_BaseLength;
        need_ea = InsnTable[opcode & 0777].od_HasEA;
        opsize = InsnTable[opcode & 0777].od_OpSize;
    }

    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize);
    }

    return length;
}
