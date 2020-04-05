extern "C" {

#include "support.h"
#include "tlsf.h"

}

void * AllocMem(int size, int type)
{
    (void)type;
    int req_size = (size + 3) & ~3;
    int *ptr = reinterpret_cast<int *>(tlsf_malloc(tlsf, req_size + 32));
    ptr[0] = size;
    ptr[1] = 0xdeadbeef;
    ptr[2] = 0xdeadbeef;
    ptr[3] = 0xdeadbeef;

    ptr[4 + req_size/4] = 0xcafebabe;
    ptr[5 + req_size/4] = 0xcafebabe;
    ptr[6 + req_size/4] = 0xcafebabe;
    ptr[7 + req_size/4] = 0xcafebabe;

    bzero(&ptr[4], size);

    return &ptr[4];
}

void FreeMem(void *ptr, int size)
{
    unsigned int *p = reinterpret_cast<unsigned int *>(ptr);

    p -= 4;

    if (*p != (unsigned int)size)
        kprintf("[C++] Size mismatch at FreeMem!! %d != %d\n", *p, size);

    size = (size + 3) & ~3;
    if (p[1] != 0xdeadbeef || p[2] != 0xdeadbeef || p[3] != 0xdeadbeef)
    {
        kprintf("FreeMem(): left wall damaged %08x%08x%08x\n", p[1], p[2], p[3]);
    }
    if (p[4 + size/4] != 0xcafebabe || p[5+size/4] != 0xcafebabe || p[6+size/4] != 0xcafebabe || p[7+size/4] != 0xcafebabe)
    {
        kprintf("FreeMem(): right wall damaged %08x%08x%08x%08x\n", p[4 + size/4], p[5+size/4], p[6+size/4],p[7+size/4]);
    }

    tlsf_free(tlsf, p);
}

void CopyMem(const void *src, void *dst, int size)
{
    memmove(dst, src, size);
}

void SetMem(void *dst, int size, char fill)
{
    memset(dst, (int)fill, size);
}

// C++ support stuff necessary when linked without libstdc++
extern "C" void __cxa_pure_virtual()
{
}

void * __dso_handle __attribute__((weak));

#define __MAX_GLOBAL_OBJECTS 256

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



#include <emu68/RegisterAllocator.h>
#include <emu68/CodeGenerator.h>

void foo() {
    CodeGenerator cgen;
    RegisterAllocator<> regalloc(cgen);
}
