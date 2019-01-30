#include <stdint.h>

#include <stdio.h>
#include "ARM.h"
#include "M68k.h"
#include "RegisterAllocator.h"

uint32_t *EMIT_moveq(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = (*m68k_ptr)[0];
    int8_t value = opcode & 0xff;
    uint8_t reg = (opcode >> 9) & 7;
    uint8_t tmp_reg = RA_MapM68kRegisterForWrite(&ptr, reg);

    (*m68k_ptr)++;

    *ptr++ = movs_immed_s8(tmp_reg, value);
    *ptr++ = add_immed(REG_PC, REG_PC, 2);

    /* If next instruction is MOVE, do not calculate flags */
    opcode = (*m68k_ptr)[0];
    if (!(
        ((opcode & 0xc000) == 0 && (opcode & 0x3000) != 0) |
        ((opcode & 0xf000) == 0x7000)
    ))
    {
        *ptr++ = bic_immed(REG_SR, REG_SR, SR_C | SR_V | SR_Z | SR_N);
        *ptr++ = or_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        *ptr++ = or_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
    }


    return ptr;
}

uint32_t *EMIT_move(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = (*m68k_ptr)[0];
    uint8_t ext_count = 0;
    uint8_t tmp_reg = 0xff;
    uint8_t size = 0;
    uint8_t tmp = 0;
    uint8_t movea_insn = (opcode & 0x01c0) == 0x0040;

    (*m68k_ptr)++;

    if ((opcode & 0x3000) == 0x1000)
        size = 1;
    else if ((opcode & 0x3000) == 0x2000)
        size = 4;
    else
        size = 2;

    ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp_reg, opcode & 0x3f, *m68k_ptr, &ext_count);

    /* Reverse destination mode, since this one is reversed in MOVE instruction */
    tmp = (opcode >> 6) & 0x3f;
    tmp = ((tmp & 7) << 3) | (tmp >> 3);

    ptr = EMIT_StoreToEffectiveAddress(ptr, size, &tmp_reg, tmp, *m68k_ptr, &ext_count);

    *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_count + 1));

    (*m68k_ptr) += ext_count;

    /* If next instruction is MOVE, do not calculate flags */
    opcode = (*m68k_ptr)[0];
    if (!movea_insn && !(
        ((opcode & 0xc000) == 0 && (opcode & 0x3000) != 0 && (opcode & 0x01c0) != 0x0040) |
        ((opcode & 0xf000) == 0x7000)
    ))
    {
        *ptr++ = cmp_immed(tmp_reg, 0);
        *ptr++ = bic_immed(REG_SR, REG_SR, SR_C | SR_V | SR_Z | SR_N);
        *ptr++ = or_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
        *ptr++ = or_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
    }

    RA_FreeARMRegister(&ptr, tmp_reg);
    return ptr;
}
