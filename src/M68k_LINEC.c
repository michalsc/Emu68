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
__attribute__((alias("EMIT_AND_reg")));
static uint32_t *EMIT_AND_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_AND_mem")));
static uint32_t *EMIT_AND_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_AND_ext")));
static uint32_t *EMIT_AND_ext(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
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
    (void)m68k_ptr;

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
__attribute__((alias("EMIT_ABCD_reg")));
static uint32_t *EMIT_ABCD_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_ABCD_mem")));
static uint32_t *EMIT_ABCD_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
#ifdef __aarch64__
    (void)m68k_ptr;
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

static EMIT_Function JumpTable[512] = {
    [0000 ... 0007] = EMIT_AND_reg,  //D0 Destination, Byte
    [0020 ... 0047] = EMIT_AND_mem,
    [0050 ... 0074] = EMIT_AND_ext,
    [0100 ... 0107] = EMIT_AND_reg, //Word
    [0120 ... 0147] = EMIT_AND_mem,
    [0150 ... 0174] = EMIT_AND_ext,
    [0200 ... 0207] = EMIT_AND_reg, //Long
    [0220 ... 0247] = EMIT_AND_mem,
    [0250 ... 0274] = EMIT_AND_ext,
    
    [0300 ... 0307] = EMIT_MULU, //_reg, //D0 Destination
    [0320 ... 0347] = EMIT_MULU, //_mem,
    [0350 ... 0374] = EMIT_MULU, //_ext,
    
    [0400 ... 0407] = EMIT_ABCD_reg, //D0 Destination
    [0410 ... 0417] = EMIT_ABCD_mem, //-Ax),-(Ay)
    [0420 ... 0447] = EMIT_AND_mem, //Byte
    [0450 ... 0471] = EMIT_AND_ext, //D0 Source
    
    [0500 ... 0517] = EMIT_EXG, //R0 Source, unsized always the full register
    [0520 ... 0547] = EMIT_AND_mem, //Word
    [0550 ... 0571] = EMIT_AND_ext, 
    
    [0610 ... 0617] = EMIT_EXG,  //D0 Source
    [0620 ... 0647] = EMIT_AND_mem, //Long
    [0650 ... 0671] = EMIT_AND_ext,
    
    [0700 ... 0707] = EMIT_MULS, //_reg, //D0 Destination
    [0720 ... 0747] = EMIT_MULS, //_mem,
    [0750 ... 0774] = EMIT_MULS, //_ext,
};


uint32_t *EMIT_lineC(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;
    *insn_consumed = 1;

    /* 1100xxx011xxxxxx - MULU */
    if (JumpTable[opcode & 00777])
    {
        ptr = JumpTable[opcode & 00777](ptr, opcode, m68k_ptr);
    }
    else
    {
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        ptr = EMIT_InjectPrintContext(ptr);
        *ptr++ = udf(opcode);
    }

    return ptr;
}
