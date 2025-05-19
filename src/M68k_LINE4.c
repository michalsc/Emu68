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

uint32_t *EMIT_MUL_DIV(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr);

uint32_t *EMIT_MUL_DIV_(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    return EMIT_MUL_DIV(ptr, opcode, m68k_ptr);
}


extern uint32_t insn_count;

uint32_t *EMIT_CLR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
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
            uint8_t dn = RA_MapM68kRegister(&ptr, opcode & 7);
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);
            switch (size) {
                case 1:
                    *ptr++ = bic_immed(dn, dn, 8, 0);
                    break;
                case 2:
                    *ptr++ = bic_immed(dn, dn, 16, 0);
            }
        }
        else {
            uint8_t dn = RA_MapM68kRegisterForWrite(&ptr, opcode & 7);
            *ptr++ = mov_reg(dn, 31);
        }
    }
    else
        ptr = EMIT_StoreToEffectiveAddress(ptr, size, &zero, opcode & 0x3f, *m68k_ptr, &ext_count, 0);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        if (update_mask & ~SR_Z)
        {
            uint8_t alt_flags = update_mask;
            if ((alt_flags & 3) != 0 && (alt_flags & 3) < 3)
                alt_flags ^= 3;
            ptr = EMIT_ClearFlags(ptr, cc, alt_flags);
        }
        if (update_mask & SR_Z)
            ptr = EMIT_SetFlags(ptr, cc, SR_Z);
    }
    return ptr;
}

uint32_t *EMIT_NOT(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
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
            dest = RA_MapM68kRegister(&ptr, opcode & 7);
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);
            test_register = dest;

            *ptr++ = mvn_reg(dest, dest, LSL, 0);
        }
        else
        {
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            /* Fetch m68k register for write */
            dest = RA_MapM68kRegister(&ptr, opcode & 7);

            /* Mark register dirty */
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);

            test_register = dest;

            switch(size)
            {
                case 2:
                    *ptr++ = mvn_reg(tmp, dest, LSL, 0);
                    *ptr++ = bfi(dest, tmp, 0, 16);
                    break;
                case 1:
                    *ptr++ = mvn_reg(tmp, dest, LSL, 0);
                    *ptr++ = bfi(dest, tmp, 0, 8);
                    break;
            }

            RA_FreeARMRegister(&ptr, tmp);
        }
    }
    else
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;
        test_register = tmp;

        /* Load effective address */
        if (mode == 4 || mode == 3)
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
        else
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);

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

            *ptr++ = mvn_reg(tmp, tmp, LSL, 0);

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

            *ptr++ = mvn_reg(tmp, tmp, LSL, 0);

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

            *ptr++ = mvn_reg(tmp, tmp, LSL, 0);

            if (mode == 3)
            {
                *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strb_offset(dest, tmp, 0);
            break;
        }
    }

    RA_FreeARMRegister(&ptr, dest);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);

        switch(size) {
            case 4:
                *ptr++ = cmn_reg(31, test_register, LSL, 0);
                break;
            case 2:
                *ptr++ = cmn_reg(31, test_register, LSL, 16);
                break;
            case 1:
                *ptr++ = cmn_reg(31, test_register, LSL, 24);
                break;
        }

        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
    }

    RA_FreeARMRegister(&ptr, test_register);

    return ptr;
}

uint32_t *EMIT_NEG(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
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
            dest = RA_MapM68kRegister(&ptr, opcode & 7);
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);

            *ptr++ = negs_reg(dest, dest, LSL, 0);
        }
        else
        {
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            /* Fetch m68k register for write */
            dest = RA_MapM68kRegister(&ptr, opcode & 7);

            /* Mark register dirty */
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);

            switch(size)
            {
                case 2:
                    if (update_mask == 0) {
                        *ptr++ = negs_reg(tmp, dest, LSL, 0);
                        *ptr++ = bfxil(dest, tmp, 0, 16);
                    }
                    else {
                        *ptr++ = negs_reg(tmp, dest, LSL, 16);
                        *ptr++ = bfxil(dest, tmp, 16, 16);
                    }
                    break;
                
                case 1:
                    if (update_mask == 0) {
                        *ptr++ = negs_reg(tmp, dest, LSL, 0);
                        *ptr++ = bfxil(dest, tmp, 0, 8);
                    }
                    else {
                        *ptr++ = negs_reg(tmp, dest, LSL, 24);
                        *ptr++ = bfxil(dest, tmp, 24, 8);
                    }
                    break;
            }

            RA_FreeARMRegister(&ptr, tmp);
        }
    }
    else
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;

        /* Load effective address */
        if (mode == 4 || mode == 3)
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
        else
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);

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

            *ptr++ = negs_reg(tmp, tmp, LSL, 0);

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

            if (update_mask == 0) {
                *ptr++ = neg_reg(tmp, tmp, LSL, 0);
            }
            else {
                *ptr++ = negs_reg(tmp, tmp, LSL, 16);
                *ptr++ = lsr(tmp, tmp, 16);
            }

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

            if (update_mask == 0) {
                *ptr++ = neg_reg(tmp, tmp, LSL, 0);
            }
            else {
                *ptr++ = negs_reg(tmp, tmp, LSL, 24);
                *ptr++ = lsr(tmp, tmp, 24);
            }

            if (mode == 3)
            {
                *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strb_offset(dest, tmp, 0);
            break;
        }

        RA_FreeARMRegister(&ptr, tmp);
    }

    RA_FreeARMRegister(&ptr, dest);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

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
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_X, ARM_CC_NE);
            else if ((update_mask & (SR_X | SR_C)) == SR_C)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Calt, ARM_CC_NE);
            else
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Calt | SR_X, ARM_CC_NE);
        }
    }

    return ptr;
}

// BROKEN!!!!
uint32_t *EMIT_NEGX(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
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

    uint8_t cc = RA_GetCC(&ptr);
    if (size == 4) {
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        *ptr++ = mvn_reg(tmp, cc, ROR, 7);
        *ptr++ = set_nzcv(tmp);

        RA_FreeARMRegister(&ptr, tmp);
    } else {
        *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_X));
    }

    if ((opcode & 0x0038) == 0)
    {
        if (size == 4)
        {
            dest = RA_MapM68kRegister(&ptr, opcode & 7);
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);

            *ptr++ = ngcs(dest, dest);
        }
        else
        {
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            /* Fetch m68k register for write */
            dest = RA_MapM68kRegister(&ptr, opcode & 7);

            /* Mark register dirty */
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);

            switch(size)
            {
                case 2:
                    *ptr++ = and_immed(tmp, dest, 16, 0);   // Take lower 16 bits of destination
                    *ptr++ = neg_reg(tmp, tmp, LSL, 0);     // negate
                    *ptr++ = b_cc(A64_CC_EQ, 2);            // Skip if X not set
                    *ptr++ = sub_immed(tmp, tmp, 1);

                    if (update_mask & SR_XVC) {

                        ptr = EMIT_ClearFlags(ptr, cc, SR_XVC);

                        uint8_t tmp_2 = RA_AllocARMRegister(&ptr);

                        *ptr++ = and_reg(tmp_2, tmp, dest, LSL, 0);

                        if (update_mask & SR_V)
                        {
                            *ptr++ = tbz(tmp_2, 15, 2);
                            *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_Valt));
                        }
                        
                        if ((update_mask & SR_XC) == SR_XC)
                        {
                            *ptr++ = tbz(tmp, 16, 3);
                            *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt));
                            *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_X));
                        }
                        else if ((update_mask & SR_XC) == SR_C)
                        {
                            *ptr++ = tbz(tmp, 16, 2);
                            *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt));
                        }
                        else if ((update_mask & SR_XC) == SR_X)
                        {
                            *ptr++ = tbz(tmp, 16, 2);
                            *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_X));
                        }


                        update_mask &= ~SR_XVC;             // Don't nag anymore with the flags

                        RA_FreeARMRegister(&ptr, tmp_2);
                    }

                    if (update_mask & SR_NZ) {
                        *ptr++ = adds_reg(31, 31, tmp, LSL, 16);
                    }

                    *ptr++ = bfxil(dest, tmp, 0, 16);       // Insert result
                    break;
                case 1:
                    *ptr++ = and_immed(tmp, dest, 8, 0);    // Take lower 8 bits of destination
                    *ptr++ = neg_reg(tmp, tmp, LSL, 0);     // negate
                    *ptr++ = b_cc(A64_CC_EQ, 2);            // Skip if X not set
                    *ptr++ = sub_immed(tmp, tmp, 1);

                    if (update_mask & SR_XVC) {
                        ptr = EMIT_ClearFlags(ptr, cc, SR_XVC);

                        uint8_t tmp_2 = RA_AllocARMRegister(&ptr);

                        *ptr++ = and_reg(tmp_2, tmp, dest, LSL, 0);  // C at position 8 in tmp, V at position 7 in tmp2

                        if (update_mask & SR_V)
                        {
                            *ptr++ = tbz(tmp_2, 7, 2);
                            *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_Valt));
                        }
                        
                        if ((update_mask & SR_XC) == SR_XC)
                        {
                            *ptr++ = tbz(tmp, 8, 3);
                            *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt));
                            *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_X));
                        }
                        else if ((update_mask & SR_XC) == SR_C)
                        {
                            *ptr++ = tbz(tmp, 8, 2);
                            *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt));
                        }
                        else if ((update_mask & SR_XC) == SR_X)
                        {
                            *ptr++ = tbz(tmp, 8, 2);
                            *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_X));
                        }

                        update_mask &= ~SR_XVC;             // Don't nag anymore with the flags

                        RA_FreeARMRegister(&ptr, tmp_2);
                    }

                    if (update_mask & SR_NZ) {
                        *ptr++ = adds_reg(31, 31, tmp, LSL, 24);
                    }

                    *ptr++ = bfxil(dest, tmp, 0, 8);
                    break;
            }

            RA_FreeARMRegister(&ptr, tmp);
        }
    }
    else
    {
        uint8_t src = RA_AllocARMRegister(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;

        /* Load effective address */
        if (mode == 4 || mode == 3)
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
        else
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);

        /* Fetch data into temporary register, perform add, store it back */
        switch (size)
        {
        case 4:
            if (mode == 4)
            {
                *ptr++ = ldr_offset_preindex(dest, src, -4);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldr_offset(dest, src, 0);

            *ptr++ = ngcs(tmp, src);

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
                *ptr++ = ldrh_offset_preindex(dest, src, -2);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldrh_offset(dest, src, 0);

            *ptr++ = neg_reg(tmp, src, LSL, 0);     // negate
            *ptr++ = b_cc(A64_CC_EQ, 2);            // Skip if X not set
            *ptr++ = sub_immed(tmp, tmp, 1);

            if (update_mask & SR_XVC) {
                ptr = EMIT_ClearFlags(ptr, cc, SR_XVC);
                uint8_t tmp_2 = RA_AllocARMRegister(&ptr);

                *ptr++ = and_reg(tmp_2, tmp, src, LSL, 0);

                if (update_mask & SR_V)
                {
                    *ptr++ = tbz(tmp_2, 15, 2);
                    *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_Valt));
                }
                
                if ((update_mask & SR_XC) == SR_XC)
                {
                    *ptr++ = tbz(tmp, 16, 3);
                    *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt));
                    *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_X));
                }
                else if ((update_mask & SR_XC) == SR_C)
                {
                    *ptr++ = tbz(tmp, 16, 2);
                    *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt));
                }
                else if ((update_mask & SR_XC) == SR_X)
                {
                    *ptr++ = tbz(tmp, 16, 2);
                    *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_X));
                }

                update_mask &= ~SR_XVC;             // Don't nag anymore with the flags

                RA_FreeARMRegister(&ptr, tmp_2);
            }

            if (update_mask & SR_NZ) {
                *ptr++ = adds_reg(31, 31, tmp, LSL, 16);
            }

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
                *ptr++ = ldrb_offset_preindex(dest, src, (opcode & 7) == 7 ? -2 : -1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldrb_offset(dest, src, 0);

            *ptr++ = neg_reg(tmp, src, LSL, 0);     // negate
            *ptr++ = b_cc(A64_CC_EQ, 2);            // Skip if X not set
            *ptr++ = sub_immed(tmp, tmp, 1);

            if (update_mask & SR_XVC) {
                ptr = EMIT_ClearFlags(ptr, cc, SR_XVC);

                uint8_t tmp_2 = RA_AllocARMRegister(&ptr);

                *ptr++ = and_reg(tmp_2, tmp, src, LSL, 0);

                if (update_mask & SR_V)
                {
                    *ptr++ = tbz(tmp_2, 7, 2);
                    *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_Valt));
                }
                
                if ((update_mask & SR_XC) == SR_XC)
                {
                    *ptr++ = tbz(tmp, 8, 3);
                    *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt));
                    *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_X));
                }
                else if ((update_mask & SR_XC) == SR_C)
                {
                    *ptr++ = tbz(tmp, 8, 2);
                    *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_Calt));
                }
                else if ((update_mask & SR_XC) == SR_X)
                {
                    *ptr++ = tbz(tmp, 8, 2);
                    *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_X));
                }
                update_mask &= ~SR_XVC;             // Don't nag anymore with the flags

                RA_FreeARMRegister(&ptr, tmp_2);
            }

            if (update_mask & SR_NZ) {
                *ptr++ = adds_reg(31, 31, tmp, LSL, 24);
            }

            if (mode == 3)
            {
                *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strb_offset(dest, tmp, 0);
            break;
        }

        RA_FreeARMRegister(&ptr, src);
        RA_FreeARMRegister(&ptr, tmp);
    }

    RA_FreeARMRegister(&ptr, dest);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        if (update_mask & SR_Z) {
            *ptr++ = b_cc(A64_CC_EQ, 2);
            *ptr++ = bic_immed(cc, cc, 1, 31 & (32 - SRB_Z));
            update_mask &= ~SR_Z;
        }

        if (update_mask) {
            uint8_t alt_mask = update_mask;
            if ((alt_mask & 3) != 0 && (alt_mask & 3) < 3)
                alt_mask ^= 3;

            *ptr++ = mov_immed_u16(tmp, alt_mask, 0);
            *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);
        }

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
        
        RA_FreeARMRegister(&ptr, tmp);

    }

    return ptr;
}

uint32_t *EMIT_TST(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
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
        dest = RA_MapM68kRegister(&ptr, opcode & 7);

        /* Perform add operation */
        switch (size)
        {
            case 4:
                *ptr++ = cmn_reg(31, dest, LSL, 0);
                break;
            case 2:
                *ptr++ = cmn_reg(31, dest, LSL, 16);
                break;
            case 1:
                *ptr++ = cmn_reg(31, dest, LSL, 24);
                break;
        }
    }
    else
    {
        /* Load effective address */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);

        /* Fetch data into temporary register, perform add, store it back */
        switch (size)
        {
            case 4:
                *ptr++ = cmn_reg(31, dest, LSL, 0);
                break;
            case 2:
                *ptr++ = cmn_reg(31, dest, LSL, 16);
                break;
            case 1:
                *ptr++ = cmn_reg(31, dest, LSL, 24);
                break;
        }
    }

    RA_FreeARMRegister(&ptr, immed);
    RA_FreeARMRegister(&ptr, dest);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

        host_z_set = 1;
        host_n_set = 1;
        host_c_set = 0;
        host_v_set = 0;

        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
    }
    return ptr;
}

uint32_t *EMIT_TAS(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t ext_count = 0;
    uint8_t dest = 0xff;
    uint8_t mode = (opcode & 0x0038) >> 3;
    uint8_t tmpreg = RA_AllocARMRegister(&ptr);
    uint8_t tmpresult = RA_AllocARMRegister(&ptr);
    uint8_t tmpstate = RA_AllocARMRegister(&ptr);

    /* handle TAS on register, just make a copy and then orr 0x80 on register */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register */
        dest = RA_MapM68kRegister(&ptr, opcode & 7);
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        *ptr++ = mov_reg(tmpresult, dest);
        *ptr++ = orr_immed(dest, dest, 1, 25);
    }
    else
    {
        /* Load effective address */
        if (mode == 4 || mode == 3)
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
        else
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);

        if (mode == 4)
        {
            *ptr++ = sub_immed(dest, dest, (opcode & 7) == 7 ? 2 : 1);
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }

        *ptr++ = ldxrb(dest, tmpresult);
        *ptr++ = orr_immed(tmpreg, tmpresult, 1, 25);
        *ptr++ = stxrb(dest, tmpreg, tmpstate);
        *ptr++ = cmp_reg(31, tmpstate, LSL, 0);
        *ptr++ = b_cc(A64_CC_NE, -4);

        if (mode == 3)
        {
            *ptr++ = add_immed(dest, dest, (opcode & 7) == 7 ? 2 : 1);
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }
    }

    RA_FreeARMRegister(&ptr, dest);
    RA_FreeARMRegister(&ptr, tmpreg);
    RA_FreeARMRegister(&ptr, tmpstate);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

    if (update_mask)
    {
        *ptr++ = cmn_reg(31, tmpresult, LSL, 24);
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
    }

    RA_FreeARMRegister(&ptr, tmpresult);

    return ptr;
}

static uint32_t *EMIT_MOVEfromSR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;

    uint8_t cc = RA_ModifyCC(&ptr);
    uint8_t ext_words = 0;
    uint32_t *tmpptr;

    ptr = EMIT_FlushPC(ptr);

    /* Test if supervisor mode is active */
    *ptr++ = ands_immed(31, cc, 1, 32 - SRB_S);
    tmpptr = ptr;
    *ptr++ = b_cc(A64_CC_EQ, 23);

    uint8_t tmp_cc = RA_AllocARMRegister(&ptr);

    *ptr++ = mov_reg(tmp_cc, cc);
    *ptr++ = rbit(0, cc);
    *ptr++ = bfxil(tmp_cc, 0, 30, 2);

    ptr = EMIT_StoreToEffectiveAddress(ptr, 2, &tmp_cc, opcode & 0x3f, *m68k_ptr, &ext_words, 0);

    RA_FreeARMRegister(&ptr, tmp_cc);

    *tmpptr = b_cc(A64_CC_EQ, 2 + ptr - tmpptr);

    *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_words + 1));
    tmpptr = ptr;
    *ptr++ = b_cc(A64_CC_AL, 0);

    /* No supervisor. Update USP, generate exception */
    ptr = EMIT_Exception(ptr, VECTOR_PRIVILEGE_VIOLATION, 0);
    
    *tmpptr = b_cc(A64_CC_AL, ptr - tmpptr);
    *ptr++ = (uint32_t)(uintptr_t)tmpptr;
    *ptr++ = 1;
    *ptr++ = 0;
    *ptr++ = INSN_TO_LE(0xfffffffe);

    (*m68k_ptr) += ext_words;

    return ptr;
}

static uint32_t *EMIT_MOVEfromCCR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;

    uint8_t cc = RA_GetCC(&ptr);
    uint8_t ext_words = 0;

    if (opcode & 0x38)
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        *ptr++ = mov_reg(tmp, cc);
        *ptr++ = rbit(0, cc);
        *ptr++ = bic_immed(tmp, tmp, 11, 27);
        *ptr++ = bfxil(tmp, 0, 30, 2);
        ptr = EMIT_StoreToEffectiveAddress(ptr, 2, &tmp, opcode & 0x3f, *m68k_ptr, &ext_words, 0);

        RA_FreeARMRegister(&ptr, tmp);
    }
    else
    {
        /* Fetch m68k register */
        uint8_t dest = RA_MapM68kRegister(&ptr, opcode & 7);

        /* Mark register dirty */
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        *ptr++ = bfi(dest, cc, 0, 5);
        *ptr++ = rbit(0, cc);
        *ptr++ = bic_immed(dest, dest, 11, 27);
        *ptr++ = bfxil(dest, 0, 30, 2);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    return ptr;
}

static uint32_t *EMIT_MOVEtoSR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    uint8_t cc = RA_ModifyCC(&ptr);
    uint8_t ext_words = 0;
    uint8_t src = 0xff;
    uint8_t orig = 1; //RA_AllocARMRegister(&ptr);
    uint8_t changed = 2; //RA_AllocARMRegister(&ptr);
    uint8_t sp = RA_MapM68kRegister(&ptr, 15);
    uint32_t *tmpptr;

    RA_SetDirtyM68kRegister(&ptr, 15);

    ptr = EMIT_FlushPC(ptr);

    /* If supervisor is not active, put an exception here */
    tmpptr = ptr;
    ptr++;
    ptr = EMIT_Exception(ptr, VECTOR_PRIVILEGE_VIOLATION, 0);

    *tmpptr = tbnz(cc, SRB_S, 1 + ptr - tmpptr);
    tmpptr = ptr;
    ptr++;

    *ptr++ = mov_reg(orig, cc);
    *ptr++ = mov_immed_u16(changed, 0xf71f, 0);
    
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
    if ((opcode & 0x38) == 0) /* Dn direct into SR */
    {
        uint8_t src_mod = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_reg(src_mod, src);
        src = src_mod;
    }
    *ptr++ = rbit(0, src);
    *ptr++ = bfxil(src, 0, 30, 2);

    cc = RA_ModifyCC(&ptr);

    *ptr++ = and_reg(cc, changed, src, LSL, 0);
    *ptr++ = eor_reg(changed, orig, cc, LSL, 0);

    /* If neither S nor M changed, go further */
    *ptr++ = ands_immed(31, changed, 2, 32 - SRB_M);
    *ptr++ = b_cc(A64_CC_EQ, 12);

    /* S or M changed. First of all, store stack pointer to either ISP or MSP */
    *ptr++ = tbz(orig, SRB_M, 3);
    *ptr++ = mov_reg_to_simd(31, TS_S, 3, sp);  // Save to MSP
    *ptr++ = b(2);
    *ptr++ = mov_reg_to_simd(31, TS_S, 2, sp);  // Save to ISP

    /* Check if changing mode to user */
    *ptr++ = tbz(changed, SRB_S, 3);
    *ptr++ = mov_simd_to_reg(sp, 31, TS_S, 1);
    *ptr++ = b(5);
    *ptr++ = tbz(cc, SRB_M, 3);
    *ptr++ = mov_simd_to_reg(sp, 31, TS_S, 3);  // Load MSP
    *ptr++ = b(2);
    *ptr++ = mov_simd_to_reg(sp, 31, TS_S, 2);  // Load ISP

    /* Advance PC */
    *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_words + 1));

    // Check if IPL is less than 6. If yes, enable ARM interrupts
    *ptr++ = and_immed(changed, cc, 3, 32 - SRB_IPL);
    *ptr++ = cmp_immed(changed, 5 << SRB_IPL);
    *ptr++ = b_cc(A64_CC_GT, 3);
    *ptr++ = msr_imm(3, 7, 7); // Enable interrupts
    *ptr++ = b(2);
    *ptr++ = msr_imm(3, 6, 7); // Mask interrupts

    *tmpptr = b(ptr - tmpptr);

    *ptr++ = INSN_TO_LE(0xffffffff);

    RA_FreeARMRegister(&ptr, src);
    RA_FreeARMRegister(&ptr, orig);
    RA_FreeARMRegister(&ptr, changed);

    (*m68k_ptr) += ext_words;

    return ptr;
}

static uint32_t *EMIT_MOVEtoCCR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;

    uint8_t ext_words = 0;
    uint8_t src = 0xff;
    uint8_t cc = RA_ModifyCC(&ptr);

    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
    
    if ((opcode & 0x38) == 0) /* Dn direct */
    {
        uint8_t src_mod = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_reg(src_mod, src);
        src = src_mod;
    }
    *ptr++ = rbit(0, src);
    *ptr++ = bfxil(src, 0, 30, 2);
    *ptr++ = bfi(cc, src, 0, 5);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));

    RA_FreeARMRegister(&ptr, src);
    
    (*m68k_ptr) += ext_words;

    return ptr;
}

static uint32_t *EMIT_EXT(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint8_t update_mask;
    uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
    uint8_t tmp = reg;
    RA_SetDirtyM68kRegister(&ptr, opcode & 7);
    uint8_t mode = (opcode >> 6) & 7;

    /* 
        If current instruction is byte-to-word and subsequent is word-to-long on the same register,
        then combine both to extb.l 
    */

    if ((mode == 2) && (opcode ^ cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0])) == 0x40) {
        (*m68k_ptr)++;
        mode = 7;
        (*insn_consumed)++;
        ptr = EMIT_AdvancePC(ptr, 2);
    } 

    /* Get update mask at this point first, otherwise the mask would suggest that flag change is not necessary */
    update_mask = M68K_GetSRMask(*m68k_ptr - 1);

    switch (mode)
    {
        case 2: /* Byte to Word */
            tmp = RA_AllocARMRegister(&ptr);

            *ptr++ = sxtb(tmp, reg);
            *ptr++ = bfi(reg, tmp, 0, 16);
            break;
        case 3: /* Word to Long */
            *ptr++ = sxth(reg, reg);
            break;
        case 7: /* Byte to Long */
            *ptr++ = sxtb(reg, reg);
            break;
    }

    ptr = EMIT_AdvancePC(ptr, 2);

    if (update_mask)
    {
        *ptr++ = cmp_reg(tmp, 31, LSL, 0);

        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
    }
    if (tmp != reg)
        RA_FreeARMRegister(&ptr, tmp);

    return ptr;
}

static uint32_t *EMIT_LINK32(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    uint8_t sp;
    uint8_t displ;
    uint8_t reg;
    int32_t offset = (cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]) << 16) | cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[1]);

    displ = RA_AllocARMRegister(&ptr);
    *ptr++ = movw_immed_u16(displ, offset & 0xffff);
    *ptr++ = movt_immed_u16(displ, (offset >> 16) & 0xffff);
    sp = RA_MapM68kRegister(&ptr, 15);
    if (8 + (opcode & 7) == 15) {
        reg = RA_CopyFromM68kRegister(&ptr, 8 + (opcode & 7));
        *ptr++ = sub_immed(reg, reg, 4);
    }
    else {
        reg = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
    }
    *ptr++ = str_offset_preindex(sp, reg, -4);  /* SP = SP - 4; An -> (SP) */
    *ptr++ = mov_reg(reg, sp);
    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));

    *ptr++ = add_reg(sp, sp, displ, LSL, 0);

    RA_SetDirtyM68kRegister(&ptr, 15);

    (*m68k_ptr)+=2;

    ptr = EMIT_AdvancePC(ptr, 6);
    RA_FreeARMRegister(&ptr, displ);
    RA_FreeARMRegister(&ptr, reg);
    return ptr;
}

static uint32_t *EMIT_LINK16(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    uint8_t sp;
    uint8_t displ;
    uint8_t reg;
    int16_t offset = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);

    displ = RA_AllocARMRegister(&ptr);

    sp = RA_MapM68kRegister(&ptr, 15);
    if (8 + (opcode & 7) == 15) {
        reg = RA_CopyFromM68kRegister(&ptr, 8 + (opcode & 7));
        *ptr++ = sub_immed(reg, reg, 4);
    }
    else {
        reg = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
    }
    *ptr++ = str_offset_preindex(sp, reg, -4);  /* SP = SP - 4; An -> (SP) */
    *ptr++ = mov_reg(reg, sp);
    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));

    if (offset > 0 && offset < 4096)
    {
        *ptr++ = add_immed(sp, sp, offset);
    }
    else if (offset < 0 && offset > -4096)
    {
        *ptr++ = sub_immed(sp, sp, -offset);
    }
    else if (offset != 0)
    {
        *ptr++ = mov_immed_u16(displ, offset, 0);
        if (offset < 0)
            *ptr++ = movk_immed_u16(displ, 0xffff, 1);
        *ptr++ = add_reg(sp, sp, displ, LSL, 0);
    }
    
    RA_SetDirtyM68kRegister(&ptr, 15);

    (*m68k_ptr)++;

    ptr = EMIT_AdvancePC(ptr, 4);
    RA_FreeARMRegister(&ptr, displ);
    RA_FreeARMRegister(&ptr, reg);

    return ptr;
}

static uint32_t *EMIT_SWAP(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
    RA_SetDirtyM68kRegister(&ptr, opcode & 7);

    *ptr++ = ror(reg, reg, 16);

    ptr = EMIT_AdvancePC(ptr, 2);

    if (update_mask)
    {
        *ptr++ = cmn_reg(31, reg, LSL, 0);
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
    }

    return ptr;
}

static uint32_t *EMIT_ILLEGAL(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    (void)opcode;
    (void)m68k_ptr;

    /* Illegal generates exception. Always */
    ptr = EMIT_FlushPC(ptr);
    ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
    *ptr++ = INSN_TO_LE(0xffffffff);

    return ptr;
}

static uint32_t *EMIT_BKPT(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    (void)opcode;
    (void)m68k_ptr;

    /* Illegal generates exception. Always */
    ptr = EMIT_AdvancePC(ptr, 2);
    ptr = EMIT_FlushPC(ptr);
    ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
    *ptr++ = INSN_TO_LE(0xffffffff);

    return ptr;
}

static uint32_t *EMIT_TRAP(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    (void)m68k_ptr;

    ptr = EMIT_AdvancePC(ptr, 2);
    ptr = EMIT_FlushPC(ptr);

    ptr = EMIT_Exception(ptr, VECTOR_INT_TRAP(opcode & 15), 0);

    *ptr++ = INSN_TO_LE(0xffffffff);

    return ptr;
}

static uint32_t *EMIT_UNLK(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    uint8_t sp;
    uint8_t reg;

    (void)m68k_ptr;

    sp = RA_MapM68kRegisterForWrite(&ptr, 15);
    reg = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));

    *ptr++ = mov_reg(sp, reg);
    *ptr++ = ldr_offset_postindex(sp, reg, 4);

    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
    RA_SetDirtyM68kRegister(&ptr, 15);

    ptr = EMIT_AdvancePC(ptr, 2);

    return ptr;
}

static uint32_t *EMIT_RESET(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    (void)opcode;
    (void)m68k_ptr;

    uint32_t *tmp;
    uint32_t *tmp2;
    ptr = EMIT_FlushPC(ptr);
    uint8_t cc = RA_ModifyCC(&ptr);

    /* Test if supervisor mode is active */
    *ptr++ = ands_immed(31, cc, 1, 32 - SRB_S);

    /* Branch to exception if not in supervisor */
    *ptr++ = b_cc(A64_CC_EQ, 3);
    *ptr++ = add_immed(REG_PC, REG_PC, 2);
    tmp = ptr;
    *ptr++ = b_cc(A64_CC_AL, 10);

    /* No supervisor. Update USP, generate exception */
    ptr = EMIT_Exception(ptr, VECTOR_PRIVILEGE_VIOLATION, 0);

    tmp2 = ptr;
    *ptr++ = b_cc(A64_CC_AL, 0);

    *tmp = b_cc(A64_CC_AL, ptr - tmp);

#ifdef PISTORM

    void do_reset();

    union {
        uint64_t u64;
        uint16_t u16[4];
    } u;

    u.u64 = (uintptr_t)do_reset;

    *ptr++ = stp64_preindex(31, 0, 1, -256);
    for (int i=2; i < 30; i += 2)
        *ptr++ = stp64(31, i, i+1, i*8);
    *ptr++ = str64_offset(31, 30, 240);

    *ptr++ = mov64_immed_u16(1, u.u16[3], 0);
    *ptr++ = movk64_immed_u16(1, u.u16[2], 1);
    *ptr++ = movk64_immed_u16(1, u.u16[1], 2);
    *ptr++ = movk64_immed_u16(1, u.u16[0], 3);

    *ptr++ = blr(1);

    for (int i=2; i < 30; i += 2)
        *ptr++ = ldp64(31, i, i+1, i*8);
    *ptr++ = ldr64_offset(31, 30, 240);
    *ptr++ = ldp64_postindex(31, 0, 1, 256);

#endif

    *tmp2 = b_cc(A64_CC_AL, ptr - tmp2);

    *ptr++ = (uint32_t)(uintptr_t)tmp2;
    *ptr++ = 1;
    *ptr++ = 0;
    *ptr++ = INSN_TO_LE(0xfffffffe);
    *ptr++ = INSN_TO_LE(0xffffffff);

    return ptr;
}

static uint32_t *EMIT_NOP(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    (void)opcode;
    (void)m68k_ptr;

    *ptr++ = dsb_sy();
    ptr = EMIT_AdvancePC(ptr, 2);
    ptr = EMIT_FlushPC(ptr);

    return ptr;
}

static uint32_t *EMIT_STOP(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    (void)opcode;

    uint32_t *tmpptr;
    uint16_t new_sr = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]) & 0xf71f;
    uint8_t changed = RA_AllocARMRegister(&ptr);
    uint8_t orig = RA_AllocARMRegister(&ptr);
    uint8_t cc = RA_ModifyCC(&ptr);
    uint8_t sp = RA_MapM68kRegister(&ptr, 15);

    RA_SetDirtyM68kRegister(&ptr, 15);

    ptr = EMIT_FlushPC(ptr);

    /* Swap C and V in new SR */
    if ((new_sr & 3) != 0 && (new_sr & 3) < 3)
    {
        new_sr ^= 3;
    }

    /* If supervisor is not active, put an exception here */
    tmpptr = ptr;
    ptr++;
    ptr = EMIT_Exception(ptr, VECTOR_PRIVILEGE_VIOLATION, 0);
    *tmpptr = tbnz(cc, SRB_S, 1 + ptr - tmpptr);
    tmpptr = ptr;
    ptr++;

    cc = RA_ModifyCC(&ptr);

    /* Put new value into SR, check what has changed */
    *ptr++ = mov_reg(orig, cc);
    *ptr++ = mov_immed_u16(cc, new_sr, 0);
    *ptr++ = eor_reg(changed, orig, cc, LSL, 0);

    /* Perform eventual stack switch */

    /* If neither S nor M changed, go further */
    *ptr++ = ands_immed(31, changed, 2, 32 - SRB_M);
    *ptr++ = b_cc(A64_CC_EQ, 12);

    /* S or M changed. First of all, store stack pointer to either ISP or MSP */
    *ptr++ = tbz(orig, SRB_M, 3);
    *ptr++ = mov_reg_to_simd(31, TS_S, 3, sp);  // Save to MSP
    *ptr++ = b(2);
    *ptr++ = mov_reg_to_simd(31, TS_S, 2, sp);  // Save to ISP

    /* Check if changing mode to user */
    *ptr++ = tbz(changed, SRB_S, 3);
    *ptr++ = mov_simd_to_reg(sp, 31, TS_S, 1);
    *ptr++ = b(5);
    *ptr++ = tbz(cc, SRB_M, 3);
    *ptr++ = mov_simd_to_reg(sp, 31, TS_S, 3);  // Load MSP
    *ptr++ = b(2);
    *ptr++ = mov_simd_to_reg(sp, 31, TS_S, 2);  // Load ISP

    /* Now do what stop does - wait for interrupt */
    *ptr++ = add_immed(REG_PC, REG_PC, 4);

    // Check if IPL is less than 6. If yes, enable ARM interrupts
    *ptr++ = and_immed(changed, cc, 3, 32 - SRB_IPL);
    *ptr++ = cmp_immed(changed, 5 << SRB_IPL);
    *ptr++ = b_cc(A64_CC_GT, 3);
    *ptr++ = msr_imm(3, 7, 7); // Enable interrupts
    *ptr++ = b(2);
    *ptr++ = msr_imm(3, 6, 7); // Mask interrupts

#ifndef PISTORM
    /* Non pistorm machines wait for interrupt only */
    *ptr++ = wfi();
#else
    uint8_t tmpreg = RA_AllocARMRegister(&ptr);
    uint8_t ctx = RA_GetCTX(&ptr);
    uint32_t *start, *end;

    // Don't wait for event if IRQ is already pending
    *ptr++ = ldr_offset(ctx, tmpreg, __builtin_offsetof(struct M68KState, INT));
    *ptr++ = cbnz(tmpreg, 4);

    start = ptr;

    /* PiStorm waits for event and checks INT - aggregate of ~IPL0 and ARM */
    *ptr++ = wfe();
    *ptr++ = ldr_offset(ctx, tmpreg, __builtin_offsetof(struct M68KState, INT));
    end = ptr;
    *ptr++ = cbz(tmpreg, start - end);

    RA_FreeARMRegister(&ptr, tmpreg);
#endif

    *tmpptr = b(ptr - tmpptr);

    *ptr++ = INSN_TO_LE(0xffffffff);

    RA_FreeARMRegister(&ptr, changed);
    RA_FreeARMRegister(&ptr, orig);

    (*m68k_ptr) += 1;

    return ptr;
}

static uint32_t *EMIT_RTE(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    (void)opcode;
    (void)m68k_ptr;

    uint8_t tmp = 1; //RA_AllocARMRegister(&ptr);
    uint8_t sp = RA_MapM68kRegister(&ptr, 15);
    uint8_t cc = RA_ModifyCC(&ptr);
    uint8_t changed = 2; //RA_AllocARMRegister(&ptr);
    uint8_t orig = 3; //RA_AllocARMRegister(&ptr);
    uint32_t *tmpptr;
    uint32_t *branch_privilege;
    uint32_t *branch_format;

    RA_SetDirtyM68kRegister(&ptr, 15);

    ptr = EMIT_FlushPC(ptr);

    // First check if supervisor mode
    /* If supervisor is not active, put an exception here */
    branch_privilege = ptr;
    ptr++;
    ptr = EMIT_Exception(ptr, VECTOR_PRIVILEGE_VIOLATION, 0);
    *branch_privilege = tbnz(cc, SRB_S, 1 + ptr - branch_privilege);
    branch_privilege = ptr;
    ptr++;

    // Now check frame format
    *ptr++ = ldrh_offset(sp, tmp, 6);
    *ptr++ = lsr(tmp, tmp, 12);

    // Is format valid?
    *ptr++ = cmp_immed(tmp, 2);
    tmpptr = ptr;
    // Forat 2 is supported, go further
    *ptr++ = b_cc(A64_CC_EQ, 0);
    *ptr++ = cmp_immed(tmp, 0);
    *ptr++ = b_cc(A64_CC_EQ, 0);

    ptr = EMIT_Exception(ptr, VECTOR_FORMAT_ERROR, 0);

    branch_format = ptr;
    *ptr++ = 0;

    // Patch both jumps to here
    *tmpptr = b_cc(A64_CC_EQ, ptr - tmpptr);
    tmpptr += 2;
    *tmpptr = b_cc(A64_CC_EQ, ptr - tmpptr);

    /* Fetch sr from stack */
    *ptr++ = ldrh_offset_postindex(sp, changed, 2);
    /* Reverse C and V */
    *ptr++ = rbit(orig, changed);
    *ptr++ = bfxil(changed, orig, 30, 2);
    /* Fetch PC from stack, advance sp so that format word is skipped */
    *ptr++ = ldr_offset_postindex(sp, REG_PC, 6);

    /* In case of format 2, skip subsequent longword on stack */
    *ptr++ = cmp_immed(tmp, 2);
    *ptr++ = b_cc(A64_CC_NE, 2);
    *ptr++ = add_immed(sp, sp, 4);

    cc = RA_ModifyCC(&ptr);

    /* Use two EORs to generate changed mask and update SR */
    *ptr++ = mov_reg(orig, cc);
    *ptr++ = eor_reg(changed, changed, cc, LSL, 0);
    *ptr++ = eor_reg(cc, changed, cc, LSL, 0);       

    /* Now since stack is cleaned up, perform eventual stack switch */
    /* If neither S nor M changed, go further */
    *ptr++ = ands_immed(31, changed, 2, 32 - SRB_M);
    *ptr++ = b_cc(A64_CC_EQ, 12);

    /* S or M changed. First of all, store stack pointer to either ISP or MSP */
    *ptr++ = tbz(orig, SRB_M, 3);
    *ptr++ = mov_reg_to_simd(31, TS_S, 3, sp);  // Save to MSP
    *ptr++ = b(2);
    *ptr++ = mov_reg_to_simd(31, TS_S, 2, sp);  // Save to ISP

    /* Check if changing mode to user */
    *ptr++ = tbz(changed, SRB_S, 3);
    *ptr++ = mov_simd_to_reg(sp, 31, TS_S, 1);
    *ptr++ = b(5);
    *ptr++ = tbz(cc, SRB_M, 3);
    *ptr++ = mov_simd_to_reg(sp, 31, TS_S, 3);  // Load MSP
    *ptr++ = b(2);
    *ptr++ = mov_simd_to_reg(sp, 31, TS_S, 2);  // Load ISP

    // Check if IPL is less than 6. If yes, enable ARM interrupts
    *ptr++ = and_immed(changed, cc, 3, 32 - SRB_IPL);
    *ptr++ = cmp_immed(changed, 5 << SRB_IPL);
    *ptr++ = b_cc(A64_CC_GT, 3);
    *ptr++ = msr_imm(3, 7, 7); // Enable interrupts
    *ptr++ = b(2);
    *ptr++ = msr_imm(3, 6, 7); // Mask interrupts

    *branch_privilege = b(ptr - branch_privilege);
    *branch_format = b(ptr - branch_format);

    // Instruction always breaks translation
    *ptr++ = INSN_TO_LE(0xffffffff);

    RA_FreeARMRegister(&ptr, tmp);
    RA_FreeARMRegister(&ptr, changed);
    RA_FreeARMRegister(&ptr, orig);

    return ptr;
}

static uint32_t *EMIT_RTD(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    (void)opcode;
    (void)m68k_ptr;

    uint8_t tmp = RA_AllocARMRegister(&ptr);
    uint8_t tmp2 = RA_AllocARMRegister(&ptr);
    uint8_t sp = RA_MapM68kRegister(&ptr, 15);
    int16_t addend = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);

    /* Fetch return address from stack */
    *ptr++ = ldr_offset_postindex(sp, tmp2, 4);

    if (addend > -4096 && addend < 4096)
    {
        if (addend < 0)
        {
            *ptr++ = sub_immed(sp, sp, -addend);
        }
        else
        {
            *ptr++ = add_immed(sp, sp, addend);
        }
    }
    else
    {
        if (addend < 0)
        {
            *ptr++ = movn_immed_u16(tmp, -addend - 1, 0);
        }
        else
        {
            *ptr++ = mov_immed_u16(tmp, addend, 0);
        }
        *ptr++ = add_reg(sp, sp, tmp, LSL, 0);
    }

    ptr = EMIT_ResetOffsetPC(ptr);
    *ptr++ = mov_reg(REG_PC, tmp2);
    RA_SetDirtyM68kRegister(&ptr, 15);
    *ptr++ = INSN_TO_LE(0xffffffff);
    RA_FreeARMRegister(&ptr, tmp);
    RA_FreeARMRegister(&ptr, tmp2);

    return ptr;
}

static uint32_t *EMIT_RTS(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    (void)opcode;

    uint8_t sp = RA_MapM68kRegister(&ptr, 15);

    /* Fetch return address from stack */
    *ptr++ = ldr_offset_postindex(sp, REG_PC, 4);
    ptr = EMIT_ResetOffsetPC(ptr);
    RA_SetDirtyM68kRegister(&ptr, 15);

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
        uint8_t reg = RA_AllocARMRegister(&ptr);
        uint32_t *tmp;
        uint32_t ret = (uint32_t)(uintptr_t)ret_addr;
        *ptr++ = mov_immed_u16(reg, ret & 0xffff, 0);
        *ptr++ = movk_immed_u16(reg, ret >> 16, 1);
        *ptr++ = cmp_reg(reg, REG_PC, LSL, 0);
        tmp = ptr;
        *ptr++ = b_cc(ARM_CC_EQ, 0);

        *m68k_ptr = ret_addr;

        RA_FreeARMRegister(&ptr, reg);

        *tmp = b_cc(ARM_CC_EQ, ptr - tmp);
        *ptr++ = (uint32_t)(uintptr_t)tmp;
        *ptr++ = 1;
        *ptr++ = 0;
        *ptr++ = INSN_TO_LE(0xfffffffe);
    }
    else
        *ptr++ = INSN_TO_LE(0xffffffff);

    return ptr;
}

static uint32_t *EMIT_TRAPV(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    (void)opcode;

    uint8_t cc = RA_GetCC(&ptr);
    uint32_t *tmpptr;
    ptr = EMIT_AdvancePC(ptr, 2);
    ptr = EMIT_FlushPC(ptr);

    *ptr++ = ands_immed(31, cc, 1, 32 - SRB_Valt);
    tmpptr = ptr;
    *ptr++ = b_cc(A64_CC_EQ, 0);
    
    ptr = EMIT_Exception(ptr, VECTOR_TRAPcc, 2, (uint32_t)(uintptr_t)(*m68k_ptr - 1));

    *tmpptr = b_cc(A64_CC_EQ, ptr - tmpptr);
    *ptr++ = (uint32_t)(uintptr_t)tmpptr;
    *ptr++ = 1;
    *ptr++ = 0;
    *ptr++ = INSN_TO_LE(0xfffffffe);

    return ptr;
}

static uint32_t *EMIT_RTR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    (void)opcode;
    (void)m68k_ptr;

    uint8_t tmp = RA_AllocARMRegister(&ptr);
    uint8_t mask = RA_AllocARMRegister(&ptr);
    uint8_t sp = RA_MapM68kRegister(&ptr, 15);

    /* Fetch status byte from stack */
    *ptr++ = ldrh_offset_postindex(sp, tmp, 2);
    /* Reverse C and V */
    *ptr++ = rbit(0, tmp);
    *ptr++ = bfxil(tmp, 0, 30, 2);
    /* Insert XNZCV into SR */
    uint8_t cc = RA_ModifyCC(&ptr);
    *ptr++ = bfi(cc, tmp, 0, 5);

    /* Fetch return address from stack */
    *ptr++ = ldr_offset_postindex(sp, REG_PC, 4);
    ptr = EMIT_ResetOffsetPC(ptr);
    RA_SetDirtyM68kRegister(&ptr, 15);
    *ptr++ = INSN_TO_LE(0xffffffff);
    RA_FreeARMRegister(&ptr, mask);
    RA_FreeARMRegister(&ptr, tmp);

    return ptr;
}

static uint32_t *EMIT_MOVEC(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;

    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    uint8_t dr = opcode & 1;
    uint8_t reg = RA_MapM68kRegister(&ptr, opcode2 >> 12);
    uint8_t ctx = RA_GetCTX(&ptr);
    uint8_t cc = RA_ModifyCC(&ptr);
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

    (*m68k_ptr) += 1;
    ptr = EMIT_FlushPC(ptr);

    /* If supervisor is not active, put an exception here */
    tmpptr = ptr;
    ptr++;
    ptr = EMIT_Exception(ptr, VECTOR_PRIVILEGE_VIOLATION, 0);
    *tmpptr = tbnz(cc, SRB_S, 1 + ptr - tmpptr);
    tmpptr = ptr;
    ptr++;

    if (dr)
    {
        switch (opcode2 & 0xfff)
        {
            case 0x000: // SFC
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = and_immed(tmp, reg, 3, 0);
                *ptr++ = strb_offset(ctx, tmp, __builtin_offsetof(struct M68KState, SFC));
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x001: // DFC
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = and_immed(tmp, reg, 3, 0);
                *ptr++ = strb_offset(ctx, tmp, __builtin_offsetof(struct M68KState, DFC));
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x800: // USP
                *ptr++ = mov_reg_to_simd(31, TS_S, 1, reg);
                break;
            case 0x801: // VBR
                *ptr++ = str_offset(ctx, reg, __builtin_offsetof(struct M68KState, VBR));
                break;
            case 0x002: // CACR
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = bic_immed(tmp, reg, 15, 0);
                *ptr++ = bic_immed(tmp, tmp, 15, 16);
                *ptr++ = mov_reg_to_simd(31, TS_S, 0, tmp);
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x803: // MSP
                sp = RA_MapM68kRegister(&ptr, 15);
                RA_SetDirtyM68kRegister(&ptr, 15);
                *ptr++ = tbz(cc, SRB_M, 2);
                *ptr++ = mov_reg(sp, reg);
                *ptr++ = mov_reg_to_simd(31, TS_S, 3, reg);
                break;
            case 0x804: // ISP
                sp = RA_MapM68kRegister(&ptr, 15);
                RA_SetDirtyM68kRegister(&ptr, 15);
                *ptr++ = tbnz(cc, SRB_M, 2);
                *ptr++ = mov_reg(sp, reg);
                *ptr++ = mov_reg_to_simd(31, TS_S, 2, reg);
                break;
            case 0x0ea: /* JITSCFTHRESH - Maximal number of JIT units for "soft" cache flush */
                *ptr++ = str_offset(ctx, reg, __builtin_offsetof(struct M68KState, JIT_SOFTFLUSH_THRESH));
                break;
            case 0x0eb: /* JITCTRL - JIT control register */
                *ptr++ = str_offset(ctx, reg, __builtin_offsetof(struct M68KState, JIT_CONTROL));
                break;
            case 0x0ed: /* DBGCTRL */
            {
                uint8_t tmp2 = RA_AllocARMRegister(&ptr);
                tmp = RA_AllocARMRegister(&ptr);
                u.u64 = (uintptr_t)&debug;
                *ptr++ = mov64_immed_u16(tmp, u.u16[3], 0);
                *ptr++ = movk64_immed_u16(tmp, u.u16[2], 1);
                *ptr++ = movk64_immed_u16(tmp, u.u16[1], 2);
                *ptr++ = movk64_immed_u16(tmp, u.u16[0], 3);
                *ptr++ = ubfx(tmp2, reg, 0, 2);
                *ptr++ = str_offset(tmp, tmp2, 0);
                
                if (_abs((intptr_t)&debug - (intptr_t)&disasm) < 255)
                {
                    int delta = (uintptr_t)&disasm - (uintptr_t)&debug;
                    *ptr++ = ubfx(tmp2, reg, 2, 1);
                    *ptr++ = stur_offset(tmp, tmp2, delta);
                }
                else
                {
                    u.u64 = (uintptr_t)&disasm;
                    *ptr++ = mov64_immed_u16(tmp, u.u16[3], 0);
                    *ptr++ = movk64_immed_u16(tmp, u.u16[2], 1);
                    *ptr++ = movk64_immed_u16(tmp, u.u16[1], 2);
                    *ptr++ = movk64_immed_u16(tmp, u.u16[0], 3);
                    *ptr++ = ubfx(tmp2, reg, 2, 1);
                    *ptr++ = str_offset(tmp, tmp2, 0);
                }
                RA_FreeARMRegister(&ptr, tmp);
                RA_FreeARMRegister(&ptr, tmp2);
                break;
            }
            case 0x0ee: /* DBGADDRLO */
                tmp = RA_AllocARMRegister(&ptr);
                u.u64 = (uintptr_t)&debug_range_min;
                *ptr++ = mov64_immed_u16(tmp, u.u16[3], 0);
                *ptr++ = movk64_immed_u16(tmp, u.u16[2], 1);
                *ptr++ = movk64_immed_u16(tmp, u.u16[1], 2);
                *ptr++ = movk64_immed_u16(tmp, u.u16[0], 3);
                *ptr++ = str_offset(tmp, reg, 0);
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x0ef: /* DBGADDRHI */
                tmp = RA_AllocARMRegister(&ptr);
                u.u64 = (uintptr_t)&debug_range_max;
                *ptr++ = mov64_immed_u16(tmp, u.u16[3], 0);
                *ptr++ = movk64_immed_u16(tmp, u.u16[2], 1);
                *ptr++ = movk64_immed_u16(tmp, u.u16[1], 2);
                *ptr++ = movk64_immed_u16(tmp, u.u16[0], 3);
                *ptr++ = str_offset(tmp, reg, 0);
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x1e0: /* JITCTRL2 - JIT second control register */
                *ptr++ = str_offset(ctx, reg, __builtin_offsetof(struct M68KState, JIT_CONTROL2));
                break;
            case 0x003: // TCR - write bits 15, 14, read all zeros for now
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = bic_immed(tmp, reg, 30, 16);
                *ptr++ = bic_immed(tmp, tmp, 1, 32 - 15); // Clear E bit, do not allow turning on MMU
                *ptr++ = strh_offset(ctx, tmp, __builtin_offsetof(struct M68KState, TCR));
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x004: // ITT0
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = movn_immed_u16(tmp, 0x1c9b, 0);
                *ptr++ = and_reg(tmp, tmp, reg, LSL, 0);
                *ptr++ = str_offset(ctx, tmp, __builtin_offsetof(struct M68KState, ITT0));
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x005: // ITT1
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = movn_immed_u16(tmp, 0x1c9b, 0);
                *ptr++ = and_reg(tmp, tmp, reg, LSL, 0);
                *ptr++ = str_offset(ctx, tmp, __builtin_offsetof(struct M68KState, ITT1));
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x006: // DTT0
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = movn_immed_u16(tmp, 0x1c9b, 0);
                *ptr++ = and_reg(tmp, tmp, reg, LSL, 0);
                *ptr++ = str_offset(ctx, tmp, __builtin_offsetof(struct M68KState, DTT0));
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x007: // DTT1
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = movn_immed_u16(tmp, 0x1c9b, 0);
                *ptr++ = and_reg(tmp, tmp, reg, LSL, 0);
                *ptr++ = str_offset(ctx, tmp, __builtin_offsetof(struct M68KState, DTT1));
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x805: // MMUSR
                *ptr++ = str_offset(ctx, reg, __builtin_offsetof(struct M68KState, MMUSR));
                break;
            case 0x806: // URP
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = bic_immed(tmp, reg, 9, 0);
                *ptr++ = str_offset(ctx, tmp, __builtin_offsetof(struct M68KState, URP));
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x807: // SRP
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = bic_immed(tmp, reg, 9, 0);
                *ptr++ = str_offset(ctx, tmp, __builtin_offsetof(struct M68KState, SRP));
                RA_FreeARMRegister(&ptr, tmp);
                break;
            default:
                ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
                illegal = 1;
                break;
        }
    }
    else
    {
        switch (opcode2 & 0xfff)
        {
            case 0x000: // SFC
                *ptr++ = ldrb_offset(ctx, reg, __builtin_offsetof(struct M68KState, SFC));
                break;
            case 0x001: // DFC
                *ptr++ = ldrb_offset(ctx, reg, __builtin_offsetof(struct M68KState, DFC));
                break;
            case 0x800: // USP
                *ptr++ = mov_simd_to_reg(reg, 31, TS_S, 1);
                break;
            case 0x801: // VBR
                *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, VBR));
                break;
            case 0x002: // CACR
                *ptr++ = mov_simd_to_reg(reg, 31, TS_S, 0);
                break;
            case 0x803: // MSP
                sp = RA_MapM68kRegister(&ptr, 15);
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = tst_immed(cc, 1, 32 - SRB_M);
                *ptr++ = mov_simd_to_reg(tmp, 31, TS_S, 3);
                *ptr++ = csel(reg, sp, tmp, A64_CC_NE);
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x804: // ISP
                sp = RA_MapM68kRegister(&ptr, 15);
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = tst_immed(cc, 1, 32 - SRB_M);
                *ptr++ = mov_simd_to_reg(tmp, 31, TS_S, 2);
                *ptr++ = csel(reg, sp, tmp, A64_CC_EQ);
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x0e0: /* CNTFRQ - speed of counter clock in Hz */
                *ptr++ = mrs(reg, 3, 3, 14, 0, 0);
                break;
            case 0x0e1: /* CNTVALLO - lower 32 bits of the counter */
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = mrs(tmp, 3, 3, 14, 0, 1);
                *ptr++ = mov_reg(reg, tmp);
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x0e2: /* CNTVALHI - higher 32 bits of the counter */
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = mrs(tmp, 3, 3, 14, 0, 1);
                *ptr++ = lsr64(reg, tmp, 32);
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x0e3: /* INSNCNTLO - lower 32 bits of m68k instruction counter */
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = mov_simd_to_reg(tmp, 30, TS_D, 0);
                *ptr++ = add64_immed(tmp, tmp, insn_count & 0xfff);
                if (insn_count & 0xfff000)
                    *ptr++ = add64_immed_lsl12(tmp, tmp, insn_count >> 12);
                *ptr++ = mov_reg(reg, tmp);
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x0e4: /* INSNCNTHI - higher 32 bits of m68k instruction counter */
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = mov_simd_to_reg(tmp, 30, TS_D, 0);
                *ptr++ = add64_immed(tmp, tmp, insn_count & 0xfff);
                if (insn_count & 0xfff000)
                    *ptr++ = add64_immed_lsl12(tmp, tmp, insn_count >> 12);
                *ptr++ = lsr64(reg, tmp, 32);
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x0e5: /* ARMCNTLO - lower 32 bits of ARM instruction counter */
                {
                    uint8_t hosttmp = RA_AllocARMRegister(&ptr);
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = mov_simd_to_reg(hosttmp, 28, TS_D, 0);
                    *ptr++ = mrs(tmp, 3, 3, 9, 13, 0);
                    *ptr++ = sub64_reg(tmp, tmp, hosttmp, LSL, 0);
                    *ptr++ = mov_reg(reg, tmp);
                    RA_FreeARMRegister(&ptr, tmp);
                    RA_FreeARMRegister(&ptr, hosttmp);
                }
                break;
            case 0x0e6: /* ARMCNTHI - higher 32 bits of ARM instruction counter */
                {
                    uint8_t hosttmp = RA_AllocARMRegister(&ptr);
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = mov_simd_to_reg(hosttmp, 28, TS_D, 0);
                    *ptr++ = mrs(tmp, 3, 3, 9, 13, 0);
                    *ptr++ = sub64_reg(tmp, tmp, hosttmp, LSL, 0);
                    *ptr++ = lsr64(reg, tmp, 32);
                    RA_FreeARMRegister(&ptr, tmp);
                    RA_FreeARMRegister(&ptr, hosttmp);
                }
                break;
            case 0x0e7: /* JITSIZE - size of JIT cache, in bytes */
                *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, JIT_CACHE_TOTAL));
                break;
            case 0x0e8: /* JITFREE - free space in JIT cache, in bytes */
                *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, JIT_CACHE_FREE));
                break;
            case 0x0e9: /* JITCOUNT - Number of JIT units in cache */
                *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, JIT_UNIT_COUNT));
                break;
            case 0x0ea: /* JITSCFTHRESH - Maximal number of JIT units for "soft" cache flush */
                *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, JIT_SOFTFLUSH_THRESH));
                break;
            case 0x0eb: /* JITCTRL - JIT control register */
                *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, JIT_CONTROL));
                break;
            case 0x0ec: /* JITCMISS */
                *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, JIT_CACHE_MISS));
                break;
            case 0x0ed: /* DBGCTRL */
            {
                uint8_t tmp2 = RA_AllocARMRegister(&ptr);
                tmp = RA_AllocARMRegister(&ptr);
                u.u64 = (uintptr_t)&debug;
                *ptr++ = mov64_immed_u16(tmp, u.u16[3], 0);
                *ptr++ = movk64_immed_u16(tmp, u.u16[2], 1);
                *ptr++ = movk64_immed_u16(tmp, u.u16[1], 2);
                *ptr++ = movk64_immed_u16(tmp, u.u16[0], 3);

                *ptr++ = ldr_offset(tmp, tmp2, 0);
                *ptr++ = ubfx(reg, tmp2, 0, 2);

                if (_abs((intptr_t)&debug - (intptr_t)&disasm) < 255)
                {
                    int delta = (uintptr_t)&disasm - (uintptr_t)&debug;
                    *ptr++ = ldur_offset(tmp, tmp2, delta);
                }
                else
                {
                    u.u64 = (uintptr_t)&disasm;
                    *ptr++ = mov64_immed_u16(tmp, u.u16[3], 0);
                    *ptr++ = movk64_immed_u16(tmp, u.u16[2], 1);
                    *ptr++ = movk64_immed_u16(tmp, u.u16[1], 2);
                    *ptr++ = movk64_immed_u16(tmp, u.u16[0], 3);
                    *ptr++ = ldr_offset(tmp, tmp2, 0);
                }
                *ptr++ = cbz(tmp2, 2);
                *ptr++ = orr_immed(reg, reg, 1, 30);

                RA_FreeARMRegister(&ptr, tmp);
                RA_FreeARMRegister(&ptr, tmp2);
                break;
            }
            case 0x0ee: /* DBGADDRLO */
                tmp = RA_AllocARMRegister(&ptr);
                u.u64 = (uintptr_t)&debug_range_min;
                *ptr++ = mov64_immed_u16(tmp, u.u16[3], 0);
                *ptr++ = movk64_immed_u16(tmp, u.u16[2], 1);
                *ptr++ = movk64_immed_u16(tmp, u.u16[1], 2);
                *ptr++ = movk64_immed_u16(tmp, u.u16[0], 3);
                *ptr++ = ldr_offset(tmp, reg, 0);
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x0ef: /* DBGADDRHI */
                tmp = RA_AllocARMRegister(&ptr);
                u.u64 = (uintptr_t)&debug_range_max;
                *ptr++ = mov64_immed_u16(tmp, u.u16[3], 0);
                *ptr++ = movk64_immed_u16(tmp, u.u16[2], 1);
                *ptr++ = movk64_immed_u16(tmp, u.u16[1], 2);
                *ptr++ = movk64_immed_u16(tmp, u.u16[0], 3);
                *ptr++ = ldr_offset(tmp, reg, 0);
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 0x1e0: /* JITCTRL2 - JIT second control register */
                *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, JIT_CONTROL2));
                break;
            case 0x003: // TCR - write bits 15, 14, read all zeros for now
                *ptr++ = ldrh_offset(ctx, reg, __builtin_offsetof(struct M68KState, TCR));
                break;
            case 0x004: // ITT0
                *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, ITT0));
                break;
            case 0x005: // ITT1
                *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, ITT1));
                break;
            case 0x006: // DTT0
                *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, DTT0));
                break;
            case 0x007: // DTT1
                *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, DTT1));
                break;
            case 0x805: // MMUSR
                *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, MMUSR));
                break;
            case 0x806: // URP
                *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, URP));
                break;
            case 0x807: // SRP
                *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, SRP));
                break;
            default:
                ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
                illegal = 1;
                break;
        }
        RA_SetDirtyM68kRegister(&ptr, opcode2 >> 12);
    }

    if (!illegal) {
        *ptr++ = add_immed(REG_PC, REG_PC, 4);
    }

    *tmpptr = b(ptr - tmpptr);

    *ptr++ = INSN_TO_LE(0xffffffff);

    return ptr;
}

static uint32_t *EMIT_MOVEUSP(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    (void)m68k_ptr;

    uint32_t *tmp;
    uint8_t cc = RA_ModifyCC(&ptr);
    uint8_t an = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));

    ptr = EMIT_FlushPC(ptr);

    /* If supervisor is not active, put an exception here */
    tmp = ptr;
    ptr++;
    ptr = EMIT_Exception(ptr, VECTOR_PRIVILEGE_VIOLATION, 0);
    *tmp = tbnz(cc, SRB_S, 1 + ptr - tmp);
    tmp = ptr;
    ptr++;

    if (opcode & 8)
    {
        *ptr++ = mov_simd_to_reg(an, 31, TS_S, 1);
        RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
    }
    else
    {
        *ptr++ = mov_reg_to_simd(31, TS_S, 1, an);
    }

    *ptr++ = add_immed(REG_PC, REG_PC, 2);

    *tmp = b(ptr - tmp);

    *ptr++ = INSN_TO_LE(0xffffffff);

    return ptr;
}

static uint32_t *EMIT_JSR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    uint8_t ext_words = 0;
    uint8_t ea = 0xff;
    uint8_t sp = 0xff;

    sp = RA_MapM68kRegister(&ptr, 15);
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &ea, opcode & 0x3f, (*m68k_ptr), &ext_words, 1, NULL);
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    ptr = EMIT_FlushPC(ptr);
    *ptr++ = str_offset_preindex(sp, REG_PC, -4);
    RA_SetDirtyM68kRegister(&ptr, 15);
    ptr = EMIT_ResetOffsetPC(ptr);
    *ptr++ = mov_reg(REG_PC, ea);
    (*m68k_ptr) += ext_words;
    RA_FreeARMRegister(&ptr, ea);
    *ptr++ = INSN_TO_LE(0xffffffff);

    return ptr;
}

static uint32_t *EMIT_JMP(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    uint8_t ext_words = 0;
    uint8_t ea = REG_PC;

    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &ea, opcode & 0x3f, (*m68k_ptr), &ext_words, 0, NULL);
    ptr = EMIT_ResetOffsetPC(ptr);
    (*m68k_ptr) += ext_words;
    RA_FreeARMRegister(&ptr, ea);
    *ptr++ = INSN_TO_LE(0xffffffff);

    return ptr;
}

static uint32_t *EMIT_NBCD(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;

    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t ext_words = 0;

    uint8_t ea = -1;
    uint8_t cc = RA_ModifyCC(&ptr);

    uint8_t hi = RA_AllocARMRegister(&ptr);
    uint8_t lo = RA_AllocARMRegister(&ptr);

    /* Dn mode */
    if ((opcode & 0x38) == 0)
    {
        uint8_t dn = RA_MapM68kRegister(&ptr, opcode & 7);

        *ptr++ = and_immed(lo, dn, 4, 0);
        *ptr++ = and_immed(hi, dn, 4, 28);
    }
    else
    {
        uint8_t t = RA_AllocARMRegister(&ptr);

        /* Load EA */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &ea, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

        /* -(An) mode? Decrease EA */
        if ((opcode & 0x38) == 0x20)
        {
            if ((opcode & 7) == 7) {
                *ptr++ = ldrb_offset_preindex(ea, t, -2);
            }
            else {
                *ptr++ = ldrb_offset_preindex(ea, t, -1);
            }
        }
        else {
            *ptr++ = ldrb_offset(ea, t, 0);
        }

        *ptr++ = and_immed(lo, t, 4, 0);
        *ptr++ = and_immed(hi, t, 4, 28);

        RA_FreeARMRegister(&ptr, t);
    }

    uint8_t tmp = RA_AllocARMRegister(&ptr);
    uint8_t result = RA_AllocARMRegister(&ptr);

    // Negate both loaded nibbles
    *ptr++ = sub_reg(lo, 31, lo, LSL, 0);
    *ptr++ = sub_reg(hi, 31, hi, LSL, 0);

    // If X was set decrement lower nibble
    *ptr++ = tbz(cc, SRB_X, 2);
    *ptr++ = sub_immed(lo, lo, 1);

    // Fix overflow in lower nibble
    *ptr++ = cmp_immed(lo, 10);
    *ptr++ = b_cc(A64_CC_CC, 2);
    *ptr++ = sub_immed(lo, lo, 6);

    *ptr++ = add_reg(result, lo, hi, LSL, 0);

    // Fix overflow in higher nibble
    *ptr++ = and_immed(tmp, result, 5, 28);
    *ptr++ = cmp_immed(tmp, 0xa0);
    *ptr++ = b_cc(A64_CC_CC, 2);
    *ptr++ = sub_immed(result, result, 0x60);

    if (update_mask & SR_XC) {
        
        cc = RA_ModifyCC(&ptr);

        switch (update_mask & SR_XC)
        {
            case SR_C:
                *ptr++ = bic_immed(cc, cc, 1, 32 - SRB_Calt);
                *ptr++ = orr_immed(tmp, cc, 1, 32 - SRB_Calt);
                break;
            case SR_X:
                *ptr++ = bic_immed(cc, cc, 1, 32 - SRB_X);
                *ptr++ = orr_immed(tmp, cc, 1, 32 - SRB_X);
                break;
            default:
                *ptr++ = mov_immed_u16(tmp, SR_XCalt, 0);
                *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);
                *ptr++ = orr_reg(tmp, cc, tmp, LSL, 0);
                break;
        }

        *ptr++ = csel(cc, tmp, cc, A64_CC_CS);
    }

    if (update_mask & SR_Z) {
        cc = RA_ModifyCC(&ptr);

        *ptr++ = ands_immed(31, result, 8, 0);
        *ptr++ = bic_immed(tmp, cc, 1, 32 - SRB_Z);
        *ptr++ = csel(cc, tmp, cc, A64_CC_NE);
    }

    /* Dn mode */
    if ((opcode & 0x38) == 0)
    {
        uint8_t dn = RA_MapM68kRegister(&ptr, opcode & 7);

        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        *ptr++ = bfi(dn, result, 0, 8);
    }
    else
    {
        /* (An)+ mode? Increase EA */
        if ((opcode & 0x38) == 0x18)
        {
            if ((opcode & 7) == 7) {
                *ptr++ = strb_offset_postindex(ea, result, 2);
            }
            else {
                *ptr++ = strb_offset_postindex(ea, result, 1);
            }
        }
        else {
            *ptr++ = strb_offset(ea, result, 0);
        }
    }

    RA_FreeARMRegister(&ptr, tmp);
    RA_FreeARMRegister(&ptr, hi);
    RA_FreeARMRegister(&ptr, lo);
    RA_FreeARMRegister(&ptr, result);
    RA_FreeARMRegister(&ptr, ea);

    (*m68k_ptr) += ext_words;
    ptr = EMIT_AdvancePC(ptr, 2*(ext_words + 1));

    return ptr;
}

static uint32_t *EMIT_PEA(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    uint8_t sp = 0xff;
    uint8_t ea = 0xff;
    uint8_t ext_words = 0;

    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &ea, opcode & 0x3f, (*m68k_ptr), &ext_words, 1, NULL);
    (*m68k_ptr) += ext_words;

    sp = RA_MapM68kRegister(&ptr, 15);
    RA_SetDirtyM68kRegister(&ptr, 15);

    *ptr++ = str_offset_preindex(sp, ea, -4);

    RA_FreeARMRegister(&ptr, sp);
    RA_FreeARMRegister(&ptr, ea);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));

    return ptr;
}

static uint32_t *EMIT_MOVEM(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    uint8_t dir = (opcode >> 10) & 1;
    uint8_t size = (opcode >> 6) & 1;
    uint16_t mask = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    uint8_t block_size = 0;
    uint8_t ext_words = 0;
    extern int debug;
    uint32_t *ptr_orig = ptr;

    (*m68k_ptr)++;

    ptr = EMIT_AdvancePC(ptr, 2);

    for (int i=0; i < 16; i++)
    {
        if (mask & (1 << i))
            block_size += size ? 4:2;
    }

    if (dir == 0)
    {
        uint8_t base = 0xff;

        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &base, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

        /* Pre-decrement mode? Decrease the base now */
        if ((opcode & 0x38) == 0x20)
        {
            uint8_t offset = 0;
            uint8_t tmp_base_reg = 0xff;

            RA_SetDirtyM68kRegister(&ptr, (opcode & 7) + 8);

            /* Check if base register is on the list */
            if (mask & (0x8000 >> ((opcode & 7) + 8)))
            {
                tmp_base_reg = RA_AllocARMRegister(&ptr);
                *ptr++ = sub_immed(tmp_base_reg, base, size ? 4 : 2);
            }

            /* In pre-decrement the register order is reversed */
            uint8_t rt1 = 0xff;

            for (int i=0; i < 16; i++)
            {
                if (mask & (0x8000 >> i))
                {
                    uint8_t reg = (i == ((opcode & 7) + 8) ? tmp_base_reg : RA_MapM68kRegister(&ptr, i));

                    if (size) {
                        if (rt1 == 0xff)
                            rt1 = reg;
                        else {
                            if (offset == 0)
                                *ptr++ = stp_preindex(base, rt1, reg, -block_size);
                            else
                                *ptr++ = stp(base, rt1, reg, offset);
                            offset += 8;
                            rt1 = 0xff;
                        }
                    }
                    else
                    {
                        if (offset == 0)
                            *ptr++ = strh_offset_preindex(base, reg, -block_size);
                        else
                            *ptr++ = strh_offset(base, reg, offset);

                        offset += 2;
                    }
                }
            }
            if (rt1 != 0xff) {
                if (offset == 0)
                    *ptr++ = str_offset_preindex(base, rt1, -block_size);
                else
                    *ptr++ = str_offset(base, rt1, offset);
            }

            RA_FreeARMRegister(&ptr, tmp_base_reg);
        }
        else
        {
            uint8_t offset = 0;
            uint8_t rt1 = 0xff;

            for (int i=0; i < 16; i++)
            {
                if (mask & (1 << i))
                {
                    uint8_t reg = RA_MapM68kRegister(&ptr, i);
                    if (size) {
                        if (rt1 == 0xff)
                            rt1 = reg;
                        else {
                            *ptr++ = stp(base, rt1, reg, offset);
                            offset += 8;
                            rt1 = 0xff;
                        }
                    }
                    else
                    {
                        *ptr++ = strh_offset(base, reg, offset);
                        offset += 2;
                    }
                }
            }
            if (rt1 != 0xff)
                *ptr++ = str_offset(base, rt1, offset);
        }

        RA_FreeARMRegister(&ptr, base);
    }
    else
    {
        uint8_t base = 0xff;
        uint8_t offset = 0;

        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &base, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

        uint8_t rt1 = 0xff;

        for (int i=0; i < 16; i++)
        {
            if (mask & (1 << i))
            {
                /* Keep base register high in LRU */
                if (((opcode & 0x38) == 0x18)) RA_MapM68kRegister(&ptr, (opcode & 7) + 8);

                uint8_t reg = RA_MapM68kRegisterForWrite(&ptr, i);
                if (size) {
                    if ((((opcode & 0x38) == 0x18) && (i == (opcode & 7) + 8))) {
                        /* If rt1 was set, flush it now and reset, skip the base register */
                        if (rt1 != 0xff) {
                            *ptr++ = ldr_offset(base, rt1, offset);
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
                            *ptr++ = ldp_postindex(base, rt1, reg, 8);
                        else 
                            *ptr++ = ldp(base, rt1, reg, offset);
                        offset += 8;
                        rt1 = 0xff;
                    }
                }
                else
                {
                    if (!(((opcode & 0x38) == 0x18) && (i == (opcode & 7) + 8)))
                        *ptr++ = ldrsh_offset(base, reg, offset);
                    offset += 2;
                }
            }
        }
        if (rt1 != 0xff) {
            *ptr++ = ldr_offset(base, rt1, offset);
        }

        /* Post-increment mode? Increase the base now */
        if ((opcode & 0x38) == 0x18 && !(block_size == 8 && size))
        {
            *ptr++ = add_immed(base, base, block_size);
            RA_SetDirtyM68kRegister(&ptr, (opcode & 7) + 8);
        }

        RA_FreeARMRegister(&ptr, base);
    }

    ptr = EMIT_AdvancePC(ptr, 2*(ext_words + 1));
    (*m68k_ptr) += ext_words;

    /* No opcode was emited? At least flush PC counter now */
    if (ptr == ptr_orig)
        ptr = EMIT_FlushPC(ptr);

    return ptr;
}

static uint32_t *EMIT_LEA(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;
    uint8_t dest = 0xff;
    uint8_t ea = 0xff;
    uint8_t ext_words = 0;

    /* On AArch64 we can always map destination reg for write - it is always there! */
    dest = RA_MapM68kRegisterForWrite(&ptr, 8 + ((opcode >> 9) & 7));

    /* Mode 2, (An) in case of LEA is a special case - just move the reg to destination */
    if ((opcode & 0x38) == 0x10)
    {
        uint8_t src = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
        *ptr++ = mov_reg(dest, src);
    }
    else
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, (*m68k_ptr), &ext_words, 1, NULL);

    (*m68k_ptr) += ext_words;

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    RA_FreeARMRegister(&ptr, ea);

    return ptr;
}

static uint32_t *EMIT_CHK(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)insn_consumed;

    uint32_t opcode_address = (uint32_t)(uintptr_t)((*m68k_ptr) - 1);
    uint8_t ext_words = 0;
    uint8_t dn = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    uint8_t src = -1;
    uint8_t tmpreg = RA_AllocARMRegister(&ptr);

    /* word operation */
    if (opcode & 0x80)
    {
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);
        *ptr++ = lsl(src, src, 16);
    }
    else
    {
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
    }

    uint8_t cc = RA_ModifyCC(&ptr);

    // Clear Z, V and C flags, set Z back if operand is zero
    *ptr++ = mov_immed_u16(tmpreg, SR_NCalt, 0);
    *ptr++ = bic_reg(cc, cc, tmpreg, LSL, 0);
    *ptr++ = tbz(src, 31, 2);
    *ptr++ = orr_immed(cc, cc, 1, 32 - SRB_N);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    ptr = EMIT_FlushPC(ptr);

    /* Check if Dn < 0 */
    if (opcode & 0x80)
        *ptr++ = adds_reg(31, 31, dn, LSL, 16); 
    else
        *ptr++ = adds_reg(31, 31, dn, LSL, 0); 

    /* Jump to exception generator if negative */
    *ptr++ = b_cc(A64_CC_MI, 5);
    
    /* Check if Dn > src */
    if (opcode & 0x80)
        *ptr++ = subs_reg(31, src, dn, LSL, 16);
    else
        *ptr++ = subs_reg(31, src, dn, LSL, 0);
    
    uint32_t *tmp = ptr;
    *ptr++ = b_cc(A64_CC_GE, 0);
    *ptr++ = bic_immed(cc, cc, 1, 31 & (32 - SRB_N));
    *ptr++ = b(2);

    *ptr++ = orr_immed(cc, cc, 1, 31 & (32 - SRB_N));

    ptr = EMIT_Exception(ptr, VECTOR_CHK, 2, opcode_address);

    RA_FreeARMRegister(&ptr, src);
    RA_FreeARMRegister(&ptr, tmpreg);

    *tmp = b_cc(A64_CC_GE, ptr - tmp);
    *ptr++ = (uint32_t)(uintptr_t)tmp;
    *ptr++ = 1;
    *ptr++ = 0;
    *ptr++ = INSN_TO_LE(0xfffffffe);

    return ptr;
}

static struct OpcodeDef InsnTable[4096] = {
    [00300 ... 00307] = { { .od_EmitMulti = EMIT_MOVEfromSR }, NULL, SR_ALL, 0, 1, 0, 2 },
    [00320 ... 00347] = { { .od_EmitMulti = EMIT_MOVEfromSR }, NULL, SR_ALL, 0, 1, 0, 2 },
    [00350 ... 00371] = { { .od_EmitMulti = EMIT_MOVEfromSR }, NULL, SR_ALL, 0, 1, 1, 2 },

    [01300 ... 01307] = { { .od_EmitMulti = EMIT_MOVEfromCCR }, NULL, SR_CCR, 0, 1, 0, 2 },
    [01320 ... 01347] = { { .od_EmitMulti = EMIT_MOVEfromCCR }, NULL, SR_CCR, 0, 1, 0, 2 },
    [01350 ... 01371] = { { .od_EmitMulti = EMIT_MOVEfromCCR }, NULL, SR_CCR, 0, 1, 1, 2 },

    [03300 ... 03307] = { { .od_EmitMulti = EMIT_MOVEtoSR }, NULL, SR_S, SR_ALL, 1, 0, 2 },
    [03320 ... 03347] = { { .od_EmitMulti = EMIT_MOVEtoSR }, NULL, SR_S, SR_ALL, 1, 0, 2 },
    [03350 ... 03374] = { { .od_EmitMulti = EMIT_MOVEtoSR }, NULL, SR_S, SR_ALL, 1, 1, 2 },

    [02300 ... 02307] = { { .od_EmitMulti = EMIT_MOVEtoCCR }, NULL, 0, SR_CCR, 1, 0, 2 },
    [02320 ... 02347] = { { .od_EmitMulti = EMIT_MOVEtoCCR }, NULL, 0, SR_CCR, 1, 0, 2 },
    [02350 ... 02374] = { { .od_EmitMulti = EMIT_MOVEtoCCR }, NULL, 0, SR_CCR, 1, 1, 2 },

    [04200 ... 04207] = { { .od_EmitMulti = EMIT_EXT }, NULL, 0, SR_NZVC, 1, 0, 0 },
    [04300 ... 04307] = { { .od_EmitMulti = EMIT_EXT }, NULL, 0, SR_NZVC, 1, 0, 0 },
    [04700 ... 04707] = { { .od_EmitMulti = EMIT_EXT }, NULL, 0, SR_NZVC, 1, 0, 0 },

    [04010 ... 04017] = { { .od_EmitMulti = EMIT_LINK32 }, NULL, 0, 0, 3, 0, 0 },
    [07120 ... 07127] = { { .od_EmitMulti = EMIT_LINK16 }, NULL, 0, 0, 2, 0, 0 },

    [04100 ... 04107] = { { .od_EmitMulti = EMIT_SWAP }, NULL, 0, SR_NZVC, 1, 0, 0 },
    [0xafc]           = { { .od_EmitMulti = EMIT_ILLEGAL }, NULL, SR_CCR, 0, 1, 0, 0 },
    [0xe40 ... 0xe4f] = { { .od_EmitMulti = EMIT_TRAP }, NULL, SR_CCR, 0, 1, 0, 0 },
    [07130 ... 07137] = { { .od_EmitMulti = EMIT_UNLK }, NULL, 0, 0, 1, 0, 0 },
    [0xe70]           = { { .od_EmitMulti = EMIT_RESET }, NULL, SR_S, 0, 1, 0, 0 },
    [0xe71]           = { { .od_EmitMulti = EMIT_NOP }, NULL, 0, 0, 1, 0, 0 },
    [0xe72]           = { { .od_EmitMulti = EMIT_STOP }, NULL, SR_S, SR_ALL, 2, 0, 0 },
    [0xe73]           = { { .od_EmitMulti = EMIT_RTE }, NULL, SR_S, SR_ALL, 1, 0, 0 },
    [0xe74]           = { { .od_EmitMulti = EMIT_RTD }, NULL, 0, 0, 2, 0, 0 },
    [0xe75]           = { { .od_EmitMulti = EMIT_RTS }, NULL, 0, 0, 1, 0, 0 },
    [0xe76]           = { { .od_EmitMulti = EMIT_TRAPV }, NULL, SR_CCR, 0, 1, 0, 0 },
    [0xe77]           = { { .od_EmitMulti = EMIT_RTR }, NULL, 0, SR_CCR, 1, 0, 0 },
    [0xe7a ... 0xe7b] = { { .od_EmitMulti = EMIT_MOVEC }, NULL, SR_S, 0, 2, 0, 4 },
    [0xe60 ... 0xe6f] = { { .od_EmitMulti = EMIT_MOVEUSP }, NULL, SR_S, 0, 1, 0, 4 },
    [04110 ... 04117] = { { .od_EmitMulti = EMIT_BKPT }, NULL, SR_ALL, 0, 1, 0, 0 },      // BKPT

    [07320 ... 07327] = { { .od_EmitMulti = EMIT_JMP }, NULL, 0, 0, 1, 0, 0 },
    [07350 ... 07373] = { { .od_EmitMulti = EMIT_JMP }, NULL, 0, 0, 1, 1, 0 },

    [07220 ... 07227] = { { .od_EmitMulti = EMIT_JSR }, NULL, 0, 0, 1, 0, 0 },
    [07250 ... 07273] = { { .od_EmitMulti = EMIT_JSR }, NULL, 0, 0, 1, 1, 0 },

    [00000 ... 00007] = { { .od_EmitMulti = EMIT_NEGX }, NULL, SR_XZ, SR_CCR, 1, 0, 1 },
    [00100 ... 00107] = { { .od_EmitMulti = EMIT_NEGX }, NULL, SR_XZ, SR_CCR, 1, 0, 2 },
    [00200 ... 00207] = { { .od_EmitMulti = EMIT_NEGX }, NULL, SR_XZ, SR_CCR, 1, 0, 4 },

    [00020 ... 00047] = { { .od_EmitMulti = EMIT_NEGX }, NULL, SR_XZ, SR_CCR, 1, 0, 1 },
    [00120 ... 00147] = { { .od_EmitMulti = EMIT_NEGX }, NULL, SR_XZ, SR_CCR, 1, 0, 2 },
    [00220 ... 00247] = { { .od_EmitMulti = EMIT_NEGX }, NULL, SR_XZ, SR_CCR, 1, 0, 4 },
    
    [00050 ... 00071] = { { .od_EmitMulti = EMIT_NEGX }, NULL, SR_XZ, SR_CCR, 1, 1, 1 },
    [00150 ... 00171] = { { .od_EmitMulti = EMIT_NEGX }, NULL, SR_XZ, SR_CCR, 1, 1, 2 },
    [00250 ... 00271] = { { .od_EmitMulti = EMIT_NEGX }, NULL, SR_XZ, SR_CCR, 1, 1, 4 },

    [01000 ... 01007] = { { .od_EmitMulti = EMIT_CLR }, NULL, 0, SR_NZVC, 1, 0, 1 },
    [01100 ... 01107] = { { .od_EmitMulti = EMIT_CLR }, NULL, 0, SR_NZVC, 1, 0, 2 },
    [01200 ... 01207] = { { .od_EmitMulti = EMIT_CLR }, NULL, 0, SR_NZVC, 1, 0, 4 },

    [01020 ... 01047] = { { .od_EmitMulti = EMIT_CLR }, NULL, 0, SR_NZVC, 1, 0, 1 },
    [01120 ... 01147] = { { .od_EmitMulti = EMIT_CLR }, NULL, 0, SR_NZVC, 1, 0, 2 },
    [01220 ... 01247] = { { .od_EmitMulti = EMIT_CLR }, NULL, 0, SR_NZVC, 1, 0, 4 },

    [01050 ... 01071] = { { .od_EmitMulti = EMIT_CLR }, NULL, 0, SR_NZVC, 1, 1, 1 },
    [01150 ... 01171] = { { .od_EmitMulti = EMIT_CLR }, NULL, 0, SR_NZVC, 1, 1, 2 },
    [01250 ... 01271] = { { .od_EmitMulti = EMIT_CLR }, NULL, 0, SR_NZVC, 1, 1, 4 },

    [02000 ... 02007] = { { .od_EmitMulti = EMIT_NEG }, NULL, 0, SR_CCR, 1, 0, 1 },
    [02100 ... 02107] = { { .od_EmitMulti = EMIT_NEG }, NULL, 0, SR_CCR, 1, 0, 2 },
    [02200 ... 02207] = { { .od_EmitMulti = EMIT_NEG }, NULL, 0, SR_CCR, 1, 0, 4 },

    [02020 ... 02047] = { { .od_EmitMulti = EMIT_NEG }, NULL, 0, SR_CCR, 1, 0, 1 },
    [02120 ... 02147] = { { .od_EmitMulti = EMIT_NEG }, NULL, 0, SR_CCR, 1, 0, 2 },
    [02220 ... 02247] = { { .od_EmitMulti = EMIT_NEG }, NULL, 0, SR_CCR, 1, 0, 4 },

    [02050 ... 02071] = { { .od_EmitMulti = EMIT_NEG }, NULL, 0, SR_CCR, 1, 1, 1 },
    [02150 ... 02171] = { { .od_EmitMulti = EMIT_NEG }, NULL, 0, SR_CCR, 1, 1, 2 },
    [02250 ... 02271] = { { .od_EmitMulti = EMIT_NEG }, NULL, 0, SR_CCR, 1, 1, 4 },

    [03000 ... 03007] = { { .od_EmitMulti = EMIT_NOT }, NULL, 0, SR_NZVC, 1, 0, 1 },
    [03100 ... 03107] = { { .od_EmitMulti = EMIT_NOT }, NULL, 0, SR_NZVC, 1, 0, 2 },
    [03200 ... 03207] = { { .od_EmitMulti = EMIT_NOT }, NULL, 0, SR_NZVC, 1, 0, 4 },

    [03020 ... 03047] = { { .od_EmitMulti = EMIT_NOT }, NULL, 0, SR_NZVC, 1, 0, 1 },
    [03120 ... 03147] = { { .od_EmitMulti = EMIT_NOT }, NULL, 0, SR_NZVC, 1, 0, 2 },
    [03220 ... 03247] = { { .od_EmitMulti = EMIT_NOT }, NULL, 0, SR_NZVC, 1, 0, 4 },

    [03050 ... 03071] = { { .od_EmitMulti = EMIT_NOT }, NULL, 0, SR_NZVC, 1, 1, 1 },
    [03150 ... 03171] = { { .od_EmitMulti = EMIT_NOT }, NULL, 0, SR_NZVC, 1, 1, 2 },
    [03250 ... 03271] = { { .od_EmitMulti = EMIT_NOT }, NULL, 0, SR_NZVC, 1, 1, 4 },

    [05000 ... 05007] = { { .od_EmitMulti = EMIT_TST }, NULL, 0, SR_NZVC, 1, 0, 1 },
    [05020 ... 05047] = { { .od_EmitMulti = EMIT_TST }, NULL, 0, SR_NZVC, 1, 0, 1 },
    [05050 ... 05074] = { { .od_EmitMulti = EMIT_TST }, NULL, 0, SR_NZVC, 1, 1, 1 },
    
    [05100 ... 05147] = { { .od_EmitMulti = EMIT_TST }, NULL, 0, SR_NZVC, 1, 0, 2 },
    [05150 ... 05174] = { { .od_EmitMulti = EMIT_TST }, NULL, 0, SR_NZVC, 1, 1, 2 },
    
    [05200 ... 05247] = { { .od_EmitMulti = EMIT_TST }, NULL, 0, SR_NZVC, 1, 0, 4 },
    [05250 ... 05274] = { { .od_EmitMulti = EMIT_TST }, NULL, 0, SR_NZVC, 1, 1, 4 },

    [04000 ... 04007] = { { .od_EmitMulti = EMIT_NBCD }, NULL, SR_XZ, SR_XZC, 1, 0, 1 },
    [04020 ... 04047] = { { .od_EmitMulti = EMIT_NBCD }, NULL, SR_XZ, SR_XZC, 1, 0, 1 },
    [04050 ... 04071] = { { .od_EmitMulti = EMIT_NBCD }, NULL, SR_XZ, SR_XZC, 1, 1, 1 },

    [04120 ... 04127] = { { .od_EmitMulti = EMIT_PEA }, NULL, 0, 0, 1, 0, 4 },
    [04150 ... 04173] = { { .od_EmitMulti = EMIT_PEA }, NULL, 0, 0, 1, 1, 4 },

    [05300 ... 05307] = { { .od_EmitMulti = EMIT_TAS }, NULL, 0, SR_NZVC, 1, 0, 1 },
    [05320 ... 05347] = { { .od_EmitMulti = EMIT_TAS }, NULL, 0, SR_NZVC, 1, 0, 1 },
    [05350 ... 05371] = { { .od_EmitMulti = EMIT_TAS }, NULL, 0, SR_NZVC, 1, 1, 1 },

    [06000 ... 06007] = { { .od_EmitMulti = EMIT_MUL_DIV_ }, NULL, 0, SR_NZVC, 2, 0, 4 },
    [06020 ... 06047] = { { .od_EmitMulti = EMIT_MUL_DIV_ }, NULL, 0, SR_NZVC, 2, 0, 4 },
    [06050 ... 06074] = { { .od_EmitMulti = EMIT_MUL_DIV_ }, NULL, 0, SR_NZVC, 2, 1, 4 },
    [06100 ... 06107] = { { .od_EmitMulti = EMIT_MUL_DIV_ }, NULL, 0, SR_NZVC, 2, 0, 4 },
    [06120 ... 06147] = { { .od_EmitMulti = EMIT_MUL_DIV_ }, NULL, 0, SR_NZVC, 2, 0, 4 },
    [06150 ... 06174] = { { .od_EmitMulti = EMIT_MUL_DIV_ }, NULL, 0, SR_NZVC, 2, 1, 4 },

    [04220 ... 04227] = { { .od_EmitMulti = EMIT_MOVEM }, NULL, 0, 0, 2, 0, 2 },
    [04320 ... 04327] = { { .od_EmitMulti = EMIT_MOVEM }, NULL, 0, 0, 2, 0, 4 },
    [04240 ... 04247] = { { .od_EmitMulti = EMIT_MOVEM }, NULL, 0, 0, 2, 0, 2 },
    [04340 ... 04347] = { { .od_EmitMulti = EMIT_MOVEM }, NULL, 0, 0, 2, 0, 4 },
    [04250 ... 04271] = { { .od_EmitMulti = EMIT_MOVEM }, NULL, 0, 0, 2, 1, 2 },
    [04350 ... 04371] = { { .od_EmitMulti = EMIT_MOVEM }, NULL, 0, 0, 2, 1, 4 },

    [06220 ... 06237] = { { .od_EmitMulti = EMIT_MOVEM }, NULL, 0, 0, 2, 0, 2 },
    [06320 ... 06337] = { { .od_EmitMulti = EMIT_MOVEM }, NULL, 0, 0, 2, 0, 4 },
    [06250 ... 06273] = { { .od_EmitMulti = EMIT_MOVEM }, NULL, 0, 0, 2, 1, 2 },
    [06350 ... 06373] = { { .od_EmitMulti = EMIT_MOVEM }, NULL, 0, 0, 2, 1, 4 },

    [00720 ... 00727] = { { .od_EmitMulti = EMIT_LEA }, NULL, 0, 0, 1, 0, 4 },
    [00750 ... 00773] = { { .od_EmitMulti = EMIT_LEA }, NULL, 0, 0, 1, 1, 4 },
    [01720 ... 01727] = { { .od_EmitMulti = EMIT_LEA }, NULL, 0, 0, 1, 0, 4 },
    [01750 ... 01773] = { { .od_EmitMulti = EMIT_LEA }, NULL, 0, 0, 1, 1, 4 },
    [02720 ... 02727] = { { .od_EmitMulti = EMIT_LEA }, NULL, 0, 0, 1, 0, 4 },
    [02750 ... 02773] = { { .od_EmitMulti = EMIT_LEA }, NULL, 0, 0, 1, 1, 4 },
    [03720 ... 03727] = { { .od_EmitMulti = EMIT_LEA }, NULL, 0, 0, 1, 0, 4 },
    [03750 ... 03773] = { { .od_EmitMulti = EMIT_LEA }, NULL, 0, 0, 1, 1, 4 },
    [04720 ... 04727] = { { .od_EmitMulti = EMIT_LEA }, NULL, 0, 0, 1, 0, 4 },
    [04750 ... 04773] = { { .od_EmitMulti = EMIT_LEA }, NULL, 0, 0, 1, 1, 4 },
    [05720 ... 05727] = { { .od_EmitMulti = EMIT_LEA }, NULL, 0, 0, 1, 0, 4 },
    [05750 ... 05773] = { { .od_EmitMulti = EMIT_LEA }, NULL, 0, 0, 1, 1, 4 },
    [06720 ... 06727] = { { .od_EmitMulti = EMIT_LEA }, NULL, 0, 0, 1, 0, 4 },
    [06750 ... 06773] = { { .od_EmitMulti = EMIT_LEA }, NULL, 0, 0, 1, 1, 4 },
    [07720 ... 07727] = { { .od_EmitMulti = EMIT_LEA }, NULL, 0, 0, 1, 0, 4 },
    [07750 ... 07773] = { { .od_EmitMulti = EMIT_LEA }, NULL, 0, 0, 1, 1, 4 },

    [00600 ... 00607] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [00620 ... 00647] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [00650 ... 00674] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [00400 ... 00407] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [00420 ... 00447] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [00450 ... 00474] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [01600 ... 01607] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [01620 ... 01647] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [01650 ... 01674] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [01400 ... 01407] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [01420 ... 01447] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [01450 ... 01474] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [02600 ... 02607] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [02620 ... 02647] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [02650 ... 02674] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [02400 ... 02407] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [02420 ... 02447] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [02450 ... 02474] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [03600 ... 03607] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [03620 ... 03647] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [03650 ... 03674] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [03400 ... 03407] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [03420 ... 03447] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [03450 ... 03474] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [04600 ... 04607] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [04620 ... 04647] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [04650 ... 04674] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [04400 ... 04407] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [04420 ... 04447] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [04450 ... 04474] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [05600 ... 05607] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [05620 ... 05647] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [05650 ... 05674] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [05400 ... 05407] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [05420 ... 05447] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [05450 ... 05474] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [06600 ... 06607] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [06620 ... 06647] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [06650 ... 06674] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [06400 ... 06407] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [06420 ... 06447] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [06450 ... 06474] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [07600 ... 07607] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [07620 ... 07647] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [07650 ... 07674] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [07400 ... 07407] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [07420 ... 07447] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [07450 ... 07474] = { { .od_EmitMulti = EMIT_CHK }, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

};

uint32_t *EMIT_line4(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    (*m68k_ptr)++;
    *insn_consumed = 1;

    if (InsnTable[opcode & 0xfff].od_EmitMulti) {
        ptr = InsnTable[opcode & 0xfff].od_EmitMulti(ptr, opcode, m68k_ptr, insn_consumed);
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


#ifdef PISTORM
void do_reset()
{
    void ps_pulse_reset();

    struct Size sz = get_display_size();
    init_display(sz, NULL, NULL);

    ps_pulse_reset();
}
#endif
