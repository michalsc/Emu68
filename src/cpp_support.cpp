
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
#if 0
[[maybe_unused]] static bool table_initialized = []() {
    //while(1);
    kprintf("lambda\n");
    return true;
}();
#endif
