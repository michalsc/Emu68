#include <stdint.h>

#include <stdio.h>
#include "ARM.h"
#include "M68k.h"
#include "RegisterAllocator.h"

uint32_t *EMIT_line0(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = (*m68k_ptr)[0];

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
