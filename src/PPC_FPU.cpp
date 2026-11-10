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

#if PPC_NAN_PROPAGATION
/*
    If the result is a NaN, PowerPC returns the first NaN operand (frA, frB, frC) made quiet, or the default NaN
    when no operand is a NaN. ARM picks a signaling NaN before a quiet one and may flip the sign of the addend.
    Operand 0xff is not used.
*/
static void fixNaN(PPCTranslatorContext *tc, uint8_t reg_rd, uint8_t reg_ra, uint8_t reg_rb, uint8_t reg_rc)
{
    FPR tmp = FPR::allocate();
    GPR nan = GPR::allocate();
    uint8_t ops[3] = { reg_rc, reg_rb, reg_ra };    // Lowest priority first
    int count = 0;

    for (int i = 0; i < 3; i++) {
        if (ops[i] != 0xff)
            count += 2;
    }

    tc->emit({
        fcmpd(reg_rd, reg_rd),
        b_cc(A64_CC_VC, 6 + count),                 // Not a NaN, skip all of it
        mov64_immed_u16(nan, 0x7ff8, 3),            // Default NaN
        fmov_from_reg(tmp, nan)
    });

    for (int i = 0; i < 3; i++) {
        if (ops[i] != 0xff) {
            tc->emit({
                fcmpd(ops[i], ops[i]),
                fcseld(tmp, ops[i], tmp, A64_CC_VS)
            });
        }
    }

    tc->emit({
        fmov_to_reg(nan, tmp),
        orr64_immed(nan, nan, 1, 13, 1),            // Make it quiet
        fmov_from_reg(reg_rd, nan)
    });
}
#endif

/* Rounding mode from FPSCR (RN: 0 nearest, 1 zero, 2 +inf, 3 -inf) to FPCR (0 nearest, 1 +inf, 2 -inf, 3 zero) */
static void updateFPCR(PPCTranslatorContext *tc, uint8_t reg_fpscr)
{
    GPR tmp = GPR::allocate();
    GPR rn = GPR::allocate();
    GPR fpcr = GPR::allocate();

    tc->emit({
        ubfiz(rn, reg_fpscr, 1, 2),                 // RN * 2, two bits per entry in the table below
        mov_immed_u16(tmp, 0x9c, 0),                // 0 -> 0, 1 -> 3, 2 -> 1, 3 -> 2
        lsrv(tmp, tmp, rn),
        get_fpcr(fpcr),
        bfi(fpcr, tmp, 22, 2),
        set_fpcr(fpcr)
    });
}

/*
    Moves the result to frD. Without PPC_NAN_PROPAGATION the operation wrote frD already. With it the operation
    wrote a temporary, since frD may be an operand fixNaN still needs. Group 59 rounds to single precision.
*/
static void writeResult(PPCTranslatorContext *tc, uint8_t reg_rd, uint8_t result, uint8_t reg_ra, uint8_t reg_rb, uint8_t reg_rc, bool single)
{
#if PPC_NAN_PROPAGATION
    fixNaN(tc, result, reg_ra, reg_rb, reg_rc);
#else
    (void)reg_ra;
    (void)reg_rb;
    (void)reg_rc;
#endif

    if (single) {
        tc->emit({
            fcvtsd(reg_rd, result),
            fcvtds(reg_rd, reg_rd)
        });
    }
    else if (result != reg_rd) {
        tc->emit(fcpyd(reg_rd, result));
    }
}

/* frD = frA op frB, fmul takes frC in place of frB */
static int arithmetic(PPCTranslatorContext *tc, Opcode opcode, uint32_t (*op)(uint8_t, uint8_t, uint8_t), bool mul, bool single, const char *name)
{
    /* Sanity check */
    if (opcode.illegal(mul ? 0x0000f800 : 0x000007c0)) return -1;

    uint8_t rd = opcode.u8(6, 10);
    uint8_t ra = opcode.u8(11, 15);
    uint8_t rb = mul ? opcode.u8(21, 25) : opcode.u8(16, 20);
    uint8_t rc = opcode.u8(31, 31);

    FPR reg_ra = tc->mapFPRForRead(ra);
    FPR reg_rb = tc->mapFPRForRead(rb);
    FPR reg_rd = tc->mapFPRForWrite(rd);

    if (rc) {
        kprintf("%s. not supported yet!", name);
        return -1;
    }

#if PPC_NAN_PROPAGATION
    FPR result = FPR::allocate();
#else
    uint8_t result = reg_rd;
#endif

    tc->emit(op(result, reg_ra, reg_rb));
    writeResult(tc, reg_rd, result, reg_ra, reg_rb, 0xff, single);

    tc->advancePC(4);

    return 1;
}

/* frD = frA * frC + frB, or - frB. fnmadd and fnmsub negate the result, not the operands */
static int multiplyAdd(PPCTranslatorContext *tc, Opcode opcode, bool subtract, bool negate, bool single, const char *name)
{
    uint8_t rd = opcode.u8(6, 10);
    uint8_t ra = opcode.u8(11, 15);
    uint8_t rb = opcode.u8(16, 20);
    uint8_t rc = opcode.u8(21, 25);
    uint8_t c = opcode.u8(31, 31);

    FPR reg_ra = tc->mapFPRForRead(ra);
    FPR reg_rb = tc->mapFPRForRead(rb);
    FPR reg_rc = tc->mapFPRForRead(rc);
    FPR reg_rd = tc->mapFPRForWrite(rd);

    if (c) {
        kprintf("%s. not supported yet!", name);
        return -1;
    }

#if PPC_NAN_PROPAGATION
    FPR result = FPR::allocate();
#else
    uint8_t result = reg_rd;
#endif

    if (subtract)
        tc->emit(fnmsubd(result, reg_ra, reg_rc, reg_rb));     // ARM fnmsub is frA * frC - frB
    else
        tc->emit(fmaddd(result, reg_ra, reg_rc, reg_rb));

    if (negate)
        tc->emit(fnegd(result, result));

    writeResult(tc, reg_rd, result, reg_ra, reg_rb, reg_rc, single);

    tc->advancePC(4);

    return 1;
}

/* Instructions with frD and frB only, emit() writes frD */
static int unary(PPCTranslatorContext *tc, Opcode opcode, uint32_t illegal, const char *name, void (*emit)(PPCTranslatorContext *tc, uint8_t reg_rd, uint8_t reg_rb))
{
    /* Sanity check */
    if (opcode.illegal(illegal)) return -1;

    uint8_t rd = opcode.u8(6, 10);
    uint8_t rb = opcode.u8(16, 20);
    uint8_t rc = opcode.u8(31, 31);

    FPR reg_rb = tc->mapFPRForRead(rb);
    FPR reg_rd = tc->mapFPRForWrite(rd);

    if (rc) {
        kprintf("%s. not supported yet!", name);
        return -1;
    }

    emit(tc, reg_rd, reg_rb);

    tc->advancePC(4);

    return 1;
}

/* fctiw and fctiwz: low word of frD gets the integer, NaN gives 0x80000000 */
static void convertToInt(PPCTranslatorContext *tc, uint8_t reg_rd, uint8_t reg_rb, bool round_to_zero)
{
    GPR tmp = GPR::allocate();
    GPR nan = GPR::allocate();

    if (round_to_zero) {
        tc->emit(fcvtzs_Dto32(tmp, reg_rb));
    }
    else {
        FPR tmp_f = FPR::allocate();

        tc->emit({
            frintxd(tmp_f, reg_rb),                 // Round with current mode
            fcvtzs_Dto32(tmp, tmp_f)
        });
    }

    tc->emit({
        sxtw64(tmp, tmp),                           // Upper half is undefined, give the sign as qemu does
        fcmpd(reg_rb, reg_rb),
        mov_immed_u16(nan, 0x8000, 1),
        csel64(tmp, nan, tmp, A64_CC_VS),
        fmov_from_reg(reg_rd, tmp)
    });
}

/* mtfsb0 and mtfsb1 */
static int setFPSCRBit(PPCTranslatorContext *tc, Opcode opcode, bool set, const char *name)
{
    /* Sanity check */
    if (opcode.illegal(0x001ff800)) return -1;

    uint8_t bt = opcode.u8(6, 10);
    uint8_t rc = opcode.u8(31, 31);

    if (rc) {
        kprintf("%s. not supported yet!", name);
        return -1;
    }

    GPR reg_fpscr = tc->mapGPRForReadAndWrite(FPSCRn);

    if (set)
        tc->emit(orr_immed(reg_fpscr, reg_fpscr, 1, (bt + 1) & 31));
    else
        tc->emit(bic_immed(reg_fpscr, reg_fpscr, 1, (bt + 1) & 31));

    /* RN is in bits 30 and 31 */
    if (bt >= 30)
        updateFPCR(tc, reg_fpscr);

    tc->advancePC(4);

    return 1;
}

int fsubx(PPCTranslatorContext *tc, Opcode opcode)
{
    return arithmetic(tc, opcode, fsubd, false, false, "fsub");
}

int faddx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return arithmetic(tc, opcode, faddd, false, false, "fadd");
}

int fmulx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return arithmetic(tc, opcode, fmuld, true, false, "fmul");
}

int fdivx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return arithmetic(tc, opcode, fdivd, false, false, "fdiv");
}

int faddsx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return arithmetic(tc, opcode, faddd, false, true, "fadds");
}

int fsubsx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return arithmetic(tc, opcode, fsubd, false, true, "fsubs");
}

int fmulsx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return arithmetic(tc, opcode, fmuld, true, true, "fmuls");
}

int fdivsx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return arithmetic(tc, opcode, fdivd, false, true, "fdivs");
}

int fmadd(PPCTranslatorContext *tc, uint32_t opcode)
{
    return multiplyAdd(tc, opcode, false, false, false, "fmadd");
}

int fmsubx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return multiplyAdd(tc, opcode, true, false, false, "fmsub");
}

int fnmaddx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return multiplyAdd(tc, opcode, false, true, false, "fnmadd");
}

int fnmsubx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return multiplyAdd(tc, opcode, true, true, false, "fnmsub");
}

int fmaddsx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return multiplyAdd(tc, opcode, false, false, true, "fmadds");
}

int fmsubsx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return multiplyAdd(tc, opcode, true, false, true, "fmsubs");
}

int fnmaddsx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return multiplyAdd(tc, opcode, false, true, true, "fnmadds");
}

int fnmsubsx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return multiplyAdd(tc, opcode, true, true, true, "fnmsubs");
}

int fsqrtx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return unary(tc, opcode, 0x001f07c0, "fsqrt", [](PPCTranslatorContext *tc, uint8_t reg_rd, uint8_t reg_rb) {
        tc->emit(fsqrtd(reg_rd, reg_rb));
    });
}

int fsqrtsx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return unary(tc, opcode, 0x001f07c0, "fsqrts", [](PPCTranslatorContext *tc, uint8_t reg_rd, uint8_t reg_rb) {
        tc->emit({
            fsqrtd(reg_rd, reg_rb),
            fcvtsd(reg_rd, reg_rd),
            fcvtds(reg_rd, reg_rd)
        });
    });
}

int fctiwx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return unary(tc, opcode, 0x001f0000, "fctiw", [](PPCTranslatorContext *tc, uint8_t reg_rd, uint8_t reg_rb) {
        convertToInt(tc, reg_rd, reg_rb, false);
    });
}

int fctiwzx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return unary(tc, opcode, 0x001f0000, "fctiwz", [](PPCTranslatorContext *tc, uint8_t reg_rd, uint8_t reg_rb) {
        convertToInt(tc, reg_rd, reg_rb, true);
    });
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

int fnegx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return unary(tc, opcode, 0x001f0000, "fneg", [](PPCTranslatorContext *tc, uint8_t reg_rd, uint8_t reg_rb) {
        tc->emit(fnegd(reg_rd, reg_rb));
    });
}

int fabsx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return unary(tc, opcode, 0x001f0000, "fabs", [](PPCTranslatorContext *tc, uint8_t reg_rd, uint8_t reg_rb) {
        tc->emit(fabsd(reg_rd, reg_rb));
    });
}

int fnabsx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return unary(tc, opcode, 0x001f0000, "fnabs", [](PPCTranslatorContext *tc, uint8_t reg_rd, uint8_t reg_rb) {
        tc->emit({
            fabsd(reg_rd, reg_rb),
            fnegd(reg_rd, reg_rd)
        });
    });
}

int frspx(PPCTranslatorContext *tc, uint32_t opcode)
{
    return unary(tc, opcode, 0x001f0000, "frsp", [](PPCTranslatorContext *tc, uint8_t reg_rd, uint8_t reg_rb) {
        tc->emit({
            fcvtsd(reg_rd, reg_rb),
            fcvtds(reg_rd, reg_rd)
        });
    });
}

/* fcmpu and fcmpo, CR field gets LT, GT, EQ or FU (unordered) */
int fcmpu(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x00600001) return -1;

    uint8_t crn = (opcode >> 23) & 7;
    uint8_t ra = (opcode >> 16) & 31;
    uint8_t rb = (opcode >> 11) & 31;

    FPR reg_rb = tc->mapFPRForRead(rb);
    FPR reg_ra = tc->mapFPRForRead(ra);
    GPR reg_cr = tc->mapGPRForReadAndWrite(CRn);
    GPR tmp = GPR::allocate();
    GPR val = GPR::allocate();

    tc->emit({
        fcmpd(reg_ra, reg_rb),
        mov_immed_u16(tmp, 1, 0),                   // FU
        mov_immed_u16(val, 8, 0),
        csel(tmp, val, tmp, A64_CC_MI),             // LT
        mov_immed_u16(val, 4, 0),
        csel(tmp, val, tmp, A64_CC_GT),             // GT
        mov_immed_u16(val, 2, 0),
        csel(tmp, val, tmp, A64_CC_EQ),             // EQ
        bfi(reg_cr, tmp, 4 * (7 - crn), 4)
    });

    tc->advancePC(4);

    return 1;
}

/* frD = frA >= 0 ? frC : frB, a NaN in frA gives frB */
int fselx(PPCTranslatorContext *tc, uint32_t opcode)
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
        kprintf("fsel. not supported yet!");
        return -1;
    }

    tc->emit({
        fcmpzd(reg_ra),
        fcseld(reg_rd, reg_rc, reg_rb, A64_CC_GE)
    });

    tc->advancePC(4);

    return 1;
}

/* frD = FPSCR, upper half zero */
int mffsx(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x001ff800) return -1;

    uint8_t rd = (opcode >> 21) & 31;
    uint8_t rc = opcode & 1;

    FPR reg_rd = tc->mapFPRForWrite(rd);
    GPR reg_fpscr = tc->mapGPRForRead(FPSCRn);
    GPR tmp = GPR::allocate();

    if (rc) {
        kprintf("mffs. not supported yet!");
        return -1;
    }

    tc->emit({
        mov_reg(tmp, reg_fpscr),
        fmov_from_reg(reg_rd, tmp)
    });

    tc->advancePC(4);

    return 1;
}

int mtfsfx(PPCTranslatorContext *tc, uint32_t opcode)
{
    uint8_t fm = (opcode >> 17) & 0xff;
    uint8_t rb = (opcode >> 11) & 31;
    uint8_t rc = opcode & 1;
    uint32_t mask = 0;

    if (rc) {
        kprintf("mtfsf. not supported yet!");
        return -1;
    }

    for (int i = 0; i < 8; i++) {
        if (fm & (1 << i))
            mask |= 15 << (4 * i);
    }

    FPR reg_rb = tc->mapFPRForRead(rb);
    GPR reg_fpscr = tc->mapGPRForReadAndWrite(FPSCRn);
    GPR tmp = GPR::allocate();
    GPR reg_mask = GPR::allocate();

    tc->emit(mov_simd_to_reg(tmp, reg_rb, TS_S, 0));
    tc->emitLoadImmediate(reg_mask, mask);
    tc->emit({
        and_reg(tmp, tmp, reg_mask, LSL, 0),
        bic_reg(reg_fpscr, reg_fpscr, reg_mask, LSL, 0),
        orr_reg(reg_fpscr, reg_fpscr, tmp, LSL, 0)
    });

    /* RN is in the last field */
    if (fm & 1)
        updateFPCR(tc, reg_fpscr);

    tc->advancePC(4);

    return 1;
}

int mtfsfix(PPCTranslatorContext *tc, uint32_t opcode)
{
    /* Sanity check */
    if (opcode & 0x007f0800) return -1;

    uint8_t bf = (opcode >> 23) & 7;
    uint8_t imm = (opcode >> 12) & 15;
    uint8_t rc = opcode & 1;

    if (rc) {
        kprintf("mtfsfi. not supported yet!");
        return -1;
    }

    GPR reg_fpscr = tc->mapGPRForReadAndWrite(FPSCRn);
    GPR tmp = GPR::allocate();

    tc->emit({
        mov_immed_u16(tmp, imm, 0),
        bfi(reg_fpscr, tmp, 4 * (7 - bf), 4)
    });

    /* RN is in the last field */
    if (bf == 7)
        updateFPCR(tc, reg_fpscr);

    tc->advancePC(4);

    return 1;
}

int mtfsb0x(PPCTranslatorContext *tc, uint32_t opcode)
{
    return setFPSCRBit(tc, opcode, false, "mtfsb0");
}

int mtfsb1x(PPCTranslatorContext *tc, uint32_t opcode)
{
    return setFPSCRBit(tc, opcode, true, "mtfsb1");
}

} // Emu68::PPC::Emit
