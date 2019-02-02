#include <stdint.h>

#include <stdio.h>
#include "ARM.h"
#include "M68k.h"

/* Get the mask of status flags changed by the instruction specified by the opcode */
uint8_t M68K_GetSRMask(uint16_t opcode)
{
    uint8_t mask = 0;

    /* Skip entire 6xxx group - these are just branches and do not affect flags */
    /* all move instructions with exception of movea change N V Z C */
    if (
        ((opcode & 0xc000) == 0 && (opcode & 0x3000) != 0 && (opcode & 0x01c0) != 0x0040) |
        ((opcode & 0xf000) == 0x7000)
    )
    {
        mask = SR_C | SR_Z | SR_N | SR_V;
    }
    /* ADDI / SUBI change NVZCX */
    else if (
        ((opcode & 0xff00) == 0x0600 && (opcode & 0x00c0) != 0x00c0) |
        ((opcode & 0xff00) == 0x0400 && (opcode & 0x00c0) != 0x00c0)
    )
    {
        mask = SR_X | SR_C | SR_Z | SR_N | SR_V;
    }
    /* ORI / ORI to CR / ORI to SSR */
    else if ((opcode & 0xff00) == 0x0000 && (opcode & 0x00c0) != 0x00c0)
    {
        if ((opcode & 0x00ff) == 0x003c || (opcode & 0x00ff) == 0x007c)
            mask = SR_X | SR_C | SR_Z | SR_N | SR_V;
        else
            mask = SR_C | SR_Z | SR_N | SR_V;
    }
    /* ANDI / ANDI to CR / ANDI to SSR */
    else if ((opcode & 0xff00) == 0x0200)
    {
        if ((opcode & 0x00ff) == 0x003c || (opcode & 0x00ff) == 0x007c)
            mask = SR_X | SR_C | SR_Z | SR_N | SR_V;
        else
            mask = SR_C | SR_Z | SR_N | SR_V;
    }
    /* EORI / EORI to CR / EORI to SSR */
    else if ((opcode & 0xff00) == 0x0a00)
    {
        if ((opcode & 0x00ff) == 0x003c || (opcode & 0x00ff) == 0x007c)
            mask = SR_X | SR_C | SR_Z | SR_N | SR_V;
        else
            mask = SR_C | SR_Z | SR_N | SR_V;
    }
    /* BTST / BCHG / BCLR / BCHG / BSET */
    else if (
        ((opcode & 0xf1c0) == 0x0100) |
        ((opcode & 0xffc0) == 0x0800) |
        ((opcode & 0xf1c0) == 0x0140) |
        ((opcode & 0xffc0) == 0x0840) |
        ((opcode & 0xf1c0) == 0x0180) |
        ((opcode & 0xffc0) == 0x0880) |
        ((opcode & 0xf1c0) == 0x01c0) |
        ((opcode & 0xffc0) == 0x08c0)
    )
    {
        mask = SR_Z;
    }
    /* CMPI */
    else if ((opcode & 0xff00) == 0x0c00)
    {
        mask = SR_C | SR_Z | SR_N | SR_V;
    }


    return mask;
}
