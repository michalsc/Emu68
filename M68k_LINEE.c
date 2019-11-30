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

uint32_t *EMIT_lineE(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    /* 1110000x11xxxxxx - ASL, ASR - memory */
    if ((opcode & 0xfec0) == 0xe0c0)
    {
        uint8_t direction = (opcode >> 8) & 1;
        uint8_t dest = 0xff;
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t ext_words = 0;
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 1);

        *ptr++ = ldrsh_offset(dest, tmp, 0);

        if (direction)
        {
            *ptr++ = lsls_immed(tmp, tmp, 17);
            *ptr++ = lsr_immed(tmp, tmp, 16);
        }
        else
        {
            *ptr++ = asrs_immed(tmp, tmp, 1);
        }

        *ptr++ = strh_offset(dest, tmp, 0);

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, dest);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));

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
            if (update_mask & (SR_X | SR_C))
                *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_X | SR_C);
        }
    }
    /* 1110001x11xxxxxx - LSL, LSR - memory */
    else if ((opcode & 0xfec0) == 0xe2c0)
    {
        uint8_t direction = (opcode >> 8) & 1;
        uint8_t dest = 0xff;
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t ext_words = 0;
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 1);

        *ptr++ = ldrh_offset(dest, tmp, 0);

        if (direction)
        {
            *ptr++ = lsls_immed(tmp, tmp, 17);
            *ptr++ = lsr_immed(tmp, tmp, 16);
        }
        else
        {
            *ptr++ = lsrs_immed(tmp, tmp, 1);
        }

        *ptr++ = strh_offset(dest, tmp, 0);

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, dest);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));

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
            if (update_mask & (SR_X | SR_C))
                *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_X | SR_C);
        }
    }
    /* 1110010x11xxxxxx - ROXL, ROXR - memory */
    else if ((opcode & 0xfec0) == 0xe4c0)
    {
        printf("Not implemented: ROXL, ROXR\n");
        *ptr++ = udf(opcode);
    }
    /* 1110011x11xxxxxx - ROL, ROR - memory */
    else if ((opcode & 0xfec0) == 0xe6c0)
    {
        uint8_t direction = (opcode >> 8) & 1;
        uint8_t dest = 0xff;
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t ext_words = 0;
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 1);

        *ptr++ = ldrh_offset(dest, tmp, 0);
        *ptr++ = bfi(tmp, tmp, 16, 16);

        if (direction)
        {
            *ptr++ = rors_immed(tmp, tmp, 32 - 1);
        }
        else
        {
            *ptr++ = rors_immed(tmp, tmp, 1);
        }

        *ptr++ = strh_offset(dest, tmp, 0);

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, dest);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));

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
            if (update_mask & (SR_X | SR_C))
                *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_C);
        }
    }
    /* 1110100011xxxxxx - BFTST */
    else if ((opcode & 0xffc0) == 0xe8c0)
    {
        uint8_t ext_words = 1;
        uint16_t opcode2 = BE16((*m68k_ptr)[0]);
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        /* Special case: Source is Dn */
        if ((opcode & 0x0038) == 0)
        {
            uint8_t src = RA_MapM68kRegister(&ptr, opcode & 7);
            *ptr++ = mov_reg(tmp, src);
            RA_FreeARMRegister(&ptr, src);

            /* Direct offset and width */
            if ((opcode2 & 0x0820) == 0)
            {
                uint8_t offset = (opcode2 >> 6) & 0x1f;
                uint8_t width = (opcode2) & 0x1f;

                /*
                    If offset == 0 and width == 0 the register value from Dn is already extracted bitfield,
                    otherwise extract bitfield
                */
                if (offset != 0 || width != 0)
                {
                    /* width == width - 1 */
                    width = (width == 0) ? 31 : width-1;
                    offset = 31 - (offset + width);
                    *ptr++ = sbfx(tmp, tmp, offset, width+1);
                }
            }
            /* Direct offset, width in reg */
            else if ((opcode2 & 0x0820) == 0x0020)
            {
                uint8_t offset = (opcode2 >> 6) & 0x1f;
                uint8_t width_reg_t = RA_MapM68kRegister(&ptr, opcode2 & 7);
                uint8_t width_reg = RA_AllocARMRegister(&ptr);

                *ptr++ = rsb_immed(width_reg, width_reg_t, 32);
                *ptr++ = and_immed(width_reg, width_reg, 31);

                RA_FreeARMRegister(&ptr, width_reg_t);

                if (offset > 0)
                {
                    *ptr++ = lsl_immed(tmp, tmp, offset);
                }
                *ptr++ = asr_reg(tmp, tmp, width_reg);

                RA_FreeARMRegister(&ptr, width_reg);
            }
            /* Offset in reg, direct width */
            else if ((opcode2 & 0x0820) == 0x0800)
            {
                uint8_t width = (opcode2) & 0x1f;
                uint8_t offset_reg = RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);

                width = 32 - width;

                *ptr++ = lsr_reg(tmp, tmp, offset_reg);
                *ptr++ = sbfx(tmp, tmp, 0, width);

                RA_FreeARMRegister(&ptr, offset_reg);
            }
            /* Both offset and width in regs */
            else
            {
                uint8_t width_reg_t = RA_MapM68kRegister(&ptr, opcode2 & 7);
                uint8_t width_reg = RA_AllocARMRegister(&ptr);
                uint8_t offset_reg = RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);

                *ptr++ = rsb_immed(width_reg, width_reg_t, 32);
                *ptr++ = and_immed(width_reg, width_reg, 31);
                RA_FreeARMRegister(&ptr, width_reg_t);

                *ptr++ = lsl_reg(tmp, tmp, offset_reg);
                *ptr++ = asr_reg(tmp, tmp, width_reg);

                RA_FreeARMRegister(&ptr, width_reg);
                RA_FreeARMRegister(&ptr, offset_reg);
            }
        }
        else
        {
            uint8_t dest;
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0);

            RA_FreeARMRegister(&ptr, dest);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        /* At this point extracted bitfield is in tmp register, compare it against 0, set zero and  */
        uint8_t mask = M68K_GetSRMask(*m68k_ptr);
        uint8_t update_mask = (SR_Z | SR_N | SR_C | SR_V) & ~mask;
        if (update_mask)
        {
            M68K_ModifyCC(&ptr);
            *ptr++ = cmp_immed(tmp, 0);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        }

        RA_FreeARMRegister(&ptr, tmp);
    }
    /* 1110100111xxxxxx - BFEXTU */
    else if ((opcode & 0xffc0) == 0xe9c0)
    {
        uint8_t ext_words = 1;
        uint16_t opcode2 = BE16((*m68k_ptr)[0]);
        uint8_t tmp = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);

        /* Special case: Source is Dn */
        if ((opcode & 0x0038) == 0)
        {
            /* Direct offset and width */
            if ((opcode2 & 0x0820) == 0)
            {
                uint8_t src = RA_MapM68kRegister(&ptr, opcode & 7);
                uint8_t offset = (opcode2 >> 6) & 0x1f;
                uint8_t width = (opcode2) & 0x1f;

                *ptr++ = mov_reg(tmp, src);

                RA_FreeARMRegister(&ptr, src);
                /*
                    If offset == 0 and width == 0 the register value from Dn is already extracted bitfield,
                    otherwise extract bitfield
                */
                if (offset != 0 || width != 0)
                {
                    /* width == width - 1 */
                    width = (width == 0) ? 31 : width-1;
                    offset = 31 - (offset + width);
                    *ptr++ = ubfx(tmp, tmp, offset, width+1);
                }
            }
        }
        else
        {
            uint8_t dest;
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0);

            RA_FreeARMRegister(&ptr, dest);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        /* At this point extracted bitfield is in tmp register, compare it against 0, set zero and  */
        uint8_t mask = M68K_GetSRMask(*m68k_ptr);
        uint8_t update_mask = (SR_Z | SR_N | SR_C | SR_V) & ~mask;
        if (update_mask)
        {
            M68K_ModifyCC(&ptr);
            *ptr++ = cmp_immed(tmp, 0);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        }

        RA_FreeARMRegister(&ptr, tmp);
    }
    /* 1110101011xxxxxx - BFCHG */
    else if ((opcode & 0xffc0) == 0xeac0)
    {
        *ptr++ = udf(opcode);
    }
    /* 1110101111xxxxxx - BFEXTS */
    else if ((opcode & 0xffc0) == 0xebc0)
    {
        uint8_t ext_words = 1;
        uint16_t opcode2 = BE16((*m68k_ptr)[0]);
        uint8_t tmp = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);

        /* Special case: Source is Dn */
        if ((opcode & 0x0038) == 0)
        {
            /* Direct offset and width */
            if ((opcode2 & 0x0820) == 0)
            {
                uint8_t src = RA_MapM68kRegister(&ptr, opcode & 7);
                uint8_t offset = (opcode2 >> 6) & 0x1f;
                uint8_t width = (opcode2) & 0x1f;

                *ptr++ = mov_reg(tmp, src);

                RA_FreeARMRegister(&ptr, src);
                /*
                    If offset == 0 and width == 0 the register value from Dn is already extracted bitfield,
                    otherwise extract bitfield
                */
                if (offset != 0 || width != 0)
                {
                    /* width == width - 1 */
                    width = (width == 0) ? 31 : width-1;
                    offset = 31 - (offset + width);
                    *ptr++ = sbfx(tmp, tmp, offset, width+1);
                }
            }
        }
        else
        {
            uint8_t dest;
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0);

            RA_FreeARMRegister(&ptr, dest);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        /* At this point extracted bitfield is in tmp register, compare it against 0, set zero and  */
        uint8_t mask = M68K_GetSRMask(*m68k_ptr);
        uint8_t update_mask = (SR_Z | SR_N | SR_C | SR_V) & ~mask;
        if (update_mask)
        {
            M68K_ModifyCC(&ptr);
            *ptr++ = cmp_immed(tmp, 0);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        }

        RA_FreeARMRegister(&ptr, tmp);
    }
    /* 1110110011xxxxxx - BFCLR */
    else if ((opcode & 0xffc0) == 0xecc0)
    {
        uint8_t ext_words = 1;
        uint16_t opcode2 = BE16((*m68k_ptr)[0]);
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        /* Special case: Target is Dn */
        if ((opcode & 0x0038) == 0)
        {
            /* Direct offset and width */
            if ((opcode2 & 0x0820) == 0)
            {
                uint8_t dst = RA_MapM68kRegister(&ptr, opcode & 7);
                uint8_t offset = (opcode2 >> 6) & 0x1f;
                uint8_t width = (opcode2) & 0x1f;

                /* Insert bitfield into destination register */
                width = (width == 0) ? 31 : width-1;
                offset = 31 - (offset + width);
                *ptr++ = sbfx(tmp, dst, offset, width+1);
                *ptr++ = bfc(dst, offset, width + 1);

                RA_SetDirtyM68kRegister(&ptr, opcode & 7);
            }
        }
        else
        {
            uint8_t dest;
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0);

            RA_FreeARMRegister(&ptr, dest);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        /* At this point extracted bitfield is in tmp register, compare it against 0, set zero and  */
        uint8_t mask = M68K_GetSRMask(*m68k_ptr);
        uint8_t update_mask = (SR_Z | SR_N | SR_C | SR_V) & ~mask;
        if (update_mask)
        {
            M68K_ModifyCC(&ptr);
            *ptr++ = cmp_immed(tmp, 0);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        }

        RA_FreeARMRegister(&ptr, tmp);
    }
    /* 1110110111xxxxxx - BFFFO */
    else if ((opcode & 0xffc0) == 0xedc0)
    {
        *ptr++ = udf(opcode);
    }
    /* 1110111011xxxxxx - BFSET */
    else if ((opcode & 0xffc0) == 0xeec0)
    {
        *ptr++ = udf(opcode);
    }
    /* 1110111111xxxxxx - BFINS */
    else if ((opcode & 0xffc0) == 0xefc0)
    {
        uint8_t ext_words = 1;
        uint16_t opcode2 = BE16((*m68k_ptr)[0]);
        uint8_t src = RA_MapM68kRegister(&ptr, (opcode2 >> 12) & 7);
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        /* Special case: Target is Dn */
        if ((opcode & 0x0038) == 0)
        {
            /* Direct offset and width */
            if ((opcode2 & 0x0820) == 0)
            {
                uint8_t dst = RA_MapM68kRegister(&ptr, opcode & 7);

                uint8_t offset = (opcode2 >> 6) & 0x1f;
                uint8_t width = (opcode2) & 0x1f;

                /* Sign-extract bitfield into temporary register. Will be used later to set condition codes */
                *ptr++ = sbfx(tmp, src, 0, width);

                /* Insert bitfield into destination register */
                width = (width == 0) ? 31 : width-1;
                offset = 31 - (offset + width);
                *ptr++ = bfi(dst, tmp, offset, width + 1);

                RA_SetDirtyM68kRegister(&ptr, opcode & 7);
            }
        }
        else
        {
            uint8_t dest;
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0);

            RA_FreeARMRegister(&ptr, dest);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        /* At this point extracted bitfield is in tmp register, compare it against 0, set zero and  */
        uint8_t mask = M68K_GetSRMask(*m68k_ptr);
        uint8_t update_mask = (SR_Z | SR_N | SR_C | SR_V) & ~mask;
        if (update_mask)
        {
            M68K_ModifyCC(&ptr);
            *ptr++ = cmp_immed(tmp, 0);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        }

        RA_FreeARMRegister(&ptr, tmp);
    }
    /* 1110xxxxxxx00xxx - ASL, ASR */
    else if ((opcode & 0xf018) == 0xe000)
    {
        uint8_t direction = (opcode >> 8) & 1;
        uint8_t shift = (opcode >> 9) & 7;
        uint8_t size = 1 << ((opcode >> 6) & 3);
        uint8_t regshift = (opcode >> 5) & 1;
        uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        if (regshift)
        {
            shift = RA_MapM68kRegister(&ptr, shift);

            if (direction)
            {
                switch(size)
                {
                    case 4:
                        *ptr++ = lsls_reg(reg, reg, shift);
                        break;
                    case 2:
                        *ptr++ = mov_reg_shift(tmp, reg, 16);
                        *ptr++ = lsls_reg(tmp, tmp, shift);
                        *ptr++ = lsr_immed(tmp, tmp, 16);
                        *ptr++ = bfi(reg, tmp, 0, 16);
                        break;
                    case 1:
                        *ptr++ = mov_reg_shift(tmp, reg, 24);
                        *ptr++ = lsls_reg(tmp, tmp, shift);
                        *ptr++ = lsr_immed(tmp, tmp, 24);
                        *ptr++ = bfi(reg, tmp, 0, 8);
                        break;
                }
            }
            else
            {
                switch (size)
                {
                    case 4:
                        *ptr++ = asrs_reg(reg, reg, shift);
                        break;
                    case 2:
                        *ptr++ = sxth(tmp, reg, 0);
                        *ptr++ = asrs_reg(tmp, tmp, shift);
                        *ptr++ = bfi(reg, tmp, 0, 16);
                        break;
                    case 1:
                        *ptr++ = sxtb(tmp, reg, 0);
                        *ptr++ = asrs_reg(tmp, tmp, shift);
                        *ptr++ = bfi(reg, tmp, 0, 8);
                        break;
                }
            }
        }
        else
        {
            if (!shift) shift = 8;

            if (direction)
            {
                switch (size)
                {
                    case 4:
                        *ptr++ = lsls_immed(reg, reg, shift);
                        break;
                    case 2:
                        *ptr++ = mov_reg_shift(tmp, reg, 16);
                        *ptr++ = lsls_immed(tmp, tmp, shift);
                        *ptr++ = lsr_immed(tmp, tmp, 16);
                        *ptr++ = bfi(reg, tmp, 0, 16);
                        break;
                    case 1:
                        *ptr++ = mov_reg_shift(tmp, reg, 24);
                        *ptr++ = lsls_immed(tmp, tmp, shift);
                        *ptr++ = lsr_immed(tmp, tmp, 24);
                        *ptr++ = bfi(reg, tmp, 0, 8);
                        break;
                }
            }
            else
            {
                switch (size)
                {
                case 4:
                    *ptr++ = asrs_immed(reg, reg, shift);
                    break;
                case 2:
                    *ptr++ = sxth(tmp, reg, 0);
                    *ptr++ = asrs_immed(tmp, tmp, shift);
                    *ptr++ = bfi(reg, tmp, 0, 16);
                    break;
                case 1:
                    *ptr++ = sxtb(tmp, reg, 0);
                    *ptr++ = asrs_immed(tmp, tmp, shift);
                    *ptr++ = bfi(reg, tmp, 0, 8);
                    break;
                }
            }
        }

        RA_FreeARMRegister(&ptr, tmp);

        ptr = EMIT_AdvancePC(ptr, 2);

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
            if (update_mask & (SR_X | SR_C))
                *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_X | SR_C);
        }
    }
    /* 1110xxxxxxx01xxx - LSL, LSR */
    else if ((opcode & 0xf018) == 0xe008)
    {
        uint8_t direction = (opcode >> 8) & 1;
        uint8_t shift = (opcode >> 9) & 7;
        uint8_t size = 1 << ((opcode >> 6) & 3);
        uint8_t regshift = (opcode >> 5) & 1;
        uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        if (regshift)
        {
            shift = RA_MapM68kRegister(&ptr, shift);

            if (direction)
            {
                switch (size)
                {
                case 4:
                    *ptr++ = lsls_reg(reg, reg, shift);
                    break;
                case 2:
                    *ptr++ = mov_reg_shift(tmp, reg, 16);
                    *ptr++ = lsls_reg(tmp, tmp, shift);
                    *ptr++ = lsr_immed(tmp, tmp, 16);
                    *ptr++ = bfi(reg, tmp, 0, 16);
                    break;
                case 1:
                    *ptr++ = mov_reg_shift(tmp, reg, 24);
                    *ptr++ = lsls_reg(tmp, tmp, shift);
                    *ptr++ = lsr_immed(tmp, tmp, 24);
                    *ptr++ = bfi(reg, tmp, 0, 8);
                    break;
                }
            }
            else
            {
                switch (size)
                {
                case 4:
                    *ptr++ = lsrs_reg(reg, reg, shift);
                    break;
                case 2:
                    *ptr++ = uxth(tmp, reg, 0);
                    *ptr++ = lsrs_reg(tmp, tmp, shift);
                    *ptr++ = bfi(reg, tmp, 0, 16);
                    break;
                case 1:
                    *ptr++ = uxtb(tmp, reg, 0);
                    *ptr++ = lsrs_reg(tmp, tmp, shift);
                    *ptr++ = bfi(reg, tmp, 0, 8);
                    break;
                }
            }
        }
        else
        {
            if (!shift)
                shift = 8;

            if (direction)
            {
                switch (size)
                {
                case 4:
                    *ptr++ = lsls_immed(reg, reg, shift);
                    break;
                case 2:
                    *ptr++ = mov_reg_shift(tmp, reg, 16);
                    *ptr++ = lsls_immed(tmp, tmp, shift);
                    *ptr++ = lsr_immed(tmp, tmp, 16);
                    *ptr++ = bfi(reg, tmp, 0, 16);
                    break;
                case 1:
                    *ptr++ = mov_reg_shift(tmp, reg, 24);
                    *ptr++ = lsls_immed(tmp, tmp, shift);
                    *ptr++ = lsr_immed(tmp, tmp, 24);
                    *ptr++ = bfi(reg, tmp, 0, 8);
                    break;
                }
            }
            else
            {
                switch (size)
                {
                case 4:
                    *ptr++ = lsrs_immed(reg, reg, shift);
                    break;
                case 2:
                    *ptr++ = uxth(tmp, reg, 0);
                    *ptr++ = lsrs_immed(tmp, tmp, shift);
                    *ptr++ = bfi(reg, tmp, 0, 16);
                    break;
                case 1:
                    *ptr++ = uxtb(tmp, reg, 0);
                    *ptr++ = lsrs_immed(tmp, tmp, shift);
                    *ptr++ = bfi(reg, tmp, 0, 8);
                    break;
                }
            }
        }

        RA_FreeARMRegister(&ptr, tmp);

        ptr = EMIT_AdvancePC(ptr, 2);

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
            if (update_mask & (SR_X | SR_C))
                *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_X | SR_C);
        }
    }
    /* 1110xxxxxxx10xxx - ROXL, ROXR */
    else if ((opcode & 0xf018) == 0xe010)
    {
        *ptr++ = udf(opcode);
    }
    /* Special case: the combination of RO(R/L).W #8, Dn; SWAP Dn; RO(R/L).W, Dn
       this is replaced by REV instruction */
    else if (((opcode & 0xfef8) == 0xe058) &&
             BE16((*m68k_ptr)[0]) == (0x4840 | (opcode & 7)) &&
             (BE16((*m68k_ptr)[1]) & 0xfeff) == (opcode & 0xfeff))
    {
        uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        *ptr++ = rev(reg, reg);

        ptr = EMIT_AdvancePC(ptr, 6);
        *m68k_ptr += 2;

        uint8_t mask = M68K_GetSRMask(*m68k_ptr);
        uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
            M68K_ModifyCC(&ptr);
            *ptr++ = cmp_immed(reg, 0);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            if (update_mask & (SR_C))
                *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_C);
        }
    }
    /* 1110xxxxxxx11xxx - ROL, ROR */
    else if ((opcode & 0xf018) == 0xe018)
    {
        uint8_t direction = (opcode >> 8) & 1;
        uint8_t shift = (opcode >> 9) & 7;
        uint8_t size = 1 << ((opcode >> 6) & 3);
        uint8_t regshift = (opcode >> 5) & 1;
        uint8_t reg = RA_MapM68kRegister(&ptr, opcode & 7);
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        if (regshift)
        {
            shift = RA_CopyFromM68kRegister(&ptr, shift);

            if (direction)
            {
                *ptr++ = rsb_immed(shift, shift, 32);
            }

            switch (size)
            {
                case 4:
                    *ptr++ = rors_reg(reg, reg, shift);
                    break;
                case 2:
                    *ptr++ = mov_reg(tmp, reg);
                    *ptr++ = bfi(tmp, tmp, 16, 16);
                    *ptr++ = rors_reg(tmp, tmp, shift);
                    *ptr++ = bfi(reg, tmp, 0, 16);
                    break;
                case 1:
                    *ptr++ = mov_reg(tmp, reg);
                    *ptr++ = bfi(tmp, tmp, 8, 8);
                    *ptr++ = bfi(tmp, tmp, 16, 16);
                    *ptr++ = rors_reg(tmp, tmp, shift);
                    *ptr++ = bfi(reg, tmp, 0, 8);
                    break;
            }

            RA_FreeARMRegister(&ptr, shift);
        }
        else
        {
            if (!shift)
                shift = 8;

            if (direction)
            {
                shift = 32 - shift;
            }

            switch (size)
            {
            case 4:
                *ptr++ = rors_immed(reg, reg, shift);
                break;
            case 2:
                *ptr++ = mov_reg(tmp, reg);
                *ptr++ = bfi(tmp, tmp, 16, 16);
                *ptr++ = rors_immed(tmp, tmp, shift);
                *ptr++ = bfi(reg, tmp, 0, 16);
                break;
            case 1:
                *ptr++ = mov_reg(tmp, reg);
                *ptr++ = bfi(tmp, tmp, 8, 8);
                *ptr++ = bfi(tmp, tmp, 16, 16);
                *ptr++ = rors_immed(tmp, tmp, shift);
                *ptr++ = bfi(reg, tmp, 0, 8);
                break;
            }
        }

        RA_FreeARMRegister(&ptr, tmp);

        ptr = EMIT_AdvancePC(ptr, 2);

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
            if (update_mask & (SR_C))
                *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_C);
        }
    }
    else
        *ptr++ = udf(opcode);

    return ptr;
}
