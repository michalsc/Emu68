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
    typedef std::initializer_list<typename arch::OpcodeSize> opcode_list;
    typedef std::initializer_list<opcode_list> opcode_cluster_list;
    CodeGenerator(uint16_t *m68k) : m68kcode(m68k), m68kptr(m68k), m68kmin(m68k), m68kmax(m68k), m68kcount(0), offsetPC(0) { }
    void Compile();
protected:
    void Emit(typename arch::OpcodeSize opcode) { _INSN_Stream.push_back(opcode); }
    void Emit(opcode_list opcodes) { _INSN_Stream.insert(_INSN_Stream.end(), opcodes); }
    void Emit(opcode_cluster_list opcodes) { for (auto list: opcodes) _INSN_Stream.insert(_INSN_Stream.end(), list); }
    void E(typename arch::OpcodeSize opcode) { Emit(opcode); }
    void E(std::initializer_list<typename arch::OpcodeSize> opcodes) { Emit(opcodes); }
    void E(std::initializer_list<std::initializer_list<typename arch::OpcodeSize>> opcodes) { for (auto list: opcodes) _INSN_Stream.insert(_INSN_Stream.end(), list); }
    void EmitPrologue();
    void EmitEpilogue();
    void AdvancePC(int8_t offset);
    void FlushPC();
    void FixupPC(int8_t &offset);
    void GetFPUFlags(Register<AArch64, INT> fpsr);
    void Load96BitFP(Register<AArch64, DOUBLE> fpreg, Register<AArch64, INT> base, int16_t offset9);

    template<typename, typename type>
    Register<arch, type> GetReg(RegisterRole role = RegisterRole::TempReg) {
        uint8_t reg;
        if (std::is_same<type, INT>::value) {
            reg = _RegAlloc.allocate();
            while (reg == 0xff) {
                LRU_DeallocLast();
                reg = _RegAlloc.allocate();
            }
        } else {
            reg = _FPUAlloc.allocate();
        }
        return Register<arch, type>(reg, role);
    }
    Register<arch, INT> GetDn(uint8_t n) {
        n &= 7;
        if (!D[n].valid()) {
            D[n] = GetReg<arch, INT>(RegisterRole::Dn + n);
            LoadReg(D[n]);
        }
        if (arch::DynamicDn) {
            LRU_MoveToFront(&D[n]);
        }
        return D[n];
    }
    Register<arch, INT> GetDnNoLoad(uint8_t n) {
        n &= 7;
        if (!D[n].valid()) {
            D[n] = AllocReg(RegisterRole::Dn + n);
        }
        if (arch::DynamicDn) {
            LRU_MoveToFront(&D[n]);
        }
        return D[n];
    }
    Register<arch, INT> GetAn(uint8_t n) {
        n &= 7;
        if (!A[n].valid()) {
            A[n] = GetReg<arch, INT>(RegisterRole::An + n);
            LoadReg(A[n]);
        }
        if (arch::DynamicAn) {
            LRU_MoveToFront(&A[n]);
        }
        return A[n];
    }
    Register<arch, INT> GetAnNoLoad(uint8_t n) {
        n &= 7;
        if (!A[n].valid()) {
            A[n] = AllocReg(RegisterRole::An + n);
        }
        if (arch::DynamicAn) {
            LRU_MoveToFront(&A[n]);
        }
        return A[n];
    }
    Register<arch, INT> GetCTX() {
        if (!CTX.valid()) {
            CTX = GetReg<arch, INT>(RegisterRole::CTX);
            LoadReg(CTX);
        }
        LRU_MoveToFront(&CTX);
        return CTX;
    }
    Register<arch, INT> GetCC() {
        if (!CC.valid()) {
            CC = GetReg<arch, INT>(RegisterRole::SR);
            LoadReg(CC);
        }
        LRU_MoveToFront(&CC);
        return CC;
    }
    Register<arch, INT> GetFPCR() {
        if (!FPCR.valid()) {
            FPCR = GetReg<arch, INT>(RegisterRole::FPCR);
            LoadReg(FPCR);
        }
        LRU_MoveToFront(&FPCR);
        return FPCR;
    }
    Register<arch, INT> GetFPSR() {
        if (!FPSR.valid()) {
            FPSR = GetReg<arch, INT>(RegisterRole::FPSR);
            LoadReg(FPSR);
        }
        LRU_MoveToFront(&FPSR);
        return FPSR;
    }
    uint8_t GetEALength(uint8_t ea, uint16_t brief, uint8_t imm_size);
private:
    void LoadReg(Register<arch, INT> dest);
    void SaveReg(Register<arch, INT> src);
    void LRU_MoveToFront(Register<arch, INT> *reg) {
        for (auto it=_LRU.begin(); it != _LRU.end(); ++it) {
            if (*it == reg) {
                _LRU.erase(it);
            }
        }
        _LRU.insert(_LRU.begin(), reg);
    }
    void LRU_DeallocLast() {
        if (_LRU.size() > 0) {
            Register<arch, INT> *reg = *_LRU.rbegin();
            if (reg->dirty()) {
                SaveReg(*reg);
            }
            if (reg->refcnt()) {
                kprintf("[CXX] CodeGenerator::lru_dealloc_last: Register %d, role %d has refcnt of %d, yet it is going to be destroyed!\n", reg->value(), static_cast<int>(reg->role()), reg->refcnt());
            }
            *reg = Register<arch, INT>();
            _LRU.pop_back();
        }
    }
    const uint16_t *m68kcode;
    uint16_t *m68kptr;
    uint16_t *m68kmin;
    uint16_t *m68kmax;
    uint32_t m68kcount;
    int32_t  offsetPC;
    tinystd::vector< typename arch::OpcodeSize, jit_allocator<typename arch::OpcodeSize> > _INSN_Stream;
    tinystd::vector< uint16_t *, allocator<uint16_t *> > _ReturnStack;
    tinystd::vector< Register<arch, INT> *, allocator<Register<arch, INT> *> > _LRU;
    RegisterAllocator< arch, INT > _RegAlloc;
    RegisterAllocator< arch, DOUBLE > _FPUAlloc;
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

#include <emu68/codegen/Base.h>

#endif /* _EMU68_CODEGENERATOR_H */
