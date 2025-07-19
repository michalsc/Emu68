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

extern uint32_t insn_count;

uint32_t EMIT_CLR(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_count = 0;
    uint8_t size = 1;
    uint8_t zero = 0xff;

    zero = 31;

    /* Determine the size of operation */
    switch (opcode & 0x00c0)
    {
        case 0x0000: /* Byte operation */
            size = 1;
            break;
        case 0x0040: /* Short operation */
            size = 2;
            break;
        case 0x0080: /* Long operation */
            size = 4;
            break;
    }

    if ((opcode & 0x38) == 0)
    {
        if (size != 4) {
            uint8_t dn = RA_MapM68kRegister(ctx, opcode & 7);
            RA_SetDirtyM68kRegister(ctx, opcode & 7);
            switch (size) {
                case 1:
                    EMIT(ctx, bic_immed(dn, dn, 8, 0));
                    break;
                case 2:
                    EMIT(ctx, bic_immed(dn, dn, 16, 0));
            }
        }
        else {
            uint8_t dn = RA_MapM68kRegisterForWrite(ctx, opcode & 7);
            EMIT(ctx, mov_reg(dn, 31));
        }
    }
    else
        EMIT_StoreToEffectiveAddress(ctx, size, &zero, opcode & 0x3f, &ext_count, 0);

    EMIT_AdvancePC(ctx, 2 * (ext_count + 1));
    ctx->tc_M68kCodePtr += ext_count;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);
        if (update_mask & ~SR_Z)
        {
            uint8_t alt_flags = update_mask;
            if ((alt_flags & 3) != 0 && (alt_flags & 3) < 3)
                alt_flags ^= 3;
            EMIT_ClearFlags(ctx, cc, alt_flags);
        }
        if (update_mask & SR_Z)
            EMIT_SetFlags(ctx, cc, SR_Z);
    }

    return 1;
}

uint32_t EMIT_NOT(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_count = 0;
    uint8_t dest = 0xff;
    uint8_t size = 0;
    uint8_t test_register = 0xff;

    /* Determine the size of operation */
    switch (opcode & 0x00c0)
    {
        case 0x0000: /* Byte operation */
            size = 1;
            break;
        case 0x0040: /* Short operation */
            size = 2;
            break;
        case 0x0080: /* Long operation */
            size = 4;
            break;
    }

    /* handle clearing D register here */
    if ((opcode & 0x0038) == 0)
    {
        if (size == 4)
        {
            dest = RA_MapM68kRegister(ctx, opcode & 7);
            RA_SetDirtyM68kRegister(ctx, opcode & 7);
            test_register = dest;

            EMIT(ctx, mvn_reg(dest, dest, LSL, 0));
        }
        else
        {
            uint8_t tmp = RA_AllocARMRegister(ctx);

            /* Fetch m68k register for write */
            dest = RA_MapM68kRegister(ctx, opcode & 7);

            /* Mark register dirty */
            RA_SetDirtyM68kRegister(ctx, opcode & 7);

            test_register = dest;

            switch(size)
            {
                case 2:
                    EMIT(ctx, 
                        eor_immed(dest, dest, 16, 0)
                    );
                    break;
                case 1:
                    EMIT(ctx, 
                        eor_immed(dest, dest, 8, 0)
                    );
                    break;
            }

            RA_FreeARMRegister(ctx, tmp);
        }
    }
    else
    {
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t mode = (opcode & 0x0038) >> 3;
        test_register = tmp;

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

                EMIT(ctx, mvn_reg(tmp, tmp, LSL, 0));

                if (mode == 3)
                {
                    EMIT(ctx, str_offset_postindex(dest, tmp, 4));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, str_offset(dest, tmp, 0));
                break;
            case 2:
                if (mode == 4)
                {
                    EMIT(ctx, ldrh_offset_preindex(dest, tmp, -2));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, ldrh_offset(dest, tmp, 0));

                EMIT(ctx, mvn_reg(tmp, tmp, LSL, 0));

                if (mode == 3)
                {
                    EMIT(ctx, strh_offset_postindex(dest, tmp, 2));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, strh_offset(dest, tmp, 0));
                break;
            case 1:
                if (mode == 4)
                {
                    EMIT(ctx, ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, ldrb_offset(dest, tmp, 0));

                EMIT(ctx, mvn_reg(tmp, tmp, LSL, 0));

                if (mode == 3)
                {
                    EMIT(ctx, strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1));
                    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
                }
                else
                    EMIT(ctx, strb_offset(dest, tmp, 0));
                break;
        }
    }

    RA_FreeARMRegister(ctx, dest);

    EMIT_AdvancePC(ctx, 2 * (ext_count + 1));
    ctx->tc_M68kCodePtr += ext_count;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);

        switch(size) {
            case 4:
                EMIT(ctx, cmn_reg(31, test_register, LSL, 0));
                break;
            case 2:
                EMIT(ctx, cmn_reg(31, test_register, LSL, 16));
                break;
            case 1:
                EMIT(ctx, cmn_reg(31, test_register, LSL, 24));
                break;
        }

        EMIT_GetNZ00(ctx, cc, &update_mask);

        if (update_mask & SR_Z)
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
    }

    RA_FreeARMRegister(ctx, test_register);

    return 1;
}

uint32_t EMIT_NEG(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_count = 0;
    uint8_t dest = 0xff;
    uint8_t size = 0;

    /* Determine the size of operation */
    switch (opcode & 0x00c0)
    {
        case 0x0000: /* Byte operation */
            size = 1;
            break;
        case 0x0040: /* Short operation */
            size = 2;
            break;
        case 0x0080: /* Long operation */
            size = 4;
            break;
    }

    /* handle clearing D register here */
    if ((opcode & 0x0038) == 0)
    {
        if (size == 4)
        {
            dest = RA_MapM68kRegister(ctx, opcode & 7);
            RA_SetDirtyM68kRegister(ctx, opcode & 7);

            EMIT(ctx, negs_reg(dest, dest, LSL, 0));
        }
        else
        {
            uint8_t tmp = RA_AllocARMRegister(ctx);

            /* Fetch m68k register for write */
            dest = RA_MapM68kRegister(ctx, opcode & 7);

            /* Mark register dirty */
            RA_SetDirtyM68kRegister(ctx, opcode & 7);

            switch(size)
            {
                case 2:
                    if (update_mask == 0) {
                        EMIT(ctx, 
                            neg_reg(tmp, dest, LSL, 0),
                            bfxil(dest, tmp, 0, 16)
                        );
                    }
                    else {
                        EMIT(ctx, 
                            negs_reg(tmp, dest, LSL, 16),
                            bfxil(dest, tmp, 16, 16)
                        );
                    }
                    break;
                
                case 1:
                    if (update_mask == 0) {
                        EMIT(ctx, 
                            neg_reg(tmp, dest, LSL, 0),
                            bfxil(dest, tmp, 0, 8)
                        );
                    }
                    else {
                        EMIT(ctx, 
                            negs_reg(tmp, dest, LSL, 24),
                            bfxil(dest, tmp, 24, 8)
                        );
                    }
                    break;
            }

            RA_FreeARMRegister(ctx, tmp);
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

            if (update_mask == 0)
                EMIT(ctx, neg_reg(tmp, tmp, LSL, 0));
            else
                EMIT(ctx, negs_reg(tmp, tmp, LSL, 0));

            if (mode == 3)
            {
                EMIT(ctx, str_offset_postindex(dest, tmp, 4));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, str_offset(dest, tmp, 0));
            break;
        case 2:
            if (mode == 4)
            {
                EMIT(ctx, ldrh_offset_preindex(dest, tmp, -2));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, ldrh_offset(dest, tmp, 0));

            if (update_mask == 0) {
                EMIT(ctx, neg_reg(tmp, tmp, LSL, 0));
            }
            else {
                EMIT(ctx, 
                    negs_reg(tmp, tmp, LSL, 16),
                    lsr(tmp, tmp, 16)
                );
            }

            if (mode == 3)
            {
                EMIT(ctx, strh_offset_postindex(dest, tmp, 2));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, strh_offset(dest, tmp, 0));
            break;
        case 1:
            if (mode == 4)
            {
                EMIT(ctx, ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, ldrb_offset(dest, tmp, 0));

            if (update_mask == 0) {
                EMIT(ctx, neg_reg(tmp, tmp, LSL, 0));
            }
            else {
                EMIT(ctx, 
                    negs_reg(tmp, tmp, LSL, 24),
                    lsr(tmp, tmp, 24)
                );
            }

            if (mode == 3)
            {
                EMIT(ctx, strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, strb_offset(dest, tmp, 0));
            break;
        }

        RA_FreeARMRegister(ctx, tmp);
    }

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
                EMIT_SetFlagsConditional(ctx, cc, SR_X, ARM_CC_NE);
            else if ((update_mask & (SR_X | SR_C)) == SR_C)
                EMIT_SetFlagsConditional(ctx, cc, SR_Calt, ARM_CC_NE);
            else
                EMIT_SetFlagsConditional(ctx, cc, SR_Calt | SR_X, ARM_CC_NE);
        }
    }

    return 1;
}

uint32_t EMIT_NEGX(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_count = 0;
    uint8_t dest = 0xff;
    uint8_t size = 0;

    /* Determine the size of operation */
    switch (opcode & 0x00c0)
    {
        case 0x0000: /* Byte operation */
            size = 1;
            break;
        case 0x0040: /* Short operation */
            size = 2;
            break;
        case 0x0080: /* Long operation */
            size = 4;
            break;
    }

    uint8_t cc = RA_GetCC(ctx);
    if (size == 4) {
        uint8_t tmp = RA_AllocARMRegister(ctx);

        EMIT(ctx, 
            mvn_reg(tmp, cc, ROR, 7),
            set_nzcv(tmp)
        );

        RA_FreeARMRegister(ctx, tmp);
    } else {
        EMIT(ctx, tst_immed(cc, 1, 31 & (32 - SRB_X)));
    }

    if ((opcode & 0x0038) == 0)
    {
        if (size == 4)
        {
            dest = RA_MapM68kRegister(ctx, opcode & 7);
            RA_SetDirtyM68kRegister(ctx, opcode & 7);

            EMIT(ctx, ngcs(dest, dest));
        }
        else
        {
            uint8_t tmp = RA_AllocARMRegister(ctx);

            /* Fetch m68k register for write */
            dest = RA_MapM68kRegister(ctx, opcode & 7);

            /* Mark register dirty */
            RA_SetDirtyM68kRegister(ctx, opcode & 7);

            switch(size)
            {
                case 2:
                    EMIT(ctx, 
                        and_immed(tmp, dest, 16, 0),   // Take lower 16 bits of destination
                        neg_reg(tmp, tmp, LSL, 0),     // negate
                        b_cc(A64_CC_EQ, 2),            // Skip if X not set
                        sub_immed(tmp, tmp, 1)
                    );

                    if (update_mask & SR_XVC) {

                        EMIT_ClearFlags(ctx, cc, SR_XVC);

                        uint8_t tmp_2 = RA_AllocARMRegister(ctx);

                        EMIT(ctx, and_reg(tmp_2, tmp, dest, LSL, 0));

                        if (update_mask & SR_V)
                        {
                            EMIT(ctx, 
                                tbz(tmp_2, 15, 2),
                                orr_immed(cc, cc, 1, 31 & (32 - SRB_Valt))
                            );
                        }
                        
                        if ((update_mask & SR_XC) == SR_XC)
                        {
                            EMIT(ctx, 
                                tbz(tmp, 16, 3),
                                orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt)),
                                orr_immed(cc, cc, 1, 31 & (32 - SRB_X))
                            );
                        }
                        else if ((update_mask & SR_XC) == SR_C)
                        {
                            EMIT(ctx, 
                                tbz(tmp, 16, 2),
                                orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt))
                            );
                        }
                        else if ((update_mask & SR_XC) == SR_X)
                        {
                            EMIT(ctx, 
                                tbz(tmp, 16, 2),
                                orr_immed(cc, cc, 1, 31 & (32 - SRB_X))
                            );
                        }


                        update_mask &= ~SR_XVC;             // Don't nag anymore with the flags

                        RA_FreeARMRegister(ctx, tmp_2);
                    }

                    if (update_mask & SR_NZ) {
                        EMIT(ctx, adds_reg(31, 31, tmp, LSL, 16));
                    }

                    EMIT(ctx, bfxil(dest, tmp, 0, 16));       // Insert result
                    break;
                case 1:
                    EMIT(ctx, 
                        and_immed(tmp, dest, 8, 0),    // Take lower 8 bits of destination
                        neg_reg(tmp, tmp, LSL, 0),     // negate
                        b_cc(A64_CC_EQ, 2),            // Skip if X not set
                        sub_immed(tmp, tmp, 1)
                    );

                    if (update_mask & SR_XVC) {
                        EMIT_ClearFlags(ctx, cc, SR_XVC);

                        uint8_t tmp_2 = RA_AllocARMRegister(ctx);

                        EMIT(ctx, and_reg(tmp_2, tmp, dest, LSL, 0));  // C at position 8 in tmp, V at position 7 in tmp2

                        if (update_mask & SR_V)
                        {
                            EMIT(ctx, 
                                tbz(tmp_2, 7, 2),
                                orr_immed(cc, cc, 1, 31 & (32 - SRB_Valt))
                            );
                        }
                        
                        if ((update_mask & SR_XC) == SR_XC)
                        {
                            EMIT(ctx, 
                                tbz(tmp, 8, 3),
                                orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt)),
                                orr_immed(cc, cc, 1, 31 & (32 - SRB_X))
                            );
                        }
                        else if ((update_mask & SR_XC) == SR_C)
                        {
                            EMIT(ctx, 
                                tbz(tmp, 8, 2),
                                orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt))
                            );
                        }
                        else if ((update_mask & SR_XC) == SR_X)
                        {
                            EMIT(ctx, 
                                tbz(tmp, 8, 2),
                                orr_immed(cc, cc, 1, 31 & (32 - SRB_X))
                            );
                        }

                        update_mask &= ~SR_XVC;             // Don't nag anymore with the flags

                        RA_FreeARMRegister(ctx, tmp_2);
                    }

                    if (update_mask & SR_NZ) {
                        EMIT(ctx, adds_reg(31, 31, tmp, LSL, 24));
                    }

                    EMIT(ctx, bfxil(dest, tmp, 0, 8));
                    break;
            }

            RA_FreeARMRegister(ctx, tmp);
        }
    }
    else
    {
        uint8_t src = RA_AllocARMRegister(ctx);
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
                EMIT(ctx, ldr_offset_preindex(dest, src, -4));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, ldr_offset(dest, src, 0));

            EMIT(ctx, ngcs(tmp, src));

            if (mode == 3)
            {
                EMIT(ctx, str_offset_postindex(dest, tmp, 4));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, str_offset(dest, tmp, 0));
            break;
        case 2:
            if (mode == 4)
            {
                EMIT(ctx, ldrh_offset_preindex(dest, src, -2));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, ldrh_offset(dest, src, 0));

            EMIT(ctx, 
                neg_reg(tmp, src, LSL, 0),     // negate
                b_cc(A64_CC_EQ, 2),            // Skip if X not set
                sub_immed(tmp, tmp, 1)
            );

            if (update_mask & SR_XVC) {
                EMIT_ClearFlags(ctx, cc, SR_XVC);
                uint8_t tmp_2 = RA_AllocARMRegister(ctx);

                EMIT(ctx, and_reg(tmp_2, tmp, src, LSL, 0));

                if (update_mask & SR_V)
                {
                    EMIT(ctx, 
                        tbz(tmp_2, 15, 2),
                        orr_immed(cc, cc, 1, 31 & (32 - SRB_Valt))
                    );
                }
                
                if ((update_mask & SR_XC) == SR_XC)
                {
                    EMIT(ctx, 
                        tbz(tmp, 16, 3),
                        orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt)),
                        orr_immed(cc, cc, 1, 31 & (32 - SRB_X))
                    );
                }
                else if ((update_mask & SR_XC) == SR_C)
                {
                    EMIT(ctx, 
                        tbz(tmp, 16, 2),
                        orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt))
                    );
                }
                else if ((update_mask & SR_XC) == SR_X)
                {
                    EMIT(ctx, 
                        tbz(tmp, 16, 2),
                        orr_immed(cc, cc, 1, 31 & (32 - SRB_X))
                    );
                }

                update_mask &= ~SR_XVC;             // Don't nag anymore with the flags

                RA_FreeARMRegister(ctx, tmp_2);
            }

            if (update_mask & SR_NZ) {
                EMIT(ctx, adds_reg(31, 31, tmp, LSL, 16));
            }

            if (mode == 3)
            {
                EMIT(ctx, strh_offset_postindex(dest, tmp, 2));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, strh_offset(dest, tmp, 0));
            break;
        case 1:
            if (mode == 4)
            {
                EMIT(ctx, ldrb_offset_preindex(dest, src, (opcode & 7) == 7 ? -2 : -1));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, ldrb_offset(dest, src, 0));

            EMIT(ctx, 
                neg_reg(tmp, src, LSL, 0),     // negate
                b_cc(A64_CC_EQ, 2),            // Skip if X not set
                sub_immed(tmp, tmp, 1)
            );

            if (update_mask & SR_XVC) {
                EMIT_ClearFlags(ctx, cc, SR_XVC);

                uint8_t tmp_2 = RA_AllocARMRegister(ctx);

                EMIT(ctx, and_reg(tmp_2, tmp, src, LSL, 0));

                if (update_mask & SR_V)
                {
                    EMIT(ctx, 
                        tbz(tmp_2, 7, 2),
                        orr_immed(cc, cc, 1, 31 & (32 - SRB_Valt))
                    );
                }
                
                if ((update_mask & SR_XC) == SR_XC)
                {
                    EMIT(ctx, 
                        tbz(tmp, 8, 3),
                        orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt)),
                        orr_immed(cc, cc, 1, 31 & (32 - SRB_X))
                    );
                }
                else if ((update_mask & SR_XC) == SR_C)
                {
                    EMIT(ctx, 
                        tbz(tmp, 8, 2),
                        orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt))
                    );
                }
                else if ((update_mask & SR_XC) == SR_X)
                {
                    EMIT(ctx, 
                        tbz(tmp, 8, 2),
                        orr_immed(cc, cc, 1, 31 & (32 - SRB_X))
                    );
                }
                update_mask &= ~SR_XVC;             // Don't nag anymore with the flags

                RA_FreeARMRegister(ctx, tmp_2);
            }

            if (update_mask & SR_NZ) {
                EMIT(ctx, adds_reg(31, 31, tmp, LSL, 24));
            }

            if (mode == 3)
            {
                EMIT(ctx, strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1));
                RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
            }
            else
                EMIT(ctx, strb_offset(dest, tmp, 0));
            break;
        }

        RA_FreeARMRegister(ctx, src);
        RA_FreeARMRegister(ctx, tmp);
    }

    RA_FreeARMRegister(ctx, dest);

    EMIT_AdvancePC(ctx, 2 * (ext_count + 1));
    ctx->tc_M68kCodePtr += ext_count;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);
        uint8_t tmp = RA_AllocARMRegister(ctx);

        if (update_mask & SR_Z) {
            EMIT(ctx, 
                b_cc(A64_CC_EQ, 2),
                bic_immed(cc, cc, 1, 31 & (32 - SRB_Z))
            );
            update_mask &= ~SR_Z;
        }

        if (update_mask) {
            uint8_t alt_mask = update_mask;
            if ((alt_mask & 3) != 0 && (alt_mask & 3) < 3)
                alt_mask ^= 3;

            EMIT(ctx, 
                mov_immed_u16(tmp, alt_mask, 0),
                bic_reg(cc, cc, tmp, LSL, 0)
            );
        }

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
        
        RA_FreeARMRegister(ctx, tmp);
    }

    return 1;
}

uint32_t EMIT_TST(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(ctx);
    uint8_t dest = 0xff;
    uint8_t size = 0;

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            size = 4;
            break;
    }

    /* handle adding to register here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register */
        dest = RA_MapM68kRegister(ctx, opcode & 7);

        /* Perform add operation */
        switch (size)
        {
            case 4:
                EMIT(ctx, cmn_reg(31, dest, LSL, 0));
                break;
            case 2:
                EMIT(ctx, cmn_reg(31, dest, LSL, 16));
                break;
            case 1:
                EMIT(ctx, cmn_reg(31, dest, LSL, 24));
                break;
        }
    }
    else
    {
        /* Load effective address */
        EMIT_LoadFromEffectiveAddress(ctx, size, &dest, opcode & 0x3f, &ext_count, 1, NULL);

        /* Fetch data into temporary register, perform add, store it back */
        switch (size)
        {
            case 4:
                EMIT(ctx, cmn_reg(31, dest, LSL, 0));
                break;
            case 2:
                EMIT(ctx, cmn_reg(31, dest, LSL, 16));
                break;
            case 1:
                EMIT(ctx, cmn_reg(31, dest, LSL, 24));
                break;
        }
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

uint32_t EMIT_TAS(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_count = 0;
    uint8_t dest = 0xff;
    uint8_t mode = (opcode & 0x0038) >> 3;
    uint8_t tmpreg = RA_AllocARMRegister(ctx);
    uint8_t tmpresult = RA_AllocARMRegister(ctx);
    uint8_t tmpstate = RA_AllocARMRegister(ctx);

    /* handle TAS on register, just make a copy and then orr 0x80 on register */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register */
        dest = RA_MapM68kRegister(ctx, opcode & 7);
        RA_SetDirtyM68kRegister(ctx, opcode & 7);

        EMIT(ctx, 
            mov_reg(tmpresult, dest),
            orr_immed(dest, dest, 1, 25)
        );
    }
    else
    {
        /* Load effective address */
        if (mode == 4 || mode == 3)
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 0, NULL);
        else
            EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_count, 1, NULL);

        if (mode == 4)
        {
            EMIT(ctx, sub_immed(dest, dest, (opcode & 7) == 7 ? 2 : 1));
            RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
        }

        EMIT(ctx, 
            ldxrb(dest, tmpresult),
            orr_immed(tmpreg, tmpresult, 1, 25),
            stxrb(dest, tmpreg, tmpstate),
            cmp_reg(31, tmpstate, LSL, 0),
            b_cc(A64_CC_NE, -4)
        );

        if (mode == 3)
        {
            EMIT(ctx, add_immed(dest, dest, (opcode & 7) == 7 ? 2 : 1));
            RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
        }
    }

    RA_FreeARMRegister(ctx, dest);
    RA_FreeARMRegister(ctx, tmpreg);
    RA_FreeARMRegister(ctx, tmpstate);

    EMIT_AdvancePC(ctx, 2 * (ext_count + 1));
    ctx->tc_M68kCodePtr += ext_count;

    if (update_mask)
    {
        EMIT(ctx, cmn_reg(31, tmpresult, LSL, 24));
        uint8_t cc = RA_ModifyCC(ctx);
        EMIT_GetNZ00(ctx, cc, &update_mask);

        if (update_mask & SR_Z)
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
    }

    RA_FreeARMRegister(ctx, tmpresult);

    return 1;
}

static uint32_t EMIT_MOVEfromSR(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t cc = RA_ModifyCC(ctx);
    uint8_t ext_words = 0;
    uint32_t *tmpptr;

    EMIT_FlushPC(ctx);

    /* Test if supervisor mode is active */
    EMIT(ctx, ands_immed(31, cc, 1, 32 - SRB_S));
    tmpptr = ctx->tc_CodePtr++;

    uint8_t tmp_cc = RA_AllocARMRegister(ctx);

    EMIT(ctx, 
        mov_reg(tmp_cc, cc),
        rbit(0, cc),
        bfxil(tmp_cc, 0, 30, 2)
    );

    EMIT_StoreToEffectiveAddress(ctx, 2, &tmp_cc, opcode & 0x3f, &ext_words, 0);

    RA_FreeARMRegister(ctx, tmp_cc);

    *tmpptr = b_cc(A64_CC_EQ, 2 + ctx->tc_CodePtr - tmpptr);

    EMIT(ctx, add_immed(REG_PC, REG_PC, 2 * (ext_words + 1)));
    tmpptr = ctx->tc_CodePtr++;

    /* No supervisor. Update USP, generate exception */
    EMIT_Exception(ctx, VECTOR_PRIVILEGE_VIOLATION, 0);
    
    *tmpptr = b_cc(A64_CC_AL, ctx->tc_CodePtr - tmpptr);
    EMIT(ctx, 
        (uint32_t)(uintptr_t)tmpptr,
        1, 0,
        INSN_TO_LE(0xfffffffe)
    );

    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_MOVEfromCCR(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t cc = RA_GetCC(ctx);
    uint8_t ext_words = 0;

    if (opcode & 0x38)
    {
        uint8_t tmp = RA_AllocARMRegister(ctx);

        EMIT(ctx, 
            mov_reg(tmp, cc),
            rbit(0, cc),
            bic_immed(tmp, tmp, 11, 27),
            bfxil(tmp, 0, 30, 2)
        );

        EMIT_StoreToEffectiveAddress(ctx, 2, &tmp, opcode & 0x3f, &ext_words, 0);

        RA_FreeARMRegister(ctx, tmp);
    }
    else
    {
        /* Fetch m68k register */
        uint8_t dest = RA_MapM68kRegister(ctx, opcode & 7);

        /* Mark register dirty */
        RA_SetDirtyM68kRegister(ctx, opcode & 7);

        EMIT(ctx, 
            bfi(dest, cc, 0, 5),
            rbit(0, cc),
            bic_immed(dest, dest, 11, 27),
            bfxil(dest, 0, 30, 2)
        );
    }

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_MOVEtoSR(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t cc = RA_ModifyCC(ctx);
    uint8_t ext_words = 0;
    uint8_t src = 0xff;
    uint8_t orig = 1; //RA_AllocARMRegister(ctx);
    uint8_t changed = 2; //RA_AllocARMRegister(ctx);
    uint8_t sp = RA_MapM68kRegister(ctx, 15);
    uint32_t *tmpptr;

    RA_SetDirtyM68kRegister(ctx, 15);

    EMIT_FlushPC(ctx);

    /* If supervisor is not active, put an exception here */
    tmpptr = ctx->tc_CodePtr++;
    EMIT_Exception(ctx, VECTOR_PRIVILEGE_VIOLATION, 0);

    *tmpptr = tbnz(cc, SRB_S, 1 + ctx->tc_CodePtr - tmpptr);
    tmpptr = ctx->tc_CodePtr++;

    EMIT(ctx, 
        mov_reg(orig, cc),
        mov_immed_u16(changed, 0xf71f, 0)
    );
    
    EMIT_LoadFromEffectiveAddress(ctx, 2, &src, opcode & 0x3f, &ext_words, 1, NULL);

    if ((opcode & 0x38) == 0) /* Dn direct into SR */
    {
        uint8_t src_mod = RA_AllocARMRegister(ctx);
        EMIT(ctx, mov_reg(src_mod, src));
        src = src_mod;
    }

    EMIT(ctx, 
        rbit(0, src),
        bfxil(src, 0, 30, 2)
    );

    cc = RA_ModifyCC(ctx);

    EMIT(ctx, 
        and_reg(cc, changed, src, LSL, 0),
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

        /* Advance PC */
        add_immed(REG_PC, REG_PC, 2 * (ext_words + 1)),

        // Check if IPL is less than 6. If yes, enable ARM interrupts
        and_immed(changed, cc, 3, 32 - SRB_IPL),
        cmp_immed(changed, 5 << SRB_IPL),
        b_cc(A64_CC_GT, 3),
        msr_imm(3, 7, 7), // Enable interrupts
        b(2),
        msr_imm(3, 6, 7)  // Mask interrupts
    );

    *tmpptr = b(ctx->tc_CodePtr - tmpptr);

    EMIT(ctx, INSN_TO_LE(0xffffffff));

    RA_FreeARMRegister(ctx, src);
    RA_FreeARMRegister(ctx, orig);
    RA_FreeARMRegister(ctx, changed);

    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_MOVEtoCCR(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t ext_words = 0;
    uint8_t src = 0xff;
    uint8_t cc = RA_ModifyCC(ctx);

    /* Invalidate host flags */
    host_flags = 0;

    EMIT_LoadFromEffectiveAddress(ctx, 2, &src, opcode & 0x3f, &ext_words, 1, NULL);
    
    if ((opcode & 0x38) == 0) /* Dn direct */
    {
        uint8_t src_mod = RA_AllocARMRegister(ctx);
        EMIT(ctx, mov_reg(src_mod, src));
        src = src_mod;
    }

    EMIT(ctx, 
        rbit(0, src),
        bfxil(src, 0, 30, 2),
        bfi(cc, src, 0, 5)
    );

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));

    RA_FreeARMRegister(ctx, src);
    
    ctx->tc_M68kCodePtr += ext_words;

    return 1;
}

static uint32_t EMIT_EXT(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask;
    uint32_t insn_consumed = 1;
    uint8_t reg = RA_MapM68kRegister(ctx, opcode & 7);
    uint8_t tmp = reg;
    RA_SetDirtyM68kRegister(ctx, opcode & 7);
    uint8_t mode = (opcode >> 6) & 7;

    /* 
        If current instruction is byte-to-word and subsequent is word-to-long on the same register,
        then combine both to extb.l 
    */

    if ((mode == 2) && (opcode ^ cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr)) == 0x40) {
        ctx->tc_M68kCodePtr++;
        mode = 7;
        insn_consumed++;
        EMIT_AdvancePC(ctx, 2);
    } 

    /* Get update mask at this point first, otherwise the mask would suggest that flag change is not necessary */
    update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);

    switch (mode)
    {
        case 2: /* Byte to Word */
            tmp = RA_AllocARMRegister(ctx);
            EMIT(ctx, 
                sxtb(tmp, reg),
                bfi(reg, tmp, 0, 16)
            );
            break;
        case 3: /* Word to Long */
            EMIT(ctx, sxth(reg, reg));
            break;
        case 7: /* Byte to Long */
            EMIT(ctx, sxtb(reg, reg));
            break;
    }

    EMIT_AdvancePC(ctx, 2);

    if (update_mask)
    {
        EMIT(ctx, cmp_reg(tmp, 31, LSL, 0));

        uint8_t cc = RA_ModifyCC(ctx);
        EMIT_GetNZ00(ctx, cc, &update_mask);

        if (update_mask & SR_Z)
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
    }
    if (tmp != reg)
        RA_FreeARMRegister(ctx, tmp);

    return insn_consumed;
}

static uint32_t EMIT_LINK32(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t sp;
    uint8_t displ;
    uint8_t reg;
    int32_t offset = (cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[0]) << 16) | cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[1]);

    displ = RA_AllocARMRegister(ctx);

    EMIT_LoadImmediate(ctx, displ, offset);

    sp = RA_MapM68kRegister(ctx, 15);
    if (8 + (opcode & 7) == 15) {
        reg = RA_CopyFromM68kRegister(ctx, 8 + (opcode & 7));
        EMIT(ctx, sub_immed(reg, reg, 4));
    }
    else {
        reg = RA_MapM68kRegister(ctx, 8 + (opcode & 7));
    }
    EMIT(ctx, 
        str_offset_preindex(sp, reg, -4),  /* SP = SP - 4; An -> (SP) */
        mov_reg(reg, sp)
    );
    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));

    EMIT(ctx, add_reg(sp, sp, displ, LSL, 0));

    RA_SetDirtyM68kRegister(ctx, 15);

    ctx->tc_M68kCodePtr+=2;

    EMIT_AdvancePC(ctx, 6);
    RA_FreeARMRegister(ctx, displ);
    RA_FreeARMRegister(ctx, reg);
    
    return 1;
}

static uint32_t EMIT_LINK16(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t sp;
    uint8_t displ;
    uint8_t reg;
    int16_t offset = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);

    displ = RA_AllocARMRegister(ctx);

    sp = RA_MapM68kRegister(ctx, 15);
    if (8 + (opcode & 7) == 15) {
        reg = RA_CopyFromM68kRegister(ctx, 8 + (opcode & 7));
        EMIT(ctx, sub_immed(reg, reg, 4));
    }
    else {
        reg = RA_MapM68kRegister(ctx, 8 + (opcode & 7));
    }
    EMIT(ctx, 
        str_offset_preindex(sp, reg, -4),  /* SP = SP - 4; An -> (SP) */
        mov_reg(reg, sp)
    );
    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));

    if (offset > 0 && offset < 4096)
    {
        EMIT(ctx, add_immed(sp, sp, offset));
    }
    else if (offset < 0 && offset > -4096)
    {
        EMIT(ctx, sub_immed(sp, sp, -offset));
    }
    else if (offset != 0)
    {
        EMIT_LoadImmediate(ctx, displ, offset);
        EMIT(ctx, add_reg(sp, sp, displ, LSL, 0));
    }
    
    RA_SetDirtyM68kRegister(ctx, 15);

    ctx->tc_M68kCodePtr++;

    EMIT_AdvancePC(ctx, 4);
    RA_FreeARMRegister(ctx, displ);
    RA_FreeARMRegister(ctx, reg);

    return 1;
}

static uint32_t EMIT_SWAP(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t reg = RA_MapM68kRegister(ctx, opcode & 7);
    RA_SetDirtyM68kRegister(ctx, opcode & 7);

    EMIT(ctx, ror(reg, reg, 16));

    EMIT_AdvancePC(ctx, 2);

    if (update_mask)
    {
        EMIT(ctx, cmn_reg(31, reg, LSL, 0));
        uint8_t cc = RA_ModifyCC(ctx);
        EMIT_GetNZ00(ctx, cc, &update_mask);

        if (update_mask & SR_Z)
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
    }

    return 1;
}

static uint32_t EMIT_ILLEGAL(struct TranslatorContext *ctx, uint16_t opcode)
{
    (void)opcode;

    /* Illegal generates exception. Always */
    EMIT_FlushPC(ctx);
    EMIT_Exception(ctx, VECTOR_ILLEGAL_INSTRUCTION, 0);
    EMIT(ctx, INSN_TO_LE(0xffffffff));

    return 1;
}

static uint32_t EMIT_BKPT(struct TranslatorContext *ctx, uint16_t opcode)
{
    (void)opcode;

    /* Breakpoint generates illegal exception, but also advances PC */
    EMIT_AdvancePC(ctx, 2);
    EMIT_FlushPC(ctx);
    EMIT_Exception(ctx, VECTOR_ILLEGAL_INSTRUCTION, 0);
    EMIT(ctx, INSN_TO_LE(0xffffffff));

    return 1;
}

static uint32_t EMIT_TRAP(struct TranslatorContext *ctx, uint16_t opcode)
{
    EMIT_AdvancePC(ctx, 2);
    EMIT_FlushPC(ctx);

    EMIT_Exception(ctx, VECTOR_INT_TRAP(opcode & 15), 0);

    EMIT(ctx, INSN_TO_LE(0xffffffff));

    return 1;
}

static uint32_t EMIT_UNLK(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t sp;
    uint8_t reg;

    sp = RA_MapM68kRegisterForWrite(ctx, 15);
    reg = RA_MapM68kRegister(ctx, 8 + (opcode & 7));

    EMIT(ctx, 
        mov_reg(sp, reg),
        ldr_offset_postindex(sp, reg, 4)
    );

    RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
    RA_SetDirtyM68kRegister(ctx, 15);

    EMIT_AdvancePC(ctx, 2);

    return 1;
}

static uint32_t EMIT_RESET(struct TranslatorContext *ctx, uint16_t opcode)
{
    (void)opcode;
 
    uint32_t *tmp;
    uint32_t *tmp2;
    EMIT_FlushPC(ctx);
    uint8_t cc = RA_ModifyCC(ctx);

    EMIT(ctx, 
        /* Test if supervisor mode is active */
        ands_immed(31, cc, 1, 32 - SRB_S),

        /* Branch to exception if not in supervisor */
        b_cc(A64_CC_EQ, 3),
        add_immed(REG_PC, REG_PC, 2)
    );
    tmp = ctx->tc_CodePtr++;

    /* No supervisor. Update USP, generate exception */
    EMIT_Exception(ctx, VECTOR_PRIVILEGE_VIOLATION, 0);

    tmp2 = ctx->tc_CodePtr++;

    *tmp = b_cc(A64_CC_AL, ctx->tc_CodePtr - tmp);

#ifdef PISTORM_ANY_MODEL

    void do_reset();

    union {
        uint64_t u64;
        uint16_t u16[4];
    } u;

    u.u64 = (uintptr_t)do_reset;

    EMIT(ctx, stp64_preindex(31, 0, 1, -256));
    for (int i=2; i < 30; i += 2)
        EMIT(ctx, stp64(31, i, i+1, i*8));
    EMIT(ctx, str64_offset(31, 30, 240));

    EMIT(ctx, 
        mov64_immed_u16(1, u.u16[3], 0),
        movk64_immed_u16(1, u.u16[2], 1),
        movk64_immed_u16(1, u.u16[1], 2),
        movk64_immed_u16(1, u.u16[0], 3),

        blr(1)
    );

    for (int i=2; i < 30; i += 2)
        EMIT(ctx, ldp64(31, i, i+1, i*8));
    EMIT(ctx, 
        ldr64_offset(31, 30, 240),
        ldp64_postindex(31, 0, 1, 256)
    );

#endif

    *tmp2 = b_cc(A64_CC_AL, ctx->tc_CodePtr - tmp2);

    EMIT(ctx, 
        (uint32_t)(uintptr_t)tmp2,
        1, 0,
        INSN_TO_LE(0xfffffffe),
        INSN_TO_LE(0xffffffff)
    );

    return 1;
}

static uint32_t EMIT_NOP(struct TranslatorContext *ctx, uint16_t opcode)
{
    (void)opcode;

    EMIT(ctx, dsb_sy());
    
    EMIT_AdvancePC(ctx, 2);
    EMIT_FlushPC(ctx);

    return 1;
}

static uint32_t EMIT_STOP(struct TranslatorContext *ctx, uint16_t opcode)
{
    (void)opcode;

    uint32_t *tmpptr;
    uint16_t new_sr = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr) & 0xf71f;
    uint8_t changed = RA_AllocARMRegister(ctx);
    uint8_t orig = RA_AllocARMRegister(ctx);
    uint8_t cc = RA_ModifyCC(ctx);
    uint8_t sp = RA_MapM68kRegister(ctx, 15);

    RA_SetDirtyM68kRegister(ctx, 15);

    EMIT_FlushPC(ctx);

    /* Swap C and V in new SR */
    if ((new_sr & 3) != 0 && (new_sr & 3) < 3)
    {
        new_sr ^= 3;
    }

    /* If supervisor is not active, put an exception here */
    tmpptr = ctx->tc_CodePtr++;
    EMIT_Exception(ctx, VECTOR_PRIVILEGE_VIOLATION, 0);
    *tmpptr = tbnz(cc, SRB_S, 1 + ctx->tc_CodePtr - tmpptr);
    tmpptr = ctx->tc_CodePtr++;

    cc = RA_ModifyCC(ctx);

    EMIT(ctx, 
        /* Put new value into SR, check what has changed */
        mov_reg(orig, cc),
        mov_immed_u16(cc, new_sr, 0),
        eor_reg(changed, orig, cc, LSL, 0),

        /* Perform eventual stack switch */

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

        /* Now do what stop does - wait for interrupt */
        add_immed(REG_PC, REG_PC, 4),

        // Check if IPL is less than 6. If yes, enable ARM interrupts
        and_immed(changed, cc, 3, 32 - SRB_IPL),
        cmp_immed(changed, 5 << SRB_IPL),
        b_cc(A64_CC_GT, 3),
        msr_imm(3, 7, 7), // Enable interrupts
        b(2),
        msr_imm(3, 6, 7)  // Mask interrupts
    );

#ifndef PISTORM_ANY_MODEL
    /* Non pistorm machines wait for interrupt only */
    EMIT(ctx, wfi());
#else
    uint8_t tmpreg = RA_AllocARMRegister(ctx);
    uint8_t ctxReg = RA_GetCTX(ctx);
    uint32_t *start, *end;

    // Don't wait for event if IRQ is already pending
    EMIT(ctx, 
        ldr_offset(ctxReg, tmpreg, __builtin_offsetof(struct M68KState, INT)),
        cbnz(tmpreg, 4)
    );

    start = ctx->tc_CodePtr;

    /* PiStorm waits for event and checks INT - aggregate of ~IPL0 and ARM */
    EMIT(ctx, 
        wfe(),
        ldr_offset(ctxReg, tmpreg, __builtin_offsetof(struct M68KState, INT))
    );
    end = ctx->tc_CodePtr;
    EMIT(ctx, cbz(tmpreg, start - end));

    RA_FreeARMRegister(ctx, tmpreg);
#endif

    *tmpptr = b(ctx->tc_CodePtr - tmpptr);

    EMIT(ctx, INSN_TO_LE(0xffffffff));

    RA_FreeARMRegister(ctx, changed);
    RA_FreeARMRegister(ctx, orig);

    ctx->tc_M68kCodePtr += 1;

    return 1;
}

static uint32_t EMIT_RTE(struct TranslatorContext *ctx, uint16_t opcode)
{
    (void)opcode;

    uint8_t tmp = 1; //RA_AllocARMRegister(ctx);
    uint8_t sp = RA_MapM68kRegister(ctx, 15);
    uint8_t cc = RA_ModifyCC(ctx);
    uint8_t changed = 2; //RA_AllocARMRegister(ctx);
    uint8_t orig = 3; //RA_AllocARMRegister(ctx);
    uint32_t *tmpptr;
    uint32_t *branch_privilege;
    uint32_t *branch_format;

    RA_SetDirtyM68kRegister(ctx, 15);

    EMIT_FlushPC(ctx);

    // First check if supervisor mode
    /* If supervisor is not active, put an exception here */
    branch_privilege = ctx->tc_CodePtr++;
    EMIT_Exception(ctx, VECTOR_PRIVILEGE_VIOLATION, 0);
    *branch_privilege = tbnz(cc, SRB_S, 1 + ctx->tc_CodePtr - branch_privilege);
    branch_privilege = ctx->tc_CodePtr++;

    // Now check frame format
    EMIT(ctx, 
        ldrh_offset(sp, tmp, 6),
        lsr(tmp, tmp, 12)
    );

    // Is format valid?
    EMIT(ctx, cmp_immed(tmp, 2));
    tmpptr = ctx->tc_CodePtr;
    // Forat 2 is supported, go further
    EMIT(ctx, 
        b_cc(A64_CC_EQ, 0),
        cmp_immed(tmp, 0),
        b_cc(A64_CC_EQ, 0)
    );

    EMIT_Exception(ctx, VECTOR_FORMAT_ERROR, 0);

    branch_format = ctx->tc_CodePtr++;

    // Patch both jumps to here
    *tmpptr = b_cc(A64_CC_EQ, ctx->tc_CodePtr - tmpptr);
    tmpptr += 2;
    *tmpptr = b_cc(A64_CC_EQ, ctx->tc_CodePtr - tmpptr);

    EMIT(ctx, 
        /* Fetch sr from stack */
        ldrh_offset_postindex(sp, changed, 2),
        /* Reverse C and V */
        rbit(orig, changed),
        bfxil(changed, orig, 30, 2),
        /* Fetch PC from stack, advance sp so that format word is skipped */
        ldr_offset_postindex(sp, REG_PC, 6),

        /* In case of format 2, skip subsequent longword on stack */
        cmp_immed(tmp, 2),
        b_cc(A64_CC_NE, 2),
        add_immed(sp, sp, 4)
    );

    cc = RA_ModifyCC(ctx);

    EMIT(ctx, 
        /* Use two EORs to generate changed mask and update SR */
        mov_reg(orig, cc),
        eor_reg(changed, changed, cc, LSL, 0),
        eor_reg(cc, changed, cc, LSL, 0),

        /* Now since stack is cleaned up, perform eventual stack switch */
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

        // Check if IPL is less than 6. If yes, enable ARM interrupts
        and_immed(changed, cc, 3, 32 - SRB_IPL),
        cmp_immed(changed, 5 << SRB_IPL),
        b_cc(A64_CC_GT, 3),
        msr_imm(3, 7, 7), // Enable interrupts
        b(2),
        msr_imm(3, 6, 7)  // Mask interrupts
    );

    *branch_privilege = b(ctx->tc_CodePtr - branch_privilege);
    *branch_format = b(ctx->tc_CodePtr - branch_format);

    // Instruction always breaks translation
    EMIT(ctx, INSN_TO_LE(0xffffffff));

    RA_FreeARMRegister(ctx, tmp);
    RA_FreeARMRegister(ctx, changed);
    RA_FreeARMRegister(ctx, orig);

    return 1;
}

static uint32_t EMIT_RTD(struct TranslatorContext *ctx, uint16_t opcode)
{
    (void)opcode;

    uint8_t tmp = RA_AllocARMRegister(ctx);
    uint8_t tmp2 = RA_AllocARMRegister(ctx);
    uint8_t sp = RA_MapM68kRegister(ctx, 15);
    int16_t addend = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);

    /* Fetch return address from stack */
    EMIT(ctx, ldr_offset_postindex(sp, tmp2, 4));

    if (addend > -4096 && addend < 4096)
    {
        if (addend < 0)
        {
            EMIT(ctx, sub_immed(sp, sp, -addend));
        }
        else
        {
            EMIT(ctx, add_immed(sp, sp, addend));
        }
    }
    else
    {
        if (addend < 0)
        {
            EMIT(ctx, movn_immed_u16(tmp, -addend - 1, 0));
        }
        else
        {
            EMIT(ctx, mov_immed_u16(tmp, addend, 0));
        }
        EMIT(ctx, add_reg(sp, sp, tmp, LSL, 0));
    }

    EMIT_ResetOffsetPC(ctx);
    EMIT(ctx, mov_reg(REG_PC, tmp2));
    RA_SetDirtyM68kRegister(ctx, 15);
    EMIT(ctx, INSN_TO_LE(0xffffffff));
    RA_FreeARMRegister(ctx, tmp);
    RA_FreeARMRegister(ctx, tmp2);

    return 1;
}

static uint32_t EMIT_RTS(struct TranslatorContext *ctx, uint16_t opcode)
{
    (void)opcode;

    uint8_t sp = RA_MapM68kRegister(ctx, 15);

    /* Fetch return address from stack */
    EMIT(ctx, ldr_offset_postindex(sp, REG_PC, 4));
    EMIT_ResetOffsetPC(ctx);
    RA_SetDirtyM68kRegister(ctx, 15);

    /*
        If Return Stack is not empty, the branch was inlined. Pop instruction counter here,
        and go back to inlining the code
    */
    uint16_t *ret_addr = M68K_PopReturnAddress(NULL);
    if (ret_addr != (uint16_t *)0xffffffff)
    {
        /* 
            If return stack is used, make sure that the code below is at the address we were expecting
            This must not be the case - it would be sufficient if code has modified the return address on the stack
        */
        uint8_t reg = RA_AllocARMRegister(ctx);
        uint32_t *tmp;
        uint32_t ret = (uint32_t)(uintptr_t)ret_addr;
        EMIT_LoadImmediate(ctx, reg, ret);
        EMIT(ctx, 
            cmp_reg(reg, REG_PC, LSL, 0)
        );
        tmp = ctx->tc_CodePtr++;

        ctx->tc_M68kCodePtr = ret_addr;

        RA_FreeARMRegister(ctx, reg);

        *tmp = b_cc(ARM_CC_EQ, ctx->tc_CodePtr - tmp);
        EMIT(ctx, 
            (uint32_t)(uintptr_t)tmp,
            1, 0,
            INSN_TO_LE(0xfffffffe)
        );
    }
    else
        EMIT(ctx, INSN_TO_LE(0xffffffff));

    return 1;
}

static uint32_t EMIT_TRAPV(struct TranslatorContext *ctx, uint16_t opcode)
{
    (void)opcode;

    uint8_t cc = RA_GetCC(ctx);
    uint32_t *tmpptr;
    EMIT_AdvancePC(ctx, 2);
    EMIT_FlushPC(ctx);

    EMIT(ctx, ands_immed(31, cc, 1, 32 - SRB_Valt));
    tmpptr = ctx->tc_CodePtr++;
    
    EMIT_Exception(ctx, VECTOR_TRAPcc, 2, (uint32_t)(uintptr_t)(ctx->tc_M68kCodePtr - 1));

    *tmpptr = b_cc(A64_CC_EQ, ctx->tc_CodePtr - tmpptr);
    EMIT(ctx, 
        (uint32_t)(uintptr_t)tmpptr,
        1, 0, 
        INSN_TO_LE(0xfffffffe));

    return 1;
}

static uint32_t EMIT_RTR(struct TranslatorContext *ctx, uint16_t opcode)
{
    (void)opcode;

    uint8_t tmp = RA_AllocARMRegister(ctx);
    uint8_t mask = RA_AllocARMRegister(ctx);
    uint8_t sp = RA_MapM68kRegister(ctx, 15);

    EMIT(ctx, 
        /* Fetch status byte from stack */
        ldrh_offset_postindex(sp, tmp, 2),
        /* Reverse C and V */
        rbit(0, tmp),
        bfxil(tmp, 0, 30, 2)
    );
    /* Insert XNZCV into SR */
    uint8_t cc = RA_ModifyCC(ctx);
    EMIT(ctx, bfi(cc, tmp, 0, 5));

    host_flags = 0;
    
    /* Fetch return address from stack */
    EMIT(ctx, ldr_offset_postindex(sp, REG_PC, 4));
    EMIT_ResetOffsetPC(ctx);
    RA_SetDirtyM68kRegister(ctx, 15);
    EMIT(ctx, INSN_TO_LE(0xffffffff));
    RA_FreeARMRegister(ctx, mask);
    RA_FreeARMRegister(ctx, tmp);

    return 1;
}

static uint32_t EMIT_MOVEC(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t dr = opcode & 1;
    uint8_t reg = RA_MapM68kRegister(ctx, opcode2 >> 12);
    uint8_t ctxreg = RA_GetCTX(ctx);
    uint8_t cc = RA_ModifyCC(ctx);
    uint8_t tmp = 0xff;
    uint8_t sp = 0xff;
    uint32_t *tmpptr;
    int illegal = 0;
    extern uint32_t debug_range_min;
    extern uint32_t debug_range_max;
    extern int disasm;
    extern int debug;
    union {
        uint16_t u16[4];
        uint64_t u64;
    } u;

    ctx->tc_M68kCodePtr += 1;
    EMIT_FlushPC(ctx);

    /* If supervisor is not active, put an exception here */
    tmpptr = ctx->tc_CodePtr++;
    EMIT_Exception(ctx, VECTOR_PRIVILEGE_VIOLATION, 0);
    *tmpptr = tbnz(cc, SRB_S, 1 + ctx->tc_CodePtr - tmpptr);
    tmpptr = ctx->tc_CodePtr++;

    if (dr)
    {
        switch (opcode2 & 0xfff)
        {
            case 0x000: // SFC
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    and_immed(tmp, reg, 3, 0),
                    strb_offset(ctxreg, tmp, __builtin_offsetof(struct M68KState, SFC))
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x001: // DFC
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    and_immed(tmp, reg, 3, 0),
                    strb_offset(ctxreg, tmp, __builtin_offsetof(struct M68KState, DFC))
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x800: // USP
                EMIT(ctx, mov_reg_to_simd(REG_USP, reg));
                break;
            case 0x801: // VBR
                EMIT(ctx, str_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, VBR)));
                break;
            case 0x002: // CACR
                tmp = RA_AllocARMRegister(ctx);
                uint32_t mask = number_to_mask(0x80008000);
                EMIT(ctx, 
                    and_immed(tmp, reg, (mask >> 16) & 0x3f, mask & 0x3f),
                    mov_reg_to_simd(REG_CACR, tmp)
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x803: // MSP
                sp = RA_MapM68kRegister(ctx, 15);
                RA_SetDirtyM68kRegister(ctx, 15);
                EMIT(ctx, 
                    tbz(cc, SRB_M, 2),
                    mov_reg(sp, reg),
                    mov_reg_to_simd(REG_MSP, reg)
                );
                break;
            case 0x804: // ISP
                sp = RA_MapM68kRegister(ctx, 15);
                RA_SetDirtyM68kRegister(ctx, 15);
                EMIT(ctx, 
                    tbnz(cc, SRB_M, 2),
                    mov_reg(sp, reg),
                    mov_reg_to_simd(REG_ISP, reg)
                );
                break;
            case 0x0ea: /* JITSCFTHRESH - Maximal number of JIT units for "soft" cache flush */
                EMIT(ctx, str_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, JIT_SOFTFLUSH_THRESH)));
                break;
            case 0x0eb: /* JITCTRL - JIT control register */
                EMIT(ctx, str_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, JIT_CONTROL)));
                break;
            case 0x0ed: /* DBGCTRL */
            {
                uint8_t tmp2 = RA_AllocARMRegister(ctx);
                tmp = RA_AllocARMRegister(ctx);
                u.u64 = (uintptr_t)&debug;
                EMIT(ctx, 
                    mov64_immed_u16(tmp, u.u16[3], 0),
                    movk64_immed_u16(tmp, u.u16[2], 1),
                    movk64_immed_u16(tmp, u.u16[1], 2),
                    movk64_immed_u16(tmp, u.u16[0], 3),
                    ubfx(tmp2, reg, 0, 2),
                    str_offset(tmp, tmp2, 0)
                );
                if (_abs((intptr_t)&debug - (intptr_t)&disasm) < 255)
                {
                    int delta = (uintptr_t)&disasm - (uintptr_t)&debug;
                    EMIT(ctx, 
                        ubfx(tmp2, reg, 2, 1),
                        stur_offset(tmp, tmp2, delta)
                    );
                }
                else
                {
                    u.u64 = (uintptr_t)&disasm;
                    EMIT(ctx, 
                        mov64_immed_u16(tmp, u.u16[3], 0),
                        movk64_immed_u16(tmp, u.u16[2], 1),
                        movk64_immed_u16(tmp, u.u16[1], 2),
                        movk64_immed_u16(tmp, u.u16[0], 3),
                        ubfx(tmp2, reg, 2, 1),
                        str_offset(tmp, tmp2, 0)
                    );
                }
                RA_FreeARMRegister(ctx, tmp);
                RA_FreeARMRegister(ctx, tmp2);
                break;
            }
            case 0x0ee: /* DBGADDRLO */
                tmp = RA_AllocARMRegister(ctx);
                u.u64 = (uintptr_t)&debug_range_min;
                EMIT(ctx, 
                    mov64_immed_u16(tmp, u.u16[3], 0),
                    movk64_immed_u16(tmp, u.u16[2], 1),
                    movk64_immed_u16(tmp, u.u16[1], 2),
                    movk64_immed_u16(tmp, u.u16[0], 3),
                    str_offset(tmp, reg, 0)
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x0ef: /* DBGADDRHI */
                tmp = RA_AllocARMRegister(ctx);
                u.u64 = (uintptr_t)&debug_range_max;
                EMIT(ctx, 
                    mov64_immed_u16(tmp, u.u16[3], 0),
                    movk64_immed_u16(tmp, u.u16[2], 1),
                    movk64_immed_u16(tmp, u.u16[1], 2),
                    movk64_immed_u16(tmp, u.u16[0], 3),
                    str_offset(tmp, reg, 0)
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x1e0: /* JITCTRL2 - JIT second control register */
                EMIT(ctx, str_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, JIT_CONTROL2)));
                break;
            case 0x003: // TCR - write bits 15, 14, read all zeros for now
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    bic_immed(tmp, reg, 30, 16),
                    bic_immed(tmp, tmp, 1, 32 - 15), // Clear E bit, do not allow turning on MMU
                    strh_offset(ctxreg, tmp, __builtin_offsetof(struct M68KState, TCR))
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x004: // ITT0
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    movn_immed_u16(tmp, 0x1c9b, 0),
                    and_reg(tmp, tmp, reg, LSL, 0),
                    str_offset(ctxreg, tmp, __builtin_offsetof(struct M68KState, ITT0))
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x005: // ITT1
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    movn_immed_u16(tmp, 0x1c9b, 0),
                    and_reg(tmp, tmp, reg, LSL, 0),
                    str_offset(ctxreg, tmp, __builtin_offsetof(struct M68KState, ITT1))
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x006: // DTT0
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    movn_immed_u16(tmp, 0x1c9b, 0),
                    and_reg(tmp, tmp, reg, LSL, 0),
                    str_offset(ctxreg, tmp, __builtin_offsetof(struct M68KState, DTT0))
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x007: // DTT1
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    movn_immed_u16(tmp, 0x1c9b, 0),
                    and_reg(tmp, tmp, reg, LSL, 0),
                    str_offset(ctxreg, tmp, __builtin_offsetof(struct M68KState, DTT1))
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x805: // MMUSR
                EMIT(ctx, str_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, MMUSR)));
                break;
            case 0x806: // URP
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    bic_immed(tmp, reg, 9, 0),
                    str_offset(ctxreg, tmp, __builtin_offsetof(struct M68KState, URP))
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x807: // SRP
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    bic_immed(tmp, reg, 9, 0),
                    str_offset(ctxreg, tmp, __builtin_offsetof(struct M68KState, SRP))
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            default:
                EMIT_Exception(ctx, VECTOR_ILLEGAL_INSTRUCTION, 0);
                illegal = 1;
                break;
        }
    }
    else
    {
        switch (opcode2 & 0xfff)
        {
            case 0x000: // SFC
                EMIT(ctx, ldrb_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, SFC)));
                break;
            case 0x001: // DFC
                EMIT(ctx, ldrb_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, DFC)));
                break;
            case 0x800: // USP
                EMIT(ctx, mov_simd_to_reg(reg, REG_USP));
                break;
            case 0x801: // VBR
                EMIT(ctx, ldr_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, VBR)));
                break;
            case 0x002: // CACR
                EMIT(ctx, mov_simd_to_reg(reg, REG_CACR));
                break;
            case 0x803: // MSP
                sp = RA_MapM68kRegister(ctx, 15);
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    tst_immed(cc, 1, 32 - SRB_M),
                    mov_simd_to_reg(tmp, REG_MSP),
                    csel(reg, sp, tmp, A64_CC_NE)
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x804: // ISP
                sp = RA_MapM68kRegister(ctx, 15);
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    tst_immed(cc, 1, 32 - SRB_M),
                    mov_simd_to_reg(tmp, REG_ISP),
                    csel(reg, sp, tmp, A64_CC_EQ)
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x0e0: /* CNTFRQ - speed of counter clock in Hz */
                EMIT(ctx, mrs(reg, 3, 3, 14, 0, 0));
                break;
            case 0x0e1: /* CNTVALLO - lower 32 bits of the counter */
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    mrs(tmp, 3, 3, 14, 0, 1),
                    mov_reg(reg, tmp)
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x0e2: /* CNTVALHI - higher 32 bits of the counter */
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    mrs(tmp, 3, 3, 14, 0, 1),
                    lsr64(reg, tmp, 32)
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x0e3: /* INSNCNTLO - lower 32 bits of m68k instruction counter */
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    mov_simd_to_reg(tmp, CTX_INSN_COUNT),
                    add64_immed(tmp, tmp, insn_count & 0xfff)
                );
                if (insn_count & 0xfff000)
                    EMIT(ctx, add64_immed_lsl12(tmp, tmp, insn_count >> 12));
                EMIT(ctx, mov_reg(reg, tmp));
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x0e4: /* INSNCNTHI - higher 32 bits of m68k instruction counter */
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    mov_simd_to_reg(tmp, CTX_INSN_COUNT),
                    add64_immed(tmp, tmp, insn_count & 0xfff)
                );
                if (insn_count & 0xfff000)
                    EMIT(ctx, add64_immed_lsl12(tmp, tmp, insn_count >> 12));
                EMIT(ctx, lsr64(reg, tmp, 32));
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x0e5: /* ARMCNTLO - lower 32 bits of ARM instruction counter */
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    mrs(tmp, 3, 3, 9, 13, 0),
                    mov_reg(reg, tmp)
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x0e6: /* ARMCNTHI - higher 32 bits of ARM instruction counter */
                tmp = RA_AllocARMRegister(ctx);
                EMIT(ctx, 
                    mrs(tmp, 3, 3, 9, 13, 0),
                    lsr64(reg, tmp, 32)
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x0e7: /* JITSIZE - size of JIT cache, in bytes */
                EMIT(ctx, ldr_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, JIT_CACHE_TOTAL)));
                break;
            case 0x0e8: /* JITFREE - free space in JIT cache, in bytes */
                EMIT(ctx, ldr_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, JIT_CACHE_FREE)));
                break;
            case 0x0e9: /* JITCOUNT - Number of JIT units in cache */
                EMIT(ctx, ldr_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, JIT_UNIT_COUNT)));
                break;
            case 0x0ea: /* JITSCFTHRESH - Maximal number of JIT units for "soft" cache flush */
                EMIT(ctx, ldr_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, JIT_SOFTFLUSH_THRESH)));
                break;
            case 0x0eb: /* JITCTRL - JIT control register */
                EMIT(ctx, ldr_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, JIT_CONTROL)));
                break;
            case 0x0ec: /* JITCMISS */
                EMIT(ctx, ldr_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, JIT_CACHE_MISS)));
                break;
            case 0x0ed: /* DBGCTRL */
            {
                uint8_t tmp2 = RA_AllocARMRegister(ctx);
                tmp = RA_AllocARMRegister(ctx);
                u.u64 = (uintptr_t)&debug;
                EMIT(ctx, 
                    mov64_immed_u16(tmp, u.u16[3], 0),
                    movk64_immed_u16(tmp, u.u16[2], 1),
                    movk64_immed_u16(tmp, u.u16[1], 2),
                    movk64_immed_u16(tmp, u.u16[0], 3),

                    ldr_offset(tmp, tmp2, 0),
                    ubfx(reg, tmp2, 0, 2)
                );

                if (_abs((intptr_t)&debug - (intptr_t)&disasm) < 255)
                {
                    int delta = (uintptr_t)&disasm - (uintptr_t)&debug;
                    EMIT(ctx, ldur_offset(tmp, tmp2, delta));
                }
                else
                {
                    u.u64 = (uintptr_t)&disasm;
                    EMIT(ctx, 
                        mov64_immed_u16(tmp, u.u16[3], 0),
                        movk64_immed_u16(tmp, u.u16[2], 1),
                        movk64_immed_u16(tmp, u.u16[1], 2),
                        movk64_immed_u16(tmp, u.u16[0], 3),
                        ldr_offset(tmp, tmp2, 0)
                    );
                }
                EMIT(ctx, 
                    cbz(tmp2, 2),
                    orr_immed(reg, reg, 1, 30)
                );

                RA_FreeARMRegister(ctx, tmp);
                RA_FreeARMRegister(ctx, tmp2);
                break;
            }
            case 0x0ee: /* DBGADDRLO */
                tmp = RA_AllocARMRegister(ctx);
                u.u64 = (uintptr_t)&debug_range_min;
                EMIT(ctx, 
                    mov64_immed_u16(tmp, u.u16[3], 0),
                    movk64_immed_u16(tmp, u.u16[2], 1),
                    movk64_immed_u16(tmp, u.u16[1], 2),
                    movk64_immed_u16(tmp, u.u16[0], 3),
                    ldr_offset(tmp, reg, 0)
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x0ef: /* DBGADDRHI */
                tmp = RA_AllocARMRegister(ctx);
                u.u64 = (uintptr_t)&debug_range_max;
                EMIT(ctx, 
                    mov64_immed_u16(tmp, u.u16[3], 0),
                    movk64_immed_u16(tmp, u.u16[2], 1),
                    movk64_immed_u16(tmp, u.u16[1], 2),
                    movk64_immed_u16(tmp, u.u16[0], 3),
                    ldr_offset(tmp, reg, 0)
                );
                RA_FreeARMRegister(ctx, tmp);
                break;
            case 0x1e0: /* JITCTRL2 - JIT second control register */
                EMIT(ctx, ldr_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, JIT_CONTROL2)));
                break;
            case 0x003: // TCR - write bits 15, 14, read all zeros for now
                EMIT(ctx, ldrh_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, TCR)));
                break;
            case 0x004: // ITT0
                EMIT(ctx, ldr_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, ITT0)));
                break;
            case 0x005: // ITT1
                EMIT(ctx, ldr_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, ITT1)));
                break;
            case 0x006: // DTT0
                EMIT(ctx, ldr_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, DTT0)));
                break;
            case 0x007: // DTT1
                EMIT(ctx, ldr_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, DTT1)));
                break;
            case 0x805: // MMUSR
                EMIT(ctx, ldr_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, MMUSR)));
                break;
            case 0x806: // URP
                EMIT(ctx, ldr_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, URP)));
                break;
            case 0x807: // SRP
                EMIT(ctx, ldr_offset(ctxreg, reg, __builtin_offsetof(struct M68KState, SRP)));
                break;
            default:
                EMIT_Exception(ctx, VECTOR_ILLEGAL_INSTRUCTION, 0);
                illegal = 1;
                break;
        }
        RA_SetDirtyM68kRegister(ctx, opcode2 >> 12);
    }

    if (!illegal) {
        EMIT(ctx, add_immed(REG_PC, REG_PC, 4));
    }

    *tmpptr = b(ctx->tc_CodePtr - tmpptr);

    EMIT(ctx, INSN_TO_LE(0xffffffff));

    return 1;
}

static uint32_t EMIT_MOVEUSP(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint32_t *tmp;
    uint8_t cc = RA_ModifyCC(ctx);
    uint8_t an = RA_MapM68kRegister(ctx, 8 + (opcode & 7));

    EMIT_FlushPC(ctx);

    /* If supervisor is not active, put an exception here */
    tmp = ctx->tc_CodePtr++;
    EMIT_Exception(ctx, VECTOR_PRIVILEGE_VIOLATION, 0);
    *tmp = tbnz(cc, SRB_S, 1 + ctx->tc_CodePtr - tmp);
    tmp = ctx->tc_CodePtr++;

    if (opcode & 8)
    {
        EMIT(ctx, mov_simd_to_reg(an, REG_USP));
        RA_SetDirtyM68kRegister(ctx, 8 + (opcode & 7));
    }
    else
    {
        EMIT(ctx, mov_reg_to_simd(REG_USP, an));
    }

    EMIT(ctx, add_immed(REG_PC, REG_PC, 2));

    *tmp = b(ctx->tc_CodePtr - tmp);

    EMIT(ctx, INSN_TO_LE(0xffffffff));

    return 1;
}

static uint32_t EMIT_JSR(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t ext_words = 0;
    uint8_t ea = 0xff;
    uint8_t sp = 0xff;

    sp = RA_MapM68kRegister(ctx, 15);
    EMIT_LoadFromEffectiveAddress(ctx, 0, &ea, opcode & 0x3f, &ext_words, 1, NULL);
    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    EMIT_FlushPC(ctx);
    EMIT(ctx, str_offset_preindex(sp, REG_PC, -4));
    RA_SetDirtyM68kRegister(ctx, 15);
    EMIT_ResetOffsetPC(ctx);
    EMIT(ctx, mov_reg(REG_PC, ea));
    ctx->tc_M68kCodePtr += ext_words;
    RA_FreeARMRegister(ctx, ea);
    EMIT(ctx, INSN_TO_LE(0xffffffff));

    return 1;
}

static uint32_t EMIT_JMP(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t ext_words = 0;
    uint8_t ea = REG_PC;

    /* JMP immediate cound be faster... */
    EMIT_LoadFromEffectiveAddress(ctx, 0, &ea, opcode & 0x3f, &ext_words, 0, NULL);
    EMIT_ResetOffsetPC(ctx);
    ctx->tc_M68kCodePtr += ext_words;
    RA_FreeARMRegister(ctx, ea);
    EMIT(ctx, INSN_TO_LE(0xffffffff));

    return 1;
}

static uint32_t EMIT_NBCD(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_words = 0;

    uint8_t ea = -1;
    uint8_t cc = RA_ModifyCC(ctx);

    uint8_t hi = RA_AllocARMRegister(ctx);
    uint8_t lo = RA_AllocARMRegister(ctx);

    /* Dn mode */
    if ((opcode & 0x38) == 0)
    {
        uint8_t dn = RA_MapM68kRegister(ctx, opcode & 7);

        EMIT(ctx, 
            and_immed(lo, dn, 4, 0),
            and_immed(hi, dn, 4, 28)
        );
    }
    else
    {
        uint8_t t = RA_AllocARMRegister(ctx);

        /* Load EA */
        EMIT_LoadFromEffectiveAddress(ctx, 0, &ea, opcode & 0x3f, &ext_words, 0, NULL);

        /* -(An) mode? Decrease EA */
        if ((opcode & 0x38) == 0x20)
        {
            if ((opcode & 7) == 7) {
                EMIT(ctx, ldrb_offset_preindex(ea, t, -2));
            }
            else {
                EMIT(ctx, ldrb_offset_preindex(ea, t, -1));
            }
        }
        else {
            EMIT(ctx, ldrb_offset(ea, t, 0));
        }

        EMIT(ctx, 
            and_immed(lo, t, 4, 0),
            and_immed(hi, t, 4, 28)
        );

        RA_FreeARMRegister(ctx, t);
    }

    uint8_t tmp = RA_AllocARMRegister(ctx);
    uint8_t result = RA_AllocARMRegister(ctx);

    EMIT(ctx, 
        // Negate both loaded nibbles
        sub_reg(lo, 31, lo, LSL, 0),
        sub_reg(hi, 31, hi, LSL, 0),

        // If X was set decrement lower nibble
        tbz(cc, SRB_X, 2),
        sub_immed(lo, lo, 1),

        // Fix overflow in lower nibble
        cmp_immed(lo, 10),
        b_cc(A64_CC_CC, 2),
        sub_immed(lo, lo, 6),

        add_reg(result, lo, hi, LSL, 0),

        // Fix overflow in higher nibble
        and_immed(tmp, result, 5, 28),
        cmp_immed(tmp, 0xa0),
        b_cc(A64_CC_CC, 2),
        sub_immed(result, result, 0x60)
    );

    if (update_mask & SR_XC) {
        
        cc = RA_ModifyCC(ctx);

        switch (update_mask & SR_XC)
        {
            case SR_C:
                EMIT(ctx, 
                    bic_immed(cc, cc, 1, 32 - SRB_Calt),
                    orr_immed(tmp, cc, 1, 32 - SRB_Calt)
                );
                break;
            case SR_X:
                EMIT(ctx, 
                    bic_immed(cc, cc, 1, 32 - SRB_X),
                    orr_immed(tmp, cc, 1, 32 - SRB_X)
                );
                break;
            default:
                EMIT(ctx, 
                    mov_immed_u16(tmp, SR_XCalt, 0),
                    bic_reg(cc, cc, tmp, LSL, 0),
                    orr_reg(tmp, cc, tmp, LSL, 0)
                );
                break;
        }

        EMIT(ctx, csel(cc, tmp, cc, A64_CC_CS));
    }

    if (update_mask & SR_Z) {
        cc = RA_ModifyCC(ctx);

        EMIT(ctx, 
            ands_immed(31, result, 8, 0),
            bic_immed(tmp, cc, 1, 32 - SRB_Z),
            csel(cc, tmp, cc, A64_CC_NE)
        );
    }

    /* Dn mode */
    if ((opcode & 0x38) == 0)
    {
        uint8_t dn = RA_MapM68kRegister(ctx, opcode & 7);

        RA_SetDirtyM68kRegister(ctx, opcode & 7);

        EMIT(ctx, bfi(dn, result, 0, 8));
    }
    else
    {
        /* (An)+ mode? Increase EA */
        if ((opcode & 0x38) == 0x18)
        {
            if ((opcode & 7) == 7) {
                EMIT(ctx, strb_offset_postindex(ea, result, 2));
            }
            else {
                EMIT(ctx, strb_offset_postindex(ea, result, 1));
            }
        }
        else {
            EMIT(ctx, strb_offset(ea, result, 0));
        }
    }

    RA_FreeARMRegister(ctx, tmp);
    RA_FreeARMRegister(ctx, hi);
    RA_FreeARMRegister(ctx, lo);
    RA_FreeARMRegister(ctx, result);
    RA_FreeARMRegister(ctx, ea);

    ctx->tc_M68kCodePtr += ext_words;
    EMIT_AdvancePC(ctx, 2*(ext_words + 1));

    return 1;
}

static uint32_t EMIT_PEA(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t sp = 0xff;
    uint8_t ea = 0xff;
    uint8_t ext_words = 0;

    EMIT_LoadFromEffectiveAddress(ctx, 0, &ea, opcode & 0x3f, &ext_words, 1, NULL);
    ctx->tc_M68kCodePtr += ext_words;

    sp = RA_MapM68kRegister(ctx, 15);
    RA_SetDirtyM68kRegister(ctx, 15);

    EMIT(ctx, str_offset_preindex(sp, ea, -4));

    RA_FreeARMRegister(ctx, sp);
    RA_FreeARMRegister(ctx, ea);

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));

    return 1;
}

static uint32_t EMIT_MOVEM(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t dir = (opcode >> 10) & 1;
    uint8_t size = (opcode >> 6) & 1;
    uint16_t mask = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t block_size = 0;
    uint8_t ext_words = 0;
    extern int debug;
    uint32_t *ptr_orig = ctx->tc_CodePtr;

    ctx->tc_M68kCodePtr++;

    EMIT_AdvancePC(ctx, 2);

    for (int i=0; i < 16; i++)
    {
        if (mask & (1 << i))
            block_size += size ? 4:2;
    }

    if (dir == 0)
    {
        uint8_t base = 0xff;

        EMIT_LoadFromEffectiveAddress(ctx, 0, &base, opcode & 0x3f, &ext_words, 0, NULL);

        /* Pre-decrement mode? Decrease the base now */
        if ((opcode & 0x38) == 0x20)
        {
            uint8_t offset = 0;
            uint8_t tmp_base_reg = 0xff;

            RA_SetDirtyM68kRegister(ctx, (opcode & 7) + 8);

            /* Check if base register is on the list */
            if (mask & (0x8000 >> ((opcode & 7) + 8)))
            {
                tmp_base_reg = RA_AllocARMRegister(ctx);
                EMIT(ctx, sub_immed(tmp_base_reg, base, size ? 4 : 2));
            }

            /* In pre-decrement the register order is reversed */
            uint8_t rt1 = 0xff;

            for (int i=0; i < 16; i++)
            {
                if (mask & (0x8000 >> i))
                {
                    uint8_t reg = (i == ((opcode & 7) + 8) ? tmp_base_reg : RA_MapM68kRegister(ctx, i));

                    if (size) {
                        if (rt1 == 0xff)
                            rt1 = reg;
                        else {
                            if (offset == 0)
                                EMIT(ctx, stp_preindex(base, rt1, reg, -block_size));
                            else
                                EMIT(ctx, stp(base, rt1, reg, offset));
                            offset += 8;
                            rt1 = 0xff;
                        }
                    }
                    else
                    {
                        if (offset == 0)
                            EMIT(ctx, strh_offset_preindex(base, reg, -block_size));
                        else
                            EMIT(ctx, strh_offset(base, reg, offset));

                        offset += 2;
                    }
                }
            }
            if (rt1 != 0xff) {
                if (offset == 0)
                    EMIT(ctx, str_offset_preindex(base, rt1, -block_size));
                else
                    EMIT(ctx, str_offset(base, rt1, offset));
            }

            RA_FreeARMRegister(ctx, tmp_base_reg);
        }
        else
        {
            uint8_t offset = 0;
            uint8_t rt1 = 0xff;

            for (int i=0; i < 16; i++)
            {
                if (mask & (1 << i))
                {
                    uint8_t reg = RA_MapM68kRegister(ctx, i);
                    if (size) {
                        if (rt1 == 0xff)
                            rt1 = reg;
                        else {
                            EMIT(ctx, stp(base, rt1, reg, offset));
                            offset += 8;
                            rt1 = 0xff;
                        }
                    }
                    else
                    {
                        EMIT(ctx, strh_offset(base, reg, offset));
                        offset += 2;
                    }
                }
            }
            if (rt1 != 0xff)
                EMIT(ctx, str_offset(base, rt1, offset));
        }

        RA_FreeARMRegister(ctx, base);
    }
    else
    {
        uint8_t base = 0xff;
        uint8_t offset = 0;

        EMIT_LoadFromEffectiveAddress(ctx, 0, &base, opcode & 0x3f, &ext_words, 0, NULL);

        uint8_t rt1 = 0xff;

        for (int i=0; i < 16; i++)
        {
            if (mask & (1 << i))
            {
                /* Keep base register high in LRU */
                if (((opcode & 0x38) == 0x18)) RA_MapM68kRegister(ctx, (opcode & 7) + 8);

                uint8_t reg = RA_MapM68kRegisterForWrite(ctx, i);
                if (size) {
                    if ((((opcode & 0x38) == 0x18) && (i == (opcode & 7) + 8))) {
                        /* If rt1 was set, flush it now and reset, skip the base register */
                        if (rt1 != 0xff) {
                            EMIT(ctx, ldr_offset(base, rt1, offset));
                            rt1 = 0xff;
                            offset += 4;
                        }
                        offset += 4;
                        continue;
                    }

                    if (rt1 == 0xff)
                        rt1 = reg;
                    else {
                        if (block_size == 8 && (opcode & 0x38) == 0x18)
                            EMIT(ctx, ldp_postindex(base, rt1, reg, 8));
                        else 
                            EMIT(ctx, ldp(base, rt1, reg, offset));
                        offset += 8;
                        rt1 = 0xff;
                    }
                }
                else
                {
                    if (!(((opcode & 0x38) == 0x18) && (i == (opcode & 7) + 8)))
                        EMIT(ctx, ldrsh_offset(base, reg, offset));
                    offset += 2;
                }
            }
        }
        if (rt1 != 0xff) {
            EMIT(ctx, ldr_offset(base, rt1, offset));
        }

        /* Post-increment mode? Increase the base now */
        if ((opcode & 0x38) == 0x18 && !(block_size == 8 && size))
        {
            EMIT(ctx, add_immed(base, base, block_size));
            RA_SetDirtyM68kRegister(ctx, (opcode & 7) + 8);
        }

        RA_FreeARMRegister(ctx, base);
    }

    EMIT_AdvancePC(ctx, 2*(ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    /* No opcode was emited? At least flush PC counter now */
    if (ctx->tc_CodePtr == ptr_orig)
        EMIT_FlushPC(ctx);

    return 1;
}

static uint32_t EMIT_LEA(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t dest = 0xff;
    uint8_t ea = 0xff;
    uint8_t ext_words = 0;

    /* On AArch64 we can always map destination reg for write - it is always there! */
    dest = RA_MapM68kRegisterForWrite(ctx, 8 + ((opcode >> 9) & 7));

    /* Mode 2, (An) in case of LEA is a special case - just move the reg to destination */
    if ((opcode & 0x38) == 0x10)
    {
        uint8_t src = RA_MapM68kRegister(ctx, 8 + (opcode & 7));
        EMIT(ctx, mov_reg(dest, src));
    }
    else
        EMIT_LoadFromEffectiveAddress(ctx, 0, &dest, opcode & 0x3f, &ext_words, 1, NULL);

    ctx->tc_M68kCodePtr += ext_words;

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    RA_FreeARMRegister(ctx, ea);

    return 1;
}

static uint32_t EMIT_CHK(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint32_t opcode_address = (uint32_t)(uintptr_t)(ctx->tc_M68kCodePtr - 1);
    uint8_t ext_words = 0;
    uint8_t dn = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
    uint8_t src = -1;
    uint8_t tmpreg = RA_AllocARMRegister(ctx);

    /* word operation */
    if (opcode & 0x80)
    {
        EMIT_LoadFromEffectiveAddress(ctx, 2, &src, opcode & 0x3f, &ext_words, 0, NULL);
        EMIT(ctx, lsl(src, src, 16));
    }
    else
    {
        EMIT_LoadFromEffectiveAddress(ctx, 4, &src, opcode & 0x3f, &ext_words, 1, NULL);
    }

    uint8_t cc = RA_ModifyCC(ctx);

    EMIT(ctx, 
        // Clear Z, V and C flags, set Z back if operand is zero
        mov_immed_u16(tmpreg, SR_NCalt, 0),
        bic_reg(cc, cc, tmpreg, LSL, 0),
        tbz(src, 31, 2),
        orr_immed(cc, cc, 1, 32 - SRB_N)
    );

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    EMIT_FlushPC(ctx);

    /* Check if Dn < 0 */
    if (opcode & 0x80)
        EMIT(ctx, adds_reg(31, 31, dn, LSL, 16));
    else
        EMIT(ctx, adds_reg(31, 31, dn, LSL, 0));

    /* Jump to exception generator if negative */
    EMIT(ctx, b_cc(A64_CC_MI, 5));
    
    /* Check if Dn > src */
    if (opcode & 0x80)
        EMIT(ctx, subs_reg(31, src, dn, LSL, 16));
    else
        EMIT(ctx, subs_reg(31, src, dn, LSL, 0));
    
    uint32_t *tmp = ctx->tc_CodePtr++;
    EMIT(ctx, 
        bic_immed(cc, cc, 1, 31 & (32 - SRB_N)),
        b(2),

        orr_immed(cc, cc, 1, 31 & (32 - SRB_N))
    );

    EMIT_Exception(ctx, VECTOR_CHK, 2, opcode_address);

    RA_FreeARMRegister(ctx, src);
    RA_FreeARMRegister(ctx, tmpreg);

    *tmp = b_cc(A64_CC_GE, ctx->tc_CodePtr - tmp);
    
    EMIT(ctx, 
        (uint32_t)(uintptr_t)tmp,
        1, 0,
        INSN_TO_LE(0xfffffffe)
    );

    return 1;
}

static struct OpcodeDef InsnTable[4096] = {
    [00300 ... 00307] = { EMIT_MOVEfromSR, NULL, SR_ALL, 0, 1, 0, 2 },
    [00320 ... 00347] = { EMIT_MOVEfromSR, NULL, SR_ALL, 0, 1, 0, 2 },
    [00350 ... 00371] = { EMIT_MOVEfromSR, NULL, SR_ALL, 0, 1, 1, 2 },

    [01300 ... 01307] = { EMIT_MOVEfromCCR, NULL, SR_CCR, 0, 1, 0, 2 },
    [01320 ... 01347] = { EMIT_MOVEfromCCR, NULL, SR_CCR, 0, 1, 0, 2 },
    [01350 ... 01371] = { EMIT_MOVEfromCCR, NULL, SR_CCR, 0, 1, 1, 2 },

    [03300 ... 03307] = { EMIT_MOVEtoSR, NULL, SR_S, SR_ALL, 1, 0, 2 },
    [03320 ... 03347] = { EMIT_MOVEtoSR, NULL, SR_S, SR_ALL, 1, 0, 2 },
    [03350 ... 03374] = { EMIT_MOVEtoSR, NULL, SR_S, SR_ALL, 1, 1, 2 },

    [02300 ... 02307] = { EMIT_MOVEtoCCR, NULL, 0, SR_CCR, 1, 0, 2 },
    [02320 ... 02347] = { EMIT_MOVEtoCCR, NULL, 0, SR_CCR, 1, 0, 2 },
    [02350 ... 02374] = { EMIT_MOVEtoCCR, NULL, 0, SR_CCR, 1, 1, 2 },

    [04200 ... 04207] = { EMIT_EXT, NULL, 0, SR_NZVC, 1, 0, 0 },
    [04300 ... 04307] = { EMIT_EXT, NULL, 0, SR_NZVC, 1, 0, 0 },
    [04700 ... 04707] = { EMIT_EXT, NULL, 0, SR_NZVC, 1, 0, 0 },

    [04010 ... 04017] = { EMIT_LINK32, NULL, 0, 0, 3, 0, 0 },
    [07120 ... 07127] = { EMIT_LINK16, NULL, 0, 0, 2, 0, 0 },

    [04100 ... 04107] = { EMIT_SWAP, NULL, 0, SR_NZVC, 1, 0, 0 },
    [0xafc]           = { EMIT_ILLEGAL, NULL, SR_CCR, 0, 1, 0, 0 },
    [0xe40 ... 0xe4f] = { EMIT_TRAP, NULL, SR_CCR, 0, 1, 0, 0 },
    [07130 ... 07137] = { EMIT_UNLK, NULL, 0, 0, 1, 0, 0 },
    [0xe70]           = { EMIT_RESET, NULL, SR_S, 0, 1, 0, 0 },
    [0xe71]           = { EMIT_NOP, NULL, 0, 0, 1, 0, 0 },
    [0xe72]           = { EMIT_STOP, NULL, SR_S, SR_ALL, 2, 0, 0 },
    [0xe73]           = { EMIT_RTE, NULL, SR_S, SR_ALL, 1, 0, 0 },
    [0xe74]           = { EMIT_RTD, NULL, 0, 0, 2, 0, 0 },
    [0xe75]           = { EMIT_RTS, NULL, 0, 0, 1, 0, 0 },
    [0xe76]           = { EMIT_TRAPV, NULL, SR_CCR, 0, 1, 0, 0 },
    [0xe77]           = { EMIT_RTR, NULL, 0, SR_CCR, 1, 0, 0 },
    [0xe7a ... 0xe7b] = { EMIT_MOVEC, NULL, SR_S, 0, 2, 0, 4 },
    [0xe60 ... 0xe6f] = { EMIT_MOVEUSP, NULL, SR_S, 0, 1, 0, 4 },
    [04110 ... 04117] = { EMIT_BKPT, NULL, SR_ALL, 0, 1, 0, 0 },      // BKPT

    [07320 ... 07327] = { EMIT_JMP, NULL, 0, 0, 1, 0, 0 },
    [07350 ... 07373] = { EMIT_JMP, NULL, 0, 0, 1, 1, 0 },

    [07220 ... 07227] = { EMIT_JSR, NULL, 0, 0, 1, 0, 0 },
    [07250 ... 07273] = { EMIT_JSR, NULL, 0, 0, 1, 1, 0 },

    [00000 ... 00007] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 1 },
    [00100 ... 00107] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 2 },
    [00200 ... 00207] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 4 },

    [00020 ... 00047] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 1 },
    [00120 ... 00147] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 2 },
    [00220 ... 00247] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 4 },
    
    [00050 ... 00071] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 1, 1 },
    [00150 ... 00171] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 1, 2 },
    [00250 ... 00271] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 1, 4 },

    [01000 ... 01007] = { EMIT_CLR, NULL, 0, SR_NZVC, 1, 0, 1 },
    [01100 ... 01107] = { EMIT_CLR, NULL, 0, SR_NZVC, 1, 0, 2 },
    [01200 ... 01207] = { EMIT_CLR, NULL, 0, SR_NZVC, 1, 0, 4 },

    [01020 ... 01047] = { EMIT_CLR, NULL, 0, SR_NZVC, 1, 0, 1 },
    [01120 ... 01147] = { EMIT_CLR, NULL, 0, SR_NZVC, 1, 0, 2 },
    [01220 ... 01247] = { EMIT_CLR, NULL, 0, SR_NZVC, 1, 0, 4 },

    [01050 ... 01071] = { EMIT_CLR, NULL, 0, SR_NZVC, 1, 1, 1 },
    [01150 ... 01171] = { EMIT_CLR, NULL, 0, SR_NZVC, 1, 1, 2 },
    [01250 ... 01271] = { EMIT_CLR, NULL, 0, SR_NZVC, 1, 1, 4 },

    [02000 ... 02007] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 0, 1 },
    [02100 ... 02107] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 0, 2 },
    [02200 ... 02207] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 0, 4 },

    [02020 ... 02047] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 0, 1 },
    [02120 ... 02147] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 0, 2 },
    [02220 ... 02247] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 0, 4 },

    [02050 ... 02071] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 1, 1 },
    [02150 ... 02171] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 1, 2 },
    [02250 ... 02271] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 1, 4 },

    [03000 ... 03007] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 0, 1 },
    [03100 ... 03107] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 0, 2 },
    [03200 ... 03207] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 0, 4 },

    [03020 ... 03047] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 0, 1 },
    [03120 ... 03147] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 0, 2 },
    [03220 ... 03247] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 0, 4 },

    [03050 ... 03071] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 1, 1 },
    [03150 ... 03171] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 1, 2 },
    [03250 ... 03271] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 1, 4 },

    [05000 ... 05007] = { EMIT_TST, NULL, 0, SR_NZVC, 1, 0, 1 },
    [05020 ... 05047] = { EMIT_TST, NULL, 0, SR_NZVC, 1, 0, 1 },
    [05050 ... 05074] = { EMIT_TST, NULL, 0, SR_NZVC, 1, 1, 1 },
    
    [05100 ... 05147] = { EMIT_TST, NULL, 0, SR_NZVC, 1, 0, 2 },
    [05150 ... 05174] = { EMIT_TST, NULL, 0, SR_NZVC, 1, 1, 2 },
    
    [05200 ... 05247] = { EMIT_TST, NULL, 0, SR_NZVC, 1, 0, 4 },
    [05250 ... 05274] = { EMIT_TST, NULL, 0, SR_NZVC, 1, 1, 4 },

    [04000 ... 04007] = { EMIT_NBCD, NULL, SR_XZ, SR_XZC, 1, 0, 1 },
    [04020 ... 04047] = { EMIT_NBCD, NULL, SR_XZ, SR_XZC, 1, 0, 1 },
    [04050 ... 04071] = { EMIT_NBCD, NULL, SR_XZ, SR_XZC, 1, 1, 1 },

    [04120 ... 04127] = { EMIT_PEA, NULL, 0, 0, 1, 0, 4 },
    [04150 ... 04173] = { EMIT_PEA, NULL, 0, 0, 1, 1, 4 },

    [05300 ... 05307] = { EMIT_TAS, NULL, 0, SR_NZVC, 1, 0, 1 },
    [05320 ... 05347] = { EMIT_TAS, NULL, 0, SR_NZVC, 1, 0, 1 },
    [05350 ... 05371] = { EMIT_TAS, NULL, 0, SR_NZVC, 1, 1, 1 },

    [06000 ... 06007] = { EMIT_MUL_DIV, NULL, 0, SR_NZVC, 2, 0, 4 },
    [06020 ... 06047] = { EMIT_MUL_DIV, NULL, 0, SR_NZVC, 2, 0, 4 },
    [06050 ... 06074] = { EMIT_MUL_DIV, NULL, 0, SR_NZVC, 2, 1, 4 },
    [06100 ... 06107] = { EMIT_MUL_DIV, NULL, 0, SR_NZVC, 2, 0, 4 },
    [06120 ... 06147] = { EMIT_MUL_DIV, NULL, 0, SR_NZVC, 2, 0, 4 },
    [06150 ... 06174] = { EMIT_MUL_DIV, NULL, 0, SR_NZVC, 2, 1, 4 },

    [04220 ... 04227] = { EMIT_MOVEM, NULL, 0, 0, 2, 0, 2 },
    [04320 ... 04327] = { EMIT_MOVEM, NULL, 0, 0, 2, 0, 4 },
    [04240 ... 04247] = { EMIT_MOVEM, NULL, 0, 0, 2, 0, 2 },
    [04340 ... 04347] = { EMIT_MOVEM, NULL, 0, 0, 2, 0, 4 },
    [04250 ... 04271] = { EMIT_MOVEM, NULL, 0, 0, 2, 1, 2 },
    [04350 ... 04371] = { EMIT_MOVEM, NULL, 0, 0, 2, 1, 4 },

    [06220 ... 06237] = { EMIT_MOVEM, NULL, 0, 0, 2, 0, 2 },
    [06320 ... 06337] = { EMIT_MOVEM, NULL, 0, 0, 2, 0, 4 },
    [06250 ... 06273] = { EMIT_MOVEM, NULL, 0, 0, 2, 1, 2 },
    [06350 ... 06373] = { EMIT_MOVEM, NULL, 0, 0, 2, 1, 4 },

    [00720 ... 00727] = { EMIT_LEA, NULL, 0, 0, 1, 0, 4 },
    [00750 ... 00773] = { EMIT_LEA, NULL, 0, 0, 1, 1, 4 },
    [01720 ... 01727] = { EMIT_LEA, NULL, 0, 0, 1, 0, 4 },
    [01750 ... 01773] = { EMIT_LEA, NULL, 0, 0, 1, 1, 4 },
    [02720 ... 02727] = { EMIT_LEA, NULL, 0, 0, 1, 0, 4 },
    [02750 ... 02773] = { EMIT_LEA, NULL, 0, 0, 1, 1, 4 },
    [03720 ... 03727] = { EMIT_LEA, NULL, 0, 0, 1, 0, 4 },
    [03750 ... 03773] = { EMIT_LEA, NULL, 0, 0, 1, 1, 4 },
    [04720 ... 04727] = { EMIT_LEA, NULL, 0, 0, 1, 0, 4 },
    [04750 ... 04773] = { EMIT_LEA, NULL, 0, 0, 1, 1, 4 },
    [05720 ... 05727] = { EMIT_LEA, NULL, 0, 0, 1, 0, 4 },
    [05750 ... 05773] = { EMIT_LEA, NULL, 0, 0, 1, 1, 4 },
    [06720 ... 06727] = { EMIT_LEA, NULL, 0, 0, 1, 0, 4 },
    [06750 ... 06773] = { EMIT_LEA, NULL, 0, 0, 1, 1, 4 },
    [07720 ... 07727] = { EMIT_LEA, NULL, 0, 0, 1, 0, 4 },
    [07750 ... 07773] = { EMIT_LEA, NULL, 0, 0, 1, 1, 4 },

    [00600 ... 00607] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [00620 ... 00647] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [00650 ... 00674] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [00400 ... 00407] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [00420 ... 00447] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [00450 ... 00474] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [01600 ... 01607] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [01620 ... 01647] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [01650 ... 01674] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [01400 ... 01407] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [01420 ... 01447] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [01450 ... 01474] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [02600 ... 02607] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [02620 ... 02647] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [02650 ... 02674] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [02400 ... 02407] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [02420 ... 02447] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [02450 ... 02474] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [03600 ... 03607] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [03620 ... 03647] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [03650 ... 03674] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [03400 ... 03407] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [03420 ... 03447] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [03450 ... 03474] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [04600 ... 04607] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [04620 ... 04647] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [04650 ... 04674] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [04400 ... 04407] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [04420 ... 04447] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [04450 ... 04474] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [05600 ... 05607] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [05620 ... 05647] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [05650 ... 05674] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [05400 ... 05407] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [05420 ... 05447] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [05450 ... 05474] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [06600 ... 06607] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [06620 ... 06647] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [06650 ... 06674] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [06400 ... 06407] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [06420 ... 06447] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [06450 ... 06474] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [07600 ... 07607] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [07620 ... 07647] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [07650 ... 07674] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [07400 ... 07407] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [07420 ... 07447] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [07450 ... 07474] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

};

uint32_t EMIT_line4(struct TranslatorContext *ctx)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr++);

    if (InsnTable[opcode & 0xfff].od_Emit) {
        return InsnTable[opcode & 0xfff].od_Emit(ctx, opcode);
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

uint32_t GetSR_Line4(uint16_t opcode)
{
    /* If instruction is in the table, return what flags it needs (shifted 16 bits left) and flags it sets */
    if (InsnTable[opcode & 0xfff].od_Emit) {
        return (InsnTable[opcode & 0xfff].od_SRNeeds << 16) | InsnTable[opcode & 0xfff].od_SRSets;
    }
    /* Instruction not found, i.e. it needs all flags and sets none (ILLEGAL INSTRUCTION exception) */
    else {
        kprintf("Undefined Line4\n");
        return SR_CCR << 16;
    }
}


int M68K_GetLine4Length(uint16_t *insn_stream)
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
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize);
    }

    return length;
}


#ifdef PISTORM_ANY_MODEL
void do_reset()
{
    void ps_pulse_reset();

    struct Size sz = get_display_size();
    init_display(sz, NULL, NULL);

    ps_pulse_reset();
}
#endif
