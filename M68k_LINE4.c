#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>
#include "ARM.h"
#include "M68k.h"
#include "RegisterAllocator.h"

uint32_t *EMIT_line4(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    /* 0100000011xxxxxx - MOVE from SR */
    if ((opcode & 0xffc0) == 0x40c0)
    {
        printf("[LINE4] Not implemented MOVE from SR\n");
    }
    /* 0100001011xxxxxx - MOVE from CCR */
    else if ((opcode &0xffc0) == 0x42c0)
    {
        printf("[LINE4] Not implemented MOVE from CCR\n");
    }
    /* 01000000ssxxxxxx - NEGX */
    else if ((opcode & 0xff00) == 0x4000 && (opcode & 0xc0) != 0xc0)
    {
        printf("[LINE4] Not implemented NEGX\n");
    }
    /* 01000010ssxxxxxx - CLR */
    else if ((opcode & 0xff00) == 0x4200 && (opcode & 0xc0) != 0xc0)
    {
        printf("[LINE4] Not implemented CLR\n");
    }
    /* 0100010011xxxxxx - MOVE to CCR */
    else if ((opcode &0xffc0) == 0x44c0)
    {
        printf("[LINE4] Not implemented MOVE to CCR\n");
    }
    /* 01000100ssxxxxxx - NEG */
    else if ((opcode &0xff00) == 0x4400 && (opcode & 0xc0) != 0xc0)
    {
        printf("[LINE4] Not implemented NEG\n");
    }
    /* 0100011011xxxxxx - MOVE to SR */
    else if ((opcode &0xffc0) == 0x46c0)
    {
        printf("[LINE4] Not implemented MOVE to CCR\n");
    }
    /* 01000110ssxxxxxx - NOT */
    else if ((opcode &0xff00) == 0x4600 && (opcode & 0xc0) != 0xc0)
    {
        printf("[LINE4] Not implemented NOT\n");
    }
    /* 0100100xxx000xxx - EXT, EXTB */
    else if ((opcode & 0xfe38) == 0x4808)
    {
        printf("[LINE4] Not implemented EXT/EXTB\n");
    }
    /* 0100100xxx000xxx - EXT, EXTB */
    else if ((opcode & 0xfe38) == 0x4808)
    {
        printf("[LINE4] Not implemented EXT/EXTB\n");
    }
    /* 0100100000001xxx - LINK */
    else if ((opcode & 0xfff8) == 0x4808)
    {
        printf("[LINE4] Not implemented LINK\n");
    }
    /* 0100100000xxxxxx - NBCD */
    else if ((opcode & 0xffc0) == 0x4800 && (opcode & 0x08) != 0x08)
    {
        printf("[LINE4] Not implemented NBCD\n");
    }
    /* 0100100001000xxx - SWAP */
    else if ((opcode & 0xfff8) == 0x4840)
    {
        printf("[LINE4] Not implemented SWAP\n");
    }
    /* 0100100001001xxx - BKPT */
    else if ((opcode & 0xfff8) == 0x4848)
    {
        printf("[LINE4] Not implemented BKPT\n");
    }
    /* 0100100001xxxxxx - PEA */
    else if ((opcode & 0xffc0) == 0x4840 && (opcode & 0x38) != 0x08)
    {
        printf("[LINE4] Not implemented PEA\n");
    }
    /* 0100101011111100 - ILLEGAL */
    else if (opcode == 0x4afc)
    {
        *ptr++ = udf(0x4afc);
    }
    /* 0100101011xxxxxx - TAS */
    else if ((opcode & 0xffc0) == 0x4ac0)
    {
        printf("[LINE4] Not implemented TAS\n");
    }
    /* 0100101011xxxxxx - TST */
    else if ((opcode & 0xff00) == 0x4a00 && (opcode & 0xc0) != 0xc0)
    {
        printf("[LINE4] Not implemented TST\n");
    }
    /* 0100110000xxxxxx - MULU, MULS, DIVU, DIVUL, DIVS, DIVSL */ 
    else if ((opcode & 0xff80) == 0x4a00)
    {
        printf("[LINE4] Not implemented MULU/MULS/DIVU/DIVUL/DIVS/DIVSL\n");
    }
    /* 010011100100xxxx - TRAP */
    else if ((opcode & 0xfff0) == 0x4e40)
    {
        printf("[LINE4] Not implemented TRAP\n");
    }
    /* 0100111001010xxx - LINK */
    else if ((opcode & 0xfff8) == 0x4e50)
    {
        printf("[LINE4] Not implemented LINK\n");
    }
    /* 0100111001011xxx - UNLK */
    else if ((opcode & 0xfff8) == 0x4e58)
    {
        printf("[LINE4] Not implemented UNLK\n");
    }
    /* 010011100110xxxx - MOVE USP */
    else if ((opcode & 0xfff0) == 0x4e60)
    {
        printf("[LINE4] Not implemented MOVE USP\n");
    }
    /* 0100111001110000 - RESET */
    else if (opcode == 0x4e70)
    {
        printf("[LINE4] Not implemented RESET\n");
    }
    /* 0100111001110000 - NOP */
    else if (opcode == 0x4e71)
    {
        *ptr++ = and_reg(0, 0, 0, 0);
    }
    /* 0100111001110010 - STOP */
    else if (opcode == 0x4e72)
    {
        printf("[LINE4] Not implemented STOP\n");
    }
    /* 0100111001110011 - RTE */
    else if (opcode == 0x4e73)
    {
        printf("[LINE4] Not implemented RTE\n");
    }
    /* 0100111001110100 - RTD */
    else if (opcode == 0x4e74)
    {
        printf("[LINE4] Not implemented RTD\n");
    }
    /* 0100111001110101 - RTS */
    else if (opcode == 0x4e75)
    {
        printf("[LINE4] Not implemented RTS\n");
    }
    /* 0100111001110110 - TRAPV */
    else if (opcode == 0x4e76)
    {
        printf("[LINE4] Not implemented TRAPV\n");
    }
    /* 0100111001110111 - RTR */
    else if (opcode == 0x4e77)
    {
        printf("[LINE4] Not implemented RTR\n");
    }
    /* 010011100111101x - MOVEC */
    else if ((opcode & 0xfffe) == 0x4e7a)
    {
        printf("[LINE4] Not implemented MOVEC\n");
    }
    /* 0100111010xxxxxx - JSR */
    else if ((opcode & 0xffc0) == 0x4e80)
    {
        printf("[LINE4] Not implemented JSR\n");
    }
    /* 0100111011xxxxxx - JMP */
    else if ((opcode & 0xffc0) == 0x4ec0)
    {
        printf("[LINE4] Not implemented JMP\n");
    }
    /* 01001x001xxxxxxx - MOVEM */
    else if ((opcode & 0xfb80) == 0x4880)
    {
        printf("[LINE4] Not implemented MOVEM\n");
    }
    /* 0100xxx111xxxxxx - LEA */
    else if ((opcode & 0xf1c0) == 0x41c0)
    {
        printf("[LINE4] Not implemented LEA\n");
    }
    /* 0100xxx1x0xxxxxx - CHK */
    else if ((opcode & 0xf140) == 0x4100)
    {
        printf("[LINE4] Not implemented CHK\n");
    }

    return ptr;
}
