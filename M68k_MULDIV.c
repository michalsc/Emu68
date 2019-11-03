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

struct Result32 {
    uint32_t q;
    uint32_t r;
};

struct Result64 {
    uint64_t q;
    uint64_t r;
};

struct Result32 uidiv(uint32_t n, uint32_t d)
{
    struct Result32 res = { 0, 0 };

    if (n == 0)
        return res;

    for (int i = 31 - __builtin_clzl(n); i >= 0; --i)
    {
        res.r <<= 1;
        if (n & (1 << i)) res.r |= 1;
        if (res.r >= d) {
            res.r -= d;
            res.q |= 1 << i;
        }
    }

    return res;
}

struct Result32 sidiv(int32_t n, int32_t d)
{
    struct Result32 res = { 0, 0 };

    if (d < 0) {
        res = sidiv(n, -d);
        res.q = -res.q;
        return res;
    }

    if (n < 0) {
        res = sidiv(-n, d);
        if (res.r == 0) {
            res.q = -res.q;
        }
        else {
            res.q = -res.q - 1;
            res.r = d - res.r;
        }

        return res;
    }

    res = uidiv(n, d);

    return res;
}

struct Result64 uldiv(uint64_t n, uint64_t d)
{
    struct Result64 res = { 0, 0 };

    if (n == 0)
        return res;

    for (int i = 63 - __builtin_clzll(n); i >= 0; --i)
    {
        res.r <<= 1;
        if (n & (1 << i)) res.r |= 1;
        if (res.r >= d) {
            res.r -= d;
            res.q |= 1 << i;
        }
    }

    return res;
}

struct Result64 sldiv(int64_t n, int64_t d)
{
    struct Result64 res = { 0, 0 };

    if (d < 0) {
        res = sldiv(n, -d);
        res.q = -res.q;
        return res;
    }

    if (n < 0) {
        res = sldiv(-n, d);
        if (res.r == 0) {
            res.q = -res.q;
        }
        else {
            res.q = -res.q - 1;
            res.r = d - res.r;
        }

        return res;
    }

    res = uldiv(n, d);

    return res;
}

uint32_t *EMIT_MULS_W(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t reg;
    uint8_t tmp;
    uint8_t src = RA_AllocARMRegister(&ptr);
    uint8_t ext_words = 0;

    // Fetch 16-bit register: source and destination
    reg = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);

    // Fetch 16-bit multiplicant
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &tmp, opcode & 0x3f, *m68k_ptr, &ext_words, 0);

    // Sign-extend 16-bit multiplicants
    *ptr++ = sxth(reg, reg, 0);
    *ptr++ = sxth(src, tmp, 0);
    RA_FreeARMRegister(&ptr, tmp);

    *ptr++ = muls(reg, reg, src);

    RA_FreeARMRegister(&ptr, src);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    uint8_t mask = M68K_GetSRMask(*m68k_ptr);
    uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

    if (update_mask)
    {
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_Z);
    }

    return ptr;
}

uint32_t *EMIT_MULU_W(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t reg;
    uint8_t tmp;
    uint8_t src = RA_AllocARMRegister(&ptr);
    uint8_t ext_words = 0;

    // Fetch 16-bit register: source and destination
    reg = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);

    // Fetch 16-bit multiplicant
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &tmp, opcode & 0x3f, *m68k_ptr, &ext_words, 0);

    // Sign-extend 16-bit multiplicants
    *ptr++ = uxth(reg, reg, 0);
    *ptr++ = uxth(src, tmp, 0);
    RA_FreeARMRegister(&ptr, tmp);

    *ptr++ = muls(reg, reg, src);

    RA_FreeARMRegister(&ptr, src);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    uint8_t mask = M68K_GetSRMask(*m68k_ptr);
    uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

    if (update_mask)
    {
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_Z);
    }

    return ptr;
}

uint32_t *EMIT_MULS_L(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t reg_dl;
    uint8_t reg_dh = 0xff;
    uint8_t src;
    uint8_t ext_words = 1;
    uint16_t opcode2 = BE16((*m68k_ptr)[0]);

    // Fetch 32-bit register: source and destination
    reg_dl = RA_MapM68kRegister(&ptr, (opcode2 >> 12) & 7);
    RA_SetDirtyM68kRegister(&ptr, (opcode2 >> 12) & 7);

    // Fetch 32-bit multiplicant
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0);

    if (opcode2 & (1 << 10))
    {
        reg_dh = RA_MapM68kRegisterForWrite(&ptr, (opcode2 & 7));
    }
    else
    {
        reg_dh = RA_AllocARMRegister(&ptr);
    }

    if (opcode2 & (1 << 11))
        *ptr++ = smulls(reg_dh, reg_dl, reg_dl, src);
    else
        *ptr++ = umulls(reg_dh, reg_dl, reg_dl, src);

    RA_FreeARMRegister(&ptr, src);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    uint8_t mask = M68K_GetSRMask(*m68k_ptr);
    uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

    if (update_mask)
    {
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_Z)
            *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        if (update_mask & SR_N)
            *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
        if ((update_mask & SR_V) && !(opcode2 & (1 << 10)))
        {
            if (opcode2 & (1 << 11))
            {
                uint8_t tmp = RA_AllocARMRegister(&ptr);
                /*
                    32-bit result was requested. Check if top 32 are sign-extension of lower 32bit variable.
                    If this is not the case, set V bit
                */
                *ptr++ = cmp_immed(reg_dl, 0);
                *ptr++ = mvn_cc_immed_u8(ARM_CC_MI, tmp, 0);
                *ptr++ = mov_cc_immed_u8(ARM_CC_PL, tmp, 0);
                *ptr++ = subs_reg(tmp, reg_dh, tmp, 0);
                *ptr++ = orr_cc_immed(ARM_CC_NE, REG_SR, REG_SR, SR_V);

                RA_FreeARMRegister(&ptr, tmp);
            }
            else
            {
                *ptr++ = cmp_immed(reg_dh, 0);
                *ptr++ = orr_cc_immed(ARM_CC_NE, REG_SR, REG_SR, SR_V);
            }

        }
    }

    RA_FreeARMRegister(&ptr, reg_dh);

    return ptr;
}

uint32_t *EMIT_DIVS_W(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t reg_a = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    uint8_t reg_q;
    uint8_t reg_quot = RA_AllocARMRegister(&ptr);
    uint8_t reg_rem = RA_AllocARMRegister(&ptr);
    uint8_t ext_words = 0;

    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &reg_q, opcode & 0x3f, *m68k_ptr, &ext_words, 0);

    *ptr++ = cmp_immed(reg_q, 0);
    *ptr++ = b_cc(ARM_CC_NE, 0);
    /* At this place handle exception - division by zero! */
    *ptr++ = udf(0);

    if (ARM_SUPPORTS_DIV)
    {
        /* Sign extend divisor from 16-bit to 32-bit */
        *ptr++ = sxth(reg_rem, reg_q, 0);
        *ptr++ = sdiv(reg_quot, reg_a, reg_rem);
        *ptr++ = mls(reg_rem, reg_a, reg_quot, reg_rem);
    }
    else
    {
        /* Keep r0-r3,lr and ip safe on the stack. Exclude reg_quot and reg_rem in case they were allocated in r0..r4 range */
        *ptr++ = push(((1 << reg_a) | (1 << reg_q) | 0x0f | (1 << 12) | (1 << 14)) & ~((1 << reg_quot) | (1 << reg_rem)));

        if (reg_a != 1)
            *ptr++ = push(1 << reg_a);
        if (reg_q != 2) {
            *ptr++ = push(1 << reg_q);
            *ptr++ = pop(4);
        }
        if (reg_a != 1)
            *ptr++ = pop(2);

        /* Call (u)idivmod */
        *ptr++ = sub_immed(13, 13, 8);
        *ptr++ = mov_reg(0, 13);
        *ptr++ = ldr_offset(15, 12, 4);
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *ptr++ = BE32((uint32_t)&sidiv);

        /* Pop quotient and (eventually) reminder from the stack */
        *ptr++ = pop(1 << reg_quot);
        *ptr++ = pop(1 << reg_rem);

        /* Restore registers from the stack */
        *ptr++ = pop(((1 << reg_a) | (1 << reg_q) | 0x0f | (1 << 12) | (1 << 14)) & ~((1 << reg_quot) | (1 << reg_rem)));
    }

    /* Test bit 15 of quotient */
    *ptr++ = tst_immed(reg_quot, 0x902);

    /* Sign-extract upper 16 bits of quotient into temporary register */
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    *ptr++ = sxth(tmp, reg_quot, 2);
    /* If bit 15 of quotient was set, increase extracted 16 bits, should advance to 0 */
    *ptr++ = add_cc_immed(ARM_CC_NE, tmp, tmp, 1);
    *ptr++ = cmp_immed(tmp, 0);
    RA_FreeARMRegister(&ptr, tmp);

    (*m68k_ptr) += ext_words;

    RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
    uint8_t mask = M68K_GetSRMask(*m68k_ptr);
    uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

    /* if temporary register was 0 the division was successful, otherwise overflow occured! */
    if (update_mask)
    {
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_V)
        {
            *ptr++ = orr_cc_immed(ARM_CC_NE, REG_SR, REG_SR, SR_V);
        }
        int cnt = 0;
        if (update_mask & SR_N)
            cnt++;
        if (update_mask & SR_Z)
            cnt++;
        if (cnt)
        {
            *ptr++ = b_cc(ARM_CC_NE, cnt+3);
            *ptr++ = cmp_immed(reg_quot, 0);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        }
    }

    /* Move signed 16-bit quotient to lower 16 bits of target register, signed 16 bit reminder to upper 16 bits */
    *ptr++ = mov_reg(reg_a, reg_quot);
    *ptr++ = bfi(reg_a, reg_rem, 16, 16);

    /* Advance PC */
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));

    RA_FreeARMRegister(&ptr, reg_a);
    RA_FreeARMRegister(&ptr, reg_q);
    RA_FreeARMRegister(&ptr, reg_quot);
    RA_FreeARMRegister(&ptr, reg_rem);

    if (!ARM_SUPPORTS_DIV)
        *ptr++ = INSN_TO_LE(0xfffffff0);

    return ptr;
}

uint32_t *EMIT_DIVU_W(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t reg_a = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    uint8_t reg_q;
    uint8_t reg_quot = RA_AllocARMRegister(&ptr);
    uint8_t reg_rem = RA_AllocARMRegister(&ptr);
    uint8_t ext_words = 0;

    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &reg_q, opcode & 0x3f, *m68k_ptr, &ext_words, 0);

    *ptr++ = cmp_immed(reg_q, 0);
    *ptr++ = b_cc(ARM_CC_NE, 0);
    /* At this place handle exception - division by zero! */
    *ptr++ = udf(0);

    if (ARM_SUPPORTS_DIV)
    {
        /* Sign extend divisor from 16-bit to 32-bit */
        *ptr++ = uxth(reg_rem, reg_q, 0);
        *ptr++ = udiv(reg_quot, reg_a, reg_rem);
        *ptr++ = mls(reg_rem, reg_a, reg_quot, reg_rem);
    }
    else
    {
        /* Keep r0-r3,lr and ip safe on the stack. Exclude reg_quot and reg_rem in case they were allocated in r0..r4 range */
        *ptr++ = push(((1 << reg_a) | (1 << reg_q) | 0x0f | (1 << 12)) & ~((1 << reg_quot) | (1 << reg_rem)));

        if (reg_a != 1)
            *ptr++ = push(1 << reg_a);
        if (reg_q != 2) {
            *ptr++ = push(1 << reg_q);
            *ptr++ = pop(4);
        }
        if (reg_a != 1)
            *ptr++ = pop(2);

        /* Call (u)idivmod */
        *ptr++ = sub_immed(13, 13, 8);
        *ptr++ = mov_reg(0, 13);
        *ptr++ = ldr_offset(15, 12, 4);
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *ptr++ = BE32((uint32_t)&uidiv);

        /* Pop quotient and (eventually) reminder from the stack */
        *ptr++ = pop(1 << reg_quot);
        *ptr++ = pop(1 << reg_rem);

        /* Restore registers from the stack */
        *ptr++ = pop(((1 << reg_a) | (1 << reg_q) | 0x0f | (1 << 12)) & ~((1 << reg_quot) | (1 << reg_rem)));
    }
    /* Extract upper 16 bits of quotient into temporary register */
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    *ptr++ = uxth(tmp, reg_quot, 2);
    *ptr++ = cmp_immed(tmp, 0);
    RA_FreeARMRegister(&ptr, tmp);

    (*m68k_ptr) += ext_words;

    RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
    uint8_t mask = M68K_GetSRMask(*m68k_ptr);
    uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

    /* if temporary register was 0 the division was successful, otherwise overflow occured! */
    if (update_mask)
    {
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_V)
        {
            *ptr++ = orr_cc_immed(ARM_CC_NE, REG_SR, REG_SR, SR_V);
        }
        int cnt = 0;
        if (update_mask & SR_N)
            cnt+=2;
        if (update_mask & SR_Z)
            cnt++;
        if (cnt)
        {
            *ptr++ = b_cc(ARM_CC_NE, cnt+3);
            *ptr++ = cmp_immed(reg_quot, 0);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            if (update_mask & SR_N) {
                /* When setting N flag do not test 32-bit quotient but rather 16 bit. Therefore test bit 15 here */
                *ptr++ = tst_immed(reg_quot, 0x902);
                *ptr++ = orr_cc_immed(ARM_CC_NE, REG_SR, REG_SR, SR_N);
            }
        }
    }

    /* Move unsigned 16-bit quotient to lower 16 bits of target register, unsigned 16 bit reminder to upper 16 bits */
    *ptr++ = mov_reg(reg_a, reg_quot);
    *ptr++ = bfi(reg_a, reg_rem, 16, 16);

    /* Advance PC */
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));

    RA_FreeARMRegister(&ptr, reg_a);
    RA_FreeARMRegister(&ptr, reg_q);
    RA_FreeARMRegister(&ptr, reg_quot);
    RA_FreeARMRegister(&ptr, reg_rem);

    if (!ARM_SUPPORTS_DIV)
        *ptr++ = INSN_TO_LE(0xfffffff0);

    return ptr;
}

uint32_t *EMIT_DIVUS_L(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint16_t opcode2 = BE16((*m68k_ptr)[0]);
    uint8_t sig = (opcode2 & (1 << 11)) != 0;
    uint8_t div64 = (opcode2 & (1 << 10)) != 0;
    uint8_t reg_q = 0xff;
    uint8_t reg_dq = RA_MapM68kRegister(&ptr, (opcode2 >> 12) & 7);
    uint8_t reg_dr = 0xff;
    uint8_t ext_words = 1;

    /* If Dr != Dq use remainder and alloc it */
    if ((opcode2 & 7) != ((opcode2 >> 12) & 7))
        reg_dr = RA_MapM68kRegister(&ptr, opcode2 & 7);

    // Load divisor
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &reg_q, opcode & 0x3f, *m68k_ptr, &ext_words, 0);

    // Check if division by 0
    *ptr++ = cmp_immed(reg_q, 0);
    *ptr++ = b_cc(ARM_CC_NE, 0);
    /* At this place handle exception - division by zero! */
    *ptr++ = udf(0);

    if (ARM_SUPPORTS_DIV)
    {
        if (div64)
        {
            /* Div64/32 ->32:32 routine based on article: https://gmplib.org/~tege/division-paper.pdf */
            /* Warning - the routine exhausts all registers from allocator! */
            uint8_t tmp_r0 = reg_dr; /* High 32 bits of dividend */
            uint8_t tmp_r1 = reg_dq; /* Low 32 bits of dividend */
            uint8_t tmp_r2 = reg_q;  /* divisor */
            uint8_t tmp_r3 = RA_AllocARMRegister(&ptr);
            uint8_t tmp_r4 = RA_AllocARMRegister(&ptr);
            uint8_t tmp_r5 = RA_AllocARMRegister(&ptr);
            uint8_t tmp_r6 = RA_AllocARMRegister(&ptr);
            uint8_t tmp_r7 = RA_AllocARMRegister(&ptr);
            uint8_t tmp_r8 = RA_AllocARMRegister(&ptr);
            uint8_t tmp_r9 = RA_AllocARMRegister(&ptr);

            printf("DUV%s_L 64/32->32:32\n", sig ? 'S':'U');

            if (sig) {

            }
            else {
                *ptr++ = clz(tmp_r3, tmp_r2);
                *ptr++ = cmp_immed(tmp_r3, 0);
                *ptr++ = mov_cc_reg(ARM_CC_EQ, tmp_r6, tmp_r2);
                *ptr++ = b_cc(ARM_CC_EQ, 9);
                *ptr++ = mov_reg(tmp_r4, tmp_r0);
                *ptr++ = mov_reg(tmp_r5, tmp_r1);
                *ptr++ = sub_immed(tmp_r1, tmp_r3, 32);
                *ptr++ = rsb_immed(tmp_r0, tmp_r3, 32);
                *ptr++ = lsl_reg(tmp_r4, tmp_r4, tmp_r3);
                *ptr++ = orr_reg_lsl_reg(tmp_r4, tmp_r4, tmp_r5, tmp_r1);
                *ptr++ = orr_reg_lsr_reg(tmp_r4, tmp_r4, tmp_r5, tmp_r0);
                *ptr++ = lsl_reg(tmp_r1, tmp_r5, tmp_r3);
                *ptr++ = mov_reg(tmp_r0, tmp_r4);
                *ptr++ = lsl_reg(tmp_r6, tmp_r2, tmp_r3);
                *ptr++ = movw_immed_u16(tmp_r3, 0xc200);
                *ptr++ = and_immed(tmp_r5, tmp_r6, 1);
                *ptr++ = lsr_immed(tmp_r4, tmp_r6, 22);
                *ptr++ = movt_immed_u16(tmp_r3, 0x00ff);
                *ptr++ = lsr_immed(tmp_r8, tmp_r6, 11);
                *ptr++ = udiv(tmp_r3, tmp_r3, tmp_r4);
                *ptr++ = mul(tmp_r4, tmp_r3, tmp_r3);
                *ptr++ = lsl_immed(tmp_r3, tmp_r3, 4);
                *ptr++ = add_immed(tmp_r8, tmp_r8, 1);
                *ptr++ = umull(tmp_r9, tmp_r8, tmp_r8, tmp_r4);
                *ptr++ = sub_immed(tmp_r4, tmp_r3, 1);
                *ptr++ = rsb_immed(tmp_r3, tmp_r3, 1);
                *ptr++ = sub_reg(tmp_r7, tmp_r4, tmp_r8, 0);
                *ptr++ = mov_reg(tmp_r9, tmp_r1);
                *ptr++ = add_reg(tmp_r3, tmp_r3, tmp_r8, 0);
                *ptr++ = lsr_immed(tmp_r4, tmp_r7, 1);
                *ptr++ = mul(tmp_r4, tmp_r4, tmp_r5);
                *ptr++ = lsr_immed(tmp_r5, tmp_r6, 1);
                *ptr++ = mla(tmp_r4, tmp_r4, tmp_r5, tmp_r3);
                *ptr++ = lsl_immed(tmp_r3, tmp_r7, 15);
                *ptr++ = umull(tmp_r5, tmp_r4, tmp_r4, tmp_r7);
                *ptr++ = mov_immed_u8(tmp_r5, 0);
                *ptr++ = add_reg_lsr_imm(tmp_r4, tmp_r3, tmp_r4, 1);
                *ptr++ = umlal(tmp_r1, tmp_r0, tmp_r0, tmp_r4);
                *ptr++ = mov_reg(tmp_r4, tmp_r0);
                *ptr++ = add_immed(tmp_r0, tmp_r4, 1);
                *ptr++ = mls(tmp_r3, tmp_r9, tmp_r6, tmp_r0);
                *ptr++ = cmp_reg(tmp_r3, tmp_r1);
                *ptr++ = add_cc_reg(ARM_CC_HI, tmp_r3, tmp_r3, tmp_r6, 0);
                *ptr++ = mov_cc_reg(ARM_CC_HI, tmp_r0, tmp_r4);
                *ptr++ = cmp_reg(tmp_r3, tmp_r6);
                *ptr++ = sub_cc_reg(ARM_CC_CS, tmp_r3, tmp_r3, tmp_r6, 0);
                *ptr++ = add_cc_immed(ARM_CC_CS, tmp_r0, tmp_r0, 1);
                *ptr++ = orr_reg(tmp_r7, tmp_r5, tmp_r0, 0);
                *ptr++ = mov_reg(tmp_r0, tmp_r3);
                *ptr++ = mov_reg(tmp_r1, tmp_r7);
            }

            RA_FreeARMRegister(&ptr, tmp_r3);
            RA_FreeARMRegister(&ptr, tmp_r4);
            RA_FreeARMRegister(&ptr, tmp_r5);
            RA_FreeARMRegister(&ptr, tmp_r6);
            RA_FreeARMRegister(&ptr, tmp_r7);
            RA_FreeARMRegister(&ptr, tmp_r8);
            RA_FreeARMRegister(&ptr, tmp_r9);
        }
        else
        {
            if (reg_dr == 0xff)
            {
                if (sig)
                    *ptr++ = sdiv(reg_dq, reg_dq, reg_q);
                else
                    *ptr++ = udiv(reg_dq, reg_dq, reg_q);
            }
            else
            {
                uint8_t tmp = RA_AllocARMRegister(&ptr);

                if (sig)
                    *ptr++ = sdiv(tmp, reg_dq, reg_q);
                else
                    *ptr++ = udiv(tmp, reg_dq, reg_q);

                *ptr++ = mls(reg_dr, reg_dq, tmp, reg_q);
                *ptr++ = mov_reg(reg_dq, tmp);

                RA_FreeARMRegister(&ptr, tmp);
            }
        }
    }
    else
    {
        /* Keep r0-r3,lr and ip safe on the stack. Exclude reg_dr and reg_dq in case they were allocated in r0..r4 range */
        *ptr++ = push(((1 << reg_q) | 0x0f | (1 << 12)) & ~((1 << reg_dr) | (1 << reg_dq)));

        /* In case of 64-bit division use (u)ldivmod, otherwise use (u)idivmod */
        if (div64)
        {
    printf("64 bit division not done yet!\n");
        }
        else
        {
            /* Use stack to put divisor and divident into registers */
            if (reg_dq != 1)
                *ptr++ = push(1 << reg_dq);
            if (reg_q != 2) {
                *ptr++ = push(1 << reg_q);
                *ptr++ = pop(4);
            }
            if (reg_dq != 1)
                *ptr++ = pop(2);

            /* Call (u)idivmod */
            *ptr++ = sub_immed(13, 13, 8);
            *ptr++ = mov_reg(0, 13);
            *ptr++ = ldr_offset(15, 12, 4);
            *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
            *ptr++ = b_cc(ARM_CC_AL, 0);
            if (sig)
                *ptr++ = BE32((uint32_t)&sidiv);
            else
                *ptr++ = BE32((uint32_t)&uidiv);

            /* Pop quotient and (eventually) reminder from the stack */
            *ptr++ = pop(1 << reg_dq);
            if (reg_dr != 0xff)
                *ptr++ = pop(1 << reg_dr);
            else
                *ptr++ = add_immed(13, 13, 4);
        }

        /* Restore registers from the stack */
        *ptr++ = pop(((1 << reg_q) | 0x0f | (1 << 12)) & ~((1 << reg_dr) | (1 << reg_dq)));
    }

    (*m68k_ptr) += ext_words;

    /* Set Dq dirty */
    RA_SetDirtyM68kRegister(&ptr, (opcode2 >> 12) & 7);
    /* Set Dr dirty if it was used/changed */
    if (reg_dr != 0xff)
        RA_SetDirtyM68kRegister(&ptr, opcode2 & 7);

    uint8_t mask = M68K_GetSRMask(*m68k_ptr);
    uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

    if (update_mask)
    {
        M68K_ModifyCC(&ptr);
        *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
        if (update_mask & SR_V)
        {
            /* V bit can be set only with 64-bit division was used. Otherwise no overflow can occur */
            if (div64) {

            }
        }
        int cnt = 0;
        if (update_mask & SR_N)
            cnt++;
        if (update_mask & SR_Z)
            cnt++;
        if (cnt)
        {
            *ptr++ = cmp_immed(reg_dq, 0);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            if (update_mask & SR_N) {
                /* When setting N flag do not test 32-bit quotient but rather 16 bit. Therefore test bit 15 here */
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            }
        }
    }

    /* Advance PC */
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));

    RA_FreeARMRegister(&ptr, reg_q);
    RA_FreeARMRegister(&ptr, reg_dq);
    if (reg_dr != 0xff)
        RA_FreeARMRegister(&ptr, reg_dr);

    if (!ARM_SUPPORTS_DIV)
        *ptr++ = INSN_TO_LE(0xfffffff0);

    return ptr;
}

uint32_t *EMIT_MUL_DIV(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    (void)m68k_ptr;

    if ((opcode & 0xf1c0) == 0xc1c0)
    {
        ptr = EMIT_MULS_W(ptr, opcode, m68k_ptr);
    }
    if ((opcode & 0xf1c0) == 0xc0c0)
    {
        ptr = EMIT_MULU_W(ptr, opcode, m68k_ptr);
    }
    if ((opcode & 0xffc0) == 0x4c00)
    {
        ptr = EMIT_MULS_L(ptr, opcode, m68k_ptr);
    }
    if ((opcode & 0xffc0) == 0x4c40)
    {
        ptr = EMIT_DIVUS_L(ptr, opcode, m68k_ptr);
    }
    if ((opcode & 0xf1c0) == 0x81c0)
    {
        ptr = EMIT_DIVS_W(ptr, opcode, m68k_ptr);
    }
    if ((opcode & 0xf1c0) == 0x80c0)
    {
        ptr = EMIT_DIVU_W(ptr, opcode, m68k_ptr);
    }

    return ptr;
}
