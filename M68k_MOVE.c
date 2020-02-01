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

uint32_t *EMIT_moveq(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    int8_t value = opcode & 0xff;
    uint8_t reg = (opcode >> 9) & 7;
    uint8_t tmp_reg = RA_MapM68kRegisterForWrite(&ptr, reg);

    (*m68k_ptr)++;

    *ptr++ = mov_immed_s8(tmp_reg, value);
    ptr = EMIT_AdvancePC(ptr, 2);

    uint8_t mask = M68K_GetSRMask(*m68k_ptr);
    uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_ClearFlags(ptr, cc, update_mask);
        if (value <= 0) {
            if (value < 0)
                ptr = EMIT_SetFlags(ptr, cc, SR_N);
            else
                ptr = EMIT_SetFlags(ptr, cc, SR_Z);
        }
    }

    return ptr;
}

uint32_t *EMIT_move(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    uint8_t ext_count = 0;
    uint8_t tmp_reg = 0xff;
    uint8_t size = 1;
    uint8_t tmp = 0;
    uint8_t is_movea = (opcode & 0x01c0) == 0x0040;
    int is_load_immediate = 0;
    uint32_t immediate_value = 0;
    int loaded_in_dest = 0;

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

    /* Copy 32bit from data reg to data reg */
    if (size == 4)
    {
        /* If source was not a register (this is handled separately), but target is a register */
        if ((opcode & 0x38) != 0 && (opcode & 0x38) != 0x08) {
            if ((tmp & 0x38) == 0) {
                loaded_in_dest = 1;
                tmp_reg = RA_MapM68kRegisterForWrite(&ptr, tmp & 7);
                ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp_reg, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);        
            }
            else if ((tmp & 0x38) == 0x08) {
                loaded_in_dest = 1;
                tmp_reg = RA_MapM68kRegisterForWrite(&ptr, 8 + (tmp & 7));
                ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp_reg, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);        
            }
        }
    }
#if 0
    if ((tmp & 0x38) == 0 && size == 4)
    {
        loaded_in_dest = 1;
        tmp_reg = RA_MapM68kRegisterForWrite(&ptr, tmp & 7);
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp_reg, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
    }
    else if ((tmp & 0x38) == 0x08)
    {
        loaded_in_dest = 1;
        tmp_reg = RA_MapM68kRegisterForWrite(&ptr, 8 + (tmp & 7));
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp_reg, opcode & 0x3f, *m68k_ptr, &ext_count, 0, NULL);
    }
    else
#endif
    if (!loaded_in_dest)
    {
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp_reg, opcode & 0x3f, *m68k_ptr, &ext_count, 1, NULL);
    }

    if ((opcode & 0x3f) == 0x3c) {
        is_load_immediate = 1;
        switch (size) {
            case 4:
                immediate_value = BE32(*(uint32_t*)(*m68k_ptr));
                break;
            case 2:
                immediate_value = BE16(**m68k_ptr);
                break;
            case 1:
                immediate_value = ((uint8_t*)*m68k_ptr)[1];
                break;
        }
    }

    /* In case of movea the value is *always* sign-extended to 32 bits */
    if (is_movea && size == 2) {
#ifdef __aarch64__
        *ptr++ = sxth(tmp_reg, tmp_reg);
#else
        *ptr++ = sxth(tmp_reg, tmp_reg, 0);
#endif
        size = 4;
    }

    if (!loaded_in_dest)
        ptr = EMIT_StoreToEffectiveAddress(ptr, size, &tmp_reg, tmp, *m68k_ptr, &ext_count);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_count + 1));

    (*m68k_ptr) += ext_count;

    if (!is_movea)
    {
        uint8_t mask = M68K_GetSRMask(*m68k_ptr);
        uint8_t update_mask = (SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_ClearFlags(ptr, cc, update_mask);

            if (is_load_immediate) {
                int32_t tmp_immediate = 0;

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

                if (tmp_immediate <= 0) {
                    if ((update_mask & SR_N) && (tmp_immediate < 0))
                        ptr = EMIT_SetFlags(ptr, cc, SR_N);
                    else if (update_mask & SR_Z)
                        ptr = EMIT_SetFlags(ptr, cc, SR_Z);
                }
            } else {
#ifdef __aarch64__
                switch (size)
                {
                    case 4:
                        *ptr++ = cmn_reg(31, tmp_reg, LSL, 0);
                        break;
                    case 2:
                        *ptr++ = cmn_reg(31, tmp_reg, LSL, 16);
                        break;
                    case 1:
                        *ptr++ = cmn_reg(31, tmp_reg, LSL, 24);
                        break;
                }
#else
                *ptr++ = cmp_immed(tmp_reg, 0);
#endif
                if (update_mask & SR_N)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
                if (update_mask & SR_Z)
                    ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            }
        }
    }

    RA_FreeARMRegister(&ptr, tmp_reg);
    return ptr;
}
