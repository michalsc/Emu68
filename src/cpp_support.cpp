/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <cstddef>

/* Don't include A64.h */
#define _A64_H
#include "support.h"
#include "tlsf.h"

// C++ support stuff necessary when linked without libstdc++
extern "C" void __cxa_pure_virtual()
{
}

void * __dso_handle __attribute__((weak));

#define __MAX_GLOBAL_OBJECTS 1024

namespace {

struct {
    void (*destructor)(void*);
    void *arg;
    void *dso;
} __global_objects[__MAX_GLOBAL_OBJECTS];

unsigned __global_count = 0;

}

extern "C" int __cxa_atexit(void (*destructor)(void*), void *arg, void *dso)
{
    if (__global_count < __MAX_GLOBAL_OBJECTS)
    {
        __global_objects[__global_count].destructor = destructor;
        __global_objects[__global_count].arg = arg;
        __global_objects[__global_count].dso = dso;

        __global_count++;

        return 0;
    }
    else return -1;
}

extern "C" void __cxa_finalize(void (*f)(void*))
{
    if (f == nullptr)
    {
        for (unsigned i=0; i < __global_count; i++)
        {
            __global_objects[i].destructor(__global_objects[i].arg);
        }

        __global_count = 0;
    }
    else
    {
        for (unsigned i=0; i < __global_count; i++)
        {
            if (__global_objects[i].destructor == f)
            {
                __global_objects[i].destructor(__global_objects[i].arg);
                for (unsigned j = i+1; j < __global_count; j++)
                {
                    __global_objects[j-1] = __global_objects[j];
                }
                __global_count--;
            }
        }
    }
}

void *operator new(std::size_t sz)
{
    extern void *tlsf;
    void *ptr = nullptr;

    if (sz > 0x80000000) {
        kprintf("[CPP] operator ::new called with size 0x%llx above limits!", sz);
        while(1) { asm volatile("wfe"); }
    } else {
        ptr = tlsf_malloc(tlsf, sz);
        if (ptr == nullptr) {
            kprintf("[CPP] operator ::new: tlsf_malloc returned NULL!");
            while(1) { asm volatile("wfe"); }
        }
    }

    return ptr;
}

void *operator new[](std::size_t sz)
{
    extern void *tlsf;
    void *ptr = nullptr;

    if (sz > 0x80000000) {
        kprintf("[CPP] operator ::new[] called with size 0x%llx above limits!", sz);
        while(1) { asm volatile("wfe"); }
    } else {
        ptr = tlsf_malloc(tlsf, sz);
        if (ptr == nullptr) {
            kprintf("[CPP] operator ::new[]: tlsf_malloc returned NULL!");
            while(1) { asm volatile("wfe"); }
        }
    }

    return ptr;
}

void operator delete(void *p)
{
    if (p != nullptr) {
        extern void *tlsf;
        tlsf_free(tlsf, p);
    }
}

void operator delete(void *p, std::size_t)
{
    operator delete(p);
}

void operator delete[](void *p)
{
    if (p != nullptr) {
        extern void *tlsf;
        tlsf_free(tlsf, p);
    }
}

void operator delete[](void *p, std::size_t)
{
    operator delete(p);
}

#if 0
[[maybe_unused]] static bool table_initialized = []() {
    //while(1);
    kprintf("lambda\n");
    return true;
}();
#endif
