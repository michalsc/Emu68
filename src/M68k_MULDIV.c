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
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0x80 | 2, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

    // Sign-extend 16-bit multiplicants
    *ptr++ = sxth(reg, reg);
    *ptr++ = mul(reg, reg, src);

    RA_FreeARMRegister(&ptr, src);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);

        *ptr++ = cmn_reg(31, reg, LSL, 0);

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
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

    /* extension of source needed only in case of Dn source */
    if ((opcode & 0x38) == 0) {
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        *ptr++ = uxth(tmp, src);
        *ptr++ = uxth(reg, reg);
        *ptr++ = mul(reg, reg, tmp);

        RA_FreeARMRegister(&ptr, tmp);
    }
    else {
        *ptr++ = uxth(reg, reg);
        *ptr++ = mul(reg, reg, src);
    }

    RA_FreeARMRegister(&ptr, src);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);

        *ptr++ = cmn_reg(31, reg, LSL, 0);

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

    if (opcode2 & (1 << 11))
        *ptr++ = smull(reg_dl, reg_dl, src);
    else
        *ptr++ = umull(reg_dl, reg_dl, src);
    if (opcode2 & (1 << 10) && (reg_dh != reg_dl))
    {
        *ptr++ = add64_reg(reg_dh, 31, reg_dl, LSR, 32);
    }

    RA_FreeARMRegister(&ptr, src);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);

        if (opcode2 & (1 << 10)) { 
            *ptr++ = cmn64_reg(31, reg_dl, LSL, 0);
        }
        else {
            *ptr++ = cmn_reg(31, reg_dl, LSL, 0);
        }

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

            uint8_t tmp = RA_AllocARMRegister(&ptr);
            /* If signed multiply check higher 32bit against 0 or -1. For unsigned multiply upper 32 bit must be zero */
            if (opcode2 & (1 << 11)) {
                *ptr++ = cmn_reg(reg_dl, 31, LSL, 0);
                *ptr++ = csetm(tmp, A64_CC_MI);
            } else {
                *ptr++ = mov_immed_u16(tmp, 0, 0);
            }
            *ptr++ = cmp64_reg(tmp, reg_dl, LSR, 32);
            RA_FreeARMRegister(&ptr, tmp);

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

    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0x80 | 2, &reg_q, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);
    ptr = EMIT_FlushPC(ptr);
    RA_GetCC(&ptr);

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
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_immed_u16(tmp, insn_count & 0xffff, 0);
        if (insn_count & 0xffff0000) {
            *ptr++ = movk_immed_u16(tmp, insn_count >> 16, 1);
        }
        *ptr++ = fmov_from_reg(0, tmp);
        *ptr++ = vadd_2d(30, 30, 0);
        
        RA_FreeARMRegister(&ptr, tmp);
#endif

        /* Return here */
        *ptr++ = bx_lr();
    }
    /* Update branch to the continuation */
    *tmp_ptr = b_cc(A64_CC_NE, ptr - tmp_ptr);

    *ptr++ = sdiv(reg_quot, reg_a, reg_q);
    *ptr++ = msub(reg_rem, reg_a, reg_quot, reg_q);

    uint8_t tmp = RA_AllocARMRegister(&ptr);

    *ptr++ = sxth(tmp, reg_quot);
    *ptr++ = cmp_reg(tmp, reg_quot, LSL, 0);

    *ptr++ = b_cc(A64_CC_NE, 3);

    /* Move signed 16-bit quotient to lower 16 bits of target register, signed 16 bit reminder to upper 16 bits */
    *ptr++ = mov_reg(reg_a, reg_quot);
    *ptr++ = bfi(reg_a, reg_rem, 16, 16);

    RA_FreeARMRegister(&ptr, tmp);

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
            *ptr++ = cmn_reg(31, reg_quot, LSL, 16);

            ptr = EMIT_GetNZxx(ptr, cc, &update_mask);
            if (update_mask & SR_Z) {
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            }
            if (update_mask & SR_N) {
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            }
        }
    }

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
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t reg_a = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    uint8_t reg_q = 0xff;
    uint8_t reg_quot = RA_AllocARMRegister(&ptr);
    uint8_t reg_rem = RA_AllocARMRegister(&ptr);
    uint8_t ext_words = 0;

    /* Promise read only here. If dealing with Dn in EA, it will be extended below */
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &reg_q, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
    ptr = EMIT_FlushPC(ptr);
    RA_GetCC(&ptr);

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
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_immed_u16(tmp, insn_count & 0xffff, 0);
        if (insn_count & 0xffff0000) {
            *ptr++ = movk_immed_u16(tmp, insn_count >> 16, 1);
        }
        *ptr++ = fmov_from_reg(0, tmp);
        *ptr++ = vadd_2d(30, 30, 0);
        
        RA_FreeARMRegister(&ptr, tmp);
#endif
        /* Return here */
        *ptr++ = bx_lr();
    }
    /* Update branch to the continuation */
    *tmp_ptr = b_cc(A64_CC_NE, ptr - tmp_ptr);

    /* If Dn was souce operant, extend it to 32bit, otherwise it is already in correct form */
    if ((opcode & 0x38) == 0) {
        *ptr++ = uxth(reg_rem, reg_q);
        *ptr++ = udiv(reg_quot, reg_a, reg_rem);
        *ptr++ = msub(reg_rem, reg_a, reg_quot, reg_rem);
    }
    else {
        *ptr++ = udiv(reg_quot, reg_a, reg_q);
        *ptr++ = msub(reg_rem, reg_a, reg_quot, reg_q);
    }
        
    uint8_t tmp = RA_AllocARMRegister(&ptr);

    *ptr++ = uxth(tmp, reg_quot);
    *ptr++ = cmp_reg(tmp, reg_quot, LSL, 0);

    *ptr++ = b_cc(A64_CC_NE, 3);

    /* Move unsigned 16-bit quotient to lower 16 bits of target register, unsigned 16 bit reminder to upper 16 bits */
    *ptr++ = mov_reg(reg_a, reg_quot);
    *ptr++ = bfi(reg_a, reg_rem, 16, 16);

    RA_FreeARMRegister(&ptr, tmp);

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
            *ptr++ = cmn_reg(31, reg_quot, LSL, 16);

            ptr = EMIT_GetNZxx(ptr, cc, &update_mask);
            if (update_mask & SR_Z) {
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            }
            if (update_mask & SR_N) {
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            }
        }
    }

    /* Advance PC */
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));

    RA_FreeARMRegister(&ptr, reg_a);
    RA_FreeARMRegister(&ptr, reg_q);
    RA_FreeARMRegister(&ptr, reg_quot);
    RA_FreeARMRegister(&ptr, reg_rem);

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
    uint8_t reg_dr = RA_MapM68kRegister(&ptr, opcode2 & 7);
    uint8_t ext_words = 1;

    // Load divisor
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &reg_q, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
    ptr = EMIT_FlushPC(ptr);
    RA_GetCC(&ptr);

    // Check if division by 0
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
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_immed_u16(tmp, insn_count & 0xffff, 0);
        if (insn_count & 0xffff0000) {
            *ptr++ = movk_immed_u16(tmp, insn_count >> 16, 1);
        }
        *ptr++ = fmov_from_reg(0, tmp);
        *ptr++ = vadd_2d(30, 30, 0);
        
        RA_FreeARMRegister(&ptr, tmp);
#endif
        /* Return here */
        *ptr++ = bx_lr();
    }
    /* Update branch to the continuation */
    *tmp_ptr = cbnz(reg_q, ptr - tmp_ptr);

    if (div64)
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t result = RA_AllocARMRegister(&ptr);
        uint8_t tmp2 = RA_AllocARMRegister(&ptr);

        // Use temporary result - in case of overflow destination regs remain unchanged
        *ptr++ = mov_reg(tmp2, reg_dq);
        *ptr++ = bfi64(tmp2, reg_dr, 32, 32);

        if (sig)
        {
            uint8_t q_ext = RA_AllocARMRegister(&ptr);
            *ptr++ = sxtw64(q_ext, reg_q);
            *ptr++ = sdiv64(result, tmp2, q_ext);
            if (reg_dr != reg_dq)
                *ptr++ = msub64(tmp, tmp2, result, q_ext);
            RA_FreeARMRegister(&ptr, q_ext);
        }
        else
        {
            *ptr++ = udiv64(result, tmp2, reg_q);
            if (reg_dr != reg_dq)
                *ptr++ = msub64(tmp, tmp2, result, reg_q);
        }

        if (sig) {
            *ptr++ = sxtw64(tmp2, result);
        }
        else {
            *ptr++ = mov_reg(tmp2, result);
        }
        *ptr++ = cmp64_reg(tmp2, result, LSL, 0);

        tmp_ptr = ptr;
        *ptr++ = b_cc(A64_CC_NE, 0);

        *ptr++ = mov_reg(reg_dq, result);
        if (reg_dr != reg_dq) {
            *ptr++ = mov_reg(reg_dr, tmp);
        }

        *tmp_ptr = b_cc(A64_CC_NE, ptr - tmp_ptr);

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, tmp2);
        RA_FreeARMRegister(&ptr, result);
    }
    else
    {
        if (reg_dr == reg_dq)
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

    (*m68k_ptr) += ext_words;

    /* Set Dq dirty */
    RA_SetDirtyM68kRegister(&ptr, (opcode2 >> 12) & 7);
    /* Set Dr dirty if it was used/changed */
    if (reg_dr != 0xff)
        RA_SetDirtyM68kRegister(&ptr, opcode2 & 7);

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        if (update_mask & SR_VC) {
            ptr = EMIT_ClearFlags(ptr, cc, SR_V | SR_C);
            if (div64) {
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_NE);
            }
        }
        if (update_mask & (SR_Z | SR_N))
        {
            *ptr++ = cmn_reg(31, reg_dq, LSL, 0);

            if ((update_mask & (SR_Z | SR_N)) == SR_Z) {
                *ptr++ = bic_immed(cc, cc, 1, (32 - SRB_Z));
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            }
            else if ((update_mask & (SR_Z | SR_N)) == SR_N) {
                *ptr++ = bic_immed(cc, cc, 1, (32 - SRB_N));
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            }
            else {
                ptr = EMIT_GetNZxx(ptr, cc, &update_mask);
            }
        }
    }

    /* Advance PC */
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));

    RA_FreeARMRegister(&ptr, reg_q);
    RA_FreeARMRegister(&ptr, reg_dq);
    if (reg_dr != 0xff)
        RA_FreeARMRegister(&ptr, reg_dr);

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
