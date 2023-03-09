#include <boards.h>
#include <mmu.h>
#include <A64.h>
#include <support.h>

__attribute__((aligned(4096)))
#include "./emmc.h"

/*
    This is a Z3 ROM board with SDHC driver. More details can be found in the Emu68-tools repository.
    The board is provided with its own m68k ROM with the driver inside. No ARM-side code is used in this board.
*/

static void map(struct ExpansionBoard *board)
{
    kprintf("[BOARD] Mapping ZIII emmc board at address %08x\n", board->map_base);
    mmu_map(mmu_virt2phys((uintptr_t)board->rom_file), board->map_base, board->rom_size, MMU_ACCESS | MMU_ISHARE | MMU_ALLOW_EL0 | MMU_READ_ONLY | MMU_ATTR_CACHED, 0);
}

static struct ExpansionBoard board = {
    brcm_emmc_bin,
    sizeof(brcm_emmc_bin),
    0,
    1,
    1,
    map
};

static void * __attribute__((used, section(".boards.z3"))) _board = &board;
