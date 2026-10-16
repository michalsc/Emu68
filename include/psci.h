#pragma once

#include <stdint.h>

static inline uint32_t psci_version()
{
    register uint64_t x0 __asm__("x0") = 0x84000000;

    asm volatile("smc #0"
        : "+r"(x0)
        :
        : "memory", "cc", "x1", "x2", "x3", "x4", "x5", "x6", "x7");
    
    return x0;
}

static inline uint32_t psci_cpu_suspend(uint32_t power_state, uint64_t entry_point_address, uint64_t context_id)
{
    register uint64_t x0 __asm__("x0") = 0xc4000001;
    register uint64_t x1 __asm__("x1") = power_state;
    register uint64_t x2 __asm__("x2") = entry_point_address;
    register uint64_t x3 __asm__("x3") = context_id;

    asm volatile("smc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x3)
        : "memory", "cc", "x1", "x2", "x3", "x4", "x5", "x6", "x7");
    
    return x0;
}

static inline uint32_t psci_cpu_off()
{
    register uint64_t x0 __asm__("x0") = 0x84000002;

    asm volatile("smc #0"
        : "+r"(x0)
        :
        : "memory", "cc", "x1", "x2", "x3", "x4", "x5", "x6", "x7");
    
    return x0;
}


static inline uint32_t psci_cpu_on(uint64_t target_cpu, uint64_t entry_point_address, uint64_t context_id)
{
    register uint64_t x0 __asm__("x0") = 0xc4000003;
    register uint64_t x1 __asm__("x1") = target_cpu;
    register uint64_t x2 __asm__("x2") = entry_point_address;
    register uint64_t x3 __asm__("x3") = context_id;

    asm volatile("smc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x3)
        : "memory", "cc", "x4", "x5", "x6", "x7");
    
    return x0;
}
