#include <boards.h>
#include <mmu.h>
#include <A64.h>
#include <devicetree.h>
#include <support.h>

/*
    This is a Z2 RAM expansion installed in the 0x200000 ... 0x9fffff space. No ROM required, the board just maps
    physical RAM in that region and thus removes the need for page fault handler. This FAST RAM is accessible to 
    the cpu only, at full speed of physically instaled memory.
*/

static void map(struct ExpansionBoard *board)
{
    kprintf("[BOARD] Mapping ZII RAM board at address %08x\n", board->map_base);
    mmu_map(board->map_base, board->map_base, board->rom_size, MMU_ACCESS | MMU_ISHARE | MMU_ALLOW_EL0 | MMU_ATTR(0), 0);
    vmm_map(board->map_base, board->map_base, board->rom_size, 0x7fc, 0);
}

#define PRODUCT_ID      0x10
#define MANUFACTURER_ID 0x6d73
#define RAM_SERIAL      0x1e0aeb68

/* No real ROM is used, so synthesize your own here */


static uint16_t z2_ram[32] = {
    0xe000, 0x0000,                             // Z2 board, link it to memory list, // Size: 8MB
    (uint16_t)~(PRODUCT_ID << 8) & 0xf000, (uint16_t)~(PRODUCT_ID << 12) & 0xf000,
    (uint16_t)~0x8fff, (uint16_t)~0x0fff,       // ERT_MEMSPACE in Z2 region
    (uint16_t)~0x0fff, (uint16_t)~0x0fff,       // Reserved - must be 0

    (uint16_t)~(MANUFACTURER_ID) & 0xf000, (uint16_t)~(MANUFACTURER_ID << 4) & 0xf000,
    (uint16_t)~(MANUFACTURER_ID << 8) & 0xf000, (uint16_t)~(MANUFACTURER_ID << 12) & 0xf000,

    (uint16_t)~(RAM_SERIAL >> 16) & 0xf000, (uint16_t)~(RAM_SERIAL >> 12) & 0xf000,
    (uint16_t)~(RAM_SERIAL >> 8) & 0xf000, (uint16_t)~(RAM_SERIAL >> 4) & 0xf000,
    (uint16_t)~(RAM_SERIAL) & 0xf000, ~(uint16_t)((uint16_t)RAM_SERIAL << 4) & 0xf000,
    (uint16_t)~((uint16_t)RAM_SERIAL << 8) & 0xf000, ~(uint16_t)((uint16_t)RAM_SERIAL << 12) & 0xf000,

    (uint16_t)~0x0fff, (uint16_t)~0x0fff, (uint16_t)~0x0fff, (uint16_t)~0x0fff,

    (uint16_t)~0x0fff, (uint16_t)~0x0fff, (uint16_t)~0x0fff, (uint16_t)~0x0fff,
    (uint16_t)~0x0fff, (uint16_t)~0x0fff, (uint16_t)~0x0fff, (uint16_t)~0x0fff,
};

static struct ExpansionBoard board = {
    z2_ram,
    8*1024*1024,
    0,
    0,
    0,
    map
};

static void init()
{
    of_node_t *e = NULL;

    kprintf("[BOOT] Initlializing Z2 RAM expansion\n");

    e = dt_find_node("/chosen");
    if (e)
    {
        of_property_t * prop = dt_find_property(e, "bootargs");
        if (prop)
        {
            if (strstr(prop->op_value, "z2_ram_size=8")) {
                board.rom_size = 8*1024*1024;
                board.enabled = 1;
                kprintf("[BOOT]   use 8MB expansion RAM\n");
            }
            if (strstr(prop->op_value, "z2_ram_size=4")) {
                board.rom_size = 4*1024*1024;
                z2_ram[1] = 0x7000;
                board.enabled = 1;
                kprintf("[BOOT]   use 4MB expansion RAM\n");
            }
            else if (strstr(prop->op_value, "z2_ram_size=2")) {
                board.rom_size = 2*1024*1024;
                z2_ram[1] = 0x6000;
                board.enabled = 1;
                kprintf("[BOOT]   use 2MB expansion RAM\n");
            }
            else if (strstr(prop->op_value, "z2_ram_size=1")) {
                board.rom_size = 1*1024*1024;
                z2_ram[1] = 0x5000;
                board.enabled = 1;
                kprintf("[BOOT]   use 1MB expansion RAM\n");
            }
            else if (strstr(prop->op_value, "z2_ram_size=0")) {
                board.enabled = 0;
                kprintf("[BOOT]   disable ZorroII expansion RAM\n");
            }
            else {
                kprintf("[BOOT]   Z2 expansion disabled\n");
            }
        }
    }
}

static void * __attribute__((used, section(".init"))) _init = &init;

static void * __attribute__((used, section(".boards.z2"))) _board = &board;
