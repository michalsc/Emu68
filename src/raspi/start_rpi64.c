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
#include "version.h"

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

extern const uint32_t topaz8_charloc[];
extern const uint8_t topaz8_chardata[];

uint32_t text_x = 0;
uint32_t text_y = 0;
const int modulo = 192;
int purple = 0;
int black = 0;

void put_char(uint8_t c)
{
    if (framebuffer && pitch)
    {
        uint16_t *pos_in_image = (uint16_t*)((uintptr_t)framebuffer + (text_y * 16 + 5)* pitch);
        pos_in_image += 4 + text_x * 8;

        if (c == 10) {
            text_x = 0;
            text_y++;
        }
        else if (c >= 32) {
            uint32_t loc = (topaz8_charloc[c - 32] >> 16) >> 3;
            const uint8_t *data = &topaz8_chardata[loc];

            for (int y = 0; y < 16; y++) {
                const uint8_t byte = *data;

                for (int x=0; x < 8; x++) {
                    if (byte & (0x80 >> x)) {
                        if (purple) {
                            pos_in_image[x] = LE16(0xed51);
                        }
                        else if (black) {
                            pos_in_image[x] = LE16(0x630c);
                        }
                        else {
                            pos_in_image[x] = 0;
                        }
                    }
                }

                if (y & 1)
                    data += modulo;
                pos_in_image += pitch / 2;
            }
            text_x++;
        }
    }
}

static void __putc(void *data, char c)
{
    (void)data;
    put_char(c);
}

void display_logo()
{
    struct Size sz = get_display_size();
    uint32_t start_x, start_y;
    uint16_t *buff;
    int32_t pix_cnt = (uint32_t)EmuLogo.el_Width * (uint32_t)EmuLogo.el_Height;
    uint8_t *rle = EmuLogo.el_Data;
    int x = 0;
    of_node_t *e = NULL;

    e = dt_find_node("/chosen");
    if (e)
    {
        of_property_t * prop = dt_find_property(e, "bootargs");
        if (prop)
        {
            const char *tok;
            if ((tok = find_token(prop->op_value, "logo=")))
            {
                tok += 5;

                if (strncmp(tok, "purple", 6) == 0)
                    purple = 1;
                else if (strncmp(tok, "black", 5) == 0)
                    black = 1;
            }
        }
    }

    kprintf("[BOOT] Display size is %dx%d\n", sz.width, sz.height);
    fb_width = sz.width;
    fb_height = sz.height;
    init_display(sz, (void**)&framebuffer, &pitch);
    kprintf("[BOOT] Framebuffer @ %08x\n", framebuffer);

    start_x = (sz.width - EmuLogo.el_Width) / 2;
    start_y = (sz.height - EmuLogo.el_Height) / 2;

    kprintf("[BOOT] Logo start coordinate: %dx%d, size: %dx%d\n", start_x, start_y, EmuLogo.el_Width, EmuLogo.el_Height);

    /* Calculate text coordinate for version string */
#if 0
    text_x = start_x / 8;
    text_y = (start_y + EmuLogo.el_Height + 15) / 16;
#else
    text_y = (fb_height - 16 - 5) / 16;
    text_x = (fb_width - strlen(&VERSION_STRING[6]) * 8 - 1) / 8;
#endif

    /* First clear the screen. Use color in top left corner of RLE image for that */
    {
        uint8_t gray = rle[0];
        uint16_t color;

        if (purple)
        {
            gray = 240 - gray;
            int r=-330,g=-343,b=-91;
            r += (gray * 848) >> 8;
            g += (gray * 768) >> 8;
            b += (gray * 341) >> 8;

            if (r < 0) r = 0;
            if (g < 0) g = 0;
            if (b < 0) b = 0;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            color = (b >> 3) | ((g >> 2) << 5) | ((r >> 3) << 11);
        }
        else if (black)
        {
            gray = 0;
            color = (gray >> 3) | ((gray >> 2) << 5) | ((gray >> 3) << 11);
        }
        else
        {
            color = (gray >> 3) | ((gray >> 2) << 5) | ((gray >> 3) << 11);
        }

        for (int i=0; i < sz.width * sz.height; i++)
            framebuffer[i] = LE16(color);
    }

    /* Now decode RLE and draw it on the screen */
    buff = (uint16_t *)((uintptr_t)framebuffer + pitch*start_y);
    buff += start_x;

    while(pix_cnt > 0) {
        uint8_t gray = *rle++;
        uint8_t cnt = *rle++;
        uint16_t color;

        if (purple)
        {
            gray = 240 - gray;
            int r=-330,g=-343,b=-91;
            r += (gray * 848) >> 8;
            g += (gray * 768) >> 8;
            b += (gray * 341) >> 8;

            if (r < 0) r = 0;
            if (g < 0) g = 0;
            if (b < 0) b = 0;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            color = (b >> 3) | ((g >> 2) << 5) | ((r >> 3) << 11);
        }
        else if (black)
        {
            int g = ((120 - (int)gray) * 5) / 4;
            if (g < 0)
                g = 0;
            if (g > 255)
                g = 255;

            color = (g >> 3) | ((g >> 2) << 5) | ((g >> 3) << 11);
        }
        else
        {
            color = (gray >> 3) | ((gray >> 2) << 5) | ((gray >> 3) << 11);
        }

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

    /* Print EMu68 version number and git sha. */
    kprintf_pc(__putc, NULL, &VERSION_STRING[6]);

    /* Reset test coordinates for further text printing (e.g. buptest) */
    text_x = 0;
    text_y = 0;
}

uintptr_t top_of_ram;

#ifdef PISTORM
#include "ps_protocol.h"

extern int block_c0;
#endif

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
                MMU_ACCESS | MMU_ALLOW_EL0 | MMU_ATTR_DEVICE, 0);

            kprintf("bus: %08x, cpu: %08x, len: %08x\n", addr_bus, addr_cpu, addr_len);

            ranges[pos_acpu] = BE32(start_map << 21);

            start_map += addr_len >> 21;

            len -= sizeof(int32_t) * (addr_bus_len + addr_cpu_len + size_bus_len);
            ranges += addr_bus_len + addr_cpu_len + size_bus_len;
        }
    }
#ifdef PISTORM
    ps_setup_protocol();
    ps_reset_state_machine();
    ps_pulse_reset();
#endif
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
    
    if (get_max_clock_rate(4) != get_clock_rate(4)) {
        kprintf("[BOOT] Changing CORE clock from %d MHz to %d MHz\n", get_clock_rate(4)/1000000, get_max_clock_rate(4)/1000000);
        set_clock_rate(4, get_max_clock_rate(4));
    }
    kprintf("[BOOT] CORE Clock at %d MHz\n", get_clock_rate(4) / 1000000);

    get_vc_memory(&base_vcmem, &size_vcmem);
    kprintf("[BOOT] VC4 memory: %p-%p\n", (intptr_t)base_vcmem, (intptr_t)base_vcmem + size_vcmem - 1);

    if (base_vcmem && size_vcmem)
    {
        mmu_map((uintptr_t)base_vcmem, (uintptr_t)base_vcmem, size_vcmem,
                MMU_ACCESS | MMU_OSHARE | MMU_ALLOW_EL0 | MMU_ATTR_WRITETHROUGH, 0);
    }

    display_logo();

#ifdef PISTORM
    kprintf("[BOOT] sending RESET signal to Amiga\n");
    ps_pulse_reset();

    block_c0 = 0;

    ps_write_8(0xde1000, 0);
    if (ps_read_8(0xde1000) & 0x80)
    {
        if (ps_read_8(0xde1000) & 0x80)
        {
            if (!(ps_read_8(0xde1000) & 0x80))
            {
                if (ps_read_8(0xde1000) & 0x80)
                {
                    kprintf("[BOOT] Gayle appears to be present\n");
                    block_c0 = 1;
                }
            }
        }
    }
#endif

    //*(volatile uint32_t *)0xf3000034 = LE32((7680000) | 0x30000000);
}
