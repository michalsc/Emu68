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

uint32_t *EMIT_line8(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr);
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;
    *insn_consumed = 1;

    /* 1000xxx011xxxxxx - DIVU */
    if ((opcode & 0xf1c0) == 0x80c0)
    {
        ptr = EMIT_MUL_DIV(ptr, opcode, m68k_ptr);
    }
    /* 1000xxx10000xxxx - SBCD */
    else if ((opcode & 0xf1f0) == 0x8100)
    {
        ptr = EMIT_InjectDebugString(ptr, "[JIT] SBCD at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
    }
    /* 1000xxx10100xxxx - PACK */
    else if ((opcode & 0xf1f0) == 0x8140)
    {
#ifdef __aarch64__
        uint16_t addend = BE16((*m68k_ptr)[0]);
        uint8_t tmp = -1;

        if (opcode & 8)
        {
            uint8_t an_src = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
            tmp = RA_AllocARMRegister(&ptr);
            
            *ptr++ = ldrsh_offset_preindex(an_src, tmp, -2);

            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }
        else
        {
            tmp = RA_CopyFromM68kRegister(&ptr, opcode & 7);
        }

        if (addend & 0xfff) {
            *ptr++ = add_immed(tmp, tmp, addend & 0xfff);
        }
        if (addend & 0xf000) {
            *ptr++ = add_immed_lsl12(tmp, tmp, addend >> 12);
        }

        *ptr++ = bfi(tmp, tmp, 4, 4);

        if (opcode & 8)
        {
            uint8_t dst = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
            RA_SetDirtyM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));

            *ptr++ = lsr(tmp, tmp, 4);
            if (((opcode >> 9) & 7) == 7) {
                *ptr++ = strb_offset_preindex(dst, tmp, -2);
            }
            else {
                *ptr++ = strb_offset_preindex(dst, tmp, -1);
            }
        }
        else
        {
            uint8_t dst = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
            *ptr++ = bfxil(dst, tmp, 4, 8);
        }

        (*m68k_ptr)++;
        ptr = EMIT_AdvancePC(ptr, 4);

        RA_FreeARMRegister(&ptr, tmp);
#else
        ptr = EMIT_InjectDebugString(ptr, "[JIT] PACK at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
#endif
    }
    /* 1000xxx11000xxxx - UNPK */
    else if ((opcode & 0xf1f0) == 0x8180)
    {
#ifdef __aarch64__
        uint16_t addend = BE16((*m68k_ptr)[0]);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mask = RA_AllocARMRegister(&ptr);
        uint8_t src = -1;

        *ptr++ = mov_immed_u16(mask, 0x0f0f, 0);

        if (opcode & 8)
        {
            uint8_t an_src = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
            src = RA_AllocARMRegister(&ptr);

            if ((opcode & 7) == 7) {
                *ptr++ = ldrsb_offset_preindex(an_src, src, -2);
            }
            else {
                *ptr++ = ldrsb_offset_preindex(an_src, src, -1);
            }

            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        }
        else
        {
            src = RA_MapM68kRegister(&ptr, opcode & 7);
        }

        *ptr++ = orr_reg(tmp, src, src, LSL, 4);
        *ptr++ = and_reg(tmp, tmp, mask, LSL, 0);

        if (addend & 0xfff) {
            *ptr++ = add_immed(tmp, tmp, addend & 0xfff);
        }
        if (addend & 0xf000) {
            *ptr++ = add_immed_lsl12(tmp, tmp, addend >> 12);
        }

        if (opcode & 8)
        {
            uint8_t dst = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
            RA_SetDirtyM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
            *ptr++ = strh_offset_preindex(dst, tmp, -2);
        }
        else
        {
            uint8_t dst = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
            *ptr++ = bfi(dst, tmp, 0, 16);
        }

        (*m68k_ptr)++;
        ptr = EMIT_AdvancePC(ptr, 4);

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, mask);
        RA_FreeARMRegister(&ptr, src);
#else
        ptr = EMIT_InjectDebugString(ptr, "[JIT] UNPK at %08x not implemented\n", *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
#endif
    }
    /* 1000xxx111xxxxxx - DIVS */
    else if ((opcode & 0xf1c0) == 0x81c0)
    {
        ptr = EMIT_MUL_DIV(ptr, opcode, m68k_ptr);
    }
    /* 1000xxxxxxxxxxxx - OR */
    else if ((opcode & 0xf000) == 0x8000)
    {
        uint8_t size = 1 << ((opcode >> 6) & 3);
        uint8_t direction = (opcode >> 8) & 1; // 0: Ea+Dn->Dn, 1: Ea+Dn->Ea
        uint8_t ext_words = 0;
        uint8_t test_register = 0xff;

        if (direction == 0)
        {
            uint8_t dest = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            uint8_t src = 0xff;

            test_register = dest;

            RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
            if (size == 4)
                ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
            else
                ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

            switch (size)
            {
            case 4:
#ifdef __aarch64__
                *ptr++ = orr_reg(dest, dest, src, LSL, 0);
#else
                *ptr++ = orrs_reg(dest, dest, src, 0);
#endif
                break;
            case 2:
#ifdef __aarch64__
                *ptr++ = orr_reg(src, src, dest, LSL, 0);
#else
                *ptr++ = lsl_immed(src, src, 16);
                *ptr++ = orrs_reg(src, src, dest, 16);
                *ptr++ = lsr_immed(src, src, 16);
#endif
                *ptr++ = bfi(dest, src, 0, 16);
                break;
            case 1:
#ifdef __aarch64__
                *ptr++ = orr_reg(src, src, dest, LSL, 0);
#else
                *ptr++ = lsl_immed(src, src, 24);
                *ptr++ = orrs_reg(src, src, dest, 24);
                *ptr++ = lsr_immed(src, src, 24);
#endif
                *ptr++ = bfi(dest, src, 0, 8);
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
                *ptr++ = orr_reg(tmp, tmp, src, LSL, 0);
#else
                *ptr++ = orrs_reg(tmp, tmp, src, 0);
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
                *ptr++ = orr_reg(tmp, tmp, src, LSL, 0);
#else
                *ptr++ = lsl_immed(tmp, tmp, 16);
                *ptr++ = orrs_reg(tmp, tmp, src, 16);
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
                *ptr++ = orr_reg(tmp, tmp, src, LSL, 0);
#else
                *ptr++ = lsl_immed(tmp, tmp, 24);
                *ptr++ = orrs_reg(tmp, tmp, src, 24);
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
    {
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
    }

    return ptr;
}
/*
    static EMIT_Function JumpTable[4096] = {
        [00000 ... 00007] = EMIT_OR,    //D0 Destination
        [00020 ... 00074] = EMIT_OR,
        [00100 ... 00107] = EMIT_OR,
        [00120 ... 00174] = EMIT_OR,
        [00200 ... 00207] = EMIT_OR,
        [00220 ... 00274] = EMIT_OR,
        
        [00300 ... 00307] = EMIT_DIVU,  //D0 Destination, DIVU.W
        [00320 ... 00374] = EMIT_DIVU,
        
        [00400 ... 00417] = EMIT_SBCD,  //R0 Destination
        [00420 ... 00474] = EMIT_OR,    //D0 Source
        
        [00500 ... 00517] = EMIT_PACK,  //R0 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
        [00520 ... 00574] = EMIT_OR,
        
        [00600 ... 00617] = EMIT_UNPK,  //R0 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
        [00620 ... 00674] = EMIT_OR,
        
        [00700 ... 00707] = EMIT_DIVS,  //D0 Destination, DIVS.W
        [00720 ... 00774] = EMIT_DIVS,
        
        [01000 ... 01007] = EMIT_OR,    //D1 Destination
        [01020 ... 01074] = EMIT_OR,
        [01100 ... 01107] = EMIT_OR,
        [01120 ... 01174] = EMIT_OR,
        [01200 ... 01207] = EMIT_OR,
        [01220 ... 01274] = EMIT_OR,
        
        [01300 ... 01307] = EMIT_DIVU,  //D1 Destination, DIVU.W
        [01320 ... 01374] = EMIT_DIVU,
        
        [01400 ... 01417] = EMIT_SBCD,  //R1 Destination
        [01420 ... 01474] = EMIT_OR,    //D1 Source
        
        [01500 ... 01517] = EMIT_PACK,  //R1 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
        [01520 ... 01574] = EMIT_OR,
        
        [01600 ... 01617] = EMIT_UNPK,  //R1 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
        [01620 ... 01674] = EMIT_OR,
        
        [01700 ... 01707] = EMIT_DIVS,  //D1 Destination, DIVS.W
        [01720 ... 01774] = EMIT_DIVS,
        
        [02000 ... 02007] = EMIT_OR,    //D2 Destination
        [02020 ... 02074] = EMIT_OR,
        [02100 ... 02107] = EMIT_OR,
        [02120 ... 02174] = EMIT_OR,
        [02200 ... 02207] = EMIT_OR,
        [02220 ... 02274] = EMIT_OR,
        
        [02300 ... 02307] = EMIT_DIVU,  //D2 Destination, DIVU.W
        [02320 ... 02374] = EMIT_DIVU,
        
        [02400 ... 02417] = EMIT_SBCD,  //R2 Destination
        [02420 ... 02474] = EMIT_OR,    //D2 Source
        
        [02500 ... 02517] = EMIT_PACK,  //R2 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
        [02520 ... 02574] = EMIT_OR,
        
        [02600 ... 02617] = EMIT_UNPK,  //R2 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
        [02620 ... 02674] = EMIT_OR,
        
        [02700 ... 02707] = EMIT_DIVS,  //D2 Destination, DIVS.W
        [02720 ... 02774] = EMIT_DIVS,
        
        [03000 ... 03007] = EMIT_OR,    //D3 Destination
        [03020 ... 03074] = EMIT_OR,
        [03100 ... 03107] = EMIT_OR,
        [03120 ... 03174] = EMIT_OR,
        [03200 ... 03207] = EMIT_OR,
        [03220 ... 03274] = EMIT_OR,
        
        [03300 ... 03307] = EMIT_DIVU,  //D3 Destination, DIVU.W
        [03320 ... 03374] = EMIT_DIVU,
        
        [03400 ... 03417] = EMIT_SBCD,  //R3 Destination
        [03420 ... 03474] = EMIT_OR,    //D3 Source
        
        [03500 ... 03517] = EMIT_PACK,  //R3 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
        [03520 ... 03574] = EMIT_OR,
        
        [03600 ... 03617] = EMIT_UNPK,  //R3 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
        [03620 ... 03674] = EMIT_OR,
        
        [03700 ... 03707] = EMIT_DIVS,  //D3 Destination, DIVS.W
        [03720 ... 03774] = EMIT_DIVS,
        
        [04000 ... 04007] = EMIT_OR,    //D4 Destination
        [04020 ... 04074] = EMIT_OR,
        [04100 ... 04107] = EMIT_OR,
        [04120 ... 04174] = EMIT_OR,
        [04200 ... 04207] = EMIT_OR,
        [04220 ... 04274] = EMIT_OR,
        
        [04300 ... 04307] = EMIT_DIVU,  //D4 Destination, DIVU.W
        [04320 ... 04374] = EMIT_DIVU,
        
        [04400 ... 04417] = EMIT_SBCD,  //R4 Destination
        [04420 ... 04474] = EMIT_OR,    //D4 Source
        
        [04500 ... 04517] = EMIT_PACK,  //R4 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
        [04520 ... 04574] = EMIT_OR,
        
        [04600 ... 04617] = EMIT_UNPK,  //R4 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
        [04620 ... 04674] = EMIT_OR,
        
        [04700 ... 04707] = EMIT_DIVS,  //D4 Destination, DIVS.W
        [04720 ... 04774] = EMIT_DIVS,
        
        [05000 ... 05007] = EMIT_OR,    //D5 Destination
        [05020 ... 05074] = EMIT_OR,
        [05100 ... 05107] = EMIT_OR,
        [05120 ... 05174] = EMIT_OR,
        [05200 ... 05207] = EMIT_OR,
        [05220 ... 05274] = EMIT_OR,
        
        [05300 ... 05307] = EMIT_DIVU,  //D5 Destination, DIVU.W
        [05320 ... 05374] = EMIT_DIVU,
        
        [05400 ... 05417] = EMIT_SBCD,  //R5 Destination
        [05420 ... 05474] = EMIT_OR,    //D5 Source
        
        [05500 ... 05517] = EMIT_PACK,  //R5 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
        [05520 ... 05574] = EMIT_OR,
        
        [05600 ... 05617] = EMIT_UNPK,  //R5 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
        [05620 ... 05674] = EMIT_OR,
        
        [05700 ... 05707] = EMIT_DIVS,  //D5 Destination, DIVS.W
        [05720 ... 05774] = EMIT_DIVS,
        
        [06000 ... 06007] = EMIT_OR,    //D6 Destination
        [06020 ... 06074] = EMIT_OR,
        [06100 ... 06107] = EMIT_OR,
        [06120 ... 06174] = EMIT_OR,
        [06200 ... 06207] = EMIT_OR,
        [06220 ... 06274] = EMIT_OR,
        
        [06300 ... 06307] = EMIT_DIVU,  //D6 Destination, DIVU.W
        [06320 ... 06374] = EMIT_DIVU,
        
        [06400 ... 06417] = EMIT_SBCD,  //R6 Destination
        [06420 ... 06474] = EMIT_OR,    //D6 Source
        
        [06500 ... 06517] = EMIT_PACK,  //R6 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
        [06520 ... 06574] = EMIT_OR,
        
        [06600 ... 06617] = EMIT_UNPK,  //R6 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
        [06620 ... 06674] = EMIT_OR,
        
        [06700 ... 06707] = EMIT_DIVS,  //D6 Destination, DIVS.W
        [06720 ... 06774] = EMIT_DIVS,
        
        [07000 ... 07007] = EMIT_OR,    //D7 Destination
        [07020 ... 07074] = EMIT_OR,
        [07100 ... 07107] = EMIT_OR,
        [07120 ... 07174] = EMIT_OR,
        [07200 ... 07207] = EMIT_OR,
        [07220 ... 07274] = EMIT_OR,
        
        [07300 ... 07307] = EMIT_DIVU,  //D7 Destination, DIVU.W
        [07320 ... 07374] = EMIT_DIVU,
        
        [07400 ... 07417] = EMIT_SBCD,  //R7 Destination
        [07420 ... 07474] = EMIT_OR,    //D7 Source
        
        [07500 ... 07517] = EMIT_PACK,  //R7 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
        [07520 ... 07574] = EMIT_OR,
        
        [07600 ... 07617] = EMIT_UNPK,  //R7 Destination, 020 and UP only, fetches another Word.(16-bit adjustment)
        [07620 ... 07674] = EMIT_OR,
        
        [07700 ... 07707] = EMIT_DIVS,  //D7 Destination, DIVS.W
        [07720 ... 07774] = EMIT_DIVS,
    }
*/
