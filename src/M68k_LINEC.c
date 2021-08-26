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

uint32_t *EMIT_MULU(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr);
uint32_t *EMIT_MULS(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr);

uint32_t *EMIT_AND(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t direction = (opcode >> 8) & 1;
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
#ifdef __aarch64__
        case 4:
            *ptr++ = ands_reg(dest, dest, src, LSL, 0);
            break;
        case 2:
            *ptr++ = and_reg(src, src, dest, LSL, 0);
            *ptr++ = bfi(dest, src, 0, 16);
            break;
        case 1:
            *ptr++ = and_reg(src, src, dest, LSL, 0);
            *ptr++ = bfi(dest, src, 0, 8);
            break;
#else
        case 4:
            *ptr++ = ands_reg(dest, dest, src, 0);
            break;
        case 2:
            *ptr++ = lsl_immed(src, src, 16);
            *ptr++ = ands_reg(src, src, dest, 16);
            *ptr++ = lsr_immed(src, src, 16);
            *ptr++ = bfi(dest, src, 0, 16);
            break;
        case 1:
            *ptr++ = lsl_immed(src, src, 24);
            *ptr++ = ands_reg(src, src, dest, 24);
            *ptr++ = lsr_immed(src, src, 24);
            *ptr++ = bfi(dest, src, 0, 8);
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
            *ptr++ = ands_reg(tmp, tmp, src, LSL, 0);
#else
            *ptr++ = ands_reg(tmp, tmp, src, 0);
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
            *ptr++ = and_reg(tmp, tmp, src, LSL, 0);
#else
            *ptr++ = lsl_immed(tmp, tmp, 16);
            *ptr++ = ands_reg(tmp, tmp, src, 16);
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
            *ptr++ = and_reg(tmp, tmp, src, LSL, 0);
#else
            *ptr++ = lsl_immed(tmp, tmp, 24);
            *ptr++ = ands_reg(tmp, tmp, src, 24);
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

    return ptr;
}

uint32_t *EMIT_EXG(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t reg1 = 0xff;
    uint8_t reg2 = 0xff;
    uint8_t tmp = RA_AllocARMRegister(&ptr);

    switch ((opcode >> 3) & 0x1f)
    {
        case 0x08: /* Data registers */
            reg1 = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            reg2 = RA_MapM68kRegister(&ptr, opcode & 7);
            *ptr++ = mov_reg(tmp, reg1);
            *ptr++ = mov_reg(reg1, reg2);
            *ptr++ = mov_reg(reg2, tmp);
            RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
            RA_SetDirtyM68kRegister(&ptr, opcode & 7);
            break;

        case 0x09:
            reg1 = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
            reg2 = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
            *ptr++ = mov_reg(tmp, reg1);
            *ptr++ = mov_reg(reg1, reg2);
            *ptr++ = mov_reg(reg2, tmp);
            RA_SetDirtyM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            break;

        case 0x11:
            reg1 = RA_MapM68kRegister(&ptr, ((opcode >> 9) & 7));
            reg2 = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
            *ptr++ = mov_reg(tmp, reg1);
            *ptr++ = mov_reg(reg1, reg2);
            *ptr++ = mov_reg(reg2, tmp);
            RA_SetDirtyM68kRegister(&ptr, ((opcode >> 9) & 7));
            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            break;
    }

    ptr = EMIT_AdvancePC(ptr, 2);

    RA_FreeARMRegister(&ptr, tmp);

    return ptr;
}


uint32_t *EMIT_ABCD(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
#ifdef __aarch64__
    uint8_t tmp_a = RA_AllocARMRegister(&ptr);
    uint8_t tmp_b = RA_AllocARMRegister(&ptr);
    uint8_t cc = RA_ModifyCC(&ptr);
    uint8_t src = -1;
    uint8_t dst = -1;

    /* Memory to memory */
    if (opcode & 8)
    {
        src = RA_AllocARMRegister(&ptr);
        dst = RA_AllocARMRegister(&ptr);
        uint8_t an_src = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
        uint8_t an_dst = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));

        if ((opcode & 7) == 7) {
            *ptr++ = ldrb_offset_preindex(an_src, src, -2);
        }
        else {
            *ptr++ = ldrb_offset_preindex(an_src, src, -1);
        }

        if (((opcode >> 9) & 7) == 7) {
            *ptr++ = ldrb_offset_preindex(an_dst, dst, -2);
        }
        else {
            *ptr++ = ldrb_offset_preindex(an_dst, dst, -1);
        }
    }
    /* Register to register */
    else
    {
        src = RA_MapM68kRegister(&ptr, opcode & 7);
        dst = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
        RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
    }

    /* Lower nibble */
    *ptr++ = ubfx(tmp_a, src, 0, 4);
    *ptr++ = ubfx(tmp_b, dst, 0, 4);
    *ptr++ = add_reg(tmp_a, tmp_a, tmp_b, LSL, 0);
    *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_X));
    *ptr++ = csinc(tmp_a, tmp_a, tmp_a, A64_CC_EQ);
    *ptr++ = cmp_immed(tmp_a, 9);
    *ptr++ = b_cc(A64_CC_LS, 2);
    *ptr++ = add_immed(tmp_a, tmp_a, 6);
    *ptr++ = bfi(dst, tmp_a, 0, 4);

    /* Higher nibble */
    *ptr++ = ubfx(tmp_a, src, 4, 4);
    *ptr++ = ubfx(tmp_b, dst, 4, 4);
    *ptr++ = add_reg(tmp_a, tmp_a, tmp_b, LSL, 0);
    *ptr++ = csinc(tmp_a, tmp_a, tmp_a, A64_CC_LS);
    *ptr++ = cmp_immed(tmp_a, 9);
    *ptr++ = b_cc(A64_CC_LS, 2);
    *ptr++ = add_immed(tmp_a, tmp_a, 6);
    *ptr++ = bfi(dst, tmp_a, 4, 4);
    *ptr++ = mov_reg(tmp_a, dst);

    if (opcode & 8)
    {
        uint8_t an_dst = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
        *ptr++ = strb_offset(an_dst, dst, 0);
    }

    /* After addition, if A64_CC_HI then there was a carry */
    *ptr++ = cset(tmp_b, A64_CC_HI);
    *ptr++ = bfi(cc, tmp_b, 0, 1);
    *ptr++ = cmp_reg(31, tmp_a, LSL, 24);
    *ptr++ = b_cc(A64_CC_EQ, 2);
    *ptr++ = bic_immed(cc, cc, 1, 31 & (32 - SRB_Z));

    /* Copy C to X */
    *ptr++ = bfi(cc, cc, 4, 1);

    RA_FreeARMRegister(&ptr, tmp_a);
    RA_FreeARMRegister(&ptr, tmp_b);
    RA_FreeARMRegister(&ptr, src);
    RA_FreeARMRegister(&ptr, dst);
    ptr = EMIT_AdvancePC(ptr, 2);
#else
    ptr = EMIT_InjectDebugString(ptr, "[JIT] ABCD at %08x not implemented\n", *m68k_ptr - 1);
    ptr = EMIT_InjectPrintContext(ptr);
    *ptr++ = udf(opcode);
#endif

    return ptr;
}

static EMIT_Function JumpTable[4096] = {
    [00000 ... 00007] = EMIT_AND,  //D0 Destination
    [00020 ... 00074] = EMIT_AND,
    [00100 ... 00107] = EMIT_AND,
    [00120 ... 00174] = EMIT_AND,
    [00200 ... 00207] = EMIT_AND,
    [00220 ... 00274] = EMIT_AND,
    
    [00300 ... 00307] = EMIT_MULU, //D0 Destination
    [00320 ... 00374] = EMIT_MULU,
    
    [00400 ... 00417] = EMIT_ABCD, //D0 Destination
    [00420 ... 00471] = EMIT_AND,  //D0 Source
    
    [00500 ... 00517] = EMIT_EXG,  //R0 Source
    [00520 ... 00571] = EMIT_AND,
    
    [00610 ... 00617] = EMIT_EXG,  //D0 Source
    [00620 ... 00671] = EMIT_AND,
    
    [00700 ... 00707] = EMIT_MULS, //D0 Destination
    [00720 ... 00774] = EMIT_MULS,
    
    [01000 ... 01007] = EMIT_AND,  //D1 Destination
    [01020 ... 01074] = EMIT_AND,
    [01100 ... 01107] = EMIT_AND,
    [01120 ... 01174] = EMIT_AND,
    [01200 ... 01207] = EMIT_AND,
    [01220 ... 01274] = EMIT_AND,
    
    [01300 ... 01307] = EMIT_MULU, //D1 Destination
    [01320 ... 01374] = EMIT_MULU,
    
    [01400 ... 01417] = EMIT_ABCD, //D1 Destination
    [01420 ... 01471] = EMIT_AND,  //D1 Source
    
    [01500 ... 01517] = EMIT_EXG,  //R1 Source
    [01520 ... 01571] = EMIT_AND,
    
    [01610 ... 01617] = EMIT_EXG,  //D1 Source
    [01620 ... 01671] = EMIT_AND,
    
    [01700 ... 01707] = EMIT_MULS, //D1 Destination
    [01720 ... 01774] = EMIT_MULS,
    
    [02000 ... 02007] = EMIT_AND,  //D2 Destination
    [02020 ... 02074] = EMIT_AND,
    [02100 ... 02107] = EMIT_AND,
    [02120 ... 02174] = EMIT_AND,
    [02200 ... 02207] = EMIT_AND,
    [02220 ... 02274] = EMIT_AND,
    
    [02300 ... 02307] = EMIT_MULU, //D2 Destination
    [02320 ... 02374] = EMIT_MULU,
    
    [02400 ... 02417] = EMIT_ABCD, //D2 Destination
    [02420 ... 02471] = EMIT_AND,  //D2 Source
    
    [02500 ... 02517] = EMIT_EXG,  //R2 Source
    [02520 ... 02571] = EMIT_AND,
    
    [02610 ... 02617] = EMIT_EXG,  //D2 Source
    [02620 ... 02671] = EMIT_AND,
    
    [02700 ... 02707] = EMIT_MULS, //D2 Destination
    [02720 ... 02774] = EMIT_MULS,
    
    [03000 ... 03007] = EMIT_AND,  //D3 Destination
    [03020 ... 03074] = EMIT_AND,
    [03100 ... 03107] = EMIT_AND,
    [03120 ... 03174] = EMIT_AND,
    [03200 ... 03207] = EMIT_AND,
    [03220 ... 03274] = EMIT_AND,
    
    [03300 ... 03307] = EMIT_MULU, //D3 Destination
    [03320 ... 03374] = EMIT_MULU,
    
    [03400 ... 03417] = EMIT_ABCD, //D3 Destination
    [03420 ... 03471] = EMIT_AND,  //D3 Source
    
    [03500 ... 03517] = EMIT_EXG,  //R3 Source
    [03520 ... 03571] = EMIT_AND,
    
    [03610 ... 03617] = EMIT_EXG,  //D3 Source
    [03620 ... 03671] = EMIT_AND,
    
    [03700 ... 03707] = EMIT_MULS, //D3 Destination
    [03720 ... 03774] = EMIT_MULS,
    
    [04000 ... 04007] = EMIT_AND,  //D4 Destination
    [04020 ... 04074] = EMIT_AND,
    [04100 ... 04107] = EMIT_AND,
    [04120 ... 04174] = EMIT_AND,
    [04200 ... 04207] = EMIT_AND,
    [04220 ... 04274] = EMIT_AND,
    
    [04300 ... 04307] = EMIT_MULU, //D4 Destination
    [04320 ... 04374] = EMIT_MULU,
    
    [04400 ... 04417] = EMIT_ABCD, //D4 Destination
    [04420 ... 04471] = EMIT_AND,  //D4 Source
    
    [04500 ... 04517] = EMIT_EXG,  //R4 Source
    [04520 ... 04571] = EMIT_AND,
    
    [04610 ... 04617] = EMIT_EXG,  //D4 Source
    [04620 ... 04671] = EMIT_AND,
    
    [04700 ... 04707] = EMIT_MULS, //D4 Destination
    [04720 ... 04774] = EMIT_MULS,
    
    [05000 ... 05007] = EMIT_AND,  //D5 Destination
    [05020 ... 05074] = EMIT_AND,
    [05100 ... 05107] = EMIT_AND,
    [05120 ... 05174] = EMIT_AND,
    [05200 ... 05207] = EMIT_AND,
    [05220 ... 05274] = EMIT_AND,
    
    [05300 ... 05307] = EMIT_MULU, //D5 Destination
    [05320 ... 05374] = EMIT_MULU,
    
    [05400 ... 05417] = EMIT_ABCD, //D5 Destination
    [05420 ... 05471] = EMIT_AND,  //D5 Source
    
    [05500 ... 05517] = EMIT_EXG,  //R5 Source
    [05520 ... 05571] = EMIT_AND,
    
    [05610 ... 05617] = EMIT_EXG,  //D5 Source
    [05620 ... 05671] = EMIT_AND,
    
    [05700 ... 05707] = EMIT_MULS, //D5 Destination
    [05720 ... 05774] = EMIT_MULS,
    
    [06000 ... 06007] = EMIT_AND,  //D6 Destination
    [06020 ... 06074] = EMIT_AND,
    [06100 ... 06107] = EMIT_AND,
    [06120 ... 06174] = EMIT_AND,
    [06200 ... 06207] = EMIT_AND,
    [06220 ... 06274] = EMIT_AND,
    
    [06300 ... 06307] = EMIT_MULU, //D6 Destination
    [06320 ... 06374] = EMIT_MULU,
    
    [06400 ... 06417] = EMIT_ABCD, //D6 Destination
    [06420 ... 06471] = EMIT_AND,  //D6 Source
    
    [06500 ... 06517] = EMIT_EXG,  //R6 Source
    [06520 ... 06571] = EMIT_AND,
    
    [06610 ... 06617] = EMIT_EXG,  //D6 Source
    [06620 ... 06671] = EMIT_AND,
    
    [06700 ... 06707] = EMIT_MULS, //D6 Destination
    [06720 ... 06774] = EMIT_MULS,
    
    [07000 ... 07007] = EMIT_AND,  //D7 Destination
    [07020 ... 07074] = EMIT_AND,
    [07100 ... 07107] = EMIT_AND,
    [07120 ... 07174] = EMIT_AND,
    [07200 ... 07207] = EMIT_AND,
    [07220 ... 07274] = EMIT_AND,
    
    [07300 ... 07307] = EMIT_MULU, //D7 Destination
    [07320 ... 07374] = EMIT_MULU,
    
    [07400 ... 07417] = EMIT_ABCD, //D7 Destination
    [07420 ... 07471] = EMIT_AND,  //D7 Source
    
    [07500 ... 07517] = EMIT_EXG,  //R7 Source
    [07520 ... 07571] = EMIT_AND,
    
    [07610 ... 07617] = EMIT_EXG,  //D7 Source
    [07620 ... 07671] = EMIT_AND,
    
    [07700 ... 07707] = EMIT_MULS, //D7 Destination
    [07720 ... 07774] = EMIT_MULS,
};


uint32_t *EMIT_lineC(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;
    *insn_consumed = 1;

    /* 1100xxx011xxxxxx - MULU */
    if (JumpTable[opcode & 0xfff])
    {
        ptr = JumpTable[opcode & 0xfff](ptr, opcode, m68k_ptr);
    }
    else
    {
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
    }

    return ptr;
}
