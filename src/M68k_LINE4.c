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

uint32_t *EMIT_MUL_DIV(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr);

extern uint32_t insn_count;

uint32_t *EMIT_CLR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t ext_count = 0;
    uint8_t size = 1;
    uint8_t zero = 0xff;
#ifdef __aarch64__
    zero = 31;
#else
    RA_AllocARMRegister(&ptr);
    *ptr++ = mov_immed_u8(zero, 0);
#endif

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
#ifdef __aarch64__
                    *ptr++ = bic_immed(dn, dn, 8, 0);
#else
                    *ptr++ = bic_immed(dn, dn, 0xff);
#endif
                    break;
                case 2:
#ifdef __aarch64__
                    *ptr++ = bic_immed(dn, dn, 16, 0);
#else
                    *ptr++ = bic_immed(dn, dn, 0xffff);
#endif
            }
        }
        else {
            uint8_t dn = RA_MapM68kRegisterForWrite(&ptr, opcode & 7);
#ifdef __aarch64__
            *ptr++ = mov_reg(dn, 31);
#else
            *ptr++ = sub_reg(dn, dn, dn, 0);
#endif
        }
    }
    else
        ptr = EMIT_StoreToEffectiveAddress(ptr, size, &zero, opcode & 0x3f, *m68k_ptr, &ext_count);
#ifndef __aarch64__
    RA_FreeARMRegister(&ptr, zero);
#endif
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));
    (*m68k_ptr) += ext_count;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        if (update_mask & ~SR_Z)
            ptr = EMIT_ClearFlags(ptr, cc, update_mask);
        if (update_mask & SR_Z)
            ptr = EMIT_SetFlags(ptr, cc, SR_Z);
    }
    return ptr;
}

uint32_t *EMIT_NOT(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
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
#ifdef __aarch64__
            *ptr++ = mvn_reg(dest, dest, LSL, 0);
#else
            *ptr++ = mvns_reg(dest, dest, 0);
#endif
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
#ifdef __aarch64__
                case 2:
                    *ptr++ = mvn_reg(tmp, dest, LSL, 0);
                    *ptr++ = bfi(dest, tmp, 0, 16);
                    break;
                case 1:
                    *ptr++ = mvn_reg(tmp, dest, LSL, 0);
                    *ptr++ = bfi(dest, tmp, 0, 8);
                    break;
#else
                case 2:
                    *ptr++ = sxth(tmp, dest, 0);        /* Extract lower 16 bits */
                    *ptr++ = mvns_reg(tmp, tmp, 0);     /* Negate */
                    *ptr++ = bfi(dest, tmp, 0, 16);     /* Insert result bitfield into register */
                    break;
                case 1:
                    *ptr++ = sxtb(tmp, dest, 0);
                    *ptr++ = mvns_reg(tmp, tmp, 0);
                    *ptr++ = bfi(dest, tmp, 0, 8);
                    break;
#endif
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
#ifdef __aarch64__
            *ptr++ = mvn_reg(tmp, tmp, LSL, 0);
#else
            *ptr++ = mvns_reg(tmp, tmp, 0);
#endif

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
#ifdef __aarch64__
            *ptr++ = mvn_reg(tmp, tmp, LSL, 0);
#else
            *ptr++ = sxth(tmp, tmp, 0);
            *ptr++ = mvns_reg(tmp, tmp, 0);
#endif
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

#ifdef __aarch64__
            *ptr++ = mvn_reg(tmp, tmp, LSL, 0);
#else
            *ptr++ = sxtb(tmp, tmp, 0);
            *ptr++ = mvns_reg(tmp, tmp, 0);
#endif
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

#ifdef __aarch64__
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
#endif

        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
    }

    RA_FreeARMRegister(&ptr, test_register);

    return ptr;
}

uint32_t *EMIT_NEG(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
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
#ifdef __aarch64__
            *ptr++ = negs_reg(dest, dest, LSL, 0);
#else
            *ptr++ = rsbs_immed(dest, dest, 0);
#endif
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
#ifdef __aarch64__
                    *ptr++ = negs_reg(tmp, dest, LSL, 16);
                    *ptr++ = bfxil(dest, tmp, 16, 16);
#else
                    *ptr++ = lsl_immed(tmp, dest, 16);
                    *ptr++ = rsbs_immed(tmp, tmp, 0);
                    *ptr++ = lsr_immed(tmp, tmp, 16);
                    *ptr++ = bfi(dest, tmp, 0, 16);
#endif
                    break;
                case 1:
#ifdef __aarch64__
                    *ptr++ = negs_reg(tmp, dest, LSL, 24);
                    *ptr++ = bfxil(dest, tmp, 24, 8);
#else
                    *ptr++ = lsl_immed(tmp, dest, 24);
                    *ptr++ = rsbs_immed(tmp, tmp, 0);
                    *ptr++ = lsr_immed(tmp, tmp, 24);
                    *ptr++ = bfi(dest, tmp, 0, 8);
#endif
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

#ifdef __aarch64__
            *ptr++ = negs_reg(tmp, tmp, LSL, 0);
#else
            *ptr++ = rsbs_immed(tmp, tmp, 0);
#endif

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

#ifdef __aarch64__
            *ptr++ = negs_reg(tmp, tmp, LSL, 16);
            *ptr++ = lsr(tmp, tmp, 16);
#else
            *ptr++ = sxth(tmp, tmp, 0);
            *ptr++ = rsbs_immed(tmp, tmp, 0);
#endif

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

#ifdef __aarch64__
            *ptr++ = negs_reg(tmp, tmp, LSL, 24);
            *ptr++ = lsr(tmp, tmp, 24);
#else
            *ptr++ = sxtb(tmp, tmp, 0);
            *ptr++ = rsbs_immed(tmp, tmp, 0);
#endif

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
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_X, ARM_CC_NE);
            else if ((update_mask & (SR_X | SR_C)) == SR_C)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C, ARM_CC_NE);
            else
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C | SR_X, ARM_CC_NE);
        }
    }

    return ptr;
}

uint32_t *EMIT_NEGX(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t ext_count = 0;
    uint8_t dest = 0xff;
    uint8_t size = 0;
    uint8_t zero = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
    uint8_t cc = RA_GetCC(&ptr);
    *ptr++ = tst_immed(cc, 1, (32 - SRB_X) & 31);
    *ptr++ = csetm(zero, A64_CC_NE);
#else
    M68K_GetCC(&ptr);
    *ptr++ = tst_immed(REG_SR, SR_X);
    *ptr++ = mov_cc_immed_u8(ARM_CC_EQ, zero, 0);
    *ptr++ = mvn_cc_immed_u8(ARM_CC_NE, zero, 0);
#endif


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
#ifdef __aarch64__
            *ptr++ = subs_reg(dest, zero, dest, LSL, 0);
#else
            *ptr++ = rsbs_reg(dest, dest, zero, 0);
#endif
        }
        else
        {
            /* Fetch m68k register for write */
            dest = RA_MapM68kRegister(&ptr, opcode & 7);

            /* Mark register dirty */
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);

            switch(size)
            {
#ifdef __aarch64__
                case 2:
                    *ptr++ = subs_reg(zero, zero, dest, LSL, 16);
                    *ptr++ = bfxil(dest, zero, 16, 16);
                    break;
                case 1:
                    *ptr++ = subs_reg(zero, zero, dest, LSL, 24);
                    *ptr++ = bfxil(dest, zero, 24, 8);
                    break;
#else
                case 2:
                    *ptr++ = subs_reg(zero, zero, dest, 16);
                    *ptr++ = lsr_immed(zero, zero, 16);
                    *ptr++ = bfi(dest, zero, 0, 16);
                    break;
                case 1:
                    *ptr++ = subs_reg(zero, zero, dest, 24);
                    *ptr++ = lsr_immed(zero, zero, 24);
                    *ptr++ = bfi(dest, zero, 0, 8);
                    break;
#endif
            }
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

#ifdef __aarch64__
            *ptr++ = subs_reg(tmp, zero, tmp, LSL, 0);
#else
            *ptr++ = subs_reg(tmp, zero, tmp, 0);
#endif
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
#ifdef __aarch64__
            *ptr++ = sxth(tmp, tmp);
            *ptr++ = subs_reg(tmp, zero, tmp, LSL, 0);
#else
            *ptr++ = sxth(tmp, tmp, 0);
            *ptr++ = rsbs_reg(tmp, tmp, zero, 0);
#endif
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
#ifdef __aarch64__
            *ptr++ = sxtb(tmp, tmp);
            *ptr++ = subs_reg(tmp, zero, tmp, LSL, 0);
#else
            *ptr++ = sxtb(tmp, tmp, 0);
            *ptr++ = rsbs_reg(tmp, tmp, zero, 0);
#endif
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

    RA_FreeARMRegister(&ptr, zero);
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

uint32_t *EMIT_TST(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
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
#ifdef __aarch64__
            case 4:
                *ptr++ = cmn_reg(31, dest, LSL, 0);
                break;
            case 2:
                *ptr++ = cmn_reg(31, dest, LSL, 16);
                break;
            case 1:
                *ptr++ = cmn_reg(31, dest, LSL, 24);
                break;
#else
            case 4:
                *ptr++ = cmp_immed(dest, 0); //rsbs_reg(immed, immed, dest, 0);
                break;
            case 2:
                *ptr++ = movs_reg_shift(immed, dest, 16); //rsbs_reg(immed, immed, dest, 16);
                break;
            case 1:
                *ptr++ = movs_reg_shift(immed, dest, 24); //rsbs_reg(immed, immed, dest, 24);
                break;
#endif
        }
    }
    else
    {
        /* Load effective address */
#ifdef __aarch64__
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);
#else
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &dest, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
#endif
        /* Fetch data into temporary register, perform add, store it back */
        switch (size)
        {
#ifdef __aarch64__
            case 4:
                *ptr++ = cmn_reg(31, dest, LSL, 0);
                break;
            case 2:
                *ptr++ = cmn_reg(31, dest, LSL, 16);
                break;
            case 1:
                *ptr++ = cmn_reg(31, dest, LSL, 24);
                break;
#else
            case 4:
                /* Perform calcualtion */
                *ptr++ = cmp_immed(dest, 0); //rsbs_reg(immed, immed, dest, 0);
                break;
            case 2:
                /* Perform calcualtion */
                *ptr++ = movs_reg_shift(immed, dest, 16); //rsbs_reg(immed, immed, dest, 16);
                break;
            case 1:
                /* Perform calcualtion */
                *ptr++ = movs_reg_shift(immed, dest, 24); //rsbs_reg(immed, immed, dest, 24);
                break;
#endif
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

        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
    }
    return ptr;
}

uint32_t *EMIT_TAS(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
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
#ifdef __aarch64__
        *ptr++ = orr_immed(dest, dest, 1, 24);
#else
        *ptr++ = orr_immed(dest, dest, 0x80);
#endif
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
#ifdef __aarch64__
        *ptr++ = ldxrb(dest, tmpresult);
        *ptr++ = orr_immed(tmpreg, tmpresult, 1, 24);
        *ptr++ = stxrb(dest, tmpreg, tmpstate);
        *ptr++ = cmp_reg(31, tmpstate, LSL, 0);
        *ptr++ = b_cc(A64_CC_NE, -4);
#else
        *ptr++ = ldrexb(dest, tmpresult);
        *ptr++ = orr_immed(tmpreg, tmpresult, 0x80);
        *ptr++ = strexb(dest, tmpreg, tmpstate);
        *ptr++ = teq_immed(tmpstate, 0);
        *ptr++ = b_cc(ARM_CC_NE, -6);
#endif
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
#ifdef __aarch64__
        *ptr++ = cmn_reg(31, tmpresult, LSL, 24);
#else
        *ptr++ = lsl_immed(tmpresult, tmpresult, 24);
        *ptr++ = teq_immed(tmpresult, 0);
#endif
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

uint32_t *EMIT_line4(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    /* 0100000011xxxxxx - MOVE from SR */
    if ((opcode & 0xffc0) == 0x40c0)
    {
#ifdef __aarch64__
        uint8_t cc = RA_ModifyCC(&ptr);
        uint8_t ext_words = 0;
        uint32_t *tmpptr;

        ptr = EMIT_FlushPC(ptr);

        /* Test if supervisor mode is active */
        *ptr++ = ands_immed(31, cc, 1, 32 - SRB_S);
        tmpptr = ptr;
        *ptr++ = b_cc(A64_CC_EQ, 23);

        ptr = EMIT_StoreToEffectiveAddress(ptr, 2, &cc, opcode & 0x3f, *m68k_ptr, &ext_words);

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
#else
        ptr = EMIT_InjectDebugString(ptr, "[JIT] MOVE from SR at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
#endif
    }
    /* 0100001011xxxxxx - MOVE from CCR */
    else if ((opcode &0xffc0) == 0x42c0)
    {
#ifdef __aarch64__
        uint8_t cc = RA_GetCC(&ptr);
        uint8_t ext_words = 0;

        if (opcode & 0x38)
        {
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            *ptr++ = mov_reg(tmp, cc);
            *ptr++ = bic_immed(tmp, tmp, 11, 27);
            ptr = EMIT_StoreToEffectiveAddress(ptr, 2, &tmp, opcode & 0x3f, *m68k_ptr, &ext_words);

            RA_FreeARMRegister(&ptr, tmp);
        }
        else
        {
            /* Fetch m68k register */
            uint8_t dest = RA_MapM68kRegister(&ptr, opcode & 7);

            /* Mark register dirty */
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);

            *ptr++ = bfi(dest, cc, 0, 5);
            *ptr++ = bic_immed(dest, dest, 11, 27);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;
#else
        kprintf("[LINE4] Not implemented MOVE from CCR @ %08x\n", *m68k_ptr - 1);
        *ptr++ = udf(opcode);
#endif
    }
    /* 01000000ssxxxxxx - NEGX */
    else if ((opcode & 0xff00) == 0x4000 && (opcode & 0xc0) != 0xc0)
    {
        ptr = EMIT_NEGX(ptr, opcode, m68k_ptr);
    }
    /* 01000010ssxxxxxx - CLR */
    else if ((opcode & 0xff00) == 0x4200 && (opcode & 0xc0) != 0xc0)
    {
        ptr = EMIT_CLR(ptr, opcode, m68k_ptr);
    }
    /* 0100010011xxxxxx - MOVE to CCR */
    else if ((opcode &0xffc0) == 0x44c0)
    {
#ifdef __aarch64__
        uint8_t ext_words = 0;
        uint8_t src = 0xff;
        uint8_t cc = RA_ModifyCC(&ptr);

        ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

        *ptr++ = bfi(cc, src, 0, 5);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));

        RA_FreeARMRegister(&ptr, src);
        
        (*m68k_ptr) += ext_words;
#else
        kprintf("[LINE4] Not implemented MOVE to CCR @ %08x\n", *m68k_ptr - 1);
        *ptr++ = udf(opcode);
#endif
    }
    /* 01000100ssxxxxxx - NEG */
    else if ((opcode &0xff00) == 0x4400 && (opcode & 0xc0) != 0xc0)
    {
        ptr = EMIT_NEG(ptr, opcode, m68k_ptr);
    }
    /* 0100011011xxxxxx - MOVE to SR */
    else if ((opcode &0xffc0) == 0x46c0)
    {
#ifdef __aarch64__
        uint8_t ext_words = 0;
        uint8_t src = 0xff;
        uint8_t cc = RA_ModifyCC(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t changed = RA_AllocARMRegister(&ptr);
        uint8_t ctx = RA_GetCTX(&ptr);
        uint8_t sp = RA_MapM68kRegister(&ptr, 15);
        uint32_t *tmpptr;

        RA_SetDirtyM68kRegister(&ptr, 15);

        ptr = EMIT_FlushPC(ptr);
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

        /* Test if supervisor mode is active */
        *ptr++ = ands_immed(31, cc, 1, 32 - SRB_S);

        *ptr++ = mov_reg(changed, cc);
        *ptr++ = mov_immed_u16(tmp, 0xf71f, 0);
        
        *ptr++ = b_cc(A64_CC_EQ, 29);
        *ptr++ = and_reg(cc, tmp, src, LSL, 0);
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

        *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_words + 1));

        *ptr++ = mvn_reg(changed, cc, LSL, 0);
        *ptr++ = ands_immed(31, changed, 3, 32 - SRB_IPL);
        *ptr++ = b_cc(A64_CC_EQ, 3);
        *ptr++ = msr_imm(3, 7, 7);
        *ptr++ = b(2);
        *ptr++ = msr_imm(3, 6, 7);

        tmpptr = ptr;
        *ptr++ = b_cc(A64_CC_AL, 0);

        /* No supervisor. Update USP, generate exception */
        ptr = EMIT_Exception(ptr, VECTOR_PRIVILEGE_VIOLATION, 0);
        
        *tmpptr = b_cc(A64_CC_AL, ptr - tmpptr);

        RA_FreeARMRegister(&ptr, src);
        RA_FreeARMRegister(&ptr, tmp);

        *ptr++ = (uint32_t)(uintptr_t)tmpptr;
        *ptr++ = 1;
        *ptr++ = 0;
        *ptr++ = INSN_TO_LE(0xfffffffe);
        *ptr++ = INSN_TO_LE(0xffffffff);

        (*m68k_ptr) += ext_words;
#else
        ptr = EMIT_InjectDebugString(ptr, "[JIT] MOVE to SR at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
#endif
    }
    /* 01000110ssxxxxxx - NOT */
    else if ((opcode &0xff00) == 0x4600 && (opcode & 0xc0) != 0xc0)
    {
        ptr = EMIT_NOT(ptr, opcode, m68k_ptr);
    }
    /* 0100100xxx000xxx - EXT, EXTB */
    else if ((opcode & 0xfeb8) == 0x4880)
    {
        uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
        uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
        uint8_t tmp = reg;
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);
        uint8_t mode = (opcode >> 6) & 7;

        switch (mode)
        {
            case 2: /* Byte to Word */
                tmp = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
                *ptr++ = sxtb(tmp, reg);
#else
                *ptr++ = sxtb(tmp, reg, 0);
#endif
                *ptr++ = bfi(reg, tmp, 0, 16);
                break;
            case 3: /* Word to Long */
#ifdef __aarch64__
                *ptr++ = sxth(reg, reg);
#else
                *ptr++ = sxth(reg, reg, 0);
#endif
                break;
            case 7: /* Byte to Long */
#ifdef __aarch64__
                *ptr++ = sxtb(reg, reg);
#else
                *ptr++ = sxtb(reg, reg, 0);
#endif
                break;
        }

        ptr = EMIT_AdvancePC(ptr, 2);

        if (update_mask)
        {
#ifdef __aarch64__
            *ptr++ = cmp_reg(tmp, 31, LSL, 0);
#else
            *ptr++ = cmp_immed(tmp, 0);
#endif
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
        }
        if (tmp != reg)
            RA_FreeARMRegister(&ptr, tmp);
    }
    /* 0100100000001xxx - LINK - 32 bit offset */
    else if ((opcode & 0xfff8) == 0x4808)
    {
        uint8_t sp;
        uint8_t displ;
        uint8_t reg;
        int8_t pc_off;
        displ = RA_AllocARMRegister(&ptr);
        pc_off = 2;
        ptr = EMIT_GetOffsetPC(ptr, &pc_off);
        *ptr++ = ldr_offset(REG_PC, displ, pc_off);
        sp = RA_MapM68kRegister(&ptr, 15);
        reg = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
        *ptr++ = str_offset_preindex(sp, reg, -4);  /* SP = SP - 4; An -> (SP) */
        *ptr++ = mov_reg(reg, sp);
        RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
#ifdef __aarch64__
        *ptr++ = add_reg(sp, sp, displ, LSL, 0);
#else
        *ptr++ = add_reg(sp, sp, displ, 0);
#endif
        RA_SetDirtyM68kRegister(&ptr, 15);

        (*m68k_ptr)+=2;

        ptr = EMIT_AdvancePC(ptr, 6);
        RA_FreeARMRegister(&ptr, displ);
    }
    /* 0100100000xxxxxx - NBCD */
    else if ((opcode & 0xffc0) == 0x4800 && (opcode & 0x08) != 0x08)
    {
        ptr = EMIT_InjectDebugString(ptr, "[JIT] NBCD at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
    }
    /* 0100100001000xxx - SWAP */
    else if ((opcode & 0xfff8) == 0x4840)
    {
        uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
        uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);
#ifdef __aarch64__
        *ptr++ = ror(reg, reg, 16);
#else
        *ptr++ = rors_immed(reg, reg, 16);
#endif
        ptr = EMIT_AdvancePC(ptr, 2);

        if (update_mask)
        {
#ifdef __aarch64__
            *ptr++ = cmn_reg(31, reg, LSL, 0);
#endif
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
        }
    }
    /* 0100100001001xxx - BKPT */
    else if ((opcode & 0xfff8) == 0x4848)
    {
        ptr = EMIT_FlushPC(ptr);
        ptr = EMIT_InjectDebugString(ptr, "[JIT] BKPT at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
    }
    /* 0100100001xxxxxx - PEA */
    else if ((opcode & 0xffc0) == 0x4840 && (opcode & 0x38) != 0x08)
    {
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
    }
    /* 0100101011111100 - ILLEGAL */
    else if (opcode == 0x4afc)
    {
        /* Illegal generates exception. Always */
        ptr = EMIT_FlushPC(ptr);
        ptr = EMIT_InjectDebugString(ptr, "[JIT] ILLEGAL opcode at %08x\n", *m68k_ptr - 1);
        ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
    }
    /* 0100101011xxxxxx - TAS */
    else if ((opcode & 0xffc0) == 0x4ac0)
    {
        ptr = EMIT_TAS(ptr, opcode, m68k_ptr);
    }
    /* 0100101011xxxxxx - TST */
    else if ((opcode & 0xff00) == 0x4a00 && (opcode & 0xc0) != 0xc0)
    {
        ptr = EMIT_TST(ptr, opcode, m68k_ptr);
    }
    /* 0100110000xxxxxx - MULU, MULS, DIVU, DIVUL, DIVS, DIVSL */
    else if ((opcode & 0xff80) == 0x4c00 || (opcode == 0x83c0))
    {
        ptr = EMIT_MUL_DIV(ptr, opcode, m68k_ptr);
    }
    /* 010011100100xxxx - TRAP */
    else if ((opcode & 0xfff0) == 0x4e40)
    {
        /* Correct implementation is missing */
        ptr = EMIT_InjectDebugString(ptr, "[JIT] TRAP #%02d at %08x\n", opcode & 15, (*m68k_ptr) - 1);

        ptr = EMIT_AdvancePC(ptr, 2);
        ptr = EMIT_FlushPC(ptr);

        ptr = EMIT_Exception(ptr, VECTOR_INT_TRAP(opcode & 15), 0);

        *ptr++ = INSN_TO_LE(0xffffffff);
    }
    /* 0100111001010xxx - LINK */
    else if ((opcode & 0xfff8) == 0x4e50)
    {
        uint8_t sp;
        uint8_t displ;
        uint8_t reg;
        int8_t pc_off;

        displ = RA_AllocARMRegister(&ptr);
        pc_off = 2;
        ptr = EMIT_GetOffsetPC(ptr, &pc_off);
        *ptr++ = ldrsh_offset(REG_PC, displ, pc_off);
        sp = RA_MapM68kRegister(&ptr, 15);
        reg = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
        *ptr++ = str_offset_preindex(sp, reg, -4);  /* SP = SP - 4; An -> (SP) */
        *ptr++ = mov_reg(reg, sp);
        RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
#ifdef __aarch64__
        *ptr++ = add_reg(sp, sp, displ, LSL, 0);
#else
        *ptr++ = add_reg(sp, sp, displ, 0);
#endif
        RA_SetDirtyM68kRegister(&ptr, 15);

        (*m68k_ptr)++;

        ptr = EMIT_AdvancePC(ptr, 4);
        RA_FreeARMRegister(&ptr, displ);
    }
    /* 0100111001011xxx - UNLK */
    else if ((opcode & 0xfff8) == 0x4e58)
    {
        uint8_t sp;
        uint8_t reg;

        sp = RA_MapM68kRegisterForWrite(&ptr, 15);
        reg = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));

        *ptr++ = mov_reg(sp, reg);
        *ptr++ = ldr_offset_postindex(sp, reg, 4);

        RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        RA_SetDirtyM68kRegister(&ptr, 15);

        ptr = EMIT_AdvancePC(ptr, 2);
    }
    /* 010011100110xxxx - MOVE USP */
    else if ((opcode & 0xfff0) == 0x4e60)
    {
#ifdef __aarch64__
        uint32_t *tmp;
        ptr = EMIT_FlushPC(ptr);
        uint8_t ctx = RA_GetCTX(&ptr);
        uint8_t cc = RA_ModifyCC(&ptr);
        uint8_t an = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));

        /* Test if supervisor mode is active */
        *ptr++ = ands_immed(31, cc, 1, 32 - SRB_S);
    
        /* Branch to exception if not in supervisor */
        *ptr++ = b_cc(A64_CC_EQ, 4);
        if (opcode & 8)
        {
            *ptr++ = ldr_offset(ctx, an, __builtin_offsetof(struct M68KState, USP));
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }
        else
        {
            *ptr++ = str_offset(ctx, an, __builtin_offsetof(struct M68KState, USP));
        }
        *ptr++ = add_immed(REG_PC, REG_PC, 2);
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
        ptr = EMIT_InjectDebugString(ptr, "[JIT] MOVE USP at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
#endif
    }
    /* 0100111001110000 - RESET */
    else if (opcode == 0x4e70)
    {
        ptr = EMIT_InjectDebugString(ptr, "[JIT] RESET opcode at %08x\n", *m68k_ptr - 1);
#ifdef __aarch64__
        uint32_t *tmp;
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

        *tmp = b_cc(A64_CC_AL, ptr - tmp);
        *ptr++ = (uint32_t)(uintptr_t)tmp;
        *ptr++ = 1;
        *ptr++ = 0;
        *ptr++ = INSN_TO_LE(0xfffffffe);
        *ptr++ = INSN_TO_LE(0xffffffff);
#else
        /* Allow only in supervisor!!! */
        ptr = EMIT_InjectPrintContext(ptr);
        ptr = EMIT_AdvancePC(ptr, 2);
        ptr = EMIT_FlushPC(ptr);
        *ptr++ = INSN_TO_LE(0xffffffff);
#endif
    }
    /* 0100111001110000 - NOP */
    else if (opcode == 0x4e71)
    {
        ptr = EMIT_AdvancePC(ptr, 2);
        ptr = EMIT_FlushPC(ptr);
    }
    /* 0100111001110010 - STOP */
    else if (opcode == 0x4e72)
    {
#ifdef __aarch64__
        uint32_t *tmpptr;
        uint16_t new_sr = BE16((*m68k_ptr)[0]) & 0xf71f;
        uint8_t changed = RA_AllocARMRegister(&ptr);
        ptr = EMIT_FlushPC(ptr);
        uint8_t cc = RA_ModifyCC(&ptr);
        uint8_t sp = RA_MapM68kRegister(&ptr, 15);
        uint8_t ctx = RA_GetCTX(&ptr);

        RA_SetDirtyM68kRegister(&ptr, 15);

        /* Test if supervisor mode is active */
        *ptr++ = ands_immed(31, cc, 1, 32 - SRB_S);
    
        /* Branch to exception if not in supervisor */
        tmpptr = ptr;
        *ptr++ = b_cc(A64_CC_EQ, 0);
        *ptr++ = mov_immed_u16(changed, new_sr, 0);
        *ptr++ = eor_reg(changed, changed, cc, LSL, 0);
        *ptr++ = eor_reg(cc, changed, cc, LSL, 0);

        /* Perform eventual stack switch */
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

        /* Now do what stop does - wait for interrupt */
        *ptr++ = add_immed(REG_PC, REG_PC, 4);

        *ptr++ = mvn_reg(changed, cc, LSL, 0);
        *ptr++ = ands_immed(31, changed, 3, 32 - SRB_IPL);
        *ptr++ = b_cc(A64_CC_EQ, 3);
        *ptr++ = msr_imm(3, 7, 7);
        *ptr++ = b(2);
        *ptr++ = msr_imm(3, 6, 7);

        *ptr++ = wfi();

        *tmpptr = b_cc(A64_CC_EQ, 1 + ptr - tmpptr);
        tmpptr = ptr;
        *ptr++ = b_cc(A64_CC_AL, 10);

        /* No supervisor. Update USP, generate exception */
        ptr = EMIT_Exception(ptr, VECTOR_PRIVILEGE_VIOLATION, 0);

        *tmpptr = b_cc(A64_CC_AL, ptr - tmpptr);
        *ptr++ = (uint32_t)(uintptr_t)tmpptr;
        *ptr++ = 1;
        *ptr++ = 0;
        *ptr++ = INSN_TO_LE(0xfffffffe);
        *ptr++ = INSN_TO_LE(0xffffffff);

        RA_FreeARMRegister(&ptr, changed);
#else
        ptr = EMIT_InjectDebugString(ptr, "[JIT] STOP at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
#endif   
        (*m68k_ptr) += 1;
    }
    /* 0100111001110011 - RTE */
    else if (opcode == 0x4e73)
    {
#ifdef __aarch64__
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t sp = RA_MapM68kRegister(&ptr, 15);
        uint8_t cc = RA_ModifyCC(&ptr);
        uint8_t changed = RA_AllocARMRegister(&ptr);
        uint8_t ctx = RA_GetCTX(&ptr);
        uint32_t *tmpptr;
        RA_SetDirtyM68kRegister(&ptr, 15);

        /* Test if supervisor mode is active */
        *ptr++ = ands_immed(31, cc, 1, 32 - SRB_S);
        tmpptr = ptr;
        *ptr++ = b_cc(A64_CC_EQ, 0);

        /* Fetch sr from stack */
        ptr = EMIT_ResetOffsetPC(ptr);
        *ptr++ = ldrh_offset_postindex(sp, changed, 2);
        /* Fetch PC from stack */
        *ptr++ = ldr_offset_postindex(sp, REG_PC, 4);
        /* Fetch format word from stack */
        *ptr++ = ldrh_offset_postindex(sp, tmp, 2);
        *ptr++ = lsr(tmp, tmp, 13);
        *ptr++ = cmp_immed(tmp, 1);
        *ptr++ = b_cc(A64_CC_NE, 2);
        *ptr++ = add_immed(sp, sp, 4);
        
        /* Use two EORs to generate changed mask and update SR */
        *ptr++ = eor_reg(changed, changed, cc, LSL, 0);
        *ptr++ = eor_reg(cc, changed, cc, LSL, 0);       

        /* Now since stack is cleaned up, perform eventual stack switch */
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

        *ptr++ = mvn_reg(changed, cc, LSL, 0);
        *ptr++ = ands_immed(31, changed, 3, 32 - SRB_IPL);
        *ptr++ = b_cc(A64_CC_EQ, 3);
        *ptr++ = msr_imm(3, 7, 7);
        *ptr++ = b(2);
        *ptr++ = msr_imm(3, 6, 7);

        *tmpptr = b_cc(A64_CC_EQ, 1 + ptr - tmpptr);
        tmpptr = ptr;
        *ptr++ = b_cc(A64_CC_AL, 0);

        /* No supervisor. Update USP, generate exception */
        ptr = EMIT_Exception(ptr, VECTOR_PRIVILEGE_VIOLATION, 0);
        
        *tmpptr = b_cc(A64_CC_AL, ptr - tmpptr);
        *ptr++ = (uint32_t)(uintptr_t)tmpptr;
        *ptr++ = 1;
        *ptr++ = 0;
        *ptr++ = INSN_TO_LE(0xfffffffe);
        *ptr++ = INSN_TO_LE(0xffffffff);

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, changed);
#else
        ptr = EMIT_InjectDebugString(ptr, "[JIT] RTE at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
#endif
    }
    /* 0100111001110100 - RTD */
    else if (opcode == 0x4e74)
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t tmp2 = RA_AllocARMRegister(&ptr);
        uint8_t sp = RA_MapM68kRegister(&ptr, 15);
        int8_t pc_off;

        /* Fetch return address from stack */
        *ptr++ = ldr_offset_postindex(sp, tmp2, 4);
        pc_off = 2;
        ptr = EMIT_GetOffsetPC(ptr, &pc_off);
        *ptr++ = ldrsh_offset(REG_PC, tmp, pc_off);
#ifdef __aarch64__
        *ptr++ = add_reg(sp, sp, tmp, LSL, 0);
#else
        *ptr++ = add_reg(sp, sp, tmp, 0);
#endif
        ptr = EMIT_ResetOffsetPC(ptr);
        *ptr++ = mov_reg(REG_PC, tmp2);
        RA_SetDirtyM68kRegister(&ptr, 15);
        *ptr++ = INSN_TO_LE(0xffffffff);
        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, tmp2);
    }
    /* 0100111001110101 - RTS */
    else if (opcode == 0x4e75)
    {
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
            *m68k_ptr = ret_addr;
        }
        else
            *ptr++ = INSN_TO_LE(0xffffffff);
    }
    /* 0100111001110110 - TRAPV */
    else if (opcode == 0x4e76)
    {
        uint8_t cc = RA_GetCC(&ptr);
        uint32_t *tmpptr;
        ptr = EMIT_AdvancePC(ptr, 2);
        ptr = EMIT_FlushPC(ptr);

        *ptr++ = ands_immed(31, cc, 1, 32 - SRB_V);
        tmpptr = ptr;
        *ptr++ = b_cc(A64_CC_EQ, 0);
        
        ptr = EMIT_Exception(ptr, VECTOR_TRAPcc, 2, (uint32_t)(uintptr_t)(*m68k_ptr - 1));

        *tmpptr = b_cc(A64_CC_EQ, ptr - tmpptr);
        *ptr++ = (uint32_t)(uintptr_t)tmpptr;
        *ptr++ = 1;
        *ptr++ = 0;
        *ptr++ = INSN_TO_LE(0xfffffffe);
    }
    /* 0100111001110111 - RTR */
    else if (opcode == 0x4e77)
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mask = RA_AllocARMRegister(&ptr);
        uint8_t sp = RA_MapM68kRegister(&ptr, 15);

        /* Fetch status byte from stack */
        *ptr++ = ldrh_offset_postindex(sp, tmp, 2);
#ifdef __aarch64__
        uint8_t cc = RA_ModifyCC(&ptr);
        *ptr++ = bfi(cc, tmp, 0, 5);
#else
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, 0x1f);
        *ptr++ = and_immed(tmp, tmp, 0x1f);
        *ptr++ = orr_reg(REG_SR, REG_SR, tmp, 0);
#endif
        /* Fetch return address from stack */
        *ptr++ = ldr_offset_postindex(sp, REG_PC, 4);
        ptr = EMIT_ResetOffsetPC(ptr);
        RA_SetDirtyM68kRegister(&ptr, 15);
        *ptr++ = INSN_TO_LE(0xffffffff);
        RA_FreeARMRegister(&ptr, mask);
        RA_FreeARMRegister(&ptr, tmp);
    }
    /* 010011100111101x - MOVEC */
    else if ((opcode & 0xfffe) == 0x4e7a)
    {
#ifdef __aarch64__
        uint16_t opcode2 = BE16((*m68k_ptr)[0]);
        uint8_t dr = opcode & 1;
        uint8_t reg = RA_MapM68kRegister(&ptr, opcode2 >> 12);
        uint8_t ctx = RA_GetCTX(&ptr);
        uint8_t cc = RA_ModifyCC(&ptr);
        uint8_t tmp = 0xff;
        uint8_t sp = 0xff;
        uint32_t *tmpptr;

        (*m68k_ptr) += 1;
        ptr = EMIT_FlushPC(ptr);

        /* Test if supervisor mode is active */
        *ptr++ = ands_immed(31, cc, 1, 32 - SRB_S);
    
        /* Branch to exception if not in supervisor */
        tmpptr = ptr;
        *ptr++ = b_cc(A64_CC_EQ, 4);

        if (dr)
        {
            switch (opcode2 & 0xfff)
            {
                case 0x800:
                    *ptr++ = str_offset(ctx, reg, __builtin_offsetof(struct M68KState, USP));
                    break;
                case 0x801:
                    *ptr++ = str_offset(ctx, reg, __builtin_offsetof(struct M68KState, VBR));
                    break;
                case 0x002:
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = bic_immed(tmp, reg, 15, 0);
                    *ptr++ = bic_immed(tmp, tmp, 15, 16);
                    *ptr++ = str_offset(ctx, tmp, __builtin_offsetof(struct M68KState, CACR));
                    RA_FreeARMRegister(&ptr, tmp);
                    break;
                case 0x803:
                    sp = RA_MapM68kRegister(&ptr, 15);
                    RA_SetDirtyM68kRegister(&ptr, 15);
                    *ptr++ = tst_immed(cc, 1, 32 - SRB_M);
                    *ptr++ = b_cc(A64_CC_EQ, 2);
                    *ptr++ = mov_reg(sp, reg);
                    *ptr++ = str_offset(ctx, reg, __builtin_offsetof(struct M68KState, MSP));
                    break;
                case 0x804:
                    sp = RA_MapM68kRegister(&ptr, 15);
                    RA_SetDirtyM68kRegister(&ptr, 15);
                    *ptr++ = tst_immed(cc, 1, 32 - SRB_M);
                    *ptr++ = b_cc(A64_CC_NE, 2);
                    *ptr++ = mov_reg(sp, reg);
                    *ptr++ = str_offset(ctx, reg, __builtin_offsetof(struct M68KState, ISP));                    
                    break;
            }
        }
        else
        {
            switch (opcode2 & 0xfff)
            {
                case 0x800:
                    *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, USP));
                    break;
                case 0x801:
                    *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, VBR));
                    break;
                case 0x002:
                    *ptr++ = ldr_offset(ctx, reg, __builtin_offsetof(struct M68KState, CACR));
                    break;
                case 0x803:
                    sp = RA_MapM68kRegister(&ptr, 15);
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = tst_immed(cc, 1, 32 - SRB_M);
                    *ptr++ = ldr_offset(ctx, tmp, __builtin_offsetof(struct M68KState, MSP));
                    *ptr++ = csel(reg, sp, tmp, A64_CC_NE);
                    RA_FreeARMRegister(&ptr, tmp);
                    break;
                case 0x804:
                    sp = RA_MapM68kRegister(&ptr, 15);
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = tst_immed(cc, 1, 32 - SRB_M);
                    *ptr++ = ldr_offset(ctx, tmp, __builtin_offsetof(struct M68KState, ISP));
                    *ptr++ = csel(reg, sp, tmp, A64_CC_EQ);
                    RA_FreeARMRegister(&ptr, tmp);
                    break;
                case 0xc00:
                    *ptr++ = mrs(reg, 3, 3, 14, 0, 0);
                    break;
                case 0xc01:
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = mrs(tmp, 3, 3, 14, 0, 1);
                    *ptr++ = mov_reg(reg, tmp);
                    RA_FreeARMRegister(&ptr, tmp);
                    break;
                case 0xc02:
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = mrs(tmp, 3, 3, 14, 0, 1);
                    *ptr++ = lsr64(reg, tmp, 32);
                    RA_FreeARMRegister(&ptr, tmp);
                    break;
                case 0xc03:
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = ldr64_offset(ctx, tmp, __builtin_offsetof(struct M68KState, INSN_COUNT));
                    *ptr++ = add64_immed(tmp, tmp, insn_count & 0xfff);
                    if (insn_count & 0xfff000)
                        *ptr++ = add64_immed_lsl12(tmp, tmp, insn_count >> 12);
                    *ptr++ = mov_reg(reg, tmp);
                    RA_FreeARMRegister(&ptr, tmp);
                    break;
                case 0xc04:
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = ldr64_offset(ctx, tmp, __builtin_offsetof(struct M68KState, INSN_COUNT));
                    *ptr++ = add64_immed(tmp, tmp, insn_count & 0xfff);
                    if (insn_count & 0xfff000)
                        *ptr++ = add64_immed_lsl12(tmp, tmp, insn_count >> 12);
                    *ptr++ = lsr64(reg, tmp, 32);
                    RA_FreeARMRegister(&ptr, tmp);
                    break;
                case 0xc05:
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = mrs(tmp, 3, 3, 9, 13, 0);
                    *ptr++ = mov_reg(reg, tmp);
                    RA_FreeARMRegister(&ptr, tmp);
                    break;
                case 0xc06:
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = mrs(tmp, 3, 3, 9, 13, 0);
                    *ptr++ = lsr64(reg, tmp, 32);
                    RA_FreeARMRegister(&ptr, tmp);
                    break;
            }
            RA_SetDirtyM68kRegister(&ptr, opcode2 >> 12);
        }

        *ptr++ = add_immed(REG_PC, REG_PC, 4);
        *tmpptr = b_cc(A64_CC_EQ, 1 + ptr - tmpptr);
        tmpptr = ptr;
        *ptr++ = b_cc(A64_CC_AL, 0);

        /* No supervisor. Update USP, generate exception */
        ptr = EMIT_Exception(ptr, VECTOR_PRIVILEGE_VIOLATION, 0);

        *tmpptr = b_cc(A64_CC_AL, ptr - tmpptr);
        *ptr++ = (uint32_t)(uintptr_t)tmpptr;
        *ptr++ = 1;
        *ptr++ = 0;
        *ptr++ = INSN_TO_LE(0xfffffffe);
        *ptr++ = INSN_TO_LE(0xffffffff);
#else
        ptr = EMIT_InjectDebugString(ptr, "[JIT] MOVEC at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
#endif
    }
    /* 0100111010xxxxxx - JSR */
    else if ((opcode & 0xffc0) == 0x4e80)
    {
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
    }
    /* 0100111011xxxxxx - JMP */
    else if ((opcode & 0xffc0) == 0x4ec0)
    {
        uint8_t ext_words = 0;
        uint8_t ea = REG_PC;

        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &ea, opcode & 0x3f, (*m68k_ptr), &ext_words, 0, NULL);
        ptr = EMIT_ResetOffsetPC(ptr);
        (*m68k_ptr) += ext_words;
        RA_FreeARMRegister(&ptr, ea);
        *ptr++ = INSN_TO_LE(0xffffffff);
    }
    /* 01001x001xxxxxxx - MOVEM */
    else if ((opcode & 0xfb80) == 0x4880)
    {
        uint8_t dir = (opcode >> 10) & 1;
        uint8_t size = (opcode >> 6) & 1;
        uint16_t mask = BE16((*m68k_ptr)[0]);
        uint8_t block_size = 0;
        uint8_t ext_words = 0;

        (*m68k_ptr)++;

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

                RA_SetDirtyM68kRegister(&ptr, (opcode & 7) + 8);

                /* In pre-decrement the register order is reversed */
#ifdef __aarch64__
                uint8_t rt1 = 0xff;

                for (int i=0; i < 16; i++)
                {
                    if (mask & (0x8000 >> i))
                    {
                        uint8_t reg = RA_MapM68kRegister(&ptr, i);
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
#else
                *ptr++ = sub_immed(base, base, block_size);

                for (int i=0; i < 16; i++)
                {
                    /* Keep base register high in LRU */
                    RA_MapM68kRegister(&ptr, (opcode & 7) + 8);
                    if (mask & (0x8000 >> i))
                    {
                        uint8_t reg = RA_MapM68kRegister(&ptr, i);
                        if (size) {
                            *ptr++ = str_offset(base, reg, offset);
                            offset += 4;
                        }
                        else
                        {
                            *ptr++ = strh_offset(base, reg, offset);
                            offset += 2;
                        }
                    }
                }
#endif
            }
            else
            {
                uint8_t offset = 0;

#ifdef __aarch64__
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
#else
                for (int i=0; i < 16; i++)
                {
                    if (mask & (1 << i))
                    {
                        uint8_t reg = RA_MapM68kRegister(&ptr, i);
                        if (size) {
                            *ptr++ = str_offset(base, reg, offset);
                            offset += 4;
                        }
                        else
                        {
                            *ptr++ = strh_offset(base, reg, offset);
                            offset += 2;
                        }
                    }
                }
#endif
            }

            RA_FreeARMRegister(&ptr, base);
        }
        else
        {
            uint8_t base = 0xff;
            uint8_t offset = 0;

            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &base, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

#ifdef __aarch64__
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
#else
            for (int i=0; i < 16; i++)
            {
                if (mask & (1 << i))
                {
                    /* Keep base register high in LRU */
                    if (((opcode & 0x38) == 0x18)) RA_MapM68kRegister(&ptr, (opcode & 7) + 8);

                    uint8_t reg = RA_MapM68kRegisterForWrite(&ptr, i);
                    if (size) {
                        if ((((opcode & 0x38) == 0x18) && (i == (opcode & 7) + 8))) {
                            offset += 4;
                            continue;
                        }

                        *ptr++ = ldr_offset(base, reg, offset);
                        offset += 4;
                    }
                    else
                    {
                        if (!(((opcode & 0x38) == 0x18) && (i == (opcode & 7) + 8)))
                            *ptr++ = ldrsh_offset(base, reg, offset);
                        offset += 2;
                    }
                }
            }
#endif
            /* Post-increment mode? Increase the base now */
            if ((opcode & 0x38) == 0x18)
            {
                *ptr++ = add_immed(base, base, block_size);
                RA_SetDirtyM68kRegister(&ptr, (opcode & 7) + 8);
            }

            RA_FreeARMRegister(&ptr, base);
        }

        ptr = EMIT_AdvancePC(ptr, 2*(ext_words + 2));
        (*m68k_ptr) += ext_words;
    }
    /* 0100xxx111xxxxxx - LEA */
    else if ((opcode & 0xf1c0) == 0x41c0)
    {
        uint8_t dest = 0xff;
        uint8_t ea = 0xff;
        uint8_t ext_words = 0;
#ifdef __aarch64__
        /* On AArch64 we can always map destination reg for write - it is always there! */
        dest = RA_MapM68kRegisterForWrite(&ptr, 8 + ((opcode >> 9) & 7));
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, (*m68k_ptr), &ext_words, 1, NULL);
#else
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &ea, opcode & 0x3f, (*m68k_ptr), &ext_words, 1, NULL);
        dest = RA_MapM68kRegisterForWrite(&ptr, 8 + ((opcode >> 9) & 7));
        *ptr++ = mov_reg(dest, ea);
#endif
        (*m68k_ptr) += ext_words;

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        RA_FreeARMRegister(&ptr, ea);
    }
    /* 0100xxx1x0xxxxxx - CHK */
    else if ((opcode & 0xf140) == 0x4100)
    {
        ptr = EMIT_InjectDebugString(ptr, "[JIT] CHK at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
    }
    else
    {
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
    }

    return ptr;
}
