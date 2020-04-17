/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _EMU68_REGISTERALLOCATOR_H
#define _EMU68_REGISTERALLOCATOR_H

#include <stdint.h>
#include <emu68/Allocators.h>
#include <emu68/CodeGenerator.h>

namespace emu68 {

class INT {};
class DOUBLE {};
class SINGLE {};

enum RegisterRole : uint8_t { Dn, An=Dn + 8, FPn = An + 8, PC = FPn + 8, SR, FPCR, FPSR, CTX, TempReg, TempConstant };

template< typename Arch, typename RegType > class Register;

template< typename Arch, typename RegType = INT >
class RegisterAllocator {
public:
    uint8_t allocate() { return __alloc_reg(); }
    void deallocate(uint8_t r) { __free_reg(r); }
private:
    static uint32_t _register_pool;

    void __free_reg(uint8_t reg) {
        const int max = std::is_same<RegType, INT>::value ? Arch().RegEnd : Arch().FPURegEnd;
        const int min = std::is_same<RegType, INT>::value ? Arch().RegStart : Arch().FPURegStart;
        if ((reg >= min) && (reg <= max)) {
            _register_pool &= ~(1 << (reg-min));
        }
    }

    uint8_t __alloc_reg() {
        const int max = std::is_same<RegType, INT>::value ? Arch().RegEnd : Arch().FPURegEnd;
        const int min = std::is_same<RegType, INT>::value ? Arch().RegStart : Arch().FPURegStart;
        int reg = __builtin_ctz(~_register_pool);
        if (reg <= (max - min)) {
            _register_pool |= 1 << reg;
            return reg + min;
        }
        return 0xff;
    }
};

template< typename arch, typename reg >
uint32_t RegisterAllocator<arch, reg>::_register_pool;

template< typename Arch, typename RegType = INT >
class Register {
private:
    struct _RefCountAndDirty {
        uint32_t _cnt;
        bool _dirty;
    };
public:
    Register() : _fixed(false), _role(RegisterRole::TempReg) { _regnum=RegisterAllocator< Arch, RegType >().allocate(); _refcount = allocator<_RefCountAndDirty>().allocate(1); _refcount->_cnt = 1; _refcount->_dirty = false; }
    explicit Register(uint8_t regnum) : _fixed(true), _regnum(regnum), _role(RegisterRole::TempReg), _refcount(nullptr) {  }
    Register(uint8_t regnum, RegisterRole role) : _fixed(false), _regnum(regnum), _role(role) { _refcount = allocator<_RefCountAndDirty>().allocate(1); _refcount->_cnt = 1; _refcount->_dirty = false; }
    Register(Register& other) : _fixed(other._fixed), _regnum(other._regnum), _role(other._role), _refcount(other._refcount) { if (_refcount) _refcount->_cnt++; }
    Register(Register&& other) : _fixed(other._fixed), _regnum(other._regnum), _role(other._role), _refcount(other._refcount) { other._refcount = nullptr; other._regnum = 0xff; }
    ~Register() { if (!_fixed) _decrease_and_release(); }
    Register& operator=(Register& other) { _decrease_and_release(); _regnum = other._regnum; _role = other._role; _refcount = other._refcount; if (_refcount) _refcount->_cnt++; return *this; }
    Register& operator=(const Register& other) { _decrease_and_release(); _regnum = other._regnum; _role = other._role; _refcount = other._refcount; if (_refcount) _refcount->_cnt++; return *this; }
    Register& operator=(Register&& other) { _decrease_and_release(); _regnum = other._regnum; _role = other._role; _refcount = other._refcount; other._refcount = nullptr; other._regnum = 0xff; return *this; }
    Register& operator=(const Register&& other) { _decrease_and_release(); _regnum = other._regnum; _role = other._role; _refcount = other._refcount; other._refcount = nullptr; other._regnum = 0xff; return *this; }
    void alloc() { _decrease_and_release(); _regnum = RegisterAllocator< Arch, RegType >().allocate(); _role = RegisterRole::TempReg; _refcount = allocator<_RefCountAndDirty>().allocate(1); _refcount->_cnt = 1; _refcount->_dirty = false; }
    bool allocated() const { const int max = std::is_same<RegType, INT>::value ? Arch().RegEnd : Arch().FPURegEnd; const int min = std::is_same<RegType, INT>::value ? Arch().RegStart : Arch().FPURegStart; if ((_regnum >= min) && (_regnum <= max)) {return true;} else { return false;} }
    uint8_t value() const { if (_regnum == 0xff) { kprintf("[CXX] Register::value() Using unitialized register!\n"); } return _regnum; }
    bool valid() const { return _regnum != 0xff; }
    void touch() { if (_refcount) { _refcount->_dirty = true; } }
    bool dirty() const { if (_refcount) { return _refcount->_dirty; } else return false; }
    uint32_t refcnt() const { if (_refcount) { return _refcount->_cnt; } else return 0; }
    RegisterRole role() const { return _role; }
private:
    void _decrease_and_release() { if (_refcount && --(_refcount->_cnt) == 0) { RegisterAllocator< Arch, RegType >().deallocate(_regnum); allocator<_RefCountAndDirty>().deallocate(_refcount, 1); } }
    const bool _fixed;
    uint8_t _regnum;
    RegisterRole _role;
    _RefCountAndDirty *_refcount;
    friend class RegisterAllocator< Arch, RegType >;
};


}

#endif /* _EMU68_REGISTERALLOCATOR_H */
