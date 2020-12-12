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

uint32_t *EMIT_lineE(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr);
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;
    *insn_consumed = 1;

    /* 1110000x11xxxxxx - ASL, ASR - memory */
    if ((opcode & 0xfec0) == 0xe0c0)
    {
        uint8_t direction = (opcode >> 8) & 1;
        uint8_t dest = 0xff;
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t ext_words = 0;
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

        *ptr++ = ldrsh_offset(dest, tmp, 0);

        if (direction)
        {
#ifdef __aarch64__
            *ptr++ = lsl(tmp, tmp, 1);

#else
            *ptr++ = lsls_immed(tmp, tmp, 17);
            *ptr++ = lsr_immed(tmp, tmp, 16);
#endif
        }
        else
        {
#ifdef __aarch64__
            *ptr++ = asr(tmp, tmp, 1);
#else
            *ptr++ = asrs_immed(tmp, tmp, 1);
#endif
        }

        *ptr++ = strh_offset(dest, tmp, 0);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));

        if (update_mask)
        {
#ifdef __aarch64__
            uint8_t cc = RA_ModifyCC(&ptr);
            *ptr++ = cmn_reg(31, tmp, LSL, 16);
            *ptr++ = mov_immed_u16(tmp, update_mask, 0);
            *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);

            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
            if (update_mask & (SR_C | SR_X)) {
                *ptr++ = b_cc(A64_CC_CS ^ 1, 3);
                *ptr++ = mov_immed_u16(tmp, SR_C | SR_X, 0);
                *ptr++ = orr_reg(cc, cc, tmp, LSL, 0);
            }
#else
            M68K_ModifyCC(&ptr);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            if (update_mask & (SR_X | SR_C))
                *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_X | SR_C);
#endif
        }
        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, dest);
    }
    /* 1110001x11xxxxxx - LSL, LSR - memory */
    else if ((opcode & 0xfec0) == 0xe2c0)
    {
        uint8_t direction = (opcode >> 8) & 1;
        uint8_t dest = 0xff;
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t ext_words = 0;
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

        *ptr++ = ldrh_offset(dest, tmp, 0);

        if (direction)
        {
#ifdef __aarch64__
            *ptr++ = lsl(tmp, tmp, 1);
#else
            *ptr++ = lsls_immed(tmp, tmp, 17);
            *ptr++ = lsr_immed(tmp, tmp, 16);
#endif
        }
        else
        {
#ifdef __aarch64__
            *ptr++ = lsr(tmp, tmp, 1);

#else
            *ptr++ = lsrs_immed(tmp, tmp, 1);
#endif
        }

        *ptr++ = strh_offset(dest, tmp, 0);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));

        if (update_mask)
        {
#ifdef __aarch64__
            uint8_t cc = RA_ModifyCC(&ptr);
            *ptr++ = cmn_reg(31, tmp, LSL, 16);
            *ptr++ = mov_immed_u16(tmp, update_mask, 0);
            *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);

            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
            if (update_mask & (SR_C | SR_X)) {
                *ptr++ = b_cc(A64_CC_CS ^ 1, 3);
                *ptr++ = mov_immed_u16(tmp, SR_C | SR_X, 0);
                *ptr++ = orr_reg(cc, cc, tmp, LSL, 0);
            }
#else
            M68K_ModifyCC(&ptr);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            if (update_mask & (SR_X | SR_C))
                *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_X | SR_C);
#endif
        }
        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, dest);
    }
    /* 1110010x11xxxxxx - ROXL, ROXR - memory */
    else if ((opcode & 0xfec0) == 0xe4c0)
    {
        ptr = EMIT_InjectDebugString(ptr, "[JIT] ROXL/ROXR at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
    }
    /* 1110011x11xxxxxx - ROL, ROR - memory */
    else if ((opcode & 0xfec0) == 0xe6c0)
    {
        uint8_t direction = (opcode >> 8) & 1;
        uint8_t dest = 0xff;
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t ext_words = 0;
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

        *ptr++ = ldrh_offset(dest, tmp, 0);
        *ptr++ = bfi(tmp, tmp, 16, 16);

        if (direction)
        {
#ifdef __aarch64__
            *ptr++ = ror(tmp, tmp, 32 - 1);
#else
            *ptr++ = rors_immed(tmp, tmp, 32 - 1);
#endif
        }
        else
        {
#ifdef __aarch64__
            *ptr++ = ror(tmp, tmp, 1);
#else
            *ptr++ = rors_immed(tmp, tmp, 1);
#endif
        }

        *ptr++ = strh_offset(dest, tmp, 0);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));

        if (update_mask)
        {
#ifdef __aarch64__
            uint8_t cc = RA_ModifyCC(&ptr);
            *ptr++ = cmn_reg(31, tmp, LSL, 16);
            *ptr++ = mov_immed_u16(tmp, update_mask, 0);
            *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);

            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
            if (update_mask & (SR_C | SR_X)) {
                *ptr++ = b_cc(A64_CC_CS ^ 1, 3);
                *ptr++ = mov_immed_u16(tmp, SR_C | SR_X, 0);
                *ptr++ = orr_reg(cc, cc, tmp, LSL, 0);
            }
#else
            M68K_ModifyCC(&ptr);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            if (update_mask & (SR_X | SR_C))
                *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_C);
#endif
        }
        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, dest);
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
                    *ptr++ = sbfx(tmp, src, offset, width+1);
                }
                else
                {
                    *ptr++ = mov_reg(tmp, src);
                }
            }
            /* Direct offset, width in reg */
            else if ((opcode2 & 0x0820) == 0x0020)
            {
                uint8_t offset = (opcode2 >> 6) & 0x1f;
                uint8_t width_reg_t = RA_MapM68kRegister(&ptr, opcode2 & 7);
                uint8_t width_reg = RA_AllocARMRegister(&ptr);
                *ptr++ = mov_reg(tmp, src);
#ifdef __aarch64__
                *ptr++ = neg_reg(width_reg, width_reg_t, LSL, 0);
                *ptr++ = add_immed(width_reg, width_reg, 32);
                *ptr++ = and_immed(width_reg, width_reg, 5, 0);
#else
                *ptr++ = rsb_immed(width_reg, width_reg_t, 32);
                *ptr++ = and_immed(width_reg, width_reg, 31);
#endif
                RA_FreeARMRegister(&ptr, width_reg_t);

#ifdef __aarch64__
                if (offset > 0)
                {
                    *ptr++ = lsl(tmp, tmp, offset);
                }
                *ptr++ = asrv(tmp, tmp, width_reg);
#else
                if (offset > 0)
                {
                    *ptr++ = lsl_immed(tmp, tmp, offset);
                }
                *ptr++ = asr_reg(tmp, tmp, width_reg);
#endif

                RA_FreeARMRegister(&ptr, width_reg);
            }
            /* Offset in reg, direct width */
            else if ((opcode2 & 0x0820) == 0x0800)
            {
                uint8_t width = (opcode2) & 0x1f;
                uint8_t offset_reg = RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);

                width = 32 - width;
#ifdef __aarch64__
                *ptr++ = lsrv(tmp, tmp, offset_reg);
#else
                *ptr++ = lsr_reg(tmp, tmp, offset_reg);
#endif
                *ptr++ = sbfx(tmp, tmp, 0, width);

                RA_FreeARMRegister(&ptr, offset_reg);
            }
            /* Both offset and width in regs */
            else
            {
                uint8_t width_reg_t = RA_MapM68kRegister(&ptr, opcode2 & 7);
                uint8_t width_reg = RA_AllocARMRegister(&ptr);
                uint8_t offset_reg = RA_MapM68kRegister(&ptr, (opcode2 >> 6) & 7);

#ifdef __aarch64__
                *ptr++ = neg_reg(width_reg, width_reg_t, LSL, 0);
                *ptr++ = add_immed(width_reg, width_reg, 32);
                *ptr++ = and_immed(width_reg, width_reg, 5, 0);
#else
                *ptr++ = rsb_immed(width_reg, width_reg_t, 32);
                *ptr++ = and_immed(width_reg, width_reg, 31);
#endif
                RA_FreeARMRegister(&ptr, width_reg_t);

#ifdef __aarch64__
                *ptr++ = lslv(tmp, tmp, offset_reg);
                *ptr++ = asrv(tmp, tmp, width_reg);
#else
                *ptr++ = lsl_reg(tmp, tmp, offset_reg);
                *ptr++ = asr_reg(tmp, tmp, width_reg);
#endif
                RA_FreeARMRegister(&ptr, width_reg);
                RA_FreeARMRegister(&ptr, offset_reg);
            }
        }
        else
        {
            uint8_t dest;
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

            ptr = EMIT_InjectDebugString(ptr, "[JIT] BFTST at %08x partially not implemented\n", *m68k_ptr - 1);
            ptr = EMIT_InjectPrintContext(ptr);

            RA_FreeARMRegister(&ptr, dest);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        /* At this point extracted bitfield is in tmp register, compare it against 0, set zero and  */
        if (update_mask)
        {
#ifdef __aarch64__
            uint8_t cc = RA_ModifyCC(&ptr);
            *ptr++ = mov_immed_u16(tmp, update_mask, 0);
            *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);
            *ptr++ = cmn_reg(31, tmp, LSL, 0);

            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
#else
            M68K_ModifyCC(&ptr);
            *ptr++ = cmp_immed(tmp, 0);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
#endif
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
                    *ptr++ = ubfx(tmp, src, offset, width+1);
                }
                else
                {
                    *ptr++ = mov_reg(tmp, src);
                }
            }
        }
        else
        {
            uint8_t dest = 0xff;
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

            ptr = EMIT_InjectDebugString(ptr, "[JIT] BFEXTU at %08x partially not implemented\n", *m68k_ptr - 1);
            ptr = EMIT_InjectPrintContext(ptr);

            RA_FreeARMRegister(&ptr, dest);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        /* At this point extracted bitfield is in tmp register, compare it against 0, set zero and  */
        if (update_mask)
        {
#ifdef __aarch64__
            uint8_t cc = RA_ModifyCC(&ptr);
            *ptr++ = mov_immed_u16(tmp, update_mask, 0);
            *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);
            *ptr++ = cmn_reg(31, tmp, LSL, 0);

            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
#else
            M68K_ModifyCC(&ptr);
            *ptr++ = cmp_immed(tmp, 0);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
#endif
        }

        RA_FreeARMRegister(&ptr, tmp);
    }
    /* 1110101011xxxxxx - BFCHG */
    else if ((opcode & 0xffc0) == 0xeac0)
    {
        ptr = EMIT_InjectDebugString(ptr, "[JIT] BFCHG at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
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
                    *ptr++ = sbfx(tmp, src, offset, width+1);
                }
                else
                {
                    *ptr++ = mov_reg(tmp, src);
                }
            }
        }
        else
        {
            uint8_t dest = 0xff;
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

            ptr = EMIT_InjectDebugString(ptr, "[JIT] BFEXTS at %08x not implemented\n", *m68k_ptr - 1);
            ptr = EMIT_InjectPrintContext(ptr);

            RA_FreeARMRegister(&ptr, dest);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        /* At this point extracted bitfield is in tmp register, compare it against 0, set zero and  */
        if (update_mask)
        {
#ifdef __aarch64__
            uint8_t cc = RA_ModifyCC(&ptr);
            *ptr++ = mov_immed_u16(tmp, update_mask, 0);
            *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);
            *ptr++ = cmn_reg(31, tmp, LSL, 0);

            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
#else
            M68K_ModifyCC(&ptr);
            *ptr++ = cmp_immed(tmp, 0);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
#endif
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
#ifdef __aarch64__
                *ptr++ = bfi(dst, 31, offset, width + 1);
#else
                *ptr++ = bfc(dst, offset, width + 1);
#endif
                RA_SetDirtyM68kRegister(&ptr, opcode & 7);
            }
        }
        else
        {
            uint8_t dest = 0xff;
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

            ptr = EMIT_InjectDebugString(ptr, "[JIT] BFCLR at %08x not implemented\n", *m68k_ptr - 1);
            ptr = EMIT_InjectPrintContext(ptr);

            RA_FreeARMRegister(&ptr, dest);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        /* At this point extracted bitfield is in tmp register, compare it against 0, set zero and  */
        if (update_mask)
        {
#ifdef __aarch64__
            uint8_t cc = RA_ModifyCC(&ptr);
            *ptr++ = mov_immed_u16(tmp, update_mask, 0);
            *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);
            *ptr++ = cmn_reg(31, tmp, LSL, 0);

            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
#else
            M68K_ModifyCC(&ptr);
            *ptr++ = cmp_immed(tmp, 0);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
#endif
        }

        RA_FreeARMRegister(&ptr, tmp);
    }
    /* 1110110111xxxxxx - BFFFO */
    else if ((opcode & 0xffc0) == 0xedc0)
    {
        uint8_t ext_words = 1;
        uint16_t opcode2 = BE16((*m68k_ptr)[0]);
        uint8_t dst = RA_MapM68kRegisterForWrite(&ptr, (opcode2 >> 12) & 7);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t src = 0xff;

        /* Special case: Source is Dn */
        if ((opcode & 0x0038) == 0)
        {
            /* Direct offset and width */
            if ((opcode2 & 0x0820) == 0)
            {
                uint8_t offset = (opcode2 >> 6) & 0x1f;
                uint8_t width = opcode2 & 0x1f;

                /* Another special case - full width and zero offset */
                if (offset == 0 && width == 0)
                {
                    src = RA_MapM68kRegister(&ptr, opcode & 7);

                    *ptr++ = clz(dst, src);
                }
                else
                {
                    src = RA_CopyFromM68kRegister(&ptr, opcode & 7);

                    if (offset)
                    {
                        *ptr++ = bic_immed(src, src, offset, offset);
                    }
                    if (width)
                    {
                        *ptr++ = orr_immed(src, src, width, 0);
                    }

                    *ptr++ = clz(dst, src);
                }

                ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
                (*m68k_ptr) += ext_words;

                if (update_mask)
                {
                    uint8_t cc = RA_ModifyCC(&ptr);
                    if (width)
                    {
                        *ptr++ = bic_immed(src, src, width, 0);
                    }
                    *ptr++ = cmn_reg(31, src, LSL, offset);
                    ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
                }
            }
            else
            {
                ptr = EMIT_InjectDebugString(ptr, "[JIT] BFFFO at %08x not implemented\n", *m68k_ptr - 1);
                ptr = EMIT_InjectPrintContext(ptr);
                *ptr++ = udf(opcode);
            }
        }
        else
        {
            ptr = EMIT_InjectDebugString(ptr, "[JIT] BFFFO at %08x not implemented\n", *m68k_ptr - 1);
            ptr = EMIT_InjectPrintContext(ptr);
            *ptr++ = udf(opcode);
        }

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, src);
        RA_FreeARMRegister(&ptr, dst);
    }
    /* 1110111011xxxxxx - BFSET */
    else if ((opcode & 0xffc0) == 0xeec0)
    {

        ptr = EMIT_InjectDebugString(ptr, "[JIT] BFSET at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
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
            else
            {
                ptr = EMIT_InjectDebugString(ptr, "[JIT] BFINS at %08x not implemented\n", *m68k_ptr - 1);
                ptr = EMIT_InjectPrintContext(ptr);
            }
        }
        else
        {
            uint8_t dest = 0xff;
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

            ptr = EMIT_InjectDebugString(ptr, "[JIT] BFINS at %08x not implemented\n", *m68k_ptr - 1);
            ptr = EMIT_InjectPrintContext(ptr);

            RA_FreeARMRegister(&ptr, dest);
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;

        /* At this point extracted bitfield is in tmp register, compare it against 0, set zero and  */
        if (update_mask)
        {
#ifdef __aarch64__
            uint8_t cc = RA_ModifyCC(&ptr);
            *ptr++ = mov_immed_u16(tmp, update_mask, 0);
            *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);
            *ptr++ = cmn_reg(31, tmp, LSL, 0);

            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
#else
            M68K_ModifyCC(&ptr);
            *ptr++ = cmp_immed(tmp, 0);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
#endif
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
#ifdef __aarch64__
                        *ptr++ = lslv(reg, reg, shift);
#else
                        *ptr++ = lsls_reg(reg, reg, shift);
#endif
                        break;
                    case 2:
#ifdef __aarch64__
                        *ptr++ = lslv(tmp, reg, shift);
#else
                        *ptr++ = mov_reg_shift(tmp, reg, 16);
                        *ptr++ = lsls_reg(tmp, tmp, shift);
                        *ptr++ = lsr_immed(tmp, tmp, 16);
#endif
                        *ptr++ = bfi(reg, tmp, 0, 16);
                        break;
                    case 1:
#ifdef __aarch64__
                        *ptr++ = lslv(tmp, reg, shift);
#else
                        *ptr++ = mov_reg_shift(tmp, reg, 24);
                        *ptr++ = lsls_reg(tmp, tmp, shift);
                        *ptr++ = lsr_immed(tmp, tmp, 24);
#endif
                        *ptr++ = bfi(reg, tmp, 0, 8);
                        break;
                }
            }
            else
            {
                switch (size)
                {
                    case 4:
#ifdef __aarch64__
                        *ptr++ = asrv(reg, reg, shift);
#else
                        *ptr++ = asrs_reg(reg, reg, shift);
#endif
                        break;
                    case 2:
#ifdef __aarch64__
                        *ptr++ = sxth(tmp, reg);
                        *ptr++ = asrv(tmp, tmp, shift);
#else
                        *ptr++ = sxth(tmp, reg, 0);
                        *ptr++ = asrs_reg(tmp, tmp, shift);
#endif
                        *ptr++ = bfi(reg, tmp, 0, 16);
                        break;
                    case 1:
#ifdef __aarch64__
                        *ptr++ = sxtb(tmp, reg);
                        *ptr++ = asrv(tmp, tmp, shift);
#else
                        *ptr++ = sxtb(tmp, reg, 0);
                        *ptr++ = asrs_reg(tmp, tmp, shift);
#endif
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
#ifdef __aarch64__
                        *ptr++ = lsl(reg, reg, shift);
#else
                        *ptr++ = lsls_immed(reg, reg, shift);
#endif
                        break;
                    case 2:
#ifdef __aarch64__
                        *ptr++ = lsl(tmp, reg, shift);
#else
                        *ptr++ = mov_reg_shift(tmp, reg, 16);
                        *ptr++ = lsls_immed(tmp, tmp, shift);
                        *ptr++ = lsr_immed(tmp, tmp, 16);
#endif
                        *ptr++ = bfi(reg, tmp, 0, 16);
                        break;
                    case 1:
#ifdef __aarch64__
                        *ptr++ = lsl(tmp, reg, shift);
#else
                        *ptr++ = mov_reg_shift(tmp, reg, 24);
                        *ptr++ = lsls_immed(tmp, tmp, shift);
                        *ptr++ = lsr_immed(tmp, tmp, 24);
#endif
                        *ptr++ = bfi(reg, tmp, 0, 8);
                        break;
                }
            }
            else
            {
                switch (size)
                {
                case 4:
#ifdef __aarch64__
                    *ptr++ = asr(reg, reg, shift);
#else
                    *ptr++ = asrs_immed(reg, reg, shift);
#endif
                    break;
                case 2:
#ifdef __aarch64__
                    *ptr++ = sxth(tmp, reg);
                    *ptr++ = asr(tmp, tmp, shift);
#else
                    *ptr++ = sxth(tmp, reg, 0);
                    *ptr++ = asrs_immed(tmp, tmp, shift);
#endif
                    *ptr++ = bfi(reg, tmp, 0, 16);
                    break;
                case 1:
#ifdef __aarch64__
                    *ptr++ = sxtb(tmp, reg);
                    *ptr++ = asr(tmp, tmp, shift);
#else
                    *ptr++ = sxtb(tmp, reg, 0);
                    *ptr++ = asrs_immed(tmp, tmp, shift);
#endif
                    *ptr++ = bfi(reg, tmp, 0, 8);
                    break;
                }
            }
        }

        ptr = EMIT_AdvancePC(ptr, 2);

        if (update_mask)
        {
#ifdef __aarch64__
            uint8_t cc = RA_ModifyCC(&ptr);

            switch(size)
            {
                case 4:
                    *ptr++ = cmn_reg(31, reg, LSL, 0);
                    break;
                case 2:
                    *ptr++ = cmn_reg(31, tmp, LSL, 16);
                    break;
                case 1:
                    *ptr++ = cmn_reg(31, tmp, LSL, 24);
                    break;
            }

            *ptr++ = mov_immed_u16(tmp, update_mask, 0);
            *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);

            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
            if (update_mask & (SR_C | SR_X)) {
                *ptr++ = b_cc(A64_CC_CS ^ 1, 3);
                *ptr++ = mov_immed_u16(tmp, SR_C | SR_X, 0);
                *ptr++ = orr_reg(cc, cc, tmp, LSL, 0);
            }
#else
            M68K_ModifyCC(&ptr);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            if (update_mask & (SR_X | SR_C))
                *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_X | SR_C);
#endif
        }
        RA_FreeARMRegister(&ptr, tmp);
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
#ifdef __aarch64__
                    *ptr++ = lslv(reg, reg, shift);
#else
                    *ptr++ = lsls_reg(reg, reg, shift);
#endif
                    break;
                case 2:
#ifdef __aarch64__
                    *ptr++ = mov_reg(tmp, reg);
                    *ptr++ = lslv(reg, reg, shift);
#else
                    *ptr++ = mov_reg_shift(tmp, reg, 16);
                    *ptr++ = lsls_reg(tmp, tmp, shift);
                    *ptr++ = lsr_immed(tmp, tmp, 16);
#endif
                    *ptr++ = bfi(reg, tmp, 0, 16);
                    break;
                case 1:
#ifdef __aarch64__
                    *ptr++ = mov_reg(tmp, reg);
                    *ptr++ = lslv(reg, reg, shift);
#else
                    *ptr++ = mov_reg_shift(tmp, reg, 24);
                    *ptr++ = lsls_reg(tmp, tmp, shift);
                    *ptr++ = lsr_immed(tmp, tmp, 24);
#endif
                    *ptr++ = bfi(reg, tmp, 0, 8);
                    break;
                }
            }
            else
            {
                switch (size)
                {
                case 4:
#ifdef __aarch64__
                    *ptr++ = lsrv(reg, reg, shift);
#else
                    *ptr++ = lsrs_reg(reg, reg, shift);
#endif
                    break;
                case 2:
#ifdef __aarch64__
                    *ptr++ = uxth(tmp, reg);
                    *ptr++ = lsrv(tmp, tmp, shift);
#else
                    *ptr++ = uxth(tmp, reg, 0);
                    *ptr++ = lsrs_reg(tmp, tmp, shift);
#endif
                    *ptr++ = bfi(reg, tmp, 0, 16);
                    break;
                case 1:
#ifdef __aarch64__
                    *ptr++ = uxtb(tmp, reg);
                    *ptr++ = lsrv(tmp, tmp, shift);
#else
                    *ptr++ = uxtb(tmp, reg, 0);
                    *ptr++ = lsrs_reg(tmp, tmp, shift);
#endif
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
#ifdef __aarch64__
                    *ptr++ = lsl(reg, reg, shift);
#else
                    *ptr++ = lsls_immed(reg, reg, shift);
#endif
                    break;
                case 2:
#ifdef __aarch64__
                    *ptr++ = lsl(tmp, reg, shift);
#else
                    *ptr++ = mov_reg_shift(tmp, reg, 16);
                    *ptr++ = lsls_immed(tmp, tmp, shift);
                    *ptr++ = lsr_immed(tmp, tmp, 16);
#endif
                    *ptr++ = bfi(reg, tmp, 0, 16);
                    break;
                case 1:
#ifdef __aarch64__
                    *ptr++ = lsl(tmp, reg, shift);
#else
                    *ptr++ = mov_reg_shift(tmp, reg, 24);
                    *ptr++ = lsls_immed(tmp, tmp, shift);
                    *ptr++ = lsr_immed(tmp, tmp, 24);
#endif
                    *ptr++ = bfi(reg, tmp, 0, 8);
                    break;
                }
            }
            else
            {
                switch (size)
                {
                case 4:
#ifdef __aarch64__
                    *ptr++ = lsr(reg, reg, shift);
#else
                    *ptr++ = lsrs_immed(reg, reg, shift);
#endif
                    break;
                case 2:
#ifdef __aarch64__
                    *ptr++ = uxth(tmp, reg);
                    *ptr++ = lsr(tmp, tmp, shift);
#else
                    *ptr++ = uxth(tmp, reg, 0);
                    *ptr++ = lsrs_immed(tmp, tmp, shift);
#endif
                    *ptr++ = bfi(reg, tmp, 0, 16);
                    break;
                case 1:
#ifdef __aarch64__
                    *ptr++ = uxtb(tmp, reg);
                    *ptr++ = lsr(tmp, tmp, shift);
#else
                    *ptr++ = uxtb(tmp, reg, 0);
                    *ptr++ = lsrs_immed(tmp, tmp, shift);
#endif
                    *ptr++ = bfi(reg, tmp, 0, 8);
                    break;
                }
            }
        }

        ptr = EMIT_AdvancePC(ptr, 2);

        if (update_mask)
        {
#ifdef __aarch64__
            uint8_t cc = RA_ModifyCC(&ptr);

            switch(size)
            {
                case 4:
                    *ptr++ = cmn_reg(31, reg, LSL, 0);
                    break;
                case 2:
                    *ptr++ = cmn_reg(31, tmp, LSL, 16);
                    break;
                case 1:
                    *ptr++ = cmn_reg(31, tmp, LSL, 24);
                    break;
            }

            *ptr++ = mov_immed_u16(tmp, update_mask, 0);
            *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);

            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
            if (update_mask & (SR_C | SR_X)) {
                *ptr++ = b_cc(A64_CC_CS ^ 1, 3);
                *ptr++ = mov_immed_u16(tmp, SR_C | SR_X, 0);
                *ptr++ = orr_reg(cc, cc, tmp, LSL, 0);
            }
#else
            M68K_ModifyCC(&ptr);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            if (update_mask & (SR_X | SR_C))
                *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_X | SR_C);
#endif
        }
        RA_FreeARMRegister(&ptr, tmp);
    }
    /* 1110xxxxxxx10xxx - ROXL, ROXR */
    else if ((opcode & 0xf018) == 0xe010)
    {

        ptr = EMIT_InjectDebugString(ptr, "[JIT] ROXL/ROXR at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
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

        *insn_consumed = 3;

        *ptr++ = rev(reg, reg);

        ptr = EMIT_AdvancePC(ptr, 6);
        *m68k_ptr += 2;

        if (update_mask)
        {
#ifdef __aarch64__
            uint8_t cc = RA_ModifyCC(&ptr);
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = cmn_reg(31, reg, LSL, 0);
            *ptr++ = mov_immed_u16(tmp, update_mask, 0);
            *ptr++ = bic_reg(cc, cc, tmp, LSL, 0);

            if (update_mask & SR_Z) {
                *ptr++ = b_cc(A64_CC_EQ ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_Z) & 31);
            }
            if (update_mask & SR_N) {
                *ptr++ = b_cc(A64_CC_MI ^ 1, 2);
                *ptr++ = orr_immed(cc, cc, 1, (32 - SRB_N) & 31);
            }
            if (update_mask & (SR_C | SR_X)) {
                *ptr++ = b_cc(A64_CC_CS ^ 1, 3);
                *ptr++ = mov_immed_u16(tmp, SR_C | SR_X, 0);
                *ptr++ = orr_reg(cc, cc, tmp, LSL, 0);
            }
            RA_FreeARMRegister(&ptr, tmp);
#else
            M68K_ModifyCC(&ptr);
            *ptr++ = cmp_immed(reg, 0);
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            if (update_mask & (SR_C))
                *ptr++ = orr_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_C);
#endif
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
#ifdef __aarch64__
                *ptr++ = neg_reg(shift, shift, LSL, 0);
                *ptr++ = add_immed(shift, shift, 32);
#else
                *ptr++ = rsb_immed(shift, shift, 32);
#endif
            }

            switch (size)
            {
                case 4:
#ifdef __aarch64__
                    *ptr++ = rorv(reg, reg, shift);
#else
                    *ptr++ = rors_reg(reg, reg, shift);
#endif
                    break;
                case 2:
                    *ptr++ = mov_reg(tmp, reg);
                    *ptr++ = bfi(tmp, tmp, 16, 16);
#ifdef __aarch64__
                    *ptr++ = rorv(tmp, tmp, shift);
#else
                    *ptr++ = rors_reg(tmp, tmp, shift);
#endif
                    *ptr++ = bfi(reg, tmp, 0, 16);
                    break;
                case 1:
                    *ptr++ = mov_reg(tmp, reg);
                    *ptr++ = bfi(tmp, tmp, 8, 8);
                    *ptr++ = bfi(tmp, tmp, 16, 16);
#ifdef __aarch64__
                    *ptr++ = rorv(tmp, tmp, shift);
#else
                    *ptr++ = rors_reg(tmp, tmp, shift);
#endif
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
#ifdef __aarch64__
                *ptr++ = ror(reg, reg, shift);
#else
                *ptr++ = rors_immed(reg, reg, shift);
#endif
                break;
            case 2:
                *ptr++ = mov_reg(tmp, reg);
                *ptr++ = bfi(tmp, tmp, 16, 16);
#ifdef __aarch64__
                *ptr++ = ror(tmp, tmp, shift);
#else
                *ptr++ = rors_immed(tmp, tmp, shift);
#endif
                *ptr++ = bfi(reg, tmp, 0, 16);
                break;
            case 1:
                *ptr++ = mov_reg(tmp, reg);
                *ptr++ = bfi(tmp, tmp, 8, 8);
                *ptr++ = bfi(tmp, tmp, 16, 16);
#ifdef __aarch64__
                *ptr++ = ror(tmp, tmp, shift);
#else
                *ptr++ = rors_immed(tmp, tmp, shift);
#endif
                *ptr++ = bfi(reg, tmp, 0, 8);
                break;
            }
        }

        ptr = EMIT_AdvancePC(ptr, 2);

        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
#ifdef __aarch64__
            switch(size)
            {
                case 4:
                    *ptr++ = cmn_reg(31, reg, LSL, 0);
                    break;
                case 2:
                    *ptr++ = cmn_reg(31, tmp, LSL, 16);
                    break;
                case 1:
                    *ptr++ = cmn_reg(31, tmp, LSL, 24);
                    break;
            }
#endif
            uint8_t old_mask = update_mask & SR_C;
            ptr = EMIT_GetNZxx(ptr, cc, &update_mask);
            update_mask |= old_mask;

            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_N) {
#ifdef __aarch64__
                if (direction) {
                    switch(size) {
                        case 4:
                            *ptr++ = bfxil(cc, reg, 31, 1);
                            break;
                        case 2:
                            *ptr++ = bfxil(cc, reg, 15, 1);
                            break;
                        case 1:
                            *ptr++ = bfxil(cc, reg, 7, 1);
                            break;
                    }
                }
                else {
                    if (size == 4)
                        *ptr++ = bfi(cc, reg, 0, 1);
                    else
                        *ptr++ = bfi(cc, tmp, 0, 1);
                }
#else
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_C, ARM_CC_CS);
#endif
            }

        }
        RA_FreeARMRegister(&ptr, tmp);
    }
    else
    {
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
    }

    return ptr;
}
