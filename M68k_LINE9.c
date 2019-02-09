#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>
#include "ARM.h"
#include "M68k.h"
#include "RegisterAllocator.h"

/* Line9 is one large SUBX/SUB/SUBA */

uint32_t *EMIT_line9(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    /* SUBA */
    if ((opcode & 0xf1c0) == 0x90c0)
    {
        uint8_t ext_words = 0;
        uint8_t size = (opcode & 0x0100) == 0x0100 ? 4 : 2;
        uint8_t reg = RA_MapM68kRegister(&ptr, ((opcode >> 9) & 7) + 8);
        uint8_t tmp;
        RA_SetDirtyM68kRegister(&ptr, ((opcode >> 9) & 7) + 8);

        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp, opcode & 0x3f, *m68k_ptr, &ext_words);

        *ptr++ = sub_reg(reg, reg, tmp, 0);

        RA_FreeARMRegister(&ptr, tmp);

        ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    }
    /* SUBX */
    else if ((opcode & 0xf130) == 0x9100)
    {
        /* Move negated C flag to ARM flags */
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_immed_u8(tmp, 0);
        *ptr++ = tst_immed(REG_SR, SR_X);
        *ptr++ = orr_cc_immed(ARM_CC_EQ, tmp, tmp, 0x202);  /* Set bit 29: 0x20000000 */
        *ptr++ = msr(tmp, 8);
        RA_FreeARMRegister(&ptr, tmp);

        /* Register to register */
        if ((opcode & 0x0008) == 0)
        {
            uint8_t size = (opcode >> 6) & 3;
            uint8_t regx = RA_MapM68kRegister(&ptr, opcode & 7);
            uint8_t regy = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
            uint8_t tmp = 0;

            RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);

            switch (size)
            {
                case 0: /* Byte */
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = lsl_immed(tmp, regx, 24);
                    *ptr++ = add_cc_immed(ARM_CC_CC, tmp, tmp, 0x401);
                    *ptr++ = rsbs_reg(tmp, tmp, regy, 24);
                    *ptr++ = lsr_immed(tmp, tmp, 24);
                    *ptr++ = bfi(regy, tmp, 0, 8);
                    RA_FreeARMRegister(&ptr, tmp);
                    break;
                case 1: /* Word */
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = lsl_immed(tmp, regx, 16);
                    *ptr++ = add_cc_immed(ARM_CC_CC, tmp, tmp, 0x801);
                    *ptr++ = rsbs_reg(tmp, tmp, regy, 16);
                    *ptr++ = lsr_immed(tmp, tmp, 16);
                    *ptr++ = bfi(regy, tmp, 0, 16);
                    RA_FreeARMRegister(&ptr, tmp);
                    break;
                case 2: /* Long */
                    *ptr++ = sbcs_reg(regy, regy, regx, 0);
                    break;
            }
        }
        /* memory to memory */
        else
        {
            uint8_t size = (opcode >> 6) & 3;
            uint8_t regx = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
            uint8_t regy = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
            uint8_t dest = RA_AllocARMRegister(&ptr);
            uint8_t src = RA_AllocARMRegister(&ptr);

            RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            RA_SetDirtyM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));

            switch (size)
            {
                case 0: /* Byte */
                    *ptr++ = ldrb_offset_preindex(regx, src, (opcode & 7) == 7 ? -2 : -1);
                    *ptr++ = ldrb_offset_preindex(regy, dest, ((opcode >> 9) & 7) == 7 ? -2 : -1);
                    *ptr++ = lsl_immed(src, src, 24);
                    *ptr++ = add_cc_immed(ARM_CC_CC, src, src, 0x401);
                    *ptr++ = rsbs_reg(dest, src, dest, 24);
                    *ptr++ = lsr_immed(dest, dest, 24);
                    *ptr++ = strb_offset(regy, dest, 0);
                    break;
                case 1: /* Word */
                    *ptr++ = ldrh_offset_preindex(regx, src, -2);
                    *ptr++ = ldrh_offset_preindex(regy, dest, -2);
                    *ptr++ = lsl_immed(src, src, 16);
                    *ptr++ = add_cc_immed(ARM_CC_CC, src, src, 0x801);
                    *ptr++ = rsbs_reg(dest, src, dest, 16);
                    *ptr++ = lsr_immed(dest, dest, 16);
                    *ptr++ = strh_offset(regy, dest, 0);
                    break;
                case 2: /* Long */
                    *ptr++ = ldr_offset_preindex(regx, src, -4);
                    *ptr++ = ldr_offset_preindex(regy, dest, -4);
                    *ptr++ = sbcs_reg(dest, dest, src, 0);
                    *ptr++ = str_offset(regy, dest, 0);
                    break;
            }
        }

        ptr = EMIT_AdvancePC(ptr, 2);

        uint8_t mask = M68K_GetSRMask(BE16((*m68k_ptr)[0]));
        uint8_t update_mask = (SR_X | SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = orr_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = orr_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            if (update_mask & SR_V)
                *ptr++ = orr_cc_immed(ARM_CC_VS, REG_SR, REG_SR, SR_V);
            if (update_mask & (SR_X | SR_C))
                *ptr++ = orr_cc_immed(ARM_CC_CC, REG_SR, REG_SR, SR_X | SR_C);
        }
    }


    return ptr;
}
