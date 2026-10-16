#include "support.h"
#include "devicetree.h"
#include "mmu.h"

void platform_init()
{
    mmu_map(0x00000000, 0xffffffa000000000ULL, 0x40000000, MMU_ACCESS | MMU_ALLOW_EL0 | MMU_ATTR_DEVICE, 0);
}

void setup_serial()
{
    of_property_t *alias = dt_find_property(dt_find_node("/aliases"), "serial0");

    if (alias != NULL) {
        of_node_t *serial = dt_find_node(alias->op_value);
        if (serial != NULL) {
            extern uintptr_t UART_BASE;

            uint32_t* reg = dt_find_property(serial, "reg")->op_value;

            uintptr_t virt = 0xffffffa000000000ULL + reg[0];

            UART_BASE = virt;
        }
    }
}

void platform_post_init()
{
    // Nothing to do here for de25nano
}
