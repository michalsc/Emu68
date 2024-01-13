#include <boards.h>
#include <mmu.h>
#include <A64.h>
#include <devicetree.h>
#include <support.h>
#include <tlsf.h>

__attribute__((aligned(4096)))
#include "./devicetree.h"

/*
    This is a Z3 ROM board with device tree resource. It provides userspace to read the keys and properties from
    the tree in order to e.g. find the available peripherals. The board is provided with its own m68k ROM with the
    resource inside. No ARM-side code is used in this board.
*/

struct fdt_header *fdt_base;
static char *strings;
static uint32_t strings_len;
static uint32_t *data;
static uint32_t data_len;
static uint32_t allocated_len;

void put_word(uint32_t word)
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

void put_words(uint32_t *words, uint32_t count)
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

int find_string(const char *str)
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

int add_string(const char *str)
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

void dump_node(of_node_t *node)
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

void build_fdt()
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

static void map(struct ExpansionBoard *board)
{
    build_fdt();

    kprintf("[BOARD] Mapping ZIII devicetree board at address %08x\n", board->map_base);
    mmu_map(mmu_virt2phys((uintptr_t)board->rom_file), board->map_base, board->rom_size, MMU_ACCESS | MMU_ISHARE | MMU_ALLOW_EL0 | MMU_READ_ONLY | MMU_ATTR_CACHED, 0);
    mmu_map(mmu_virt2phys((uintptr_t)fdt_base), board->map_base + board->rom_size, (fdt_base->totalsize + 4095) & ~4095, MMU_ACCESS | MMU_ISHARE | MMU_ALLOW_EL0 | MMU_READ_ONLY | MMU_ATTR_CACHED, 0);
}

static struct ExpansionBoard board = {
    devicetree_bin,
    4096,
    0,
    1,
    1,
    map
};

static void * __attribute__((used, section(".boards.z3"))) _board = &board;
