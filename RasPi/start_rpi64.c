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

    /* In case the screen is too small, attempt to adjust its size */
    if (sz.height < 800 || sz.width < 1280)
    {
        sz.width = 800; sz.height = 500;
    }

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

            mmu_map(addr_cpu, start_map << 21, addr_len, 0x475, 0);

            kprintf("bus: %08x, cpu: %08x, len: %08x\n", addr_bus, addr_cpu, addr_len);

            ranges[pos_acpu] = BE32(start_map << 21);

            start_map += addr_len >> 21;

            len -= sizeof(int32_t) * (addr_bus_len + addr_cpu_len + size_bus_len);
            ranges += addr_bus_len + addr_cpu_len + size_bus_len;
        }
    }
}

#if 0

void boot(void *dtree)
{
    uintptr_t kernel_top_virt = ((uintptr_t)boot + (KERNEL_RSRVD_PAGES << 21)) & ~((KERNEL_RSRVD_PAGES << 21)-1);
    uintptr_t pool_size = kernel_top_virt - (uintptr_t)&__bootstrap_end;
    uint64_t tmp;
    void *base_vcmem;
    uint32_t size_vcmem;

    of_node_t *e = NULL;

    /* Enable caches and cache maintenance instructions from EL0 */
    asm volatile("mrs %0, SCTLR_EL1":"=r"(tmp));
    tmp |= (1 << 2) | (1 << 12);    // Enable D and I caches
    tmp |= (1 << 26);               // Enable Cache clear instructions from EL0
    asm volatile("msr SCTLR_EL1, %0"::"r"(tmp));

    /* Initialize tlsf and parse device tree */
    tlsf = tlsf_init();
    tlsf_add_memory(tlsf, &__bootstrap_end, pool_size);
    dt_parse((void*)dtree);

    platform_init();

    arm_flush_cache((intptr_t)mmu_user_L2, sizeof(mmu_user_L2));

    /*
        At this stage the user space memory is not set up yet, but the kernel runs in high address
        space, so it is safe to adjust the MMU tables for lower region. Here, only peripherals are
        mapped, but the rest will come very soon.
    */
    mmu_user_L1[0] = virt2phys((intptr_t)&mmu_user_L2[0 * 512]) | 3;
    mmu_user_L1[1] = virt2phys((intptr_t)&mmu_user_L2[1 * 512]) | 3;
    mmu_user_L1[2] = virt2phys((intptr_t)&mmu_user_L2[2 * 512]) | 3;
    mmu_user_L1[3] = virt2phys((intptr_t)&mmu_user_L2[3 * 512]) | 3;

    arm_flush_cache((intptr_t)mmu_user_L1, sizeof(mmu_user_L1));

    setup_serial();

    kprintf("[BOOT] Booting %s\n", bootstrapName);
    kprintf("[BOOT] Boot address is %p\n", _start);

    print_build_id();

    kprintf("[BOOT] ARM stack top at %p\n", &_boot);
    kprintf("[BOOT] Bootstrap ends at %p\n", &__bootstrap_end);

    kprintf("[BOOT] Kernel args (%p)\n", dtree);
    kprintf("[BOOT] Local memory pool:\n");
    kprintf("[BOOT]    %p - %p (size=%d)\n", &__bootstrap_end, kernel_top_virt - 1, pool_size);

    if (get_max_clock_rate(3) != get_clock_rate(3)) {
        kprintf("[BOOT] Changing ARM clock from %d MHz to %d MHz\n", get_clock_rate(3)/1000000, get_max_clock_rate(3)/1000000);
        set_clock_rate(3, get_max_clock_rate(3));
    } else {
        kprintf("[BOOT] ARM Clock at %d MHz\n", get_clock_rate(3) / 1000000);
    }

    get_vc_memory(&base_vcmem, &size_vcmem);
    kprintf("[BOOT] VC4 memory: %p-%p\n", (intptr_t)base_vcmem, (intptr_t)base_vcmem + size_vcmem - 1);

    if (base_vcmem && size_vcmem)
    {
        for (uint32_t i=(intptr_t)(base_vcmem) >> 21; i < ((intptr_t)base_vcmem + size_vcmem) >> 21; i++)
        {
            /* User/super RW mode, cached */
            mmu_user_L2[i] = (i << 21) | 0x074d;
        }
        arm_flush_cache((intptr_t)mmu_user_L2, sizeof(mmu_user_L2));
    }

    e = dt_find_node("/memory");
    if (e)
    {
        of_property_t *p = dt_find_property(e, "reg");
        uint32_t *range = p->op_value;
        int size_cells = dt_get_property_value_u32(e, "#size-cells", 1, TRUE);
        int address_cells = dt_get_property_value_u32(e, "#address-cells", 1, TRUE);
        int addr_pos = address_cells - 1;
        int size_pos = address_cells + size_cells - 1;

        top_of_ram = BE32(range[addr_pos]) + BE32(range[size_pos]);
        intptr_t kernel_new_loc = top_of_ram - (KERNEL_RSRVD_PAGES << 21);
        intptr_t kernel_old_loc = virt2phys((intptr_t)_boot) & 0xffe00000;
        top_of_ram = kernel_new_loc - 0x1000;

        range[size_pos] = BE32(BE32(range[size_pos])-(KERNEL_RSRVD_PAGES << 21));

        kprintf("[BOOT] System memory: %p-%p\n", BE32(range[addr_pos]), BE32(range[addr_pos]) + BE32(range[size_pos]) - 1);

        for (uint32_t i=BE32(range[addr_pos]) >> 21; i < (BE32(range[addr_pos]) + BE32(range[size_pos])) >> 21; i++)
        {
            /* User/super RW mode, cached */
            mmu_user_L2[i] = (i << 21) | 0x0741;
        }

        kprintf("[BOOT] Moving kernel from %p to %p\n", (void*)kernel_old_loc, (void*)kernel_new_loc);

        /*
            Copy the kernel memory block from origin to new destination, use the top of
            the kernel space which is a 1:1 map of first 4GB region, uncached
        */
        arm_flush_cache((intptr_t)_boot, KERNEL_RSRVD_PAGES << 21);

        /*
            We use routine in assembler here, because we will move both kernel code *and* stack.
            Playing with C code without knowledge what will happen to the stack after move is ready
            can result in funny Heisenbugs...
        */
        move_kernel(kernel_old_loc, kernel_new_loc);

        kprintf("[BOOT] Kernel moved, MMU tables updated\n");
    }

    display_logo();

    e = dt_find_node("/chosen");
    if (e)
    {
        void *image_start, *image_end;
        of_property_t *p = dt_find_property(e, "linux,initrd-start");

        if (p)
        {
            image_start = (void*)(intptr_t)BE32(*(uint32_t*)p->op_value);
            p = dt_find_property(e, "linux,initrd-end");
            image_end = (void*)(intptr_t)BE32(*(uint32_t*)p->op_value);

            kprintf("[BOOT] Loading executable from %p-%p\n", image_start, image_end);
            void *hunks = LoadHunkFile(image_start);
            (void)hunks;
            M68K_StartEmu((void *)((intptr_t)hunks + 4));
        }
        else
        {
            dt_dump_tree();
            kprintf("[BOOT] No executable to run...\n");
        }
    }

    while(1) asm volatile("wfe");
}

void M68K_LoadContext(struct M68KState *ctx)
{
    asm volatile("msr TPIDRRO_EL0, %0\n"::"r"(ctx));

    asm volatile("ldr w%0, %1"::"i"(REG_D0),"m"(ctx->D[0].u32));
    asm volatile("ldr w%0, %1"::"i"(REG_D1),"m"(ctx->D[1].u32));
    asm volatile("ldr w%0, %1"::"i"(REG_D2),"m"(ctx->D[2].u32));
    asm volatile("ldr w%0, %1"::"i"(REG_D3),"m"(ctx->D[3].u32));
    asm volatile("ldr w%0, %1"::"i"(REG_D4),"m"(ctx->D[4].u32));
    asm volatile("ldr w%0, %1"::"i"(REG_D5),"m"(ctx->D[5].u32));
    asm volatile("ldr w%0, %1"::"i"(REG_D6),"m"(ctx->D[6].u32));
    asm volatile("ldr w%0, %1"::"i"(REG_D7),"m"(ctx->D[7].u32));

    asm volatile("ldr w%0, %1"::"i"(REG_A0),"m"(ctx->A[0].u32));
    asm volatile("ldr w%0, %1"::"i"(REG_A1),"m"(ctx->A[1].u32));
    asm volatile("ldr w%0, %1"::"i"(REG_A2),"m"(ctx->A[2].u32));
    asm volatile("ldr w%0, %1"::"i"(REG_A3),"m"(ctx->A[3].u32));
    asm volatile("ldr w%0, %1"::"i"(REG_A4),"m"(ctx->A[4].u32));
    asm volatile("ldr w%0, %1"::"i"(REG_A5),"m"(ctx->A[5].u32));
    asm volatile("ldr w%0, %1"::"i"(REG_A6),"m"(ctx->A[6].u32));
    asm volatile("ldr w%0, %1"::"i"(REG_A7),"m"(ctx->A[7].u32));

    asm volatile("ldr w%0, %1"::"i"(REG_PC),"m"(ctx->PC));

    asm volatile("ldr d%0, %1"::"i"(REG_FP0),"m"(ctx->FP[0]));
    asm volatile("ldr d%0, %1"::"i"(REG_FP1),"m"(ctx->FP[1]));
    asm volatile("ldr d%0, %1"::"i"(REG_FP2),"m"(ctx->FP[2]));
    asm volatile("ldr d%0, %1"::"i"(REG_FP3),"m"(ctx->FP[3]));
    asm volatile("ldr d%0, %1"::"i"(REG_FP4),"m"(ctx->FP[4]));
    asm volatile("ldr d%0, %1"::"i"(REG_FP5),"m"(ctx->FP[5]));
    asm volatile("ldr d%0, %1"::"i"(REG_FP6),"m"(ctx->FP[6]));
    asm volatile("ldr d%0, %1"::"i"(REG_FP7),"m"(ctx->FP[7]));
}

void M68K_SaveContext(struct M68KState *ctx)
{
    asm volatile("str w%0, %1"::"i"(REG_D0),"m"(ctx->D[0].u32));
    asm volatile("str w%0, %1"::"i"(REG_D1),"m"(ctx->D[1].u32));
    asm volatile("str w%0, %1"::"i"(REG_D2),"m"(ctx->D[2].u32));
    asm volatile("str w%0, %1"::"i"(REG_D3),"m"(ctx->D[3].u32));
    asm volatile("str w%0, %1"::"i"(REG_D4),"m"(ctx->D[4].u32));
    asm volatile("str w%0, %1"::"i"(REG_D5),"m"(ctx->D[5].u32));
    asm volatile("str w%0, %1"::"i"(REG_D6),"m"(ctx->D[6].u32));
    asm volatile("str w%0, %1"::"i"(REG_D7),"m"(ctx->D[7].u32));

    asm volatile("str w%0, %1"::"i"(REG_A0),"m"(ctx->A[0].u32));
    asm volatile("str w%0, %1"::"i"(REG_A1),"m"(ctx->A[1].u32));
    asm volatile("str w%0, %1"::"i"(REG_A2),"m"(ctx->A[2].u32));
    asm volatile("str w%0, %1"::"i"(REG_A3),"m"(ctx->A[3].u32));
    asm volatile("str w%0, %1"::"i"(REG_A4),"m"(ctx->A[4].u32));
    asm volatile("str w%0, %1"::"i"(REG_A5),"m"(ctx->A[5].u32));
    asm volatile("str w%0, %1"::"i"(REG_A6),"m"(ctx->A[6].u32));
    asm volatile("str w%0, %1"::"i"(REG_A7),"m"(ctx->A[7].u32));

    asm volatile("str w%0, %1"::"i"(REG_PC),"m"(ctx->PC));

    asm volatile("str d%0, %1"::"i"(REG_FP0),"m"(ctx->FP[0]));
    asm volatile("str d%0, %1"::"i"(REG_FP1),"m"(ctx->FP[1]));
    asm volatile("str d%0, %1"::"i"(REG_FP2),"m"(ctx->FP[2]));
    asm volatile("str d%0, %1"::"i"(REG_FP3),"m"(ctx->FP[3]));
    asm volatile("str d%0, %1"::"i"(REG_FP4),"m"(ctx->FP[4]));
    asm volatile("str d%0, %1"::"i"(REG_FP5),"m"(ctx->FP[5]));
    asm volatile("str d%0, %1"::"i"(REG_FP6),"m"(ctx->FP[6]));
    asm volatile("str d%0, %1"::"i"(REG_FP7),"m"(ctx->FP[7]));
}

void M68K_PrintContext(struct M68KState *m68k)
{
    M68K_SaveContext(m68k);

    kprintf("[JIT]\n[JIT] M68K Context:\n[JIT] ");

    for (int i=0; i < 8; i++) {
        if (i==4)
            kprintf("\n[JIT] ");
        kprintf("    D%d = 0x%08x", i, BE32(m68k->D[i].u32));
    }
    kprintf("\n[JIT] ");

    for (int i=0; i < 8; i++) {
        if (i==4)
            kprintf("\n[JIT] ");
        kprintf("    A%d = 0x%08x", i, BE32(m68k->A[i].u32));
    }
    kprintf("\n[JIT] ");

    kprintf("    PC = 0x%08x    SR = ", BE32((int)m68k->PC));
    uint16_t sr = BE16(m68k->SR);
    if (sr & SR_X)
        kprintf("X");
    else
        kprintf(".");

    if (sr & SR_N)
        kprintf("N");
    else
        kprintf(".");

    if (sr & SR_Z)
        kprintf("Z");
    else
        kprintf(".");

    if (sr & SR_V)
        kprintf("V");
    else
        kprintf(".");

    if (sr & SR_C)
        kprintf("C");
    else
        kprintf(".");

    kprintf("\n[JIT]     USP= 0x%08x    MSP= 0x%08x    ISP= 0x%08x\n[JIT] ", BE32(m68k->USP.u32), BE32(m68k->MSP.u32), BE32(m68k->ISP.u32));

    for (int i=0; i < 8; i++) {
        union {
            double d;
            uint32_t u[2];
        } u;
        if (i==4)
            kprintf("\n[JIT] ");
        u.d = m68k->FP[i];
        kprintf("    FP%d = %08x%08x", i, u.u[0], u.u[1]);
    }
    kprintf("\n[JIT] ");

    kprintf("    FPSR=0x%08x    FPIAR=0x%08x   FPCR=0x%04x\n", BE32(m68k->FPSR), BE32(m68k->FPIAR), BE32(m68k->FPCR));
}

void M68K_StartEmu(void *addr)
{
//    void (*arm_code)(); //(struct M68KState *ctx);
//    struct M68KTranslationUnit * unit = (void*)0;
    struct M68KState __m68k;
    uint64_t t1=0, t2=0;

    uint32_t stream[512];
    uint32_t *ptr = stream;

    bzero(&__m68k, sizeof(__m68k));

    __m68k.A[7].u32 = BE32((uint32_t)top_of_ram);
    __m68k.PC = BE32((intptr_t)addr);
    __m68k.A[7].u32 = BE32(BE32(__m68k.A[7].u32) - 4);
    *(uint32_t*)(intptr_t)(BE32(__m68k.A[7].u32)) = 0;

    M68K_LoadContext(&__m68k);
    M68K_PrintContext(&__m68k);

    kprintf("[JIT] Let it go...\n");

    t1 = LE32(*(volatile uint32_t*)0xf2003004) | (uint64_t)LE32(*(volatile uint32_t *)0xf2003008) << 32;

    RA_ModifyFPCR(&ptr);
    RA_FlushFPCR(&ptr);

    for (uint32_t *p = stream; p != ptr; p++)
    {
        kprintf("%08x ", *p);
    }
    kprintf("\n");

    t2 = LE32(*(volatile uint32_t*)0xf2003004) | (uint64_t)LE32(*(volatile uint32_t *)0xf2003008) << 32;

    kprintf("[JIT] Time spent in m68k mode: %lld us\n", t2-t1);

    kprintf("[JIT] Back from translated code\n");

    M68K_PrintContext(&__m68k);
}
#endif
