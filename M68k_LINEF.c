/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdint.h>
#include <stdlib.h>

#include <math.h>
#include <stdio.h>
#include "ARM.h"
#include "M68k.h"
#include "RegisterAllocator.h"

long double constants[128] = {
    [0x00] = M_PI,
    [0x0b] = 0.3010299956639812, //Log10(2)
    [0x0c] = M_E,
    [0x0d] = M_LOG2E,
    [0x0e] = M_LOG10E,
    [0x0f] = 0.0,
    [0x30] = M_LN2,
    [0x31] = M_LN10,
    [0x32] = 1.0,
    [0x33] = 1E1,
    [0x34] = 1E2,
    [0x35] = 1E4,
    [0x36] = 1E8,
    [0x37] = 1E16,
    [0x38] = 1E32,
    [0x39] = 1E64,
    [0x3a] = 1E128,
    [0x3b] = 1E256,
    [0x3c] = HUGE_VAL,  //  1E512 - too large for double!
    [0x3d] = HUGE_VAL,  // 1E1024 - too large for double!
    [0x3e] = HUGE_VAL,  // 1E2048 - too large for double!
    [0x3f] = HUGE_VAL,  // 1E4096 - too large for double!
};

uint32_t *EMIT_lineF(uint32_t *ptr, uint16_t **m68k_ptr)
{
    uint16_t opcode = BE16((*m68k_ptr)[0]);
    uint16_t opcode2 = BE16((*m68k_ptr)[1]);
    (*m68k_ptr)+=2;

    /* FABS.X reg-reg */
    if (opcode == 0xf200 && (opcode2 & 0x407f) == 0x0018) // <- fix second word!
    {
        uint8_t fp_src = (opcode2 >> 10) & 7;
        uint8_t fp_dst = (opcode2 >> 7) & 7;
        *ptr++ = INSN_TO_LE(0xeeb00bc0 | (fp_dst << 12) | (fp_src));
        ptr = EMIT_AdvancePC(ptr, 4);
    }
    /* FMOVECR reg */
    if (opcode == 0xf200 && (opcode2 & 0xfc00) == 0x5c00)
    {
        uint8_t fp_dst = (opcode2 >> 7) & 7;
        uint8_t base_reg = RA_AllocARMRegister(&ptr);
        uint8_t offset = opcode2 & 0x7f;

        /* Alloc destination FP register for write */
        fp_dst = RA_MapFPURegisterForWrite(&ptr, fp_dst);

        /* 
            Load pointer to constants into base register, then load the value from table into
            destination VFP register, finally skip the base address (which is not an ARM INSN)
        */
        *ptr++ = ldr_offset(15, base_reg, 4);
        *ptr++ = INSN_TO_LE(0xed900b00 | ((fp_dst) << 12) | (base_reg << 16) | (offset * 2));
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *ptr++ = BE32((uint32_t)&constants);

        RA_FreeARMRegister(&ptr, base_reg);
    }
    /* FNOP */
    else if (opcode == 0xf280 && opcode2 == 0)
    {
        ptr = EMIT_AdvancePC(ptr, 4);
        ptr = EMIT_FlushPC(ptr);
    }
    else
        *ptr++ = udf(opcode);

    return ptr;
}
