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

uint32_t *EMIT_moveq(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr);
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    int8_t value = opcode & 0xff;
    uint8_t reg = (opcode >> 9) & 7;
    uint8_t tmp_reg = RA_MapM68kRegisterForWrite(&ptr, reg);
    *insn_consumed = 1;

    if (opcode & 0x100) {
        ptr = EMIT_FlushPC(ptr);
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        *ptr++ = svc(0x100);
        *ptr++ = svc(0x101);
        *ptr++ = svc(0x103);
        *ptr++ = (uint32_t)(uintptr_t)(*m68k_ptr - 8);
        *ptr++ = 48;
        ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
        return ptr;
    }

    (*m68k_ptr)++;

    *ptr++ = mov_immed_s8(tmp_reg, value);
    ptr = EMIT_AdvancePC(ptr, 2);

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        uint8_t alt_mask = update_mask;
        if ((alt_mask & 3) != 0 && (alt_mask & 3) < 3)
            alt_mask ^= 3;
        ptr = EMIT_ClearFlags(ptr, cc, alt_mask);
        if (value <= 0) {
            if (value < 0)
                ptr = EMIT_SetFlags(ptr, cc, SR_N);
            else
                ptr = EMIT_SetFlags(ptr, cc, SR_Z);
        }
    }

    return ptr;
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



uint32_t *EMIT_move(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr);
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    int move_length = M68K_GetINSNLength(*m68k_ptr);
    uint8_t ext_count = 0;
    uint8_t tmp_reg = 0xff;
    uint8_t size = 1;
    uint8_t tmp = 0;
    uint8_t is_movea = (opcode & 0x01c0) == 0x0040;
    int is_load_immediate = 0;
    uint32_t immediate_value = 0;
    int loaded_in_dest = 0;
    *insn_consumed = 1;
    int done = 0;
    int fused_opcodes = 0;

    // Move from/to An in byte size is illegal
    if ((opcode & 0xf000) == 0x1000)
    {
        if (is_movea || (opcode & 0x38) == 0x08) {
            ptr = EMIT_FlushPC(ptr);
            ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
            *ptr++ = svc(0x100);
            *ptr++ = svc(0x101);
            *ptr++ = svc(0x103);
            *ptr++ = (uint32_t)(uintptr_t)(*m68k_ptr - 8);
            *ptr++ = 48;
            ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
            *ptr++ = INSN_TO_LE(0xffffffff);
            return ptr;
        }
    }

    if ((opcode & 0x3f) > 0x3c)
    {
        ptr = EMIT_FlushPC(ptr);
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        *ptr++ = svc(0x100);
        *ptr++ = svc(0x101);
        *ptr++ = svc(0x103);
        *ptr++ = (uint32_t)(uintptr_t)(*m68k_ptr - 8);
        *ptr++ = 48;
        ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
        return ptr;
    }

    if ((opcode & 0x01c0) == 0x01c0) {
        if ((opcode & 0x0c00)) {
            ptr = EMIT_FlushPC(ptr);
            ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
            *ptr++ = svc(0x100);
            *ptr++ = svc(0x101);
            *ptr++ = svc(0x103);
            *ptr++ = (uint32_t)(uintptr_t)(*m68k_ptr - 8);
            *ptr++ = 48;
            ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
            *ptr++ = INSN_TO_LE(0xffffffff);
            return ptr;
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
        uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[1]);

        // Is move.l Reg, -(An) ?: Dest mode 100, source mode 000 or 001
        if ((opcode & 0x01f0) == 0x0100)
        {
            // Candidate found. Is next opcode of same kind and same -(An)?
            if (
                (opcode2 & 0xf000) == 0x2000 &&          // move.l
                (opcode2 & 0x0ff0) == (opcode & 0x0ff0)  // same dest reg, same mode?
            )
            {
                uint8_t addr_reg = RA_MapM68kRegisterForWrite(&ptr, ((opcode >> 9) & 7) + 8);
                uint8_t src_reg_1 = RA_MapM68kRegister(&ptr, opcode & 0xf);
                uint8_t src_reg_2 = RA_MapM68kRegister(&ptr, opcode2 & 0xf);

                /* Merging not allowed if any of the registers stored is also address register */
                if (!(src_reg_1 == addr_reg || src_reg_2 == addr_reg))
                {
                    /* Two subsequent register moves to -(An) */
                    (*m68k_ptr)++;
                    update_mask = M68K_GetSRMask(*m68k_ptr);
                    (*m68k_ptr)++;

                    if (update_mask)
                    {
                        *ptr++ = cmn_reg(31, src_reg_2, LSL, 0);
                    }

                    *ptr++ = stp_preindex(addr_reg, src_reg_2, src_reg_1, -8);

                    tmp_reg = src_reg_2;
                
                    done = 1;
                    ptr = EMIT_AdvancePC(ptr, 4);
                    *insn_consumed = 2;
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
                uint8_t addr_reg = RA_MapM68kRegisterForWrite(&ptr, ((opcode >> 9) & 7) + 8);
                uint8_t src_reg_1 = RA_MapM68kRegister(&ptr, opcode & 0xf);
                uint8_t src_reg_2 = RA_MapM68kRegister(&ptr, opcode2 & 0xf);
                
                /* Merging not allowed if any of the registers stored is also address register */
                if (!(src_reg_1 == addr_reg || src_reg_2 == addr_reg))
                {
                    /* Two subsequent register moves to (An)+ */
                    (*m68k_ptr)++;
                    update_mask = M68K_GetSRMask(*m68k_ptr);
                    (*m68k_ptr)++;

                    if (update_mask)
                    {
                        *ptr++ = cmn_reg(31, src_reg_2, LSL, 0);
                    }

                    *ptr++ = stp_postindex(addr_reg, src_reg_1, src_reg_2, 8);

                    tmp_reg = src_reg_2;
                
                    done = 1;
                    ptr = EMIT_AdvancePC(ptr, 4);
                    *insn_consumed = 2;
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
                uint8_t addr_reg = RA_MapM68kRegisterForWrite(&ptr, (opcode & 7) + 8);
                uint8_t dst_reg_1 = RA_MapM68kRegisterForWrite(&ptr, ((opcode >> 9) & 0x7) + ((opcode >> 3) & 8));
                uint8_t dst_reg_2 = RA_MapM68kRegisterForWrite(&ptr, ((opcode2 >> 9) & 0x7) + ((opcode2 >> 3) & 8));
                uint8_t is_movea2 = (opcode2 & 0x01c0) == 0x0040;

                /* merging not allowed if any of the registers stored is also address register */
                if (!(dst_reg_1 == addr_reg || dst_reg_2 == addr_reg))
                {
                    /* Allow merging if two dest registers differ */
                    if (dst_reg_1 != dst_reg_2)
                    {
                        /* Two subsequent register moves from (An)+ */
                        (*m68k_ptr)+=2;
                        
                        *ptr++ = ldp_postindex(addr_reg, dst_reg_1, dst_reg_2, 8);

                        if (!is_movea2) {
                            update_mask = M68K_GetSRMask(*m68k_ptr - 1);
                                if (update_mask) {
                                *ptr++ = cmn_reg(31, dst_reg_2, LSL, 0);
                                tmp_reg = dst_reg_2;
                            }
                        }
                        else if (!is_movea) {
                            if (update_mask) {
                                *ptr++ = cmn_reg(31, dst_reg_1, LSL, 0);
                                tmp_reg = dst_reg_1;
                            }
                        }

                        is_movea = is_movea && is_movea2;
                                
                        done = 1;
                        ptr = EMIT_AdvancePC(ptr, 4);
                        *insn_consumed = 2;
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
                uint8_t addr_reg = RA_MapM68kRegisterForWrite(&ptr, (opcode & 7) + 8);
                uint8_t dst_reg_1 = RA_MapM68kRegisterForWrite(&ptr, ((opcode >> 9) & 0x7) + ((opcode >> 3) & 8));
                uint8_t dst_reg_2 = RA_MapM68kRegisterForWrite(&ptr, ((opcode2 >> 9) & 0x7) + ((opcode2 >> 3) & 8));
                uint8_t is_movea2 = (opcode2 & 0x01c0) == 0x0040;

                /* merging not allowed if any of the registers stored is also address register */
                if (!(dst_reg_1 == addr_reg || dst_reg_2 == addr_reg))
                {
                    /* Allow merging if two dest registers differ */
                    if (dst_reg_1 != dst_reg_2)
                    {
                        /* Two subsequent register moves to (An)+ */
                        (*m68k_ptr)+=2;

                        *ptr++ = ldp_preindex(addr_reg, dst_reg_2, dst_reg_1, -8);

                        if (!is_movea2) {
                            update_mask = M68K_GetSRMask(*m68k_ptr - 1);
                                if (update_mask) {
                                *ptr++ = cmn_reg(31, dst_reg_2, LSL, 0);
                                tmp_reg = dst_reg_2;
                            }
                        }
                        else if (!is_movea) {
                            if (update_mask) {
                                *ptr++ = cmn_reg(31, dst_reg_1, LSL, 0);
                                tmp_reg = dst_reg_1;
                            }
                        }

                        is_movea = is_movea && is_movea2;
                    
                        done = 1;
                        ptr = EMIT_AdvancePC(ptr, 4);
                        *insn_consumed = 2;
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

        (*m68k_ptr)++;

        if ((opcode & 0x3000) == 0x1000)
            size = 1;
        else if ((opcode & 0x3000) == 0x2000)
            size = 4;
        else
            size = 2;

        /* Only if target is data register */
        if ((tmp & 0x38) == 0)
        {
            uint16_t opcode2 = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[move_length - 1]);
            
            /* Check if subsequent instruction is extb.l on the same target reg */
            if (size == 1 && (opcode2 & 0xfff8) == 0x49c0 && (opcode2 & 7) == (tmp & 7))
            {
                sign_ext = 1;
                fused_opcodes = 1;
                (*insn_consumed)++;
            }
            /* Check if subsequent instruction is ext.l (word->long) on same target and size is word */
            else if (size == 2 && (opcode2 & 0xfff8) == 0x48c0 && (opcode2 & 7) == (tmp & 7))
            {
                sign_ext = 1;
                fused_opcodes = 1;
                (*insn_consumed)++;
            }
            /* Check if subsequent instructions are ext.w + ext.l on the same target and size is byte */
            else if (size == 1 && (opcode2 & 0xfff8) == 0x4880 && (opcode2 & 7) == (tmp & 7))
            {
                uint16_t opcode3 = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[move_length]);
                if ((opcode3 & 0xfff8) == 0x48c0 && (opcode3 & 7) == (tmp & 7))
                {
                    sign_ext = 1;
                    fused_opcodes = 2;
                    *insn_consumed+=2;
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
                    tmp_reg = RA_MapM68kRegisterForWrite(&ptr, tmp & 7);
                    if (sign_ext)
                        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0x80 | size, &tmp_reg, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
                    else
                        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp_reg, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
                }
                else if ((tmp & 0x38) == 0x08) {
                    loaded_in_dest = 1;
                    tmp_reg = RA_MapM68kRegisterForWrite(&ptr, 8 + (tmp & 7));
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp_reg, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
                }
            }
        }
        if (!loaded_in_dest)
        {
            if (is_movea && size == 2) {

                if (!((tmp & 7) == (opcode & 7) && ((opcode & 0x38) == 0x18 || (opcode & 0x38) == 0x20)))
                {
                    loaded_in_dest = 1;
                    tmp_reg = RA_MapM68kRegisterForWrite(&ptr, 8 + (tmp & 7));
                }
                // If dest register An and source is (An)+ mode, don't do postincrement at all. Change mode to (An)
                ptr = EMIT_LoadFromEffectiveAddress(ptr, 0x80 | size, &tmp_reg, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
            }
            else {
                /* No need to check if target is register if sign_ext is active */
                if (sign_ext)
                {
                    tmp_reg = RA_MapM68kRegisterForWrite(&ptr, tmp & 7);
                    loaded_in_dest = 1;
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, 0x80 | size, &tmp_reg, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);
                }
                else if (size == 2 && (opcode & 0x3f) == 0x3c)
                {
                    /* Special case - 16-bit immediate load into register */
                    uint8_t dn = RA_MapM68kRegisterForWrite(&ptr, tmp & 7);
                    *ptr++ = movk_immed_u16(dn, cache_read_16(ICACHE, (uintptr_t)&(**m68k_ptr)), 0);
                    ext_count++;
                    loaded_in_dest = 1;
                }
                else
                {
                    ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp_reg, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);
                }
            }
        }

        if ((opcode & 0x3f) == 0x3c) {
            is_load_immediate = 1;
            switch (size) {
                case 4:
                    immediate_value = cache_read_32(ICACHE, (uintptr_t)&(*(uint32_t*)(*m68k_ptr)));
                    break;
                case 2:
                    immediate_value = cache_read_16(ICACHE, (uintptr_t)&(**m68k_ptr));
                    break;
                case 1:
                    immediate_value = ((uint8_t*)*m68k_ptr)[1];
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
            update_mask = M68K_GetSRMask(*m68k_ptr + ext_count + fused_opcodes - 1);
        }

        if (update_mask && !is_load_immediate)
        {
            switch (size)
            {
                case 4:
                    if (!loaded_in_dest && (tmp & 0x38) == 0)
                    {
                        uint8_t dst_reg = RA_MapM68kRegisterForWrite(&ptr, tmp & 7);
                        *ptr++ = adds_reg(dst_reg, 31, tmp_reg, LSL, 0);
                        loaded_in_dest = 1;
                    }
                    else
                        *ptr++ = cmn_reg(31, tmp_reg, LSL, 0);
                    break;
                case 2:
                    *ptr++ = cmn_reg(31, tmp_reg, LSL, 16);
                    break;
                case 1:
                    *ptr++ = cmn_reg(31, tmp_reg, LSL, 24);
                    break;
            }
        }

        if (!loaded_in_dest) {
            /* Handle loading 8- and 16-bit data into target register*/
            if ((tmp & 0x38) == 0 && tmp_reg < 12) {
                uint8_t dn = RA_MapM68kRegisterForWrite(&ptr, tmp & 7);
                if (size == 1)
                    *ptr++ = bic_immed(dn, dn, 8, 0);
                else if (size == 2)
                    *ptr++ = bic_immed(dn, dn, 16, 0);
                *ptr++ = orr_reg(dn, dn, tmp_reg, LSL, 0);
            } else {
                ptr = EMIT_StoreToEffectiveAddress(ptr, size, &tmp_reg, tmp, *m68k_ptr, &ext_count, 0);
            }
        }

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1 + fused_opcodes));

        (*m68k_ptr) += ext_count + fused_opcodes;
    }

    if (!is_movea)
    {
        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);

            if (is_load_immediate) {
                int32_t tmp_immediate = 0;
                uint8_t alt_mask = update_mask;
                if ((alt_mask & 3) != 0 && (alt_mask & 3) < 3)
                    alt_mask ^= 3;
                ptr = EMIT_ClearFlags(ptr, cc, alt_mask);
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
                    ptr = EMIT_SetFlags(ptr, cc, SR_N);
                }
                if (tmp_immediate == 0 && update_mask & SR_Z) {
                    ptr = EMIT_SetFlags(ptr, cc, SR_Z);
                }
            } else {
                ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

                if (update_mask & SR_Z)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
                if (update_mask & SR_N)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            }
        }
    }

    RA_FreeARMRegister(&ptr, tmp_reg);
    return ptr;
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
