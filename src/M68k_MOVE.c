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
#include "cache.h"

uint32_t EMIT_moveq(struct TranslatorContext *ctx)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr);
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    int8_t value = opcode & 0xff;
    uint8_t reg = (opcode >> 9) & 7;
    uint8_t tmp_reg = RA_MapM68kRegisterForWrite(ctx, reg);
    uint32_t insn_consumed = 1;

    if (opcode & 0x100) {
        EMIT_FlushPC(ctx);
        EMIT_InjectDebugString(ctx, "[JIT] opcode %04x at %08x not implemented\n", opcode, ctx->tc_M68kCodePtr - 1);
        EMIT(ctx,
            svc(0x100),
            svc(0x101),
            svc(0x103),
            (uint32_t)(uintptr_t)(ctx->tc_M68kCodePtr - 8),
            48
        );
        EMIT_Exception(ctx, VECTOR_ILLEGAL_INSTRUCTION, 0);
        EMIT(ctx, INSN_TO_LE(0xffffffff));

        return insn_consumed;
    }

    ctx->tc_M68kCodePtr++;

    /* Special case which can be 0-cycle on A76 and above - load zero to register */
    if (value == 0)
        EMIT(ctx, mov_reg(tmp_reg, 31));
    else
        EMIT(ctx, mov_immed_s8(tmp_reg, value));

    EMIT_AdvancePC(ctx, 2);

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(ctx);
        uint8_t alt_mask = update_mask;
        if ((alt_mask & 3) != 0 && (alt_mask & 3) < 3)
            alt_mask ^= 3;
        EMIT_ClearFlags(ctx, cc, alt_mask);
        if (value <= 0) {
            if (value < 0)
                EMIT_SetFlags(ctx, cc, SR_N);
            else
                EMIT_SetFlags(ctx, cc, SR_Z);
        }
    }

    return insn_consumed;
}

uint32_t GetSR_Line7(uint16_t opcode)
{
    /* Line7 is moveq, if bit8 == 0, otherwise illegal */
    if (opcode & 0x100) {
        return SR_CCR << 16;    // Illegal, needs all CCR
    } else {
        return SR_NZVC;         // moveq needs none, sets NZVC
    }
}

uint32_t EMIT_move(struct TranslatorContext *ctx)
{
    uint8_t update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr);
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
    int move_length = M68K_GetINSNLength(ctx->tc_M68kCodePtr);
    uint8_t ext_count = 0;
    uint8_t tmp_reg = 0xff;
    uint8_t size = 1;
    uint8_t tmp = 0;
    uint8_t is_movea = (opcode & 0x01c0) == 0x0040;
    int is_load_immediate = 0;
    uint32_t immediate_value = 0;
    int loaded_in_dest = 0;
    uint32_t insn_consumed = 1;
    int done = 0;
    int fused_opcodes = 0;

    // Move from/to An in byte size is illegal
    if ((opcode & 0xf000) == 0x1000)
    {
        if (is_movea || (opcode & 0x38) == 0x08) {
            EMIT_FlushPC(ctx);
            EMIT_InjectDebugString(ctx, "[JIT] opcode %04x at %08x not implemented\n", opcode, ctx->tc_M68kCodePtr);
            EMIT(ctx,
                svc(0x100),
                svc(0x101),
                svc(0x103),
                (uint32_t)(uintptr_t)(ctx->tc_M68kCodePtr - 8),
                48
            );
            EMIT_Exception(ctx, VECTOR_ILLEGAL_INSTRUCTION, 0);
            EMIT(ctx, INSN_TO_LE(0xffffffff));
            return insn_consumed;
        }
    }

    if ((opcode & 0x3f) > 0x3c)
    {
        EMIT_FlushPC(ctx);
        EMIT_InjectDebugString(ctx, "[JIT] opcode %04x at %08x not implemented\n", opcode, ctx->tc_M68kCodePtr);
        EMIT(ctx, 
            svc(0x100),
            svc(0x101),
            svc(0x103),
            (uint32_t)(uintptr_t)(ctx->tc_M68kCodePtr - 8),
            48
        );
        EMIT_Exception(ctx, VECTOR_ILLEGAL_INSTRUCTION, 0);
        EMIT(ctx, INSN_TO_LE(0xffffffff));
        
        return insn_consumed;
    }

    if ((opcode & 0x01c0) == 0x01c0) {
        if ((opcode & 0x0c00)) {
            EMIT_FlushPC(ctx);
            EMIT_InjectDebugString(ctx, "[JIT] opcode %04x at %08x not implemented\n", opcode, ctx->tc_M68kCodePtr - 1);
            EMIT(ctx, 
                svc(0x100),
                svc(0x101),
                svc(0x103),
                (uint32_t)(uintptr_t)(ctx->tc_M68kCodePtr - 8),
                48
            );
            EMIT_Exception(ctx, VECTOR_ILLEGAL_INSTRUCTION, 0);
            EMIT(ctx, INSN_TO_LE(0xffffffff));
            
            return insn_consumed;
        }
    }

    /*
        Check if MOVE is qualified for merging:
        - move.l Reg, -(An)
        - move.l Reg, (An)+
        - move.l -(An), Reg
        - move.l (An)+, Reg
    */
    if ((opcode & 0xf000) == 0x2000)
    {
        // Fetch 2nd opcode just now
        uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[1]);

        // Is move.l Reg, -(An) ?: Dest mode 100, source mode 000 or 001
        if ((opcode & 0x01f0) == 0x0100)
        {
            // Candidate found. Is next opcode of same kind and same -(An)?
            if (
                (opcode2 & 0xf000) == 0x2000 &&          // move.l
                (opcode2 & 0x0ff0) == (opcode & 0x0ff0)  // same dest reg, same mode?
            )
            {
                uint8_t addr_reg = RA_MapM68kRegisterForWrite(ctx, ((opcode >> 9) & 7) + 8);
                uint8_t src_reg_1 = RA_MapM68kRegister(ctx, opcode & 0xf);
                uint8_t src_reg_2 = RA_MapM68kRegister(ctx, opcode2 & 0xf);

                /* Merging not allowed if any of the registers stored is also address register */
                if (!(src_reg_1 == addr_reg || src_reg_2 == addr_reg))
                {
                    /* Two subsequent register moves to -(An) */
                    ctx->tc_M68kCodePtr++;
                    update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr);
                    ctx->tc_M68kCodePtr++;

                    if (update_mask)
                    {
                        EMIT(ctx, cmn_reg(31, src_reg_2, LSL, 0));
                    }

                    EMIT(ctx, stp_preindex(addr_reg, src_reg_2, src_reg_1, -8));

                    tmp_reg = src_reg_2;
                
                    done = 1;
                    EMIT_AdvancePC(ctx, 4);
                    insn_consumed = 2;
                    size = 4;
                }
            }
        }

        // Is move.l Reg, (An)+ ?: Dest mode 011, source mode 000 or 001
        else if ((opcode & 0x01f0) == 0x00c0)
        {
            // Candidate found. Is next opcode of same kind and same (An)+?
            if (
                (opcode2 & 0xf000) == 0x2000 &&          // move.l
                (opcode2 & 0x0ff0) == (opcode & 0x0ff0)  // same dest reg, same mode?
            )
            {
                uint8_t addr_reg = RA_MapM68kRegisterForWrite(ctx, ((opcode >> 9) & 7) + 8);
                uint8_t src_reg_1 = RA_MapM68kRegister(ctx, opcode & 0xf);
                uint8_t src_reg_2 = RA_MapM68kRegister(ctx, opcode2 & 0xf);
                
                /* Merging not allowed if any of the registers stored is also address register */
                if (!(src_reg_1 == addr_reg || src_reg_2 == addr_reg))
                {
                    /* Two subsequent register moves to (An)+ */
                    ctx->tc_M68kCodePtr++;
                    update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr);
                    ctx->tc_M68kCodePtr++;

                    if (update_mask)
                    {
                        EMIT(ctx, cmn_reg(31, src_reg_2, LSL, 0));
                    }

                    EMIT(ctx, stp_postindex(addr_reg, src_reg_1, src_reg_2, 8));

                    tmp_reg = src_reg_2;
                
                    done = 1;
                    EMIT_AdvancePC(ctx, 4);
                    insn_consumed = 2;
                    size = 4;
                }
            }
        }

        // Is move.l (An)+, Reg ?: Dest mode 001 or 000, source mode 011
        else if ((opcode & 0x01b8) == 0x0018)
        {
            // Candidate found. Is next opcode of same kind and same (An)+?
            if (
                (opcode2 & 0xf000) == 0x2000 &&             // move.l
                (opcode2 & 0x01bf) == (opcode & 0x01bf) &&  // same src reg, same mode?
                (opcode2 & 0x0e40) != (opcode & 0x0e40)     // Two different dest registers!
            )
            {
                uint8_t addr_reg = RA_MapM68kRegisterForWrite(ctx, (opcode & 7) + 8);
                uint8_t dst_reg_1 = RA_MapM68kRegisterForWrite(ctx, ((opcode >> 9) & 0x7) + ((opcode >> 3) & 8));
                uint8_t dst_reg_2 = RA_MapM68kRegisterForWrite(ctx, ((opcode2 >> 9) & 0x7) + ((opcode2 >> 3) & 8));
                uint8_t is_movea2 = (opcode2 & 0x01c0) == 0x0040;

                /* merging not allowed if any of the registers stored is also address register */
                if (!(dst_reg_1 == addr_reg || dst_reg_2 == addr_reg))
                {
                    /* Allow merging if two dest registers differ */
                    if (dst_reg_1 != dst_reg_2)
                    {
                        /* Two subsequent register moves from (An)+ */
                        ctx->tc_M68kCodePtr+=2;
                        
                        EMIT(ctx, ldp_postindex(addr_reg, dst_reg_1, dst_reg_2, 8));

                        if (!is_movea2) {
                            update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
                                if (update_mask) {
                                EMIT(ctx, cmn_reg(31, dst_reg_2, LSL, 0));
                                tmp_reg = dst_reg_2;
                            }
                        }
                        else if (!is_movea) {
                            if (update_mask) {
                                EMIT(ctx, cmn_reg(31, dst_reg_1, LSL, 0));
                                tmp_reg = dst_reg_1;
                            }
                        }

                        is_movea = is_movea && is_movea2;
                                
                        done = 1;
                        EMIT_AdvancePC(ctx, 4);
                        insn_consumed = 2;
                        size = 4;
                    }
                }
            }
        }

        // Is move.l -(An), Reg ?: Dest mode 001 or 000, source mode 011
        else if ((opcode & 0x01b8) == 0x0020)
        {
            // Candidate found. Is next opcode of same kind and same (An)+?
            if (
                (opcode2 & 0xf000) == 0x2000 &&             // move.l
                (opcode2 & 0x01bf) == (opcode & 0x01bf) &&  // same src reg, same mode?
                (opcode2 & 0x0e40) != (opcode & 0x0e40)     // Two different dest registers!
            )
            {
                uint8_t addr_reg = RA_MapM68kRegisterForWrite(ctx, (opcode & 7) + 8);
                uint8_t dst_reg_1 = RA_MapM68kRegisterForWrite(ctx, ((opcode >> 9) & 0x7) + ((opcode >> 3) & 8));
                uint8_t dst_reg_2 = RA_MapM68kRegisterForWrite(ctx, ((opcode2 >> 9) & 0x7) + ((opcode2 >> 3) & 8));
                uint8_t is_movea2 = (opcode2 & 0x01c0) == 0x0040;

                /* merging not allowed if any of the registers stored is also address register */
                if (!(dst_reg_1 == addr_reg || dst_reg_2 == addr_reg))
                {
                    /* Allow merging if two dest registers differ */
                    if (dst_reg_1 != dst_reg_2)
                    {
                        /* Two subsequent register moves to (An)+ */
                        ctx->tc_M68kCodePtr+=2;

                        EMIT(ctx, ldp_preindex(addr_reg, dst_reg_2, dst_reg_1, -8));

                        if (!is_movea2) {
                            update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr - 1);
                            if (update_mask) {
                                EMIT(ctx, cmn_reg(31, dst_reg_2, LSL, 0));
                                tmp_reg = dst_reg_2;
                            }
                        }
                        else if (!is_movea) {
                            if (update_mask) {
                                EMIT(ctx, cmn_reg(31, dst_reg_1, LSL, 0));
                                tmp_reg = dst_reg_1;
                            }
                        }

                        is_movea = is_movea && is_movea2;
                    
                        done = 1;
                        EMIT_AdvancePC(ctx, 4);
                        insn_consumed = 2;
                        size = 4;
                    }
                }
            }
        }
    }

    if (!done)
    {
        int sign_ext = 0;

        /* Reverse destination mode, since this one is reversed in MOVE instruction */
        tmp = (opcode >> 6) & 0x3f;
        tmp = ((tmp & 7) << 3) | (tmp >> 3);

        ctx->tc_M68kCodePtr++;

        if ((opcode & 0x3000) == 0x1000)
            size = 1;
        else if ((opcode & 0x3000) == 0x2000)
            size = 4;
        else
            size = 2;

        /* Only if target is data register */
        if ((tmp & 0x38) == 0)
        {
            uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[move_length - 1]);
            
            /* Check if subsequent instruction is extb.l on the same target reg */
            if (size == 1 && (opcode2 & 0xfff8) == 0x49c0 && (opcode2 & 7) == (tmp & 7))
            {
                sign_ext = 1;
                fused_opcodes = 1;
                insn_consumed++;
            }
            /* Check if subsequent instruction is ext.l (word->long) on same target and size is word */
            else if (size == 2 && (opcode2 & 0xfff8) == 0x48c0 && (opcode2 & 7) == (tmp & 7))
            {
                sign_ext = 1;
                fused_opcodes = 1;
                insn_consumed++;
            }
            /* Check if subsequent instructions are ext.w + ext.l on the same target and size is byte */
            else if (size == 1 && (opcode2 & 0xfff8) == 0x4880 && (opcode2 & 7) == (tmp & 7))
            {
                uint16_t opcode3 = cache_read_16(ICACHE, (uintptr_t)&ctx->tc_M68kCodePtr[move_length]);
                if ((opcode3 & 0xfff8) == 0x48c0 && (opcode3 & 7) == (tmp & 7))
                {
                    sign_ext = 1;
                    fused_opcodes = 2;
                    insn_consumed+=2;
                }
            }

        }

        /* Copy 32bit from data reg to data reg */
        if (size == 4 || sign_ext)
        {
            /* If source was not a register (this is handled separately), but target is a register */
            if ((opcode & 0x38) != 0 && (opcode & 0x38) != 0x08) {
                if ((tmp & 0x38) == 0) {
                    loaded_in_dest = 1;
                    tmp_reg = RA_MapM68kRegisterForWrite(ctx, tmp & 7);
                    if (sign_ext)
                        EMIT_LoadFromEffectiveAddress(ctx, 0x80 | size, &tmp_reg, opcode & 0x3f, &ext_count, 0, NULL);
                    else
                        EMIT_LoadFromEffectiveAddress(ctx, size, &tmp_reg, opcode & 0x3f, &ext_count, 0, NULL);
                }
                else if ((tmp & 0x38) == 0x08) {
                    loaded_in_dest = 1;
                    tmp_reg = RA_MapM68kRegisterForWrite(ctx, 8 + (tmp & 7));
                    EMIT_LoadFromEffectiveAddress(ctx, size, &tmp_reg, opcode & 0x3f, &ext_count, 0, NULL);
                }
            }
        }
        if (!loaded_in_dest)
        {
            if (is_movea && size == 2) {

                if (!((tmp & 7) == (opcode & 7) && ((opcode & 0x38) == 0x18 || (opcode & 0x38) == 0x20)))
                {
                    loaded_in_dest = 1;
                    tmp_reg = RA_MapM68kRegisterForWrite(ctx, 8 + (tmp & 7));
                }
                // If dest register An and source is (An)+ mode, don't do postincrement at all. Change mode to (An)
                EMIT_LoadFromEffectiveAddress(ctx, 0x80 | size, &tmp_reg, opcode & 0x3f, &ext_count, 0, NULL);
            }
            else {
                /* No need to check if target is register if sign_ext is active */
                if (sign_ext)
                {
                    tmp_reg = RA_MapM68kRegisterForWrite(ctx, tmp & 7);
                    loaded_in_dest = 1;
                    EMIT_LoadFromEffectiveAddress(ctx, 0x80 | size, &tmp_reg, opcode & 0x3f, &ext_count, 1, NULL);
                }
                else if (size == 2 && (opcode & 0x3f) == 0x3c && (tmp & 0x38) == 0)
                {
                    /* Special case - 16-bit immediate load into Dn register */
                    uint8_t dn = RA_MapM68kRegisterForWrite(ctx, tmp & 7);
                    EMIT(ctx, movk_immed_u16(dn, cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr), 0));
                    ext_count++;
                    loaded_in_dest = 1;
                }
                else
                {
                    /* TODO! Handle byte/word moves between registers if update mask is not needed */
                    if (update_mask == 0 && (opcode & 0x3f) == 0 && (tmp & 0x38) == 0 && size < 4) {
                        tmp_reg = RA_MapM68kRegister(ctx, opcode & 7);
                    }
                    else {
                        /* Not loaded in dest. Get data into temporary register with sign extending */
                        if (update_mask && update_mask != SR_Z)
                            EMIT_LoadFromEffectiveAddress(ctx, 0x80 | size, &tmp_reg, opcode & 0x3f, &ext_count, 1, NULL);
                        else
                            EMIT_LoadFromEffectiveAddress(ctx, size, &tmp_reg, opcode & 0x3f, &ext_count, 1, NULL);
                    }
                }
            }
        }

        if ((opcode & 0x3f) == 0x3c) {
            is_load_immediate = 1;
            switch (size) {
                case 4:
                    immediate_value = cache_read_32(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
                    break;
                case 2:
                    immediate_value = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr);
                    break;
                case 1:
                    immediate_value = cache_read_16(ICACHE, (uintptr_t)ctx->tc_M68kCodePtr) & 0xff;
                    break;
            }
        }

        /* In case of movea the value is *always* sign-extended to 32 bits */
        if (is_movea && size == 2) {
            size = 4;
        }

        /* If opcodes were fused, make a size of 4 and reload SR mask **past** the fused stuff */
        if (fused_opcodes)
        {
            size = 4;
            update_mask = M68K_GetSRMask(ctx->tc_M68kCodePtr + ext_count + fused_opcodes - 1);
        }

        if (update_mask && !is_load_immediate)
        {
            /* preload CC into a register */
            (void)RA_GetCC(ctx);

            switch (size)
            {
                case 4:
                    if (!loaded_in_dest && (tmp & 0x38) == 0)
                    {
                        uint8_t dst_reg = RA_MapM68kRegisterForWrite(ctx, tmp & 7);
                        EMIT(ctx, adds_reg(dst_reg, 31, tmp_reg, LSL, 0));
                        loaded_in_dest = 1;
                    }
                    else
                        EMIT(ctx, cmn_reg(31, tmp_reg, LSL, 0));
                    break;
                case 2:
                    if (update_mask == SR_Z)
                        EMIT(ctx, tst_immed(tmp_reg, 16, 0));
                    else
                        EMIT(ctx, cmn_reg(31, tmp_reg, LSL, loaded_in_dest ? 16 : 0));
                    break;
                case 1:
                    if (update_mask == SR_Z)
                        EMIT(ctx, tst_immed(tmp_reg, 8, 0));
                    else
                        EMIT(ctx, cmn_reg(31, tmp_reg, LSL, loaded_in_dest ? 24 : 0));
                    break;
            }
        }

        if (!loaded_in_dest) {
            /* Handle loading 8- and 16-bit data into target register*/
            if ((tmp & 0x38) == 0 && tmp_reg < 12) {
                if (update_mask) {
                    uint8_t dn = RA_MapM68kRegisterForWrite(ctx, tmp & 7);
                    if (size == 1)
                        EMIT(ctx, bfi(dn, tmp_reg, 0, 8));
                    else if (size == 2)
                        EMIT(ctx, bfi(dn, tmp_reg, 0, 16));
                }
                else {
                    uint8_t dn = RA_MapM68kRegisterForWrite(ctx, tmp & 7);
                    if (size == 1)
                        EMIT(ctx, bic_immed(dn, dn, 8, 0));
                    else if (size == 2)
                        EMIT(ctx, bic_immed(dn, dn, 16, 0));
                    EMIT(ctx, orr_reg(dn, dn, tmp_reg, LSL, 0));
                }
            } else {
                EMIT_StoreToEffectiveAddress(ctx, size, &tmp_reg, tmp, &ext_count, 0);
            }
        }

        EMIT_AdvancePC(ctx, 2 * (ext_count + 1 + fused_opcodes));

        ctx->tc_M68kCodePtr += ext_count + fused_opcodes;
    }

    if (!is_movea)
    {
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(ctx);

            if (is_load_immediate) {
                int32_t tmp_immediate = 0;
                uint8_t alt_mask = update_mask;
                if ((alt_mask & 3) != 0 && (alt_mask & 3) < 3)
                    alt_mask ^= 3;
                EMIT_ClearFlags(ctx, cc, alt_mask);
                switch (size)
                {
                    case 4:
                        tmp_immediate = (int32_t)immediate_value;
                        break;
                    case 2:
                        tmp_immediate = (int16_t)immediate_value;
                        break;
                    case 1:
                        tmp_immediate = (int8_t)immediate_value;
                        break;
                }

                if (tmp_immediate < 0 && update_mask & SR_N) {
                    EMIT_SetFlags(ctx, cc, SR_N);
                }
                if (tmp_immediate == 0 && update_mask & SR_Z) {
                    EMIT_SetFlags(ctx, cc, SR_Z);
                }
            } else {
                EMIT_GetNZ00(ctx, cc, &update_mask);

                if (update_mask & SR_Z)
                    EMIT_SetFlagsConditional(ctx, cc, SR_Z, ARM_CC_EQ);
                if (update_mask & SR_N)
                    EMIT_SetFlagsConditional(ctx, cc, SR_N, ARM_CC_MI);
            }
        }
    }

    RA_FreeARMRegister(ctx, tmp_reg);
    
    return insn_consumed;
}

uint32_t GetSR_Line1(uint16_t opcode)
{
    /* MOVEA case - illegal with byte size */
    if ((opcode & 0x01c0) == 0x0040) {
        kprintf("Undefined Line1\n");
        return SR_CCR << 16;
    }
    
    /* Normal move case: destination allows highest mode + reg of 071 */
    if ((opcode & 0x01c0) == 0x01c0 && (opcode & 0x0e00) > 0x0200) {
        kprintf("Undefined Line1\n");
        return SR_CCR << 16;
    }

    /* Normal move case: source allows highest mode + reg of 074 */
    if ((opcode & 0x0038) == 0x0038 && (opcode & 7) > 4) {
        kprintf("Undefined Line1\n");
        return SR_CCR << 16;
    }

    return SR_NZVC;
}

uint32_t GetSR_Line2(uint16_t opcode)
{
    /* MOVEA case - needs none, sets none */
    if ((opcode & 0x01c0) == 0x0040) {
        return 0;
    }

    /* Normal move case: destination allows highest mode + reg of 071 */
    if ((opcode & 0x01c0) == 0x01c0 && (opcode & 0x0e00) > 0x0200) {
        kprintf("Undefined Line2\n");
        return SR_CCR << 16;
    }

    /* Normal move case: source allows highest mode + reg of 074 */
    if ((opcode & 0x0038) == 0x0038 && (opcode & 7) > 4) {
        kprintf("Undefined Line2\n");
        return SR_CCR << 16;
    }

    return SR_NZVC;
}

uint32_t GetSR_Line3(uint16_t opcode)
{
    return GetSR_Line2(opcode);
}
