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

uint32_t *EMIT_lineD(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;
    *insn_consumed = 1;

    /* ADDA */
    if ((opcode & 0xf0c0) == 0xd0c0)
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
        if (size == 2)
            *ptr++ = sxth(tmp, tmp, 0);

        *ptr++ = add_reg(reg, reg, tmp, 0);
#endif
        RA_SetDirtyM68kRegister(&ptr, ((opcode >> 9) & 7) + 8);

        RA_FreeARMRegister(&ptr, tmp);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
        (*m68k_ptr) += ext_words;
    }
    /* ADDX */
    else if ((opcode & 0xf130) == 0xd100)
    {
        uint8_t size = (opcode >> 6) & 3;
        uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
#ifdef __aarch64__
        uint8_t cc = RA_GetCC(&ptr);
        if (size == 2) {
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            *ptr++ = ror(tmp, cc, 3);
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

            RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);

            switch (size)
            {
                case 0: /* Byte */
#ifdef __aarch64__
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = cset(tmp, A64_CC_NE);
                    *ptr++ = lsl(tmp, tmp, 24);
                    *ptr++ = add_reg(tmp, tmp, regy, LSL, 24);
                    *ptr++ = adds_reg(tmp, tmp, regx, LSL, 24);
                    *ptr++ = bfxil(regy, tmp, 24, 8);
                    RA_FreeARMRegister(&ptr, tmp);
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
                    *ptr++ = cset(tmp, A64_CC_NE);
                    *ptr++ = lsl(tmp, tmp, 16);
                    *ptr++ = add_reg(tmp, tmp, regy, LSL, 16);
                    *ptr++ = adds_reg(tmp, tmp, regx, LSL, 16);
                    *ptr++ = bfxil(regy, tmp, 16, 16);
                    RA_FreeARMRegister(&ptr, tmp);
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
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = adcs(regy, regy, regx);
                    RA_FreeARMRegister(&ptr, tmp);
#else
                    *ptr++ = add_cc_immed(ARM_CC_NE, regy, regy, 1);
                    *ptr++ = adds_reg(regy, regy, regx, 0);
#endif
                    break;
            }
        }
        /* memory to memory */
        else
        {
            uint8_t regx = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
            uint8_t regy = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
            uint8_t dest = RA_AllocARMRegister(&ptr);
            uint8_t src = RA_AllocARMRegister(&ptr);

            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            RA_SetDirtyM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));

            switch (size)
            {
                case 0: /* Byte */
                    *ptr++ = ldrb_offset_preindex(regx, src, (opcode & 7) == 7 ? -2 : -1);
                    *ptr++ = ldrb_offset_preindex(regy, dest, ((opcode >> 9) & 7) == 7 ? -2 : -1);
#ifdef __aarch64__
                    uint8_t tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = cset(tmp, A64_CC_NE);
                    *ptr++ = lsl(tmp, tmp, 24);
                    *ptr++ = add_reg(dest, tmp, dest, LSL, 24);
                    *ptr++ = adds_reg(dest, dest, src, LSL, 24);
                    *ptr++ = lsr(dest, dest, 24);
                    *ptr++ = strb_offset(regy, dest, 0);
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
                    *ptr++ = cset(tmp, A64_CC_NE);
                    *ptr++ = lsl(tmp, tmp, 16);
                    *ptr++ = add_reg(dest, tmp, dest, LSL, 16);
                    *ptr++ = adds_reg(dest, dest, src, LSL, 16);
                    *ptr++ = lsr(dest, dest, 16);
                    *ptr++ = strh_offset(regy, dest, 0);
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
        }

        ptr = EMIT_AdvancePC(ptr, 2);

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
    }
    /* ADD */
    else if ((opcode & 0xf000) == 0xd000)
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
    }
    else
    {
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
    }

    return ptr;
}
/*
    static EMIT_Function JumpTable[4096] = {
        [00000 ... 00007] = EMIT_ADD,  //D0 Destination, Byte
        [00020 ... 00074] = EMIT_ADD,
        [00100 ... 00107] = EMIT_ADD,  //Word
        [00120 ... 00174] = EMIT_ADD,
        [00200 ... 00207] = EMIT_ADD,  //Long
        [00220 ... 00274] = EMIT_ADD,
        [00300 ... 00374] = EMIT_ADDA, //Word
        [00400 ... 00417] = EMIT_ADDX, //R0
        [00500 ... 00517] = EMIT_ADDX, //Word
        [00600 ... 00617] = EMIT_ADDX, //Long
        [00420 ... 00471] = EMIT_ADD,  //D0 Source
        [00520 ... 00571] = EMIT_ADD,  //Word
        [00620 ... 00671] = EMIT_ADD,  //Long
        [00700 ... 00774] = EMIT_ADDA, //Long

        [01000 ... 01007] = EMIT_ADD,  //D1 Destination, Byte
        [01020 ... 01074] = EMIT_ADD,
        [01100 ... 01107] = EMIT_ADD,  //Word
        [01120 ... 01174] = EMIT_ADD,
        [01200 ... 01207] = EMIT_ADD,  //Long
        [01220 ... 01274] = EMIT_ADD,
        [01300 ... 01374] = EMIT_ADDA, //Word
        [01400 ... 01417] = EMIT_ADDX, //R1, Byte
        [01500 ... 01517] = EMIT_ADDX, //Word
        [01600 ... 01617] = EMIT_ADDX, //Long
        [01420 ... 01471] = EMIT_ADD,  //D1 Source, Byte
        [01520 ... 01571] = EMIT_ADD,  //Word
        [01620 ... 01671] = EMIT_ADD,  //Long
        [01700 ... 01774] = EMIT_ADDA, //Long
        
        [02000 ... 02007] = EMIT_ADD,  //D2 Destination
        [02020 ... 02074] = EMIT_ADD,
        [02100 ... 02107] = EMIT_ADD,
        [02120 ... 02174] = EMIT_ADD,
        [02200 ... 02207] = EMIT_ADD,
        [02220 ... 02274] = EMIT_ADD,
        [02300 ... 02374] = EMIT_ADDA, //Word
        [02400 ... 02417] = EMIT_ADDX, //R2
        [02500 ... 02517] = EMIT_ADDX,
        [02600 ... 02617] = EMIT_ADDX,
        [02420 ... 02471] = EMIT_ADD,  //D2 Source
        [02520 ... 02571] = EMIT_ADD,
        [02620 ... 02671] = EMIT_ADD,
        [02700 ... 02774] = EMIT_ADDA, //Long

        [03000 ... 03007] = EMIT_ADD,  //D3 Destination
        [03020 ... 03074] = EMIT_ADD,
        [03100 ... 03107] = EMIT_ADD,
        [03120 ... 03174] = EMIT_ADD,
        [03200 ... 03207] = EMIT_ADD,
        [03220 ... 03274] = EMIT_ADD,
        [03300 ... 03374] = EMIT_ADDA, //Word
        [03400 ... 03417] = EMIT_ADDX, //R3
        [03500 ... 03517] = EMIT_ADDX,
        [03600 ... 03617] = EMIT_ADDX,
        [03420 ... 03471] = EMIT_ADD,  //D3 Source
        [03520 ... 03571] = EMIT_ADD,
        [03620 ... 03671] = EMIT_ADD,
        [03700 ... 03774] = EMIT_ADDA, //Long

        [04000 ... 04007] = EMIT_ADD,  //D4 Destination
        [04020 ... 04074] = EMIT_ADD,
        [04100 ... 04107] = EMIT_ADD,
        [04120 ... 04174] = EMIT_ADD,
        [04200 ... 04207] = EMIT_ADD,
        [04220 ... 04274] = EMIT_ADD,
        [04300 ... 04374] = EMIT_ADDA, //Word
        [04400 ... 04417] = EMIT_ADDX, //R4
        [04500 ... 04517] = EMIT_ADDX,
        [04600 ... 04617] = EMIT_ADDX,
        [04420 ... 04471] = EMIT_ADD,  //D4 Source
        [04520 ... 04571] = EMIT_ADD,
        [04620 ... 04671] = EMIT_ADD,
        [04700 ... 04774] = EMIT_ADDA, //Long
        
        [05000 ... 05007] = EMIT_ADD,  //D5 Destination
        [05020 ... 05074] = EMIT_ADD,
        [05100 ... 05107] = EMIT_ADD,
        [05120 ... 05174] = EMIT_ADD,
        [05200 ... 05207] = EMIT_ADD,
        [05220 ... 05274] = EMIT_ADD,
        [05300 ... 05374] = EMIT_ADDA, //Word
        [05400 ... 05417] = EMIT_ADDX, //R5
        [05500 ... 05517] = EMIT_ADDX,
        [05600 ... 05617] = EMIT_ADDX,
        [05420 ... 05471] = EMIT_ADD,  //D5 Source
        [05520 ... 05571] = EMIT_ADD,
        [05620 ... 05671] = EMIT_ADD,
        [05700 ... 05774] = EMIT_ADDA, //Long

        [06000 ... 06007] = EMIT_ADD,  //D6 Destination
        [06020 ... 06074] = EMIT_ADD,
        [06100 ... 06107] = EMIT_ADD,
        [06120 ... 06174] = EMIT_ADD,
        [06200 ... 06207] = EMIT_ADD,
        [06220 ... 06274] = EMIT_ADD,
        [06300 ... 06374] = EMIT_ADDA, //Word
        [06400 ... 06417] = EMIT_ADDX, //R6
        [06500 ... 06517] = EMIT_ADDX,
        [06600 ... 06617] = EMIT_ADDX,
        [06420 ... 06471] = EMIT_ADD,  //D6 Source
        [06520 ... 06571] = EMIT_ADD,
        [06620 ... 06671] = EMIT_ADD,
        [06700 ... 06774] = EMIT_ADDA, //Long

        [07000 ... 07007] = EMIT_ADD,  //D7 Destination
        [07020 ... 07074] = EMIT_ADD,
        [07100 ... 07107] = EMIT_ADD,
        [07120 ... 07174] = EMIT_ADD,
        [07200 ... 07207] = EMIT_ADD,
        [07220 ... 07274] = EMIT_ADD,
        [07300 ... 07374] = EMIT_ADDA, //Word
        [07400 ... 07417] = EMIT_ADDX, //R7
        [07500 ... 07517] = EMIT_ADDX,
        [07600 ... 07617] = EMIT_ADDX,
        [07420 ... 07471] = EMIT_ADD,  //D7 Source
        [07520 ... 07571] = EMIT_ADD,
        [07620 ... 07671] = EMIT_ADD,
        [07700 ... 07774] = EMIT_ADDA, //Long
    }
*/
