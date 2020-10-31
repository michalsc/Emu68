/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdarg.h>
#include <stdint.h>
#include "A64.h"
#include "config.h"
#include "support.h"
#include "tlsf.h"
#include "mmu.h"
#include "devicetree.h"
#include "M68k.h"
#include "HunkLoader.h"
#include "DuffCopy.h"
#include "EmuLogo.h"
#include "EmuFeatures.h"
#include "RegisterAllocator.h"

void _start();
void _boot();
void move_kernel(intptr_t from, intptr_t to);
extern uint64_t mmu_user_L1[512];
extern uint64_t mmu_user_L2[4*512];

void M68K_StartEmu(void *addr);

uint16_t *framebuffer;
uint32_t pitch;
uint32_t fb_width;
uint32_t fb_height;

void display_logo()
{
    struct Size sz = get_display_size();
    uint32_t start_x, start_y;
    uint16_t *buff;
    int32_t pix_cnt = (uint32_t)EmuLogo.el_Width * (uint32_t)EmuLogo.el_Height;
    uint8_t *rle = EmuLogo.el_Data;
    int x = 0;
#if 0
    /* In case the screen is too small, attempt to adjust its size */
    if (sz.height < 800 || sz.width < 1280)
    {
        sz.width = 800; sz.height = 500;
    }
#endif
    kprintf("[BOOT] Display size is %dx%d\n", sz.width, sz.height);
    fb_width = sz.width;
    fb_height = sz.height;
    init_display(sz, (void**)&framebuffer, &pitch);
    kprintf("[BOOT] Framebuffer @ %08x\n", framebuffer);

    start_x = (sz.width - EmuLogo.el_Width) / 2;
    start_y = (sz.height - EmuLogo.el_Height) / 2;

    kprintf("[BOOT] Logo start coordinate: %dx%d, size: %dx%d\n", start_x, start_y, EmuLogo.el_Width, EmuLogo.el_Height);

    /* First clear the screen. Use color in top left corner of RLE image for that */
    {
        uint8_t gray = rle[0];
        uint16_t color = (gray >> 3) | ((gray >> 2) << 5) | ((gray >> 3) << 11);

        for (int i=0; i < sz.width * sz.height; i++)
            framebuffer[i] = LE16(color);
    }

    /* Now decode RLE and draw it on the screen */
    buff = (uint16_t *)((uintptr_t)framebuffer + pitch*start_y);
    buff += start_x;

    while(pix_cnt > 0) {
        uint8_t gray = *rle++;
        uint8_t cnt = *rle++;
        uint16_t color = (gray >> 3) | ((gray >> 2) << 5) | ((gray >> 3) << 11);
        pix_cnt -= cnt;
        while(cnt--) {
            buff[x++] = LE16(color);
            /* If new line, advance the buffer by pitch and reset x counter */
            if (x >= EmuLogo.el_Width) {
                buff += pitch / 2;
                x = 0;
            }
        }
    }
}

uintptr_t top_of_ram;

void platform_init()
{
    of_node_t *e = NULL;

    /*
        Prepare mapping for peripherals. Use and update the data from device tree here
        All peripherals are mapped in the lower 4G address space so that they can be
        accessed from m68k.
    */
    e = dt_find_node("/soc");
    if (e)
    {
        of_property_t *p = dt_find_property(e, "ranges");
        uint32_t *ranges = p->op_value;
        int32_t len = p->op_length;
        uint32_t start_map = 0xf20 / 2;

        int addr_cpu_len = dt_get_property_value_u32(e->on_parent, "#address-cells", 1, FALSE);
        int addr_bus_len = dt_get_property_value_u32(e, "#address-cells", 1, TRUE);
        int size_bus_len = dt_get_property_value_u32(e, "#size-cells", 1, TRUE);

        int pos_abus = addr_bus_len - 1;
        int pos_acpu = pos_abus + addr_cpu_len;
        int pos_sbus = pos_acpu + size_bus_len;

        while (len > 0)
        {
            uint32_t addr_bus, addr_cpu;
            uint32_t addr_len;

            addr_bus = BE32(ranges[pos_abus]);
            addr_cpu = BE32(ranges[pos_acpu]);
            addr_len = BE32(ranges[pos_sbus]);

            (void)addr_bus;

            mmu_map(addr_cpu, start_map << 21, addr_len, 
                MMU_ACCESS | MMU_ALLOW_EL0 | MMU_ATTR(1), 0);

            kprintf("bus: %08x, cpu: %08x, len: %08x\n", addr_bus, addr_cpu, addr_len);

            ranges[pos_acpu] = BE32(start_map << 21);

            start_map += addr_len >> 21;

            len -= sizeof(int32_t) * (addr_bus_len + addr_cpu_len + size_bus_len);
            ranges += addr_bus_len + addr_cpu_len + size_bus_len;
        }
    }
}

void platform_post_init()
{
    void *base_vcmem;
    uint32_t size_vcmem;

    kprintf("[BOOT] Platform post init\n");

    if (get_max_clock_rate(3) != get_clock_rate(3)) {
        kprintf("[BOOT] Changing ARM clock from %d MHz to %d MHz\n", get_clock_rate(3)/1000000, get_max_clock_rate(3)/1000000);
        set_clock_rate(3, get_max_clock_rate(3));
    }
    kprintf("[BOOT] ARM Clock at %d MHz\n", get_clock_rate(3) / 1000000);

    get_vc_memory(&base_vcmem, &size_vcmem);
    kprintf("[BOOT] VC4 memory: %p-%p\n", (intptr_t)base_vcmem, (intptr_t)base_vcmem + size_vcmem - 1);

    if (base_vcmem && size_vcmem)
    {
        mmu_map((uintptr_t)base_vcmem, (uintptr_t)base_vcmem, size_vcmem,
                MMU_ACCESS | MMU_OSHARE | MMU_ALLOW_EL0 | MMU_ATTR(2), 0);
    }

    display_logo();

    //*(volatile uint32_t *)0xf3000034 = LE32((7680000) | 0x30000000);
}
