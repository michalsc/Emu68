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

uint32_t *EMIT_lineB(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr);
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;
    *insn_consumed = 1;

    /* 1011xxxx11xxxxxx - CMPA */
    if ((opcode & 0xf0c0) == 0xb0c0)
    {
        uint8_t size = ((opcode >> 8) & 1) ? 4 : 2;
        uint8_t src = 0xff;
        uint8_t dst = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
        uint8_t ext_words = 0;

        if (size == 4)
            ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
        else
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
    }
    /* 1011xxx1xx001xxx - CMPM */
    else if ((opcode & 0xf138) == 0xb108)
    {
        uint8_t size = 1 << ((opcode >> 6) & 3);
        uint8_t src = 0xff;
        uint8_t dst = 0xff;
        uint8_t ext_words = 0;
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, 0x18 | (opcode & 7), *m68k_ptr, &ext_words, 1, NULL);
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &dst, 0x18 | ((opcode >> 9) & 7), *m68k_ptr, &ext_words, 1, NULL);

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
    }
    /* 1011xxx0xxxxxxxx - CMP */
    else if ((opcode & 0xf100) == 0xb000)
    {
        uint8_t size = 1 << ((opcode >> 6) & 3);
        uint8_t src = 0xff;
        uint8_t dst = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
        uint8_t ext_words = 0;
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

        switch(size)
        {
#ifdef __aarch64__
            case 4:
                *ptr++ = subs_reg(31, dst, src, LSL, 0);
                break;
            case 2:
                *ptr++ = lsl(tmp, dst, 16);
                *ptr++ = subs_reg(31, tmp, src, LSL, 16);
                break;
            case 1:
                *ptr++ = lsl(tmp, dst, 24);
                *ptr++ = subs_reg(31, tmp, src, LSL, 24);
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
    [00000 ... 00007] = EMIT_CMP, //D0
    [00020 ... 00074] = EMIT_CMP,
    [00100 ... 00174] = EMIT_CMP,
    [00100 ... 00174] = EMIT_CMP,
    [00200 ... 00274] = EMIT_CMP,
    
    [00300 ... 00374] = EMIT_CMPA,
    
    [00400 ... 00407] = EMIT_EOR,
    [00410 ... 00417] = EMIT_CMPM,
    [00420 ... 00471] = EMIT_EOR,
    [00500 ... 00507] = EMIT_EOR,
    [00510 ... 00517] = EMIT_CMPM,
    [00520 ... 00571] = EMIT_EOR,
    [00600 ... 00607] = EMIT_EOR,
    [00610 ... 00617] = EMIT_CMPM,
    [00620 ... 00671] = EMIT_EOR,
     
    [00700 ... 00774] = EMIT_CMPA,
    
    [01000 ... 01007] = EMIT_CMP, //D1
    [01020 ... 01074] = EMIT_CMP,
    [01100 ... 01174] = EMIT_CMP,
    [01100 ... 01174] = EMIT_CMP,
    [01200 ... 01274] = EMIT_CMP,
    
    [01300 ... 01374] = EMIT_CMPA,
    
    [01400 ... 01407] = EMIT_EOR,
    [01410 ... 01417] = EMIT_CMPM,
    [01420 ... 01471] = EMIT_EOR,
    [01500 ... 01507] = EMIT_EOR,
    [01510 ... 01517] = EMIT_CMPM,
    [01520 ... 01571] = EMIT_EOR,
    [01600 ... 01607] = EMIT_EOR,
    [01610 ... 01617] = EMIT_CMPM,
    [01620 ... 01671] = EMIT_EOR,
     
    [01700 ... 01774] = EMIT_CMPA,
    
    [02000 ... 02007] = EMIT_CMP, //D2
    [02020 ... 02074] = EMIT_CMP,
    [02100 ... 02174] = EMIT_CMP,
    [02100 ... 02174] = EMIT_CMP,
    [02200 ... 02274] = EMIT_CMP,
    
    [02300 ... 02374] = EMIT_CMPA,
    
    [02400 ... 02407] = EMIT_EOR,
    [02410 ... 02417] = EMIT_CMPM,
    [02420 ... 02471] = EMIT_EOR,
    [02500 ... 02507] = EMIT_EOR,
    [02510 ... 02517] = EMIT_CMPM,
    [02520 ... 02571] = EMIT_EOR,
    [02600 ... 02607] = EMIT_EOR,
    [02610 ... 02617] = EMIT_CMPM,
    [02620 ... 02671] = EMIT_EOR,
     
    [02700 ... 02774] = EMIT_CMPA,
    
    [03000 ... 03007] = EMIT_CMP, //D3
    [03020 ... 03074] = EMIT_CMP,
    [03100 ... 03174] = EMIT_CMP,
    [03100 ... 03174] = EMIT_CMP,
    [03200 ... 03274] = EMIT_CMP,
    
    [03300 ... 03374] = EMIT_CMPA,
    
    [03400 ... 03407] = EMIT_EOR,
    [03410 ... 03417] = EMIT_CMPM,
    [03420 ... 03471] = EMIT_EOR,
    [03500 ... 03507] = EMIT_EOR,
    [03510 ... 03517] = EMIT_CMPM,
    [03520 ... 03571] = EMIT_EOR,
    [03600 ... 03607] = EMIT_EOR,
    [03610 ... 03617] = EMIT_CMPM,
    [03620 ... 03671] = EMIT_EOR,
     
    [03700 ... 03774] = EMIT_CMPA,
    
    [04000 ... 04007] = EMIT_CMP, //D4
    [04020 ... 04074] = EMIT_CMP,
    [04100 ... 04174] = EMIT_CMP,
    [04100 ... 04174] = EMIT_CMP,
    [04200 ... 04274] = EMIT_CMP,
    
    [04300 ... 04374] = EMIT_CMPA,
    
    [04400 ... 04407] = EMIT_EOR,
    [04410 ... 04417] = EMIT_CMPM,
    [04420 ... 04471] = EMIT_EOR,
    [04500 ... 04507] = EMIT_EOR,
    [04510 ... 04517] = EMIT_CMPM,
    [04520 ... 04571] = EMIT_EOR,
    [04600 ... 04607] = EMIT_EOR,
    [04610 ... 04617] = EMIT_CMPM,
    [04620 ... 04671] = EMIT_EOR,
     
    [04700 ... 04774] = EMIT_CMPA,
    
    [05000 ... 05007] = EMIT_CMP, //D5
    [05020 ... 05074] = EMIT_CMP,
    [05100 ... 05174] = EMIT_CMP,
    [05100 ... 05174] = EMIT_CMP,
    [05200 ... 05274] = EMIT_CMP,
    
    [05300 ... 05374] = EMIT_CMPA,
    
    [05400 ... 05407] = EMIT_EOR,
    [05410 ... 05417] = EMIT_CMPM,
    [05420 ... 05471] = EMIT_EOR,
    [05500 ... 05507] = EMIT_EOR,
    [05510 ... 05517] = EMIT_CMPM,
    [05520 ... 05571] = EMIT_EOR,
    [05600 ... 05607] = EMIT_EOR,
    [05610 ... 05617] = EMIT_CMPM,
    [05620 ... 05671] = EMIT_EOR,
     
    [05700 ... 05774] = EMIT_CMPA,
    
    [06000 ... 06007] = EMIT_CMP, //D6
    [06020 ... 06074] = EMIT_CMP,
    [06100 ... 06174] = EMIT_CMP,
    [06100 ... 06174] = EMIT_CMP,
    [06200 ... 06274] = EMIT_CMP,
    
    [06300 ... 06374] = EMIT_CMPA,
    
    [06400 ... 06407] = EMIT_EOR,
    [06410 ... 06417] = EMIT_CMPM,
    [06420 ... 06471] = EMIT_EOR,
    [06500 ... 06507] = EMIT_EOR,
    [06510 ... 06517] = EMIT_CMPM,
    [06520 ... 06571] = EMIT_EOR,
    [06600 ... 06607] = EMIT_EOR,
    [06610 ... 06617] = EMIT_CMPM,
    [06620 ... 06671] = EMIT_EOR,
     
    [00700 ... 06774] = EMIT_CMPA,
    
    [07000 ... 07007] = EMIT_CMP, //D7
    [07020 ... 07074] = EMIT_CMP,
    [07100 ... 07174] = EMIT_CMP,
    [07100 ... 07174] = EMIT_CMP,
    [07200 ... 07274] = EMIT_CMP,
    
    [07300 ... 07374] = EMIT_CMPA,
    
    [07400 ... 07407] = EMIT_EOR,
    [07410 ... 07417] = EMIT_CMPM,
    [07420 ... 07471] = EMIT_EOR,
    [07500 ... 07507] = EMIT_EOR,
    [07510 ... 07517] = EMIT_CMPM,
    [07520 ... 07571] = EMIT_EOR,
    [07600 ... 07607] = EMIT_EOR,
    [07610 ... 07617] = EMIT_CMPM,
    [07620 ... 07671] = EMIT_EOR,
     
    [07700 ... 07774] = EMIT_CMPA,
  }
*/
