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

uint32_t *EMIT_CMPI(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    uint16_t lo16;
    uint32_t u32;

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            lo16 = -BE16((*m68k_ptr)[ext_count++]);
#ifdef __aarch64__
            *ptr++ = mov_immed_u16(immed, (lo16 & 0xff) << 8, 1);
#else
            *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 4);
#endif
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            lo16 = -BE16((*m68k_ptr)[ext_count++]);
#ifdef __aarch64__
            *ptr++ = mov_immed_u16(immed, lo16, 1);
#else
            if (lo16 <= 0xff)
                *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 8);
            else {
                *ptr++ = sub_reg(immed, immed, immed, 0);
                *ptr++ = movt_immed_u16(immed, lo16);
            }
#endif
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            u32 = BE16((*m68k_ptr)[ext_count++]) << 16;
            u32 |= BE16((*m68k_ptr)[ext_count++]);
            u32 = -u32;
            *ptr++ = movw_immed_u16(immed, u32 & 0xffff);
            if (((u32 >> 16) & 0xffff) != 0)
                *ptr++ = movt_immed_u16(immed, u32 >> 16);
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
#ifdef __aarch64__
            case 4:
                *ptr++ = adds_reg(31, immed, dest, LSL, 0);
                break;
            case 2:
                *ptr++ = adds_reg(31, immed, dest, LSL, 16);
                break;
            case 1:
                *ptr++ = adds_reg(31, immed, dest, LSL, 24);
                break;
#else
            case 4:
                *ptr++ = adds_reg(immed, immed, dest, 0);
                break;
            case 2:
                *ptr++ = adds_reg(immed, immed, dest, 16);
                break;
            case 1:
                *ptr++ = adds_reg(immed, immed, dest, 24);
                break;
#endif
        }
    }
    else
    {
        /* Load effective address */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);

        /* Fetch data into temporary register, perform add, store it back */
        switch (size)
        {
#ifdef __aarch64__
        case 4:
            /* Perform calcualtion */
            *ptr++ = adds_reg(31, immed, dest, LSL, 0);
            break;
        case 2:
            /* Perform calcualtion */
            *ptr++ = adds_reg(31, immed, dest, LSL, 16);
            break;
        case 1:
            /* Perform calcualtion */
            *ptr++ = adds_reg(31, immed, dest, LSL, 24);
            break;
#else
        case 4:
            /* Perform calcualtion */
            *ptr++ = adds_reg(immed, immed, dest, 0);
            break;
        case 2:
            /* Perform calcualtion */
            *ptr++ = adds_reg(immed, immed, dest, 16);
            break;
        case 1:
            /* Perform calcualtion */
            *ptr++ = adds_reg(immed, immed, dest, 24);
            break;
#endif
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
        uint8_t cc = RA_ModifyCC(&ptr);
//        ptr = EMIT_GetNZVC(ptr, cc, &update_mask);

        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
        if (update_mask & SR_V)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_VS);
        if (update_mask & SR_C)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C, ARM_CC_CC);
    }
    return ptr;
}

uint32_t *EMIT_SUBI(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    uint16_t lo16;
    uint32_t u32;

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            lo16 = -BE16((*m68k_ptr)[ext_count++]);
#ifdef __aarch64__
            *ptr++ = mov_immed_u16(immed, (lo16 & 0xff) << 8, 1);
#else
            *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 4);
#endif
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            lo16 = -BE16((*m68k_ptr)[ext_count++]);
#ifdef __aarch64__
            *ptr++ = mov_immed_u16(immed, lo16, 1);
#else
            if (lo16 <= 0xff)
                *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 8);
            else {
                *ptr++ = sub_reg(immed, immed, immed, 0);
                *ptr++ = movt_immed_u16(immed, lo16);
            }
#endif
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            u32 = BE16((*m68k_ptr)[ext_count++]) << 16;
            u32 |= BE16((*m68k_ptr)[ext_count++]);
            u32 = -u32;
            *ptr++ = movw_immed_u16(immed, u32 & 0xffff);
            if (((u32 >> 16) & 0xffff) != 0)
                *ptr++ = movt_immed_u16(immed, u32 >> 16);
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
#ifdef __aarch64__
            case 4:
                *ptr++ = adds_reg(dest, immed, dest, LSL, 0);
                break;
            case 2:
                *ptr++ = adds_reg(immed, immed, dest, LSL, 16);
                *ptr++ = bfxil(dest, immed, 16, 16);
                break;
            case 1:
                *ptr++ = adds_reg(immed, immed, dest, LSL, 24);
                *ptr++ = bfxil(dest, immed, 24, 8);
                break;
#else
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
#endif
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
#ifdef __aarch64__
            *ptr++ = adds_reg(immed, immed, tmp, LSL, 0);
#else
            *ptr++ = adds_reg(immed, immed, tmp, 0);
#endif
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
#ifdef __aarch64__
            *ptr++ = adds_reg(immed, immed, tmp, LSL, 16);
            *ptr++ = lsr(immed, immed, 16);
#else
            *ptr++ = adds_reg(immed, immed, tmp, 16);
            *ptr++ = lsr_immed(immed, immed, 16);
#endif
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
#ifdef __aarch64__
            *ptr++ = adds_reg(immed, immed, tmp, LSL, 24);
            *ptr++ = lsr(immed, immed, 24);
#else
            *ptr++ = adds_reg(immed, immed, tmp, 24);
            *ptr++ = lsr_immed(immed, immed, 24);
#endif
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
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_ClearFlags(ptr, cc, update_mask);
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
    return ptr;
}

uint32_t *EMIT_ADDI(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    uint16_t lo16;
    uint32_t u32;

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
#ifdef __aarch64__
            *ptr++ = mov_immed_u16(immed, (lo16 & 0xff) << 8, 1);
#else
            *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 4);
#endif
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
#ifdef __aarch64__
            *ptr++ = mov_immed_u16(immed, lo16, 1);
#else
            if (lo16 <= 0xff)
                *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 8);
            else {
                *ptr++ = sub_reg(immed, immed, immed, 0);
                *ptr++ = movt_immed_u16(immed, lo16);
            }
#endif
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            u32 = BE16((*m68k_ptr)[ext_count++]) << 16;
            u32 |= BE16((*m68k_ptr)[ext_count++]);
            *ptr++ = movw_immed_u16(immed, u32 & 0xffff);
            if (((u32 >> 16) & 0xffff) != 0)
                *ptr++ = movt_immed_u16(immed, u32 >> 16);
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
#ifdef __aarch64__
            case 4:
                *ptr++ = adds_reg(dest, immed, dest, LSL, 0);
                break;
            case 2:
                *ptr++ = adds_reg(immed, immed, dest, LSL, 16);
                *ptr++ = bfxil(dest, immed, 16, 16);
                break;
            case 1:
                *ptr++ = adds_reg(immed, immed, dest, LSL, 24);
                *ptr++ = bfxil(dest, immed, 24, 8);
                break;
#else
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
#endif
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
#ifdef __aarch64__
                *ptr++ = adds_reg(immed, immed, tmp, LSL, 0);
#else
                *ptr++ = adds_reg(immed, immed, tmp, 0);
#endif
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
#ifdef __aarch64__
                *ptr++ = adds_reg(immed, immed, tmp, LSL, 16);
                *ptr++ = lsr(immed, immed, 16);
#else
                *ptr++ = adds_reg(immed, immed, tmp, 16);
                *ptr++ = lsr_immed(immed, immed, 16);
#endif
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
#ifdef __aarch64__
                *ptr++ = adds_reg(immed, immed, tmp, LSL, 24);
                *ptr++ = lsr(immed, immed, 24);
#else
                *ptr++ = adds_reg(immed, immed, tmp, 24);
                *ptr++ = lsr_immed(immed, immed, 24);
#endif
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
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_ClearFlags(ptr, cc, update_mask);
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
#ifdef __aarch64__
    uint8_t cc = RA_ModifyCC(&ptr);
    *ptr++ = orr_reg(cc, cc, immed, LSL, 0);
#else
    M68K_ModifyCC(&ptr);
    *ptr++ = orr_reg(REG_SR, REG_SR, immed, 0);
#endif

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

    kprintf("[LINE0] Supervisor ORI to SR!\n");

    return ptr;
}

uint32_t *EMIT_ORI(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    uint16_t lo16;
    uint32_t u32;

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
#ifdef __aarch64__
            *ptr++ = mov_immed_u16(immed, (lo16 & 0xff) << 8, 1);
#else
            *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 4);
#endif
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
#ifdef __aarch64__
            *ptr++ = mov_immed_u16(immed, lo16, 1);
#else
            if (lo16 <= 0xff)
                *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 8);
            else {
                *ptr++ = sub_reg(immed, immed, immed, 0);
                *ptr++ = movt_immed_u16(immed, lo16);
            }
#endif
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            u32 = BE16((*m68k_ptr)[ext_count++]) << 16;
            u32 |= BE16((*m68k_ptr)[ext_count++]);
            *ptr++ = movw_immed_u16(immed, u32 & 0xffff);
            if (((u32 >> 16) & 0xffff) != 0)
                *ptr++ = movt_immed_u16(immed, u32 >> 16);
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
#ifdef __aarch64__
            case 4:
                *ptr++ = orr_reg(dest, immed, dest, LSL, 0);
                *ptr++ = cmp_reg(31, dest, LSL, 0);
                break;
            case 2:
                *ptr++ = orr_reg(immed, immed, dest, LSL, 16);
                *ptr++ = cmp_reg(31, immed, LSL, 0);
                *ptr++ = bfxil(dest, immed, 16, 16);
                break;
            case 1:
                *ptr++ = orr_reg(immed, immed, dest, LSL, 24);
                *ptr++ = cmp_reg(31, immed, LSL, 0);
                *ptr++ = bfxil(dest, immed, 24, 8);
                break;
#else
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
#endif
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
#ifdef __aarch64__
                *ptr++ = orr_reg(immed, immed, tmp, LSL, 0);
                *ptr++ = cmp_reg(31, immed, LSL, 0);
#else
                *ptr++ = orrs_reg(immed, immed, tmp, 0);
#endif
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
#ifdef __aarch64__
                *ptr++ = orr_reg(immed, immed, tmp, LSL, 16);
                *ptr++ = cmp_reg(31, immed, LSL, 0);
                *ptr++ = lsr(immed, immed, 16);
#else
                *ptr++ = orrs_reg(immed, immed, tmp, 16);
                *ptr++ = lsr_immed(immed, immed, 16);
#endif
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
#ifdef __aarch64__
                *ptr++ = orr_reg(immed, immed, tmp, LSL, 24);
                *ptr++ = cmp_reg(31, immed, LSL, 0);
                *ptr++ = lsr(immed, immed, 24);
#else
                *ptr++ = orrs_reg(immed, immed, tmp, 24);
                *ptr++ = lsr_immed(immed, immed, 24);
#endif
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
        uint8_t cc = RA_ModifyCC(&ptr);

        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
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
#ifdef __aarch64__
    uint8_t cc = RA_ModifyCC(&ptr);
    *ptr++ = and_reg(cc, cc, immed, LSL, 0);
#else
    M68K_ModifyCC(&ptr);
    *ptr++ = and_reg(REG_SR, REG_SR, immed, 0);
#endif
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

    kprintf("[LINE0] Supervisor ANDI to SR!\n");

    return ptr;
}

uint32_t *EMIT_ANDI(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    int16_t lo16;
    uint32_t u32;

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
#ifdef __aarch64__
            *ptr++ = mov_immed_u16(immed, (lo16 & 0xff) << 8, 1);
#else
            *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 4);
#endif
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
#ifdef __aarch64__
            *ptr++ = mov_immed_u16(immed, lo16, 1);
#else
            if (lo16 <= 0xff)
                *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 8);
            else {
                *ptr++ = sub_reg(immed, immed, immed, 0);
                *ptr++ = movt_immed_u16(immed, lo16);
            }
#endif
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            u32 = BE16((*m68k_ptr)[ext_count++]) << 16;
            u32 |= BE16((*m68k_ptr)[ext_count++]);
            *ptr++ = movw_immed_u16(immed, u32 & 0xffff);
            if (((u32 >> 16) & 0xffff) != 0)
                *ptr++ = movt_immed_u16(immed, u32 >> 16);
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
#ifdef __aarch64__
            case 4:
                *ptr++ = ands_reg(dest, immed, dest, LSL, 0);
                break;
            case 2:
                *ptr++ = ands_reg(immed, immed, dest, LSL, 16);
                *ptr++ = bfxil(dest, immed, 16, 16);
                break;
            case 1:
                *ptr++ = ands_reg(immed, immed, dest, LSL, 24);
                *ptr++ = bfxil(dest, immed, 24, 8);
                break;
#else
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
#endif
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
#ifdef __aarch64__
                *ptr++ = ands_reg(immed, immed, tmp, LSL, 0);
#else
                *ptr++ = ands_reg(immed, immed, tmp, 0);
#endif
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
#ifdef __aarch64__
                *ptr++ = ands_reg(immed, immed, tmp, LSL, 16);
                *ptr++ = lsr(immed, immed, 16);
#else
                *ptr++ = ands_reg(immed, immed, tmp, 16);
                *ptr++ = lsr_immed(immed, immed, 16);
#endif
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
#ifdef __aarch64__
                *ptr++ = ands_reg(immed, immed, tmp, LSL, 24);
                *ptr++ = lsr(immed, immed, 24);
#else
                *ptr++ = ands_reg(immed, immed, tmp, 24);
                *ptr++ = lsr_immed(immed, immed, 24);
#endif
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
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
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
#ifdef __aarch64__
    uint8_t cc = RA_ModifyCC(&ptr);
    *ptr++ = eor_reg(cc, cc, immed, LSL, 0);
#else
    M68K_ModifyCC(&ptr);
    *ptr++ = eor_reg(REG_SR, REG_SR, immed, 0);
#endif
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

    kprintf("[LINE0] Supervisor EORI to SR!\n");

    return ptr;
}

uint32_t *EMIT_EORI(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    int16_t lo16;
    uint32_t u32;

    /* Load immediate into the register */
    switch (opcode & 0x00c0)
    {
        case 0x0000:    /* Byte operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
#ifdef __aarch64__
            *ptr++ = mov_immed_u16(immed, (lo16 & 0xff) << 8, 1);
#else
            *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 4);
#endif
            size = 1;
            break;
        case 0x0040:    /* Short operation */
            lo16 = BE16((*m68k_ptr)[ext_count++]);
#ifdef __aarch64__
            *ptr++ = mov_immed_u16(immed, lo16, 1);
#else
            if (lo16 <= 0xff)
                *ptr++ = mov_immed_u8_shift(immed, lo16 & 0xff, 8);
            else {
                *ptr++ = sub_reg(immed, immed, immed, 0);
                *ptr++ = movt_immed_u16(immed, lo16);
            }
#endif
            size = 2;
            break;
        case 0x0080:    /* Long operation */
            u32 = BE16((*m68k_ptr)[ext_count++]) << 16;
            u32 |= BE16((*m68k_ptr)[ext_count++]);
            *ptr++ = movw_immed_u16(immed, u32 & 0xffff);
            if (((u32 >> 16) & 0xffff) != 0)
                *ptr++ = movt_immed_u16(immed, u32 >> 16);
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
#ifdef __aarch64__
            case 4:
                *ptr++ = eor_reg(dest, dest, immed, LSL, 0);
                *ptr++ = cmp_reg(31, dest, LSL, 0);
                break;
            case 2:
                *ptr++ = eor_reg(immed, immed, dest, LSL, 16);
                *ptr++ = cmp_reg(31, immed, LSL, 0);
                *ptr++ = bfxil(dest, immed, 16, 16);
                break;
            case 1:
                *ptr++ = eor_reg(immed, immed, dest, LSL, 24);
                *ptr++ = cmp_reg(31, immed, LSL, 0);
                *ptr++ = bfxil(dest, immed, 24, 8);
                break;
#else
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
#endif
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
#ifdef __aarch64__
                *ptr++ = eor_reg(immed, immed, tmp, LSL, 0);
                *ptr++ = cmp_reg(31, immed, LSL, 0);
#else
                *ptr++ = eors_reg(immed, immed, tmp, 0);
#endif
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
#ifdef __aarch64__
                *ptr++ = eor_reg(immed, immed, tmp, LSL, 16);
                *ptr++ = cmp_reg(31, immed, LSL, 0);
                *ptr++ = lsr(immed, immed, 16);
#else
                *ptr++ = eors_reg(immed, immed, tmp, 16);
                *ptr++ = lsr_immed(immed, immed, 16);
#endif
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
#ifdef __aarch64__
                *ptr++ = eor_reg(immed, immed, tmp, LSL, 24);
                *ptr++ = cmp_reg(31, immed, LSL, 0);
                *ptr++ = lsr(immed, immed, 24);
#else
                *ptr++ = eors_reg(immed, immed, tmp, 24);
                *ptr++ = lsr_immed(immed, immed, 24);
#endif
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
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
    }

    return ptr;
}

uint32_t *EMIT_BTST(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
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
        imm_shift = BE16((*m68k_ptr)[ext_count++]) & 31;
    }
    else
    {
        bit_number = RA_CopyFromM68kRegister(&ptr, (opcode >> 9) & 3);
        bit_mask = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_immed_u8(bit_mask, 1);
    }

    /* handle direct register more here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register */
        dest = RA_MapM68kRegister(&ptr, opcode & 7);

        if (immediate)
        {
#ifdef __aarch64__
            *ptr++ = tst_immed(dest, 1, 31 & (32 - imm_shift));
#else
            int shifter_operand = imm_shift & 1 ? 2:1;
            shifter_operand |= ((16 - (imm_shift >> 1)) & 15) << 8;
            *ptr++ = tst_immed(dest, shifter_operand);
#endif
        }
        else
        {
#ifdef __aarch64__
            *ptr++ = and_immed(bit_number, bit_number, 5, 0);
            *ptr++ = lslv(bit_mask, bit_mask, bit_number);

            *ptr++ = tst_reg(dest, bit_mask, LSL, 0);
#else
            *ptr++ = and_immed(bit_number, bit_number, 31);
            *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

            *ptr++ = tst_reg(dest, bit_mask, 0);
#endif
        }
    }
    else
    {
        /* Load byte from effective address */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 1, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);

        if (immediate)
        {
#ifdef __aarch64__
            *ptr++ = tst_immed(dest, 1, 31 & (32 - (imm_shift & 7)));
#else
            int shifter_operand = 1 << (imm_shift & 7);
            *ptr++ = tst_immed(dest, shifter_operand);
#endif
        }
        else
        {
#ifdef __aarch64__
            *ptr++ = and_immed(bit_number, bit_number, 3, 0);
            *ptr++ = lslv(bit_mask, bit_mask, bit_number);

            *ptr++ = tst_reg(dest, bit_mask, LSL, 0);
#else
            *ptr++ = and_immed(bit_number, bit_number, 7);
            *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

            *ptr++ = tst_reg(dest, bit_mask, 0);
#endif
        }
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
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_ClearFlags(ptr, cc, update_mask);
        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
    }

    return ptr;
}

uint32_t *EMIT_BCHG(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
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
        imm_shift = BE16((*m68k_ptr)[ext_count++]) & 31;
    }
    else
    {
        bit_number = RA_CopyFromM68kRegister(&ptr, (opcode >> 9) & 3);
        bit_mask = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_immed_u8(bit_mask, 1);
    }

    /* handle direct register more here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register and mark it dirty - one bit will change */
        dest = RA_MapM68kRegister(&ptr, opcode & 7);
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        if (immediate)
        {
#ifdef __aarch64__
            *ptr++ = tst_immed(dest, 1, 31 & (32 - imm_shift));
            *ptr++ = eor_immed(dest, dest, 1, 31 & (32 - imm_shift));
#else
            int shifter_operand = imm_shift & 1 ? 2:1;
            shifter_operand |= ((16 - (imm_shift >> 1)) & 15) << 8;

            *ptr++ = tst_immed(dest, shifter_operand);
            *ptr++ = eor_immed(dest, dest, shifter_operand);
#endif
        }
        else
        {
#ifdef __aarch64__
            *ptr++ = and_immed(bit_number, bit_number, 5, 0);
            *ptr++ = lslv(bit_mask, bit_mask, bit_number);

            *ptr++ = tst_reg(dest, bit_mask, LSL, 0);
            *ptr++ = eor_reg(dest, dest, bit_mask, LSL, 0);
#else
            *ptr++ = and_immed(bit_number, bit_number, 31);
            *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

            *ptr++ = tst_reg(dest, bit_mask, 0);
            *ptr++ = eor_reg(dest, dest, bit_mask, 0);
#endif
        }
    }
    else
    {
        /* Load effective address */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;

        /* Fetch data into temporary register, perform bit flip, store it back */
        if (mode == 4)
        {
            *ptr++ = ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1);
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }
        else
            *ptr++ = ldrb_offset(dest, tmp, 0);

        if (immediate)
        {
#ifdef __aarch64__
            *ptr++ = tst_immed(tmp, 1, 31 & (32 - (imm_shift & 7)));
            *ptr++ = eor_immed(tmp, tmp, 1, 31 & (32 - (imm_shift & 7)));
#else
            int shifter_operand = 1 << (imm_shift & 7);
            *ptr++ = tst_immed(tmp, shifter_operand);
            *ptr++ = eor_immed(tmp, tmp, shifter_operand);
#endif
        }
        else
        {
#ifdef __aarch64__
            *ptr++ = and_immed(bit_number, bit_number, 3, 0);
            *ptr++ = lslv(bit_mask, bit_mask, bit_number);

            *ptr++ = tst_reg(tmp, bit_mask, LSL, 0);
            *ptr++ = eor_reg(tmp, tmp, bit_mask, LSL, 0);
#else
            *ptr++ = and_immed(bit_number, bit_number, 7);
            *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

            *ptr++ = tst_reg(tmp, bit_mask, 0);
            *ptr++ = eor_reg(tmp, tmp, bit_mask, 0);
#endif
        }

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
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_ClearFlags(ptr, cc, update_mask);
        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
    }

    return ptr;
}

uint32_t *EMIT_BCLR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
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
        imm_shift = BE16((*m68k_ptr)[ext_count++]) & 31;
    }
    else
    {
        bit_number = RA_CopyFromM68kRegister(&ptr, (opcode >> 9) & 3);
        bit_mask = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_immed_u8(bit_mask, 1);
    }

    /* handle direct register more here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register and mark it dirty - one bit will change */
        dest = RA_MapM68kRegister(&ptr, opcode & 7);
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        if (immediate)
        {
#ifdef __aarch64__
            *ptr++ = tst_immed(dest, 1, 31 & (32 - imm_shift));
            *ptr++ = bic_immed(dest, dest, 1, 31 & (32 - imm_shift));
#else
            int shifter_operand = imm_shift & 1 ? 2:1;
            shifter_operand |= ((16 - (imm_shift >> 1)) & 15) << 8;

            *ptr++ = tst_immed(dest, shifter_operand);
            *ptr++ = bic_immed(dest, dest, shifter_operand);
#endif
        }
        else
        {
#ifdef __aarch64__
            *ptr++ = and_immed(bit_number, bit_number, 3, 0);
            *ptr++ = lslv(bit_mask, bit_mask, bit_number);

            *ptr++ = tst_reg(dest, bit_mask, LSL, 0);
            *ptr++ = bic_reg(dest, dest, bit_mask, LSL, 0);
#else
            *ptr++ = and_immed(bit_number, bit_number, 31);
            *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

            *ptr++ = tst_reg(dest, bit_mask, 0);
            *ptr++ = bic_reg(dest, dest, bit_mask);
#endif
        }
    }
    else
    {
        /* Load effective address */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;

        /* Fetch data into temporary register, perform bit flip, store it back */
        if (mode == 4)
        {
            *ptr++ = ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1);
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }
        else
            *ptr++ = ldrb_offset(dest, tmp, 0);

        if (immediate)
        {
#ifdef __aarch64__
            *ptr++ = tst_immed(tmp, 1, 31 & (32 - (imm_shift & 7)));
            *ptr++ = bic_immed(tmp, tmp, 1, 31 & (32 - (imm_shift & 7)));
#else
            int shifter_operand = 1 << (imm_shift & 7);
            *ptr++ = tst_immed(tmp, shifter_operand);
            *ptr++ = bic_immed(tmp, tmp, shifter_operand);
#endif
        }
        else
        {
#ifdef __aarch64__
            *ptr++ = and_immed(bit_number, bit_number, 3, 0);
            *ptr++ = lslv(bit_mask, bit_mask, bit_number);

            *ptr++ = tst_reg(tmp, bit_mask, LSL, 0);
            *ptr++ = bic_reg(tmp, tmp, bit_mask, LSL, 0);
#else
            *ptr++ = and_immed(bit_number, bit_number, 7);
            *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

            *ptr++ = tst_reg(tmp, bit_mask, 0);
            *ptr++ = bic_reg(tmp, tmp, bit_mask);
#endif
        }

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
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_ClearFlags(ptr, cc, update_mask);
        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
    }

    return ptr;
}

uint32_t *EMIT_BSET(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
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
        imm_shift = BE16((*m68k_ptr)[ext_count++]) & 31;
    }
    else
    {
        bit_number = RA_CopyFromM68kRegister(&ptr, (opcode >> 9) & 3);
        bit_mask = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_immed_u8(bit_mask, 1);
    }

    /* handle direct register more here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register and mark it dirty - one bit will change */
        dest = RA_MapM68kRegister(&ptr, opcode & 7);
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        if (immediate)
        {
#ifdef __aarch64__
            *ptr++ = tst_immed(dest, 1, 31 & (32 - imm_shift));
            *ptr++ = orr_immed(dest, dest, 1, 31 & (32 - imm_shift));
#else
            int shifter_operand = imm_shift & 1 ? 2:1;
            shifter_operand |= ((16 - (imm_shift >> 1)) & 15) << 8;

            *ptr++ = tst_immed(dest, shifter_operand);
            *ptr++ = orr_immed(dest, dest, shifter_operand);
#endif
        }
        else
        {
#ifdef __aarch64__
            *ptr++ = and_immed(bit_number, bit_number, 3, 0);
            *ptr++ = lslv(bit_mask, bit_mask, bit_number);

            *ptr++ = tst_reg(dest, bit_mask, LSL, 0);
            *ptr++ = orr_reg(dest, dest, bit_mask, LSL, 0);
#else
            *ptr++ = and_immed(bit_number, bit_number, 31);
            *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

            *ptr++ = tst_reg(dest, bit_mask, 0);
            *ptr++ = orr_reg(dest, dest, bit_mask, 0);
#endif
        }
    }
    else
    {
        /* Load effective address */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;

        /* Fetch data into temporary register, perform bit flip, store it back */
        if (mode == 4)
        {
            *ptr++ = ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1);
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }
        else
            *ptr++ = ldrb_offset(dest, tmp, 0);

        if (immediate)
        {
#ifdef __aarch64__
            *ptr++ = tst_immed(tmp, 1, 31 & (32 - (imm_shift & 7)));
            *ptr++ = orr_immed(tmp, tmp, 1, 31 & (32 - (imm_shift & 7)));
#else
            int shifter_operand = 1 << (imm_shift & 7);
            *ptr++ = tst_immed(tmp, shifter_operand);
            *ptr++ = orr_immed(tmp, tmp, shifter_operand);
#endif
        }
        else
        {
#ifdef __aarch64__
            *ptr++ = and_immed(bit_number, bit_number, 3, 0);
            *ptr++ = lslv(bit_mask, bit_mask, bit_number);

            *ptr++ = tst_reg(tmp, bit_mask, LSL, 0);
            *ptr++ = orr_reg(tmp, tmp, bit_mask, LSL, 0);
#else
            *ptr++ = and_immed(bit_number, bit_number, 7);
            *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

            *ptr++ = tst_reg(tmp, bit_mask, 0);
            *ptr++ = orr_reg(tmp, tmp, bit_mask, 0);
#endif
        }

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
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_ClearFlags(ptr, cc, update_mask);
        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
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
        kprintf("[LINE0] Not implemented CMP2/CHK2");
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
        kprintf("[LINE0] Supervisor MOVES\n");
        *ptr++ = udf(opcode);
    }
    else if ((opcode & 0xf9c0) == 0x08c0)   /* 00001xx011xxxxxx - CAS, CAS2 */
    {
        kprintf("[LINE0] Not implemented CAS/CAS2");
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
        kprintf("[LINE0] Not implemented MOVEP");
        *ptr++ = udf(opcode);
    }
    else
    {
        *ptr++ = udf(opcode);
    }


    return ptr;
}
