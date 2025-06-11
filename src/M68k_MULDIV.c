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
#include "cache.h"

uint32_t EMIT_MULU(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_MUL_DIV")));
uint32_t EMIT_MULS(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_MUL_DIV")));

uint32_t EMIT_MULS_W(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t reg;
    uint8_t src = 0xff;
    uint8_t ext_words = 0;

    // Fetch 16-bit register: source and destination
    reg = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
    RA_SetDirtyM68kRegister(ctx, (opcode >> 9) & 7);

    // Fetch 16-bit multiplicant
    EMIT_LoadFromEffectiveAddress(ctx, 0x80 | 2, &src, opcode & 0x3f, &ext_words, 0, NULL);

    // Sign-extend 16-bit multiplicants
    EMIT(ctx, 
        sxth(reg, reg),
        mul(reg, reg, src)
    );

    RA_FreeARMRegister(ctx, src);

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);

        EMIT(ctx, cmn_reg(31, reg, LSL, 0));

        EMIT_GetNZ00(ctx, cc, &update_mask);
        if (update_mask & SR_Z) {
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
        }
        if (update_mask & SR_N) {
            EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
        }
    }

    return 1;
}

uint32_t EMIT_MULU_W(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t reg;
    uint8_t src = 0xff;
    uint8_t ext_words = 0;

    // Fetch 16-bit register: source and destination
    reg = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
    RA_SetDirtyM68kRegister(ctx, (opcode >> 9) & 7);

    // Fetch 16-bit multiplicant
    EMIT_LoadFromEffectiveAddress(ctx, 2, &src, opcode & 0x3f, &ext_words, 1, NULL);

    /* extension of source needed only in case of Dn source */
    if ((opcode & 0x38) == 0) {
        uint8_t tmp = RA_AllocARMRegister(ctx);

        EMIT(ctx, 
            uxth(tmp, src),
            uxth(reg, reg),
            mul(reg, reg, tmp)
        );

        RA_FreeARMRegister(ctx, tmp);
    }
    else {
        EMIT(ctx, 
            uxth(reg, reg),
            mul(reg, reg, src)
        );
    }

    RA_FreeARMRegister(ctx, src);

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);

        EMIT(ctx, cmn_reg(31, reg, LSL, 0));

        EMIT_GetNZ00(ctx, cc, &update_mask);
        if (update_mask & SR_Z) {
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
        }
        if (update_mask & SR_N) {
            EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
        }
    }

    return 1;
}

uint32_t EMIT_MULS_L(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t reg_dl;
    uint8_t reg_dh = 0xff;
    uint8_t src = 0xff;
    uint8_t ext_words = 1;
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);

    // Fetch 32-bit register: source and destination
    reg_dl = RA_MapM68kRegister(ctx, (opcode2 >> 12) & 7);
    RA_SetDirtyM68kRegister(ctx, (opcode2 >> 12) & 7);

    // Fetch 32-bit multiplicant
    EMIT_LoadFromEffectiveAddress(ctx, 4, &src, opcode & 0x3f, &ext_words, 1, NULL);

    if (opcode2 & (1 << 10))
    {
        reg_dh = RA_MapM68kRegisterForWrite(ctx, (opcode2 & 7));
    }
    else
    {
        reg_dh = RA_AllocARMRegister(ctx);
    }

    if (opcode2 & (1 << 11))
        EMIT(ctx, smull(reg_dl, reg_dl, src));
    else
        EMIT(ctx, umull(reg_dl, reg_dl, src));
    if (opcode2 & (1 << 10) && (reg_dh != reg_dl))
    {
        EMIT(ctx, add64_reg(reg_dh, 31, reg_dl, LSR, 32));
    }

    RA_FreeARMRegister(ctx, src);

    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));
    ctx->tc_M68kCodePtr += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);

        if (opcode2 & (1 << 10)) { 
            EMIT(ctx, cmn64_reg(31, reg_dl, LSL, 0));
        }
        else {
            EMIT(ctx, cmn_reg(31, reg_dl, LSL, 0));
        }

        uint8_t old_mask = update_mask & SR_V;
        EMIT_GetNZ00(ctx, cc, &update_mask);
        update_mask |= old_mask;

        if (update_mask & SR_Z) {
            EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
        }
        if (update_mask & SR_N) {
            EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
        }
        if ((update_mask & SR_V) && 0 == (opcode2 & (1 << 10))) {
            EMIT_ClearFlags(ctx, cc, SR_Valt);

            uint8_t tmp = RA_AllocARMRegister(ctx);
            /* If signed multiply check higher 32bit against 0 or -1. For unsigned multiply upper 32 bit must be zero */
            if (opcode2 & (1 << 11)) {
                EMIT(ctx, 
                    cmn_reg(reg_dl, 31, LSL, 0),
                    csetm(tmp, A64_CC_MI)
                );
            } else {
                EMIT(ctx, mov_immed_u16(tmp, 0, 0));
            }
            EMIT(ctx, cmp64_reg(tmp, reg_dl, LSR, 32));
            RA_FreeARMRegister(ctx, tmp);

            EMIT_SetFlagsConditional(ctx, cc, SR_Valt, ARM_CC_NE);
        }
    }

    RA_FreeARMRegister(ctx, reg_dh);

    return 1;
}

uint32_t EMIT_DIVS_W(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t reg_a = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
    uint8_t reg_q = 0xff;
    uint8_t reg_quot = RA_AllocARMRegister(ctx);
    uint8_t reg_rem = RA_AllocARMRegister(ctx);
    uint8_t ext_words = 0;

    EMIT_LoadFromEffectiveAddress(ctx, 0x80 | 2, &reg_q, opcode & 0x3f, &ext_words, 0, NULL);
    EMIT_FlushPC(ctx);
    RA_GetCC(ctx);

    EMIT(ctx, ands_immed(31, reg_q, 16, 0));
    uint32_t *tmp_ptr = ctx->tc_CodePtr++;

    if (1)
    {
        /*
            This is a point of no return. Issue division by zero exception here
        */
        EMIT(ctx, add_immed(REG_PC, REG_PC, 2 * (ext_words + 1)));

        EMIT_Exception(ctx, VECTOR_DIVIDE_BY_ZERO, 2, (uint32_t)(intptr_t)(ctx->tc_M68kCodePtr - 1));

        RA_StoreDirtyFPURegs(ctx);
        RA_StoreDirtyM68kRegs(ctx);

        RA_StoreCC(ctx);
        RA_StoreFPCR(ctx);
        RA_StoreFPSR(ctx);

#if EMU68_INSN_COUNTER        
        extern uint32_t insn_count;
        uint8_t tmp = RA_AllocARMRegister(ctx);
        EMIT(ctx, mov_immed_u16(tmp, insn_count & 0xffff, 0));
        if (insn_count & 0xffff0000) {
            EMIT(ctx, movk_immed_u16(tmp, insn_count >> 16, 1));
        }
        EMIT(ctx, 
            fmov_from_reg(0, tmp),
            vadd_2d(30, 30, 0)
        );
        
        RA_FreeARMRegister(ctx, tmp);
#endif

        /* Return here */
        EMIT(ctx, bx_lr());
    }
    /* Update branch to the continuation */
    *tmp_ptr = b_cc(A64_CC_NE, ctx->tc_CodePtr - tmp_ptr);

    EMIT(ctx, 
        sdiv(reg_quot, reg_a, reg_q),
        msub(reg_rem, reg_a, reg_quot, reg_q)
    );

    uint8_t tmp = RA_AllocARMRegister(ctx);

    EMIT(ctx, 
        sxth(tmp, reg_quot),
        cmp_reg(tmp, reg_quot, LSL, 0),

        b_cc(A64_CC_NE, 3),

        /* Move signed 16-bit quotient to lower 16 bits of target register, signed 16 bit reminder to upper 16 bits */
        mov_reg(reg_a, reg_quot),
        bfi(reg_a, reg_rem, 16, 16)
    );

    RA_FreeARMRegister(ctx, tmp);

    ctx->tc_M68kCodePtr += ext_words;

    RA_SetDirtyM68kRegister(ctx, (opcode >> 9) & 7);

    /* if temporary register was 0 the division was successful, otherwise overflow occured! */
    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);
        uint8_t alt_mask = update_mask;
        if ((alt_mask & 3) != 0 && (alt_mask & 3) < 3)
            alt_mask ^= 3;
        EMIT_ClearFlags(ctx, cc, alt_mask);

        if (update_mask & SR_V) {
            EMIT_SetFlagsConditional(ctx, cc, SR_Valt, ARM_CC_NE);
        }
        if (update_mask & (SR_Z | SR_N))
        {
            EMIT(ctx, cmn_reg(31, reg_quot, LSL, 16));

            EMIT_GetNZxx(ctx, cc, &update_mask);
            if (update_mask & SR_Z) {
                EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
            }
            if (update_mask & SR_N) {
                EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
            }
        }
    }

    /* Advance PC */
    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));

    RA_FreeARMRegister(ctx, reg_a);
    RA_FreeARMRegister(ctx, reg_q);
    RA_FreeARMRegister(ctx, reg_quot);
    RA_FreeARMRegister(ctx, reg_rem);

    return 1;
}

uint32_t EMIT_DIVU_W(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint8_t reg_a = RA_MapM68kRegister(ctx, (opcode >> 9) & 7);
    uint8_t reg_q = 0xff;
    uint8_t reg_quot = RA_AllocARMRegister(ctx);
    uint8_t reg_rem = RA_AllocARMRegister(ctx);
    uint8_t ext_words = 0;

    /* Promise read only here. If dealing with Dn in EA, it will be extended below */
    EMIT_LoadFromEffectiveAddress(ctx, 2, &reg_q, opcode & 0x3f, &ext_words, 1, NULL);
    EMIT_FlushPC(ctx);
    RA_GetCC(ctx);

    EMIT(ctx, ands_immed(31, reg_q, 16, 0));
    uint32_t *tmp_ptr = ctx->tc_CodePtr++;

    if (1)
    {
        /*
            This is a point of no return. Issue division by zero exception here
        */
        EMIT(ctx, add_immed(REG_PC, REG_PC, 2 * (ext_words + 1)));

        EMIT_Exception(ctx, VECTOR_DIVIDE_BY_ZERO, 2, (uint32_t)(intptr_t)(ctx->tc_M68kCodePtr - 1));

        RA_StoreDirtyFPURegs(ctx);
        RA_StoreDirtyM68kRegs(ctx);

        RA_StoreCC(ctx);
        RA_StoreFPCR(ctx);
        RA_StoreFPSR(ctx);
        
#if EMU68_INSN_COUNTER        
        extern uint32_t insn_count;
        uint8_t tmp = RA_AllocARMRegister(ctx);
        EMIT(ctx, mov_immed_u16(tmp, insn_count & 0xffff, 0));
        if (insn_count & 0xffff0000) {
            EMIT(ctx, movk_immed_u16(tmp, insn_count >> 16, 1));
        }
        EMIT(ctx, 
            fmov_from_reg(0, tmp),
            vadd_2d(30, 30, 0)
        );
        
        RA_FreeARMRegister(ctx, tmp);
#endif
        /* Return here */
        EMIT(ctx, bx_lr());
    }
    /* Update branch to the continuation */
    *tmp_ptr = b_cc(A64_CC_NE, ctx->tc_CodePtr - tmp_ptr);

    /* If Dn was souce operant, extend it to 32bit, otherwise it is already in correct form */
    if ((opcode & 0x38) == 0) {
        EMIT(ctx, 
            uxth(reg_rem, reg_q),
            udiv(reg_quot, reg_a, reg_rem),
            msub(reg_rem, reg_a, reg_quot, reg_rem)
        );
    }
    else {
        EMIT(ctx, 
            udiv(reg_quot, reg_a, reg_q),
            msub(reg_rem, reg_a, reg_quot, reg_q)
        );
    }
        
    uint8_t tmp = RA_AllocARMRegister(ctx);

    EMIT(ctx, 
        uxth(tmp, reg_quot),
        cmp_reg(tmp, reg_quot, LSL, 0),

        b_cc(A64_CC_NE, 3),

        /* Move unsigned 16-bit quotient to lower 16 bits of target register, unsigned 16 bit reminder to upper 16 bits */
        mov_reg(reg_a, reg_quot),
        bfi(reg_a, reg_rem, 16, 16)
    );

    RA_FreeARMRegister(ctx, tmp);

    ctx->tc_M68kCodePtr += ext_words;

    RA_SetDirtyM68kRegister(ctx, (opcode >> 9) & 7);

    /* if temporary register was 0 the division was successful, otherwise overflow occured! */
    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);
        uint8_t alt_mask = update_mask;
        if ((alt_mask & 3) != 0 && (alt_mask & 3) < 3)
            alt_mask ^= 3;
        EMIT_ClearFlags(ctx, cc, alt_mask);

        if (update_mask & SR_V) {
            
            EMIT_SetFlagsConditional(ctx, cc, SR_Valt, ARM_CC_NE);
        }

        if (update_mask & (SR_Z | SR_N))
        {
            EMIT(ctx, cmn_reg(31, reg_quot, LSL, 16));

            EMIT_GetNZxx(ctx, cc, &update_mask);
            if (update_mask & SR_Z) {
                EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
            }
            if (update_mask & SR_N) {
                EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
            }
        }
    }

    /* Advance PC */
    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));

    RA_FreeARMRegister(ctx, reg_a);
    RA_FreeARMRegister(ctx, reg_q);
    RA_FreeARMRegister(ctx, reg_quot);
    RA_FreeARMRegister(ctx, reg_rem);

    return 1;
}

uint32_t EMIT_DIVUS_L(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
    uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    uint8_t sig = (opcode2 & (1 << 11)) != 0;
    uint8_t div64 = (opcode2 & (1 << 10)) != 0;
    uint8_t reg_q = 0xff;
    uint8_t reg_dq = RA_MapM68kRegister(ctx, (opcode2 >> 12) & 7);
    uint8_t reg_dr = RA_MapM68kRegister(ctx, opcode2 & 7);
    uint8_t ext_words = 1;

    // Load divisor
    EMIT_LoadFromEffectiveAddress(ctx, 4, &reg_q, opcode & 0x3f, &ext_words, 1, NULL);
    EMIT_FlushPC(ctx);
    RA_GetCC(ctx);

    // Check if division by 0
    uint32_t *tmp_ptr = ctx->tc_CodePtr++;

    if (1)
    {
        /*
            This is a point of no return. Issue division by zero exception here
        */
        EMIT(ctx, add_immed(REG_PC, REG_PC, 2 * (ext_words + 1)));

        EMIT_Exception(ctx, VECTOR_DIVIDE_BY_ZERO, 2, (uint32_t)(intptr_t)(ctx->tc_M68kCodePtr - 1));

        RA_StoreDirtyFPURegs(ctx);
        RA_StoreDirtyM68kRegs(ctx);

        RA_StoreCC(ctx);
        RA_StoreFPCR(ctx);
        RA_StoreFPSR(ctx);
        
#if EMU68_INSN_COUNTER        
        extern uint32_t insn_count;
        uint8_t tmp = RA_AllocARMRegister(ctx);
        EMIT(ctx, mov_immed_u16(tmp, insn_count & 0xffff, 0));
        if (insn_count & 0xffff0000) {
            EMIT(ctx, movk_immed_u16(tmp, insn_count >> 16, 1));
        }
        EMIT(ctx, 
            fmov_from_reg(0, tmp),
            vadd_2d(30, 30, 0)
        );
        
        RA_FreeARMRegister(ctx, tmp);
#endif
        /* Return here */
        EMIT(ctx, bx_lr());
    }
    /* Update branch to the continuation */
    *tmp_ptr = cbnz(reg_q, ctx->tc_CodePtr - tmp_ptr);

    if (div64)
    {
        uint8_t tmp = RA_AllocARMRegister(ctx);
        uint8_t result = RA_AllocARMRegister(ctx);
        uint8_t tmp2 = RA_AllocARMRegister(ctx);

        // Use temporary result - in case of overflow destination regs remain unchanged
        EMIT(ctx, 
            mov_reg(tmp2, reg_dq),
            bfi64(tmp2, reg_dr, 32, 32)
        );

        if (sig)
        {
            uint8_t q_ext = RA_AllocARMRegister(ctx);
            EMIT(ctx, 
                sxtw64(q_ext, reg_q),
                sdiv64(result, tmp2, q_ext)
            );
            if (reg_dr != reg_dq)
                EMIT(ctx, msub64(tmp, tmp2, result, q_ext));
            RA_FreeARMRegister(ctx, q_ext);
        }
        else
        {
            EMIT(ctx, udiv64(result, tmp2, reg_q));
            if (reg_dr != reg_dq)
                EMIT(ctx, msub64(tmp, tmp2, result, reg_q));
        }

        if (sig) {
            EMIT(ctx, sxtw64(tmp2, result));
        }
        else {
            EMIT(ctx, mov_reg(tmp2, result));
        }
        EMIT(ctx, cmp64_reg(tmp2, result, LSL, 0));

        tmp_ptr = ctx->tc_CodePtr++;

        EMIT(ctx, mov_reg(reg_dq, result));
        if (reg_dr != reg_dq) {
            EMIT(ctx, mov_reg(reg_dr, tmp));
        }

        *tmp_ptr = b_cc(A64_CC_NE, ctx->tc_CodePtr - tmp_ptr);

        RA_FreeARMRegister(ctx, tmp);
        RA_FreeARMRegister(ctx, tmp2);
        RA_FreeARMRegister(ctx, result);
    }
    else
    {
        if (reg_dr == reg_dq)
        {
            if (sig)
                EMIT(ctx, sdiv(reg_dq, reg_dq, reg_q));
            else
                EMIT(ctx, udiv(reg_dq, reg_dq, reg_q));
        }
        else
        {
            uint8_t tmp = RA_AllocARMRegister(ctx);

            if (sig)
                EMIT(ctx, sdiv(tmp, reg_dq, reg_q));
            else
                EMIT(ctx, udiv(tmp, reg_dq, reg_q));

            EMIT(ctx, 
                msub(reg_dr, reg_dq, tmp, reg_q),
                mov_reg(reg_dq, tmp)
            );

            RA_FreeARMRegister(ctx, tmp);
        }
    }

    ctx->tc_M68kCodePtr += ext_words;

    /* Set Dq dirty */
    RA_SetDirtyM68kRegister(ctx, (opcode2 >> 12) & 7);
    /* Set Dr dirty if it was used/changed */
    if (reg_dr != 0xff)
        RA_SetDirtyM68kRegister(ctx, opcode2 & 7);

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);
        if (update_mask & SR_VC) {
            EMIT_ClearFlags(ctx, cc, SR_Valt | SR_Calt);
            if (div64) {
                EMIT_SetFlagsConditional(ctx, cc, SR_Valt, ARM_CC_NE);
            }
        }
        if (update_mask & (SR_Z | SR_N))
        {
            EMIT(ctx, cmn_reg(31, reg_dq, LSL, 0));

            if ((update_mask & (SR_Z | SR_N)) == SR_Z) {
                EMIT(ctx, bic_immed(cc, cc, 1, (32 - SRB_Z)));
                EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
            }
            else if ((update_mask & (SR_Z | SR_N)) == SR_N) {
                EMIT(ctx, bic_immed(cc, cc, 1, (32 - SRB_N)));
                EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
            }
            else {
                EMIT_GetNZxx(ctx, cc, &update_mask);
            }
        }
    }

    /* Advance PC */
    EMIT_AdvancePC(ctx, 2 * (ext_words + 1));

    RA_FreeARMRegister(ctx, reg_q);
    RA_FreeARMRegister(ctx, reg_dq);
    if (reg_dr != 0xff)
        RA_FreeARMRegister(ctx, reg_dr);

    return 1;
}

uint32_t EMIT_MUL_DIV(struct TranslatorContext *ctx, uint16_t opcode)
{
    if ((opcode & 0xf1c0) == 0xc1c0)
    {
        return EMIT_MULS_W(ctx, opcode);
    }
    if ((opcode & 0xf1c0) == 0xc0c0)
    {
        return EMIT_MULU_W(ctx, opcode);
    }
    if ((opcode & 0xffc0) == 0x4c00)
    {
        return EMIT_MULS_L(ctx, opcode);
    }
    if ((opcode & 0xffc0) == 0x4c40)
    {
        return EMIT_DIVUS_L(ctx, opcode);
    }
    if ((opcode & 0xf1c0) == 0x81c0)
    {
        return EMIT_DIVS_W(ctx, opcode);
    }
    if ((opcode & 0xf1c0) == 0x80c0)
    {
        return EMIT_DIVU_W(ctx, opcode);
    }
    return 0;
}
