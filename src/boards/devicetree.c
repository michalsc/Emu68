#include <boards.h>
#include <mmu.h>
#include <A64.h>
#include <devicetree.h>
#include <support.h>
#include "./devicetree.h"

static void map(struct ExpansionBoard *board)
{
    kprintf("[BOARD] Mapping ZIII devicetree board at address %08x\n", board->map_base);
    mmu_map((uintptr_t)board->rom_file, board->map_base, board->rom_size, MMU_ACCESS | MMU_ISHARE | MMU_ALLOW_EL0 | MMU_READ_ONLY | MMU_ATTR(0), 0);
    mmu_map(mmu_virt2phys((uintptr_t)dt_fdt_base()), board->map_base + board->rom_size, (dt_total_size() + 4095) & ~4096, MMU_ACCESS | MMU_ISHARE | MMU_ALLOW_EL0 | MMU_READ_ONLY | MMU_ATTR(0), 0);
}

static struct ExpansionBoard board = {
    devicetree_bin,
    4096,
    0,
    1,
    map
};

static void * __attribute__((used, section(".boards.z3"))) _board = &board;
