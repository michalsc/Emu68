#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>
#include "ARM.h"
#include "M68k.h"
#include "RegisterAllocator.h"

uint32_t *EMIT_MUL_DIV(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr);

uint32_t *EMIT_line8(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    (*m68k_ptr)++;

    /* 1000xxx011xxxxxx - DIVU */
    if ((opcode & 0xf1c0) == 0x80c0)
    {
        ptr = EMIT_MUL_DIV(ptr, opcode, m68k_ptr);
    }
    /* 1000xxx10000xxxx - SBCD */
    else if ((opcode & 0xf1f0) == 0x8100)
    {

    }
    /* 1000xxx10100xxxx - PACK */
    else if ((opcode & 0xf1f0) == 0x8140)
    {

    }
    /* 1000xxx11000xxxx - UNPK */
    else if ((opcode & 0xf1f0) == 0x8180)
    {

    }
    /* 1000xxx111xxxxxx - DIVS */
    else if ((opcode & 0xf1c0) == 0x81c0)
    {
        ptr = EMIT_MUL_DIV(ptr, opcode, m68k_ptr);
    }
    /* 1000xxxxxxxxxxxx - OR */
    else if ((opcode & 0xf000) == 0x8000)
    {

    }

    return ptr;
}
