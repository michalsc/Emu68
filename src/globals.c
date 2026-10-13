#include "support.h"
#include "config.h"
#include "globals.h"

struct Globals globals = {
    .zorro_disable = false,
    .fpu_disable = false,
    .ppc_enable = false,
    .scsi_disable = false,
    .beamcon0_pal_clear = false,
    .beamcon0_pal_set = false,

    .fast_page_zero = true,

    .trace_branch_flag = false,
    .debug_not_implemented = false,
    .disasm = false,
    .dcache_mask_bits = 0,
    .debug = 0,

    .debug_range_min = 0x00000000,
    .debug_range_max = 0xffffffff,

    .m68k_jit_phys_base = NULL,
    .ppc_jit_phys_base = NULL,
    .m68k_jit_virt_base = NULL,
    .ppc_jit_virt_base = NULL
};
