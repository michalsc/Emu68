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

uint32_t *EMIT_Exception_old(uint32_t *ptr, uint16_t exception, uint8_t format, ...)
{
    va_list args;
    uint8_t ctx = RA_TryCTX(&ptr); //RA_GetCTX(&ptr);
    uint8_t sp = RA_MapM68kRegister(&ptr, 15);
    uint8_t vbr = RA_AllocARMRegister(&ptr);
    uint8_t cc = RA_ModifyCC(&ptr);
    int need_free_ctx = 0;
    
    if (ctx == 0xff) {
        need_free_ctx = 1;
        ctx = RA_GetCTX(&ptr);
    }

    va_start(args, format);

    RA_SetDirtyM68kRegister(&ptr, 15);

    /* In case of privilege violation there is no need to check if we were in supervisor mode. We were not! */
    if (exception == VECTOR_PRIVILEGE_VIOLATION) {
        // Store USP
        *ptr++ = mov_reg_to_simd(31, TS_S, 1, sp);

        /* Check if we need to load ISP or MSP */
        *ptr++ = tbnz(cc, SRB_M, 3);

        /* Load ISP to A7 */
        *ptr++ = mov_simd_to_reg(sp, 31, TS_S, 2);
        *ptr++ = b(2);
        /* Load MSP to A7 */
        *ptr++ = mov_simd_to_reg(sp, 31, TS_S, 3);
    }
    else
    {
        /* Check if we are changing stack due to user->supervisor transition */
        *ptr++ = tbnz(cc, SRB_S, 6);

        /* We were in user mode. Store A7 as USP */
        *ptr++ = mov_reg_to_simd(31, TS_S, 1, sp);

        /* Check if we need to load ISP or MSP */
        *ptr++ = tbnz(cc, SRB_M, 3);

        /* Load ISP to A7 */
        *ptr++ = mov_simd_to_reg(sp, 31, TS_S, 2);
        *ptr++ = b(2);
        /* Load MSP to A7 */
        *ptr++ = mov_simd_to_reg(sp, 31, TS_S, 3);
    }

    if (format == 2 || format == 3)
    {
        /* Format 2 and 3, store Address / Effective address */
        uint32_t ea = va_arg(args, uint32_t);
        *ptr++ = movw_immed_u16(vbr, ea & 0xffff);
        if ((ea >> 16) != 0)
            *ptr++ = movt_immed_u16(vbr, ea >> 16);
        *ptr++ = str_offset_preindex(sp, vbr, -4);
    }
    else if (format == 4)
    {
        /* Format 4, store Effective address and address of faulting instruction */
        uint32_t ea = va_arg(args, uint32_t);
        uint32_t fault = va_arg(args, uint32_t);

        *ptr++ = movw_immed_u16(vbr, fault & 0xffff);
        if ((fault >> 16) != 0)
            *ptr++ = movt_immed_u16(vbr, fault >> 16);
        *ptr++ = str_offset_preindex(sp, vbr, -8);

        *ptr++ = movw_immed_u16(vbr, ea & 0xffff);
        if ((ea >> 16) != 0)
            *ptr++ = movt_immed_u16(vbr, ea >> 16);
        *ptr++ = str_offset(sp, vbr, 4);
    }

    uint8_t cc_copy = RA_AllocARMRegister(&ptr);
    /* Reverse C and V */
    *ptr++ = mov_reg(cc_copy, cc);
    *ptr++ = rbit(vbr, cc);
    *ptr++ = bfxil(cc_copy, vbr, 30, 2);
    /* Store SR */
    *ptr++ = strh_offset_preindex(sp, cc_copy, -8);

    RA_FreeARMRegister(&ptr, cc_copy);

    /* Store program counter */
    *ptr++ = stur_offset(sp, REG_PC, 2);

    /* Store exception vector and type */
    *ptr++ = mov_immed_u16(vbr, (format << 12) | (exception & 0xfff), 0);
    *ptr++ = strh_offset(sp, vbr, 6);

    /* Clear trace flags, set supervisor */
    *ptr++ = bic_immed(cc, cc, 2, 32 - SRB_T0);
    *ptr++ = orr_immed(cc, cc, 1, 32 - SRB_S);

    /* Load VBR */
    *ptr++ = ldr_offset(ctx, vbr, __builtin_offsetof(struct M68KState, VBR));
    *ptr++ = ldr_offset(vbr, REG_PC, exception);

    RA_FreeARMRegister(&ptr, vbr);
    
    if (need_free_ctx) {
        RA_FlushCTX(&ptr);
    }

    va_end(args);

    return ptr;
}

uint32_t *EMIT_Exception(uint32_t *ptr, uint16_t exception, uint8_t format, ...)
{
    va_list args;
    uint8_t sr = RA_ModifyCC(&ptr);
    uint32_t ea = 0;
    uint32_t fault = 0;

    va_start(args, format);

#if 0
    /* Most of preparations will be performed by the M68K_Exception, we prepare known state here */
    ptr = EMIT_FlushPC(ptr);

    RA_StoreDirtyFPURegs(&ptr);
    RA_StoreDirtyM68kRegs(&ptr);

    RA_StoreCC(&ptr);
    RA_StoreFPCR(&ptr);
    RA_StoreFPSR(&ptr);
#endif

    *ptr++ = mov_immed_u16(1, (format << 12) | (exception & 0x0fff), 0);

    if (format == 2 || format == 3)
    {
        ea = va_arg(args, uint32_t);
        *ptr++ = mov_immed_u16(0, (ea & 0xffff), 0);
        *ptr++ = movk_immed_u16(0, (ea >> 16) & 0xffff, 1);
        *ptr++ = str64_offset_preindex(31, 0, -8);
    }
    else if (format == 4)
    {
        ea = va_arg(args, uint32_t);
        *ptr++ = mov_immed_u16(0, (ea & 0xffff), 0);
        *ptr++ = movk_immed_u16(0, (ea >> 16) & 0xffff, 1);
        *ptr++ = str64_offset_preindex(31, 0, -16);
        fault = va_arg(args, uint32_t);
        *ptr++ = mov_immed_u16(0, fault & 0xffff, 0);
        *ptr++ = movk_immed_u16(0, (fault >> 16) & 0xffff, 1);
        *ptr++ = str_offset(31, 0, 12);
    }

    extern void M68K_Exception();
    uint32_t val = (uintptr_t)M68K_Exception;

    *ptr++ = mov_reg(0, sr);
    *ptr++ = mov_immed_u16(2, val & 0xffff, 0);
    *ptr++ = movk_immed_u16(2, val >> 16, 1);
    *ptr++ = orr64_immed(2, 2, 25, 25, 1);
    *ptr++ = blr(2);
    *ptr++ = mov_reg(sr, 0);

    if (format == 2 || format == 3) {
        *ptr++ = add64_immed(31, 31, 8);
    }
    else if (format == 4)
    {
        *ptr++ = add64_immed(31, 31, 16);
    }

    va_end(args);
    return ptr;
}
