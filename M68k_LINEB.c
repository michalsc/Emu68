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

uint32_t *EMIT_lineB(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    /* 1011xxxx11xxxxxx - CMPA */
    if ((opcode & 0xf0c0) == 0xb0c0)
    {
        uint8_t size = ((opcode >> 8) & 1) ? 4 : 2;
        uint8_t src = 0xff;
        uint8_t dst = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
        uint8_t ext_words = 0;

        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

#ifdef __aarch64__
        if (size == 2)
            *ptr++ = sxth(src, src);

        *ptr++ = cmp_reg(dst, src, LSL, 0);
#else
        if (size == 2)
            *ptr++ = sxth(src, src, 0);

        *ptr++ = cmp_reg(dst, src);
#endif

        RA_FreeARMRegister(&ptr, src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        uint8_t mask = M68K_GetSRMask(*m68k_ptr);
        uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

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
    }
    /* 1011xxx1xx001xxx - CMPM */
    else if ((opcode & 0xf138) == 0xb108)
    {
        uint8_t size = 1 << ((opcode >> 6) & 3);
        uint8_t src = 0xff;
        uint8_t dst = 0xff;
        uint8_t ext_words = 0;
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, 0x18 | (opcode & 7), *m68k_ptr, &ext_words, 0, NULL);
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &dst, 0x18 | ((opcode >> 9) & 7), *m68k_ptr, &ext_words, 0, NULL);

        switch (size)
        {
#ifdef __aarch64__
        case 4:
            *ptr++ = subs_reg(tmp, dst, src, LSL, 0);
            break;
        case 2:
            *ptr++ = lsl(tmp, dst, 16);
            *ptr++ = subs_reg(tmp, tmp, src, LSL, 16);
            break;
        case 1:
            *ptr++ = lsl(tmp, dst, 24);
            *ptr++ = subs_reg(tmp, tmp, src, LSL, 24);
            break;
#else
        case 4:
            *ptr++ = rsbs_reg(tmp, src, dst, 0);
            break;
        case 2:
            *ptr++ = lsl_immed(tmp, src, 16);
            *ptr++ = rsbs_reg(tmp, tmp, dst, 16);
            break;
        case 1:
            *ptr++ = lsl_immed(tmp, src, 24);
            *ptr++ = rsbs_reg(tmp, tmp, dst, 24);
            break;
#endif
        }

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, src);
        RA_FreeARMRegister(&ptr, dst);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        uint8_t mask = M68K_GetSRMask(*m68k_ptr);
        uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

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
    }
    /* 1011xxx0xxxxxxxx - CMP */
    else if ((opcode & 0xf100) == 0xb000)
    {
        uint8_t size = 1 << ((opcode >> 6) & 3);
        uint8_t src = 0xff;
        uint8_t dst = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
        uint8_t ext_words = 0;
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

        switch(size)
        {
#ifdef __aarch64__
            case 4:
                *ptr++ = subs_reg(tmp, dst, src, LSL, 0);
                break;
            case 2:
                *ptr++ = lsl(tmp, dst, 16);
                *ptr++ = subs_reg(tmp, tmp, src, LSL, 16);
                break;
            case 1:
                *ptr++ = lsl(tmp, dst, 24);
                *ptr++ = subs_reg(tmp, tmp, src, LSL, 24);
                break;
#else
            case 4:
                *ptr++ = rsbs_reg(tmp, src, dst, 0);
                break;
            case 2:
                *ptr++ = lsl_immed(tmp, src, 16);
                *ptr++ = rsbs_reg(tmp, tmp, dst, 16);
                break;
            case 1:
                *ptr++ = lsl_immed(tmp, src, 24);
                *ptr++ = rsbs_reg(tmp, tmp, dst, 24);
                break;
#endif
        }

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, src);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        uint8_t mask = M68K_GetSRMask(*m68k_ptr);
        uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

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
    }
    /* 1011xxxxxxxxxxxx - EOR */
    else if ((opcode & 0xf000) == 0xb000)
    {
        uint8_t size = 1 << ((opcode >> 6) & 3);
        uint8_t ext_words = 0;
        uint8_t test_register;

        if ((opcode & 0x38) == 0)
        {
            uint8_t src = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            uint8_t dest = RA_MapM68kRegister(&ptr, (opcode) & 7);
            uint8_t tmp = 0xff;

            test_register = dest;

            RA_SetDirtyM68kRegister(&ptr, opcode & 7);

            switch (size)
            {
#ifdef __aarch64__
            case 4:
                *ptr++ = eor_reg(dest, dest, src, LSL, 0);
                break;
            case 2:
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_reg(tmp, src, dest, LSL, 0);
                *ptr++ = bfi(dest, tmp, 0, 16);
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 1:
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_reg(tmp, src, dest, LSL, 0);
                *ptr++ = bfi(dest, tmp, 0, 8);
                RA_FreeARMRegister(&ptr, tmp);
                break;
#else
            case 4:
                *ptr++ = eors_reg(dest, dest, src, 0);
                break;
            case 2:
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = lsl_immed(tmp, src, 16);
                *ptr++ = eors_reg(tmp, src, dest, 16);
                *ptr++ = lsr_immed(tmp, tmp, 16);
                *ptr++ = bfi(dest, tmp, 0, 16);
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 1:
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = lsl_immed(tmp, src, 24);
                *ptr++ = eors_reg(tmp, src, dest, 24);
                *ptr++ = lsr_immed(tmp, tmp, 24);
                *ptr++ = bfi(dest, tmp, 0, 8);
                RA_FreeARMRegister(&ptr, tmp);
                break;
#endif
            }

            RA_FreeARMRegister(&ptr, src);
        }
        else
        {
            uint8_t dest = 0xff;
            uint8_t src = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            uint8_t mode = (opcode & 0x0038) >> 3;

            test_register = tmp;

            if (mode == 4 || mode == 3)
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);
            else
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

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
                *ptr++ = eor_reg(tmp, tmp, src, LSL, 0);
#else
                *ptr++ = eors_reg(tmp, tmp, src, 0);
#endif
                /* Store back */
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
                /* Perform calcualtion */
#ifdef __aarch64__
                *ptr++ = eor_reg(tmp, tmp, src, LSL, 0);
#else
                *ptr++ = lsl_immed(tmp, tmp, 16);
                *ptr++ = eors_reg(tmp, tmp, src, 16);
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
                *ptr++ = eor_reg(tmp, tmp, src, LSL, 0);
#else
                *ptr++ = lsl_immed(tmp, tmp, 24);
                *ptr++ = eors_reg(tmp, tmp, src, 24);
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
            }

            RA_FreeARMRegister(&ptr, dest);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        uint8_t mask = M68K_GetSRMask(*m68k_ptr);
        uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
#ifdef __aarch64__
            switch(size)
            {
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
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
        }
        RA_FreeARMRegister(&ptr, test_register);
    }
    else
        *ptr++ = udf(opcode);

    return ptr;
}
