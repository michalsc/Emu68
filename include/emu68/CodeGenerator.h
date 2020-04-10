/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _EMU68_CODEGENERATOR_H
#define _EMU68_CODEGENERATOR_H

#include <stdint.h>
#include <support.h>
#include <tinystl/vector>
#include <emu68/Architectures.h>
#include <emu68/Allocators.h>
#include <emu68/RegisterAllocator.h>

namespace emu68 {

template< typename arch >
class CodeGenerator {
public:
    CodeGenerator(uint16_t *m68k) : m68kcode(m68k), m68kptr(m68k), m68kmin(m68k), m68kmax(m68k) { }
protected:
    void Emit(uint32_t opcode) { _INSN_Stream.push_back(opcode); }
private:
    const uint16_t *m68kcode;
    uint16_t *m68kptr;
    uint16_t *m68kmin;
    uint16_t *m68kmax;
    tinystd::vector< uint32_t, jit_allocator<uint32_t> > _INSN_Stream;
    tinystd::vector< uint16_t *, allocator<uint16_t *> > _return_stack;
    RegisterAllocator< arch, INT > _regalloc;
    RegisterAllocator< arch, FPU > _fpualloc;
    Register< arch, INT > D[8];
    Register< arch, INT > A[8];
    Register< arch, SR > CC;
    Register< arch, SR > CTX;
    Register< arch, SR > FPCR;
    Register< arch, SR > FPSR;
    Register< arch, FPU > FP[8];
};

}

#endif /* _EMU68_CODEGENERATOR_H */
