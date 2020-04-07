#ifndef _EMU68_REGISTERALLOCATOR_H
#define _EMU68_REGISTERALLOCATOR_H

/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <tinystl/allocator>
#include <stdint.h>
#include <emu68/CodeGenerator.h>

class IntegerRegister {};
class FloatingPointRegister {};

template< typename RegType = IntegerRegister >
class Register {
public:
    Register() : _regnum(0xff), _refcount(nullptr) {}
    Register(uint8_t regnum) : _regnum(regnum) { _refcount = tinystd::allocator<uint32_t>().allocate(1); }
    Register(Register& other) : _regnum(other._regnum), _refcount(other._refcount) { (*_refcount)++; }
    Register(Register&& other) : _regnum(other._regnum), _refcount(other._refcount) { other._refcount = nullptr; other._regnum = 0xff; }
    ~Register() { if (--(*_refcount) == 0) { tinystd::allocator<uint32_t>().deallocate(_refcount, 1); } }
    Register& operator=(Register& other) { if (_refcount) { (*_refcount)--; } _regnum = other._regnum; _refcount = other._refcount; (*_refcount)++; return *this; }
    Register& operator=(const Register& other) { if (_refcount) { (*_refcount)--; } _regnum = other._regnum; _refcount = other._refcount; (*_refcount)++; return *this; }
    Register& operator=(Register&& other) { if (_refcount) { (*_refcount)--; } _regnum = other._regnum; _refcount = other._refcount; (*_refcount)++; other._refcount = nullptr; other._regnum = 0xff; return *this; }
    Register& operator=(const Register&& other) { if (_refcount) { (*_refcount)--; } _regnum = other._regnum; _refcount = other._refcount; (*_refcount)++; return *this; }

    uint8_t value() { return _regnum; }
private:
    uint8_t _regnum;
    uint32_t* _refcount;
};

#if 0
template< uint8_t MaxRegCount = 8, uint8_t MaxFPURegCount = 8 >
class RegisterAllocator {
public:
    class Register {
        private:
            uint8_t _regnum;
        public:
            Register(uint8_t reg) : _regnum(reg) {};
    };

    RegisterAllocator(CodeGenerator& cgen) : _int_register_pool(0), _int_changed_mask(0), _fpu_register_pool(0), _fpu_changed_mask(0), _codegen(cgen) {};

private:
    uint32_t        _int_register_pool;
    uint32_t        _int_changed_mask;
    uint32_t        _fpu_register_pool;
    uint32_t        _fpu_changed_mask;
    CodeGenerator&  _codegen;

    void __int_free_reg(uint8_t reg) {
        if (reg < MaxRegCount) {
            _int_register_pool &= ~(1 << reg);
        }
    }
    void __fpu_free_reg(uint8_t reg) {
        if (reg < MaxFPURegCount) {
            _fpu_register_pool &= ~(1 << reg);
        }
    }
    uint8_t __int_alloc_reg() {
        int reg = __builtin_ctz(~_int_register_pool);
        if (reg < MaxRegCount) {
            _int_register_pool |= 1 << reg;
            _int_changed_mask |= 1 << reg;
            return reg;
        }
        return 0xff;
    }
    uint8_t __fpu_alloc_reg() {
        int reg = __builtin_ctz(~_fpu_register_pool);
        if (reg < MaxFPURegCount) {
            _fpu_register_pool |= 1 << reg;
            _fpu_changed_mask |= 1 << reg;
            return reg;
        }
        return 0xff;
    }
};
#endif
#endif /* _EMU68_REGISTERALLOCATOR_H */
