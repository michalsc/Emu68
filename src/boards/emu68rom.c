#include <boards.h>
#include <mmu.h>
#include <A64.h>
#include <support.h>
#include <HunkLoader.h>
#include <tlsf.h>

__attribute__((aligned(4096)))
#include "./emu68rom.h"
#include "./unicam_resource.h"
#include "./brcm-sdhc_device.h"
#include "./brcm-emmc_device.h"
#include "./68040_library.h"

static struct { 
    void *rom_base;
    uint32_t rom_size;
    uint32_t load_size;
} roms[] = {
    { unicam_resource, sizeof(unicam_resource), 0 },
    { brcm_sdhc_device, sizeof(brcm_sdhc_device), 0 },
    { brcm_emmc_device, sizeof(brcm_emmc_device), 0 },
    { __68040_library, sizeof(__68040_library), 0 },
    { NULL, 0, 0 }
};

/*
    This is a Z3 ROM board with SDHC driver. More details can be found in the Emu68-tools repository.
    The board is provided with its own m68k ROM with the driver inside. No ARM-side code is used in this board.
*/

static void map(struct ExpansionBoard *board)
{
    uint32_t total_length = 0;
    void *rom_modules = 0;
    void *virt_base = (void*)((uintptr_t)board->map_base + 0x1000);

    kprintf("[BOARD] Mapping ZIII Emu68 ROM board at address %08x\n", board->map_base);
    mmu_map(mmu_virt2phys((uintptr_t)board->rom_file), board->map_base, board->rom_size, MMU_ACCESS | MMU_ISHARE | MMU_ALLOW_EL0 | MMU_READ_ONLY | MMU_ATTR_CACHED, 0);

    for (int i=0; roms[i].rom_base; i++)
    {
        roms[i].load_size = GetHunkFileSize(roms[i].rom_base);
        if (roms[i].load_size > 0) total_length += roms[i].load_size;
    }

    total_length = (total_length + 4095) & ~4095;

    rom_modules = tlsf_malloc_aligned(tlsf, total_length, 4096);

    kprintf("[BOARD] Emu68 ROM needs %ld extra bytes\n", total_length);
    //kprintf("[BOARD]   buffer at %p\n", rom_modules);

    mmu_map(mmu_virt2phys((uintptr_t)rom_modules), board->map_base + 0x1000, total_length, MMU_ACCESS | MMU_ISHARE | MMU_ALLOW_EL0 | MMU_READ_ONLY | MMU_ATTR_CACHED, 0);

    for (int i=0; roms[i].rom_base; i++)
    {
        LoadHunkFile(roms[i].rom_base, rom_modules, virt_base);

        rom_modules = (void*)((uintptr_t)rom_modules + roms[i].load_size);
        virt_base = (void*)((uintptr_t)virt_base + roms[i].load_size);
    }
}

static struct ExpansionBoard board = {
    emu68rom_bin,
    sizeof(emu68rom_bin),
    0,
    1,
    1,
    map
};

static void * __attribute__((used, section(".boards.z3"))) _board = &board;
