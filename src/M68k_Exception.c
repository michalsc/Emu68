#include <stdint.h>
#include <stdarg.h>
/*
    Copyright © 2020 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "support.h"
#include "M68k.h"
#include "RegisterAllocator.h"

void EMIT_Exception(struct TranslatorContext *ctx, uint16_t exception, uint8_t format, ...)
{
    va_list args;
    uint8_t sr = RA_ModifyCC(ctx);
    uint32_t ea = 0;
    uint32_t fault = 0;

    va_start(args, format);

    EMIT(ctx, mov_immed_u16(1, (format << 12) | (exception & 0x0fff), 0));

    if (format == 0)
    {
        EMIT(ctx, str64_offset_preindex(31, 30, -8));
    }
    if (format == 2 || format == 3)
    {
        ea = va_arg(args, uint32_t);
        EMIT(ctx,
            mov_immed_u16(0, (ea & 0xffff), 0),
            movk_immed_u16(0, (ea >> 16) & 0xffff, 1),
            stp64_preindex(31, 0, 30, -16)
        );
    }
    else if (format == 4)
    {
        fault = va_arg(args, uint32_t);
        ea = va_arg(args, uint32_t);

        EMIT(ctx,
            // Push Fault
            mov_immed_u16(0, fault & 0xffff, 0),
            movk_immed_u16(0, (fault >> 16) & 0xffff, 1),
            stp64_preindex(31, 0, 30, -16),
            
            // Push EA
            mov_immed_u16(2, (ea & 0xffff), 0),
            movk_immed_u16(2, (ea >> 16) & 0xffff, 1),
            str64_offset_preindex(31, 2, -8)
        );
    }

    extern void M68K_Exception();
    uint32_t val = (uintptr_t)M68K_Exception;

    EMIT(ctx,
        mov_reg(0, sr),
        mov_immed_u16(2, val & 0xffff, 0),
        movk_immed_u16(2, val >> 16, 1),
        orr64_immed(2, 2, 25, 25, 1),
        blr(2),
        mov_reg(sr, 0)
    );

    if (format == 2 || format == 3)
    {
        EMIT(ctx,
            ldr64_offset(31, 30, 8),
            add64_immed(31, 31, 16)
        );
    }
    else if (format == 4)
    {
        EMIT(ctx,
            ldr64_offset(31, 30, 16),
            add64_immed(31, 31, 24)
        );
    }
    else
    {
        EMIT(ctx, ldr64_offset_postindex(31, 30, 8));
    }

    va_end(args);
}
