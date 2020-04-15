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
CodeGenerator<AArch64>::CodeGenerator(uint16_t *m68k) : m68kcode(m68k), m68kptr(m68k), m68kmin(m68k), m68kmax(m68k), m68kcount(0)
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

}

#endif /* _CODEGENERATOR_AARCH64_H */
