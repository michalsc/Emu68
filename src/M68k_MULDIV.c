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
#include "EmuFeatures.h"

uint32_t *EMIT_MULU(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_MUL_DIV")));
uint32_t *EMIT_MULS(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_MUL_DIV")));

/* AArch64 has both multiply and divide. No need to have them in C form */
#ifndef __aarch64__
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
#endif

uint32_t *EMIT_MULS_W(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t reg;
    uint8_t src = 0xff;
    uint8_t ext_words = 0;

    // Fetch 16-bit register: source and destination
    reg = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);

    // Fetch 16-bit multiplicant
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

    // Sign-extend 16-bit multiplicants
#ifdef __aarch64__
    *ptr++ = sxth(reg, reg);
    *ptr++ = sxth(src, src);

    *ptr++ = mul(reg, reg, src);
#else
    *ptr++ = sxth(reg, reg, 0);
    *ptr++ = sxth(src, src, 0);

    *ptr++ = muls(reg, reg, src);
#endif
    RA_FreeARMRegister(&ptr, src);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
#ifdef __aarch64__
        *ptr++ = cmn_reg(31, reg, LSL, 0);
#endif
        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        if (update_mask & SR_Z) {
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        }
        if (update_mask & SR_N) {
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
        }
    }

    return ptr;
}

uint32_t *EMIT_MULU_W(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t reg;
    uint8_t src = 0xff;
    uint8_t ext_words = 0;

    // Fetch 16-bit register: source and destination
    reg = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);

    // Fetch 16-bit multiplicant
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

    // Sign-extend 16-bit multiplicants
#ifdef __aarch64__
    *ptr++ = uxth(reg, reg);
    *ptr++ = uxth(src, src);
    *ptr++ = mul(reg, reg, src);
#else
    *ptr++ = uxth(reg, reg, 0);
    *ptr++ = uxth(src, src, 0);

    *ptr++ = muls(reg, reg, src);
#endif
    RA_FreeARMRegister(&ptr, src);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
#ifdef __aarch64__
        *ptr++ = cmn_reg(31, reg, LSL, 0);
#endif
        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        if (update_mask & SR_Z) {
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        }
        if (update_mask & SR_N) {
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
        }
    }

    return ptr;
}

uint32_t *EMIT_MULS_L(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t reg_dl;
    uint8_t reg_dh = 0xff;
    uint8_t src = 0xff;
    uint8_t ext_words = 1;
    uint16_t opcode2 = BE16((*m68k_ptr)[0]);

    // Fetch 32-bit register: source and destination
    reg_dl = RA_MapM68kRegister(&ptr, (opcode2 >> 12) & 7);
    RA_SetDirtyM68kRegister(&ptr, (opcode2 >> 12) & 7);

    // Fetch 32-bit multiplicant
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

    if (opcode2 & (1 << 10))
    {
        reg_dh = RA_MapM68kRegisterForWrite(&ptr, (opcode2 & 7));
    }
    else
    {
        reg_dh = RA_AllocARMRegister(&ptr);
    }

#ifdef __aarch64__
    if (opcode2 & (1 << 11))
        *ptr++ = smull(reg_dl, reg_dl, src);
    else
        *ptr++ = umull(reg_dl, reg_dl, src);
    if (opcode2 & (1 << 10))
    {
        *ptr++ = add64_reg(reg_dh, 31, reg_dl, LSR, 32);
    }
#else
    if (opcode2 & (1 << 11))
        *ptr++ = smulls(reg_dh, reg_dl, reg_dl, src);
    else
        *ptr++ = umulls(reg_dh, reg_dl, reg_dl, src);
#endif

    RA_FreeARMRegister(&ptr, src);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);

#ifdef __aarch64__
        *ptr++ = cmn64_reg(31, reg_dl, LSL, 0);
#endif
        uint8_t old_mask = update_mask & SR_V;
        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        update_mask |= old_mask;

        if (update_mask & SR_Z) {
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        }
        if (update_mask & SR_N) {
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
        }
        if ((update_mask & SR_V) && 0 == (opcode2 & (1 << 10))) {
            ptr = EMIT_ClearFlags(ptr, cc, SR_V);
#ifdef __aarch64__
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            *ptr++ = cmn_reg(reg_dl, 31, LSL, 0);
            *ptr++ = csetm(tmp, A64_CC_MI);
            *ptr++ = cmp64_reg(tmp, reg_dl, LSR, 32);
            RA_FreeARMRegister(&ptr, tmp);
#else
            if (opcode2 & (1 << 10))
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
                RA_FreeARMRegister(&ptr, tmp);
            }
            else
            {
                *ptr++ = cmp_immed(reg_dh, 0);
            }
#endif
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_NE);
        }
    }

    RA_FreeARMRegister(&ptr, reg_dh);

    return ptr;
}

uint32_t *EMIT_DIVS_W(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t reg_a = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    uint8_t reg_q = 0xff;
    uint8_t reg_quot = RA_AllocARMRegister(&ptr);
    uint8_t reg_rem = RA_AllocARMRegister(&ptr);
    uint8_t ext_words = 0;

    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &reg_q, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);
    ptr = EMIT_FlushPC(ptr);
    RA_GetCC(&ptr);

#ifdef __aarch64__
    *ptr++ = ands_immed(31, reg_q, 16, 0);
    uint32_t *tmp_ptr = ptr;
    *ptr++ = b_cc(A64_CC_NE, 2);

    if (1)
    {
        /*
            This is a point of no return. Issue division by zero exception here
        */
        *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_words + 1));

        ptr = EMIT_Exception(ptr, VECTOR_DIVIDE_BY_ZERO, 2, (uint32_t)(intptr_t)(*m68k_ptr - 1));

        RA_StoreDirtyFPURegs(&ptr);
        RA_StoreDirtyM68kRegs(&ptr);

        RA_StoreCC(&ptr);
        RA_StoreFPCR(&ptr);
        RA_StoreFPSR(&ptr);
        
#if EMU68_INSN_COUNTER
        extern uint32_t insn_count;
        uint8_t ctx_free = 0;
        uint8_t ctx = RA_TryCTX(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        if (ctx == 0xff)
        {
            ctx = RA_AllocARMRegister(&ptr);
            *ptr++ = mrs(ctx, 3, 3, 13, 0, 3);
            ctx_free = 1;
        }
        *ptr++ = ldr64_offset(ctx, tmp, __builtin_offsetof(struct M68KState, INSN_COUNT));
        *ptr++ = add64_immed(tmp, tmp, insn_count & 0xfff);
        if (insn_count & 0xfff000)
            *ptr++ = adds64_immed_lsl12(tmp, tmp, insn_count >> 12);
        *ptr++ = str64_offset(ctx, tmp, __builtin_offsetof(struct M68KState, INSN_COUNT));

        RA_FreeARMRegister(&ptr, tmp);
        if (ctx_free)
            RA_FreeARMRegister(&ptr, ctx);
#endif
        /* Return here */
        *ptr++ = bx_lr();
    }
    /* Update branch to the continuation */
    *tmp_ptr = b_cc(A64_CC_NE, ptr - tmp_ptr);
#else
    *ptr++ = cmp_immed(reg_q, 0);
    *ptr++ = b_cc(ARM_CC_NE, 0);
    /* At this place handle exception - division by zero! */
    *ptr++ = udf(0);
#endif


#ifdef __aarch64__
    *ptr++ = sxth(reg_rem, reg_q);
    *ptr++ = sdiv(reg_quot, reg_a, reg_rem);
    *ptr++ = msub(reg_rem, reg_a, reg_quot, reg_rem);
#else
    if (Features.ARM_SUPPORTS_DIV)
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
#endif

#ifdef __aarch64__
    uint8_t tmp = RA_AllocARMRegister(&ptr);

    *ptr++ = sxth(tmp, reg_quot);
    *ptr++ = cmp_reg(tmp, reg_quot, LSL, 0);

    RA_FreeARMRegister(&ptr, tmp);
#else
    /* Test bit 15 of quotient */
    *ptr++ = tst_immed(reg_quot, 0x902);

    /* Sign-extract upper 16 bits of quotient into temporary register */
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    *ptr++ = sxth(tmp, reg_quot, 2);
    /* If bit 15 of quotient was set, increase extracted 16 bits, should advance to 0 */
    *ptr++ = add_cc_immed(ARM_CC_NE, tmp, tmp, 1);
    *ptr++ = cmp_immed(tmp, 0);
    RA_FreeARMRegister(&ptr, tmp);
#endif

    (*m68k_ptr) += ext_words;

    RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);

    /* if temporary register was 0 the division was successful, otherwise overflow occured! */
    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);

        ptr = EMIT_ClearFlags(ptr, cc, update_mask);

        if (update_mask & SR_V) {
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_NE);
        }
        if (update_mask & (SR_Z | SR_N))
        {
#ifdef __aarch64__
            *ptr++ = cmn_reg(31, reg_quot, LSL, 16);
#else
            *ptr++ = cmp_immed(reg_quot, 0);
#endif
            ptr = EMIT_GetNZxx(ptr, cc, &update_mask);
            if (update_mask & SR_Z) {
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            }
            if (update_mask & SR_N) {
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            }
        }
    }

#ifdef __aarch64__
    if (update_mask & SR_V) {
        uint8_t cc = RA_GetCC(&ptr);
        *ptr++ = tbnz(cc, SRB_V, 3);
    }
    else {
        *ptr++ = b_cc(A64_CC_NE, 3);
    }
#endif

    /* Move signed 16-bit quotient to lower 16 bits of target register, signed 16 bit reminder to upper 16 bits */
    *ptr++ = mov_reg(reg_a, reg_quot);
    *ptr++ = bfi(reg_a, reg_rem, 16, 16);

    /* Advance PC */
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));

    RA_FreeARMRegister(&ptr, reg_a);
    RA_FreeARMRegister(&ptr, reg_q);
    RA_FreeARMRegister(&ptr, reg_quot);
    RA_FreeARMRegister(&ptr, reg_rem);

#ifndef __aarch64__
    if (!Features.ARM_SUPPORTS_DIV)
        *ptr++ = INSN_TO_LE(0xfffffff0);
#endif

    return ptr;
}

uint32_t *EMIT_DIVU_W(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t reg_a = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    uint8_t reg_q = 0xff;
    uint8_t reg_quot = RA_AllocARMRegister(&ptr);
    uint8_t reg_rem = RA_AllocARMRegister(&ptr);
    uint8_t ext_words = 0;

    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &reg_q, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);
    ptr = EMIT_FlushPC(ptr);
    RA_GetCC(&ptr);

#ifdef __aarch64__
    *ptr++ = ands_immed(31, reg_q, 16, 0);
    uint32_t *tmp_ptr = ptr;
    *ptr++ = b_cc(A64_CC_NE, 2);

    if (1)
    {
        /*
            This is a point of no return. Issue division by zero exception here
        */
        *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_words + 1));

        ptr = EMIT_Exception(ptr, VECTOR_DIVIDE_BY_ZERO, 2, (uint32_t)(intptr_t)(*m68k_ptr - 1));

        RA_StoreDirtyFPURegs(&ptr);
        RA_StoreDirtyM68kRegs(&ptr);

        RA_StoreCC(&ptr);
        RA_StoreFPCR(&ptr);
        RA_StoreFPSR(&ptr);
        
#if EMU68_INSN_COUNTER
        extern uint32_t insn_count;
        uint8_t ctx_free = 0;
        uint8_t ctx = RA_TryCTX(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        if (ctx == 0xff)
        {
            ctx = RA_AllocARMRegister(&ptr);
            *ptr++ = mrs(ctx, 3, 3, 13, 0, 3);
            ctx_free = 1;
        }
        *ptr++ = ldr64_offset(ctx, tmp, __builtin_offsetof(struct M68KState, INSN_COUNT));
        *ptr++ = add64_immed(tmp, tmp, insn_count & 0xfff);
        if (insn_count & 0xfff000)
            *ptr++ = adds64_immed_lsl12(tmp, tmp, insn_count >> 12);
        *ptr++ = str64_offset(ctx, tmp, __builtin_offsetof(struct M68KState, INSN_COUNT));

        RA_FreeARMRegister(&ptr, tmp);
        if (ctx_free)
            RA_FreeARMRegister(&ptr, ctx);
#endif
        /* Return here */
        *ptr++ = bx_lr();
    }
    /* Update branch to the continuation */
    *tmp_ptr = b_cc(A64_CC_NE, ptr - tmp_ptr);
#else
    *ptr++ = cmp_immed(reg_q, 0);
    *ptr++ = b_cc(ARM_CC_NE, 0);
    /* At this place handle exception - division by zero! */
    *ptr++ = udf(0);
#endif

#ifdef __aarch64__
    *ptr++ = uxth(reg_rem, reg_q);
    *ptr++ = udiv(reg_quot, reg_a, reg_rem);
    *ptr++ = msub(reg_rem, reg_a, reg_quot, reg_rem);
#else
    if (Features.ARM_SUPPORTS_DIV)
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
#endif

#ifdef __aarch64__
    uint8_t tmp = RA_AllocARMRegister(&ptr);

    *ptr++ = uxth(tmp, reg_quot);
    *ptr++ = cmp_reg(tmp, reg_quot, LSL, 0);

    RA_FreeARMRegister(&ptr, tmp);
#else
    /* Extract upper 16 bits of quotient into temporary register */
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    *ptr++ = uxth(tmp, reg_quot, 2);
    *ptr++ = cmp_immed(tmp, 0);
    RA_FreeARMRegister(&ptr, tmp);
#endif

    (*m68k_ptr) += ext_words;

    RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);

    /* if temporary register was 0 the division was successful, otherwise overflow occured! */
    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);

        ptr = EMIT_ClearFlags(ptr, cc, update_mask);

        if (update_mask & SR_V) {
            
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_NE);
        }

        if (update_mask & (SR_Z | SR_N))
        {
#ifdef __aarch64__
            *ptr++ = cmn_reg(31, reg_quot, LSL, 16);
#else
            *ptr++ = cmp_immed(reg_quot, 0);
#endif
            ptr = EMIT_GetNZxx(ptr, cc, &update_mask);
            if (update_mask & SR_Z) {
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            }
            if (update_mask & SR_N) {
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            }
        }
    }

#ifdef __aarch64__
    if (update_mask & SR_V) {
        uint8_t cc = RA_GetCC(&ptr);
        *ptr++ = tbnz(cc, SRB_V, 3);
    }
    else {
        *ptr++ = b_cc(A64_CC_NE, 3);
    }
#endif

    /* Move unsigned 16-bit quotient to lower 16 bits of target register, unsigned 16 bit reminder to upper 16 bits */
    *ptr++ = mov_reg(reg_a, reg_quot);
    *ptr++ = bfi(reg_a, reg_rem, 16, 16);

    /* Advance PC */
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));

    RA_FreeARMRegister(&ptr, reg_a);
    RA_FreeARMRegister(&ptr, reg_q);
    RA_FreeARMRegister(&ptr, reg_quot);
    RA_FreeARMRegister(&ptr, reg_rem);

#ifndef __aarch64__
    if (!Features.ARM_SUPPORTS_DIV)
        *ptr++ = INSN_TO_LE(0xfffffff0);
#endif

    return ptr;
}

uint32_t *EMIT_DIVUS_L(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
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
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &reg_q, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
    ptr = EMIT_FlushPC(ptr);
    RA_GetCC(&ptr);

    // Check if division by 0
#ifdef __aarch64__
    uint32_t *tmp_ptr = ptr;
    *ptr++ = cbnz(reg_q, 2);

    if (1)
    {
        /*
            This is a point of no return. Issue division by zero exception here
        */
        *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_words + 1));

        ptr = EMIT_Exception(ptr, VECTOR_DIVIDE_BY_ZERO, 2, (uint32_t)(intptr_t)(*m68k_ptr - 1));

        RA_StoreDirtyFPURegs(&ptr);
        RA_StoreDirtyM68kRegs(&ptr);

        RA_StoreCC(&ptr);
        RA_StoreFPCR(&ptr);
        RA_StoreFPSR(&ptr);
        
#if EMU68_INSN_COUNTER
        extern uint32_t insn_count;
        uint8_t ctx_free = 0;
        uint8_t ctx = RA_TryCTX(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        if (ctx == 0xff)
        {
            ctx = RA_AllocARMRegister(&ptr);
            *ptr++ = mrs(ctx, 3, 3, 13, 0, 3);
            ctx_free = 1;
        }
        *ptr++ = ldr64_offset(ctx, tmp, __builtin_offsetof(struct M68KState, INSN_COUNT));
        *ptr++ = add64_immed(tmp, tmp, insn_count & 0xfff);
        if (insn_count & 0xfff000)
            *ptr++ = adds64_immed_lsl12(tmp, tmp, insn_count >> 12);
        *ptr++ = str64_offset(ctx, tmp, __builtin_offsetof(struct M68KState, INSN_COUNT));

        RA_FreeARMRegister(&ptr, tmp);
        if (ctx_free)
            RA_FreeARMRegister(&ptr, ctx);
#endif
        /* Return here */
        *ptr++ = bx_lr();
    }
    /* Update branch to the continuation */
    *tmp_ptr = cbnz(reg_q, ptr - tmp_ptr);
#else
    *ptr++ = cmp_immed(reg_q, 0);
    *ptr++ = b_cc(ARM_CC_NE, 0);
    /* At this place handle exception - division by zero! */
    *ptr++ = udf(0);
#endif


#ifdef __aarch64__
    if (div64)
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        *ptr++ = bfi64(reg_dq, reg_dr, 32, 32);
        if (reg_dr == 0xff)
        {
            if (sig)
            {
                *ptr++ = sdiv64(reg_dq, reg_dq, reg_q);
            }
            else
            {
                *ptr++ = udiv64(reg_dq, reg_dq, reg_q);
            }
        }
        else
        {
            if (sig)
            {
                *ptr++ = sdiv64(tmp, reg_dq, reg_q);
            }
            else
            {
                *ptr++ = udiv64(tmp, reg_dq, reg_q);
            }

            *ptr++ = msub64(reg_dr, reg_dq, tmp, reg_q);
            *ptr++ = mov_reg(reg_dq, tmp);
        }

        *ptr++ = sxtw64(tmp, reg_dq);
        *ptr++ = cmp64_reg(tmp, reg_dq, LSL, 0);

        RA_FreeARMRegister(&ptr, tmp);
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

            *ptr++ = msub(reg_dr, reg_dq, tmp, reg_q);
            *ptr++ = mov_reg(reg_dq, tmp);

            RA_FreeARMRegister(&ptr, tmp);
        }
    }
#else
    if (Features.ARM_SUPPORTS_DIV)
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

            kprintf("DIV%c_L 64/32->32:32\n", sig ? 'S':'U');

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
    kprintf("64 bit division not done yet!\n");
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
#endif

    (*m68k_ptr) += ext_words;

    /* Set Dq dirty */
    RA_SetDirtyM68kRegister(&ptr, (opcode2 >> 12) & 7);
    /* Set Dr dirty if it was used/changed */
    if (reg_dr != 0xff)
        RA_SetDirtyM68kRegister(&ptr, opcode2 & 7);

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        if (div64) {
            if (update_mask & SR_V) {
                ptr = EMIT_ClearFlags(ptr, cc, SR_V | SR_C);
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_NE);
            }
        }
        if (update_mask & (SR_C | SR_N))
        {
#ifdef __aarch64__
            *ptr++ = cmn_reg(31, reg_dq, LSL, 0);
#else
            *ptr++ = cmp_immed(reg_dq, 0);
#endif
            ptr = EMIT_GetNZxx(ptr, cc, &update_mask);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        }
    }

    /* Advance PC */
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));

    RA_FreeARMRegister(&ptr, reg_q);
    RA_FreeARMRegister(&ptr, reg_dq);
    if (reg_dr != 0xff)
        RA_FreeARMRegister(&ptr, reg_dr);

#ifndef __aarch64__
    if (!Features.ARM_SUPPORTS_DIV)
        *ptr++ = INSN_TO_LE(0xfffffff0);
#endif

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
