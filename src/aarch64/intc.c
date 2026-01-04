/*
    Copyright © 2020 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "support.h"
#include "devicetree.h"

/* GIC-400 (GIC-v2) controller used in Pi4 and CM4 */

uintptr_t gic_base;
uintptr_t gic_base_cpu;
uintptr_t local_intc_base;

/* Distributor registers */

#define GICD_CTLR           0x000
#define GICD_TYPER          0x004
#define GICD_IIDR           0x008
#define GICD_IGROUPR(n)    (0x080 + 4 * (n))
#define GICD_ISENABLER(n)  (0x100 + 4 * (n))
#define GICD_ICENABLER(n)  (0x180 + 4 * (n))
#define GICD_ISPENDR(n)    (0x200 + 4 * (n))
#define GICD_ICPENDR(n)    (0x280 + 4 * (n))
#define GICD_ISACTIVER(n)  (0x300 + 4 * (n))
#define GICD_ICACTIVER(n)  (0x380 + 4 * (n))
#define GICD_IPRIORITYR(n) (0x400 + 4 * (n))
#define GICD_ITARGETSR(n)  (0x800 + 4 * (n))
#define GICD_ICFGR(n)      (0xc00 + 4 * (n))
#define GICD_PPISR          0xd00
#define GICD_SPISR(n)      (0xd00 + 4 * (n))
#define GICD_SGIR           0xf00
#define GICD_CPENDSGIR(n)  (0xf10 + 4 * (n))
#define GICD_SPENDSGIR(n)  (0xf20 + 4 * (n))
#define GICD_PIDR4          0xfd0
#define GICD_PIDR5          0xfd4
#define GICD_PIDR6          0xfd8
#define GICD_PIDR7          0xfdc
#define GICD_PIDR0          0xfe0
#define GICD_PIDR1          0xfe4
#define GICD_PIDR2          0xfe8
#define GICD_PIDR3          0xfec
#define GICD_CIDR0          0xff0
#define GICD_CIDR1          0xff4
#define GICD_CIDR2          0xff8
#define GICD_CIDR3          0xffc

// PPI Id to bit in PPISR.
// SPISR are bits numbered from 0 to 31, so the correct bit and word address are:
// word: GICD_SPISR(n), bit: 1 << (n & 31)
#define GICD_PPISR_ID(n)    (1 << ((n) - 16))

/* CPU registers */

#define GICC_CTLR           0x0000
#define GICC_PMR            0x0004
#define GICC_BPR            0x0008
#define GICC_IAR            0x000c
#define GICC_EOIR           0x0010
#define GICC_RPR            0x0014
#define GICC_HPPIR          0x0018
#define GICC_ABPR           0x001c
#define GICC_AIAR           0x0020
#define GICC_AEOIR          0x0024
#define GICC_AHPPIR         0x0028
#define GICC_APR0           0x00d0
#define GICC_NSAPR0         0x00e0
#define GICC_IIDR           0x00fc
#define GICC_DIR            0x1000

void gic_local_init()
{
    kprintf("[GIC] gic_local_init()\n");

    /* Enable distributor */
    wr32le(gic_base + GICD_CTLR, 1);

    kprintf("[GIC] Distributor enabled, Version %08x\n", rd32le(gic_base + GICD_IIDR));

    /* Set CPU interface - enable and allow all interrupts */
    wr32le(gic_base_cpu + GICC_PMR, 0xff);
    wr32le(gic_base_cpu + GICC_CTLR, 1);

    kprintf("[GIC] CPU local enabled, Version %08x\n", rd32le(gic_base_cpu + GICC_IIDR));
}

void gic_local_disable()
{
    kprintf("[GIC] gic_local_disable()\n");

    /* Disable CPU interface */
    uint32_t reg = rd32le(gic_base_cpu + GICC_CTLR);
    reg &= ~1;
    wr32le(gic_base_cpu + GICC_CTLR, reg);
}

void gic_irq_eanble(unsigned int id)
{
    uint32_t reg = id / 32;
    uint32_t bit = 1u << (id & 31);

    wr32le(gic_base + GICD_ISENABLER(reg), bit);
}

void gic_irq_disable(unsigned int id)
{
    uint32_t reg = id / 32;
    uint32_t bit = 1u << (id & 31);

    wr32le(gic_base + GICD_ICENABLER(reg), bit);
}

void gic_set_priority(unsigned int id, uint8_t prio)
{
    uintptr_t reg = id / 4;
    uint32_t shift = (int)(id % 4) * 8;

    uint32_t cur = rd32le(gic_base + GICD_IPRIORITYR(reg));
    cur = (cur & ~(0xffu << shift)) | ((uint32_t)prio << shift);
    wr32le(gic_base + GICD_IPRIORITYR(reg), cur);
}

uint32_t gic_read_iar()
{
    return rd32le(gic_base_cpu + GICC_IAR);
}

void gic_write_eoir(uint32_t id)
{
    wr32le(gic_base_cpu + GICC_EOIR, id);
}

int gic_available()
{
    return !!(gic_base);
}

/* Legacy interrupt controller, as used in Pi3 */

void legacy_local_init()
{
    kprintf("[BOOT] Setting IRQ routing to core 0\n");
    wr32le(local_intc_base + 0x000c, 0);
    
    kprintf("[BOOT] Enabling PMU and Timer interrupts on core 0\n");
    wr32le(local_intc_base + 0x0010, 1);      // Enable PMU IRQ on core 0
    wr32le(local_intc_base + 0x0014, 0xfe);   // Disable PMU IRQ on all otehr cores

    wr32le(local_intc_base + 0x0040, 0x0f);   // Enable all CNT IRQs on core 0
    wr32le(local_intc_base + 0x0044, 0x00);   // Disable all CNT IRQs on core 1
    wr32le(local_intc_base + 0x0048, 0x00);   // Disable all CNT IRQs on core 2
    
    kprintf("[BOOT] Enabling Timer interrupts on core 3\n");
    wr32le(local_intc_base + 0x004c, 0xf0);   // Enable all CNT IRQs on core 3

    kprintf("[BOOT] Disabling mailbox interrupts\n");
    wr32le(local_intc_base + 0x0050, 0x00);   // Disable Mailbox IRQs on core 0
    wr32le(local_intc_base + 0x0054, 0x00);   // Disable Mailbox IRQs on core 1
    wr32le(local_intc_base + 0x0058, 0x00);   // Disable Mailbox IRQs on core 2
    wr32le(local_intc_base + 0x005c, 0x00);   // Disable Mailbox IRQs on core 3
}

/* Setup */

void intc_global_init()
{
    union ptr {
        uint32_t *  u32;
        uint64_t *  u64;
        void *      v;
        uintptr_t   num;
    };

    struct of_node *symbols = dt_find_node("/__symbols__");
    struct of_property *prop;
    
    if ((prop = dt_find_property(symbols, "gicv2")))
    {
        of_node_t *gic = dt_find_node(prop->op_value);
        uintptr_t vpu_gic_phys[2] = { 0, 0 };
        uintptr_t gic_virt_base[2] = { 0, 0 };
        union ptr gic_reg;
        union ptr soc_ranges;

        gic_reg.v = dt_find_property(gic, "reg")->op_value;

        of_node_t *soc = dt_find_node("/soc");
        int soc_size_cells = dt_get_property_value_u32(soc, "#size-cells", 1, 1);;
        int soc_address_cells = dt_get_property_value_u32(soc, "#address-cells", 1, 1);
        int cpu_address_cells = dt_get_property_value_u32(dt_find_node("/"), "#address-cells", 1, 0);

        /* Repeat two times - GICv2 has four areas but we use only first two: Distributor and CPU */
        for (int i=0; i < 2; i++)
        {
            /* Get base address, vpu phys */
            switch (soc_address_cells) {
                case 1:
                    vpu_gic_phys[i] = *gic_reg.u32;
                    gic_reg.num += 4;
                    break;
                case 2:
                    vpu_gic_phys[i] = *gic_reg.u64;
                    gic_reg.num += 8;
                    break;
            }
            /* Skip size */
            gic_reg.num += 4 * soc_size_cells;

            soc_ranges.v = dt_find_property(soc, "ranges")->op_value;
            union ptr range = soc_ranges;
            uint32_t ranges_length = dt_find_property(soc, "ranges")->op_length;

            while(range.num < soc_ranges.num + ranges_length)
            {
                uintptr_t soc_start = 0;
                uintptr_t soc_end = 0;
                uintptr_t cpu_start = 0;

                switch(soc_address_cells) {
                    case 1:
                        soc_start = *range.u32;
                        range.num += 4;
                        break;
                    case 2:
                        soc_start = *range.u64;
                        range.num += 8;
                        break;
                }

                switch(cpu_address_cells) {
                    case 1:
                        cpu_start = *range.u32;
                        range.num += 4;
                        break;
                    case 2:
                        cpu_start = *range.u64;
                        range.num += 8;
                        break;
                }

                switch(soc_size_cells) {
                    case 1:
                        soc_end = soc_start + *range.u32 - 1;
                        range.num += 4;
                        break;
                    case 2:
                        soc_end = soc_start + *range.u64 - 1;
                        range.num += 8;
                        break;
                }
                
                if (vpu_gic_phys[i] >= soc_start && vpu_gic_phys[i] < soc_end) {
                    gic_virt_base[i] = vpu_gic_phys[i] - soc_start + cpu_start;
                    break;
                }
            }
        }

        gic_base = gic_virt_base[0];
        gic_base_cpu = gic_virt_base[1];

        kprintf("[BOOT] Found GICv2\n[BOOT]   Distributor @ %p\n[BOOT]   CPU area @ %p\n", gic_base, gic_base_cpu);
    }
    if ((prop = dt_find_property(symbols, "local_intc")))
    {
        of_node_t *local_intc = dt_find_node(prop->op_value);
        uintptr_t vpu_addr_phys = 0;
        union ptr intc_reg;
        union ptr soc_ranges;

        intc_reg.v = dt_find_property(local_intc, "reg")->op_value;

        of_node_t *soc = dt_find_node("/soc");
        int soc_size_cells = dt_get_property_value_u32(soc, "#size-cells", 1, 1);;
        int soc_address_cells = dt_get_property_value_u32(soc, "#address-cells", 1, 1);
        int cpu_address_cells = dt_get_property_value_u32(dt_find_node("/"), "#address-cells", 1, 0);

        soc_ranges.v = dt_find_property(soc, "ranges")->op_value;
        union ptr range = soc_ranges;
        uint32_t ranges_length = dt_find_property(soc, "ranges")->op_length;
        
        switch (soc_address_cells) {
            case 1:
                vpu_addr_phys = *intc_reg.u32;
                break;
            case 2:
                vpu_addr_phys = *intc_reg.u64;
                break;
        }

        while(range.num < soc_ranges.num + ranges_length)
        {
            uintptr_t soc_start = 0;
            uintptr_t soc_end = 0;
            uintptr_t cpu_start = 0;

            switch(soc_address_cells) {
                case 1:
                    soc_start = *range.u32;
                    range.num += 4;
                    break;
                case 2:
                    soc_start = *range.u64;
                    range.num += 8;
                    break;
            }

            switch(cpu_address_cells) {
                case 1:
                    cpu_start = *range.u32;
                    range.num += 4;
                    break;
                case 2:
                    cpu_start = *range.u64;
                    range.num += 8;
                    break;
            }

            switch(soc_size_cells) {
                case 1:
                    soc_end = soc_start + *range.u32 - 1;
                    range.num += 4;
                    break;
                case 2:
                    soc_end = soc_start + *range.u64 - 1;
                    range.num += 8;
                    break;
            }
            
            if (vpu_addr_phys >= soc_start && vpu_addr_phys < soc_end) {
                local_intc_base = vpu_addr_phys - soc_start + cpu_start;
                break;
            }
        }

        kprintf("[BOOT] Found Legacy INTC @ %p\n", local_intc_base);
    }

    if (gic_base) {
        gic_local_init();
    } else {
        legacy_local_init();
    }
}
