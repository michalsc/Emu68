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

uint32_t EMIT_ADDQ(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    
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
            uint8_t dest = RA_MapM68kRegister(ctx, (opcode & 7) + (dx ? 0 : 8));
            RA_SetDirtyM68kRegister(ctx, (opcode & 7) + (dx ? 0 : 8));

            switch ((opcode >> 6) & 3)
            {
            case 0:
                tmp = RA_AllocARMRegister(ctx);

                if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                    EMIT(ctx, 
                        add_immed(tmp, dest, data),
                        bfxil(dest, tmp, 0, 8)
                    );

                    if (update_mask == SR_Z) {
                        EMIT(ctx, ands_immed(31, tmp, 8, 0));
                    }
                    else if (update_mask == SR_N) {
                        EMIT(ctx, ands_immed(31, tmp, 1, 32-7));
                    }
                }
                else 
                {
                    EMIT(ctx, 
                        mov_immed_u16(tmp, data << 8, 1),
                        adds_reg(tmp, tmp, dest, LSL, 24),
                        bfxil(dest, tmp, 24, 8)
                    );
                }

                RA_FreeARMRegister(ctx, tmp);
                break;

            case 1:
                tmp = RA_AllocARMRegister(ctx);

                if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                    EMIT(ctx, 
                        add_immed(tmp, dest, data),
                        bfxil(dest, tmp, 0, 16)
                    );

                    if (update_mask == SR_Z) {
                        EMIT(ctx, ands_immed(31, tmp, 16, 0));
                    }
                    else if (update_mask == SR_N) {
                        EMIT(ctx, ands_immed(31, tmp, 1, 32-15));
                    }
                }
                else 
                {
                    EMIT(ctx, 
                        mov_immed_u16(tmp, data, 1),
                        adds_reg(tmp, tmp, dest, LSL, 16),
                        bfxil(dest, tmp, 16, 16)
                    );
                }

                RA_FreeARMRegister(ctx, tmp);
                break;

            case 2:
                EMIT(ctx, 
                    update_mask ? 
                        adds_immed(dest, dest, data) : 
                        add_immed(dest, dest, data)
                );
                break;
            }
        }
        else
        {
            /* Fetch m68k register */
            uint8_t dest = RA_MapM68kRegister(ctx, (opcode & 7) + 8);
            RA_SetDirtyM68kRegister(ctx, (opcode & 7) + 8);

            update_cc = 0;

            EMIT(ctx, add_immed(dest, dest, data));
        }
    }
    else
    {
        /* Load effective address */
        uint8_t dest = 0xff;
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t mode = (opcode & 0x0038) >> 3;

        if (mode == 4 || mode == 3)
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 0, NULL);
        else
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 1, NULL);

        switch ((opcode >> 6) & 3)
        {
            case 0: /* 8-bit */
                if (mode == 4)
                {
                    EMIT(ctx, ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, ldrb_offset(dest, tmp, 0));
                
                /* Perform calcualtion */
                if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                    EMIT(ctx, add_immed(tmp, tmp, data));
                    
                    if (update_mask == SR_Z) {
                        EMIT(ctx, ands_immed(31, tmp, 8, 0));
                    }
                    else if (update_mask == SR_N) {
                        EMIT(ctx, ands_immed(31, tmp, 1, 32-7));
                    }
                }
                else 
                {
                    uint8_t immed = RA_AllocARMRegister(ctx);
                    EMIT(ctx, 
                        mov_immed_u16(immed, data << 8, 1),
                        adds_reg(tmp, immed, tmp, LSL, 24),
                        lsr(tmp, tmp, 24)
                    );
                    RA_FreeARMRegister(ctx, immed);
                }

                /* Store back */
                if (mode == 3)
                {
                    EMIT(ctx, strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, strb_offset(dest, tmp, 0));
                break;
            
            case 1: /* 16-bit */
                if (mode == 4)
                {
                    EMIT(ctx, ldrh_offset_preindex(dest, tmp, -2));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, ldrh_offset(dest, tmp, 0));

                /* Perform calcualtion */
                if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                    EMIT(ctx, add_immed(tmp, tmp, data));

                    if (update_mask == SR_Z) {
                        EMIT(ctx, ands_immed(31, tmp, 16, 0));
                    }
                    else if (update_mask == SR_N) {
                        EMIT(ctx, ands_immed(31, tmp, 1, 32-15));
                    }
                }
                else 
                {
                    uint8_t immed = RA_AllocARMRegister(ctx);
                    EMIT(ctx, 
                        mov_immed_u16(immed, data, 1),
                        adds_reg(tmp, immed, tmp, LSL, 16),
                        lsr(tmp, tmp, 16)
                    );
                    RA_FreeARMRegister(ctx, immed);
                }

                /* Store back */
                if (mode == 3)
                {
                    EMIT(ctx, strh_offset_postindex(dest, tmp, 2));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, strh_offset(dest, tmp, 0));
                break;

            case 2: /* 32-bit */
                if (mode == 4)
                {
                    EMIT(ctx, ldr_offset_preindex(dest, tmp, -4));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, ldr_offset(dest, tmp, 0));

                /* Perform calcualtion */
                EMIT(ctx, 
                    update_mask ? 
                        adds_immed(tmp, tmp, data) : 
                        add_immed(tmp, tmp, data)
                );

                /* Store back */
                if (mode == 3)
                {
                    EMIT(ctx, str_offset_postindex(dest, tmp, 4));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, str_offset(dest, tmp, 0));
                break;
        }

        RA_FreeARMRegister(ctx, dest);
        RA_FreeARMRegister(ctx, tmp);
    }

    if (((opcode >> 6) & 3) < 2)
    {
        if (update_mask == SR_Z) 
        {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_ClearFlags(ctx, cc, SR_Z);
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, A64_CC_EQ);
            update_mask = 0;
        }
        else if (update_mask == SR_N) 
        {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_ClearFlags(ctx, cc, SR_N);
            EMIT_SetFlagsConditional(ctx, cc, SR_N, A64_CC_NE);
            update_mask = 0;
        }
    }

    EMIT_AdvancePC(ctx, 2 * (ext_count + 1));

    ctx->tc_M68kCodePtr += ext_count;

    if (update_cc)
    {
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(ctx);
            if (update_mask & SR_X)
                EMIT_GetNZCVX(ctx, cc, &update_mask);
            else
                EMIT_GetNZCV(ctx, cc, &update_mask);

            if (update_mask & SR_Z)
                EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                EMIT_SetFlagsConditional(ctx, cc, SR_Valt, ARM_CC_VS);
            if (update_mask & (SR_X | SR_C)) {
                if ((update_mask & (SR_X | SR_C)) == SR_X)
                    EMIT_SetFlagsConditional(ctx, cc, SR_X, ARM_CC_CS);
                else if ((update_mask & (SR_X | SR_C)) == SR_C)
                    EMIT_SetFlagsConditional(ctx, cc, SR_Calt, ARM_CC_CS);
                else
                    EMIT_SetFlagsConditional(ctx, cc, SR_Calt | SR_X, ARM_CC_CS);
            }
        }
    }

    return 1;
}

uint32_t EMIT_SUBQ(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);

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
            uint8_t dest = RA_MapM68kRegister(ctx, (opcode & 7));
            RA_SetDirtyM68kRegister(ctx, (opcode & 7));
            uint8_t tmp2 = RA_AllocARMRegister(ctx);

            switch ((opcode >> 6) & 3)
            {
            case 0:
                tmp = RA_AllocARMRegister(ctx);
                if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                    EMIT(ctx, 
                        sub_immed(tmp, dest, data),
                        bfxil(dest, tmp, 0, 8)
                    );

                    if (update_mask == SR_Z) {
                        EMIT(ctx, ands_immed(31, tmp, 8, 0));
                    }
                    else if (update_mask == SR_N) {
                        EMIT(ctx, ands_immed(31, tmp, 1, 32-7));
                    }
                }
                else
                {
                    EMIT(ctx, 
                        lsl(tmp2, dest, 24),
                        mov_immed_u16(tmp, data << 8, 1),
                        subs_reg(tmp, tmp2, tmp, LSL, 0),
                        bfxil(dest, tmp, 24, 8)
                    );
                }

                RA_FreeARMRegister(ctx, tmp);
                break;

            case 1:
                tmp = RA_AllocARMRegister(ctx);
                if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                    EMIT(ctx, 
                        sub_immed(tmp, dest, data),
                        bfxil(dest, tmp, 0, 16)
                    );

                    if (update_mask == SR_Z) {
                        EMIT(ctx, ands_immed(31, tmp, 16, 0));
                    }
                    else if (update_mask == SR_N) {
                        EMIT(ctx, ands_immed(31, tmp, 1, 32-15));
                    }
                }
                else
                {
                    EMIT(ctx, 
                        lsl(tmp2, dest, 16),
                        mov_immed_u16(tmp, data, 1),
                        subs_reg(tmp, tmp2, tmp, LSL, 0),
                        bfxil(dest, tmp, 16, 16)
                    );
                }

                RA_FreeARMRegister(ctx, tmp);
                break;

            case 2:
                EMIT(ctx, 
                    update_mask ? 
                        subs_immed(dest, dest, data) : 
                        sub_immed(dest, dest, data)
                );
                break;
            }

            RA_FreeARMRegister(ctx, tmp2);
        }
        else
        {
            /* Fetch m68k register */
            uint8_t dest = RA_MapM68kRegister(ctx, (opcode & 7) + 8);
            RA_SetDirtyM68kRegister(ctx, (opcode & 7) + 8);

            update_cc = 0;

            EMIT(ctx, sub_immed(dest, dest, data));
        }
    }
    else
    {
        /* Load effective address */
        uint8_t dest = 0xff;
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t mode = (opcode & 0x0038) >> 3;

        if (mode == 4 || mode == 3)
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 0, NULL);
        else
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 1, NULL);

        switch ((opcode >> 6) & 3)
        {
        case 0: /* 8-bit */
            if (mode == 4)
            {
                EMIT(ctx, ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, ldrb_offset(dest, tmp, 0));
            
            /* Perform calcualtion */
            if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                EMIT(ctx, sub_immed(tmp, tmp, data));

                if (update_mask == SR_Z) {
                    EMIT(ctx, ands_immed(31, tmp, 8, 0));
                }
                else if (update_mask == SR_N) {
                    EMIT(ctx, ands_immed(31, tmp, 1, 32-7));
                }
            }
            else
            {
                uint8_t immed = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    lsl(tmp, tmp, 24),
                    mov_immed_u16(immed, data << 8, 1),
                    subs_reg(tmp, tmp, immed, LSL, 0),
                    lsr(tmp, tmp, 24)
                );
                RA_FreeARMRegister(ctx, immed);
            }

            /* Store back */
            if (mode == 3)
            {
                EMIT(ctx, strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, strb_offset(dest, tmp, 0));
            break;
        
        case 1: /* 16-bit */
            if (mode == 4)
            {
                EMIT(ctx, ldrh_offset_preindex(dest, tmp, -2));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, ldrh_offset(dest, tmp, 0));

            /* Perform calcualtion */
            if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                EMIT(ctx, sub_immed(tmp, tmp, data));

                if (update_mask == SR_Z) {
                    EMIT(ctx, ands_immed(31, tmp, 16, 0));
                }
                else if (update_mask == SR_N) {
                    EMIT(ctx, ands_immed(31, tmp, 1, 32-15));
                }
            }
            else
            {
                uint8_t immed = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    lsl(tmp, tmp, 16),
                    mov_immed_u16(immed, data, 1),
                    subs_reg(tmp, tmp, immed, LSL, 0),
                    lsr(tmp, tmp, 16)
                );
                RA_FreeARMRegister(ctx, immed);
            }

            /* Store back */
            if (mode == 3)
            {
                EMIT(ctx, strh_offset_postindex(dest, tmp, 2));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, strh_offset(dest, tmp, 0));
            break;

        case 2: /* 32-bit */
            if (mode == 4)
            {
                EMIT(ctx, ldr_offset_preindex(dest, tmp, -4));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, ldr_offset(dest, tmp, 0));

            /* Perform calcualtion */
            EMIT(ctx, 
                update_mask ? 
                    subs_immed(tmp, tmp, data) : 
                    sub_immed(tmp, tmp, data)
            );

            /* Store back */
            if (mode == 3)
            {
                EMIT(ctx, str_offset_postindex(dest, tmp, 4));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, str_offset(dest, tmp, 0));
            break;
        }

        RA_FreeARMRegister(ctx, dest);
        RA_FreeARMRegister(ctx, tmp);
    }

    if (((opcode >> 6) & 3) < 2)
    {
        if (update_mask == SR_Z) 
        {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_ClearFlags(ctx, cc, SR_Z);          
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, A64_CC_EQ);
            update_mask = 0;
        }
        else if (update_mask == SR_N) 
        {
            uint8_t cc = RA_ModifyCC(ctx);
            EMIT_ClearFlags(ctx, cc, SR_N);          
            EMIT_SetFlagsConditional(ctx, cc, SR_N, A64_CC_NE);
            update_mask = 0;
        }
    }

    EMIT_AdvancePC(ctx, 2 * (ext_count + 1));

    ctx->tc_M68kCodePtr += ext_count;

    if (update_cc)
    {
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(ctx);
            if (update_mask & SR_X)
                EMIT_GetNZnCVX(ctx, cc, &update_mask);
            else
                EMIT_GetNZnCV(ctx, cc, &update_mask);

            if (update_mask & SR_Z)
                EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                EMIT_SetFlagsConditional(ctx, cc, SR_Valt, ARM_CC_VS);
            if (update_mask & (SR_X | SR_C)) {
                if ((update_mask & (SR_X | SR_C)) == SR_X)
                    EMIT_SetFlagsConditional(ctx, cc, SR_X, ARM_CC_CC);
                else if ((update_mask & (SR_X | SR_C)) == SR_C)
                    EMIT_SetFlagsConditional(ctx, cc, SR_Calt, ARM_CC_CC);
                else
                    EMIT_SetFlagsConditional(ctx, cc, SR_Calt | SR_X, ARM_CC_CC);
            }
        }
    }

    return 1;
}

uint32_t EMIT_Scc(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t m68k_condition = (opcode >> 8) & 0x0f;
    uint8_t arm_condition = 0;
    uint8_t ext_count = 0;

    if ((opcode & 0x38) == 0)
    {
        /* Scc Dx case */
        uint8_t dest = RA_MapM68kRegister(ctx, opcode & 7);
        RA_SetDirtyM68kRegister(ctx, opcode & 7);

        /* T condition always sets lowest 8 bis, F condition always clears them */
        if ((opcode & 0x0f00) == 0x0100)
        {
            EMIT(ctx, bic_immed(dest, dest, 8, 0));
        }
        else if ((opcode & 0x0f00) == 0x0000)
        {
            EMIT(ctx, orr_immed(dest, dest, 8, 0));
        }
        else
        {
            arm_condition = EMIT_TestCondition(ctx, m68k_condition);

            uint8_t tmp = RA_AllocARMRegister(ctx);

            EMIT(ctx, 
                csetm(tmp, arm_condition),
                bfi(dest, tmp, 0, 8)
            );

            RA_FreeARMRegister(ctx, tmp);
        }
    }
    else
    {
        uint8_t dest = RA_AllocARMRegister(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);

        /* T condition always sets lowest 8 bis, F condition always clears them */
        if ((opcode & 0x0f00) == 0x0100)
        {
            EMIT(ctx, mov_immed_u16(dest, 0, 0));
        }
        else if ((opcode & 0x0f00) == 0x0000)
        {
            EMIT(ctx, mov_immed_u16(dest, 0xff, 0));
        }
        else
        {
            arm_condition = EMIT_TestCondition(ctx, m68k_condition);
            uint8_t tmp = RA_AllocARMRegister(ctx);

            EMIT(ctx, 
                csetm(tmp, arm_condition),
                bfi(dest, tmp, 0, 8)
            );

            RA_FreeARMRegister(ctx, tmp);
        }

        EMIT_StoreToEffectiveAddress(ctx, 1, &dest, opcode & 0x3f, &ext_count, 0);

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, dest);
    }

    ctx->tc_M68kCodePtr += ext_count;
    EMIT_AdvancePC(ctx, 2 * (ext_count + 1));

    return 1;
}

uint32_t EMIT_TRAPcc(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint32_t source = (uint32_t)(uintptr_t)(ctx->tc_M68kCodePtr - 1);
    uint8_t arm_condition = 0xff;
    uint8_t m68k_condition = (opcode >> 8) & 15;
    switch (opcode & 7)
    {
        case 4:
            EMIT_AdvancePC(ctx, 2);
            break;
        case 2:
            EMIT_AdvancePC(ctx, 4);
            ctx->tc_M68kCodePtr++;
            break;
        case 3:
            EMIT_AdvancePC(ctx, 6);
            ctx->tc_M68kCodePtr+=2;
            break;
        default:
            EMIT_InjectDebugString(ctx, "[JIT] Illegal OPMODE %d in TRAPcc at %08x. Opcode %04x\n", opcode & 7, source, opcode);
            EMIT_FlushPC(ctx);
            EMIT_Exception(ctx, VECTOR_ILLEGAL_INSTRUCTION, 0);
            EMIT(ctx, INSN_TO_LE(0xffffffff));
            break;
    }
    EMIT_FlushPC(ctx);

    /* If condition is TRUE, always generate exception */
    if (m68k_condition == M_CC_T)
    {
        EMIT_Exception(ctx, VECTOR_TRAPcc, 2, source);
        EMIT(ctx, INSN_TO_LE(0xffffffff));
    }
    /* If condition is FALSE, never generate exception, otherwise test CC */
    else if (m68k_condition != M_CC_F)
    {
        uint32_t *tmpptr;
        arm_condition = EMIT_TestCondition(ctx, m68k_condition);

        tmpptr = ctx->tc_CodePtr++;
        EMIT_Exception(ctx, VECTOR_TRAPcc, 2, source);
        *tmpptr = b_cc(arm_condition ^ 1, ctx->tc_CodePtr - tmpptr);
        EMIT(ctx, 
            (uint32_t)(uintptr_t)tmpptr,
            1, 0,
            INSN_TO_LE(0xfffffffe)
        );
    }
    
    return 1;
}

uint32_t EMIT_DBcc(struct TranslatorContext *ctx, uint16_t opcode)
{
    extern struct M68KState *__m68k_state;
    uint8_t counter_reg = RA_MapM68kRegister(ctx, opcode & 7);
    uint8_t m68k_condition = (opcode >> 8) & 0x0f;
    //uint8_t arm_condition = 0;
    uint32_t *branch_1 = NULL;
    uint32_t *branch_2 = NULL;
    uint32_t branch_1_type = 0;
    uint32_t branch_2_type = 0;
    int32_t branch_offset = 2 + (int16_t)cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr++);
    uint16_t *bra_rel_ptr = ctx->tc_M68kCodePtr - 2;

    /* Seldom case of DBT which does nothing */
    if (m68k_condition == M_CC_T)
    {
        /* Emu68 needs to emit at least one aarch64 opcode, push nop */
        EMIT(ctx, nop());
        EMIT_AdvancePC(ctx, 4);
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
            if (m68k_condition == M_CC_F && branch_offset == 0 && (uintptr_t)ctx->tc_M68kCodePtr < 0x200000)
            {
                uint8_t c_true = RA_AllocARMRegister(ctx);
                uint8_t c_false = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    mov_immed_u16(c_true, 0, 0),
                    ldrb_offset(c_true, c_false, 0),
                    ldrb_offset(c_true, c_false, 0),
                    ldrb_offset(c_true, c_false, 0)
                );
                RA_FreeARMRegister(ctx, c_true);
                RA_FreeARMRegister(ctx, c_false);
            }
        }

        EMIT_GetOffsetPC(ctx, &off8);
        off += off8;
        EMIT_ResetOffsetPC(ctx);

        int32_t true_pc_addend = off;

        //*ptr++ = add_immed(c_true, REG_PC, off);

        off = branch_offset + off8;

        int32_t false_pc_addend = off;

        /* If condition was not false check the condition and eventually break the loop */
        if (m68k_condition != M_CC_F)
        {
            //arm_condition = EMIT_TestCondition(&ptr, m68k_condition);

            ///* Adjust PC, negated CC is loop condition, CC is loop break condition */
            //*ptr++ = csel(REG_PC, c_true, c_false, arm_condition);

            /* conditionally exit loop */
            EMIT_JumpOnCondition(ctx, m68k_condition, 0, &branch_1_type);
            branch_1 = ctx->tc_CodePtr - 1;
        }

        /* Copy register to temporary, shift 16 bits left */
        uint8_t reg = RA_AllocARMRegister(ctx);

        EMIT(ctx, 
            /* Extract counter to 32-bit */
            uxth(reg, counter_reg),
            
            /* Substract 0x10000 from temporary, compare with 0xffff0000 */
            subs_immed(reg, reg, 1)
        );

        RA_SetDirtyM68kRegister(ctx, opcode & 7);

        /* If counter was 0xffff (temprary reg 0xffff0000) break the loop */
        //*ptr++ = csel(REG_PC, c_true, c_false, A64_CC_MI);

        /* Insert register back */
        EMIT(ctx, bfi(counter_reg, reg, 0, 16));

        branch_2 = ctx->tc_CodePtr;
        branch_2_type = FIXUP_BCC;
        EMIT(ctx, b_cc(A64_CC_MI, 0));
        
        RA_FreeARMRegister(ctx, reg);

        off = false_pc_addend;
        if (off > -4096 && off < 0)
        {
            EMIT(ctx, sub_immed(REG_PC, REG_PC, -off));
        }
        else if (off > 0 && off < 4096)
        {
            EMIT(ctx, add_immed(REG_PC, REG_PC, off));
        }
        else if (off != 0)
        {
            uint8_t reg = RA_AllocARMRegister(ctx);
            EMIT(ctx, 
                movw_immed_u16(reg, off & 0xffff),
                movt_immed_u16(reg, (off >> 16) & 0xffff),
                add_reg(REG_PC, REG_PC, reg, LSL, 0)
            );
            RA_FreeARMRegister(ctx, reg);
        }

        ctx->tc_M68kCodePtr = (void *)((uintptr_t)bra_rel_ptr + branch_offset);

        uint32_t *exit_code_start = ctx->tc_CodePtr;

        EMIT(ctx, add_immed(REG_PC, REG_PC, true_pc_addend));

        /* Insert local exit */
        EMIT_LocalExit(ctx, 1);
        uint32_t *exit_code_end = ctx->tc_CodePtr;

        /* Insert fixup location - if branch_1 is not NULL, insert double exit, otherwise single one */
        if (branch_1) {
            /* Insert fixup location */
            EMIT(ctx, 
                exit_code_end - branch_1,
                branch_1_type,
                exit_code_end - branch_2,
                branch_2_type,
                exit_code_end - exit_code_start,
                INSN_TO_LE(MARKER_DOUBLE_EXIT)
            );
        }
        else {
            EMIT(ctx, 
                exit_code_end - branch_2,
                branch_2_type,
                exit_code_end - exit_code_start,
                INSN_TO_LE(MARKER_EXIT_BLOCK)
            );
        }

        RA_FreeARMRegister(ctx, counter_reg);
    }

    return 1;
}

static struct OpcodeDef InsnTable[512] = {
    [0000 ... 0007] = { EMIT_ADDQ, NULL, 0, SR_CCR, 1, 0, 1 },
    [0020 ... 0047] = { EMIT_ADDQ, NULL, 0, SR_CCR, 1, 0, 1 },
    [0050 ... 0071] = { EMIT_ADDQ, NULL, 0, SR_CCR, 1, 1, 1 }, 
    [0100 ... 0107] = { EMIT_ADDQ, NULL, 0, SR_CCR, 1, 0, 2 },
    [0110 ... 0117] = { EMIT_ADDQ, NULL, 0, 0, 1, 0, 2 },
    [0120 ... 0147] = { EMIT_ADDQ, NULL, 0, SR_CCR, 1, 0, 2 },
    [0150 ... 0171] = { EMIT_ADDQ, NULL, 0, SR_CCR, 1, 1, 2 },
    [0200 ... 0207] = { EMIT_ADDQ, NULL, 0, SR_CCR, 1, 0, 4 },
    [0210 ... 0217] = { EMIT_ADDQ, NULL, 0, 0, 1, 0, 4 },
    [0220 ... 0247] = { EMIT_ADDQ, NULL, 0, SR_CCR, 1, 0, 4 },
    [0250 ... 0271] = { EMIT_ADDQ, NULL, 0, SR_CCR, 1, 1, 4 },

    [0300 ... 0307] = { EMIT_Scc, NULL, SR_NZVC, 0, 1, 0, 1 },
    [0710 ... 0717] = { EMIT_DBcc, NULL, SR_NZVC, 0, 2, 0, 0 },
    [0320 ... 0347] = { EMIT_Scc, NULL, SR_NZVC, 0, 1, 0, 1 },
    [0350 ... 0371] = { EMIT_Scc, NULL, SR_NZVC, 0, 1, 1, 1 },
    [0372]          = { EMIT_TRAPcc, NULL, SR_CCR, 0, 2, 0, 0 },
    [0373]          = { EMIT_TRAPcc, NULL, SR_CCR, 0, 3, 0, 0 },
    [0374]          = { EMIT_TRAPcc, NULL, SR_CCR, 0, 1, 0, 0 },

    [0400 ... 0407] = { EMIT_SUBQ, NULL, 0, SR_CCR, 1, 0, 1 },
    [0420 ... 0447] = { EMIT_SUBQ, NULL, 0, SR_CCR, 1, 0, 1 },
    [0450 ... 0471] = { EMIT_SUBQ, NULL, 0, SR_CCR, 1, 1, 1 },
    [0500 ... 0507] = { EMIT_SUBQ, NULL, 0, SR_CCR, 1, 0, 2 },
    [0510 ... 0517] = { EMIT_SUBQ, NULL, 0, 0, 1, 0, 2 },
    [0520 ... 0547] = { EMIT_SUBQ, NULL, 0, SR_CCR, 1, 0, 2 },
    [0550 ... 0571] = { EMIT_SUBQ, NULL, 0, SR_CCR, 1, 1, 2 },
    [0600 ... 0607] = { EMIT_SUBQ, NULL, 0, SR_CCR, 1, 0, 4 },
    [0610 ... 0617] = { EMIT_SUBQ, NULL, 0, 0, 1, 0, 4 },
    [0620 ... 0647] = { EMIT_SUBQ, NULL, 0, SR_CCR, 1, 0, 4 },
    [0650 ... 0671] = { EMIT_SUBQ, NULL, 0, SR_CCR, 1, 1, 4 },

    [0700 ... 0707] = { EMIT_Scc, NULL, SR_NZVC, 0, 1, 0, 1  },
    [0310 ... 0317] = { EMIT_DBcc, NULL, SR_NZVC, 0, 2, 0, 0 },
    [0720 ... 0747] = { EMIT_Scc, NULL, SR_NZVC, 0, 1, 0, 1  },
    [0750 ... 0771] = { EMIT_Scc, NULL, SR_NZVC, 0, 1, 1, 1  },
    [0772]          = { EMIT_TRAPcc, NULL, SR_CCR, 0, 2, 0, 0},
    [0773]          = { EMIT_TRAPcc, NULL, SR_CCR, 0, 3, 0, 0},
    [0774]          = { EMIT_TRAPcc, NULL, SR_CCR, 0, 1, 0, 0},
};

uint32_t EMIT_line5(struct TranslatorContext *ctx)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr++);

    if (InsnTable[opcode & 0777].od_Emit) {
        return InsnTable[opcode & 0777].od_Emit(ctx, opcode);
    }
    else
    {
        EMIT_FlushPC(ctx);
        EMIT_InjectDebugString(ctx, "[JIT] opcode %04x at %08x not implemented\n", opcode, ctx->tc_M68kCodePtr - 1);
        EMIT(ctx, 
            svc(0x100),
            svc(0x101),
            svc(0x103),
            (uint32_t)(uintptr_t)(ctx->tc_M68kCodePtr - 8),
            48
        );
        EMIT_Exception(ctx, VECTOR_ILLEGAL_INSTRUCTION, 0);
        EMIT(ctx, INSN_TO_LE(0xffffffff));
    }

    return 1;
}

static const uint32_t CC[16] = {
    [M_CC_T] = 0,
    [M_CC_F] = 0,
    [M_CC_EQ] = SR_Z,
    [M_CC_NE] = SR_Z,
    [M_CC_MI] = SR_N,
    [M_CC_PL] = SR_N,
    [M_CC_VC] = SR_V,
    [M_CC_VS] = SR_V,
    [M_CC_CC] = SR_C,
    [M_CC_CS] = SR_C,
    [M_CC_LS] = SR_ZC,
    [M_CC_HI] = SR_ZC,
    [M_CC_GE] = SR_NV,
    [M_CC_LT] = SR_NV,
    [M_CC_GT] = SR_NZV,
    [M_CC_LE] = SR_NZV
};

uint32_t GetSR_Line5(uint16_t opcode)
{
    /* If instruction is in the table, return what flags it needs (shifted 16 bits left) and flags it sets */
    if (InsnTable[opcode & 0777].od_Emit)
    {
        // DBcc do not need all flags. Return only what is really needed
        if (InsnTable[opcode & 0777].od_Emit == EMIT_DBcc)
            return CC[(opcode & 0x0f00) >> 8] << 16;

        // Scc does not need all flags. Update
        if (InsnTable[opcode & 0777].od_Emit == EMIT_Scc)
            return CC[(opcode & 0x0f00) >> 8] << 16;

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
