#pragma once

#include <stdint.h>

struct Globals
{
    // Globals in /emu68
    bool zorro_disable;
    bool fpu_disable;
    bool ppc_enable;
    bool scsi_disable;
    bool beamcon0_pal_clear;
    bool beamcon0_pal_set;

    // Globals in /emu68/defaults
    bool fast_page_zero;

    // Trace mode: set by an interpreted instruction that constitutes
    // a "change of flow" per the trace-on-change-of-flow (T0) mode.
    // Cleared at the start of every TraceInterpreterLoop() iteration.
    bool trace_branch_flag;
    bool debug_not_implemented;
    bool disasm;
    bool block_c0;
    int dcache_mask_bits;
    int debug;

    /* Debug ranges */
    uint32_t debug_range_min;
    uint32_t debug_range_max;

    // Location of JIT buffers for M68k and PPC
    void* m68k_jit_phys_base;
    void* ppc_jit_phys_base;
    void* m68k_jit_virt_base;
    void* ppc_jit_virt_base;
};

#ifndef __cplusplus
static_assert(sizeof(_Bool) == 1, "bool is not 1 byte");
#endif

extern struct Globals globals;
