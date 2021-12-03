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

/* Line9 is one large ADDX/ADD/ADDA */

static uint32_t *EMIT_ADD(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_ADD_reg")));
static uint32_t *EMIT_ADD_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_ADD_mem")));
static uint32_t *EMIT_ADD_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_ADD_ext")));
static uint32_t *EMIT_ADD_ext(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t direction = (opcode >> 8) & 1; // 0: Ea+Dn->Dn, 1: Ea+Dn->Ea
    uint8_t ext_words = 0;
#ifdef __aarch64__
    uint8_t tmp = 0xff;
#endif

    if (direction == 0)
    {
        uint8_t dest = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
        uint8_t src = 0xff;

        RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
        if (size == 4)
            ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
        else
            ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

        switch (size)
        {
        case 4:
#ifdef __aarch64__
            *ptr++ = adds_reg(dest, dest, src, LSL, 0);
#else
            *ptr++ = adds_reg(dest, dest, src, 0);
#endif
            break;
        case 2:
#ifdef __aarch64__
            tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = lsl(tmp, dest, 16);
            *ptr++ = adds_reg(src, tmp, src, LSL, 16);
            *ptr++ = bfxil(dest, src, 16, 16);
            RA_FreeARMRegister(&ptr, tmp);
#else
            *ptr++ = lsl_immed(src, src, 16);
            *ptr++ = adds_reg(src, src, dest, 16);
            *ptr++ = lsr_immed(src, src, 16);
            *ptr++ = bfi(dest, src, 0, 16);
#endif
            break;
        case 1:
#ifdef __aarch64__
            tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = lsl(tmp, dest, 24);
            *ptr++ = adds_reg(src, tmp, src, LSL, 24);
            *ptr++ = bfxil(dest, src, 24, 8);
            RA_FreeARMRegister(&ptr, tmp);
#else
            *ptr++ = lsl_immed(src, src, 24);
            *ptr++ = adds_reg(src, src, dest, 24);
            *ptr++ = lsr_immed(src, src, 24);
            *ptr++ = bfi(dest, src, 0, 8);
#endif
            break;
        }

        RA_FreeARMRegister(&ptr, src);
    }
    else
    {
        uint8_t dest = 0xff;
        uint8_t src = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;

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
            *ptr++ = adds_reg(tmp, tmp, src, LSL, 0);
#else
            *ptr++ = adds_reg(tmp, tmp, src, 0);
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
            *ptr++ = lsl(tmp, tmp, 16);
            *ptr++ = adds_reg(tmp, tmp, src, LSL, 16);
            *ptr++ = lsr(tmp, tmp, 16);
#else
            *ptr++ = lsl_immed(tmp, tmp, 16);
            *ptr++ = adds_reg(tmp, tmp, src, 16);
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
            *ptr++ = lsl(tmp, tmp, 24);
            *ptr++ = adds_reg(tmp, tmp, src, LSL, 24);
            *ptr++ = lsr(tmp, tmp, 24);
#else
            *ptr++ = lsl_immed(tmp, tmp, 24);
            *ptr++ = adds_reg(tmp, tmp, src, 24);
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
        RA_FreeARMRegister(&ptr, tmp);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

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

static uint32_t *EMIT_ADDA(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_ADDA_reg")));
static uint32_t *EMIT_ADDA_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_ADDA_mem")));
static uint32_t *EMIT_ADDA_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_ADDA_ext")));
static uint32_t *EMIT_ADDA_ext(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_words = 0;
    uint8_t size = (opcode & 0x0100) == 0x0100 ? 4 : 2;
    uint8_t reg = RA_MapM68kRegister(&ptr, ((opcode >> 9) & 7) + 8);
    uint8_t tmp = 0xff;

    if (size == 2)
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);
    else
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

#ifdef __aarch64__
    if (size == 2)
        *ptr++ = sxth(tmp, tmp);

    *ptr++ = add_reg(reg, reg, tmp, LSL, 0);
#else
    if (size == 2)396
        *ptr++ = sxth(tmp, tmp, 0);

    *ptr++ = add_reg(reg, reg, tmp, 0);
#endif
    RA_SetDirtyM68kRegister(&ptr, ((opcode >> 9) & 7) + 8);

    RA_FreeARMRegister(&ptr, tmp);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    return ptr;
}

static uint32_t *EMIT_ADDX(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_ADDX_reg")));
static uint32_t *EMIT_ADDX_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_ADDX_mem")));
static uint32_t *EMIT_ADDX_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t size = (opcode >> 6) & 3;
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
#ifdef __aarch64__
    uint8_t cc = RA_GetCC(&ptr);
    if (size == 2) {
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        *ptr++ = ror(tmp, cc, 7);
        *ptr++ = set_nzcv(tmp);

        RA_FreeARMRegister(&ptr, tmp);
    } else {
        *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_X));
    }
#else
    M68K_GetCC(&ptr);
    *ptr++ = tst_immed(REG_SR, SR_X);
#endif
    /* Register to register */
    if ((opcode & 0x0008) == 0)
    {
        uint8_t regx = RA_MapM68kRegister(&ptr, opcode & 7);
        uint8_t regy = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
        uint8_t tmp = 0;
        uint8_t tmp_2 = 0;
        uint8_t tmp_cc_1 = RA_AllocARMRegister(&ptr);
        uint8_t tmp_cc_2 = RA_AllocARMRegister(&ptr);

        RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);

        switch (size)
        {
            case 0: /* Byte */
#ifdef __aarch64__
                tmp = RA_AllocARMRegister(&ptr);
                tmp_2 = RA_AllocARMRegister(&ptr);
                *ptr++ = and_immed(tmp, regx, 8, 0);
                *ptr++ = and_immed(tmp_2, regy, 8, 0);
                *ptr++ = add_reg(tmp, tmp, tmp_2, LSL, 0);
                *ptr++ = csinc(tmp, tmp, tmp, A64_CC_EQ);

                if (update_mask & SR_XVC) {
                    uint8_t tmp_3 = RA_AllocARMRegister(&ptr);

                    *ptr++ = eor_reg(tmp_3, tmp_2, tmp, LSL, 0); // D ^ R -> tmp_3
                    *ptr++ = eor_reg(tmp_2, regx, tmp, LSL, 0);  // S ^ R -> tmp_2
                    *ptr++ = and_reg(tmp_3, tmp_2, tmp_3, LSL, 0); // V = (D^R) & (S^R), bit 7
                    *ptr++ = bfxil(tmp_3, tmp, 2, 7);            // C at position 6, V at position 7
                    *ptr++ = bfxil(cc, tmp_3, 6, 2);

                    if (update_mask & SR_X) {
                        *ptr++ = bfi(cc, cc, 4, 1);
                    }

                    RA_FreeARMRegister(&ptr, tmp_3);

                    update_mask &= ~SR_XVC;
                }

                if (update_mask & SR_NZ) {
                    *ptr++ = adds_reg(31, 31, tmp, LSL, 24);
                }

                *ptr++ = bfxil(regy, tmp, 0, 8);
                RA_FreeARMRegister(&ptr, tmp);
                RA_FreeARMRegister(&ptr, tmp_2);
#else
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = lsl_immed(tmp, regx, 24);
                *ptr++ = add_cc_immed(ARM_CC_NE, tmp, tmp, 0x401);
                *ptr++ = adds_reg(tmp, tmp, regy, 24);
                *ptr++ = lsr_immed(tmp, tmp, 24);
                *ptr++ = bfi(regy, tmp, 0, 8);
                RA_FreeARMRegister(&ptr, tmp);
#endif
                break;
            case 1: /* Word */
#ifdef __aarch64__
                tmp = RA_AllocARMRegister(&ptr);
                tmp_2 = RA_AllocARMRegister(&ptr);
                *ptr++ = and_immed(tmp, regx, 16, 0);
                *ptr++ = and_immed(tmp_2, regy, 16, 0);
                *ptr++ = add_reg(tmp, tmp, tmp_2, LSL, 0);
                *ptr++ = csinc(tmp, tmp, tmp, A64_CC_EQ);

                if (update_mask & SR_XVC) {
                    uint8_t tmp_3 = RA_AllocARMRegister(&ptr);

                    *ptr++ = eor_reg(tmp_3, tmp_2, tmp, LSL, 0); // D ^ R -> tmp_3
                    *ptr++ = eor_reg(tmp_2, regx, tmp, LSL, 0);  // S ^ R -> tmp_2
                    *ptr++ = and_reg(tmp_3, tmp_2, tmp_3, LSL, 0); // V = (D^R) & (S^R), bit 15
                    *ptr++ = bfxil(tmp_3, tmp, 2, 15);            // C at position 14, V at position 15
                    *ptr++ = bfxil(cc, tmp_3, 14, 2);

                    if (update_mask & SR_X) {
                        *ptr++ = bfi(cc, cc, 4, 1);
                    }

                    RA_FreeARMRegister(&ptr, tmp_3);

                    update_mask &= ~SR_XVC;
                }

                if (update_mask & SR_NZ) {
                    *ptr++ = adds_reg(31, 31, tmp, LSL, 16);
                }

                *ptr++ = bfxil(regy, tmp, 0, 16);
                RA_FreeARMRegister(&ptr, tmp);
                RA_FreeARMRegister(&ptr, tmp_2);
#else
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = lsl_immed(tmp, regx, 16);
                *ptr++ = add_cc_immed(ARM_CC_NE, tmp, tmp, 0x801);
                *ptr++ = adds_reg(tmp, tmp, regy, 16);
                *ptr++ = lsr_immed(tmp, tmp, 16);
                *ptr++ = bfi(regy, tmp, 0, 16);
                RA_FreeARMRegister(&ptr, tmp);
#endif
                break;
            case 2: /* Long */
#ifdef __aarch64__
                *ptr++ = adcs(regy, regy, regx);
#else
                *ptr++ = add_cc_immed(ARM_CC_NE, regy, regy, 1);
                *ptr++ = adds_reg(regy, regy, regx, 0);
#endif
                break;
        }
        RA_FreeARMRegister(&ptr, tmp_cc_1);
        RA_FreeARMRegister(&ptr, tmp_cc_2);
    }
    /* memory to memory */
    else
    {
        uint8_t regx = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
        uint8_t regy = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
        uint8_t dest = RA_AllocARMRegister(&ptr);
        uint8_t src = RA_AllocARMRegister(&ptr);
        uint8_t tmp_cc_1 = RA_AllocARMRegister(&ptr);
        uint8_t tmp_cc_2 = RA_AllocARMRegister(&ptr);
        uint8_t tmp = 0xff;

        RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        RA_SetDirtyM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));

        switch (size)
        {
            case 0: /* Byte */
                *ptr++ = ldrb_offset_preindex(regx, src, (opcode & 7) == 7 ? -2 : -1);
                *ptr++ = ldrb_offset_preindex(regy, dest, ((opcode >> 9) & 7) == 7 ? -2 : -1);
#ifdef __aarch64__
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = add_reg(tmp, dest, src, LSL, 0);
                *ptr++ = csinc(tmp, tmp, tmp, A64_CC_EQ);

                if (update_mask & SR_XVC) {
                    uint8_t tmp_3 = RA_AllocARMRegister(&ptr);
                    uint8_t tmp_2 = RA_AllocARMRegister(&ptr);

                    *ptr++ = eor_reg(tmp_3, dest, tmp, LSL, 0); // D ^ R -> tmp_3
                    *ptr++ = eor_reg(tmp_2, src, tmp, LSL, 0);  // S ^ R -> tmp_2
                    *ptr++ = and_reg(tmp_3, tmp_2, tmp_3, LSL, 0); // V = (D^R) & (S^R), bit 7
                    *ptr++ = bfxil(tmp_3, tmp, 2, 7);            // C at position 6, V at position 7
                    *ptr++ = bfxil(cc, tmp_3, 6, 2);

                    if (update_mask & SR_X) {
                        *ptr++ = bfi(cc, cc, 4, 1);
                    }

                    RA_FreeARMRegister(&ptr, tmp_3);
                    RA_FreeARMRegister(&ptr, tmp_2);

                    update_mask &= ~SR_XVC;
                }

                if (update_mask & SR_NZ) {
                    *ptr++ = adds_reg(31, 31, tmp, LSL, 24);
                }

                *ptr++ = strb_offset(regy, tmp, 0);
                RA_FreeARMRegister(&ptr, tmp);
#else
                *ptr++ = lsl_immed(src, src, 24);
                *ptr++ = add_cc_immed(ARM_CC_NE, src, src, 0x401);
                *ptr++ = adds_reg(dest, src, dest, 24);
                *ptr++ = lsr_immed(dest, dest, 24);
                *ptr++ = strb_offset(regy, dest, 0);
#endif
                break;
            case 1: /* Word */
                *ptr++ = ldrh_offset_preindex(regx, src, -2);
                *ptr++ = ldrh_offset_preindex(regy, dest, -2);
#ifdef __aarch64__
                tmp = RA_AllocARMRegister(&ptr);

                *ptr++ = add_reg(tmp, src, dest, LSL, 0);
                *ptr++ = csinc(tmp, tmp, tmp, A64_CC_EQ);

                if (update_mask & SR_XVC) {
                    uint8_t tmp_3 = RA_AllocARMRegister(&ptr);
                    uint8_t tmp_2 = RA_AllocARMRegister(&ptr);

                    *ptr++ = eor_reg(tmp_3, dest, tmp, LSL, 0); // D ^ R -> tmp_3
                    *ptr++ = eor_reg(tmp_2, src, tmp, LSL, 0);  // S ^ R -> tmp_2
                    *ptr++ = and_reg(tmp_3, tmp_2, tmp_3, LSL, 0); // V = (D^R) & (S^R), bit 15
                    *ptr++ = bfxil(tmp_3, tmp, 2, 15);            // C at position 14, V at position 15
                    *ptr++ = bfxil(cc, tmp_3, 14, 2);

                    if (update_mask & SR_X) {
                        *ptr++ = bfi(cc, cc, 4, 1);
                    }

                    RA_FreeARMRegister(&ptr, tmp_3);
                    RA_FreeARMRegister(&ptr, tmp_2);

                    update_mask &= ~SR_XVC;
                }

                if (update_mask & SR_NZ) {
                    *ptr++ = adds_reg(31, 31, tmp, LSL, 16);
                }

                *ptr++ = strh_offset(regy, tmp, 0);
                RA_FreeARMRegister(&ptr, tmp);
#else
                *ptr++ = lsl_immed(src, src, 16);
                *ptr++ = add_cc_immed(ARM_CC_NE, src, src, 0x801);
                *ptr++ = adds_reg(dest, src, dest, 16);
                *ptr++ = lsr_immed(dest, dest, 16);
                *ptr++ = strh_offset(regy, dest, 0);
#endif
                break;
            case 2: /* Long */
                *ptr++ = ldr_offset_preindex(regx, src, -4);
                *ptr++ = ldr_offset_preindex(regy, dest, -4);
#ifdef __aarch64__
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = adcs(dest, dest, src);
                *ptr++ = str_offset(regy, dest, 0);
                RA_FreeARMRegister(&ptr, tmp);
#else
                *ptr++ = add_cc_immed(ARM_CC_NE, dest, dest, 1);
                *ptr++ = adds_reg(dest, dest, src, 0);
                *ptr++ = str_offset(regy, dest, 0);
#endif
                break;
        }

        RA_FreeARMRegister(&ptr, dest);
        RA_FreeARMRegister(&ptr, src);
        RA_FreeARMRegister(&ptr, tmp_cc_1);
        RA_FreeARMRegister(&ptr, tmp_cc_2);
    }

    ptr = EMIT_AdvancePC(ptr, 2);

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);

        /* No chance of getting NZVC in quick way. The Z flags is unchanged if result is zero */

        if (update_mask & SR_Z) {
#ifdef __aarch64__
            *ptr++ = b_cc(ARM_CC_EQ, 2);
            *ptr++ = bic_immed(cc, cc, 1, 30);
#endif
            update_mask &= ~SR_Z;
        }
        ptr = EMIT_ClearFlags(ptr, cc, update_mask);
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

static struct OpcodeDef InsnTable[4096] = {
    [0000 ... 0007] = { { EMIT_ADD_reg }, NULL, 0, SR_CCR, 1, 0, 1 },        //Dn Destination, Byte
    [0020 ... 0047] = { { EMIT_ADD_mem }, NULL, 0, SR_CCR, 1, 0, 1 },
    [0050 ... 0074] = { { EMIT_ADD_ext }, NULL, 0, SR_CCR, 1, 1, 1 },
    [0100 ... 0117] = { { EMIT_ADD_reg }, NULL, 0, SR_CCR, 1, 0, 2 },        //Word
    [0120 ... 0147] = { { EMIT_ADD_mem }, NULL, 0, SR_CCR, 1, 0, 2 },
    [0150 ... 0174] = { { EMIT_ADD_ext }, NULL, 0, SR_CCR, 1, 1, 2 },
    [0200 ... 0217] = { { EMIT_ADD_reg }, NULL, 0, SR_CCR, 1, 0, 4 },        //Long
    [0220 ... 0247] = { { EMIT_ADD_mem }, NULL, 0, SR_CCR, 1, 0, 4 },
    [0250 ... 0274] = { { EMIT_ADD_ext }, NULL, 0, SR_CCR, 1, 1, 4 },
    [0300 ... 0317] = { { EMIT_ADDA_reg }, NULL, 0, 0, 1, 0, 2 },            //Word
    [0320 ... 0347] = { { EMIT_ADDA_mem }, NULL, 0, 0, 1, 0, 2 },
    [0350 ... 0374] = { { EMIT_ADDA_ext }, NULL, 0, 0, 1, 1, 2 },
    [0400 ... 0407] = { { EMIT_ADDX_reg }, NULL, SR_X, SR_CCR, 1, 0, 1 },   //Byte
    [0410 ... 0417] = { { EMIT_ADDX_mem }, NULL, SR_X, SR_CCR, 1, 0, 1 }, 
    [0500 ... 0507] = { { EMIT_ADDX_reg }, NULL, SR_X, SR_CCR, 1, 0, 2 },   //Word
    [0510 ... 0517] = { { EMIT_ADDX_mem }, NULL, SR_X, SR_CCR, 1, 0, 2 },
    [0600 ... 0607] = { { EMIT_ADDX_reg }, NULL, SR_X, SR_CCR, 1, 0, 4 },   //Long
    [0610 ... 0617] = { { EMIT_ADDX_mem }, NULL, SR_X, SR_CCR, 1, 0, 4 },
    [0420 ... 0447] = { { EMIT_ADD_mem }, NULL, 0, SR_CCR, 1, 0, 1 },        //Dn Source, Byte
    [0450 ... 0471] = { { EMIT_ADD_ext }, NULL, 0, SR_CCR, 1, 1, 1 },
    [0520 ... 0547] = { { EMIT_ADD_mem }, NULL, 0, SR_CCR, 1, 0, 2 },        //Word
    [0550 ... 0571] = { { EMIT_ADD_ext }, NULL, 0, SR_CCR, 1, 1, 2 },
    [0620 ... 0647] = { { EMIT_ADD_mem }, NULL, 0, SR_CCR, 1, 0, 4 },        //Long
    [0650 ... 0671] = { { EMIT_ADD_ext }, NULL, 0, SR_CCR, 1, 1, 4 },
    [0700 ... 0717] = { { EMIT_ADDA_reg }, NULL, 0, 0, 1, 0, 4 },
    [0720 ... 0747] = { { EMIT_ADDA_mem }, NULL, 0, 0, 1, 0, 4 },
    [0750 ... 0774] = { { EMIT_ADDA_ext }, NULL, 0, 0, 1, 1, 4 },            //Long
};

uint32_t *EMIT_lineD(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    (void)InsnTable;
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;
    *insn_consumed = 1;

    if (InsnTable[opcode & 00777].od_Emit)
    {
        ptr = InsnTable[opcode & 00777].od_Emit(ptr, opcode, m68k_ptr);
    }
    else
    {
        ptr = EMIT_FlushPC(ptr);
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        *ptr++ = svc(0x103);
        *ptr++ = (uint32_t)(uintptr_t)(*m68k_ptr - 8);
        *ptr++ = 48;
        ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
    }

    return ptr;
}

uint32_t GetSR_LineD(uint16_t opcode)
{
    /* If instruction is in the table, return what flags it needs (shifted 16 bits left) and flags it sets */
    if (InsnTable[opcode & 00777].od_Emit) {
        return (InsnTable[opcode & 00777].od_SRNeeds << 16) | InsnTable[opcode & 00777].od_SRSets;
    }
    /* Instruction not found, i.e. it needs all flags and sets none (ILLEGAL INSTRUCTION exception) */
    else {
        kprintf("Undefined LineD\n");
        return SR_CCR << 16;
    }
}


int M68K_GetLineDLength(uint16_t *insn_stream)
{
    uint16_t opcode = BE16(*insn_stream);
    
    int length = 0;
    int need_ea = 0;
    int opsize = 0;

    if (InsnTable[opcode & 0777].od_Emit) {
        length = InsnTable[opcode & 0777].od_BaseLength;
        need_ea = InsnTable[opcode & 0777].od_HasEA;
        opsize = InsnTable[opcode & 0777].od_OpSize;
    }

    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize);
    }

    return length;
}