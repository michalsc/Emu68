#include <stdint.h>

#include <stdio.h>
#include "ARM.h"
#include "M68k.h"
#include "RegisterAllocator.h"

uint32_t *EMIT_line0(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = (*m68k_ptr)[0];
    (*m68k_ptr)++;

    if ((opcode & 0xff00) == 0x0000 && (opcode & 0x00c0) != 0x00c0)   /* 00000000xxxxxxxx - ORI to CCR, ORI to SR, ORI */
    {

    }
    else if ((opcode & 0xff00) == 0x0200)   /* 00000010xxxxxxxx - ANDI to CCR, ANDI to SR, ANDI */
    {

    }
    else if ((opcode & 0xff00) == 0x0400)   /* 00000100xxxxxxxx - SUBI */
    {

    }
    else if ((opcode & 0xff00) == 0x0600 && (opcode & 0x00c0) != 0x00c0)   /* 00000110xxxxxxxx - ADDI */
    {
        uint8_t ext_count = 0;
        uint8_t tmp_count = 0;
        uint8_t immed = RA_AllocARMRegister(&ptr);
        uint8_t dest;
        uint8_t size = 0;

        /* Load immediate into the register */
        switch (opcode & 0x00c0)
        {
            case 0x0000:    /* Byte operation */
                *ptr++ = ldrb_offset(REG_PC, immed, 3);
                *ptr++ = lsl_immed(immed, immed, 24);
                ext_count++;
                size = 1;
                break;
            case 0x0040:    /* Short operation */
                *ptr++ = ldrh_offset(REG_PC, immed, 2);
                *ptr++ = lsl_immed(immed, immed, 16);
                ext_count++;
                size = 2;
                break;
            case 0x0080:    /* Long operation */
                *ptr++ = ldr_offset(REG_PC, immed, 2);
                ext_count+=2;
                size = 4;
                break;
        }

        /* Fetch data from destination */
        tmp_count = ext_count;
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &dest, opcode & 0x3f, *m68k_ptr, &ext_count);

        /* Perform calcualtion */
        *ptr++ = adds_reg(immed, immed, dest, 24);
        *ptr++ = lsr_immed(immed, immed, 24);

        /* Store back to destination */
        ptr = EMIT_StoreToEffectiveAddress(ptr, size, &immed, opcode & 0x3f, *m68k_ptr, &tmp_count);

        RA_FreeARMRegister(&ptr, immed);

        *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_count + 1));
        (*m68k_ptr) += ext_count;

        uint8_t mask = M68K_GetSRMask((*m68k_ptr)[0]);
        uint8_t update_mask = (SR_X | SR_C | SR_V | SR_Z | SR_N) & ~mask;

        if (update_mask)
        {
            *ptr++ = bic_immed(REG_SR, REG_SR, update_mask);
            if (update_mask & SR_N)
                *ptr++ = or_cc_immed(ARM_CC_MI, REG_SR, REG_SR, SR_N);
            if (update_mask & SR_Z)
                *ptr++ = or_cc_immed(ARM_CC_EQ, REG_SR, REG_SR, SR_Z);
            if (update_mask & SR_V)
                *ptr++ = or_cc_immed(ARM_CC_VS, REG_SR, REG_SR, SR_V);
            if (update_mask & (SR_X | SR_C))
                *ptr++ = or_cc_immed(ARM_CC_CS, REG_SR, REG_SR, SR_X | SR_C);
        }
    }
    else if ((opcode & 0xf9c0) == 0x00c0)   /* 00000xx011xxxxxx - CMP2, CHK2 */
    {

    }
    else if ((opcode & 0xff00) == 0x0a00)   /* 00001010xxxxxxxx - EORI to CCR, EORI to SR, EORI */
    {

    }
    else if ((opcode & 0xff00) == 0x0c00)   /* 00001100xxxxxxxx - CMPI */
    {

    }
    else if ((opcode & 0xffc0) == 0x0800)   /* 0000100000xxxxxx - BTST */
    {

    }
    else if ((opcode & 0xffc0) == 0x0840)   /* 0000100001xxxxxx - BCHG */
    {

    }
    else if ((opcode & 0xffc0) == 0x0880)   /* 0000100010xxxxxx - BCLR */
    {

    }
    else if ((opcode & 0xffc0) == 0x08c0)   /* 0000100011xxxxxx - BSET */
    {

    }
    else if ((opcode & 0xff00) == 0x0e00)   /* 00001110xxxxxxxx - MOVES */
    {

    }
    else if ((opcode & 0xf9c0) == 0x08c0)   /* 00001xx011xxxxxx - CAS, CAS2 */
    {

    }
    else if ((opcode & 0xf1c0) == 0x0100)   /* 0000xxx100xxxxxx - BTST */
    {

    }
    else if ((opcode & 0xf1c0) == 0x0140)   /* 0000xxx101xxxxxx - BCHG */
    {

    }
    else if ((opcode & 0xf1c0) == 0x0180)   /* 0000xxx110xxxxxx - BCLR */
    {

    }
    else if ((opcode & 0xf1c0) == 0x01c0)   /* 0000xxx111xxxxxx - BSET */
    {

    }
    else if ((opcode & 0xf038) == 0x0008)   /* 0000xxxxxx001xxx - MOVEP */
    {

    }

    return ptr;
}
