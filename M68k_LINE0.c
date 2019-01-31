#include <stdint.h>

#include <stdio.h>
#include "ARM.h"
#include "M68k.h"
#include "RegisterAllocator.h"

uint32_t *EMIT_line0(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = (*m68k_ptr)[0];

    if ((opcode & 0xff00) == 0x0000)   /* ORI to CCR, ORI to SR, ORI */
    {

    }
    else if ((opcode & 0xff00) == 0x0200)   /* ANDI to CCR, ANDI to SR, ANDI */
    {

    }
    else if ((opcode & 0xff00) == 0x0400)   /* SUBI */
    {

    }
    else if ((opcode & 0xff00) == 0x0600)   /* ADDI */
    {
        
    }
    /* CMP2, CHK2 - 0000 SIZE 011 | ADDI 0000110 SIZE ??? */
    else if ((opcode & 0xff00) == 0x0a00)   /* EORI */
    {
        
    }
    else if ((opcode & 0xff00) == 0x0c00)   /* CMPI */
    {
        
    }
    else if ((opcode & 0xff00) == 0x0800)   /* BTST, BCHG, BCLR, BSET */
    {
        
    }

    return ptr;
}
