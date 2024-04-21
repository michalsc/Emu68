#include <boards.h>
#include <mmu.h>
#include <A64.h>
#include <support.h>
#include <HunkLoader.h>
#include <tlsf.h>
#include <devicetree.h>
#include <DuffCopy.h>

__attribute__((aligned(4096)))
#include "./emu68rom.h"
#include "./devicetree.resource.h"
#include "./unicam.resource.h"
#include "./brcm-sdhc.device.h"
#include "./brcm-emmc.device.h"
#include "./68040.library.h"
#include "./gic.resource.h"

static struct { 
    void *rom_base;
    uint32_t rom_size;
    uint32_t load_size;
} roms[] = {
    { devicetree_resource, sizeof(devicetree_resource), 0},
    { unicam_resource, sizeof(unicam_resource), 0 },
    { brcm_sdhc_device, sizeof(brcm_sdhc_device), 0 },
    { brcm_emmc_device, sizeof(brcm_emmc_device), 0 },
    { __68040_library, sizeof(__68040_library), 0 },
    { gic_resource, sizeof(gic_resource), 0 },
    { NULL, 0, 0 }
};

/*
    This is a Z3 ROM board with device tree resource. It provides userspace to read the keys and properties from
    the tree in order to e.g. find the available peripherals. The board is provided with its own m68k ROM with the
    resource inside. No ARM-side code is used in this board.
*/

static struct fdt_header *fdt_base;
static char *strings;
static uint32_t strings_len;
static uint32_t *data;
static uint32_t data_len;
static uint32_t allocated_len;

static void put_word(uint32_t word)
{
    while ((sizeof(uint32_t) * (data_len + 1)) > allocated_len)
    {
        uint32_t *new_data = tlsf_malloc(tlsf, allocated_len + 4096);
        memcpy(new_data, data, allocated_len);
        tlsf_free(tlsf, data);
        data = new_data;
        allocated_len += 4096;
    }

    data[data_len++] = word;
}

static void put_words(uint32_t *words, uint32_t count)
{
    while ((sizeof(uint32_t) * (data_len + count)) > allocated_len)
    {
        uint32_t *new_data = tlsf_malloc(tlsf, allocated_len + 4096);
        memcpy(new_data, data, allocated_len);
        tlsf_free(tlsf, data);
        data = new_data;
        allocated_len += 4096;
    }

    for (unsigned i=0; i < count; i++)
    {
        data[data_len++] = *words++;
    }
}

static int find_string(const char *str)
{
    uint32_t idx = 0;

    if (strings_len == 0)
        return -1;

    while(idx < strings_len)
    {
        if (strcmp(str, &strings[idx]) == 0)
            break;
        
        idx += strlen(&strings[idx]) + 1;
    }

    if (idx >= strings_len)
        return -1;
    else
        return idx;
}

static int add_string(const char *str)
{
    int idx = -1;

    if (str)
    {
        int str_len = strlen(str) + 1;
        strings = tlsf_realloc(tlsf, strings, str_len + strings_len);
        idx = strings_len;
        memcpy(&strings[strings_len], str, str_len);
        strings_len = strings_len + str_len;
    }

    return idx;
}

static void dump_node(of_node_t *node)
{
    of_property_t *prop;
    of_node_t *child;

    put_word(FDT_BEGIN_NODE);
    put_words((uint32_t *)node->on_name, (strlen(node->on_name) + 4) >> 2);

    prop = node->on_properties;
    while(prop)
    {
        int idx = find_string(prop->op_name);
        if (idx == -1)
            idx = add_string(prop->op_name);

        put_word(FDT_PROP);
        put_word(prop->op_length);
        put_word(idx);
        put_words((uint32_t *)prop->op_value, (prop->op_length + 3) >> 2);

        prop = prop->op_next;
    }

    child = node->on_children;
    while(child)
    {
        dump_node(child);
        child = child->on_next;
    }

    put_word(FDT_END_NODE);
}

static void build_fdt()
{
    struct fdt_header *fdt_orig = dt_fdt_base();
    struct fdt_header fdt = *fdt_orig;

    data = tlsf_malloc(tlsf, 262144);
    allocated_len = 262144;
    data_len = 0;
    strings_len = 0;
    strings = NULL;

    dump_node(dt_find_node("/"));
    put_word(FDT_END);

    fdt.totalsize -= fdt.size_dt_strings;
    fdt.totalsize -= fdt.size_dt_struct;  

    fdt.size_dt_strings = strings_len;
    fdt.size_dt_struct = data_len * sizeof(uint32_t);

    fdt.totalsize += strings_len + data_len * sizeof(uint32_t);

    fdt_base = tlsf_malloc_aligned(tlsf, (fdt.totalsize + 4095) & ~4095, 4096);
    *fdt_base = fdt;
    fdt_base->off_mem_rsvmap = sizeof(struct fdt_header);

    uint64_t *rsrvd_src = (uint64_t *)((uintptr_t)fdt_orig + fdt_orig->off_mem_rsvmap);
    uint64_t *rsrvd_dest = (uint64_t *)((uintptr_t)fdt_base + sizeof(struct fdt_header));

    do {
        *rsrvd_dest++ = *rsrvd_src++;
        *rsrvd_dest++ = *rsrvd_src++;
    } while(*(rsrvd_src - 1) != 0);

    fdt_base->off_dt_struct = (uintptr_t)rsrvd_dest - (uintptr_t)fdt_base;
    memcpy((void*)((uintptr_t)fdt_base + fdt_base->off_dt_struct), data, data_len * sizeof(uint32_t));

    fdt_base->off_dt_strings = fdt_base->off_dt_struct + data_len * sizeof(uint32_t);
    memcpy((void*)((uintptr_t)fdt_base + fdt_base->off_dt_strings), strings, strings_len);

    tlsf_free(tlsf, data);
    tlsf_free(tlsf, strings);
}

/*
    This is a Z3 ROM board with SDHC driver. More details can be found in the Emu68-tools repository.
    The board is provided with its own m68k ROM with the driver inside. No ARM-side code is used in this board.
*/

static void map(struct ExpansionBoard *board)
{
    uint32_t total_length = 0;
    void *rom_modules = 0;
    void *virt_base = (void*)((uintptr_t)board->map_base + 0x1000);
    
    build_fdt();

    kprintf("[BOARD] Mapping ZIII Emu68 ROM board at address %08x\n", board->map_base);
    mmu_map(mmu_virt2phys((uintptr_t)board->rom_file), board->map_base, board->rom_size, MMU_ACCESS | MMU_ISHARE | MMU_ALLOW_EL0 | MMU_READ_ONLY | MMU_ATTR_CACHED, 0);

    for (int i=0; roms[i].rom_base; i++)
    {
        roms[i].load_size = GetHunkFileSize(roms[i].rom_base);
        //if (i == 0) {
        //    roms[i].load_size = (roms[i].load_size + 4095) & ~4095;
        //}

        if (roms[i].load_size > 0) total_length += roms[i].load_size;
    }

    total_length += (fdt_base->totalsize + 255) & ~255;

    total_length = (total_length + 4095) & ~4095;

    rom_modules = tlsf_malloc_aligned(tlsf, total_length, 4096);

    kprintf("[BOARD] Emu68 ROM needs %ld extra bytes, \n", total_length);
    //kprintf("[BOARD]   buffer at %p\n", rom_modules);

    mmu_map(mmu_virt2phys((uintptr_t)rom_modules), board->map_base + 0x1000, total_length, MMU_ACCESS | MMU_ISHARE | MMU_ALLOW_EL0 | MMU_READ_ONLY | MMU_ATTR_CACHED, 0);

    for (int i=0; roms[i].rom_base; i++)
    {
        LoadHunkFile(roms[i].rom_base, rom_modules, virt_base);
        kprintf("[BOARD] Loaded module to %08lx...%08lx\n", virt_base, virt_base + roms[i].load_size - 1);
        
        rom_modules = (void*)((uintptr_t)rom_modules + roms[i].load_size);
        virt_base = (void*)((uintptr_t)virt_base + roms[i].load_size);

        if (i == 0)
        {
            virt_base = (void*)((uintptr_t)virt_base - 0x18);
            rom_modules = (void*)((uintptr_t)rom_modules - 0x18);
            uint64_t size = (fdt_base->totalsize + 31) & ~31;
            kprintf("[BOARD]   Copy FDT to %08lx...%08lx\n", virt_base, virt_base + size - 1);

            DuffCopy(rom_modules, (const uint32_t *)fdt_base, size / 4);
            rom_modules = (void*)((uintptr_t)rom_modules + size);
            virt_base = (void*)((uintptr_t)virt_base + size);
        }
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