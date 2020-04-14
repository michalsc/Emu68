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
    typedef Register<arch, INT> int_reg;
    typedef Register<arch, DOUBLE> double_reg;
    typedef Register<arch, SINGLE> float_reg;

    CodeGenerator(uint16_t *m68k) : m68kcode(m68k), m68kptr(m68k), m68kmin(m68k), m68kmax(m68k) { }
//protected:
    void Emit(typename arch::OpcodeSize opcode) { _INSN_Stream.push_back(opcode); }
    void Emit(std::initializer_list<typename arch::OpcodeSize> opcodes) { _INSN_Stream.insert(_INSN_Stream.end(), opcodes); }
    void E(typename arch::OpcodeSize opcode) { Emit(opcode); }
    void E(std::initializer_list<typename arch::OpcodeSize> opcodes) { Emit(opcodes); }
    int_reg allocReg(RegisterRole role = RegisterRole::TempReg) {
        uint8_t reg = _regalloc.allocate();
        while (reg == 0xff) {
            lru_dealloc_last();
            reg = _regalloc.allocate();
        }
        return int_reg(reg, role);
    }
    double_reg allocDOUBLEReg() {
        uint8_t reg = _fpualloc.allocate();
        return Register<arch, DOUBLE>(reg, RegisterRole::TempReg);
    }
    float_reg allocSINGLEReg() {
        uint8_t reg = _fpualloc.allocate();
        return Register<arch, SINGLE>(reg, RegisterRole::TempReg);
    }
    int_reg getCTX() {
        if (!CTX.valid()) {
            CTX = allocReg(RegisterRole::CTX);
            loadCTX(CTX);
        }
        lru_move_to_front(&CTX);
        return CTX;
    }
    int_reg getCC() {
        if (!CC.valid()) {
            CC = allocReg(RegisterRole::SR);
            loadCC(CC);
        }
        lru_move_to_front(&CC);
        return CC;
    }
    int_reg getFPCR() {
        if (!FPCR.valid()) {
            FPCR = allocReg(RegisterRole::FPCR);
            loadFPCR(FPCR);
        }
        lru_move_to_front(&FPCR);
        return FPCR;
    }
    int_reg getFPSR() {
        if (!FPSR.valid()) {
            FPSR = allocReg(RegisterRole::FPSR);
            loadFPSR(FPSR);
        }
        lru_move_to_front(&FPSR);
        return FPSR;
    }
private:
    void loadFPCR(Register<arch, INT> dest);
    void loadFPSR(Register<arch, INT> dest);
    void loadCC(Register<arch, INT> dest);
    void loadCTX(Register<arch, INT> dest);
    void saveFPCR(Register<arch, INT> src);
    void saveFPSR(Register<arch, INT> src);
    void saveCC(Register<arch, INT> src);
    void lru_move_to_front(Register<arch, INT> *reg) {
        for (auto it=_lru.begin(); it != _lru.end(); ++it) {
            if (*it == reg) {
                _lru.erase(it);
            }
        }
        _lru.insert(_lru.begin(), reg);
    }
    void lru_dealloc_last() {
        if (_lru.size() > 0) {
            Register<arch, INT> *reg = *_lru.rbegin();
            if (reg->dirty()) {

            }
            **_lru.rbegin() = Register<arch, INT>();
            _lru.pop_back();
        }
    }
    const uint16_t *m68kcode;
    uint16_t *m68kptr;
    uint16_t *m68kmin;
    uint16_t *m68kmax;
    tinystd::vector< typename arch::OpcodeSize, jit_allocator<typename arch::OpcodeSize> > _INSN_Stream;
    tinystd::vector< uint16_t *, allocator<uint16_t *> > _return_stack;
    tinystd::vector< int_reg *, allocator<int_reg *> > _lru;
    RegisterAllocator< arch, INT > _regalloc;
    RegisterAllocator< arch, DOUBLE > _fpualloc;
    Register< arch, INT > D[8];
    Register< arch, INT > A[8];
    Register< arch, INT > PC;
    Register< arch, INT > CC;
    Register< arch, INT > CTX;
    Register< arch, INT > FPCR;
    Register< arch, INT > FPSR;
    Register< arch, DOUBLE > FP[8];
};

}

#endif /* _EMU68_CODEGENERATOR_H */
