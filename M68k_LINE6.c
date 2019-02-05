#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>
#include "ARM.h"
#include "M68k.h"
#include "RegisterAllocator.h"

uint32_t *EMIT_line6(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    /* 01100000xxxxxxxx - BRA */
    if ((opcode & 0xff00) == 0x6000)
    {
        /* use 16-bit offset */
        if ((opcode & 0x00ff) == 0x00)
        {

        }
        /* use 32-bit offset */
        else if ((opcode & 0x00ff) == 0xff)
        {

        }
        /* otherwise use 8-bit offset */
        {

        }
    }
    /* 01100001xxxxxxxx - BSR */
    else if ((opcode & 0xff00) == 0x6100)
    {

    }
    /* 0110ccccxxxxxxxx - Bcc */
    else
    {

    }


    return ptr;
}
