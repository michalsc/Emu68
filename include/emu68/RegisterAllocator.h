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
class FPU {};
class SR {};

enum class Role : uint8_t { TempReg, M68KReg, M68KAlias, M68KSpecial, TempConstant };

template< typename Arch, typename RegType > class Register;

template< typename Arch, typename RegType = INT >
class RegisterAllocator {
public:
    Register< Arch, RegType > get() {
        uint8_t regnum = __alloc_reg();
        Register< Arch, RegType > reg(regnum);
        reg._role = Role::TempReg;
        return std::move(reg);
    }
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
public:
    Register(bool alloc=false) : _role(Role::TempReg), _dirty(false) { if (alloc) { _regnum=RegisterAllocator< Arch, RegType >().allocate(); _refcount = allocator<uint32_t>().allocate(1); (*_refcount) = 1;} else {_regnum=0xff; _refcount=nullptr;}}
    Register(uint8_t regnum) : _regnum(regnum), _role(Role::TempReg), _dirty(false) { _refcount = allocator<uint32_t>().allocate(1); (*_refcount) = 1; }
    Register(uint8_t regnum, Role role) : _regnum(regnum), _role(_role), _dirty(false) { _refcount = allocator<uint32_t>().allocate(1); (*_refcount) = 1; }
    Register(Register& other) : _regnum(other._regnum), _role(other._role), _dirty(other._dirty), _refcount(other._refcount) { (*_refcount)++; }
    Register(Register&& other) : _regnum(other._regnum), _role(other._role), _dirty(other._dirty), _refcount(other._refcount) { other._refcount = nullptr; other._regnum = 0xff; }
    ~Register() { _decrease_and_release(); }
    Register& operator=(Register& other) { _decrease_and_release(); _regnum = other._regnum; _role = other._role; _dirty = other._dirty; _refcount = other._refcount; (*_refcount)++; return *this; }
    Register& operator=(const Register& other) { _decrease_and_release(); _regnum = other._regnum; _role = other._role; _dirty = other._dirty; _refcount = other._refcount; (*_refcount)++; return *this; }
    Register& operator=(Register&& other) { _decrease_and_release(); _regnum = other._regnum; _role = other._role; _dirty = other._dirty; _refcount = other._refcount; other._refcount = nullptr; other._regnum = 0xff; return *this; }
    Register& operator=(const Register&& other) { _decrease_and_release(); _regnum = other._regnum; _role = other._role; _dirty = other._dirty; _refcount = other._refcount; other._refcount = nullptr; other._regnum = 0xff; return *this; }
    void alloc() { _decrease_and_release(); _regnum = RegisterAllocator< Arch, RegType >().allocate(); _role = Role::TempReg; _dirty = false; _refcount = allocator<uint32_t>().allocate(1); (*_refcount) = 1; }
    uint8_t value() { if (_regnum == 0xff) { kprintf("[CXX:Register] Using unitialized register!\n"); } return _regnum; }
    void touch() { _dirty = true; }
    bool dirty() { return _dirty; }
private:
    void _decrease_and_release() { if (_refcount && --(*_refcount) == 0) { RegisterAllocator< Arch, RegType >().deallocate(_regnum); allocator<uint32_t>().deallocate(_refcount, 1); } }
    uint8_t _regnum;
    Role _role;
    bool _dirty;
    uint32_t* _refcount;
    friend class RegisterAllocator< Arch, RegType >;
};

}

#endif /* _EMU68_REGISTERALLOCATOR_H */
