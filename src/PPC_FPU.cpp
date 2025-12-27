/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#define restrict __restrict__

#include <emu68/FPR>
#include <emu68/GPR>
#include <emu68/ppc/Opcode>

#include "config.h"
#include "PPC.h"
#include "A64.h"

namespace Emu68::PPC::Emit {

int fsubx(PPCTranslatorContext *tc, Opcode opcode)
{
    /* Sanity check */
    if (opcode.illegal(0x000007c0)) return -1;

    uint8_t rd = opcode.u8(6, 10);
    uint8_t ra = opcode.u8(11, 15);
    uint8_t rb = opcode.u8(16, 20);
    uint8_t rc = opcode.u8(31, 31);

    FPR reg_ra = tc->mapFPRForRead(ra);
    FPR reg_rb = tc->mapFPRForRead(rb);
    FPR reg_rd = tc->mapFPRForWrite(rd);

    if (rc) {
        kprintf("fsub. not supported yet!");
        return -1;
    }

    tc->emit(fsubd(reg_rd, reg_ra, reg_rb));

    tc->advancePC(4);
    
    return 1;
}

int faddx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x000007c0) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = opcode & 1;

    FPR reg_ra = tc->mapFPRForRead(ra);
    FPR reg_rb = tc->mapFPRForRead(rb);
    FPR reg_rd = tc->mapFPRForWrite(rd);

    if (rc) {
        kprintf("fadd. not supported yet!");
        return -1;
    }

    tc->emit(faddd(reg_rd, reg_ra, reg_rb));

    tc->advancePC(4);

    return 1;
}

int fmulx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x0000f800) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 6) & 31;
    uint8_t rc = opcode & 1;

    FPR reg_ra = tc->mapFPRForRead(ra);
    FPR reg_rb = tc->mapFPRForRead(rb);
    FPR reg_rd = tc->mapFPRForWrite(rd);

    if (rc) {
        kprintf("fmul. not supported yet!");
        return -1;
    }

    tc->emit(fmuld(reg_rd, reg_ra, reg_rb));

    tc->advancePC(4);

    return 1;
}

int fdivx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x000007c0) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = opcode & 1;

    FPR reg_ra = tc->mapFPRForRead(ra);
    FPR reg_rb = tc->mapFPRForRead(rb);
    FPR reg_rd = tc->mapFPRForWrite(rd);

    if (rc) {
        kprintf("fdiv. not supported yet!");
        return -1;
    }

    tc->emit(fdivd(reg_rd, reg_ra, reg_rb));

    tc->advancePC(4);

    return 1;
}

int fctiwzx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x001f07c0) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = opcode & 1;

    FPR reg_rb = tc->mapFPRForRead(rb);
    FPR reg_rd = tc->mapFPRForWrite(rd);
    GPR tmp = GPR::allocate();

    if (rc) {
        kprintf("fctiwzx. not supported yet!");
        return -1;
    }

    tc->emit({
        fcvtzs_Dto32(tmp, reg_rb),
        mov_reg_to_simd(reg_rd, TS_S, 0, tmp)
    });

    tc->advancePC(4);

    return 1;
}

int fmrx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x001f0000) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = opcode & 1;

    FPR reg_rb = tc->mapFPRForRead(rb);
    FPR reg_rd = tc->mapFPRForWrite(rd);

    if (rc) {
        kprintf("fmr. not supported yet!");
        return -1;
    }

    tc->emit(fcpyd(reg_rd, reg_rb));

    tc->advancePC(4);

    return 1;
}

int fcmpu(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x00600001) return -1;

    uint8_t crn = (opcode >> 23) & 7;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    FPR reg_rb = tc->mapFPRForRead(rb);
    FPR reg_ra = tc->mapFPRForRead(ra);

    tc->emit(fcmpd(reg_ra, reg_rb));

    setCRnSigned(tc, crn);

    tc->advancePC(4);

    return 1;
}

int fmadd(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t rd = (opcode >> 21) & 31;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = (opcode >> 6) & 31;
    uint8_t c = opcode & 1;

    FPR reg_ra = tc->mapFPRForRead(ra);
    FPR reg_rb = tc->mapFPRForRead(rb);
    FPR reg_rc = tc->mapFPRForRead(rc);
    FPR reg_rd = tc->mapFPRForWrite(rd);

    if (c) {
        kprintf("fmadd. not supported yet!");
        return -1;
    }

    tc->emit(fmaddd(reg_rd, reg_ra, reg_rc, reg_rb));

    tc->advancePC(4);

    return 1;
}

} // Emu68::PPC::Emit
