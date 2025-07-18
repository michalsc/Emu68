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

uint32_t EMIT_CMPI(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(ctx);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    uint8_t u8 = 0;
    uint16_t u16 = 0;
    uint32_t u32 = 0;
    int immediate = 0;
    int lsl12 = 0;
    uint8_t tmpreg = RA_AllocARMRegister(ctx);

    /* Simple tests are much faster to perform */
    uint8_t simple_test = ((update_mask & 0x13) == 0);

    /* Preload SR */
    uint8_t cc = RA_ModifyCC(ctx);

    /* Load immediate */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            u8 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]) & 0xff;
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            u16 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]);
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            u32 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]) << 16;
            u32 |= cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]);
            size = 4;
            break;
    }

    if ((opcode & 0x0038) != 0)
    {
        /* Load effective address */
        if (simple_test)
            EMIT_LoadFromEffectiveAddress(ctx, 0x80 | size, &dest, opcode & 0x3f, &ext_count, 1, NULL);
        else
            EMIT_LoadFromEffectiveAddress(ctx, size, &dest, opcode & 0x3f, &ext_count, 1, NULL);
    }

    switch (size)
    {
        case 1:
            if (simple_test) {
                if (u8 & 0x80)
                    EMIT(ctx, mov_immed_s8(immed, (int8_t)u8));
                else {
                    u32 = u8;
                    immediate = 1;
                }
            }
            else
                EMIT(ctx, mov_immed_u16(immed, u8 << 8, 1));
            break;
        case 2:
            if (simple_test) {
                if (u16 < 4096) {
                    u32 = u16;
                    immediate = 1;
                }
                else if (u16 & 0x8000)
                    EMIT(ctx, movn_immed_u16(immed, (-(int16_t)u16 - 1), 0));
                else
                    EMIT(ctx, mov_immed_u16(immed, u16, 0));
            } else
                EMIT(ctx, mov_immed_u16(immed, u16, 1));
            break;
        case 4:
            if ((u32 & 0xfffff000) == 0)
            {
                immediate = 1;
            }
            else if ((u32 & 0xff000fff) == 0) {
                immediate = 1;
                lsl12 = 1;
            }
            else
            {
                EMIT_LoadImmediate(ctx, immed, u32);
            }
            break;
    }

    /* handle adding to register here */
    if ((opcode & 0x0038) == 0)
    {
        if (simple_test && size != 4) {
            dest = RA_AllocARMRegister(ctx);
            if (size == 1)
                EMIT(ctx, sxtb(dest, RA_MapM68kRegister(ctx, opcode & 7)));
            else
                EMIT(ctx, sxth(dest, RA_MapM68kRegister(ctx, opcode & 7)));
        } else {
            /* Fetch m68k register */
            dest = RA_MapM68kRegister(ctx, opcode & 7);
        }
    }

    if (simple_test)
    {
        size = 4;
    }

    /* Perform add operation */
    switch (size)
    {
        case 4:
            if (immediate)
                if (lsl12)
                    EMIT(ctx, cmp_immed_lsl12(dest, (u32 >> 12) & 0xfff));
                else
                    EMIT(ctx, cmp_immed(dest, u32));
            else
                EMIT(ctx, cmp_reg(dest, immed, LSL, 0));
            break;
        case 2:
            EMIT(ctx, 
                lsl(tmpreg, dest, 16),
                cmp_reg(tmpreg, immed, LSL, 0)
            );
            break;
        case 1:
            EMIT(ctx, 
                lsl(tmpreg, dest, 24),
                cmp_reg(tmpreg, immed, LSL, 0)
            );
            break;
    }

    RA_FreeARMRegister(ctx, tmpreg);
    RA_FreeARMRegister(ctx, immed);
    RA_FreeARMRegister(ctx, dest);

    EMIT_AdvancePC(ctx, 2 * (ext_count + 1));
    ctx->tc_M68kCodePtr += ext_count;

    if (update_mask)
    {
        EMIT_GetNZnCV(ctx, cc, &update_mask);

        if (update_mask & SR_Z)
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
        if (update_mask & SR_V)
            EMIT_SetFlagsConditional(ctx, cc, SR_Valt, ARM_CC_VS);
        if (update_mask & SR_C)
            EMIT_SetFlagsConditional(ctx, cc, SR_Calt, ARM_CC_CC);
    }
    return 1;
}

uint32_t EMIT_SUBI(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(ctx);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    uint16_t lo16 = 0;
    uint32_t u32 = 0;
    int immediate = 0;
    int lsl12 = 0;

    // Preload CC if flags need to be updated
    if (update_mask) RA_GetCC(ctx);

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            lo16 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]) & 0xff;
            if (!(update_mask == 0)) {
                EMIT(ctx, mov_immed_u16(immed, lo16 << 8, 1));
            }
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            lo16 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]);
            if (!(update_mask == 0)) {
                EMIT(ctx, mov_immed_u16(immed, lo16, 1));
            }
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            u32 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]) << 16;
            u32 |= cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]);
            if ((u32 & 0xfffff000) == 0)
            {
                immediate = 1;
            }
            else if ((u32 & 0xff000fff) == 0) {
                immediate = 1;
                lsl12 = 1;
            }
            else {
                EMIT_LoadImmediate(ctx, immed, u32);
            }
            size = 4;
            break;
    }

    /* handle adding to register here */
    if ((opcode & 0x0038) == 0)
    {
        /* Temporary register for 8/16 bit operations */
        uint8_t temp = RA_AllocARMRegister(ctx);

        /* Fetch m68k register */
        dest = RA_MapM68kRegister(ctx, opcode & 7);

        /* Mark register dirty */
        RA_SetDirtyM68kRegister(ctx, opcode & 7);

        /* Perform add operation */
        switch (size)
        {
            case 4:
                if (immediate)
                    if (lsl12)
                        EMIT(ctx, 
                            update_mask == 0 ? 
                                sub_immed_lsl12(dest, dest, (u32 >> 12) & 0xfff) : 
                                subs_immed_lsl12(dest, dest, (u32 >> 12) & 0xfff)
                            );
                    else
                        EMIT(ctx, 
                            update_mask == 0 ?
                                sub_immed(dest, dest, u32 & 0xfff) :
                                subs_immed(dest, dest, u32 & 0xfff)
                        );
                else
                    EMIT(ctx, 
                        update_mask == 0 ?
                            sub_reg(dest, dest, immed, LSL, 0) : 
                            subs_reg(dest, dest, immed, LSL, 0)
                    );
                break;
            case 2:
                if (update_mask == 0) {
                    if (lo16 & 0xfff || lo16 == 0)
                        EMIT(ctx, sub_immed(temp, dest, lo16 & 0xfff));
                    if (lo16 & 0xf000) {
                        if (lo16 & 0xfff)
                            EMIT(ctx, sub_immed_lsl12(temp, temp, lo16 >> 12));
                        else
                            EMIT(ctx, sub_immed_lsl12(temp, dest, lo16 >> 12));
                    }
                        
                    EMIT(ctx, bfxil(dest, temp, 0, 16));
                }
                else {
                    if (update_mask == SR_N) {
                        kprintf("SUBI.W with update_mask == SR_N\n");
                    }
                    if (update_mask == SR_Z) {
                        kprintf("SUBI.W with update_mask == SR_Z\n");
                    }
                    EMIT(ctx, 
                        lsl(temp, dest, 16),
                        subs_reg(temp, temp, immed, LSL, 0),
                        bfxil(dest, temp, 16, 16)
                    );
                }
                break;
            case 1:
                if (update_mask == 0) {
                    EMIT(ctx, 
                        sub_immed(temp, dest, lo16 & 0xff),
                        bfxil(dest, temp, 0, 8)
                    );
                }
                else {
                    if (update_mask == SR_N) {
                        kprintf("SUBI.B with update_mask == SR_N\n");
                    }
                    if (update_mask == SR_Z) {
                        kprintf("SUBI.B with update_mask == SR_Z\n");
                    }
                    EMIT(ctx, 
                        lsl(temp, dest, 24),
                        subs_reg(temp, temp, immed, LSL, 0),
                        bfxil(dest, temp, 24, 8)
                    );
                }
                break;
        }

        RA_FreeARMRegister(ctx, temp);
    }
    else
    {
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t mode = (opcode & 0x0038) >> 3;

        /* Load effective address */
        if (mode == 4 || mode == 3)
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 0, NULL);
        else
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 1, NULL);

        /* Fetch data into temporary register, perform add, store it back */
        switch (size)
        {
        case 4:
            if (mode == 4)
            {
                EMIT(ctx, ldr_offset_preindex(dest, tmp, -4));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, ldr_offset(dest, tmp, 0));

            /* Perform calcualtion */
            if (immediate)
                if (lsl12)
                    EMIT(ctx, 
                        update_mask == 0 ? 
                            sub_immed_lsl12(immed, tmp, u32 >> 12) : 
                            subs_immed_lsl12(immed, tmp, u32 >> 12)
                    );
                else
                    EMIT(ctx, 
                        update_mask == 0 ? 
                            sub_immed(immed, tmp, u32) : 
                            subs_immed(immed, tmp, u32)
                    );
            else
                EMIT(ctx, 
                    update_mask == 0 ? 
                        sub_reg(immed, tmp, immed, LSL, 0) : 
                        subs_reg(immed, tmp, immed, LSL, 0)
                );

            /* Store back */
            if (mode == 3)
            {
                EMIT(ctx, str_offset_postindex(dest, immed, 4));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, str_offset(dest, immed, 0));
            break;
        case 2:
            if (mode == 4)
            {
                EMIT(ctx, ldrh_offset_preindex(dest, tmp, -2));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, ldrh_offset(dest, tmp, 0));
            
            /* Perform calcualtion */
            if (update_mask == 0) {               
                if (lo16 & 0xfff || lo16 == 0)
                    EMIT(ctx, sub_immed(immed, tmp, lo16 & 0xfff));
                if (lo16 & 0xf000) {
                    if (lo16 & 0xfff)
                        EMIT(ctx, sub_immed_lsl12(immed, immed, lo16 >> 12));
                    else
                        EMIT(ctx, sub_immed_lsl12(immed, tmp, lo16 >> 12));
                }   
            }
            else {
                if (update_mask == SR_N) {
                    kprintf("SUBI.W (EA) with update_mask == SR_N\n");
                }
                if (update_mask == SR_Z) {
                    kprintf("SUBI.W (EA) with update_mask == SR_Z\n");
                }
                EMIT(ctx, 
                    lsl(tmp, tmp, 16),
                    subs_reg(immed, tmp, immed, LSL, 0),
                    lsr(immed, immed, 16)
                );
            }

            /* Store back */
            if (mode == 3)
            {
                EMIT(ctx, strh_offset_postindex(dest, immed, 2));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, strh_offset(dest, immed, 0));
            break;
        case 1:
            if (mode == 4)
            {
                EMIT(ctx, ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, ldrb_offset(dest, tmp, 0));

            /* Perform calcualtion */
            if (update_mask == 0) {
                EMIT(ctx, sub_immed(immed, tmp, lo16 & 0xff));
            }
            else {
                if (update_mask == SR_N) {
                    kprintf("SUBI.B (EA) with update_mask == SR_N\n");
                }
                if (update_mask == SR_Z) {
                    kprintf("SUBI.B (EA) with update_mask == SR_Z\n");
                }
                EMIT(ctx, 
                    lsl(tmp, tmp, 24),
                    subs_reg(immed, tmp, immed, LSL, 0),
                    lsr(immed, immed, 24)
                );
            }

            /* Store back */
            if (mode == 3)
            {
                EMIT(ctx, strb_offset_postindex(dest, immed, (opcode & 7) == 7 ? 2 : 1));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, strb_offset(dest, immed, 0));
            break;
        }

        RA_FreeARMRegister(ctx, tmp);
    }

    RA_FreeARMRegister(ctx, immed);
    RA_FreeARMRegister(ctx, dest);

    EMIT_AdvancePC(ctx, 2 * (ext_count + 1));
    ctx->tc_M68kCodePtr += ext_count;

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

    return 1;
}

uint32_t EMIT_ADDI(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(ctx);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    uint16_t lo16 = 0;
    uint32_t u32 = 0;
    int add_immediate = 0;
    int lsl12 = 0;

    // Preload CC if flags need to be updated
    if (update_mask)
        RA_ModifyCC(ctx);

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            lo16 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]) & 0xff;
            if (!(update_mask == 0)) {
                EMIT(ctx, mov_immed_u16(immed, (lo16 & 0xff) << 8, 1));
            }
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            lo16 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]);
            if (!(update_mask == 0)) {
                EMIT(ctx, mov_immed_u16(immed, lo16, 1));
            }
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            u32 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]) << 16;
            u32 |= cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]);
            if ((u32 & 0xfffff000) == 0)
            {
                add_immediate = 1;
            }
            else if ((u32 & 0xff000fff) == 0) {
                add_immediate = 1;
                lsl12 = 1;
            }
            else
            {
                EMIT_LoadImmediate(ctx, immed, u32);
            }
            size = 4;
            break;
    }

    /* handle adding to register here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register */
        dest = RA_MapM68kRegister(ctx, opcode & 7);

        /* Mark register dirty */
        RA_SetDirtyM68kRegister(ctx, opcode & 7);

        /* Perform add operation */
        switch(size)
        {
            case 4:
                if (add_immediate)
                    if (lsl12)
                        EMIT(ctx, 
                            update_mask == 0 ? 
                                add_immed_lsl12(dest, dest, u32 >> 12) : 
                                adds_immed_lsl12(dest, dest, u32 >> 12)
                        );
                    else
                        EMIT(ctx, 
                            update_mask == 0 ? 
                                add_immed(dest, dest, u32) : 
                                adds_immed(dest, dest, u32)
                        );
                else
                    EMIT(ctx, 
                        update_mask == 0 ? 
                            add_reg(dest, immed, dest, LSL, 0) : 
                            adds_reg(dest, immed, dest, LSL, 0)
                    );
                break;
            case 2:
                if (update_mask == 0) {
                    if (lo16 & 0xfff || lo16 == 0)
                        EMIT(ctx, add_immed(immed, dest, lo16 & 0xfff));
                    if (lo16 & 0xf000) {
                        if (lo16 & 0xfff)
                            EMIT(ctx, add_immed_lsl12(immed, immed, lo16 >> 12));
                        else
                            EMIT(ctx, add_immed_lsl12(immed, dest, lo16 >> 12));
                    }
                    EMIT(ctx, bfi(dest, immed, 0, 16));
                }
                else {
                    if (update_mask == SR_N) {
                        kprintf("ADDI.W with update_mask == SR_N\n");
                    }
                    if (update_mask == SR_Z) {
                        kprintf("ADDI.W with update_mask == SR_Z\n");
                    }
                    EMIT(ctx, 
                        adds_reg(immed, immed, dest, LSL, 16),
                        bfxil(dest, immed, 16, 16)
                    );
                }
                break;
            case 1:
                if (update_mask == 0) {
                    EMIT(ctx, 
                        add_immed(immed, dest, lo16 & 0xff),
                        bfi(dest, immed, 0, 8)
                    );
                }
                else {
                    if (update_mask == SR_N) {
                        kprintf("ADDI.B with update_mask == SR_N\n");
                    }
                    if (update_mask == SR_Z) {
                        kprintf("ADDI.B with update_mask == SR_Z\n");
                    }
                    EMIT(ctx, 
                        adds_reg(immed, immed, dest, LSL, 24),
                        bfxil(dest, immed, 24, 8)
                    );
                }
                break;
        }
    }
    else
    {
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t mode = (opcode & 0x0038) >> 3;

        /* Load effective address */
        if (mode == 4 || mode == 3)
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 0, NULL);
        else
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 1, NULL);

        /* Fetch data into temporary register, perform add, store it back */
        switch(size)
        {
            case 4:
                if (mode == 4)
                {
                    EMIT(ctx, ldr_offset_preindex(dest, tmp, -4));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, ldr_offset(dest, tmp, 0));

                /* Perform calcualtion */
                if (add_immediate)
                    if (lsl12)
                        EMIT(ctx, 
                            update_mask == 0 ? 
                                add_immed_lsl12(immed, tmp, u32 >> 12) : 
                                adds_immed_lsl12(immed, tmp, u32 >> 12)
                        );
                    else
                        EMIT(ctx, 
                            update_mask == 0 ? 
                                add_immed(immed, tmp, u32) : 
                                adds_immed(immed, tmp, u32)
                        );
                else
                    EMIT(ctx, 
                        update_mask == 0 ? 
                            add_reg(immed, immed, tmp, LSL, 0) : 
                            adds_reg(immed, immed, tmp, LSL, 0)
                    );

                /* Store back */
                if (mode == 3)
                {
                    EMIT(ctx, str_offset_postindex(dest, immed, 4));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, str_offset(dest, immed, 0));
                break;
            case 2:
                if (mode == 4)
                {
                    EMIT(ctx, ldrh_offset_preindex(dest, tmp, -2));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, ldrh_offset(dest, tmp, 0));
                
                /* Perform calcualtion */
                if (update_mask == 0) {
                    if (lo16 & 0xfff || lo16 == 0)
                        EMIT(ctx, add_immed(immed, tmp, lo16 & 0xfff));
                    if (lo16 & 0xf000) {
                        if (lo16 & 0xfff)
                            EMIT(ctx, add_immed_lsl12(immed, immed, lo16 >> 12));
                        else
                            EMIT(ctx, add_immed_lsl12(immed, tmp, lo16 >> 12));
                    }
                }
                else {
                    if (update_mask == SR_N) {
                        kprintf("ADDI.W (EA) with update_mask == SR_N\n");
                    }
                    if (update_mask == SR_Z) {
                        kprintf("ADDI.W (EA) with update_mask == SR_Z\n");
                    }
                    EMIT(ctx, 
                        adds_reg(immed, immed, tmp, LSL, 16),
                        lsr(immed, immed, 16)
                    );
                }

                /* Store back */
                if (mode == 3)
                {
                    EMIT(ctx, strh_offset_postindex(dest, immed, 2));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, strh_offset(dest, immed, 0));
                break;
            case 1:
                if (mode == 4)
                {
                    EMIT(ctx, ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, ldrb_offset(dest, tmp, 0));
                
                /* Perform calcualtion */
                if (update_mask == 0) {
                    EMIT(ctx, add_immed(immed, tmp, lo16 & 0xff));
                }
                else {
                    if (update_mask == SR_N) {
                        kprintf("ADDI.B (EA) with update_mask == SR_N\n");
                    }
                    if (update_mask == SR_Z) {
                        kprintf("ADDI.B (EA) with update_mask == SR_Z\n");
                    }
                    EMIT(ctx, 
                        adds_reg(immed, immed, tmp, LSL, 24),
                        lsr(immed, immed, 24)
                    );
                }

                /* Store back */
                if (mode == 3)
                {
                    EMIT(ctx, strb_offset_postindex(dest, immed, (opcode & 7) == 7 ? 2 : 1));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, strb_offset(dest, immed, 0));
                break;
        }

        RA_FreeARMRegister(ctx, tmp);
    }

    RA_FreeARMRegister(ctx, immed);
    RA_FreeARMRegister(ctx, dest);

    EMIT_AdvancePC(ctx, 2 * (ext_count + 1));

    ctx->tc_M68kCodePtr += ext_count;

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

    return 1;
}

uint32_t EMIT_ORI_TO_CCR(struct TranslatorContext *ctx, uint16_t opcode)
{
    (void)opcode;
    uint8_t immed = RA_AllocARMRegister(ctx);
    uint16_t val8 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);

    /* 
        Before swapping flags - invalidate host flags: all flags modified by this instruction
        are out of sync now
    */
    host_flags &= ~val8;

    /* Swap C and V flags in immediate */
    if ((val8 & 3) != 0 && (val8 & 3) < 3)
        val8 ^= 3;

    uint8_t cc = RA_ModifyCC(ctx);
    
    EMIT(ctx, 
        /* Load immediate into the register */
        mov_immed_u8(immed, val8 & 0x1f),

        /* OR with status register, no need to check mask, ARM sequence way too short! */
        orr_reg(cc, cc, immed, LSL, 0)
    );

    RA_FreeARMRegister(ctx, immed);

    EMIT_AdvancePC(ctx, 4);
    ctx->tc_M68kCodePtr += 1;

    return 1;
}

uint32_t EMIT_ORI_TO_SR(struct TranslatorContext *ctx, uint16_t opcode)
{
    (void)opcode;
    uint8_t immed = RA_AllocARMRegister(ctx);
    uint8_t changed = RA_AllocARMRegister(ctx);
    int16_t val = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t sp = RA_MapM68kRegister(ctx, 15);
    uint32_t *tmp;
    RA_SetDirtyM68kRegister(ctx, 15);

    /* Swap C and V flags in immediate */
    if ((val & 3) != 0 && (val & 3) < 3)
        val ^= 3;

    uint8_t cc = RA_ModifyCC(ctx);
    
    EMIT_FlushPC(ctx);
    
    /* If supervisor is not active, put an exception here */
    tmp = ctx->tc_CodePtr++;
    EMIT_Exception(ctx, VECTOR_PRIVILEGE_VIOLATION, 0);
    *tmp = tbnz(cc, SRB_S, 1 + ctx->tc_CodePtr - tmp);
    tmp = ctx->tc_CodePtr++;

    /* Load immediate into the register */
    EMIT(ctx, mov_immed_u16(immed, val & 0xf71f, 0)); 
    
    cc = RA_ModifyCC(ctx);

    EMIT(ctx,
        /* OR is here */
        mov_reg(changed, cc),
        orr_reg(cc, cc, immed, LSL, 0),
        
        /* Check what has changed */
        eor_reg(changed, changed, cc, LSL, 0),

        /* Skip switching ISP/MSP if not changed */
        tbz(changed, SRB_M, 7),

        /* Swap ISP/MSP */
        tbz(cc, SRB_M, 4),
        
        // M is not set now, Store MSP, load ISP
        mov_reg_to_simd(REG_MSP, sp),
        mov_simd_to_reg(sp, REG_ISP),
        b(3),

        // M is set now, store ISP, load MSP
        mov_reg_to_simd(REG_ISP, sp),
        mov_simd_to_reg(sp, REG_MSP),

        // Advance PC
        add_immed(REG_PC, REG_PC, 4),
        
        // Check if IPL is less than 6. If yes, enable ARM interrupts
        and_immed(changed, cc, 3, 32 - SRB_IPL),
        cmp_immed(changed, 5 << SRB_IPL),
        b_cc(A64_CC_GT, 3),
        // Enable interrupts
        msr_imm(3, 7, 7), 
        b(2),
        // Mask interrupts
        msr_imm(3, 6, 7)
    );

    *tmp = b(ctx->tc_CodePtr - tmp);

    EMIT(ctx, INSN_TO_LE(0xffffffff));

    RA_FreeARMRegister(ctx, immed);
    RA_FreeARMRegister(ctx, changed);

    ctx->tc_M68kCodePtr += 1;

    return 1;
}

uint32_t EMIT_ORI(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(ctx);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    uint16_t lo16;
    uint32_t u32;
    uint32_t mask32 = 0;

    // Preload CC if flags need to be updated
    if (update_mask)
        RA_ModifyCC(ctx);

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            lo16 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]) & 0xff;
            if (update_mask == 0 || update_mask == SR_Z) {
                mask32 = number_to_mask(lo16);
                if (mask32 == 0) {
                    EMIT(ctx, mov_immed_u16(immed, lo16 & 0xff, 0));
                }
            }
            else
                EMIT(ctx, mov_immed_u16(immed, (lo16 & 0xff) << 8, 1));

            size = 1;
            break;
        case 0x0040:    /* Short operation */
            lo16 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]);
            if (update_mask == 0 || update_mask == SR_Z) {
                mask32 = number_to_mask(lo16 & 0xffff);
                if (mask32 == 0) {
                    EMIT(ctx, mov_immed_u16(immed, lo16, 0));
                }
            }
            else
                EMIT(ctx, mov_immed_u16(immed, lo16, 1));

            size = 2;
            break;
        case 0x0080:    /* Long operation */
            u32 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]) << 16;
            u32 |= cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]);
            mask32 = number_to_mask(u32);
            if (mask32 == 0)
            {
                EMIT_LoadImmediate(ctx, immed, u32);
            }
            size = 4;
            break;
    }

    /* handle adding to register here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register */
        dest = RA_MapM68kRegister(ctx, opcode & 7);

        /* Mark register dirty */
        RA_SetDirtyM68kRegister(ctx, opcode & 7);

        /* Perform add operation */
        switch(size)
        {
            case 4:
                if (mask32 == 0)
                    EMIT(ctx, orr_reg(dest, immed, dest, LSL, 0));
                else
                    EMIT(ctx, orr_immed(dest, dest, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                
                if (update_mask)
                    EMIT(ctx, cmn_reg(31, dest, LSL, 0));
                break;
            case 2:
                if (update_mask == 0) {
                    if (mask32 == 0) {
                        EMIT(ctx, orr_reg(dest, dest, immed, LSL, 0));
                    }
                    else
                    {
                        EMIT(ctx, orr_immed(dest, dest, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                    }
                }
                else {
                    if (update_mask == SR_N) {
                        kprintf("ORI.W with update_mask == SR_N\n");
                    }
                    if (update_mask == SR_Z) {
                        if (mask32 == 0) {
                            EMIT(ctx, 
                                orr_reg(dest, dest, immed, LSL, 0),
                                ands_immed(WZR, dest, 16, 0)
                            );
                        }
                        else
                        {
                            EMIT(ctx, 
                                orr_immed(dest, dest, (mask32 >> 16) & 0x3f, mask32 & 0x3f),
                                ands_immed(WZR, dest, 16, 0)
                            );
                        }
                    }
                    else {
                        EMIT(ctx, 
                            orr_reg(immed, immed, dest, LSL, 16),
                            cmn_reg(31, immed, LSL, 0),
                            bfxil(dest, immed, 16, 16)
                        );
                    }
                }
                break;
            case 1:
                if (update_mask == 0) {
                    if (mask32 == 0) {
                        EMIT(ctx, orr_reg(dest, dest, immed, LSL, 0));
                    }
                    else
                    {
                        EMIT(ctx, orr_immed(dest, dest, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                    }
                }
                else {
                    if (update_mask == SR_N) {
                        kprintf("ORI.B with update_mask == SR_N\n");
                    }
                    if (update_mask == SR_Z) {
                        if (mask32 == 0) {
                            EMIT(ctx, 
                                orr_reg(dest, dest, immed, LSL, 0),
                                ands_immed(WZR, dest, 8, 0)
                            );
                        }
                        else
                        {
                            EMIT(ctx, 
                                orr_immed(dest, dest, (mask32 >> 16) & 0x3f, mask32 & 0x3f),
                                ands_immed(WZR, dest, 8, 0)
                            );
                        }
                    }
                    else {
                        EMIT(ctx,
                            orr_reg(immed, immed, dest, LSL, 24),
                            cmn_reg(31, immed, LSL, 0),
                            bfxil(dest, immed, 24, 8)
                        );
                    }
                }
                break;
        }
    }
    else
    {
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t mode = (opcode & 0x0038) >> 3;

        /* Load effective address */
        if (mode == 4 || mode == 3)
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 0, NULL);
        else
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 1, NULL);

        /* Fetch data into temporary register, perform add, store it back */
        switch(size)
        {
            case 4:
                if (mode == 4)
                {
                    EMIT(ctx, ldr_offset_preindex(dest, tmp, -4));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, ldr_offset(dest, tmp, 0));

                /* Perform calcualtion */
                if (mask32 == 0)
                    EMIT(ctx, orr_reg(immed, immed, tmp, LSL, 0));
                else
                    EMIT(ctx, orr_immed(immed, tmp, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                
                if (update_mask)
                    EMIT(ctx, cmn_reg(31, immed, LSL, 0));

                /* Store back */
                if (mode == 3)
                {
                    EMIT(ctx, str_offset_postindex(dest, immed, 4));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, str_offset(dest, immed, 0));
                break;
            case 2:
                if (mode == 4)
                {
                    EMIT(ctx, ldrh_offset_preindex(dest, tmp, -2));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, ldrh_offset(dest, tmp, 0));

                /* Perform calcualtion */
                if (update_mask == 0) {
                    if (mask32 == 0)
                        EMIT(ctx, orr_reg(immed, immed, tmp, LSL, 0));
                    else
                        EMIT(ctx, orr_immed(immed, tmp, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                }
                else {
                    if (update_mask == SR_N) {
                        kprintf("ORI.W (EA) with update_mask == SR_N\n");
                    }
                    if (update_mask == SR_Z) {
                        if (mask32 == 0) {
                            EMIT(ctx, 
                                orr_reg(immed, immed, tmp, LSL, 0),
                                ands_immed(WZR, immed, 16, 0)
                            );
                        }
                        else
                        {
                            EMIT(ctx, 
                                orr_immed(immed, tmp, (mask32 >> 16) & 0x3f, mask32 & 0x3f),
                                ands_immed(WZR, immed, 16, 0)
                            );
                        }
                    }
                    else {
                        EMIT(ctx,
                            orr_reg(immed, immed, tmp, LSL, 16),
                            cmn_reg(31, immed, LSL, 0),
                            lsr(immed, immed, 16)
                        );
                    }
                }

                /* Store back */
                if (mode == 3)
                {
                    EMIT(ctx, strh_offset_postindex(dest, immed, 2));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, strh_offset(dest, immed, 0));
                break;
            case 1:
                if (mode == 4)
                {
                    EMIT(ctx, ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, ldrb_offset(dest, tmp, 0));
                
                /* Perform calcualtion */
                if (update_mask == 0) {
                    if (mask32 == 0)
                        EMIT(ctx, orr_reg(immed, immed, tmp, LSL, 0));
                    else
                        EMIT(ctx, orr_immed(immed, tmp, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                }
                else {
                    if (update_mask == SR_N) {
                        kprintf("ORI.B (EA) with update_mask == SR_N\n");
                    }
                    if (update_mask == SR_Z) {
                        if (mask32 == 0) {
                            EMIT(ctx, 
                                orr_reg(immed, immed, tmp, LSL, 0),
                                ands_immed(WZR, immed, 8, 0)
                            );
                        }
                        else
                        {
                            EMIT(ctx, 
                                orr_immed(immed, tmp, (mask32 >> 16) & 0x3f, mask32 & 0x3f),
                                ands_immed(WZR, immed, 8, 0)
                            );
                        }
                    }
                    else {
                        EMIT(ctx, 
                            orr_reg(immed, immed, tmp, LSL, 24),
                            cmn_reg(31, immed, LSL, 0),
                            lsr(immed, immed, 24)
                        );
                    }
                }

                /* Store back */
                if (mode == 3)
                {
                    EMIT(ctx, strb_offset_postindex(dest, immed, (opcode & 7) == 7 ? 2 : 1));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, strb_offset(dest, immed, 0));
                break;
        }

        RA_FreeARMRegister(ctx, tmp);
    }

    RA_FreeARMRegister(ctx, immed);
    RA_FreeARMRegister(ctx, dest);

    EMIT_AdvancePC(ctx, 2 * (ext_count + 1));
    ctx->tc_M68kCodePtr += ext_count;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);

        EMIT_GetNZ00(ctx, cc, &update_mask);

        if (update_mask & SR_Z)
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
    }

    return 1;
}

uint32_t EMIT_ANDI_TO_CCR(struct TranslatorContext *ctx, uint16_t opcode)
{
    (void)opcode;
    uint8_t immed = RA_AllocARMRegister(ctx);
    uint16_t val = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
   
    /* 
        Before swapping flags - invalidate host flags: all flags modified by this instruction
        are out of sync now
    */
    host_flags &= ~val;

    /* Swap C and V flags in immediate */
    if ((val & 3) != 0 && (val & 3) < 3)
        val ^= 3;

    uint8_t cc = RA_ModifyCC(ctx);

    /* Load immediate into the register */
    EMIT(ctx, 
        mov_immed_u16(immed, 0xff00 | (val & 0x1f), 0),
        and_reg(cc, cc, immed, LSL, 0)
    );

    RA_FreeARMRegister(ctx, immed);

    EMIT_AdvancePC(ctx, 4);
    ctx->tc_M68kCodePtr += 1;

    return 1;
}

uint32_t EMIT_ANDI_TO_SR(struct TranslatorContext *ctx, uint16_t opcode)
{
    (void)opcode;
    uint8_t immed = RA_AllocARMRegister(ctx);
    int16_t val = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint32_t *tmp;

    uint8_t changed = RA_AllocARMRegister(ctx);
    uint8_t orig = RA_AllocARMRegister(ctx);
    uint8_t sp = RA_MapM68kRegister(ctx, 15);
    RA_SetDirtyM68kRegister(ctx, 15);

    uint8_t cc = RA_ModifyCC(ctx);

    /* Swap C and V flags in immediate */
    if ((val & 3) != 0 && (val & 3) < 3)
        val ^= 3;

    EMIT_FlushPC(ctx);
    
    /* If supervisor is not active, put an exception here */
    tmp = ctx->tc_CodePtr++;
    EMIT_Exception(ctx, VECTOR_PRIVILEGE_VIOLATION, 0);
    *tmp = tbnz(cc, SRB_S, 1 + ctx->tc_CodePtr - tmp);
    tmp = ctx->tc_CodePtr++;

    cc = RA_ModifyCC(ctx);

    EMIT(ctx,
        /* Load immediate into the register */
        mov_immed_u16(immed, val & 0xf71f, 0),

        /* AND is here */
        mov_reg(orig, cc),
        and_reg(cc, cc, immed, LSL, 0),

        /* Check what has changed */
        eor_reg(changed, orig, cc, LSL, 0),

        /* If neither S nor M changed, go further */
        ands_immed(31, changed, 2, 32 - SRB_M),
        b_cc(A64_CC_EQ, 12),

        /* S or M changed. First of all, store stack pointer to either ISP or MSP */
        tbz(orig, SRB_M, 3),
        mov_reg_to_simd(REG_MSP, sp),  // Save to MSP
        b(2),
        mov_reg_to_simd(REG_ISP, sp),  // Save to ISP

        /* Check if changing mode to user */
        tbz(changed, SRB_S, 3),
        mov_simd_to_reg(sp, REG_USP),
        b(5),
        tbz(cc, SRB_M, 3),
        mov_simd_to_reg(sp, REG_MSP),  // Load MSP
        b(2),
        mov_simd_to_reg(sp, REG_ISP),  // Load ISP

        // Advance PC
        add_immed(REG_PC, REG_PC, 4),

        // Check if IPL is less than 6. If yes, enable ARM interrupts
        and_immed(changed, cc, 3, 32 - SRB_IPL),
        cmp_immed(changed, 5 << SRB_IPL),
        b_cc(A64_CC_GT, 3),
        msr_imm(3, 7, 7), // Enable interrupts
        b(2),
        msr_imm(3, 6, 7)  // Mask interrupts
    );

    *tmp = b(ctx->tc_CodePtr - tmp);

    EMIT(ctx, INSN_TO_LE(0xffffffff));

    RA_FreeARMRegister(ctx, immed);
    RA_FreeARMRegister(ctx, changed);
    RA_FreeARMRegister(ctx, orig);

    ctx->tc_M68kCodePtr += 1;

    return 1;
}

uint32_t EMIT_ANDI(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(ctx);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    uint16_t lo16;
    uint32_t u32;
    uint32_t mask32 = 0;

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            lo16 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]) & 0xff;
            if (update_mask == 0) {
                if ((opcode & 0x0038) == 0) {
                    if (lo16 != 0xff) {
                        mask32 = number_to_mask(0xffffff00 | lo16);
                    }
                    else mask32 = 0;
                    if (mask32 == 0) {
                        EMIT(ctx, movn_immed_u16(immed, (~lo16) & 0xff, 0));
                    }
                }
                else {
                    mask32 = number_to_mask(lo16);
                    if (mask32 == 0) {
                        EMIT(ctx, mov_immed_u16(immed, (lo16 & 0xff), 0));
                    }
                }
            }
            else
                EMIT(ctx, mov_immed_u16(immed, (lo16 & 0xff) << 8, 1));

            size = 1;
            break;
        case 0x0040:    /* Short operation */
            lo16 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]);
            if (update_mask == 0) {
                if ((opcode & 0x0038) == 0) {
                    if (lo16 != 0xffff) 
                        mask32 = number_to_mask(0xffff0000 | lo16);
                    else
                        mask32 = 0;
                    if (mask32 == 0) {
                        EMIT(ctx, movn_immed_u16(immed, ~lo16, 0));
                    }
                }
                else {
                    mask32 = number_to_mask(lo16);
                    if (mask32 == 0) {
                        EMIT(ctx, mov_immed_u16(immed, lo16, 0));
                    }
                }
            }
            else
                EMIT(ctx, mov_immed_u16(immed, lo16, 1));

            size = 2;
            break;
        case 0x0080:    /* Long operation */
            u32 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]) << 16;
            u32 |= cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]);
            mask32 = number_to_mask(u32);
            if (mask32 == 0)
            {
                EMIT_LoadImmediate(ctx, immed, u32);
            }
            size = 4;
            break;
    }

    /* handle adding to register here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register */
        dest = RA_MapM68kRegister(ctx, opcode & 7);

        /* Mark register dirty */
        RA_SetDirtyM68kRegister(ctx, opcode & 7);

        /* Perform add operation */
        switch(size)
        {
            case 4:
                if (mask32 == 0) {
                    if (update_mask == 0)
                        EMIT(ctx, and_reg(dest, immed, dest, LSL, 0));
                    else
                        EMIT(ctx, ands_reg(dest, immed, dest, LSL, 0));
                }
                else {
                    if (update_mask == 0)
                        EMIT(ctx, and_immed(dest, dest, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                    else
                        EMIT(ctx, ands_immed(dest, dest, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                }
                break;
            case 2:
                if (update_mask == 0) {
                    if (mask32 == 0)
                        EMIT(ctx, and_reg(dest, dest, immed, LSL, 0));
                    else
                        EMIT(ctx, and_immed(dest, dest, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                }
                else {
                    EMIT(ctx, 
                        ands_reg(immed, immed, dest, LSL, 16),
                        bfxil(dest, immed, 16, 16)
                    );
                }
                break;
            case 1:
                if (update_mask == 0) {
                    if (mask32 == 0)
                        EMIT(ctx, and_reg(dest, dest, immed, LSL, 0));
                    else
                        EMIT(ctx, and_immed(dest, dest, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                }
                else {
                    EMIT(ctx, 
                        ands_reg(immed, immed, dest, LSL, 24),
                        bfxil(dest, immed, 24, 8)
                    );
                }
                break;
        }
    }
    else
    {
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t mode = (opcode & 0x0038) >> 3;

        /* Load effective address */
        if (mode == 4 || mode == 3)
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 0, NULL);
        else
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 1, NULL);

        /* Fetch data into temporary register, perform add, store it back */
        switch(size)
        {
            case 4:
                if (mode == 4)
                {
                    EMIT(ctx, ldr_offset_preindex(dest, tmp, -4));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, ldr_offset(dest, tmp, 0));

                /* Perform calcualtion */
                if (mask32 == 0)
                    EMIT(ctx, ands_reg(immed, immed, tmp, LSL, 0));
                else
                    EMIT(ctx, ands_immed(immed, tmp, (mask32 >> 16) & 0x3f, mask32 & 0x3f));

                /* Store back */
                if (mode == 3)
                {
                    EMIT(ctx, str_offset_postindex(dest, immed, 4));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, str_offset(dest, immed, 0));
                break;
            case 2:
                if (mode == 4)
                {
                    EMIT(ctx, ldrh_offset_preindex(dest, tmp, -2));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, ldrh_offset(dest, tmp, 0));
                
                /* Perform calcualtion */
                if (update_mask == 0) {
                    if (mask32 == 0)
                        EMIT(ctx, and_reg(immed, immed, tmp, LSL, 0));
                    else
                        EMIT(ctx, and_immed(immed, tmp, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                }
                else {
                    EMIT(ctx, 
                        ands_reg(immed, immed, tmp, LSL, 16),
                        lsr(immed, immed, 16)
                    );
                }

                /* Store back */
                if (mode == 3)
                {
                    EMIT(ctx, strh_offset_postindex(dest, immed, 2));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, strh_offset(dest, immed, 0));
                break;
            case 1:
                if (mode == 4)
                {
                    EMIT(ctx, ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, ldrb_offset(dest, tmp, 0));
                
                /* Perform calcualtion */
                if (update_mask == 0) {
                    if (mask32 == 0)
                        EMIT(ctx, and_reg(immed, immed, tmp, LSL, 0));
                    else
                        EMIT(ctx, and_immed(immed, tmp, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                }
                else {
                    EMIT(ctx, 
                        ands_reg(immed, immed, tmp, LSL, 24),
                        lsr(immed, immed, 24)
                    );
                }

                /* Store back */
                if (mode == 3)
                {
                    EMIT(ctx, strb_offset_postindex(dest, immed, (opcode & 7) == 7 ? 2 : 1));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, strb_offset(dest, immed, 0));
                break;
        }

        RA_FreeARMRegister(ctx, tmp);
    }

    RA_FreeARMRegister(ctx, immed);
    RA_FreeARMRegister(ctx, dest);

    EMIT_AdvancePC(ctx, 2 * (ext_count + 1));
    ctx->tc_M68kCodePtr += ext_count;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);
        EMIT_GetNZ00(ctx, cc, &update_mask);

        if (update_mask & SR_Z)
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
    }

    return 1;
}

uint32_t EMIT_EORI_TO_CCR(struct TranslatorContext *ctx, uint16_t opcode)
{
    (void)opcode;
    uint8_t immed = RA_AllocARMRegister(ctx);
    int16_t val = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);

    /* 
        Before swapping flags - invalidate host flags: all flags modified by this instruction
        are out of sync now
    */
    host_flags &= ~val;

    /* Swap C and V flags in immediate */
    if ((val & 3) != 0 && (val & 3) < 3)
        val ^= 3;

    uint8_t cc = RA_ModifyCC(ctx);

    EMIT(ctx,
        /* Load immediate into the register */
        mov_immed_u8(immed, val & 0x1f),
        
        /* EOR with status register, no need to check mask, ARM sequence way too short! */
        eor_reg(cc, cc, immed, LSL, 0)
    );
    
    RA_FreeARMRegister(ctx, immed);

    EMIT_AdvancePC(ctx, 4);
    ctx->tc_M68kCodePtr += 1;

    return 1;
}

uint32_t EMIT_EORI_TO_SR(struct TranslatorContext *ctx, uint16_t opcode)
{
    (void)opcode;
    uint8_t immed = RA_AllocARMRegister(ctx);
    int16_t val = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint32_t *tmp;

    uint8_t orig = RA_AllocARMRegister(ctx);
    uint8_t sp = RA_MapM68kRegister(ctx, 15);
    RA_SetDirtyM68kRegister(ctx, 15);

    uint8_t cc = RA_ModifyCC(ctx);
    
    /* Swap C and V flags in immediate */
    if ((val & 3) != 0 && (val & 3) < 3)
        val ^= 3;

    EMIT_FlushPC(ctx);
    
    /* If supervisor is not active, put an exception here */
    tmp = ctx->tc_CodePtr++;
    EMIT_Exception(ctx, VECTOR_PRIVILEGE_VIOLATION, 0);
    *tmp = tbnz(cc, SRB_S, 1 + ctx->tc_CodePtr - tmp);
    tmp = ctx->tc_CodePtr++;

    cc = RA_ModifyCC(ctx);

    EMIT(ctx,
        /* Load immediate into the register */
        mov_immed_u16(immed, val & 0xf71f, 0),

        /* EOR is here */
        mov_reg(orig, cc),
        eor_reg(cc, cc, immed, LSL, 0),

        /* If neither S nor M changed, go further */
        ands_immed(31, immed, 2, 32 - SRB_M),
        b_cc(A64_CC_EQ, 12),

        /* S or M changed. First of all, store stack pointer to either ISP or MSP */
        tbz(orig, SRB_M, 3),
        mov_reg_to_simd(REG_MSP, sp),  // Save to MSP
        b(2),
        mov_reg_to_simd(REG_ISP, sp),  // Save to ISP

        /* Check if changing mode to user */
        tbz(immed, SRB_S, 3),
        mov_simd_to_reg(sp, REG_USP),
        b(5),
        tbz(cc, SRB_M, 3),
        mov_simd_to_reg(sp, REG_MSP),  // Load MSP
        b(2),
        mov_simd_to_reg(sp, REG_ISP),  // Load ISP

        // Advance PC
        add_immed(REG_PC, REG_PC, 4),

        // Check if IPL is less than 6. If yes, enable ARM interrupts
        and_immed(immed, cc, 3, 32 - SRB_IPL),
        cmp_immed(immed, 5 << SRB_IPL),
        b_cc(A64_CC_GT, 3),
        msr_imm(3, 7, 7), // Enable interrupts
        b(2),
        msr_imm(3, 6, 7)  // Mask interrupts
    );


    *tmp = b(ctx->tc_CodePtr - tmp);

    EMIT(ctx, INSN_TO_LE(0xffffffff));

    RA_FreeARMRegister(ctx, immed);
    RA_FreeARMRegister(ctx, orig);

    ctx->tc_M68kCodePtr += 1;

    return 1;
}

uint32_t EMIT_EORI(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(ctx);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    int16_t lo16 = 0;
    uint32_t u32;
    uint32_t mask32 = 0;

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            lo16 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]) & 0xff;
            mask32 = number_to_mask(lo16);
            if (mask32 == 0)
            {
                EMIT(ctx, mov_immed_u16(immed, (lo16 & 0xff), 0));
            }
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            lo16 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]);
            mask32 = number_to_mask(lo16);
            if (mask32 == 0)
            {
                EMIT(ctx, mov_immed_u16(immed, lo16, 0));
            }
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            u32 = cache_read_16(ICACHE, (uintptr_t)&ctx->   tc_M68kCodePtr[ext_count++]) << 16;
            u32 |= cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]);
            mask32 = number_to_mask(u32);
            if (mask32 == 0)
            {
                EMIT_LoadImmediate(ctx, immed, u32);
            }
            size = 4;
            break;
    }

    /* handle adding to register here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register */
        dest = RA_MapM68kRegister(ctx, opcode & 7);

        /* Mark register dirty */
        RA_SetDirtyM68kRegister(ctx, opcode & 7);

        /* Perform add operation */
        switch(size)
        {
            case 4:
                if (mask32 == 0)
                    EMIT(ctx, eor_reg(dest, dest, immed, LSL, 0));
                else
                    EMIT(ctx, eor_immed(dest, dest, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                if (update_mask)
                    EMIT(ctx, cmn_reg(31, dest, LSL, 0));
                break;
            case 2:
                if (mask32 == 0)
                {
                    EMIT(ctx, eor_reg(immed, immed, dest, LSL, 0));
                    if (update_mask)
                        EMIT(ctx, cmn_reg(31, immed, LSL, 16));
                    EMIT(ctx, bfxil(dest, immed, 0, 16));
                }
                else
                {
                    EMIT(ctx, eor_immed(dest, dest, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                    if (update_mask)
                        EMIT(ctx, cmn_reg(31, dest, LSL, 16));
                }
                break;
            case 1:
                if (mask32 == 0)
                {
                    EMIT(ctx, eor_reg(immed, immed, dest, LSL, 0));
                    if (update_mask)
                        EMIT(ctx, cmn_reg(31, immed, LSL, 24));
                    EMIT(ctx, bfxil(dest, immed, 0, 8));
                }
                else
                {
                    EMIT(ctx, eor_immed(dest, dest, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                    if (update_mask)
                        EMIT(ctx, cmn_reg(31, dest, LSL, 24));
                }
                break;
        }
    }
    else
    {
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t mode = (opcode & 0x0038) >> 3;

        /* Load effective address */
        if (mode == 4 || mode == 3)
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 0, NULL);
        else
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 1, NULL);

        /* Fetch data into temporary register, perform add, store it back */
        switch(size)
        {
            case 4:
                if (mode == 4)
                {
                    EMIT(ctx, ldr_offset_preindex(dest, tmp, -4));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, ldr_offset(dest, tmp, 0));

                /* Perform calcualtion */
                if (mask32 == 0)
                    EMIT(ctx, eor_reg(immed, immed, tmp, LSL, 0));
                else
                    EMIT(ctx, eor_immed(immed, tmp, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                if (update_mask)
                    EMIT(ctx, cmn_reg(31, immed, LSL, 0));

                /* Store back */
                if (mode == 3)
                {
                    EMIT(ctx, str_offset_postindex(dest, immed, 4));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, str_offset(dest, immed, 0));
                break;
            case 2:
                if (mode == 4)
                {
                    EMIT(ctx, ldrh_offset_preindex(dest, tmp, -2));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, ldrh_offset(dest, tmp, 0));
                
                /* Perform calcualtion */
                if (mask32 == 0)
                    EMIT(ctx, eor_reg(immed, immed, tmp, LSL, 0));
                else
                    EMIT(ctx, eor_immed(immed, tmp, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                if (update_mask)
                    EMIT(ctx, cmn_reg(31, immed, LSL, 16));

                /* Store back */
                if (mode == 3)
                {
                    EMIT(ctx, strh_offset_postindex(dest, immed, 2));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, strh_offset(dest, immed, 0));
                break;
            
            case 1:
                if (mode == 4)
                {
                    EMIT(ctx, ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, ldrb_offset(dest, tmp, 0));
                
                /* Perform calcualtion */
                if (mask32 == 0)
                    EMIT(ctx, eor_reg(immed, immed, tmp, LSL, 0));
                else
                    EMIT(ctx, eor_immed(immed, tmp, (mask32 >> 16) & 0x3f, mask32 & 0x3f));
                if (update_mask)
                    EMIT(ctx, cmn_reg(31, immed, LSL, 24));

                /* Store back */
                if (mode == 3)
                {
                    EMIT(ctx, strb_offset_postindex(dest, immed, (opcode & 7) == 7 ? 2 : 1));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, strb_offset(dest, immed, 0));
                break;
        }

        RA_FreeARMRegister(ctx, tmp);
    }

    RA_FreeARMRegister(ctx, immed);
    RA_FreeARMRegister(ctx, dest);

    EMIT_AdvancePC(ctx, 2 * (ext_count + 1));
    ctx->tc_M68kCodePtr += ext_count;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);
        EMIT_GetNZ00(ctx, cc, &update_mask);

        if (update_mask & SR_Z)
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
    }

    return 1;
}

uint32_t EMIT_BTST(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_count = 0;
    uint8_t bit_number = 0xff;
    uint8_t dest = 0xff;
    uint8_t bit_mask = 0xff;
    int imm_shift = 0;
    int immediate = 0;

    /* Get the bit number either as immediate or from register */
    if ((opcode & 0xffc0) == 0x0800)
    {
        immediate = 1;
        imm_shift = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]) & 31;
    }
    else
    {
        bit_number = RA_CopyFromM68kRegister(ctx, (opcode >> 9) & 7);
        bit_mask = RA_AllocARMRegister(ctx);
        EMIT(ctx, mov_immed_u8(bit_mask, 1));
    }

    /* handle direct register more here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register */
        dest = RA_MapM68kRegister(ctx, opcode & 7);

        if (immediate)
        {
            EMIT(ctx, tst_immed(dest, 1, 31 & (32 - imm_shift)));
        }
        else
        {
            EMIT(ctx, 
                and_immed(bit_number, bit_number, 5, 0),
                lslv(bit_mask, bit_mask, bit_number),
                tst_reg(dest, bit_mask, LSL, 0)
            );
        }
    }
    else
    {
        /* Load byte from effective address */
        EMIT_LoadFromEffectiveAddress(ctx, 1, &dest, opcode & 0x3f, &ext_count, 0, NULL);

        if (immediate)
        {
            EMIT(ctx, tst_immed(dest, 1, 31 & (32 - (imm_shift & 7))));
        }
        else
        {
            EMIT(ctx, 
                and_immed(bit_number, bit_number, 3, 0),
                lslv(bit_mask, bit_mask, bit_number),
                tst_reg(dest, bit_mask, LSL, 0)
            );
        }
    }

    host_flags = SR_Z;

    RA_FreeARMRegister(ctx, bit_number);
    RA_FreeARMRegister(ctx, bit_mask);
    RA_FreeARMRegister(ctx, dest);

    EMIT_AdvancePC(ctx, 2 * (ext_count + 1));
    ctx->tc_M68kCodePtr += ext_count;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);

        if (update_mask & SR_Z)
        {
            EMIT(ctx, 
                cset(0, A64_CC_EQ),
                bfi(cc, 0, SRB_Z, 1)
            );
        }
    }

    return 1;
}

uint32_t EMIT_BCHG(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_count = 0;
    uint8_t bit_number = 0xff;
    uint8_t dest = 0xff;
    uint8_t bit_mask = 0xff;
    int imm_shift = 0;
    int immediate = 0;

    /* Get the bit number either as immediate or from register */
    if ((opcode & 0xffc0) == 0x0840)
    {
        immediate = 1;
        imm_shift = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]) & 31;
    }
    else
    {
        bit_number = RA_CopyFromM68kRegister(ctx, (opcode >> 9) & 7);
        bit_mask = RA_AllocARMRegister(ctx);
        EMIT(ctx, mov_immed_u8(bit_mask, 1));
    }

    /* handle direct register more here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register and mark it dirty - one bit will change */
        dest = RA_MapM68kRegister(ctx, opcode & 7);
        RA_SetDirtyM68kRegister(ctx, opcode & 7);

        if (immediate)
        {
            if (update_mask)
                EMIT(ctx, tst_immed(dest, 1, 31 & (32 - imm_shift)));
            EMIT(ctx, eor_immed(dest, dest, 1, 31 & (32 - imm_shift)));
        }
        else
        {
            EMIT(ctx, 
                and_immed(bit_number, bit_number, 5, 0),
                lslv(bit_mask, bit_mask, bit_number)
            );

            if (update_mask)
                EMIT(ctx, tst_reg(dest, bit_mask, LSL, 0));
            
            EMIT(ctx, eor_reg(dest, dest, bit_mask, LSL, 0));
        }
    }
    else
    {
        /* Load effective address */
        EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 1, NULL);
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t mode = (opcode & 0x0038) >> 3;

        /* Fetch data into temporary register, perform bit flip, store it back */
        if (mode == 4)
        {
            EMIT(ctx, ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1));
            RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
        }
        else
            EMIT(ctx, ldrb_offset(dest, tmp, 0));

        if (immediate)
        {
            if (update_mask)
                EMIT(ctx, tst_immed(tmp, 1, 31 & (32 - (imm_shift & 7))));
            
            EMIT(ctx, eor_immed(tmp, tmp, 1, 31 & (32 - (imm_shift & 7))));
        }
        else
        {
            EMIT(ctx, 
                and_immed(bit_number, bit_number, 3, 0),
                lslv(bit_mask, bit_mask, bit_number)
            );

            if (update_mask)
                EMIT(ctx, tst_reg(tmp, bit_mask, LSL, 0));
            
            EMIT(ctx, eor_reg(tmp, tmp, bit_mask, LSL, 0));
        }

        /* Store back */
        if (mode == 3)
        {
            EMIT(ctx, strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1));
            RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
        }
        else
            EMIT(ctx, strb_offset(dest, tmp, 0));

        RA_FreeARMRegister(ctx, tmp);
    }

    RA_FreeARMRegister(ctx, bit_number);
    RA_FreeARMRegister(ctx, bit_mask);
    RA_FreeARMRegister(ctx, dest);

    EMIT_AdvancePC(ctx, 2 * (ext_count + 1));
    ctx->tc_M68kCodePtr += ext_count;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);

        host_flags = SR_Z;

        if (update_mask & SR_Z)
        {
            EMIT(ctx, 
                cset(0, A64_CC_EQ),
                bfi(cc, 0, SRB_Z, 1)
            );
        }
    }

    return 1;
}

uint32_t EMIT_BCLR(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_count = 0;
    uint8_t bit_number = 0xff;
    uint8_t dest = 0xff;
    uint8_t bit_mask = 0xff;
    int imm_shift = 0;
    int immediate = 0;

    /* Get the bit number either as immediate or from register */
    if ((opcode & 0xffc0) == 0x0880)
    {
        immediate = 1;
        imm_shift = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]) & 31;
    }
    else
    {
        bit_number = RA_CopyFromM68kRegister(ctx, (opcode >> 9) & 7);
        bit_mask = RA_AllocARMRegister(ctx);
        EMIT(ctx, mov_immed_u8(bit_mask, 1));
    }

    /* handle direct register more here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register and mark it dirty - one bit will change */
        dest = RA_MapM68kRegister(ctx, opcode & 7);
        RA_SetDirtyM68kRegister(ctx, opcode & 7);

        if (immediate)
        {
            if (update_mask)
                EMIT(ctx, tst_immed(dest, 1, 31 & (32 - imm_shift)));
            
            EMIT(ctx, bic_immed(dest, dest, 1, 31 & (32 - imm_shift)));
        }
        else
        {
            EMIT(ctx, 
                and_immed(bit_number, bit_number, 5, 0),
                lslv(bit_mask, bit_mask, bit_number)
            );

            if (update_mask)
                EMIT(ctx, tst_reg(dest, bit_mask, LSL, 0));
            
            EMIT(ctx, bic_reg(dest, dest, bit_mask, LSL, 0));
        }
    }
    else
    {
        /* Load effective address */
        EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 1, NULL);
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t mode = (opcode & 0x0038) >> 3;

        /* Fetch data into temporary register, perform bit flip, store it back */
        if (mode == 4)
        {
            EMIT(ctx, ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1));
            RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
        }
        else
            EMIT(ctx, ldrb_offset(dest, tmp, 0));

        if (immediate)
        {
            if (update_mask)
                EMIT(ctx, tst_immed(tmp, 1, 31 & (32 - (imm_shift & 7))));
            
            EMIT(ctx, bic_immed(tmp, tmp, 1, 31 & (32 - (imm_shift & 7))));
        }
        else
        {
            EMIT(ctx, 
                and_immed(bit_number, bit_number, 3, 0),
                lslv(bit_mask, bit_mask, bit_number)
            );

            if (update_mask)
                EMIT(ctx, tst_reg(tmp, bit_mask, LSL, 0));
            
            EMIT(ctx, bic_reg(tmp, tmp, bit_mask, LSL, 0));
        }

        /* Store back */
        if (mode == 3)
        {
            EMIT(ctx, strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1));
            RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
        }
        else
            EMIT(ctx, strb_offset(dest, tmp, 0));

        RA_FreeARMRegister(ctx, tmp);
    }

    RA_FreeARMRegister(ctx, bit_number);
    RA_FreeARMRegister(ctx, bit_mask);
    RA_FreeARMRegister(ctx, dest);

    EMIT_AdvancePC(ctx, 2 * (ext_count + 1));
    ctx->tc_M68kCodePtr += ext_count;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);

        host_flags = SR_Z;

        if (update_mask & SR_Z)
        {
            EMIT(ctx, 
                cset(0, A64_CC_EQ),
                bfi(cc, 0, SRB_Z, 1)
            );
        }
    }

    return 1;
}

uint32_t EMIT_CMP2(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint32_t opcode_address = (uint32_t)(uintptr_t)(ctx->tc_M68kCodePtr - 1);
    uint8_t update_mask = SR_Z | SR_C;
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t ea = -1;
    uint8_t lower = RA_AllocARMRegister(ctx);
    uint8_t higher = RA_AllocARMRegister(ctx);
    uint8_t reg = RA_MapM68kRegister(ctx, opcode2 >> 12);

    /* Get address of bounds */
    EMIT_LoadFromEffectiveAddress(ctx, 0, &ea, opcode & 0x3f, &ext_words, 1, NULL);

    uint8_t cc = RA_ModifyCC(ctx);

    /* load bounds into registers */
    switch ((opcode >> 9) & 3)
    {
        case 0:
            EMIT(ctx, 
                ldrsb_offset(ea, lower, 0),
                ldrsb_offset(ea, higher, 1)
            );
            break;
        case 1:
            EMIT(ctx, 
                ldrsh_offset(ea, lower, 0),
                ldrsh_offset(ea, higher, 2)
            );
            break;
        case 2:
            EMIT(ctx, ldp(ea, lower, higher, 0));
            break;
    }

    /* If data register, extend the 8 or 16 bit */
    if ((opcode2 & 0x8000) == 0)
    {
        uint8_t tmp = -1;
        switch((opcode >> 9) & 3)
        {
            case 0:
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, sxtb(tmp, reg));
                reg = tmp;
                break;
            case 1:
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, sxth(tmp, reg));
                reg = tmp;
                break;
        }
    }

    EMIT_ClearFlags(ctx, cc, SR_ZCalt);

    uint32_t *exit_1, *exit_2;
    uint8_t tmp1 = RA_AllocARMRegister(ctx);
    uint8_t tmp2 = RA_AllocARMRegister(ctx);

    EMIT(ctx, 
        cmp_reg(reg, lower, LSL, 0),
        ccmp_reg(reg, higher, 4, A64_CC_NE),
        b_cc(A64_CC_NE, 3),
        orr_immed(cc, cc, 1, 31 & (32 - SRB_Z))
    );
    
    exit_1 = ctx->tc_CodePtr++;

    EMIT(ctx, 
        cmp_reg(reg, higher, LSL, 0),
        cset(tmp1, A64_CC_HI),
        cmp_reg(reg, lower, LSL, 0),
        cset(tmp2, A64_CC_CC),
        cmp_reg(lower, higher, LSL, 0),
        b_cc(A64_CC_HI, 6),
        cmp_reg(31, tmp2, LSL, 0),
        orr_immed(lower, cc, 1, 32 - SRB_Calt),
        ccmp_reg(tmp1, 31, 0, A64_CC_EQ),
        csel(cc, lower, cc, A64_CC_NE)
    );
    
    exit_2 = ctx->tc_CodePtr++;

    EMIT(ctx, 
        cmp_reg(31, tmp2, LSL, 0),
        orr_immed(lower, cc, 1, 32 - SRB_Calt),
        ccmp_reg(31, tmp1, 4, A64_CC_NE),
        csel(cc, lower, cc, A64_CC_NE)
    );

    RA_FreeARMRegister(ctx, tmp1);
    RA_FreeARMRegister(ctx, tmp2);

    *exit_2 = b(ctx->tc_CodePtr - exit_2);
    *exit_1 = b(ctx->tc_CodePtr - exit_1);  

    (void)update_mask;

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    RA_FreeARMRegister(ctx, ea);
    RA_FreeARMRegister(ctx, reg);
    RA_FreeARMRegister(ctx, lower);
    RA_FreeARMRegister(ctx, higher);

    /* If CHK2 opcode then emit exception if tested value was out of range (C flag set) */
    if (opcode2 & (1 << 11))
    {
        /* Flush program counter, since it might be pushed on the stack when
        exception is generated */
        EMIT_FlushPC(ctx);

        /* Skip exception if C is not set */
        uint32_t *t = ctx->tc_CodePtr++;

        /* Emit CHK exception */
        EMIT_Exception(ctx, VECTOR_CHK, 2, opcode_address);
        *t = tbz(cc, SRB_Calt, ctx->tc_CodePtr - t);

        EMIT(ctx, 
            (uint32_t)(uintptr_t)t,
            1, 0,
            INSN_TO_LE(0xfffffffe)
        );
    }

    return 1;
}

uint32_t EMIT_BSET(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_count = 0;
    uint8_t bit_number = 0xff;
    uint8_t dest = 0xff;
    uint8_t bit_mask = 0xff;
    int imm_shift = 0;
    int immediate = 0;

    /* Get the bit number either as immediate or from register */
    if ((opcode & 0xffc0) == 0x08c0)
    {
        immediate = 1;
        imm_shift = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[ext_count++]) & 31;
    }
    else
    {
        bit_number = RA_CopyFromM68kRegister(ctx, (opcode >> 9) & 7);
        bit_mask = RA_AllocARMRegister(ctx);
        EMIT(ctx, mov_immed_u8(bit_mask, 1));
    }

    /* handle direct register more here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register and mark it dirty - one bit will change */
        dest = RA_MapM68kRegister(ctx, opcode & 7);
        RA_SetDirtyM68kRegister(ctx, opcode & 7);

        if (immediate)
        {
            if (update_mask)
                EMIT(ctx, tst_immed(dest, 1, 31 & (32 - imm_shift)));
            
            EMIT(ctx, orr_immed(dest, dest, 1, 31 & (32 - imm_shift)));
        }
        else
        {
            EMIT(ctx, 
                and_immed(bit_number, bit_number, 5, 0),
                lslv(bit_mask, bit_mask, bit_number)
            );

            if (update_mask)
                EMIT(ctx, tst_reg(dest, bit_mask, LSL, 0));

            EMIT(ctx, orr_reg(dest, dest, bit_mask, LSL, 0));
        }
    }
    else
    {
        /* Load effective address */
        EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 1, NULL);
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t mode = (opcode & 0x0038) >> 3;

        /* Fetch data into temporary register, perform bit flip, store it back */
        if (mode == 4)
        {
            EMIT(ctx, ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1));
            RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
        }
        else
            EMIT(ctx, ldrb_offset(dest, tmp, 0));

        if (immediate)
        {
            if (update_mask)
                EMIT(ctx, tst_immed(tmp, 1, 31 & (32 - (imm_shift & 7))));

            EMIT(ctx, orr_immed(tmp, tmp, 1, 31 & (32 - (imm_shift & 7))));
        }
        else
        {
            EMIT(ctx, 
                and_immed(bit_number, bit_number, 3, 0),
                lslv(bit_mask, bit_mask, bit_number)
            );

            if (update_mask)
                EMIT(ctx, tst_reg(tmp, bit_mask, LSL, 0));

            EMIT(ctx, orr_reg(tmp, tmp, bit_mask, LSL, 0));
        }

        /* Store back */
        if (mode == 3)
        {
            EMIT(ctx, strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1));
            RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
        }
        else
            EMIT(ctx, strb_offset(dest, tmp, 0));

        RA_FreeARMRegister(ctx, tmp);
    }

    RA_FreeARMRegister(ctx, bit_number);
    RA_FreeARMRegister(ctx, bit_mask);
    RA_FreeARMRegister(ctx, dest);

    EMIT_AdvancePC(ctx, 2 * (ext_count + 1));
    ctx->tc_M68kCodePtr += ext_count;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);

        host_flags = SR_Z;

        if (update_mask & SR_Z)
        {
            EMIT(ctx, 
                cset(0, A64_CC_EQ),
                bfi(cc, 0, SRB_Z, 1)
            );
        }
    }

    return 1;
}

uint32_t EMIT_CAS2(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_CAS")));
uint32_t EMIT_CAS(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);

#define CAS_ATOMIC() do { \
        uint32_t *l0 = ctx->tc_CodePtr; \
        switch (size) \
        { \
            case 1:\
                EMIT(ctx, ldxrb(ea, tmp),\
                          lsl(tmp, tmp, 24),\
                          subs_reg(31, tmp, dc, LSL, 24));\
                break;\
            case 2:\
                EMIT(ctx, ldxrh(ea, tmp),\
                          lsl(tmp, tmp, 16),\
                          subs_reg(31, tmp, dc, LSL, 16));\
                break;\
            case 3:\
                EMIT(ctx, ldxr(ea, tmp),\
                          subs_reg(31, tmp, dc, LSL, 0));\
                break;\
        }\
        uint32_t *b0 = ctx->tc_CodePtr;\
        EMIT(ctx, b_cc(A64_CC_NE, 0));\
        switch (size)\
        {\
            case 1:\
                EMIT(ctx, stlxrb(ea, du, status));\
                break;\
            case 2:\
                EMIT(ctx, stlxrh(ea, du, status));\
                break;\
            case 3:\
                EMIT(ctx, stlxr(ea, du, status));\
                break;\
        }\
        *ctx->tc_CodePtr = cbnz(status, l0 - ctx->tc_CodePtr);\
        ctx->tc_CodePtr++;\
        EMIT(ctx, b(2));\
        *b0 = b_cc(A64_CC_NE, ctx->tc_CodePtr - b0);\
        switch (size) \
        {\
            case 1:\
                EMIT(ctx, bfxil(dc, tmp, 24, 8));\
                break;\
            case 2:\
                EMIT(ctx, bfxil(dc, tmp, 16, 16));\
                break;\
            case 3:\
                EMIT(ctx, mov_reg(dc, tmp));\
                break;\
        }\
} while(0)

#define CAS_UNSAFE() do { \
        switch (size) \
        { \
            case 1:\
                EMIT(ctx, ldrb_offset(ea, tmp, 0),\
                          lsl(tmp, tmp, 24),\
                          subs_reg(31, tmp, dc, LSL, 24));\
                break;\
            case 2:\
                EMIT(ctx, ldrh_offset(ea, tmp, 0),\
                          lsl(tmp, tmp, 16),\
                          subs_reg(31, tmp, dc, LSL, 16));\
                break;\
            case 3:\
                EMIT(ctx, ldr_offset(ea, tmp, 0),\
                          subs_reg(31, tmp, dc, LSL, 0));\
                break;\
        }\
        uint32_t *b0 = ctx->tc_CodePtr;\
        EMIT(ctx, b_cc(A64_CC_NE, 0));\
        switch (size)\
        {\
            case 1:\
                EMIT(ctx, strb_offset(ea, du, 0));\
                break;\
            case 2:\
                EMIT(ctx, strh_offset(ea, du, 0));\
                break;\
            case 3:\
                EMIT(ctx, str_offset(ea, du, 0));\
                break;\
        }\
        EMIT(ctx, b(2));\
        *b0 = b_cc(A64_CC_NE, ctx->tc_CodePtr - b0);\
        switch (size) \
        {\
            case 1:\
                EMIT(ctx, bfxil(dc, tmp, 24, 8));\
                break;\
            case 2:\
                EMIT(ctx, bfxil(dc, tmp, 16, 16));\
                break;\
            case 3:\
                EMIT(ctx, mov_reg(dc, tmp));\
                break;\
        }\
} while(0)

    /* CAS2 */
    if ((opcode & 0xfdff) == 0x0cfc)
    {
        uint8_t ext_words = 2;
        uint8_t size = (opcode >> 9) & 3;
        uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[0]);
        uint16_t opcode3 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[1]);

        uint8_t rn1 = RA_MapM68kRegister(ctx, (opcode2 >> 12) & 15);
        uint8_t rn2 = RA_MapM68kRegister(ctx, (opcode3 >> 12) & 15);
        uint8_t du1 = RA_MapM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t du2 = RA_MapM68kRegister(ctx, (opcode3 >> 6) & 7);
        uint8_t dc1 = RA_MapM68kRegister(ctx, (opcode2) & 7);
        uint8_t dc2 = RA_MapM68kRegister(ctx, (opcode3) & 7);

        RA_SetDirtyM68kRegister(ctx, (opcode2) & 7);
        RA_SetDirtyM68kRegister(ctx, (opcode3) & 7);

        uint8_t val1 = RA_AllocARMRegister(ctx);
        uint8_t val2 = RA_AllocARMRegister(ctx);

        if (size==2)
        {
            uint8_t tmp1 = RA_AllocARMRegister(ctx);
            uint8_t tmp2 = RA_AllocARMRegister(ctx);

            EMIT(ctx, 
                ldrh_offset(rn1, val1, 0),
                ldrh_offset(rn2, val2, 0),
                lsl(val1, val1, 16),
                lsl(val2, val2, 16),
                subs_reg(31, val1, dc1, LSL, 16),
                b_cc(A64_CC_NE, 6),
                subs_reg(31, val2, dc2, LSL, 16),
                b_cc(A64_CC_NE, 4),
                
                // 68040 stores du2 first, then du1
                strh_offset(rn2, du2, 0),
                strh_offset(rn1, du1, 0),
                b(3),
                bfxil(dc1, val1, 16, 16),
                bfxil(dc2, val2, 16, 16)
            );

            RA_FreeARMRegister(ctx, tmp1);
            RA_FreeARMRegister(ctx, tmp2);
        }
        else
        {
            EMIT(ctx, 
                ldr_offset(rn1, val1, 0),
                ldr_offset(rn2, val2, 0),
                subs_reg(31, val1, dc1, LSL, 0),
                b_cc(A64_CC_NE, 6),
                subs_reg(31, val2, dc2, LSL, 0),
                b_cc(A64_CC_NE, 4),
                
                // 68040 stores du2 first, then du1
                str_offset(rn2, du2, 0),
                str_offset(rn1, du1, 0),
                b(3),
                mov_reg(dc1, val1),
                mov_reg(dc2, val2)
            );
        }

        EMIT(ctx, dmb_ish());

        EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
        ctx->tc_M68kCodePtr += ext_words;

        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(ctx);

            if (__builtin_popcount(update_mask) != 0)
            {
                EMIT_GetNZnCV(ctx, cc, &update_mask);
                
                if (update_mask & SR_Z)
                    EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
                if (update_mask & SR_N)
                    EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
                if (update_mask & SR_V)
                    EMIT_SetFlagsConditional(ctx, cc, SR_Valt, ARM_CC_VS);
                if (update_mask & SR_C)
                    EMIT_SetFlagsConditional(ctx, cc, SR_Calt, ARM_CC_CC);
            }
        }

        RA_FreeARMRegister(ctx, val1);
        RA_FreeARMRegister(ctx, val2);
    }
    /* CAS */
    else
    {
        uint8_t ext_words = 1;
        uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
        uint8_t ea = -1;
        uint8_t du = RA_MapM68kRegister(ctx, (opcode2 >> 6) & 7);
        uint8_t dc = RA_MapM68kRegister(ctx, opcode2 & 7);
        uint8_t status = RA_AllocARMRegister(ctx);
        RA_SetDirtyM68kRegister(ctx, opcode2 & 7);

        uint8_t size = (opcode >> 9) & 3;
        uint8_t mode = (opcode >> 3) & 7;

        uint8_t tmp = RA_AllocARMRegister(ctx);

        /* Load effective address */
        if (mode == 4 || mode == 3)
            EMIT_LoadFromEffectiveAddress(ctx, 0, &ea, opcode & 0x3f, &ext_words, 0, NULL);
        else
            EMIT_LoadFromEffectiveAddress(ctx, 0, &ea, opcode & 0x3f, &ext_words, 1, NULL);

        if (mode == 4)
        {
            if (size == 2 || (size == 1 && (opcode & 7) == 7))
            {
                EMIT(ctx, sub_immed(ea, ea, 2));
            }
            else if (size == 3)
            {
                EMIT(ctx, sub_immed(ea, ea, 4));
            }
            else
            {
                EMIT(ctx, sub_immed(ea, ea, 1));
            }

            RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
        }

        if (size == 1)
        {
            CAS_ATOMIC();
        }
        else if ((opcode & 0x3f) == 0x38)
        {
            switch(size)
            {
                case 2:
                    if (cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[1]) & 1)
                        CAS_UNSAFE();
                    else
                        CAS_ATOMIC();
                    break;
                case 3:
                    if ((cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[1]) & 3) == 0)
                        CAS_ATOMIC();
                    else
                        CAS_UNSAFE();
                    break;
            }
        }
        else if ((opcode & 0x3f) == 0x39)
        {
            switch(size)
            {
                case 2:
                    if (cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[2]) & 1)
                        CAS_UNSAFE();
                    else
                        CAS_ATOMIC();
                    break;
                case 3:
                    if ((cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[2]) & 3) == 0)
                        CAS_ATOMIC();
                    else
                        CAS_UNSAFE();
                    break;
            }
        }
        else
        {
            uint32_t *b_eq;
            uint32_t *b_;

            switch(size)
            {
                case 3:
                    EMIT(ctx, ands_immed(31, ea, 2, 0));
                    break;
                case 2:
                    EMIT(ctx, ands_immed(31, ea, 1, 0));
                    break;
            }

            b_eq = ctx->tc_CodePtr++;
            CAS_UNSAFE();
            b_ = ctx->tc_CodePtr++;
            CAS_ATOMIC();
            *b_ = b(ctx->tc_CodePtr - b_);
            *b_eq = b_cc(A64_CC_EQ, 1 + b_ - b_eq);
        }

        EMIT(ctx, dmb_ish());

        if (mode == 3)
        {
            if (size == 2 || (size == 1 && (opcode & 7) == 7))
            {
                EMIT(ctx, add_immed(ea, ea, 2));
            }
            else if (size == 3)
            {
                EMIT(ctx, add_immed(ea, ea, 4));
            }
            else
            {
                EMIT(ctx, add_immed(ea, ea, 1));
            }

            RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
        }

        EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
        ctx->tc_M68kCodePtr += ext_words;

        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(ctx);

            if (__builtin_popcount(update_mask) != 0)
            {
                EMIT_GetNZnCV(ctx, cc, &update_mask);
                
                if (update_mask & SR_Z)
                    EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
                if (update_mask & SR_N)
                    EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
                if (update_mask & SR_V)
                    EMIT_SetFlagsConditional(ctx, cc, SR_Valt, ARM_CC_VS);
                if (update_mask & SR_C)
                    EMIT_SetFlagsConditional(ctx, cc, SR_Calt, ARM_CC_CC);
            }
        }

        RA_FreeARMRegister(ctx, ea);
        RA_FreeARMRegister(ctx, status);
        RA_FreeARMRegister(ctx, tmp);
    }

    return 1;
}

uint32_t EMIT_MOVEP(struct TranslatorContext *ctx, uint16_t opcode)
{
    int32_t offset = (int16_t)cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t an = RA_MapM68kRegister(ctx, 8 + (opcode & 7));
    uint8_t dn = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
    uint8_t tmp = RA_AllocARMRegister(ctx);
    uint8_t addr = an;

    /* For offset == 0 just use the m68k register */
    if (offset != 0) {
        /* For all other offsets get a temporary reg for address */
        if (offset > 0) {
            if ((offset & 0xfff) && (offset > 0xfff-8)) {
                if (addr == an) {
                    addr = RA_AllocARMRegister(ctx);
                }
                EMIT(ctx, add_immed(addr, an, offset & 0xfff));
                offset &= 0xf000;
            }
            if (offset & 0x7000) {
                if (addr == an) {
                    addr = RA_AllocARMRegister(ctx);
                    EMIT(ctx, add_immed_lsl12(addr, an, offset >> 12));
                }
                else
                {
                    EMIT(ctx, add_immed_lsl12(addr, addr, offset >> 12));
                }
                offset &= 0xfff;
            }
        }
        else {
            offset = -offset;
            if (offset & 0xfff) {
                if (addr == an) {
                    addr = RA_AllocARMRegister(ctx);
                }
                EMIT(ctx, sub_immed(addr, an, offset & 0xfff));
                offset &= 0xf000;
            }
            if (offset & 0xf000) {
                if (addr == an) {
                    addr = RA_AllocARMRegister(ctx);
                    EMIT(ctx, sub_immed_lsl12(addr, an, offset >> 12));
                }
                else
                {
                    EMIT(ctx, sub_immed_lsl12(addr, addr, offset >> 12));
                }
                offset &= 0xfff;
            }
        }
    }

    /* Register to memory transfer */
    if (opcode & 0x80) {
        /* Long mode */
        if (opcode & 0x40) {
            EMIT(ctx, 
                lsr(tmp, dn, 24),
                strb_offset(addr, tmp, offset),
                lsr(tmp, dn, 16),
                strb_offset(addr, tmp, offset + 2),
                lsr(tmp, dn, 8),
                strb_offset(addr, tmp, offset + 4),
                strb_offset(addr, dn, offset + 6)
            );
        }
        /* Word mode */
        else {
            EMIT(ctx, 
                lsr(tmp, dn, 8),
                strb_offset(addr, tmp, offset),
                strb_offset(addr, dn, offset + 2)
            );
        }
    }
    /* Memory to register transfer */
    else {
        RA_SetDirtyM68kRegister(ctx, (opcode >> 9) & 7);
        
        /* Long mode */
        if (opcode & 0x40) {
            EMIT(ctx, 
                ldrb_offset(addr, dn, offset),
                ldrb_offset(addr, tmp, offset + 2),
                lsl(dn, dn, 24),
                orr_reg(dn, dn, tmp, LSL, 16),
                ldrb_offset(addr, tmp, offset + 4),
                orr_reg(dn, dn, tmp, LSL, 8),
                ldrb_offset(addr, tmp, offset + 6),
                orr_reg(dn, dn, tmp, LSL, 0)
            );
        }
        /* Word mode */
        else {
            EMIT(ctx, 
                bic_immed(dn, dn, 16, 0),
                ldrb_offset(addr, tmp, offset),
                orr_reg(dn, dn, tmp, LSL, 8),
                ldrb_offset(addr, tmp, offset + 2),
                orr_reg(dn, dn, tmp, LSL, 0)
            );
        }
    }

    RA_FreeARMRegister(ctx, addr);
    RA_FreeARMRegister(ctx, tmp);
    EMIT_AdvancePC(ctx, 4);
    
    ctx->tc_M68kCodePtr++;

    return 1;
}

uint32_t EMIT_MOVES(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t cc = RA_GetCC(ctx);
    uint8_t size = (opcode >> 6) & 3;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint32_t *tmp;
    uint32_t *tmp_priv;
    uint8_t ext_count = 1;
    uint8_t reg = RA_MapM68kRegister(ctx, opcode2 >> 12);
    
    size = size == 0 ? 1 : size == 1 ? 2 : 4;

    EMIT_FlushPC(ctx);

    /* Test if supervisor mode is active */
    EMIT(ctx, ands_immed(31, cc, 1, 32 - SRB_S));

    /* Branch to exception if not in supervisor */
    tmp_priv = ctx->tc_CodePtr++;

    // Transfer from Register to EA
    if (opcode2 & (1 << 11)) {

        if (((opcode & 0x38) == 0x18) && (8 + (opcode & 7)) == (opcode2 >> 12))
        {
            uint8_t tmpreg = RA_AllocARMRegister(ctx);

            EMIT(ctx, add_immed(tmpreg, reg, size));

            reg = tmpreg;
        }
        if (((opcode & 0x38) == 0x20) && (8 + (opcode & 7)) == (opcode2 >> 12))
        {
            uint8_t tmpreg = RA_AllocARMRegister(ctx);

            EMIT(ctx, sub_immed(tmpreg, reg, size));

            reg = tmpreg;
        }

        EMIT_StoreToEffectiveAddress(ctx, size, &reg, opcode & 0x3f, &ext_count, 0);
    }
    // Transfer from EA to Register
    else {
        RA_SetDirtyM68kRegister(ctx, opcode2 >> 12);
        if (size == 4)
            EMIT_LoadFromEffectiveAddress(ctx, size, &reg, opcode & 0x3f, &ext_count, 0, NULL);
        else {
            if (opcode2 & 0x8000) {
                if (!(((opcode2 >> 12) & 7) == (opcode & 7) && ((opcode & 0x38) == 0x18 || (opcode & 0x38) == 0x20)))
                    EMIT_LoadFromEffectiveAddress(ctx, 0x80 | size, &reg, opcode & 0x3f, &ext_count, 0, NULL);
                else {
                    uint8_t tmpreg = 0xff;
                    EMIT_LoadFromEffectiveAddress(ctx, 0x80 | size, &tmpreg, opcode & 0x3f, &ext_count, 0, NULL);
                    EMIT(ctx, mov_reg(reg, tmpreg));
                    RA_FreeARMRegister(ctx, tmpreg);
                }
            }
            else {
                uint8_t tmpreg = 0xff;

                EMIT_LoadFromEffectiveAddress(ctx, size, &tmpreg, opcode & 0x3f, &ext_count, 0, NULL);

                switch (size)
                {
                    case 1:
                        EMIT(ctx, bfi(reg, tmpreg, 0, 8));
                        break;
                
                    case 2:
                        EMIT(ctx, bfi(reg, tmpreg, 0, 16));
                        break;
                }

                RA_FreeARMRegister(ctx, tmpreg);
            }
        }
    }

    RA_FreeARMRegister(ctx, reg);

    EMIT(ctx, add_immed(REG_PC, REG_PC, 2 * (ext_count + 1)));

    tmp = ctx->tc_CodePtr++;

    *tmp_priv = b_cc(A64_CC_EQ, ctx->tc_CodePtr - tmp_priv);
    EMIT_Exception(ctx, VECTOR_PRIVILEGE_VIOLATION, 0);

    ctx->tc_M68kCodePtr += ext_count;

    RA_FreeARMRegister(ctx, reg);

    *tmp = b_cc(A64_CC_AL, ctx->tc_CodePtr - tmp);
    EMIT(ctx, 
        (uint32_t)(uintptr_t)tmp,
        1, 0,
        INSN_TO_LE(0xfffffffe)
    );

    return 1;
}

static struct OpcodeDef InsnTable[4096] = {
	[0x03c]			  = { EMIT_ORI_TO_CCR, NULL, SR_CCR, SR_CCR, 2, 0, 1 },
	[0x07c]			  = { EMIT_ORI_TO_SR, NULL, SR_ALL, SR_ALL, 2, 0, 2  },
	[0x23c]			  = { EMIT_ANDI_TO_CCR, NULL, SR_CCR, SR_CCR, 2, 0, 1 },
	[0x27c]			  = { EMIT_ANDI_TO_SR, NULL, SR_ALL, SR_ALL, 2, 0, 2 },
	[0xa3c]			  = { EMIT_EORI_TO_CCR, NULL, SR_CCR, SR_CCR, 2, 0, 1 },
	[0xa7c]			  = { EMIT_EORI_TO_SR, NULL, SR_ALL, SR_ALL, 2, 0, 2 },

	[00000 ... 00007] = { EMIT_ORI, NULL, 0, SR_NZVC, 2, 0, 1 },
	[00020 ... 00047] = { EMIT_ORI, NULL, 0, SR_NZVC, 2, 0, 1 },
	[00050 ... 00071] = { EMIT_ORI, NULL, 0, SR_NZVC, 2, 1, 1 },
	[00100 ... 00107] = { EMIT_ORI, NULL, 0, SR_NZVC, 2, 0, 2 },
	[00120 ... 00147] = { EMIT_ORI, NULL, 0, SR_NZVC, 2, 0, 2 },
	[00150 ... 00171] = { EMIT_ORI, NULL, 0, SR_NZVC, 2, 1, 2 },
	[00200 ... 00207] = { EMIT_ORI, NULL, 0, SR_NZVC, 3, 0, 4 },
	[00220 ... 00247] = { EMIT_ORI, NULL, 0, SR_NZVC, 3, 0, 4 },
	[00250 ... 00271] = { EMIT_ORI, NULL, 0, SR_NZVC, 3, 1, 4 },

	[01000 ... 01007] = { EMIT_ANDI, NULL, 0, SR_NZVC, 2, 0, 1 },
	[01020 ... 01047] = { EMIT_ANDI, NULL, 0, SR_NZVC, 2, 0, 1 },
	[01050 ... 01071] = { EMIT_ANDI, NULL, 0, SR_NZVC, 2, 1, 1 },
	[01100 ... 01107] = { EMIT_ANDI, NULL, 0, SR_NZVC, 2, 0, 2 },
	[01120 ... 01147] = { EMIT_ANDI, NULL, 0, SR_NZVC, 2, 0, 2 },
	[01150 ... 01171] = { EMIT_ANDI, NULL, 0, SR_NZVC, 2, 1, 2 },
	[01200 ... 01207] = { EMIT_ANDI, NULL, 0, SR_NZVC, 3, 0, 4 },
	[01220 ... 01247] = { EMIT_ANDI, NULL, 0, SR_NZVC, 3, 0, 4 },
	[01250 ... 01271] = { EMIT_ANDI, NULL, 0, SR_NZVC, 3, 1, 4 },

	[02000 ... 02007] = { EMIT_SUBI, NULL, 0, SR_CCR, 2, 0, 1 },
	[02020 ... 02047] = { EMIT_SUBI, NULL, 0, SR_CCR, 2, 0, 1 },
	[02050 ... 02071] = { EMIT_SUBI, NULL, 0, SR_CCR, 2, 1, 1 },
	[02100 ... 02107] = { EMIT_SUBI, NULL, 0, SR_CCR, 2, 0, 2 },
	[02120 ... 02147] = { EMIT_SUBI, NULL, 0, SR_CCR, 2, 0, 2 },
	[02150 ... 02171] = { EMIT_SUBI, NULL, 0, SR_CCR, 2, 1, 2 },
	[02200 ... 02207] = { EMIT_SUBI, NULL, 0, SR_CCR, 3, 0, 4 },
	[02220 ... 02247] = { EMIT_SUBI, NULL, 0, SR_CCR, 3, 0, 4 },
	[02250 ... 02271] = { EMIT_SUBI, NULL, 0, SR_CCR, 3, 1, 4 },

	[03000 ... 03007] = { EMIT_ADDI, NULL, 0, SR_CCR, 2, 0, 1 },
	[03020 ... 03047] = { EMIT_ADDI, NULL, 0, SR_CCR, 2, 0, 1 },
	[03050 ... 03071] = { EMIT_ADDI, NULL, 0, SR_CCR, 2, 1, 1 },
	[03100 ... 03107] = { EMIT_ADDI, NULL, 0, SR_CCR, 2, 0, 2 },
	[03120 ... 03147] = { EMIT_ADDI, NULL, 0, SR_CCR, 2, 0, 2 },
	[03150 ... 03171] = { EMIT_ADDI, NULL, 0, SR_CCR, 2, 1, 2 },
	[03200 ... 03207] = { EMIT_ADDI, NULL, 0, SR_CCR, 3, 0, 4 },
	[03220 ... 03247] = { EMIT_ADDI, NULL, 0, SR_CCR, 3, 0, 4 },
	[03250 ... 03271] = { EMIT_ADDI, NULL, 0, SR_CCR, 3, 1, 4 },

	[04000 ... 04007] = { EMIT_BTST, NULL, 0, SR_Z, 2, 0, 4 },
	[04020 ... 04047] = { EMIT_BTST, NULL, 0, SR_Z, 2, 0, 1 },
	[04050 ... 04073] = { EMIT_BTST, NULL, 0, SR_Z, 2, 1, 1 },
	[04100 ... 04107] = { EMIT_BCHG, NULL, 0, SR_Z, 2, 0, 4 },
	[04120 ... 04147] = { EMIT_BCHG, NULL, 0, SR_Z, 2, 1, 1 },
	[04150 ... 04171] = { EMIT_BCHG, NULL, 0, SR_Z, 2, 1, 1 },
	[04200 ... 04207] = { EMIT_BCLR, NULL, 0, SR_Z, 2, 0, 4 },
	[04220 ... 04247] = { EMIT_BCLR, NULL, 0, SR_Z, 2, 0, 1 },
	[04250 ... 04271] = { EMIT_BCLR, NULL, 0, SR_Z, 2, 1, 1 },
	[04300 ... 04307] = { EMIT_BSET, NULL, 0, SR_Z, 2, 0, 4 },
	[04320 ... 04347] = { EMIT_BSET, NULL, 0, SR_Z, 2, 0, 1 },
	[04350 ... 04371] = { EMIT_BSET, NULL, 0, SR_Z, 2, 1, 1 },

	[05000 ... 05007] = { EMIT_EORI, NULL, 0, SR_NZVC, 2, 0, 1 },
	[05020 ... 05047] = { EMIT_EORI, NULL, 0, SR_NZVC, 2, 0, 1 },
	[05050 ... 05071] = { EMIT_EORI, NULL, 0, SR_NZVC, 2, 1, 1 },
	[05100 ... 05107] = { EMIT_EORI, NULL, 0, SR_NZVC, 2, 0, 2 },
	[05120 ... 05147] = { EMIT_EORI, NULL, 0, SR_NZVC, 2, 0, 2 },
	[05150 ... 05171] = { EMIT_EORI, NULL, 0, SR_NZVC, 2, 1, 2 },
	[05200 ... 05207] = { EMIT_EORI, NULL, 0, SR_NZVC, 3, 0, 4 },
	[05220 ... 05247] = { EMIT_EORI, NULL, 0, SR_NZVC, 3, 0, 4 },
	[05250 ... 05271] = { EMIT_EORI, NULL, 0, SR_NZVC, 3, 1, 4 },

	[06000 ... 06007] = { EMIT_CMPI, NULL, 0, SR_NZVC, 2, 0, 1 },
	[06020 ... 06047] = { EMIT_CMPI, NULL, 0, SR_NZVC, 2, 0, 1 },
	[06050 ... 06073] = { EMIT_CMPI, NULL, 0, SR_NZVC, 2, 1, 1 },
	[06100 ... 06107] = { EMIT_CMPI, NULL, 0, SR_NZVC, 2, 0, 2 },
	[06120 ... 06147] = { EMIT_CMPI, NULL, 0, SR_NZVC, 2, 0, 2 },
	[06150 ... 06173] = { EMIT_CMPI, NULL, 0, SR_NZVC, 2, 1, 2 },
	[06200 ... 06207] = { EMIT_CMPI, NULL, 0, SR_NZVC, 3, 0, 4 },
	[06220 ... 06247] = { EMIT_CMPI, NULL, 0, SR_NZVC, 3, 0, 4 },
	[06250 ... 06273] = { EMIT_CMPI, NULL, 0, SR_NZVC, 3, 1, 4 },

	[00400 ... 00407] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 4 },
	[00420 ... 00447] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 1 },
	[00450 ... 00474] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },
	[01400 ... 01407] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 4 },
	[01420 ... 01447] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 1 },
	[01450 ... 01474] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },
	[02400 ... 02407] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 4 },
	[02420 ... 02447] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 1 },
	[02450 ... 02474] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },
	[03400 ... 03407] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 4 },
	[03420 ... 03447] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 1 },
	[03450 ... 03474] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },
	[04400 ... 04407] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 4 },
	[04420 ... 04447] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 1 },
	[04450 ... 04474] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },
	[05400 ... 05407] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 4 },
	[05420 ... 05447] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },
	[05450 ... 05474] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },
	[06400 ... 06407] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 4 },
	[06420 ... 06447] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 1 },
	[06450 ... 06474] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },
	[07400 ... 07407] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 4 },
	[07420 ... 07447] = { EMIT_BTST, NULL, 0, SR_Z, 1, 0, 1 },
	[07450 ... 07474] = { EMIT_BTST, NULL, 0, SR_Z, 1, 1, 1 },

	[00500 ... 00507] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 4 },
	[00520 ... 00547] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 1 },
	[00550 ... 00571] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 1, 1 },
	[01500 ... 01507] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 4 },
	[01520 ... 01547] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 1 },
	[01550 ... 01571] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 1, 1 },
	[02500 ... 02507] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 4 },
	[02520 ... 02547] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 1 },
	[02550 ... 02571] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 1, 1 },
	[03500 ... 03507] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 4 },
	[03520 ... 03547] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 1 },
	[03550 ... 03571] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 1, 1 },
	[04500 ... 04507] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 4 },
	[04520 ... 04547] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 1 },
	[04550 ... 04571] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 1, 1 },
	[05500 ... 05507] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 4 },
	[05520 ... 05547] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 1 },
	[05550 ... 05571] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 1, 1 },
	[06500 ... 06507] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 4 },
	[06520 ... 06547] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 1 },
	[06550 ... 06571] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 1, 1 },
	[07500 ... 07507] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 4 },
	[07520 ... 07547] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 0, 1 },
	[07550 ... 07571] = { EMIT_BCHG, NULL, 0, SR_Z, 1, 1, 1 },

	[00600 ... 00607] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 4 },
	[00620 ... 00647] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 1 },
	[00650 ... 00671] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 1, 1 },
	[01600 ... 01607] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 4 },
	[01620 ... 01647] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 1 },
	[01650 ... 01671] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 1, 1 },
	[02600 ... 02607] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 4 },
	[02620 ... 02647] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 1 },
	[02650 ... 02671] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 1, 1 },
	[03600 ... 03607] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 4 },
	[03620 ... 03647] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 1 },
	[03650 ... 03671] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 1, 1 },
	[04600 ... 04607] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 4 },
	[04620 ... 04647] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 1 },
	[04650 ... 04671] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 1, 1 },
	[05600 ... 05607] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 4 },
	[05620 ... 05647] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 1 },
	[05650 ... 05671] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 1, 1 },
	[06600 ... 06607] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 4 },
	[06620 ... 06647] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 1 },
	[06650 ... 06671] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 1, 1 },
	[07600 ... 07607] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 4 },
	[07620 ... 07647] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 0, 1 },
	[07650 ... 07671] = { EMIT_BCLR, NULL, 0, SR_Z, 1, 1, 1 },

	[00700 ... 00707] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 4 },
	[00720 ... 00747] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 1 },
	[00750 ... 00771] = { EMIT_BSET, NULL, 0, SR_Z, 1, 1, 1 },
	[01700 ... 01707] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 4 },
	[01720 ... 01747] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 1 },
	[01750 ... 01771] = { EMIT_BSET, NULL, 0, SR_Z, 1, 1, 1 },
	[02700 ... 02707] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 4 },
	[02720 ... 02747] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 1 },
	[02750 ... 02771] = { EMIT_BSET, NULL, 0, SR_Z, 1, 1, 1 },
	[03700 ... 03707] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 4 },
	[03720 ... 03747] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 1 },
	[03750 ... 03771] = { EMIT_BSET, NULL, 0, SR_Z, 1, 1, 1 },
	[04700 ... 04707] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 4 },
	[04720 ... 04747] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 1 },
	[04750 ... 04771] = { EMIT_BSET, NULL, 0, SR_Z, 1, 1, 1 },
	[05700 ... 05707] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 4 },
	[05720 ... 05747] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 1 },
	[05750 ... 05771] = { EMIT_BSET, NULL, 0, SR_Z, 1, 1, 1 },
	[06700 ... 06707] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 4 },
	[06720 ... 06747] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 1 },
	[06750 ... 06771] = { EMIT_BSET, NULL, 0, SR_Z, 1, 1, 1 },
	[07700 ... 07707] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 4 },
	[07720 ... 07747] = { EMIT_BSET, NULL, 0, SR_Z, 1, 0, 1 },
	[07750 ... 07771] = { EMIT_BSET, NULL, 0, SR_Z, 1, 1, 1 },

	[05320 ... 05347] = { EMIT_CAS, NULL, 0, SR_NZVC, 2, 0, 1 },
	[05350 ... 05371] = { EMIT_CAS, NULL, 0, SR_NZVC, 2, 1, 1 },
	[06320 ... 06347] = { EMIT_CAS, NULL, 0, SR_NZVC, 2, 0, 2 },
	[06350 ... 06371] = { EMIT_CAS, NULL, 0, SR_NZVC, 2, 1, 2 },
	[07320 ... 07347] = { EMIT_CAS, NULL, 0, SR_NZVC, 2, 0, 4 },
	[07350 ... 07371] = { EMIT_CAS, NULL, 0, SR_NZVC, 2, 1, 4 },

	[0xcfc]			  = { EMIT_CAS2, NULL, 0, SR_NZVC, 3, 0, 2 },
	[0xefc]			  = { EMIT_CAS2, NULL, 0, SR_NZVC, 3, 0, 4 },

	[00320 ... 00327] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 0, 1 },
	[00350 ... 00373] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 1, 1 },
	[01320 ... 01327] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 0, 2 },
	[01350 ... 01373] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 1, 2 },
	[02320 ... 02327] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 0, 4 },
	[02350 ... 02373] = { EMIT_CMP2, NULL, SR_CCR, SR_NZVC, 2, 1, 4 },

	[00410 ... 00417] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[00510 ... 00517] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[00610 ... 00617] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[00710 ... 00717] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[01410 ... 01417] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[01510 ... 01517] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[01610 ... 01617] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[01710 ... 01717] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[02410 ... 02417] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[02510 ... 02517] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[02610 ... 02617] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[02710 ... 02717] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[03410 ... 03417] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[03510 ... 03517] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[03610 ... 03617] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[03710 ... 03717] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[04410 ... 04417] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[04510 ... 04517] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[04610 ... 04617] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[04710 ... 04717] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[05410 ... 05417] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[05510 ... 05517] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[05610 ... 05617] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[05710 ... 05717] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[06410 ... 06417] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[06510 ... 06517] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[06610 ... 06617] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[06710 ... 06717] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[07410 ... 07417] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[07510 ... 07517] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },
	[07610 ... 07617] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 2 },
	[07710 ... 07717] = { EMIT_MOVEP, NULL, 0, 0, 2, 0, 4 },

	[07020 ... 07047] = { EMIT_MOVES, NULL, SR_S, 0, 2, 0, 1 },
	[07050 ... 07071] = { EMIT_MOVES, NULL, SR_S, 0, 2, 1, 1 },
	[07120 ... 07147] = { EMIT_MOVES, NULL, SR_S, 0, 2, 0, 2 },
	[07150 ... 07171] = { EMIT_MOVES, NULL, SR_S, 0, 2, 1, 2 },
	[07220 ... 07247] = { EMIT_MOVES, NULL, SR_S, 0, 2, 0, 4 },
	[07250 ... 07271] = { EMIT_MOVES, NULL, SR_S, 0, 2, 1, 4 },
};

uint32_t EMIT_line0(struct TranslatorContext *ctx)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint32_t insn_consumed = 1;
    ctx->tc_M68kCodePtr++;

    if (InsnTable[opcode & 0xfff].od_Emit) {
        insn_consumed = InsnTable[opcode & 0xfff].od_Emit(ctx, opcode);
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

    return insn_consumed;
}

uint32_t GetSR_Line0(uint16_t opcode)
{
    /* If instruction is in the table, return what flags it needs (shifted 16 bits left) and flags it sets */
    if (InsnTable[opcode & 0xfff].od_Emit) {
        return (InsnTable[opcode & 0xfff].od_SRNeeds << 16) | InsnTable[opcode & 0xfff].od_SRSets;
    }
    /* Instruction not found, i.e. it needs all flags and sets none (ILLEGAL INSTRUCTION exception) */
    else {
        kprintf("Undefined Line0 %04x\n", opcode);
        return SR_CCR << 16;
    }
}

int M68K_GetLine0Length(uint16_t *insn_stream)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)&(*insn_stream));
    
    int length = 0;
    int need_ea = 0;
    int opsize = 0;

    if (InsnTable[opcode & 0xfff].od_Emit) {
        length = InsnTable[opcode & 0xfff].od_BaseLength;
        need_ea = InsnTable[opcode & 0xfff].od_HasEA;
        opsize = InsnTable[opcode & 0xfff].od_OpSize;
    }

    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 077, opsize);
    }

    return length;
}
