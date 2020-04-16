/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _CODEGENERATOR_AARCH64_H
#define _CODEGENERATOR_AARCH64_H

#include <emu68/M68KState.h>
#include <emu68/CodeGenerator.h>
#include <emu68/aarch64/opcodes.h>

namespace emu68 {


template<>
CodeGenerator<AArch64>::CodeGenerator(uint16_t *m68k) : m68kcode(m68k), m68kptr(m68k), m68kmin(m68k), m68kmax(m68k), m68kcount(0), offsetPC(0)
{
    PC = Register<AArch64, INT>(13, RegisterRole::PC);
    for (int i=0; i < 8; i++)
    {
        FP[i] = Register<AArch64, DOUBLE>(i + 8, static_cast<RegisterRole>(RegisterRole::FPn + i));
        D[i] = Register<AArch64, INT>(i + 14, static_cast<RegisterRole>(RegisterRole::Dn + i));
        A[i] = Register<AArch64, INT>(i + 14 + 8, static_cast<RegisterRole>(RegisterRole::An + i));
    }
    _INSN_Stream.reserve(1024);
}

template<>
void CodeGenerator<AArch64>::FlushPC()
{
    Emit(
        offsetPC > 0 ? ADD(PC, PC, offsetPC) : SUB(PC, PC, -offsetPC)
    );

    offsetPC = 0;
}

template<>
void CodeGenerator<AArch64>::AdvancePC(int8_t offset)
{
    offsetPC += (int32_t)offset;

    // If overflow would occur then compute PC and get new offset
    if (offsetPC > 120 || offsetPC < -120)
    {
        FlushPC();
    }
}

template<>
void CodeGenerator<AArch64>::FixupPC(int8_t &offset)
{
    // Calculate new PC relative offset
    int32_t new_offset = offsetPC + offset;

    // If overflow would occur then compute PC and get new offset
    if (new_offset > 127 || new_offset < -127)
    {
        FlushPC();
        offsetPC = 0;
    }
    else
        offset = new_offset;
}

template<>
void CodeGenerator<AArch64>::EmitPrologue()
{

}

template<>
void CodeGenerator<AArch64>::EmitEpilogue()
{
    Emit({
        RET()
    });
}

template<>
void CodeGenerator<AArch64>::LoadReg(Register<AArch64, INT> dest)
{
    if (dest.role() == RegisterRole::CTX) Emit({
        MRS<3, 3, 13, 0, 3>(dest)
    });
    if (dest.role() == RegisterRole::SR) Emit({
        MRS<3, 3, 13, 0, 2>(dest)
    });
    else if (dest.role() == RegisterRole::FPCR) Emit({
        LDRH(dest, GetCTX(), __builtin_offsetof(struct M68KState, FPCR))
    });
    else if (dest.role() == RegisterRole::FPSR) Emit({
        LDR(dest, GetCTX(), __builtin_offsetof(struct M68KState, FPSR))
    });
}

template<>
void CodeGenerator<AArch64>::SaveReg(Register<AArch64, INT> src)
{
    if (src.role() == RegisterRole::SR) Emit({
        MSR<3, 3, 13, 0, 2>(src)
    });
    else if (src.role() == RegisterRole::FPCR) Emit({
        STRH(src, GetCTX(), __builtin_offsetof(struct M68KState, FPCR))
    });
    else if (src.role() == RegisterRole::FPSR) Emit({
        STR(src, GetCTX(), __builtin_offsetof(struct M68KState, FPSR))
    });
}

template<>
void CodeGenerator<AArch64>::GetFPUFlags(Register<AArch64, INT> fpsr)
{
    auto tmp = AllocReg();
    Emit({
        GET_NZCV    (tmp),
        BIC         (tmp, tmp, 1, 3),
        ROR         (tmp, tmp, 28),
        BFI         (fpsr, tmp, 24, 4)
    });
}

template<>
void CodeGenerator<AArch64>::Load96BitFP(Register<AArch64, DOUBLE> fpreg, Register<AArch64, INT> base, int16_t offset9)
{
    auto zero = Register<AArch64, INT>(31, false);
    auto exp_reg = AllocReg();
    auto mant_reg = AllocReg();
    auto tmp_reg = AllocReg();

    Emit({
        LDUR                     (exp_reg, base, offset9),
        MOV                      (tmp_reg, 0xc400),
        CMP                      (exp_reg, zero),
        LDUR<X64>                (mant_reg, base, offset9+4),
        LSR<X64>                 (mant_reg, mant_reg, 11),
        ADD<W32, SHIFT::LSR, 16> (tmp_reg, tmp_reg, exp_reg),
        BFI<X64>                 (mant_reg, tmp_reg, 52, 11),
        CSET                     (tmp_reg, CC::MI),
        BFI<X64>                 (mant_reg, tmp_reg, 63, 1),
        MOV                      (fpreg, mant_reg)
    });
}

}

#endif /* _CODEGENERATOR_AARCH64_H */
