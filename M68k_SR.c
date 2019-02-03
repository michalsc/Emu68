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
    /* MOVE from SR */
    else if ((opcode & 0xffc0) == 0x40c0)
    {
    }
    /* MOVE from CCR */
    else if ((opcode & 0xffc0) == 0x42c0)
    {
    }
    /* NEGX */
    else if ((opcode & 0xff00) == 0x4000 && (opcode & 0xc0) != 0xc0)
    {
    }
    /* CLR */
    else if ((opcode & 0xff00) == 0x4200 && (opcode & 0xc0) != 0xc0)
    {
        mask = SR_N | SR_Z | SR_C | SR_V;
    }
    /* MOVE to CCR */
    else if ((opcode & 0xffc0) == 0x44c0)
    {
    }
    /* NEG */
    else if ((opcode & 0xff00) == 0x4400 && (opcode & 0xc0) != 0xc0)
    {
    }
    /* MOVE to SR */
    else if ((opcode & 0xffc0) == 0x46c0)
    {
    }
    /* NOT */
    else if ((opcode & 0xff00) == 0x4600 && (opcode & 0xc0) != 0xc0)
    {
    }
    /* EXT, EXTB */
    else if ((opcode & 0xfe38) == 0x4808)
    {
    }
    /* LINK */
    else if ((opcode & 0xfff8) == 0x4808)
    {
    }
    /* NBCD */
    else if ((opcode & 0xffc0) == 0x4800 && (opcode & 0x08) != 0x08)
    {
    }
    /* SWAP */
    else if ((opcode & 0xfff8) == 0x4840)
    {
        mask = SR_Z | SR_N | SR_C | SR_V;
    }
    /* BKPT */
    else if ((opcode & 0xfff8) == 0x4848)
    {
    }
    /* 0100100001xxxxxx - PEA */
    else if ((opcode & 0xffc0) == 0x4840 && (opcode & 0x38) != 0x08)
    {
    }
    /* 0100101011111100 - ILLEGAL */
    else if (opcode == 0x4afc)
    {
    }
    /* 0100101011xxxxxx - TAS */
    else if ((opcode & 0xffc0) == 0x4ac0)
    {
    }
    /* 0100101011xxxxxx - TST */
    else if ((opcode & 0xff00) == 0x4a00 && (opcode & 0xc0) != 0xc0)
    {
    }
    /* 0100110000xxxxxx - MULU, MULS, DIVU, DIVUL, DIVS, DIVSL */
    else if ((opcode & 0xff80) == 0x4a00)
    {
    }
    /* 010011100100xxxx - TRAP */
    else if ((opcode & 0xfff0) == 0x4e40)
    {
    }
    /* 0100111001010xxx - LINK */
    else if ((opcode & 0xfff8) == 0x4e50)
    {
    }
    /* 0100111001011xxx - UNLK */
    else if ((opcode & 0xfff8) == 0x4e58)
    {
    }
    /* 010011100110xxxx - MOVE USP */
    else if ((opcode & 0xfff0) == 0x4e60)
    {
    }
    /* 0100111001110000 - RESET */
    else if (opcode == 0x4e70)
    {
    }
    /* 0100111001110000 - NOP */
    else if (opcode == 0x4e71)
    {
    }
    /* 0100111001110010 - STOP */
    else if (opcode == 0x4e72)
    {
    }
    /* 0100111001110011 - RTE */
    else if (opcode == 0x4e73)
    {
    }
    /* 0100111001110100 - RTD */
    else if (opcode == 0x4e74)
    {
    }
    /* 0100111001110101 - RTS */
    else if (opcode == 0x4e75)
    {
    }
    /* 0100111001110110 - TRAPV */
    else if (opcode == 0x4e76)
    {
    }
    /* 0100111001110111 - RTR */
    else if (opcode == 0x4e77)
    {
    }
    /* 010011100111101x - MOVEC */
    else if ((opcode & 0xfffe) == 0x4e7a)
    {
    }
    /* 0100111010xxxxxx - JSR */
    else if ((opcode & 0xffc0) == 0x4e80)
    {
    }
    /* 0100111011xxxxxx - JMP */
    else if ((opcode & 0xffc0) == 0x4ec0)
    {
    }
    /* 01001x001xxxxxxx - MOVEM */
    else if ((opcode & 0xfb80) == 0x4880)
    {
    }
    /* 0100xxx111xxxxxx - LEA */
    else if ((opcode & 0xf1c0) == 0x41c0)
    {
    }
    /* 0100xxx1x0xxxxxx - CHK */
    else if ((opcode & 0xf140) == 0x4100)
    {
    }

    return mask;
}
