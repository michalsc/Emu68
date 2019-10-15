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
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &tmp, opcode & 0x3f, *m68k_ptr, &ext_words);

    // Sign-extend 16-bit multiplicants
    *ptr++ = sxth(reg, reg, 0);
    *ptr++ = sxth(src, tmp, 0);
    RA_FreeARMRegister(&ptr, tmp);

    *ptr++ = muls(reg, reg, src);

    RA_FreeARMRegister(&ptr, src);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
    uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

    if (update_mask)
    {
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
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &tmp, opcode & 0x3f, *m68k_ptr, &ext_words);

    // Sign-extend 16-bit multiplicants
    *ptr++ = uxth(reg, reg, 0);
    *ptr++ = uxth(src, tmp, 0);
    RA_FreeARMRegister(&ptr, tmp);

    *ptr++ = muls(reg, reg, src);

    RA_FreeARMRegister(&ptr, src);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
    uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

    if (update_mask)
    {
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
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &src, opcode & 0x3f, *m68k_ptr, &ext_words);

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

    uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
    uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

    if (update_mask)
    {
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

extern void * __aeabi_uidivmod;
extern void * __aeabi_idivmod;
extern void * __aeabi_uldivmod;
extern void * __aeabi_ldivmod;

/* Used for 16-bit divisions */
void * ptr_uidivmod = &__aeabi_uidivmod;    /* In: r0, r1. Out: r0 = r0 / r1, r1 = r0 % r1 */
void * ptr_idivmod  = &__aeabi_idivmod;     /* In: r0, r1. Out: r0 = r0 / r1, r1 = r0 % r1 */

/* Used for 32-bit divisions */
void * ptr_uldivmod  = &__aeabi_uldivmod;     /* In: r0:r1, r2:r3. Out: r0:r1 = quotient, r2:r3 = reminder */
void * ptr_ldivmod  = &__aeabi_ldivmod;     /* In: r0:r1, r2:r3. Out: r0:r1 = quotient, r2:r3 = reminder */

uint32_t *EMIT_DIVS_W(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t reg_a = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    uint8_t reg_q;
    uint8_t reg_quot = RA_AllocARMRegister(&ptr);
    uint8_t reg_rem = RA_AllocARMRegister(&ptr);
    uint8_t ext_words = 0;

    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &reg_q, opcode & 0x3f, *m68k_ptr, &ext_words);

    *ptr++ = cmp_immed(reg_q, 0);
    *ptr++ = b_cc(ARM_CC_NE, 0);
    /* At this place handle exception - division by zero! */
    *ptr++ = udf(0);

    /* Keep r0-r3,lr and ip safe on the stack. Exclude reg_quot and reg_rem in case they were allocated in r0..r4 range */
    *ptr++ = push(((1 << reg_a) | (1 << reg_q) | 0x0f | (1 << 12) | (1 << 14)) & ~((1 << reg_quot) | (1 << reg_rem)));

    /* Push a and q on the stack and pop them back into r0 and r1 */
    if (reg_a == 0 && reg_q == 1)
    {
        /* Registers are at correct positions. Do nothing */
    }
    else if (reg_a == 1 && reg_q == 0)
    {
        /*
            Registers need to be swapped. Use ip as temporaray register (it's contents is on the stack anyway )
            NOTE: Usually this would destroy the destination register, but luckily for us this one is saved on
            the stack.
        */
        *ptr++ = mov_reg(12, reg_a);
        *ptr++ = mov_reg(reg_a, reg_q);
        *ptr++ = mov_reg(reg_q, 12);
    }
    else
    {
        /* Use stack, the registers are totally wrong */
        *ptr++ = push(1 << reg_a);
        *ptr++ = push(1 << reg_q);
        *ptr++ = pop(0x2);
        *ptr++ = pop(0x1);
    }

    *ptr++ = ldr_offset(15, 12, 4);
    #if !(EMU68_HOST_BIG_ENDIAN) && EMU68_HAS_SETEND
    *end++ = setend_le();
    #endif
    *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
    #if !(EMU68_HOST_BIG_ENDIAN) && EMU68_HAS_SETEND
    *end++ = setend_be();
    #endif
    *ptr++ = b_cc(ARM_CC_AL, 0);
    *ptr++ = BE32((uint32_t)&__aeabi_idivmod);

    /* Get back results. Use same technique as before */
    if (reg_quot == 0 && reg_rem == 1)
    {
        /* Output registers are already correctly placed, do nothing here */
    }
    else if (reg_quot == 1 && reg_rem == 0)
    {
        *ptr++ = mov_reg(12, reg_a);
        *ptr++ = mov_reg(reg_a, reg_q);
        *ptr++ = mov_reg(reg_q, 12);
    }
    else
    {
        /* Push r0 and r1 on the stack, pop them back into quotient and reminder */
        *ptr++ = push(0x1);
        *ptr++ = push(0x2);
        *ptr++ = pop(1 << reg_rem);
        *ptr++ = pop(1 << reg_quot);
    }

    /* Restore registers from the stack */
    *ptr++ = pop(((1 << reg_a) | (1 << reg_q) | 0x0f | (1 << 12) | (1 << 14)) & ~((1 << reg_quot) | (1 << reg_rem)));

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
    uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
    uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

    /* if temporary register was 0 the division was successful, otherwise overflow occured! */
    if (update_mask)
    {
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

    return ptr;
}

uint32_t *EMIT_DIVU_W(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t reg_a = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    uint8_t reg_q;
    uint8_t reg_quot = RA_AllocARMRegister(&ptr);
    uint8_t reg_rem = RA_AllocARMRegister(&ptr);
    uint8_t ext_words = 0;

    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &reg_q, opcode & 0x3f, *m68k_ptr, &ext_words);

    *ptr++ = cmp_immed(reg_q, 0);
    *ptr++ = b_cc(ARM_CC_NE, 0);
    /* At this place handle exception - division by zero! */
    *ptr++ = udf(0);

    /* Keep r0-r3,lr and ip safe on the stack. Exclude reg_quot and reg_rem in case they were allocated in r0..r4 range */
    *ptr++ = push(((1 << reg_a) | (1 << reg_q) | 0x0f | (1 << 12)) & ~((1 << reg_quot) | (1 << reg_rem)));

    /* Push a and q on the stack and pop them back into r0 and r1 */
    if (reg_a == 0 && reg_q == 1)
    {
        /* Registers are at correct positions. Do nothing */
    }
    else if (reg_a == 1 && reg_q == 0)
    {
        /* Registers need to be swapped. Use ip as temporaray register (it's contents is on the stack anyway ) */
        *ptr++ = mov_reg(12, reg_a);
        *ptr++ = mov_reg(reg_a, reg_q);
        *ptr++ = mov_reg(reg_q, 12);
    }
    else
    {
        /* Use stack, the registers are totally wrong */
        *ptr++ = push(1 << reg_a);
        *ptr++ = push(1 << reg_q);
        *ptr++ = pop(0x2);
        *ptr++ = pop(0x1);
    }

    *ptr++ = ldr_offset(15, 12, 4);
    #if !(EMU68_HOST_BIG_ENDIAN) && EMU68_HAS_SETEND
    *end++ = setend_le();
    #endif
    *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
    #if !(EMU68_HOST_BIG_ENDIAN) && EMU68_HAS_SETEND
    *end++ = setend_be();
    #endif
    *ptr++ = b_cc(ARM_CC_AL, 0);
    *ptr++ = BE32((uint32_t)&__aeabi_uidivmod);

    /* Get back results. Use same technique as before */
    if (reg_quot == 0 && reg_rem == 1)
    {
        /* Output registers are already correctly placed, do nothing here */
    }
    else if (reg_quot == 1 && reg_rem == 0)
    {
        *ptr++ = mov_reg(12, reg_a);
        *ptr++ = mov_reg(reg_a, reg_q);
        *ptr++ = mov_reg(reg_q, 12);
    }
    else
    {
        /* Push r0 and r1 on the stack, pop them back into quotient and reminder */
        *ptr++ = push(0x1);
        *ptr++ = push(0x2);
        *ptr++ = pop(1 << reg_rem);
        *ptr++ = pop(1 << reg_quot);
    }

    /* Restore registers from the stack */
    *ptr++ = pop(((1 << reg_a) | (1 << reg_q) | 0x0f | (1 << 12)) & ~((1 << reg_quot) | (1 << reg_rem)));

    /* Extract upper 16 bits of quotient into temporary register */
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    *ptr++ = uxth(tmp, reg_quot, 2);
    *ptr++ = cmp_immed(tmp, 0);
    RA_FreeARMRegister(&ptr, tmp);

    (*m68k_ptr) += ext_words;

    RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
    uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
    uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

    /* if temporary register was 0 the division was successful, otherwise overflow occured! */
    if (update_mask)
    {
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
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &reg_q, opcode & 0x3f, *m68k_ptr, &ext_words);

    // Check if division by 0
    *ptr++ = cmp_immed(reg_q, 0);
    *ptr++ = b_cc(ARM_CC_NE, 0);
    /* At this place handle exception - division by zero! */
    *ptr++ = udf(0);

    /* Keep r0-r3,lr and ip safe on the stack. Exclude reg_dr and reg_dq in case they were allocated in r0..r4 range */
    *ptr++ = push(((1 << reg_q) | 0x0f | (1 << 12)) & ~((1 << reg_dr) | (1 << reg_dq)));

    /* In case of 64-bit division use (u)ldivmod, otherwise use (u)idivmod */
    if (div64)
    {

    }
    else
    {
        /* Use stack to put divisor and divident into registers */
        if (reg_dq != 0)
            *ptr++ = push(1 << reg_dq);
        if (reg_q != 1) {
            *ptr++ = push(1 << reg_q);
            *ptr++ = pop(2);
        }
        if (reg_dq != 0)
            *ptr++ = pop(1);

        /* Call (u)idivmod */
        *ptr++ = ldr_offset(15, 12, 4);
        #if !(EMU68_HOST_BIG_ENDIAN) && EMU68_HAS_SETEND
        *end++ = setend_le();
        #endif
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
        #if !(EMU68_HOST_BIG_ENDIAN) && EMU68_HAS_SETEND
            *end++ = setend_be();
        #endif
        *ptr++ = b_cc(ARM_CC_AL, 0);
        if (sig)
            *ptr++ = BE32((uint32_t)&__aeabi_idivmod);
        else
            *ptr++ = BE32((uint32_t)&__aeabi_uidivmod);

        /* Use stack to put quotient and (if requested) reminder into proper registers */
        if (reg_dq != 0)
            *ptr++ = push(1);
        if ((reg_dr != 0xff) && (reg_dr != 1)) {
            *ptr++ = push(2);
            *ptr++ = pop(1 << reg_dr);
        }
        if (reg_dq != 0)
            *ptr++ = pop(1 << reg_dq);
    }

    /* Restore registers from the stack */
    *ptr++ = pop(((1 << reg_q) | 0x0f | (1 << 12)) & ~((1 << reg_dr) | (1 << reg_dq)));

    (*m68k_ptr) += ext_words;

    /* Set Dq dirty */
    RA_SetDirtyM68kRegister(&ptr, (opcode2 >> 12) & 7);
    /* Set Dr dirty if it was used/changed */
    if (reg_dr != 0xff)
        RA_SetDirtyM68kRegister(&ptr, opcode2 & 7);

    uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
    uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

    if (update_mask)
    {
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
/*    if ((opcode & 0xf1c0) == 0x80c0)
    {
        ptr = EMIT_DIVU_W(ptr, opcode, m68k_ptr);
    }
*/

    return ptr;
}
