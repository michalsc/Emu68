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
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    uint16_t lo16;
    uint32_t u32;
    int immediate = 0;

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
            if (u32 < 4096)
            {
                immediate = 1;
            }
            else
            {
                *ptr++ = movw_immed_u16(immed, u32 & 0xffff);
                if (((u32 >> 16) & 0xffff) != 0)
                    *ptr++ = movt_immed_u16(immed, u32 >> 16);
            }
            size = 4;
            break;
    }

    /* handle adding to register here */
    if ((opcode & 0x0038) == 0)
    {
        /* Fetch m68k register */
        dest = RA_MapM68kRegister(&ptr, opcode & 7);
        uint8_t tmpreg = RA_AllocARMRegister(&ptr);

        /* Perform add operation */
        switch (size)
        {
#ifdef __aarch64__
            case 4:
                if (immediate)
                    *ptr++ = cmp_immed(dest, u32);
                else
                    *ptr++ = cmp_reg(dest, immed, LSL, 0);
                break;
            case 2:
                *ptr++ = lsl(tmpreg, dest, 16);
                *ptr++ = cmp_reg(tmpreg, immed, LSL, 0);
                break;
            case 1:
                *ptr++ = lsl(tmpreg, dest, 24);
                *ptr++ = cmp_reg(tmpreg, immed, LSL, 0);
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
        RA_FreeARMRegister(&ptr, tmpreg);
    }
    else
    {
        /* Load effective address */
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);
        uint8_t tmpreg = RA_AllocARMRegister(&ptr);

        /* Fetch data into temporary register, perform add, store it back */
        switch (size)
        {
#ifdef __aarch64__
        case 4:
            /* Perform calcualtion */
            if (immediate)
                *ptr++ = cmp_immed(dest, u32);
            else
                *ptr++ = cmp_reg(dest, immed, LSL, 0);
            break;
        case 2:
            /* Perform calcualtion */
            *ptr++ = lsl(tmpreg, dest, 16);
            *ptr++ = cmp_reg(tmpreg, immed, LSL, 0);
            break;
        case 1:
            /* Perform calcualtion */
            *ptr++ = lsl(tmpreg, dest, 24);
            *ptr++ = cmp_reg(tmpreg, immed, LSL, 0);
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

        RA_FreeARMRegister(&ptr, tmpreg);
    }

    RA_FreeARMRegister(&ptr, immed);
    RA_FreeARMRegister(&ptr, dest);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_GetNZVnC(ptr, cc, &update_mask);

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
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    uint16_t lo16;
    uint32_t u32 = 0;
    int immediate = 0;

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
            if (u32 < 4096)
            {
                immediate = 1;
            }
            else {
                *ptr++ = movw_immed_u16(immed, u32 & 0xffff);
                if (((u32 >> 16) & 0xffff) != 0)
                    *ptr++ = movt_immed_u16(immed, u32 >> 16);
            }
            size = 4;
            break;
    }

    /* handle adding to register here */
    if ((opcode & 0x0038) == 0)
    {
        /* Temporary register for 8/16 bit operations */
        uint8_t temp = RA_AllocARMRegister(&ptr);

        /* Fetch m68k register */
        dest = RA_MapM68kRegister(&ptr, opcode & 7);

        /* Mark register dirty */
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        /* Perform add operation */
        switch (size)
        {
#ifdef __aarch64__
            case 4:
                if (immediate)
                    *ptr++ = subs_immed(dest, dest, u32 & 0xffff);
                else
                    *ptr++ = subs_reg(dest, dest, immed, LSL, 0);
                break;
            case 2:
                *ptr++ = lsl(temp, dest, 16);
                *ptr++ = subs_reg(temp, temp, immed, LSL, 0);
                *ptr++ = bfxil(dest, temp, 16, 16);
                break;
            case 1:
                *ptr++ = lsl(temp, dest, 24);
                *ptr++ = subs_reg(temp, temp, immed, LSL, 0);
                *ptr++ = bfxil(dest, temp, 24, 8);
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

        RA_FreeARMRegister(&ptr, temp);
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
            if (immediate)
                *ptr++ = subs_immed(immed, tmp, u32);
            else
                *ptr++ = subs_reg(immed, tmp, immed, LSL, 0);
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
            *ptr++ = lsl(tmp, tmp, 16);
            *ptr++ = subs_reg(immed, tmp, immed, LSL, 0);
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
            *ptr++ = lsl(tmp, tmp, 24);
            *ptr++ = subs_reg(immed, tmp, immed, LSL, 0);
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
    return ptr;
}

uint32_t *EMIT_ADDI(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    uint16_t lo16;
    uint32_t u32 = 0;
    int add_immediate = 0;

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
#ifdef __aarch64__
            if (u32 < 4096)
            {
                add_immediate = 1;
            }
            else
            {
                if (u32 & 0xffff) {
                    *ptr++ = mov_immed_u16(immed, u32 & 0xffff, 0);
                    if ((u32 >> 16) & 0xffff) {
                        *ptr++ = movk_immed_u16(immed, u32 >> 16, 1);
                    }
                } else if (u32 & 0xffff0000) {
                    *ptr++ = mov_immed_u16(immed, u32 >> 16, 1);
                }
            }
#else
            *ptr++ = movw_immed_u16(immed, u32 & 0xffff);
            if (((u32 >> 16) & 0xffff) != 0)
                *ptr++ = movt_immed_u16(immed, u32 >> 16);
#endif
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
                if (add_immediate)
                    *ptr++ = adds_immed(dest, dest, u32);
                else
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
                if (add_immediate)
                    *ptr++ = adds_immed(immed, tmp, u32);
                else
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
    return ptr;
}

uint32_t *EMIT_ORI_TO_CCR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    (void)opcode;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint16_t val8 = BE16(*m68k_ptr[0]);

    /* Load immediate into the register */
    *ptr++ = mov_immed_u8(immed, val8 & 0x1f);
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
    (void)opcode;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t changed = RA_AllocARMRegister(&ptr);
    int16_t val = BE16((*m68k_ptr)[0]);
    uint8_t ctx = RA_GetCTX(&ptr);
    uint8_t sp = RA_MapM68kRegister(&ptr, 15);
    uint32_t *tmp;
    RA_SetDirtyM68kRegister(&ptr, 15);

    /* Load immediate into the register */
#ifdef __aarch64__
    *ptr++ = mov_immed_u16(immed, val & 0xf71f, 0);
    uint8_t cc = RA_ModifyCC(&ptr);
    
    ptr = EMIT_FlushPC(ptr);

    /* Test if supervisor mode is active */
    *ptr++ = ands_immed(31, cc, 1, 32 - SRB_S);
    
    /* OR is here */
    *ptr++ = mov_reg(changed, cc);
    
    *ptr++ = b_cc(A64_CC_EQ, 20);   
    *ptr++ = orr_reg(cc, cc, immed, LSL, 0);
    
    *ptr++ = eor_reg(changed, changed, cc, LSL, 0);
    *ptr++ = ands_immed(31, changed, 1, 32 - SRB_M);
    *ptr++ = b_cc(A64_CC_EQ, 8);

    *ptr++ = ands_immed(31, cc, 1, 32 - SRB_M);
    *ptr++ = b_cc(A64_CC_EQ, 4);
    *ptr++ = str_offset(ctx, sp, __builtin_offsetof(struct M68KState, ISP)); // Switching from ISP to MSP
    *ptr++ = ldr_offset(ctx, sp, __builtin_offsetof(struct M68KState, MSP));
    *ptr++ = b(3);
    *ptr++ = str_offset(ctx, sp, __builtin_offsetof(struct M68KState, MSP)); // Switching from MSP to ISP
    *ptr++ = ldr_offset(ctx, sp, __builtin_offsetof(struct M68KState, ISP));

    *ptr++ = add_immed(REG_PC, REG_PC, 4);
    
    *ptr++ = mvn_reg(changed, cc, LSL, 0);
    *ptr++ = ands_immed(31, changed, 3, 32 - SRB_IPL);
    *ptr++ = b_cc(A64_CC_EQ, 3);
    *ptr++ = msr_imm(3, 7, 7);
    *ptr++ = b(2);
    *ptr++ = msr_imm(3, 6, 7);

    tmp = ptr;
    *ptr++ = b_cc(A64_CC_AL, 10);

    /* No supervisor. Update USP, generate exception */
    ptr = EMIT_Exception(ptr, VECTOR_PRIVILEGE_VIOLATION, 0);
    
    *tmp = b_cc(A64_CC_AL, ptr - tmp);
    *ptr++ = (uint32_t)(uintptr_t)tmp;
    *ptr++ = 1;
    *ptr++ = 0;
    *ptr++ = INSN_TO_LE(0xfffffffe);
    *ptr++ = INSN_TO_LE(0xffffffff);
#else
    *ptr++ = udf(opcode);
#endif
    RA_FreeARMRegister(&ptr, immed);
    RA_FreeARMRegister(&ptr, changed);

    (*m68k_ptr) += 1;

    return ptr;
}

uint32_t *EMIT_ORI(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    uint16_t lo16;
    uint32_t u32;
    uint32_t mask32 = 0;
    uint32_t *tst_pos = (uint32_t *)NULL;

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
            mask32 = number_to_mask(u32);
            if (mask32 == 0 || mask32 == 0xffffffff)
            {
                mask32 = 0;
                *ptr++ = movw_immed_u16(immed, u32 & 0xffff);
                if (((u32 >> 16) & 0xffff) != 0)
                    *ptr++ = movt_immed_u16(immed, u32 >> 16);
            }
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
                if (mask32 == 0)
                    *ptr++ = orr_reg(dest, immed, dest, LSL, 0);
                else
                    *ptr++ = orr_immed(dest, dest, (mask32 >> 16) & 0x3f, (32 - (mask32 & 0x3f)) & 31);
                tst_pos = ptr;
                *ptr++ = cmn_reg(31, dest, LSL, 0);
                break;
            case 2:
                *ptr++ = orr_reg(immed, immed, dest, LSL, 16);
                tst_pos = ptr;
                *ptr++ = cmn_reg(31, immed, LSL, 0);
                *ptr++ = bfxil(dest, immed, 16, 16);
                break;
            case 1:
                *ptr++ = orr_reg(immed, immed, dest, LSL, 24);
                tst_pos = ptr;
                *ptr++ = cmn_reg(31, immed, LSL, 0);
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
                if (mask32 == 0)
                    *ptr++ = orr_reg(immed, immed, tmp, LSL, 0);
                else
                    *ptr++ = orr_immed(immed, tmp, (mask32 >> 16) & 0x3f, (32 - (mask32 & 0x3f)) & 31);
                tst_pos = ptr;
                *ptr++ = cmn_reg(31, immed, LSL, 0);
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
                tst_pos = ptr;
                *ptr++ = cmn_reg(31, immed, LSL, 0);
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
                tst_pos = ptr;
                *ptr++ = cmn_reg(31, immed, LSL, 0);
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

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);

        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
    } else {
        for (uint32_t *p = tst_pos; p < ptr; p++)
            p[0] = p[1];
        ptr--;
    }

    return ptr;
}

uint32_t *EMIT_ANDI_TO_CCR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    (void)opcode;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint16_t val = BE16(*m68k_ptr[0]);
   
    /* OR with status register, no need to check mask, ARM sequence way too short! */
#ifdef __aarch64__
    /* Load immediate into the register */
    *ptr++ = mov_immed_u16(immed, 0xff00 | (val & 0x1f), 0);
    uint8_t cc = RA_ModifyCC(&ptr);
    *ptr++ = and_reg(cc, cc, immed, LSL, 0);
#else
    /* Load immediate into the register */
    *ptr++ = mov_immed_u16(immed, 0xff00 | val);
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
    (void)opcode;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    int16_t val = BE16((*m68k_ptr)[0]);
    uint32_t *tmp;

    uint8_t changed = RA_AllocARMRegister(&ptr);
    uint8_t ctx = RA_GetCTX(&ptr);
    uint8_t sp = RA_MapM68kRegister(&ptr, 15);
    RA_SetDirtyM68kRegister(&ptr, 15);

    /* Load immediate into the register */
#ifdef __aarch64__
    *ptr++ = mov_immed_u16(immed, val & 0xf71f, 0);
    uint8_t cc = RA_ModifyCC(&ptr);
    
    ptr = EMIT_FlushPC(ptr);

    /* Test if supervisor mode is active */
    *ptr++ = ands_immed(31, cc, 1, 32 - SRB_S);

    /* AND is here */
    *ptr++ = mov_reg(changed, cc);
    *ptr++ = b_cc(A64_CC_EQ, 29);

    *ptr++ = and_reg(cc, cc, immed, LSL, 0);
    *ptr++ = eor_reg(changed, changed, cc, LSL, 0);

    *ptr++ = ands_immed(31, changed, 1, 32 - SRB_M);
    *ptr++ = b_cc(A64_CC_EQ, 8);

    *ptr++ = ands_immed(31, cc, 1, 32 - SRB_M);
    *ptr++ = b_cc(A64_CC_EQ, 4);
    *ptr++ = str_offset(ctx, sp, __builtin_offsetof(struct M68KState, ISP)); // Switching from ISP to MSP
    *ptr++ = ldr_offset(ctx, sp, __builtin_offsetof(struct M68KState, MSP));
    *ptr++ = b(3);
    *ptr++ = str_offset(ctx, sp, __builtin_offsetof(struct M68KState, MSP)); // Switching from MSP to ISP
    *ptr++ = ldr_offset(ctx, sp, __builtin_offsetof(struct M68KState, ISP));

    // No need to check if S was set - it cannot, sueprvisor can only switch it off
    *ptr++ = ands_immed(31, changed, 1, 32 - SRB_S);    
    *ptr++ = b_cc(A64_CC_EQ, 8);

    *ptr++ = ands_immed(31, cc, 1, 32 - SRB_M);
    *ptr++ = b_cc(A64_CC_EQ, 4);
    *ptr++ = str_offset(ctx, sp, __builtin_offsetof(struct M68KState, MSP)); // Switching from MSP to USP
    *ptr++ = ldr_offset(ctx, sp, __builtin_offsetof(struct M68KState, USP));
    *ptr++ = b(3);
    *ptr++ = str_offset(ctx, sp, __builtin_offsetof(struct M68KState, ISP)); // Switching from ISP to ISP
    *ptr++ = ldr_offset(ctx, sp, __builtin_offsetof(struct M68KState, USP));

    *ptr++ = add_immed(REG_PC, REG_PC, 4);

    *ptr++ = mvn_reg(changed, cc, LSL, 0);
    *ptr++ = ands_immed(31, changed, 3, 32 - SRB_IPL);
    *ptr++ = b_cc(A64_CC_EQ, 3);
    *ptr++ = msr_imm(3, 7, 7);
    *ptr++ = b(2);
    *ptr++ = msr_imm(3, 6, 7);

    tmp = ptr;
    *ptr++ = b_cc(A64_CC_AL, 10);

    /* No supervisor. Update USP, generate exception */
    ptr = EMIT_Exception(ptr, VECTOR_PRIVILEGE_VIOLATION, 0);
    
    *tmp = b_cc(A64_CC_AL, ptr - tmp);
    *ptr++ = (uint32_t)(uintptr_t)tmp;
    *ptr++ = 1;
    *ptr++ = 0;
    *ptr++ = INSN_TO_LE(0xfffffffe);
    *ptr++ = INSN_TO_LE(0xffffffff);
#else
    *ptr++ = udf(opcode);
#endif
    RA_FreeARMRegister(&ptr, immed);
    RA_FreeARMRegister(&ptr, changed);

    (*m68k_ptr) += 1;

    return ptr;
}

uint32_t *EMIT_ANDI(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    int16_t lo16;
    uint32_t u32;
    uint32_t mask32 = 0;

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
            mask32 = number_to_mask(u32);
            if (mask32 == 0 || mask32 == 0xffffffff)
            {
                mask32 = 0;
                *ptr++ = movw_immed_u16(immed, u32 & 0xffff);
                if (((u32 >> 16) & 0xffff) != 0)
                    *ptr++ = movt_immed_u16(immed, u32 >> 16);
            }
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
                if (mask32 == 0)
                    *ptr++ = ands_reg(dest, immed, dest, LSL, 0);
                else
                    *ptr++ = ands_immed(dest, dest, (mask32 >> 16) & 0x3f, (32 - (mask32 & 0x3f)) & 31);
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
                if (mask32 == 0)
                    *ptr++ = ands_reg(immed, immed, tmp, LSL, 0);
                else
                    *ptr++ = ands_immed(immed, tmp, (mask32 >> 16) & 0x3f, (32 - (mask32 & 0x3f)) & 31);
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
    *ptr++ = mov_immed_u8(immed, val & 0x1f);
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
    (void)opcode;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    int16_t val = BE16((*m68k_ptr)[0]);
    uint32_t *tmp;

    uint8_t changed = RA_AllocARMRegister(&ptr);
    uint8_t ctx = RA_GetCTX(&ptr);
    uint8_t sp = RA_MapM68kRegister(&ptr, 15);
    RA_SetDirtyM68kRegister(&ptr, 15);

    /* Load immediate into the register */
#ifdef __aarch64__
    *ptr++ = mov_immed_u16(immed, val & 0xf71f, 0);
    uint8_t cc = RA_ModifyCC(&ptr);
    
    ptr = EMIT_FlushPC(ptr);

    /* Test if supervisor mode is active */
    *ptr++ = ands_immed(31, cc, 1, 32 - SRB_S);
    
    /* EOR is here */
    *ptr++ = mov_reg(changed, cc);
    *ptr++ = b_cc(A64_CC_EQ, 29);
    *ptr++ = eor_reg(cc, cc, immed, LSL, 0);
    *ptr++ = eor_reg(changed, changed, cc, LSL, 0);

    *ptr++ = ands_immed(31, changed, 1, 32 - SRB_M);
    *ptr++ = b_cc(A64_CC_EQ, 8);

    *ptr++ = ands_immed(31, cc, 1, 32 - SRB_M);
    *ptr++ = b_cc(A64_CC_EQ, 4);
    *ptr++ = str_offset(ctx, sp, __builtin_offsetof(struct M68KState, ISP)); // Switching from ISP to MSP
    *ptr++ = ldr_offset(ctx, sp, __builtin_offsetof(struct M68KState, MSP));
    *ptr++ = b(3);
    *ptr++ = str_offset(ctx, sp, __builtin_offsetof(struct M68KState, MSP)); // Switching from MSP to ISP
    *ptr++ = ldr_offset(ctx, sp, __builtin_offsetof(struct M68KState, ISP));

    // No need to check if S was set - it cannot, sueprvisor can only switch it off
    *ptr++ = ands_immed(31, changed, 1, 32 - SRB_S);    
    *ptr++ = b_cc(A64_CC_EQ, 8);

    *ptr++ = ands_immed(31, cc, 1, 32 - SRB_M);
    *ptr++ = b_cc(A64_CC_EQ, 4);
    *ptr++ = str_offset(ctx, sp, __builtin_offsetof(struct M68KState, MSP)); // Switching from MSP to USP
    *ptr++ = ldr_offset(ctx, sp, __builtin_offsetof(struct M68KState, USP));
    *ptr++ = b(3);
    *ptr++ = str_offset(ctx, sp, __builtin_offsetof(struct M68KState, ISP)); // Switching from ISP to ISP
    *ptr++ = ldr_offset(ctx, sp, __builtin_offsetof(struct M68KState, USP));

    *ptr++ = add_immed(REG_PC, REG_PC, 4);

    *ptr++ = mvn_reg(changed, cc, LSL, 0);
    *ptr++ = ands_immed(31, changed, 3, 32 - SRB_IPL);
    *ptr++ = b_cc(A64_CC_EQ, 3);
    *ptr++ = msr_imm(3, 7, 7);
    *ptr++ = b(2);
    *ptr++ = msr_imm(3, 6, 7);

    tmp = ptr;
    *ptr++ = b_cc(A64_CC_AL, 10);

    /* No supervisor. Update USP, generate exception */
    ptr = EMIT_Exception(ptr, VECTOR_PRIVILEGE_VIOLATION, 0);
    
    *tmp = b_cc(A64_CC_AL, ptr - tmp);
    *ptr++ = (uint32_t)(uintptr_t)tmp;
    *ptr++ = 1;
    *ptr++ = 0;
    *ptr++ = INSN_TO_LE(0xfffffffe);
    *ptr++ = INSN_TO_LE(0xffffffff);
#else
    *ptr++ = udf(opcode);
#endif
    RA_FreeARMRegister(&ptr, immed);
    RA_FreeARMRegister(&ptr, changed);

    (*m68k_ptr) += 1;

    return ptr;
}

uint32_t *EMIT_EORI(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t ext_count = 0;
    uint8_t immed = RA_AllocARMRegister(&ptr);
    uint8_t dest = 0xff;
    uint8_t size = 0;
    int16_t lo16;
    uint32_t u32;
    uint32_t mask32 = 0;
    uint32_t *tst_pos = (uint32_t *)NULL;

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
            mask32 = number_to_mask(u32);
            if (mask32 == 0 || mask32 == 0xffffffff)
            {
                mask32 = 0;
                *ptr++ = movw_immed_u16(immed, u32 & 0xffff);
                if (((u32 >> 16) & 0xffff) != 0)
                    *ptr++ = movt_immed_u16(immed, u32 >> 16);
            }
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
                if (mask32 == 0)
                    *ptr++ = eor_reg(dest, dest, immed, LSL, 0);
                else
                    *ptr++ = eor_immed(dest, dest, (mask32 >> 16) & 0x3f, (32 - (mask32 & 0x3f)) & 31);
                tst_pos = ptr;
                *ptr++ = cmn_reg(31, dest, LSL, 0);
                break;
            case 2:
                *ptr++ = eor_reg(immed, immed, dest, LSL, 16);
                tst_pos = ptr;
                *ptr++ = cmn_reg(31, immed, LSL, 0);
                *ptr++ = bfxil(dest, immed, 16, 16);
                break;
            case 1:
                *ptr++ = eor_reg(immed, immed, dest, LSL, 24);
                tst_pos = ptr;
                *ptr++ = cmn_reg(31, immed, LSL, 0);
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
                if (mask32 == 0)
                    *ptr++ = eor_reg(immed, immed, tmp, LSL, 0);
                else
                    *ptr++ = eor_immed(immed, tmp, (mask32 >> 16) & 0x3f, (32 - (mask32 & 0x3f)) & 31);
                tst_pos = ptr;
                *ptr++ = cmn_reg(31, immed, LSL, 0);
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
                tst_pos = ptr;
                *ptr++ = cmn_reg(31, immed, LSL, 0);
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
                tst_pos = ptr;
                *ptr++ = cmn_reg(31, immed, LSL, 0);
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

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
    } else {
        for (uint32_t *p = tst_pos; p < ptr; p++)
            p[0] = p[1];
        ptr--;
    }

    return ptr;
}

uint32_t *EMIT_BTST(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
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
        bit_number = RA_CopyFromM68kRegister(&ptr, (opcode >> 9) & 7);
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
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t ext_count = 0;
    uint8_t bit_number = 0xff;
    uint8_t dest = 0xff;
    uint8_t bit_mask = 0xff;
    int imm_shift = 0;
    int immediate = 0;
    uint32_t *tst_pos;

    /* Get the bit number either as immediate or from register */
    if ((opcode & 0xffc0) == 0x0840)
    {
        immediate = 1;
        imm_shift = BE16((*m68k_ptr)[ext_count++]) & 31;
    }
    else
    {
        bit_number = RA_CopyFromM68kRegister(&ptr, (opcode >> 9) & 7);
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
            tst_pos = ptr;
            *ptr++ = tst_immed(dest, 1, 31 & (32 - imm_shift));
            *ptr++ = eor_immed(dest, dest, 1, 31 & (32 - imm_shift));
#else
            int shifter_operand = imm_shift & 1 ? 2:1;
            shifter_operand |= ((16 - (imm_shift >> 1)) & 15) << 8;

            tst_pos = ptr;
            *ptr++ = tst_immed(dest, shifter_operand);
            *ptr++ = eor_immed(dest, dest, shifter_operand);
#endif
        }
        else
        {
#ifdef __aarch64__
            *ptr++ = and_immed(bit_number, bit_number, 5, 0);
            *ptr++ = lslv(bit_mask, bit_mask, bit_number);

            tst_pos = ptr;
            *ptr++ = tst_reg(dest, bit_mask, LSL, 0);
            *ptr++ = eor_reg(dest, dest, bit_mask, LSL, 0);
#else
            *ptr++ = and_immed(bit_number, bit_number, 31);
            *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

            tst_pos = ptr;
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
            tst_pos = ptr;
            *ptr++ = tst_immed(tmp, 1, 31 & (32 - (imm_shift & 7)));
            *ptr++ = eor_immed(tmp, tmp, 1, 31 & (32 - (imm_shift & 7)));
#else
            int shifter_operand = 1 << (imm_shift & 7);
            tst_pos = ptr;
            *ptr++ = tst_immed(tmp, shifter_operand);
            *ptr++ = eor_immed(tmp, tmp, shifter_operand);
#endif
        }
        else
        {
#ifdef __aarch64__
            *ptr++ = and_immed(bit_number, bit_number, 3, 0);
            *ptr++ = lslv(bit_mask, bit_mask, bit_number);

            tst_pos = ptr;
            *ptr++ = tst_reg(tmp, bit_mask, LSL, 0);
            *ptr++ = eor_reg(tmp, tmp, bit_mask, LSL, 0);
#else
            *ptr++ = and_immed(bit_number, bit_number, 7);
            *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

            tst_pos = ptr;
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

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_ClearFlags(ptr, cc, update_mask);
        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
    } else {
        for (uint32_t *p = tst_pos; p < ptr; p++)
            p[0] = p[1];
        ptr--;
    }

    return ptr;
}

uint32_t *EMIT_BCLR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t ext_count = 0;
    uint8_t bit_number = 0xff;
    uint8_t dest = 0xff;
    uint8_t bit_mask = 0xff;
    int imm_shift = 0;
    int immediate = 0;
    uint32_t *tst_pos;

    /* Get the bit number either as immediate or from register */
    if ((opcode & 0xffc0) == 0x0880)
    {
        immediate = 1;
        imm_shift = BE16((*m68k_ptr)[ext_count++]) & 31;
    }
    else
    {
        bit_number = RA_CopyFromM68kRegister(&ptr, (opcode >> 9) & 7);
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
            tst_pos = ptr;
            *ptr++ = tst_immed(dest, 1, 31 & (32 - imm_shift));
            *ptr++ = bic_immed(dest, dest, 1, 31 & (32 - imm_shift));
#else
            int shifter_operand = imm_shift & 1 ? 2:1;
            shifter_operand |= ((16 - (imm_shift >> 1)) & 15) << 8;

            tst_pos = ptr;
            *ptr++ = tst_immed(dest, shifter_operand);
            *ptr++ = bic_immed(dest, dest, shifter_operand);
#endif
        }
        else
        {
#ifdef __aarch64__
            *ptr++ = and_immed(bit_number, bit_number, 5, 0);
            *ptr++ = lslv(bit_mask, bit_mask, bit_number);

            tst_pos = ptr;
            *ptr++ = tst_reg(dest, bit_mask, LSL, 0);
            *ptr++ = bic_reg(dest, dest, bit_mask, LSL, 0);
#else
            *ptr++ = and_immed(bit_number, bit_number, 31);
            *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

            tst_pos = ptr;
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
            tst_pos = ptr;
            *ptr++ = tst_immed(tmp, 1, 31 & (32 - (imm_shift & 7)));
            *ptr++ = bic_immed(tmp, tmp, 1, 31 & (32 - (imm_shift & 7)));
#else
            int shifter_operand = 1 << (imm_shift & 7);
            tst_pos = ptr;
            *ptr++ = tst_immed(tmp, shifter_operand);
            *ptr++ = bic_immed(tmp, tmp, shifter_operand);
#endif
        }
        else
        {
#ifdef __aarch64__
            *ptr++ = and_immed(bit_number, bit_number, 3, 0);
            *ptr++ = lslv(bit_mask, bit_mask, bit_number);

            tst_pos = ptr;
            *ptr++ = tst_reg(tmp, bit_mask, LSL, 0);
            *ptr++ = bic_reg(tmp, tmp, bit_mask, LSL, 0);
#else
            *ptr++ = and_immed(bit_number, bit_number, 7);
            *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

            tst_pos = ptr;
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

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_ClearFlags(ptr, cc, update_mask);
        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
    } else {
        for (uint32_t *p = tst_pos; p < ptr; p++)
            p[0] = p[1];
        ptr--;
    }

    return ptr;
}

uint32_t *EMIT_CMP2(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
#ifdef __aarch64__
    uint32_t opcode_address = (uint32_t)(uintptr_t)((*m68k_ptr) - 1);
    uint8_t update_mask = SR_Z | SR_C;
    uint8_t ext_words = 1;
    uint16_t opcode2 = BE16((*m68k_ptr)[0]);
    uint8_t ea = -1;
    uint8_t lower = RA_AllocARMRegister(&ptr);
    uint8_t higher = RA_AllocARMRegister(&ptr);
    uint8_t reg = RA_MapM68kRegister(&ptr, opcode2 >> 12);

    /* Get address of bounds */
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &ea, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

    /* load bounds into registers */
    switch ((opcode >> 9) & 3)
    {
        case 0:
            *ptr++ = ldrsb_offset(ea, lower, 0);
            *ptr++ = ldrsb_offset(ea, higher, 1);
            break;
        case 1:
            *ptr++ = ldrsh_offset(ea, lower, 0);
            *ptr++ = ldrsh_offset(ea, higher, 2);
            break;
        case 2:
            *ptr++ = ldp(ea, lower, higher, 0);
            break;
    }

    /* If data register, extend the 8 or 16 bit */
    if ((opcode2 & 0x8000) == 0)
    {
        uint8_t tmp = -1;
        switch((opcode >> 9) & 3)
        {
            case 0:
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = sxtb(tmp, reg);
                reg = tmp;
                break;
            case 1:
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = sxth(tmp, reg);
                reg = tmp;
                break;
        }
    }

    *ptr++ = subs_reg(31, reg, lower, LSL, 0);
    *ptr++ = b_cc(A64_CC_LE, 2);
    *ptr++ = subs_reg(31, higher, reg,  LSL, 0);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);

        if (__builtin_popcount(update_mask) > 1)
            ptr = EMIT_GetNZVnC(ptr, cc, &update_mask);
        else
            ptr = EMIT_ClearFlags(ptr, cc, update_mask);
            
        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_C)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C, ARM_CC_CC);
    }

    RA_FreeARMRegister(&ptr, ea);
    RA_FreeARMRegister(&ptr, reg);
    RA_FreeARMRegister(&ptr, lower);
    RA_FreeARMRegister(&ptr, higher);

    /* If CHK2 opcode then emit exception if tested value was out of range (C flag set) */
    if (opcode2 & (1 << 11))
    {
        /* Flush program counter, since it might be pushed on the stack when
        exception is generated */
        ptr = EMIT_FlushPC(ptr);

        /* Skip exception if tested value is in range */
        uint32_t *t = ptr;
        *ptr++ = b_cc(A64_CC_CS, 0);

        /* Emit CHK exception */
        ptr = EMIT_Exception(ptr, VECTOR_CHK, 2, opcode_address);
        *t = b_cc(A64_CC_CS, ptr - t);
        *ptr++ = (uint32_t)(uintptr_t)t;
        *ptr++ = 1;
        *ptr++ = 0;
        *ptr++ = INSN_TO_LE(0xfffffffe);
    }
#else
    ptr = EMIT_InjectDebugString(ptr, "[JIT] CMP2/CHK2 at %08x not implemented\n", *m68k_ptr - 1);
    ptr = EMIT_InjectPrintContext(ptr);
    *ptr++ = udf(opcode);
#endif

    return ptr;
}

uint32_t *EMIT_BSET(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t ext_count = 0;
    uint8_t bit_number = 0xff;
    uint8_t dest = 0xff;
    uint8_t bit_mask = 0xff;
    int imm_shift = 0;
    int immediate = 0;
    uint32_t *tst_pos;

    /* Get the bit number either as immediate or from register */
    if ((opcode & 0xffc0) == 0x08c0)
    {
        immediate = 1;
        imm_shift = BE16((*m68k_ptr)[ext_count++]) & 31;
    }
    else
    {
        bit_number = RA_CopyFromM68kRegister(&ptr, (opcode >> 9) & 7);
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
            tst_pos = ptr;
            *ptr++ = tst_immed(dest, 1, 31 & (32 - imm_shift));
            *ptr++ = orr_immed(dest, dest, 1, 31 & (32 - imm_shift));
#else
            int shifter_operand = imm_shift & 1 ? 2:1;
            shifter_operand |= ((16 - (imm_shift >> 1)) & 15) << 8;

            tst_pos = ptr;
            *ptr++ = tst_immed(dest, shifter_operand);
            *ptr++ = orr_immed(dest, dest, shifter_operand);
#endif
        }
        else
        {
#ifdef __aarch64__
            *ptr++ = and_immed(bit_number, bit_number, 5, 0);
            *ptr++ = lslv(bit_mask, bit_mask, bit_number);

            tst_pos = ptr;
            *ptr++ = tst_reg(dest, bit_mask, LSL, 0);
            *ptr++ = orr_reg(dest, dest, bit_mask, LSL, 0);
#else
            *ptr++ = and_immed(bit_number, bit_number, 31);
            *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);

            tst_pos = ptr;
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
            tst_pos = ptr;
            *ptr++ = tst_immed(tmp, 1, 31 & (32 - (imm_shift & 7)));
            *ptr++ = orr_immed(tmp, tmp, 1, 31 & (32 - (imm_shift & 7)));
#else
            int shifter_operand = 1 << (imm_shift & 7);
            tst_pos = ptr;
            *ptr++ = tst_immed(tmp, shifter_operand);
            *ptr++ = orr_immed(tmp, tmp, shifter_operand);
#endif
        }
        else
        {
#ifdef __aarch64__
            *ptr++ = and_immed(bit_number, bit_number, 3, 0);
            *ptr++ = lslv(bit_mask, bit_mask, bit_number);

            tst_pos = ptr;
            *ptr++ = tst_reg(tmp, bit_mask, LSL, 0);
            *ptr++ = orr_reg(tmp, tmp, bit_mask, LSL, 0);
#else
            *ptr++ = and_immed(bit_number, bit_number, 7);
            *ptr++ = lsl_reg(bit_mask, bit_mask, bit_number);
            
            tst_pos = ptr;
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

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_ClearFlags(ptr, cc, update_mask);
        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
    } else {
        for (uint32_t *p = tst_pos; p < ptr; p++)
            p[0] = p[1];
        ptr--;
    }

    return ptr;
}

uint32_t *EMIT_CAS2(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_CAS")));
uint32_t *EMIT_CAS(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
#ifdef __aarch64__

#define CAS_ATOMIC() do { \
        uint32_t *l0 = ptr; \
        switch (size) \
        { \
            case 1:\
                *ptr++ = ldxrb(ea, tmp);\
                *ptr++ = lsl(tmp, tmp, 24);\
                *ptr++ = subs_reg(31, tmp, dc, LSL, 24);\
                break;\
            case 2:\
                *ptr++ = ldxrh(ea, tmp);\
                *ptr++ = lsl(tmp, tmp, 16);\
                *ptr++ = subs_reg(31, tmp, dc, LSL, 16);\
                break;\
            case 3:\
                *ptr++ = ldxr(ea, tmp);\
                *ptr++ = subs_reg(31, tmp, dc, LSL, 0);\
                break;\
        }\
        uint32_t *b0 = ptr;\
        *ptr++ = b_cc(A64_CC_NE, 0);\
        switch (size)\
        {\
            case 1:\
                *ptr++ = stlxrb(ea, du, status);\
                break;\
            case 2:\
                *ptr++ = stlxrh(ea, du, status);\
                break;\
            case 3:\
                *ptr++ = stlxr(ea, du, status);\
                break;\
        }\
        *ptr = cbnz(status, l0 - ptr);\
        ptr++;\
        *ptr++ = b(2);\
        *b0 = b_cc(A64_CC_NE, ptr - b0);\
        switch (size) \
        {\
            case 1:\
                *ptr++ = bfxil(dc, tmp, 24, 8);\
                break;\
            case 2:\
                *ptr++ = bfxil(dc, tmp, 16, 16);\
                break;\
            case 3:\
                *ptr++ = mov_reg(dc, tmp);\
                break;\
        }\
} while(0)

#define CAS_UNSAFE() do { \
        switch (size) \
        { \
            case 1:\
                *ptr++ = ldrb_offset(ea, tmp, 0);\
                *ptr++ = lsl(tmp, tmp, 24);\
                *ptr++ = subs_reg(31, tmp, dc, LSL, 24);\
                break;\
            case 2:\
                *ptr++ = ldrh_offset(ea, tmp, 0);\
                *ptr++ = lsl(tmp, tmp, 16);\
                *ptr++ = subs_reg(31, tmp, dc, LSL, 16);\
                break;\
            case 3:\
                *ptr++ = ldr_offset(ea, tmp, 0);\
                *ptr++ = subs_reg(31, tmp, dc, LSL, 0);\
                break;\
        }\
        uint32_t *b0 = ptr;\
        *ptr++ = b_cc(A64_CC_NE, 0);\
        switch (size)\
        {\
            case 1:\
                *ptr++ = strb_offset(ea, du, 0);\
                break;\
            case 2:\
                *ptr++ = strh_offset(ea, du, 0);\
                break;\
            case 3:\
                *ptr++ = str_offset(ea, du, 0);\
                break;\
        }\
        *ptr++ = b(2);\
        *b0 = b_cc(A64_CC_NE, ptr - b0);\
        switch (size) \
        {\
            case 1:\
                *ptr++ = bfxil(dc, tmp, 24, 8);\
                break;\
            case 2:\
                *ptr++ = bfxil(dc, tmp, 16, 16);\
                break;\
            case 3:\
                *ptr++ = mov_reg(dc, tmp);\
                break;\
        }\
} while(0)

    /* CAS2 */
    if ((opcode & 0xfdff) == 0x0cfc)
    {
        uint8_t ext_words = 2;
        uint8_t size = (opcode >> 9) & 3;
        uint16_t opcode2 = BE16((*m68k_ptr)[0]);
        uint16_t opcode3 = BE16((*m68k_ptr)[1]);

        uint8_t rn1 = RA_MapM68kRegister(&ptr, (opcode2 >> 12) & 15);
        uint8_t rn2 = RA_MapM68kRegister(&ptr, (opcode3 >> 12) & 15);
        uint8_t du1 = RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t du2 = RA_MapM68kRegister(&ptr, (opcode3 >> 6) & 7);
        uint8_t dc1 = RA_MapM68kRegister(&ptr, (opcode2) & 7);
        uint8_t dc2 = RA_MapM68kRegister(&ptr, (opcode3) & 7);

        RA_SetDirtyM68kRegister(&ptr, (opcode2) & 7);
        RA_SetDirtyM68kRegister(&ptr, (opcode3) & 7);

        uint8_t val1 = RA_AllocARMRegister(&ptr);
        uint8_t val2 = RA_AllocARMRegister(&ptr);

        if (size==2)
        {
            *ptr++ = ldrh_offset(rn1, val1, 0);
            *ptr++ = ldrh_offset(rn2, val2, 0);
            *ptr++ = lsl(val1, val1, 16);
            *ptr++ = lsl(val2, val2, 16);
            *ptr++ = subs_reg(31, val1, dc1, LSL, 16);
            *ptr++ = b_cc(A64_CC_NE, 6);
            *ptr++ = subs_reg(31, val2, dc2, LSL, 16);
            *ptr++ = b_cc(A64_CC_NE, 4);
            *ptr++ = strh_offset(rn1, du1, 0);
            *ptr++ = strh_offset(rn2, du2, 0);
            *ptr++ = b(3);
            *ptr++ = bfxil(dc1, val1, 16, 16);
            *ptr++ = bfxil(dc2, val2, 16, 16);
        }
        else
        {
            *ptr++ = ldr_offset(rn1, val1, 0);
            *ptr++ = ldr_offset(rn2, val2, 0);
            *ptr++ = subs_reg(31, val1, dc1, LSL, 0);
            *ptr++ = b_cc(A64_CC_NE, 6);
            *ptr++ = subs_reg(31, val2, dc2, LSL, 0);
            *ptr++ = b_cc(A64_CC_NE, 4);
            *ptr++ = str_offset(rn1, du1, 0);
            *ptr++ = str_offset(rn2, du2, 0);
            *ptr++ = b(3);
            *ptr++ = mov_reg(dc1, val1);
            *ptr++ = mov_reg(dc2, val2);
        }

        *ptr++ = dmb_ish();

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);

            if (__builtin_popcount(update_mask) > 1)
                ptr = EMIT_GetNZVnC(ptr, cc, &update_mask);
            else
                ptr = EMIT_ClearFlags(ptr, cc, update_mask);
                
            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_VS);
            if (update_mask & SR_C)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C, ARM_CC_CC);
        }

        RA_FreeARMRegister(&ptr, val1);
        RA_FreeARMRegister(&ptr, val2);
    }
    /* CAS */
    else
    {
        uint8_t ext_words = 1;
        uint16_t opcode2 = BE16((*m68k_ptr)[0]);
        uint8_t ea = -1;
        uint8_t du = RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);
        uint8_t dc = RA_MapM68kRegister(&ptr, opcode2 & 7);
        uint8_t status = RA_AllocARMRegister(&ptr);
        RA_SetDirtyM68kRegister(&ptr, opcode2 & 7);

        uint8_t size = (opcode >> 9) & 3;
        uint8_t mode = (opcode >> 3) & 7;

        uint8_t tmp = RA_AllocARMRegister(&ptr);

        /* Load effective address */
        if (mode == 4 || mode == 3)
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &ea, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);
        else
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &ea, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

        if (mode == 4)
        {
            if (size == 2 || (size == 1 && (opcode & 7) == 7))
            {
                *ptr++ = sub_immed(ea, ea, 2);
            }
            else if (size == 3)
            {
                *ptr++ = sub_immed(ea, ea, 4);
            }
            else
            {
                *ptr++ = sub_immed(ea, ea, 1);
            }
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
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
                    if (BE16((*m68k_ptr)[1]) & 1)
                        CAS_UNSAFE();
                    else
                        CAS_ATOMIC();
                    break;
                case 3:
                    if ((BE16((*m68k_ptr)[1]) & 3) == 0)
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
                    if (BE16((*m68k_ptr)[2]) & 1)
                        CAS_UNSAFE();
                    else
                        CAS_ATOMIC();
                    break;
                case 3:
                    if ((BE16((*m68k_ptr)[2]) & 3) == 0)
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
                    *ptr++ = ands_immed(31, ea, 2, 0);
                    break;
                case 2:
                    *ptr++ = ands_immed(31, ea, 1, 0);
                    break;
            }

            b_eq = ptr;
            *ptr++ = b_cc(A64_CC_EQ, 0);
            CAS_UNSAFE();
            b_ = ptr;
            *ptr++ = b(0);
            CAS_ATOMIC();
            *b_ = b(ptr - b_);
            *b_eq = b_cc(A64_CC_EQ, 1 + b_ - b_eq);
        }

        *ptr++ = dmb_ish();

        if (mode == 3)
        {
            if (size == 2 || (size == 1 && (opcode & 7) == 7))
            {
                *ptr++ = add_immed(ea, ea, 2);
            }
            else if (size == 3)
            {
                *ptr++ = add_immed(ea, ea, 4);
            }
            else
            {
                *ptr++ = add_immed(ea, ea, 1);
            }
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);

            if (__builtin_popcount(update_mask) > 1)
                ptr = EMIT_GetNZVnC(ptr, cc, &update_mask);
            else
                ptr = EMIT_ClearFlags(ptr, cc, update_mask);
                
            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_VS);
            if (update_mask & SR_C)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C, ARM_CC_CC);
        }

        RA_FreeARMRegister(&ptr, ea);
        RA_FreeARMRegister(&ptr, status);
        RA_FreeARMRegister(&ptr, tmp);
    }
#else
    ptr = EMIT_InjectDebugString(ptr, "[JIT] CAS/CAS2 at %08x not implemented\n", *m68k_ptr - 1);
    ptr = EMIT_InjectPrintContext(ptr);
    *ptr++ = udf(opcode);
#endif
    return ptr;
}

uint32_t *EMIT_MOVEP(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
#ifdef __aarch64__
    int16_t offset = BE16((*m68k_ptr)[0]);
    uint8_t an = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
    uint8_t dn = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    uint8_t addr = an;

    /* For offset == 0 just use the m68k register */
    if (offset != 0) {
        /* For all other offsets get a temporary reg for address */
        if (offset > 0) {
            if ((offset & 0xfff) && (offset > 0xfff-8)) {
                if (addr == an) {
                    addr = RA_AllocARMRegister(&ptr);
                }
                *ptr++ = add_immed(addr, an, offset & 0xfff);
                offset &= 0xf000;
            }
            if (offset & 0x7000) {
                if (addr == an) {
                    addr = RA_AllocARMRegister(&ptr);
                    *ptr++ = add_immed_lsl12(addr, an, offset >> 12);
                }
                else
                {
                    *ptr++ = add_immed_lsl12(addr, addr, offset >> 12);
                }
                offset &= 0xfff;
            }
        }
        else {
            offset = -offset;
            if (offset & 0xfff) {
                if (addr == an) {
                    addr = RA_AllocARMRegister(&ptr);
                }
                *ptr++ = sub_immed(addr, an, offset & 0xfff);
                offset &= 0xf000;
            }
            if (offset & 0x7000) {
                if (addr == an) {
                    addr = RA_AllocARMRegister(&ptr);
                    *ptr++ = sub_immed_lsl12(addr, an, offset >> 12);
                }
                else
                {
                    *ptr++ = sub_immed_lsl12(addr, addr, offset >> 12);
                }
                offset &= 0xfff;
            }
        }
    }

    /* Register to memory transfer */
    if (opcode & 0x80) {
        /* Long mode */
        if (opcode & 0x40) {
            *ptr++ = lsr(tmp, dn, 24);
            *ptr++ = strb_offset(addr, tmp, offset);
            *ptr++ = lsr(tmp, dn, 16);
            *ptr++ = strb_offset(addr, tmp, offset + 2);
            *ptr++ = lsr(tmp, dn, 8);
            *ptr++ = strb_offset(addr, tmp, offset + 4);
            *ptr++ = strb_offset(addr, dn, offset + 6);
        }
        /* Word mode */
        else {
            *ptr++ = lsr(tmp, dn, 8);
            *ptr++ = strb_offset(addr, tmp, offset);
            *ptr++ = strb_offset(addr, dn, offset + 2);
        }
    }
    /* Memory to register transfer */
    else {
        RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
        
        /* Long mode */
        if (opcode & 0x40) {
            *ptr++ = ldrb_offset(addr, dn, offset);
            *ptr++ = ldrb_offset(addr, tmp, offset + 2);
            *ptr++ = lsl(dn, dn, 24);
            *ptr++ = orr_reg(dn, dn, tmp, LSL, 16);
            *ptr++ = ldrb_offset(addr, tmp, offset + 4);
            *ptr++ = orr_reg(dn, dn, tmp, LSL, 8);
            *ptr++ = ldrb_offset(addr, tmp, offset + 6);
            *ptr++ = orr_reg(dn, dn, tmp, LSL, 0);
        }
        /* Word mode */
        else {
            *ptr++ = bic_immed(dn, dn, 16, 0);
            *ptr++ = ldrb_offset(addr, tmp, offset);
            *ptr++ = orr_reg(dn, dn, tmp, LSL, 8);
            *ptr++ = ldrb_offset(addr, tmp, offset + 2);
            *ptr++ = orr_reg(dn, dn, tmp, LSL, 0);
        }
    }

    RA_FreeARMRegister(&ptr, addr);
    RA_FreeARMRegister(&ptr, tmp);
    ptr = EMIT_AdvancePC(ptr, 4);
    (*m68k_ptr)++;
#else
    ptr = EMIT_InjectDebugString(ptr, "[JIT] MOVEP at %08x not implemented\n", *m68k_ptr - 1);
    ptr = EMIT_InjectPrintContext(ptr);
    *ptr++ = udf(opcode);
#endif
    return ptr;
}

uint32_t *EMIT_MOVES(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    (void)opcode;
    (void)m68k_ptr;
    /* todo... */
    return ptr;
}

static struct OpcodeDef InsnTable[4096] = {
	[0x03c] = { { EMIT_ORI_TO_CCR }, NULL, SR_S | SR_CCR, SR_CCR, 2, 0, 2 },
	[0x07c] = { { EMIT_ORI_TO_SR }, NULL, SR_ALL, SR_ALL, 2, 0, 2  },
	[0x23c] = { { EMIT_ANDI_TO_CCR }, NULL, SR_S | SR_CCR, SR_CCR, 2, 0, 2 },
	[0x27c] = { { EMIT_ANDI_TO_SR }, NULL, SR_ALL, SR_ALL, 2, 0, 2 },
	[0xa3c] = { { EMIT_EORI_TO_CCR }, NULL, SR_S | SR_CCR, SR_CCR, 2, 0, 2 },
	[0xa7c] = { { EMIT_EORI_TO_SR }, NULL, SR_ALL, SR_ALL, 2, 0, 2 },

	[00000 ... 00007] = { { EMIT_ORI }, NULL, 0, SR_NZVC, 2, 0, 1 },
	[00020 ... 00047] = { { EMIT_ORI }, NULL, 0, SR_NZVC, 2, 0, 1 },
	[00050 ... 00071] = { { EMIT_ORI }, NULL, 0, SR_NZVC, 2, 1, 1 },
	[00100 ... 00107] = { { EMIT_ORI }, NULL, 0, SR_NZVC, 2, 0, 2 },
	[00120 ... 00147] = { { EMIT_ORI }, NULL, 0, SR_NZVC, 2, 0, 2 },
	[00150 ... 00171] = { { EMIT_ORI }, NULL, 0, SR_NZVC, 2, 1, 2 },
	[00200 ... 00207] = { { EMIT_ORI }, NULL, 0, SR_NZVC, 3, 0, 4 },
	[00220 ... 00247] = { { EMIT_ORI }, NULL, 0, SR_NZVC, 3, 0, 4 },
	[00250 ... 00271] = { { EMIT_ORI }, NULL, 0, SR_NZVC, 3, 1, 4 },



	[01000 ... 01007] = { { EMIT_ANDI }, NULL, 0, SR_NZVC, 2, 0, 1 },
	[01020 ... 01047] = { { EMIT_ANDI }, NULL, 0, SR_NZVC, 2, 0, 1 },
	[01050 ... 01071] = { { EMIT_ANDI }, NULL, 0, SR_NZVC, 2, 1, 1 },
	[01100 ... 01107] = { { EMIT_ANDI }, NULL, 0, SR_NZVC, 2, 0, 2 },
	[01120 ... 01147] = { { EMIT_ANDI }, NULL, 0, SR_NZVC, 2, 0, 2 },
	[01150 ... 01171] = { { EMIT_ANDI }, NULL, 0, SR_NZVC, 2, 1, 2 },
	[01200 ... 01207] = { { EMIT_ANDI }, NULL, 0, SR_NZVC, 3, 0, 4 },
	[01220 ... 01247] = { { EMIT_ANDI }, NULL, 0, SR_NZVC, 3, 0, 4 },
	[01250 ... 01271] = { { EMIT_ANDI }, NULL, 0, SR_NZVC, 3, 1, 4 },

	[02000 ... 02007] = { { EMIT_SUBI }, NULL, 0, SR_CCR, 2, 0, 1 },
	[02020 ... 02047] = { { EMIT_SUBI }, NULL, 0, SR_CCR, 2, 0, 1 },
	[02050 ... 02071] = { { EMIT_SUBI }, NULL, 0, SR_CCR, 2, 1, 1 },
	[02100 ... 02107] = { { EMIT_SUBI }, NULL, 0, SR_CCR, 2, 0, 2 },
	[02120 ... 02147] = { { EMIT_SUBI }, NULL, 0, SR_CCR, 2, 0, 2 },
	[02150 ... 02171] = { { EMIT_SUBI }, NULL, 0, SR_CCR, 2, 1, 2 },
	[02200 ... 02207] = { { EMIT_SUBI }, NULL, 0, SR_CCR, 3, 0, 4 },
	[02220 ... 02247] = { { EMIT_SUBI }, NULL, 0, SR_CCR, 3, 0, 4 },
	[02250 ... 02271] = { { EMIT_SUBI }, NULL, 0, SR_CCR, 3, 1, 4 },

	[03000 ... 03007] = { { EMIT_ADDI }, NULL, 0, SR_CCR, 2, 0, 1 },
	[03020 ... 03047] = { { EMIT_ADDI }, NULL, 0, SR_CCR, 2, 0, 1 },
	[03050 ... 03071] = { { EMIT_ADDI }, NULL, 0, SR_CCR, 2, 1, 1 },
	[03100 ... 03107] = { { EMIT_ADDI }, NULL, 0, SR_CCR, 2, 0, 2 },
	[03120 ... 03147] = { { EMIT_ADDI }, NULL, 0, SR_CCR, 2, 0, 2 },
	[03150 ... 03171] = { { EMIT_ADDI }, NULL, 0, SR_CCR, 2, 1, 2 },
	[03200 ... 03207] = { { EMIT_ADDI }, NULL, 0, SR_CCR, 3, 0, 4 },
	[03220 ... 03247] = { { EMIT_ADDI }, NULL, 0, SR_CCR, 3, 0, 4 },
	[03250 ... 03271] = { { EMIT_ADDI }, NULL, 0, SR_CCR, 3, 1, 4 },

	[04000 ... 04007] = { { EMIT_BTST }, NULL, 0, SR_Z, 2, 0, 4 },
	[04020 ... 04047] = { { EMIT_BTST }, NULL, 0, SR_Z, 2, 0, 1 },
	[04050 ... 04073] = { { EMIT_BTST }, NULL, 0, SR_Z, 2, 1, 1 },
	[04100 ... 04107] = { { EMIT_BCHG }, NULL, 0, SR_Z, 2, 0, 4 },
	[04120 ... 04147] = { { EMIT_BCHG }, NULL, 0, SR_Z, 2, 1, 1 },
	[04150 ... 04171] = { { EMIT_BCHG }, NULL, 0, SR_Z, 2, 1, 1 },
	[04200 ... 04207] = { { EMIT_BCLR }, NULL, 0, SR_Z, 2, 0, 4 },
	[04220 ... 04247] = { { EMIT_BCLR }, NULL, 0, SR_Z, 2, 0, 1 },
	[04250 ... 04271] = { { EMIT_BCLR }, NULL, 0, SR_Z, 2, 1, 1 },
	[04300 ... 04307] = { { EMIT_BSET }, NULL, 0, SR_Z, 2, 0, 4 },
	[04320 ... 04347] = { { EMIT_BSET }, NULL, 0, SR_Z, 2, 0, 1 },
	[04350 ... 04371] = { { EMIT_BSET }, NULL, 0, SR_Z, 2, 1, 1 },

	[05000 ... 05007] = { { EMIT_EORI }, NULL, 0, SR_NZVC, 2, 0, 1 },
	[05020 ... 05047] = { { EMIT_EORI }, NULL, 0, SR_NZVC, 2, 0, 1 },
	[05050 ... 05071] = { { EMIT_EORI }, NULL, 0, SR_NZVC, 2, 1, 1 },
	[05100 ... 05107] = { { EMIT_EORI }, NULL, 0, SR_NZVC, 2, 0, 2 },
	[05120 ... 05147] = { { EMIT_EORI }, NULL, 0, SR_NZVC, 2, 0, 2 },
	[05150 ... 05171] = { { EMIT_EORI }, NULL, 0, SR_NZVC, 2, 1, 2 },
	[05200 ... 05207] = { { EMIT_EORI }, NULL, 0, SR_NZVC, 3, 0, 4 },
	[05220 ... 05247] = { { EMIT_EORI }, NULL, 0, SR_NZVC, 3, 0, 4 },
	[05250 ... 05271] = { { EMIT_EORI }, NULL, 0, SR_NZVC, 3, 1, 4 },

	[06000 ... 06007] = { { EMIT_CMPI }, NULL, 0, SR_NZVC, 2, 0, 1 },
	[06020 ... 06047] = { { EMIT_CMPI }, NULL, 0, SR_NZVC, 2, 0, 1 },
	[06050 ... 06071] = { { EMIT_CMPI }, NULL, 0, SR_NZVC, 2, 1, 1 },
	[06100 ... 06107] = { { EMIT_CMPI }, NULL, 0, SR_NZVC, 2, 0, 2 },
	[06120 ... 06147] = { { EMIT_CMPI }, NULL, 0, SR_NZVC, 2, 0, 2 },
	[06150 ... 06171] = { { EMIT_CMPI }, NULL, 0, SR_NZVC, 2, 1, 2 },
	[06200 ... 06207] = { { EMIT_CMPI }, NULL, 0, SR_NZVC, 3, 0, 4 },
	[06220 ... 06247] = { { EMIT_CMPI }, NULL, 0, SR_NZVC, 3, 0, 4 },
	[06250 ... 06271] = { { EMIT_CMPI }, NULL, 0, SR_NZVC, 3, 1, 4 },

	[00400 ... 00407] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 0, 4 },
	[00420 ... 00447] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 0, 1 },
	[00450 ... 00474] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 1, 1 },
	[01400 ... 01407] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 0, 4 },
	[01420 ... 01447] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 0, 1 },
	[01450 ... 01474] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 1, 1 },
	[02400 ... 02407] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 0, 4 },
	[02420 ... 02447] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 0, 1 },
	[02450 ... 02474] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 1, 1 },
	[03400 ... 03407] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 0, 4 },
	[03420 ... 03447] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 0, 1 },
	[03450 ... 03474] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 1, 1 },
	[04400 ... 04407] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 0, 4 },
	[04420 ... 04447] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 0, 1 },
	[04450 ... 04474] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 1, 1 },
	[05400 ... 05407] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 0, 4 },
	[05420 ... 05447] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 1, 1 },
	[05450 ... 05474] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 1, 1 },
	[06400 ... 06407] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 0, 4 },
	[06420 ... 06447] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 0, 1 },
	[06450 ... 06474] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 1, 1 },
	[07400 ... 07407] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 0, 4 },
	[07420 ... 07447] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 0, 1 },
	[07450 ... 07474] = { { EMIT_BTST }, NULL, 0, SR_Z, 1, 1, 1 },

	[00500 ... 00507] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 0, 4 },
	[00520 ... 00547] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 0, 1 },
	[00550 ... 00571] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 1, 1 },
	[01500 ... 01507] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 0, 4 },
	[01520 ... 01547] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 0, 1 },
	[01550 ... 01571] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 1, 1 },
	[02500 ... 02507] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 0, 4 },
	[02520 ... 02547] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 0, 1 },
	[02550 ... 02571] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 1, 1 },
	[03500 ... 03507] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 0, 4 },
	[03520 ... 03547] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 0, 1 },
	[03550 ... 03571] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 1, 1 },
	[04500 ... 04507] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 0, 4 },
	[04520 ... 04547] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 0, 1 },
	[04550 ... 04571] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 1, 1 },
	[05500 ... 05507] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 0, 4 },
	[05520 ... 05547] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 0, 1 },
	[05550 ... 05571] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 1, 1 },
	[06500 ... 06507] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 0, 4 },
	[06520 ... 06547] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 0, 1 },
	[06550 ... 06571] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 1, 1 },
	[07500 ... 07507] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 0, 4 },
	[07520 ... 07547] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 0, 1 },
	[07550 ... 07571] = { { EMIT_BCHG }, NULL, 0, SR_Z, 1, 1, 1 },

	[00600 ... 00607] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 0, 4 },
	[00620 ... 00647] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 0, 1 },
	[00650 ... 00671] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 1, 1 },
	[01600 ... 01607] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 0, 4 },
	[01620 ... 01647] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 0, 1 },
	[01650 ... 01671] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 1, 1 },
	[02600 ... 02607] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 0, 4 },
	[02620 ... 02647] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 0, 1 },
	[02650 ... 02671] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 1, 1 },
	[03600 ... 03607] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 0, 4 },
	[03620 ... 03647] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 0, 1 },
	[03650 ... 03671] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 1, 1 },
	[04600 ... 04607] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 0, 4 },
	[04620 ... 04647] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 0, 1 },
	[04650 ... 04671] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 1, 1 },
	[05600 ... 05607] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 0, 4 },
	[05620 ... 05647] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 0, 1 },
	[05650 ... 05671] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 1, 1 },
	[06600 ... 06607] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 0, 4 },
	[06620 ... 06647] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 0, 1 },
	[06650 ... 06671] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 1, 1 },
	[07600 ... 07607] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 0, 4 },
	[07620 ... 07647] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 0, 1 },
	[07650 ... 07671] = { { EMIT_BCLR }, NULL, 0, SR_Z, 1, 1, 1 },

	[00700 ... 00707] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 0, 4 },
	[00720 ... 00747] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 0, 1 },
	[00750 ... 00771] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 1, 1 },
	[01700 ... 01707] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 0, 4 },
	[01720 ... 01747] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 0, 1 },
	[01750 ... 01771] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 1, 1 },
	[02700 ... 02707] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 0, 4 },
	[02720 ... 02747] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 0, 1 },
	[02750 ... 02771] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 1, 1 },
	[03700 ... 03707] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 0, 4 },
	[03720 ... 03747] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 0, 1 },
	[03750 ... 03771] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 1, 1 },
	[04700 ... 04707] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 0, 4 },
	[04720 ... 04747] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 0, 1 },
	[04750 ... 04771] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 1, 1 },
	[05700 ... 05707] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 0, 4 },
	[05720 ... 05747] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 0, 1 },
	[05750 ... 05771] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 1, 1 },
	[06700 ... 06707] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 0, 4 },
	[06720 ... 06747] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 0, 1 },
	[06750 ... 06771] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 1, 1 },
	[07700 ... 07707] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 0, 4 },
	[07720 ... 07747] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 0, 1 },
	[07750 ... 07771] = { { EMIT_BSET }, NULL, 0, SR_Z, 1, 1, 1 },

	[05320 ... 05347] = { { EMIT_CAS }, NULL, 0, SR_NZVC, 2, 0, 1 },
	[05350 ... 05371] = { { EMIT_CAS }, NULL, 0, SR_NZVC, 2, 1, 1 },
	[06320 ... 06347] = { { EMIT_CAS }, NULL, 0, SR_NZVC, 2, 0, 2 },
	[06350 ... 06371] = { { EMIT_CAS }, NULL, 0, SR_NZVC, 2, 1, 2 },
	[07320 ... 07347] = { { EMIT_CAS }, NULL, 0, SR_NZVC, 2, 0, 4 },
	[07350 ... 07371] = { { EMIT_CAS }, NULL, 0, SR_NZVC, 2, 1, 4 },

	[0xcfc] = { { EMIT_CAS2 }, NULL, 0, SR_NZVC, 3, 0, 2 },
	[0xefc] = { { EMIT_CAS2 }, NULL, 0, SR_NZVC, 3, 0, 4 },

	[00320 ... 00327] = { { EMIT_CMP2 }, NULL, 0, SR_NZVC, 2, 0, 1 },
	[00350 ... 00373] = { { EMIT_CMP2 }, NULL, 0, SR_NZVC, 2, 1, 1 },
	[01320 ... 01327] = { { EMIT_CMP2 }, NULL, 0, SR_NZVC, 2, 0, 2 },
	[01350 ... 01373] = { { EMIT_CMP2 }, NULL, 0, SR_NZVC, 2, 1, 2 },
	[02320 ... 02327] = { { EMIT_CMP2 }, NULL, 0, SR_NZVC, 2, 0, 4 },
	[02350 ... 02371] = { { EMIT_CMP2 }, NULL, 0, SR_NZVC, 2, 1, 4 },

	[0x108 ... 0x10f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 2 },
	[0x148 ... 0x14f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 4 },
	[0x188 ... 0x18f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 2 },
	[0x1c8 ... 0x1cf] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 4 },
	[0x308 ... 0x30f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 2 },
	[0x348 ... 0x34f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 4 },
	[0x388 ... 0x38f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 2 },
	[0x3c8 ... 0x3cf] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 4 },
	[0x508 ... 0x50f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 2 },
	[0x548 ... 0x54f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 4 },
	[0x588 ... 0x58f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 2 },
	[0x5c8 ... 0x5cf] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 4 },
	[0x708 ... 0x70f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 2 },
	[0x748 ... 0x74f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 4 },
	[0x788 ... 0x78f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 2 },
	[0x7c8 ... 0x7cf] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 4 },
	[0x908 ... 0x90f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 2 },
	[0x948 ... 0x94f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 4 },
	[0x988 ... 0x98f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 2 },
	[0x9c8 ... 0x9cf] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 4 },
	[0xb08 ... 0xb0f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 2 },
	[0xb48 ... 0xb4f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 4 },
	[0xb88 ... 0xb8f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 2 },
	[0xbc8 ... 0xbcf] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 4 },
	[0xd08 ... 0xd0f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 2 },
	[0xd48 ... 0xd4f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 4 },
	[0xd88 ... 0xd8f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 2 },
	[0xdc8 ... 0xdcf] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 4 },
	[0xf08 ... 0xf0f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 2 },
	[0xf48 ... 0xf4f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 4 },
	[0xf88 ... 0xf8f] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 2 },
	[0xfc8 ... 0xfcf] = { { EMIT_MOVEP }, NULL, 0, 0, 2, 0, 4 },

	[07020 ... 07047] = { { EMIT_MOVES }, NULL, SR_S, 0, 2, 0, 1 },
	[07050 ... 07071] = { { EMIT_MOVES }, NULL, SR_S, 0, 2, 1, 1 },
	[07120 ... 07147] = { { EMIT_MOVES }, NULL, SR_S, 0, 2, 0, 2 },
	[07150 ... 07171] = { { EMIT_MOVES }, NULL, SR_S, 0, 2, 1, 2 },
	[07220 ... 07247] = { { EMIT_MOVES }, NULL, SR_S, 0, 2, 0, 4 },
	[07250 ... 07271] = { { EMIT_MOVES }, NULL, SR_S, 0, 2, 1, 4 },
};

uint32_t *EMIT_line0(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{

    uint16_t opcode = BE16((*m68k_ptr)[0]);
    *insn_consumed = 1;
    (*m68k_ptr)++;

    if (InsnTable[opcode & 0xfff].od_Emit) {
        ptr = InsnTable[opcode & 0xfff].od_Emit(ptr, opcode, m68k_ptr);
    }
    else
    {
        ptr = EMIT_FlushPC(ptr);
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
    }

    return ptr;
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
    uint16_t opcode = BE16(*insn_stream);
    
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
