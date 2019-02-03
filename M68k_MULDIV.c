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

    *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_words + 1));
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

    *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_words + 1));
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

    *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_words + 1));
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

/* Used for 16-bit divisions */
void * ptr_uidivmod = &__aeabi_uidivmod;    /* In: r0, r1. Out: r0 = r0 / r1, r1 = r0 % r1 */
void * ptr_idivmod  = &__aeabi_idivmod;     /* In: r0, r1. Out: r0 = r0 / r1, r1 = r0 % r1 */

uint32_t *EMIT_DIVS_W(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t reg_a = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    uint8_t reg_q;
    uint8_t ext_words = 0;
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &reg_q, opcode & 0x3f, *m68k_ptr, &ext_words);

    uint8_t tmp0 = reg_a;
    uint8_t tmp1 = reg_q;
    uint8_t tmp2 = RA_AllocARMRegister(&ptr);
    uint8_t tmp3 = RA_AllocARMRegister(&ptr);
    uint8_t tmp4 = RA_AllocARMRegister(&ptr);
    uint8_t tmp5 = RA_AllocARMRegister(&ptr);
    uint8_t tmp6 = RA_AllocARMRegister(&ptr);
    uint8_t tmp7 = RA_AllocARMRegister(&ptr);

    /* At this point reg_a is divident, reg_b is divisor */
    *ptr++ = subs_immed(tmp6, reg_a, 0);
    *ptr++ = rsb_cc_immed(ARM_CC_LT, tmp4, tmp6, 0);
    *ptr++ = mov_cc_immed_u8(ARM_CC_LT, tmp7, 1);
    *ptr++ = mov_cc_reg(ARM_CC_GE, tmp4, tmp6);
    *ptr++ = mov_cc_immed_u8(ARM_CC_GE, tmp7, 0);
    *ptr++ = cmp_immed(reg_q, 0);
    *ptr++ = mov_reg(tmp5, reg_q);
    *ptr++ = rsb_cc_immed(ARM_CC_LT, tmp3, reg_q, 0);
    *ptr++ = sxth_cc(ARM_CC_LT, tmp3, tmp3, 0);
    *ptr++ = b_cc(ARM_CC_LT, 2);
    *ptr++ = b_cc(ARM_CC_NE, 0);
    *ptr++ = udf(0);
    *ptr++ = mov_reg(tmp3, tmp5);
    *ptr++ = cmp_reg(tmp3, tmp4);
    *ptr++ = b_cc(ARM_CC_LE, 1);   /* Exit */
    *ptr++ = lsl_immed(reg_a, tmp4, 16);
    *ptr++ = b_cc(ARM_CC_AL, 22);
    *ptr++ = clz(tmp2, tmp3);
    *ptr++ = mov_immed_u8(tmp0, 0);
    *ptr++ = clz(tmp1, tmp4);
    *ptr++ = sub_reg(tmp1, tmp2, tmp1, 0);
    *ptr++ = mov_immed_u8(tmp2, 1);
    *ptr++ = lsl_reg(tmp3, tmp3, tmp1);
    *ptr++ = lsl_reg(tmp2, tmp2, tmp1);
    *ptr++ = sxth(tmp3, tmp3, 0);

    *ptr++ = cmp_reg(tmp3, tmp4);
    *ptr++ = sub_cc_reg(ARM_CC_LE, tmp4, tmp4, tmp3, 0);
    *ptr++ = orr_cc_reg(ARM_CC_LE, tmp0, tmp0, tmp2, 0);
    *ptr++ = lsrs_immed(tmp2, tmp2, 1);
    *ptr++ = asr_immed(tmp3, tmp3, 1);
    *ptr++ = b_cc(ARM_CC_NE, -7);

    *ptr++ = cmp_immed(tmp7, 0);
    *ptr++ = rsb_cc_immed(ARM_CC_NE, tmp4, tmp4, 0);
    *ptr++ = eors_reg(tmp3, tmp6, tmp5, 16);
    *ptr++ = rsb_cc_immed(ARM_CC_MI, tmp0, tmp0, 0);
    *ptr++ = cmp_immed(tmp0, 0xa10);    // 65536
    *ptr++ = b_cc(ARM_CC_LT, 0);
    *ptr++ = udf(1);
    *ptr++ = uxth(tmp0, tmp0, 0);
    *ptr++ = orr_reg(tmp0, tmp0, tmp4, 16);

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
    if ((opcode & 0xf1c0) == 0x81c0)
    {
        ptr = EMIT_DIVS_W(ptr, opcode, m68k_ptr);
    }


    return ptr;
}
