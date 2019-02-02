#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>
#include "ARM.h"
#include "M68k.h"
#include "RegisterAllocator.h"

uint32_t *EMIT_line4(uint32_t *ptr, uint16_t **m68k_ptr)
{
    (void)m68k_ptr;
    return ptr;
}
