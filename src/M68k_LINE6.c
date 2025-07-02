/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "config.h"
#include "support.h"
#include "M68k.h"
#include "RegisterAllocator.h"
#include "cache.h"

extern struct M68KState *__m68k_state;
extern uint16_t * m68k_entry_point;

uint32_t EMIT_BRA(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint8_t bsr = 0;
    uint8_t reg = RA_AllocARMRegister(ctx);
    int8_t addend = 0;
    uint16_t *bra_rel_ptr = ctx->tc_M68kCodePtr;
    int32_t bra_off = 0;

    int8_t current_pc_off = 2;
    int32_t abs_off = 0;
    EMIT_GetOffsetPC(ctx, &current_pc_off);
    EMIT_ResetOffsetPC(ctx);
    abs_off = current_pc_off;

    /* use 16-bit offset */
    if ((opcode & 0x00ff) == 0x00)
    {
        addend = 2;
        bra_off = (int16_t)(cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr++));
    }
    /* use 32-bit offset */
    else if ((opcode & 0x00ff) == 0xff)
    {
        addend = 4;
        bra_off = (int32_t)(cache_read_32(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr));
        ctx->tc_M68kCodePtr += 2;
    }
    else
    /* otherwise use 8-bit offset */
    {
        bra_off = (int8_t)(opcode & 0xff);
    }

    /* Check if INSN is BSR */
    if (opcode & 0x0100)
    {
        uint8_t sp = RA_MapM68kRegister(ctx, 15);
        RA_SetDirtyM68kRegister(ctx, 15);

        uint8_t tmp = RA_AllocARMRegister(ctx);
        if ((addend + abs_off) > 0 && (addend + abs_off) < 256)
            EMIT(ctx, add_immed(tmp, REG_PC, (addend + abs_off)));
        else if ((addend + abs_off) > -256 && (addend + abs_off) < 0)
            EMIT(ctx, sub_immed(tmp, REG_PC, -(addend + abs_off)));
        else if ((addend + abs_off) != 0) {
            int32_t v = addend + abs_off;
            EMIT(ctx, movw_immed_u16(tmp, v & 0xffff));
            if ((v >> 16) & 0xffff)
                EMIT(ctx, movt_immed_u16(tmp, v >> 16));
            EMIT(ctx, add_reg(tmp, REG_PC, tmp, LSL, 0));
        }

        if ((addend + abs_off))
            EMIT(ctx, str_offset_preindex(sp, tmp, -4));
        else
            EMIT(ctx, str_offset_preindex(sp, REG_PC, -4));

        bsr = 1;

        RA_FreeARMRegister(ctx, tmp);
    }

    abs_off += bra_off;

    if (abs_off > -4096 && abs_off < 4096)
    {
        if (abs_off > 0 && abs_off < 4096)
            EMIT(ctx, add_immed(REG_PC, REG_PC, abs_off));
        else if (abs_off > -4096 && abs_off < 0)
            EMIT(ctx, sub_immed(REG_PC, REG_PC, -abs_off));
    }
    else
    {
        EMIT(ctx, movw_immed_u16(reg, abs_off & 0xffff));
        if ((abs_off >> 16) & 0xffff)
            EMIT(ctx, movt_immed_u16(reg, abs_off >> 16));
        EMIT(ctx, add_reg(REG_PC, REG_PC, reg, LSL, 0));
    }
    RA_FreeARMRegister(ctx, reg);

    int32_t var_EMU68_BRANCH_INLINE_DISTANCE = (__m68k_state->JIT_CONTROL >> JCCB_INLINE_RANGE) & JCCB_INLINE_RANGE_MASK;

    /* If branch is done within +- 4KB, try to inline it instead of breaking up the translation unit */
    if ((uintptr_t)ctx->tc_M68kCodePtr >= 0x00f00000 && (bra_off >= -var_EMU68_BRANCH_INLINE_DISTANCE && bra_off <= var_EMU68_BRANCH_INLINE_DISTANCE)) {
        if (bsr) {
            M68K_PushReturnAddress(ctx->tc_M68kCodePtr);
        }

        ctx->tc_M68kCodePtr = (void *)((uintptr_t)bra_rel_ptr + bra_off);
    }
    else
        EMIT(ctx, INSN_TO_LE(0xffffffff));

    return 1;
}

uint32_t EMIT_BSR(struct TranslatorContext *ctx, uint16_t opcode) __attribute__((alias("EMIT_BRA")));

uint32_t EMIT_Bcc(struct TranslatorContext *ctx, uint16_t opcode)
{
    uint32_t *tmpptr;
    uint8_t m68k_condition = (opcode >> 8) & 15;
    intptr_t branch_target = (intptr_t)ctx->tc_M68kCodePtr;
    intptr_t branch_offset = 0;
    int8_t local_pc_off = 2;
    int take_branch = 1;

    EMIT_GetOffsetPC(ctx, &local_pc_off);
    EMIT_ResetOffsetPC(ctx);

    /* use 16-bit offset */
    if ((opcode & 0x00ff) == 0x00)
    {
        branch_offset = (int16_t)cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr++);
    }
    /* use 32-bit offset */
    else if ((opcode & 0x00ff) == 0xff)
    {
        uint16_t lo16, hi16;
        hi16 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr++);
        lo16 = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr++);
        branch_offset = lo16 | (hi16 << 16);
    }
    else
    /* otherwise use 8-bit offset */
    {
        branch_offset = (int8_t)(opcode & 0xff);
    }

    branch_offset += local_pc_off;
    branch_target += branch_offset - local_pc_off;

#if EMU68_DEF_BRANCH_AUTO
    /* Branch backward with distance up to EMU68_DEF_BRANCH_AUTO_RANGE bytes considered as taken */
    if(
#if 0
        branch_target < (intptr_t)ctx->tc_M68kCodePtr /* &&
        ((intptr_t)ctx->tc_M68kCodePtr - branch_target) < EMU68_DEF_BRANCH_AUTO_RANGE */
#else
        branch_offset - local_pc_off < 0
#endif
    )
        take_branch = 1;
//    else if (branch_offset - local_pc_off < 16)
//        take_branch = 1;
    else
        take_branch = 0;
#else
#if EMU68_DEF_BRANCH_TAKEN
    take_branch = 1;
#else
    take_branch = 0;
#endif
#endif

    if (take_branch)
    {
        m68k_condition ^= 1;
    }

    /* Force getting CC in place */
    RA_GetCC(ctx);

    /* Prepare fake jump on condition, assume def branch is taken */
    uint32_t fixup_type = 0;
    
    EMIT_JumpOnCondition(ctx, m68k_condition, 0, &fixup_type);
    tmpptr = ctx->tc_CodePtr - 1;

    /* Insert the branch non-taken case here */
    if (!take_branch)
    {
        intptr_t local_pc_off_16 = local_pc_off - 2;

        /* Adjust PC accordingly */
        if ((opcode & 0x00ff) == 0x00)
        {
            local_pc_off_16 += 4;
        }
        /* use 32-bit offset */
        else if ((opcode & 0x00ff) == 0xff)
        {
            local_pc_off_16 += 6;
        }
        else
        /* otherwise use 8-bit offset */
        {
            local_pc_off_16 += 2;
        }

        if (local_pc_off_16 > 0 && local_pc_off_16 < 255)
            EMIT(ctx, add_immed(REG_PC, REG_PC, local_pc_off_16));
        else if (local_pc_off_16 > -256 && local_pc_off_16 < 0)
            EMIT(ctx, sub_immed(REG_PC, REG_PC, -local_pc_off_16));
        else if (local_pc_off_16 != 0) {
            EMIT(ctx, movw_immed_u16(0, local_pc_off_16));
            if ((local_pc_off_16 >> 16) & 0xffff)
                EMIT(ctx, movt_immed_u16(0, local_pc_off_16 >> 16));
            EMIT(ctx, add_reg(REG_PC, REG_PC, 0, LSL, 0));
        }
    }
    else
    {
        if (branch_offset > 0 && branch_offset < 4096)
            EMIT(ctx, add_immed(REG_PC, REG_PC, branch_offset));
        else if (branch_offset > -4096 && branch_offset < 0)
            EMIT(ctx, sub_immed(REG_PC, REG_PC, -branch_offset));
        else if (branch_offset != 0) {
            EMIT(ctx, movw_immed_u16(0, branch_offset));
            if ((branch_offset >> 16) & 0xffff)
                EMIT(ctx, movt_immed_u16(0, (branch_offset >> 16) & 0xffff));
            EMIT(ctx, add_reg(REG_PC, REG_PC, 0, LSL, 0));
        }

        ctx->tc_M68kCodePtr = (uint16_t *)branch_target;
    }

    /* Now insert the branch taken case - this will be treated as exit code */
    uint32_t *exit_code_start = ctx->tc_CodePtr;

    /* Insert the first case here */
    if (take_branch)
    {
        intptr_t local_pc_off_16 = local_pc_off - 2;

        /* Adjust PC accordingly */
        if ((opcode & 0x00ff) == 0x00)
        {
            local_pc_off_16 += 4;
        }
        /* use 32-bit offset */
        else if ((opcode & 0x00ff) == 0xff)
        {
            local_pc_off_16 += 6;
        }
        else
        /* otherwise use 8-bit offset */
        {
            local_pc_off_16 += 2;
        }

        if (local_pc_off_16 > 0 && local_pc_off_16 < 255)
            EMIT(ctx, add_immed(REG_PC, REG_PC, local_pc_off_16));
        else if (local_pc_off_16 > -256 && local_pc_off_16 < 0)
            EMIT(ctx, sub_immed(REG_PC, REG_PC, -local_pc_off_16));
        else if (local_pc_off_16 != 0)
        {
            EMIT(ctx, movw_immed_u16(0, local_pc_off_16));
            if ((local_pc_off_16 >> 16) & 0xffff)
                EMIT(ctx, movt_immed_u16(0, local_pc_off_16 >> 16));
            EMIT(ctx, add_reg(REG_PC, REG_PC, 0, LSL, 0));
        }
    }
    else
    {
        if (branch_offset > 0 && branch_offset < 4096)
            EMIT(ctx, add_immed(REG_PC, REG_PC, branch_offset));
        else if (branch_offset > -4096 && branch_offset < 0)
            EMIT(ctx, sub_immed(REG_PC, REG_PC, -branch_offset));
        else if (branch_offset != 0)
        {
            EMIT(ctx, movw_immed_u16(0, branch_offset));
            if ((branch_offset >> 16) & 0xffff)
                EMIT(ctx, movt_immed_u16(0, (branch_offset >> 16) & 0xffff));
            EMIT(ctx, add_reg(REG_PC, REG_PC, 0, LSL, 0));
        }
    }

    /* Insert local exit */
    EMIT_LocalExit(ctx, 1);
    uint32_t *exit_code_end = ctx->tc_CodePtr;

    /* Insert fixup location */
    EMIT(ctx, 
        exit_code_end - tmpptr,
        fixup_type,
        exit_code_end - exit_code_start,
        INSN_TO_LE(MARKER_EXIT_BLOCK)
    );

    return 1;
}

static struct OpcodeDef InsnTable[16] = {
    [0]         = { EMIT_BRA, NULL, 0, 0, 0, 0, 0 },
    [1]         = { EMIT_BSR, NULL, 0, 0, 0, 0, 0 },
    [M_CC_HI]   = { EMIT_Bcc, NULL, SR_ZC, 0, 0, 0, 0 },
    [M_CC_LS]   = { EMIT_Bcc, NULL, SR_ZC, 0, 0, 0, 0 },
    [M_CC_CC]   = { EMIT_Bcc, NULL, SR_C, 0, 0, 0, 0 },
    [M_CC_CS]   = { EMIT_Bcc, NULL, SR_C, 0, 0, 0, 0 },
    [M_CC_NE]   = { EMIT_Bcc, NULL, SR_Z, 0, 0, 0, 0 },
    [M_CC_EQ]   = { EMIT_Bcc, NULL, SR_Z, 0, 0, 0, 0 },
    [M_CC_VC]   = { EMIT_Bcc, NULL, SR_V, 0, 0, 0, 0 },
    [M_CC_VS]   = { EMIT_Bcc, NULL, SR_V, 0, 0, 0, 0 },
    [M_CC_PL]   = { EMIT_Bcc, NULL, SR_N, 0, 0, 0, 0 },
    [M_CC_MI]   = { EMIT_Bcc, NULL, SR_N, 0, 0, 0, 0 },
    [M_CC_GE]   = { EMIT_Bcc, NULL, SR_NV, 0, 0, 0, 0 },
    [M_CC_LT]   = { EMIT_Bcc, NULL, SR_NV, 0, 0, 0, 0 },
    [M_CC_GT]   = { EMIT_Bcc, NULL, SR_NZV, 0, 0, 0, 0 },
    [M_CC_LE]   = { EMIT_Bcc, NULL, SR_NZV, 0, 0, 0, 0 }
};

uint32_t EMIT_line6(struct TranslatorContext *ctx)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);

    ctx->tc_M68kCodePtr++;

    return InsnTable[(opcode >> 8) & 15].od_Emit(ctx, opcode);
}

uint32_t GetSR_Line6(uint16_t opcode)
{
    return (InsnTable[(opcode >> 8) & 15].od_SRNeeds << 16) | InsnTable[(opcode >> 8) & 15].od_SRSets;
}

int M68K_GetLine6Length(uint16_t *insn_stream)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)insn_stream);
    int length = 1;
    
    if ((opcode & 0xff) == 0) {
        length = 2;
    }
    else if ((opcode & 0xff) == 0xff) {
        length = 3;
    }

    return length;
}