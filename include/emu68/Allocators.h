/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _EMU64_ALLOCATORS_H
#define _EMU64_ALLOCATORS_H

#include <tlsf.h>

namespace emu68 {

template <class T>
class jit_allocator {

public:
    typedef T                   value_type;
    typedef value_type*         pointer;
    typedef value_type&         reference;
    typedef const value_type&   const_reference;
    typedef const value_type*   const_pointer;
    typedef uintptr_t           size_type;
    typedef ptrdiff_t           difference_type;

    jit_allocator() {};
    jit_allocator(const jit_allocator&) : jit_allocator() {};
    ~jit_allocator() {};
    pointer address(reference x) { return &x; }
    const_pointer address(const_reference x) { return &x; }
    pointer allocate(size_type n) { pointer p = (pointer)tlsf_malloc(jit_tlsf, n * sizeof(value_type)); return p; }
    void deallocate(pointer p, size_type n) { if (n>0 && p!=nullptr) tlsf_free(jit_tlsf, (void*)p); }
    size_type max_size() { return (size_type)-1 / sizeof(value_type); }
    void construct(pointer p, const_reference val) { new((void*)p) value_type(val); }
    void destroy(pointer p) { p->~value_type(); }
};

template <class T>
class allocator {

public:
    typedef T                   value_type;
    typedef value_type*         pointer;
    typedef value_type&         reference;
    typedef const value_type&   const_reference;
    typedef const value_type*   const_pointer;
    typedef uintptr_t           size_type;
    typedef ptrdiff_t           difference_type;

    allocator() {};
    allocator(const allocator&) : allocator() {};
    ~allocator() {};
    pointer address(reference x) { return &x; }
    const_pointer address(const_reference x) { return &x; }
    pointer allocate(size_type n) { pointer p = (pointer)tlsf_malloc(tlsf, n * sizeof(value_type)); return p; }
    void deallocate(pointer p, size_type n) { if (n>0 && p!=nullptr) tlsf_free(tlsf, (void*)p); }
    size_type max_size() { return (size_type)-1 / sizeof(value_type); }
    void construct(pointer p, const_reference val) { new((void*)p) value_type(val); }
    void destroy(pointer p) { p->~value_type(); }
};

}

#endif /* _EMU64_ALLOCATORS_H */
