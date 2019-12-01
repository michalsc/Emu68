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

uint32_t *EMIT_CMPI(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest;
    uint8_t size = 0;
    uint16_t lo16, hi16;

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 4);
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            if (lo16 <= 0xff)
                *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 8);
            else {
                *ptr++ = sub_reg(immed, immed, immed, 0);
                *ptr++ = movt_immed_u16(immed, lo16);
            }
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            hi16 = BE16((*m68k_ptr)[ext_count++]);
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            *ptr++ = movw_immed_u16(immed, lo16);
            if (hi16 != 0)
                *ptr++ = movt_immed_u16(immed, hi16);
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
                *ptr++ = rsbs_reg(immed, immed, dest, 0);
                break;
            case 2:
                *ptr++ = rsbs_reg(immed, immed, dest, 16);
                break;
            case 1:
                *ptr++ = rsbs_reg(immed, immed, dest, 24);
                break;
        }
    }
    else
    {
        /* Load effective address */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);

        /* Fetch data into temporary register, perform add, store it back */
        switch (size)
        {
        case 4:
            /* Perform calcualtion */
            *ptr++ = rsbs_reg(immed, immed, dest, 0);
            break;
        case 2:
            /* Perform calcualtion */
            *ptr++ = rsbs_reg(immed, immed, dest, 16);
            break;
        case 1:
            /* Perform calcualtion */
            *ptr++ = rsbs_reg(immed, immed, dest, 24);
            break;
        }
    }

    RA_FreeARMRegister(&ptr, immed);
    RA_FreeARMRegister(&ptr, dest);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

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
        if (update_mask & SR_V)
            *ptr++ = orr_cc_immed(ARM_CC_VS, REG_SR, REG_SR, SR_V);
        if (update_mask & SR_C)    /* Note - after sub/rsb C flag on ARM is inverted! */
            *ptr++ = orr_cc_immed(ARM_CC_CC, REG_SR, REG_SR, SR_C);
    }
    return ptr;
}

uint32_t *EMIT_SUBI(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest;
    uint8_t size = 0;
    uint16_t lo16, hi16;

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 4);
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            if (lo16 <= 0xff)
                *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 8);
            else {
                *ptr++ = sub_reg(immed, immed, immed, 0);
                *ptr++ = movt_immed_u16(immed, lo16);
            }
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            hi16 = BE16((*m68k_ptr)[ext_count++]);
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            *ptr++ = movw_immed_u16(immed, lo16);
            if (hi16 != 0)
                *ptr++ = movt_immed_u16(immed, hi16);
            size = 4;
            break;
    }

    /* handle adding to register here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register */
        dest = RA_MapM68kRegister(&ptr, opcode & 7);

        /* Mark register dirty */
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        /* Perform add operation */
        switch (size)
        {
            case 4:
                *ptr++ = subs_reg(dest, dest, immed, 0);
                break;
            case 2:
                *ptr++ = rsbs_reg(immed, immed, dest, 16);
                *ptr++ = lsr_immed(immed, immed, 16);
                *ptr++ = bfi(dest, immed, 0, 16);
                break;
            case 1:
                *ptr++ = rsbs_reg(immed, immed, dest, 24);
                *ptr++ = lsr_immed(immed, immed, 24);
                *ptr++ = bfi(dest, immed, 0, 8);
                break;
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

            /* Perform calcualtion */
            *ptr++ = rsbs_reg(immed, immed, tmp, 0);

            /* Store back */
            if (mode == 3)
            {
                *ptr++ = str_offset_postindex(dest, immed, 4);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = str_offset(dest, immed, 0);
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
            *ptr++ = rsbs_reg(immed, immed, tmp, 16);
            *ptr++ = lsr_immed(immed, immed, 16);
            /* Store back */
            if (mode == 3)
            {
                *ptr++ = strh_offset_postindex(dest, immed, 2);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strh_offset(dest, immed, 0);
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
            *ptr++ = rsbs_reg(immed, immed, tmp, 24);
            *ptr++ = lsr_immed(immed, immed, 24);
            /* Store back */
            if (mode == 3)
            {
                *ptr++ = strb_offset_postindex(dest, immed, (opcode & 7) == 7 ? 2 : 1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strb_offset(dest, immed, 0);
            break;
        }

        RA_FreeARMRegister(&ptr, tmp);
    }

    RA_FreeARMRegister(&ptr, immed);
    RA_FreeARMRegister(&ptr, dest);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

    uint8_t mask = M68K_GetSRMask(*m68k_ptr);
    uint8_t update_mask = (SR_X | SR_C | SR_V | SR_Z | SR_N) & ~mask;

    if (update_mask)
    {
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        if (update_mask & SR_V)
            *ptr++ = orr_cc_immed(ARM_CC_VS, REG_SR, REG_SR, SR_V);
        if (update_mask & (SR_X | SR_C))    /* Note - after sub/rsb C flag on ARM is inverted! */
            *ptr++ = orr_cc_immed(ARM_CC_CC, REG_SR, REG_SR, SR_X | SR_C);
    }
    return ptr;
}

uint32_t *EMIT_ADDI(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest;
    uint8_t size = 0;
    uint16_t lo16, hi16;

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 4);
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            if (lo16 <= 0xff)
                *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 8);
            else {
                *ptr++ = sub_reg(immed, immed, immed, 0);
                *ptr++ = movt_immed_u16(immed, lo16);
            }
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            hi16 = BE16((*m68k_ptr)[ext_count++]);
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            *ptr++ = movw_immed_u16(immed, lo16);
            if (hi16 != 0)
                *ptr++ = movt_immed_u16(immed, hi16);
            size = 4;
            break;
    }

    /* handle adding to register here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register */
        dest = RA_MapM68kRegister(&ptr, opcode & 7);

        /* Mark register dirty */
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        /* Perform add operation */
        switch(size)
        {
            case 4:
                *ptr++ = adds_reg(dest, dest, immed, 0);
                break;
            case 2:
                *ptr++ = adds_reg(immed, immed, dest, 16);
                *ptr++ = lsr_immed(immed, immed, 16);
                *ptr++ = bfi(dest, immed, 0, 16);
                break;
            case 1:
                *ptr++ = adds_reg(immed, immed, dest, 24);
                *ptr++ = lsr_immed(immed, immed, 24);
                *ptr++ = bfi(dest, immed, 0, 8);
                break;
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
        switch(size)
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
                *ptr++ = adds_reg(immed, immed, tmp, 0);

                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = str_offset_postindex(dest, immed, 4);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = str_offset(dest, immed, 0);
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
                *ptr++ = adds_reg(immed, immed, tmp, 16);
                *ptr++ = lsr_immed(immed, immed, 16);
                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = strh_offset_postindex(dest, immed, 2);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = strh_offset(dest, immed, 0);
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
                *ptr++ = adds_reg(immed, immed, tmp, 24);
                *ptr++ = lsr_immed(immed, immed, 24);
                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = strb_offset_postindex(dest, immed, (opcode & 7) == 7 ? 2 : 1);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = strb_offset(dest, immed, 0);
                break;
        }

        RA_FreeARMRegister(&ptr, tmp);
    }

    RA_FreeARMRegister(&ptr, immed);
    RA_FreeARMRegister(&ptr, dest);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));

    (*m68k_ptr) += ext_count;

    uint8_t mask = M68K_GetSRMask(*m68k_ptr);
    uint8_t update_mask = (SR_X | SR_C | SR_V | SR_Z | SR_N) & ~mask;

    if (update_mask)
    {
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        if (update_mask & SR_V)
            *ptr++ = orr_cc_immed(ARM_CC_VS, REG_SR, REG_SR, SR_V);
        if (update_mask & (SR_X | SR_C))
            *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_X | SR_C);
    }
    return ptr;
}

uint32_t *EMIT_ORI_TO_CCR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    (void)opcode;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint16_t val8 = BE16(*m68k_ptr[0]);

    /* Load immediate into the register */
    *ptr++ = mov_immed_u8(immed, val8 & 0xff);
    /* OR with status register, no need to check mask, ARM sequence way too short! */
    M68K_ModifyCC(&ptr);
    *ptr++ = orr_reg(REG_SR, REG_SR, immed, 0);

    RA_FreeARMRegister(&ptr, immed);

    ptr = EMIT_AdvancePC(ptr, 4);
    (*m68k_ptr) += 1;

    return ptr;
}

uint32_t *EMIT_ORI_TO_SR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    (void)ptr;
    (void)opcode;
    (void)m68k_ptr;

    printf("[LINE0] Supervisor ORI to SR!\n");

    return ptr;
}

uint32_t *EMIT_ORI(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest;
    uint8_t size = 0;
    uint16_t lo16, hi16;

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 4);
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            if (lo16 <= 0xff)
                *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 8);
            else {
                *ptr++ = sub_reg(immed, immed, immed, 0);
                *ptr++ = movt_immed_u16(immed, lo16);
            }
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            hi16 = BE16((*m68k_ptr)[ext_count++]);
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            *ptr++ = movw_immed_u16(immed, lo16);
            if (hi16 != 0)
                *ptr++ = movt_immed_u16(immed, hi16);
            size = 4;
            break;
    }

    /* handle adding to register here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register */
        dest = RA_MapM68kRegister(&ptr, opcode & 7);

        /* Mark register dirty */
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        /* Perform add operation */
        switch(size)
        {
            case 4:
                *ptr++ = orrs_reg(dest, dest, immed, 0);
                break;
            case 2:
                *ptr++ = orrs_reg(immed, immed, dest, 16);
                *ptr++ = lsr_immed(immed, immed, 16);
                *ptr++ = bfi(dest, immed, 0, 16);
                break;
            case 1:
                *ptr++ = orrs_reg(immed, immed, dest, 24);
                *ptr++ = lsr_immed(immed, immed, 24);
                *ptr++ = bfi(dest, immed, 0, 8);
                break;
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
        switch(size)
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
                *ptr++ = orrs_reg(immed, immed, tmp, 0);

                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = str_offset_postindex(dest, immed, 4);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = str_offset(dest, immed, 0);
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
                *ptr++ = orrs_reg(immed, immed, tmp, 16);
                *ptr++ = lsr_immed(immed, immed, 16);
                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = strh_offset_postindex(dest, immed, 2);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = strh_offset(dest, immed, 0);
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
                *ptr++ = orrs_reg(immed, immed, tmp, 24);
                *ptr++ = lsr_immed(immed, immed, 24);
                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = strb_offset_postindex(dest, immed, (opcode & 7) == 7 ? 2 : 1);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = strb_offset(dest, immed, 0);
                break;
        }

        RA_FreeARMRegister(&ptr, tmp);
    }

    RA_FreeARMRegister(&ptr, immed);
    RA_FreeARMRegister(&ptr, dest);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

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

    return ptr;
}

uint32_t *EMIT_ANDI_TO_CCR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    (void)opcode;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint16_t val = BE16(*m68k_ptr[0]);

    /* Load immediate into the register */
    *ptr++ = mov_immed_u8(immed, val & 0xff);
    /* OR with status register, no need to check mask, ARM sequence way too short! */
    M68K_ModifyCC(&ptr);
    *ptr++ = and_reg(REG_SR, REG_SR, immed, 0);

    RA_FreeARMRegister(&ptr, immed);

    ptr = EMIT_AdvancePC(ptr, 4);
    (*m68k_ptr) += 1;

    return ptr;
}

uint32_t *EMIT_ANDI_TO_SR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    (void)ptr;
    (void)opcode;
    (void)m68k_ptr;

    printf("[LINE0] Supervisor ANDI to SR!\n");

    return ptr;
}

uint32_t *EMIT_ANDI(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest;
    uint8_t size = 0;
    int16_t lo16, hi16;

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 4);
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            if (lo16 <= 0xff)
                *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 8);
            else {
                *ptr++ = sub_reg(immed, immed, immed, 0);
                *ptr++ = movt_immed_u16(immed, lo16);
            }
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            hi16 = BE16((*m68k_ptr)[ext_count++]);
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            *ptr++ = movw_immed_u16(immed, lo16);
            if (hi16 != 0)
                *ptr++ = movt_immed_u16(immed, hi16);
            size = 4;
            break;
    }

    /* handle adding to register here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register */
        dest = RA_MapM68kRegister(&ptr, opcode & 7);

        /* Mark register dirty */
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        /* Perform add operation */
        switch(size)
        {
            case 4:
                *ptr++ = ands_reg(dest, dest, immed, 0);
                break;
            case 2:
                *ptr++ = ands_reg(immed, immed, dest, 16);
                *ptr++ = lsr_immed(immed, immed, 16);
                *ptr++ = bfi(dest, immed, 0, 16);
                break;
            case 1:
                *ptr++ = ands_reg(immed, immed, dest, 24);
                *ptr++ = lsr_immed(immed, immed, 24);
                *ptr++ = bfi(dest, immed, 0, 8);
                break;
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
        switch(size)
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
                *ptr++ = ands_reg(immed, immed, tmp, 0);

                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = str_offset_postindex(dest, immed, 4);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = str_offset(dest, immed, 0);
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
                *ptr++ = ands_reg(immed, immed, tmp, 16);
                *ptr++ = lsr_immed(immed, immed, 16);
                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = strh_offset_postindex(dest, immed, 2);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = strh_offset(dest, immed, 0);
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
                *ptr++ = ands_reg(immed, immed, tmp, 24);
                *ptr++ = lsr_immed(immed, immed, 24);
                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = strb_offset_postindex(dest, immed, (opcode & 7) == 7 ? 2 : 1);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = strb_offset(dest, immed, 0);
                break;
        }

        RA_FreeARMRegister(&ptr, tmp);
    }

    RA_FreeARMRegister(&ptr, immed);
    RA_FreeARMRegister(&ptr, dest);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

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

    return ptr;
}

uint32_t *EMIT_EORI_TO_CCR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    (void)opcode;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    int16_t val = BE16((*m68k_ptr)[0]);

    /* Load immediate into the register */
    *ptr++ = mov_immed_u8(immed, val & 0xff);
    /* OR with status register, no need to check mask, ARM sequence way too short! */
    M68K_ModifyCC(&ptr);
    *ptr++ = eor_reg(REG_SR, REG_SR, immed, 0);

    RA_FreeARMRegister(&ptr, immed);

    ptr = EMIT_AdvancePC(ptr, 4);
    (*m68k_ptr) += 1;

    return ptr;
}

uint32_t *EMIT_EORI_TO_SR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    (void)ptr;
    (void)opcode;
    (void)m68k_ptr;

    printf("[LINE0] Supervisor EORI to SR!\n");

    return ptr;
}

uint32_t *EMIT_EORI(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest;
    uint8_t size = 0;
    int16_t lo16, hi16;

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 4);
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            if (lo16 <= 0xff)
                *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 8);
            else {
                *ptr++ = sub_reg(immed, immed, immed, 0);
                *ptr++ = movt_immed_u16(immed, lo16);
            }
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            hi16 = BE16((*m68k_ptr)[ext_count++]);
            lo16 = BE16((*m68k_ptr)[ext_count++]);
            *ptr++ = movw_immed_u16(immed, lo16);
            if (hi16 != 0)
                *ptr++ = movt_immed_u16(immed, hi16);
            size = 4;
            break;
    }

    /* handle adding to register here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register */
        dest = RA_MapM68kRegister(&ptr, opcode & 7);

        /* Mark register dirty */
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        /* Perform add operation */
        switch(size)
        {
            case 4:
                *ptr++ = eors_reg(dest, dest, immed, 0);
                break;
            case 2:
                *ptr++ = eors_reg(immed, immed, dest, 16);
                *ptr++ = lsr_immed(immed, immed, 16);
                *ptr++ = bfi(dest, immed, 0, 16);
                break;
            case 1:
                *ptr++ = eors_reg(immed, immed, dest, 24);
                *ptr++ = lsr_immed(immed, immed, 24);
                *ptr++ = bfi(dest, immed, 0, 8);
                break;
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
        switch(size)
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
                *ptr++ = eors_reg(immed, immed, tmp, 0);

                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = str_offset_postindex(dest, immed, 4);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = str_offset(dest, immed, 0);
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
                *ptr++ = eors_reg(immed, immed, tmp, 16);
                *ptr++ = lsr_immed(immed, immed, 16);
                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = strh_offset_postindex(dest, immed, 2);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = strh_offset(dest, immed, 0);
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
                *ptr++ = eors_reg(immed, immed, tmp, 24);
                *ptr++ = lsr_immed(immed, immed, 24);
                /* Store back */
                if (mode == 3)
                {
                    *ptr++ = strb_offset_postindex(dest, immed, (opcode & 7) == 7 ? 2 : 1);
                    RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
                }
                else
                    *ptr++ = strb_offset(dest, immed, 0);
                break;
        }

        RA_FreeARMRegister(&ptr, tmp);
    }

    RA_FreeARMRegister(&ptr, immed);
    RA_FreeARMRegister(&ptr, dest);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

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

    return ptr;
}

uint32_t *EMIT_BTST(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t bit_number = RA_AllocARMRegister(&ptr);
    uint8_t dest;
    uint8_t bit_mask = RA_AllocARMRegister(&ptr);

    /* Load 1 into mask */
    *ptr++ = mov_immed_u8(bit_mask, 1);

    /* Get the bit number either as immediate or from register */
    if ((opcode & 0xffc0) == 0x0800)
    {
        int8_t pc_off = 3;
        ptr = EMIT_GetOffsetPC(ptr, &pc_off);
        *ptr++ = ldrb_offset(REG_PC, bit_number, pc_off);
        ext_count++;
    }
    else
    {
        uint8_t reg = RA_MapM68kRegister(&ptr, (opcode >> 9) & 3);
        *ptr++ = mov_reg(bit_number, reg);
    }

    /* handle direct register more here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register */
        dest = RA_MapM68kRegister(&ptr, opcode & 7);

        *ptr++ = and_immed(bit_number, bit_number, 31);
        *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

        *ptr++ = ands_reg(bit_mask, dest, bit_mask, 0);
    }
    else
    {
        /* Load byte from effective address */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 1, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);

        *ptr++ = and_immed(bit_number, bit_number, 7);
        *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

        *ptr++ = ands_reg(bit_mask, dest, bit_mask, 0);
    }

    RA_FreeARMRegister(&ptr, bit_number);
    RA_FreeARMRegister(&ptr, bit_mask);
    RA_FreeARMRegister(&ptr, dest);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

    uint8_t mask = M68K_GetSRMask(*m68k_ptr);
    uint8_t update_mask = SR_Z & ~mask;

    if (update_mask)
    {
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
    }

    return ptr;
}

uint32_t *EMIT_BCHG(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t bit_number = RA_AllocARMRegister(&ptr);
    uint8_t dest = 0;
    uint8_t bit_mask = RA_AllocARMRegister(&ptr);

    /* Load 1 into mask */
    *ptr++ = mov_immed_u8(bit_mask, 1);

    /* Get the bit number either as immediate or from register */
    if ((opcode & 0xffc0) == 0x0840)
    {
        int8_t pc_off = 3;
        ptr = EMIT_GetOffsetPC(ptr, &pc_off);
        *ptr++ = ldrb_offset(REG_PC, bit_number, pc_off);
        ext_count++;
    }
    else
    {
        uint8_t reg = RA_MapM68kRegister(&ptr, (opcode >> 9) & 3);
        *ptr++ = mov_reg(bit_number, reg);
    }

    /* handle direct register more here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register and mark it dirty - one bit will change */
        dest = RA_MapM68kRegister(&ptr, opcode & 7);
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        *ptr++ = and_immed(bit_number, bit_number, 31);
        *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

        /* Get old bit state, waste bit_number register (not used anymore) */
        *ptr++ = ands_reg(bit_number, dest, bit_mask, 0);
        /* Switch bit */
        *ptr++ = eor_reg(dest, dest, bit_mask, 0);
    }
    else
    {
        /* Load effective address */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;

        *ptr++ = and_immed(bit_number, bit_number, 7);
        *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

        /* Fetch data into temporary register, perform bit flip, store it back */
        if (mode == 4)
        {
            *ptr++ = ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1);
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }
        else
            *ptr++ = ldrb_offset(dest, tmp, 0);

        /* Perform calcualtion */
        /* Get old bit state, waste bit_number register (not used anymore) */
        *ptr++ = ands_reg(bit_number, tmp, bit_mask, 0);
        /* Switch bit */
        *ptr++ = eor_reg(tmp, tmp, bit_mask, 0);

        /* Store back */
        if (mode == 3)
        {
            *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }
        else
            *ptr++ = strb_offset(dest, tmp, 0);

        RA_FreeARMRegister(&ptr, tmp);
    }

    RA_FreeARMRegister(&ptr, bit_number);
    RA_FreeARMRegister(&ptr, bit_mask);
    RA_FreeARMRegister(&ptr, dest);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

    uint8_t mask = M68K_GetSRMask(*m68k_ptr);
    uint8_t update_mask = SR_Z & ~mask;

    if (update_mask)
    {
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
    }

    return ptr;
}

uint32_t *EMIT_BCLR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t bit_number = RA_AllocARMRegister(&ptr);
    uint8_t dest = 0;
    uint8_t bit_mask = RA_AllocARMRegister(&ptr);

    /* Load 1 into mask */
    *ptr++ = mov_immed_u8(bit_mask, 1);

    /* Get the bit number either as immediate or from register */
    if ((opcode & 0xffc0) == 0x0880)
    {
        int8_t off = 3;
        ptr = EMIT_GetOffsetPC(ptr, &off);
        *ptr++ = ldrb_offset(REG_PC, bit_number, off);
        ext_count++;
    }
    else
    {
        uint8_t reg = RA_MapM68kRegister(&ptr, (opcode >> 9) & 3);
        *ptr++ = mov_reg(bit_number, reg);
    }

    /* handle direct register more here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register and mark it dirty - one bit will change */
        dest = RA_MapM68kRegister(&ptr, opcode & 7);
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        *ptr++ = and_immed(bit_number, bit_number, 31);
        *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

        /* Get old bit state, waste bit_number register (not used anymore) */
        *ptr++ = ands_reg(bit_number, dest, bit_mask, 0);
        /*  Clear bit using eor - if bit mask & dest != 0, then eor with result will reverse (clear) the bit.
            otherwise result was zero and zero xor zero results in zero anyway */
        *ptr++ = eor_reg(dest, dest, bit_number, 0);
    }
    else
    {
        /* Load effective address */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;

        *ptr++ = and_immed(bit_number, bit_number, 7);
        *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

        /* Fetch data into temporary register, perform bit flip, store it back */
        if (mode == 4)
        {
            *ptr++ = ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1);
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }
        else
            *ptr++ = ldrb_offset(dest, tmp, 0);

        /* Perform calcualtion */
        /* Get old bit state, waste bit_number register (not used anymore) */
        *ptr++ = ands_reg(bit_number, tmp, bit_mask, 0);
        /* Switch bit */
        *ptr++ = eor_reg(tmp, tmp, bit_number, 0);

        /* Store back */
        if (mode == 3)
        {
            *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }
        else
            *ptr++ = strb_offset(dest, tmp, 0);

        RA_FreeARMRegister(&ptr, tmp);
    }

    RA_FreeARMRegister(&ptr, bit_number);
    RA_FreeARMRegister(&ptr, bit_mask);
    RA_FreeARMRegister(&ptr, dest);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

    uint8_t mask = M68K_GetSRMask(*m68k_ptr);
    uint8_t update_mask = SR_Z & ~mask;

    if (update_mask)
    {
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
    }

    return ptr;
}

uint32_t *EMIT_BSET(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t bit_number = RA_AllocARMRegister(&ptr);
    uint8_t dest = 0;
    uint8_t bit_mask = RA_AllocARMRegister(&ptr);

    /* Load 1 into mask */
    *ptr++ = mov_immed_u8(bit_mask, 1);

    /* Get the bit number either as immediate or from register */
    if ((opcode & 0xffc0) == 0x08c0)
    {
        int8_t pc_off = 3;
        ptr = EMIT_GetOffsetPC(ptr, &pc_off);
        *ptr++ = ldrb_offset(REG_PC, bit_number, pc_off);
        ext_count++;
    }
    else
    {
        uint8_t reg = RA_MapM68kRegister(&ptr, (opcode >> 9) & 3);
        *ptr++ = mov_reg(bit_number, reg);
    }

    /* handle direct register more here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register and mark it dirty - one bit will change */
        dest = RA_MapM68kRegister(&ptr, opcode & 7);
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        *ptr++ = and_immed(bit_number, bit_number, 31);
        *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

        /* Get old bit state, waste bit_number register (not used anymore) */
        *ptr++ = ands_reg(bit_number, dest, bit_mask, 0);
        /*  Set bit */
        *ptr++ = orr_reg(dest, dest, bit_mask, 0);
    }
    else
    {
        /* Load effective address */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;

        *ptr++ = and_immed(bit_number, bit_number, 7);
        *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

        /* Fetch data into temporary register, perform bit flip, store it back */
        if (mode == 4)
        {
            *ptr++ = ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1);
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }
        else
            *ptr++ = ldrb_offset(dest, tmp, 0);

        /* Perform calcualtion */
        /* Get old bit state, waste bit_number register (not used anymore) */
        *ptr++ = ands_reg(bit_number, tmp, bit_mask, 0);
        /* Switch bit */
        *ptr++ = orr_reg(tmp, tmp, bit_mask, 0);

        /* Store back */
        if (mode == 3)
        {
            *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }
        else
            *ptr++ = strb_offset(dest, tmp, 0);

        RA_FreeARMRegister(&ptr, tmp);
    }

    RA_FreeARMRegister(&ptr, bit_number);
    RA_FreeARMRegister(&ptr, bit_mask);
    RA_FreeARMRegister(&ptr, dest);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

    uint8_t mask = M68K_GetSRMask(*m68k_ptr);
    uint8_t update_mask = SR_Z & ~mask;

    if (update_mask)
    {
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
    }

    return ptr;
}

uint32_t *EMIT_line0(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    if ((opcode & 0xff00) == 0x0000 && (opcode & 0x00c0) != 0x00c0)   /* 00000000xxxxxxxx - ORI to CCR, ORI to SR, ORI */
    {
        if ((opcode & 0x00ff) == 0x003c)
            ptr = EMIT_ORI_TO_CCR(ptr, opcode, m68k_ptr);
        else if ((opcode & 0x00ff) == 0x007c)
            ptr = EMIT_ORI_TO_SR(ptr, opcode, m68k_ptr);
        else
            ptr = EMIT_ORI(ptr, opcode, m68k_ptr);
    }
    else if ((opcode & 0xff00) == 0x0200)   /* 00000010xxxxxxxx - ANDI to CCR, ANDI to SR, ANDI */
    {
        if ((opcode & 0x00ff) == 0x003c)
            ptr = EMIT_ANDI_TO_CCR(ptr, opcode, m68k_ptr);
        else if ((opcode & 0x00ff) == 0x007c)
            ptr = EMIT_ANDI_TO_SR(ptr, opcode, m68k_ptr);
        else
            ptr = EMIT_ANDI(ptr, opcode, m68k_ptr);
    }
    else if ((opcode & 0xff00) == 0x0400)   /* 00000100xxxxxxxx - SUBI */
    {
        ptr = EMIT_SUBI(ptr, opcode, m68k_ptr);
    }
    else if ((opcode & 0xff00) == 0x0600 && (opcode & 0x00c0) != 0x00c0)   /* 00000110xxxxxxxx - ADDI */
    {
        ptr = EMIT_ADDI(ptr, opcode, m68k_ptr);
    }
    else if ((opcode & 0xf9c0) == 0x00c0)   /* 00000xx011xxxxxx - CMP2, CHK2 */
    {
        printf("[LINE0] Not implemented CMP2/CHK2");
        *ptr++ = udf(opcode);
    }
    else if ((opcode & 0xff00) == 0x0a00)   /* 00001010xxxxxxxx - EORI to CCR, EORI to SR, EORI */
    {
        if ((opcode & 0x00ff) == 0x003c)
            ptr = EMIT_EORI_TO_CCR(ptr, opcode, m68k_ptr);
        else if ((opcode & 0x00ff) == 0x007c)
            ptr = EMIT_EORI_TO_SR(ptr, opcode, m68k_ptr);
        else
            ptr = EMIT_EORI(ptr, opcode, m68k_ptr);
    }
    else if ((opcode & 0xff00) == 0x0c00)   /* 00001100xxxxxxxx - CMPI */
    {
        ptr = EMIT_CMPI(ptr, opcode, m68k_ptr);
    }
    else if ((opcode & 0xffc0) == 0x0800)   /* 0000100000xxxxxx - BTST */
    {
        ptr = EMIT_BTST(ptr, opcode, m68k_ptr);
    }
    else if ((opcode & 0xffc0) == 0x0840)   /* 0000100001xxxxxx - BCHG */
    {
        ptr = EMIT_BCHG(ptr, opcode, m68k_ptr);
    }
    else if ((opcode & 0xffc0) == 0x0880)   /* 0000100010xxxxxx - BCLR */
    {
        ptr = EMIT_BCLR(ptr, opcode, m68k_ptr);
    }
    else if ((opcode & 0xffc0) == 0x08c0)   /* 0000100011xxxxxx - BSET */
    {
        ptr = EMIT_BSET(ptr, opcode, m68k_ptr);
    }
    else if ((opcode & 0xff00) == 0x0e00)   /* 00001110xxxxxxxx - MOVES */
    {
        printf("[LINE0] Supervisor MOVES\n");
        *ptr++ = udf(opcode);
    }
    else if ((opcode & 0xf9c0) == 0x08c0)   /* 00001xx011xxxxxx - CAS, CAS2 */
    {
        printf("[LINE0] Not implemented CAS/CAS2");
        *ptr++ = udf(opcode);
    }
    else if ((opcode & 0xf1c0) == 0x0100)   /* 0000xxx100xxxxxx - BTST */
    {
        ptr = EMIT_BTST(ptr, opcode, m68k_ptr);
    }
    else if ((opcode & 0xf1c0) == 0x0140)   /* 0000xxx101xxxxxx - BCHG */
    {
        ptr = EMIT_BCHG(ptr, opcode, m68k_ptr);
    }
    else if ((opcode & 0xf1c0) == 0x0180)   /* 0000xxx110xxxxxx - BCLR */
    {
        ptr = EMIT_BCLR(ptr, opcode, m68k_ptr);
    }
    else if ((opcode & 0xf1c0) == 0x01c0)   /* 0000xxx111xxxxxx - BSET */
    {
        ptr = EMIT_BSET(ptr, opcode, m68k_ptr);
    }
    else if ((opcode & 0xf038) == 0x0008)   /* 0000xxxxxx001xxx - MOVEP */
    {
        printf("[LINE0] Not implemented MOVEP");
        *ptr++ = udf(opcode);
    }
    else
    {
        *ptr++ = udf(opcode);
    }


    return ptr;
}
