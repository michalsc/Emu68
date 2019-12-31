/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdarg.h>
#include <stdint.h>
#include "config.h"
#include "support_rpi.h"
#include "tlsf.h"
#include "devicetree.h"
#include "M68k.h"
#include "HunkLoader.h"
#include "DuffCopy.h"
#include "EmuLogo.h"
#include "Features.h"

#define DV2P(x) /* x */

void _start();

asm("   .section .startup           \n"
"       .globl _start               \n"
"       .type _start,%function      \n"
"_start:                            \n"
"       mrs     x9, CurrentEL       \n" /* Since we do not use EL2 mode yet, we fall back to EL1 immediately */
"       and     x9, x9, #0xc        \n"
"       cmp     x9, #8              \n"
"       b.eq    leave_EL2           \n" /* In case of EL2 or EL3 switch back to EL1 */
"       b.gt    leave_EL3           \n"
"continue_boot:                     \n"
#if EMU68_HOST_BIG_ENDIAN
"       mrs     x10, SCTLR_EL1      \n" /* If necessary, set endianess of EL1 and EL0 before fetching any data */
"       orr     x10, x10, #(1 << 25) | (1 << 24)\n"
"       msr     SCTLR_EL1, x10      \n"
#endif
"       ldr     x9, =_start         \n"
"       mov     sp, x9              \n"
"       mov     x10, #0x00300000    \n" /* Enable signle and double VFP coprocessors in EL1 and EL0 */
"       msr     CPACR_EL1, x10      \n"
"       isb     sy                  \n"
"       isb     sy                  \n" /* Drain the insn queue */
"       ic      IALLU               \n" /* Invalidate entire instruction cache */
"       isb     sy                  \n"

"       ldr     x10, =0x4404ff      \n" /* Attr0 - write-back cacheable RAM, Attr1 - device, Attr2 - non-cacheable */
"       msr     MAIR_EL1, x10       \n" /* Set memory attributes */

"       ldr     x10, =0xb5193519    \n" /* Upper and lower enabled, both 39bit in size */
"       msr     TCR_EL1, x10        \n"

"       adrp    x10, mmu_user_L1    \n" /* Load table pointers for low and high memory regions */
"       msr     TTBR0_EL1, x10      \n" /* Initially only 4GB in each region is mapped, the rest comes later */
"       adrp    x10, mmu_kernel_L1  \n"
"       msr     TTBR1_EL1, x10      \n"

"       isb     sy                  \n"
"       mrs     x10, SCTLR_EL1      \n"
"       orr     x10, x10, #1        \n"
"       msr     SCTLR_EL1, x10      \n"
"       isb     sy                  \n"

"       ldr     x9, =__bss_start    \n"
"       ldr     w10, =__bss_size    \n"
"1:     cbz     w10, 2f             \n"
"       str     xzr, [x9], #8       \n"
"       sub     w10, w10, 1         \n"
"       cbnz    w10, 1b             \n"
"2:     ldr     x30, =boot          \n"
"       br      x30                 \n"

"leave_EL3:                         \n"
#if EMU68_HOST_BIG_ENDIAN
"       mrs     x10, SCTLR_EL3      \n" /* If necessary, set endianess of EL3 before fetching any data */
"       orr     x10, x10, #(1 << 25)\n"
"       msr     SCTLR_EL3, x10      \n"
#endif
"       adr     x10, leave_EL2      \n" /* Fallback to continue_boot in EL2 here below */
"       msr     ELR_EL3, x10        \n"
"       ldr     w10, =0x000003c5    \n"
"       msr     SPSR_EL3, x10       \n"
"       eret                        \n"

"leave_EL2:                         \n"
#if EMU68_HOST_BIG_ENDIAN
"       mrs     x10, SCTLR_EL2      \n" /* If necessary, set endianess of EL2 before fetching any data */
"       orr     x10, x10, #(1 << 25)\n"
"       msr     SCTLR_EL2, x10      \n"
#endif
"       mov     x10, #3             \n" /* Enable CNTL access from EL1 and EL0 */
"       msr     CNTHCTL_EL2, x10    \n"
"       mov     x10, #0x80000000    \n" /* EL1 is AArch64 */
"       msr     HCR_EL2, x10        \n"
"       adr     x10, continue_boot  \n" /* Fallback to continue_boot in EL1 */
"       msr     ELR_EL2, x10        \n"
"       ldr     w10, =0x000003c5    \n"
"       msr     SPSR_EL2, x10       \n"
"       eret                        \n"

"       .section .text              \n"
".byte 0                            \n"
".string \"$VER: Emu68.img " VERSION_STRING_DATE "\"\n"
".byte 0                            \n"
"\n\t\n\t"
);

extern int __bootstrap_end;
extern const struct BuildID g_note_build_id;

#if EMU68_HOST_BIG_ENDIAN
static __attribute__((used)) const char bootstrapName[] = "Emu68 runtime/AArch64 BigEndian";
#else
static __attribute__((used)) const char bootstrapName[] = "Emu68 runtime/AArch64 LittleEndian";
#endif


/* Initial MMU maps are pretty simple - four 1GB blocks necessary to boot the C code */
static __attribute__((used, section(".mmu"))) uint64_t mmu_user_L1[512] = 
{
    [0x000] = 0x0000000000000701,
    [0x001] = 0x0000000040000701,
    [0x002] = 0x0000000080000701,
    [0x003] = 0x00000000c0000701,
};

/* Four additional directories to map the 4GB address space in 2MB pages here */
static __attribute__((used, section(".mmu"))) uint64_t mmu_user_L2[4*512];

static __attribute__((used, section(".mmu"))) uint64_t mmu_kernel_L1[512] = 
{
    [0x000] = 0x0000000000000701,
    [0x001] = 0x0000000040000701,
    [0x002] = 0x0000000080000701,
    [0x003] = 0x00000000c0000701,

    /* Top of RAM - 1:1 map of 32bit address space, uncached */
    [0x1fc] = 0x000000000000070d,
    [0x1fd] = 0x000000004000070d,
    [0x1fe] = 0x000000008000070d,
    [0x1ff] = 0x00000000c000070d,
};

/* Four additional directories to map the 4GB address space in 2MB pages here */
static __attribute__((used, section(".mmu"))) uint64_t mmu_kernel_L2[512];

intptr_t virt2phys(intptr_t addr)
{
    intptr_t phys = 0;
    uint64_t *tbl = NULL;
    int idx_l1, idx_l2, idx_l3;
    uint64_t tmp;

    DV2P(kprintf("virt2phys(%p)\n", addr));

    if (addr & 0xffff000000000000) {
        DV2P(kprintf("selecting kernel tables\n"));
        asm volatile("mrs %0, TTBR1_EL1":"=r"(tbl));
        tbl = (uint64_t *)((intptr_t)tbl | 0xffffffff00000000);
    } else {
        DV2P(kprintf("selecting user tables\n"));
        asm volatile("mrs %0, TTBR0_EL1":"=r"(tbl));
        tbl = (uint64_t *)((intptr_t)tbl | 0xffffffff00000000);
    }

    DV2P(kprintf("L1 table: %p\n", tbl));

    idx_l1 = (addr >> 30) & 0x1ff;
    idx_l2 = (addr >> 21) & 0x1ff;
    idx_l3 = (addr >> 12) & 0x1ff;

    DV2P(kprintf("idx_l1 = %d, idx_l2 = %d, idx_l3 = %d\n", idx_l1, idx_l2, idx_l3));

    tmp = tbl[idx_l1];
    DV2P(kprintf("item in L1 table: %016x\n", tmp));
    if (tmp & 1)
    {
        DV2P(kprintf("is valid\n"));
        if (tmp & 2) {
            tbl = (uint64_t *)((tmp & 0x0000fffffffff000) | 0xffffffff00000000);
            DV2P(kprintf("L2 table at %p\n", tbl));

            tmp = tbl[idx_l2];
            DV2P(kprintf("item in L2 table: %016x\n", tmp));
            if (tmp & 1)
            {
                DV2P(kprintf("is valid\n"));
                if (tmp & 2)
                {
                    tbl = (uint64_t *)((tmp & 0x0000fffffffff000) | 0xffffffff00000000);
                    DV2P(kprintf("L3 table at %p\n", tbl));

                    tmp = tbl[idx_l3];
                    DV2P(kprintf("item in L3 table: %016x\n", tmp));
                    if ((tmp & 3) == 3)
                    {
                        DV2P(kprintf("is valid 4K page\n"));
                        phys = (tmp & 0xfffffffff000) + (addr & 0xfff);
                    }
                    else {
                        DV2P(kprintf("invalid!\n"));
                        return -1;
                    }
                }
                else {
                    DV2P(kprintf("2MB page!\n"));
                    phys = (tmp & 0xffffffe00000) + (addr & 0x1fffff);
                }
            }
            else
            {
                DV2P(kprintf("invalid!\n"));
                return -1;
            }
        } else {
            DV2P(kprintf("1GB page!\n"));
            phys = (tmp & 0xffffc0000000) + (addr & 0x3fffffff);
        }
    }
    else {
        DV2P(kprintf("invalid!\n"));
        return -1;
    }

    DV2P(kprintf("returning %p\n", phys));

    return phys;
}


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
        sz.width = 1280; sz.height = 800;
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

void print_build_id()
{
    const uint8_t *build_id_data = &g_note_build_id.bid_Data[g_note_build_id.bid_NameLen];

    kprintf("[BOOT] Build ID: ");
    for (unsigned i = 0; i < g_note_build_id.bid_DescLen; ++i) {
        kprintf("%02x", build_id_data[i]);
    }
    kprintf("\n");
}

void boot(void *dtree)
{
    uintptr_t kernel_top_virt = ((uintptr_t)boot + 0x1000000) & ~0xffffff;
    uintptr_t pool_size = kernel_top_virt - (uintptr_t)&__bootstrap_end;
    uint64_t tmp;
    uintptr_t top_of_ram;
    void *base_vcmem;
    uint32_t size_vcmem;

    of_node_t *e = NULL;
    char *compatible = NULL;
    int raspi4 = 0;

    (void)raspi4;

    /* Enable caches and cache maintenance instructions from EL0 */
    asm volatile("mrs %0, SCTLR_EL1":"=r"(tmp));
    tmp |= (1 << 2) | (1 << 12);    // Enable D and I caches
    tmp |= (1 << 26);               // Enable Cache clear instructions from EL0
    asm volatile("msr SCTLR_EL1, %0"::"r"(tmp)); 

    /* Initialize tlsf and parse device tree */
    tlsf = tlsf_init();
    tlsf_add_memory(tlsf, &__bootstrap_end, pool_size);
    dt_parse((void*)dtree);

    e = dt_find_node("/");
    if (e)
    {
        of_property_t *p = dt_find_property(e, "compatible");
        if (p)
        {
            compatible = p->op_value;
            if (compatible[12] >= '4')
                raspi4 = 1;
        }
    }

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

        while (len > 0)
        {
            uint32_t addr_bus, addr_cpu;
            uint32_t addr_len;

            addr_bus = BE32(*ranges++);
            /* ignore higher half of addr_cpu here */
            if (raspi4)
                ranges++;
            addr_cpu = BE32(*ranges++);
            addr_len = BE32(*ranges++);

            (void)addr_bus;

            if (addr_len < 0x00200000)
                addr_len = 0x00200000;

            kprintf("bus: %08x, cpu: %08x, len: %08x\n", addr_bus, addr_cpu, addr_len);

            /* Prepare mapping - device type */
            for (unsigned i=0; i < (addr_len >> 21); i++)
            {
                /* Strongly-ordered device, uncached, 16MB region */
                mmu_user_L2[start_map + i] = (i << 21) | addr_cpu | 0x745;
            }

            ranges[-2] = BE32(start_map << 21);

            start_map += addr_len >> 21;

            if (raspi4)
                len -= 16;
            else
                len -= 12;
        }
    }

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

    kprintf("[BOOT] ARM stack top at %p\n", &_start);
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

        if (raspi4)
            range++;

        top_of_ram = BE32(range[0]) + BE32(range[1]);
        intptr_t kernel_new_loc = top_of_ram - 0x01000000;
        top_of_ram = kernel_new_loc - 0x1000;

        range[1] = BE32(BE32(range[1])-0x01000000);

        kprintf("[BOOT] System memory: %p-%p\n", BE32(range[0]), BE32(range[0]) + BE32(range[1]) - 1);

        for (uint32_t i=BE32(range[0]) >> 21; i < (BE32(range[0]) + BE32(range[1])) >> 21; i++)
        {
            /* User/super RW mode, cached */
            mmu_user_L2[i] = (i << 21) | 0x0741;
        }

        kprintf("[BOOT] Moving kernel to %p\n", (void*)kernel_new_loc);

        /* First prepare the correct MMU L2 map for the kernel */
        for (int i=0; i < 8; i++)
        {
            mmu_kernel_L2[i] = ((kernel_new_loc & 0xffe00000) + (i << 21)) | 0x701;
        }
        arm_flush_cache((intptr_t)mmu_kernel_L2, sizeof(mmu_kernel_L2));

        /*
            Next copy the 16MB memory block from origin to new destination, use kernel space
            which is still 1:1 map of first 4GB region
        */
        DuffCopy((void *)(0xffffff8000000000 + kernel_new_loc), (void *)0xffffff8000000000, 0x01000000 / 4);
        arm_flush_cache(0xffffff8000000000 + kernel_new_loc, 0x01000000);

        /*
            At this point the copy of kernel is at the new location, but MMU tables are still in old place.
            First, update the old MMU L1 table to point at the new kernel region, then update the pointers in
            kernel and user MMU maps.
        */
        mmu_kernel_L1[0] = virt2phys((intptr_t)&mmu_kernel_L2[0]) | 3;
        mmu_kernel_L1[1] = 0;
        mmu_kernel_L1[2] = 0;
        mmu_kernel_L1[3] = 0;
        arm_flush_cache((intptr_t)mmu_kernel_L1, sizeof(mmu_kernel_L1));
        asm volatile("tlbi VMALLE1");

        /*
            After last fix the mmu_kernel_L1[0] is pointing to the new value, although the table
            is still in wrong location. Now we can update kernel and user maps, and then, finally,
            uptade both ttbr.
        */
        mmu_kernel_L1[0] = virt2phys((intptr_t)&mmu_kernel_L2[0]) | 3;
        arm_flush_cache((intptr_t)mmu_kernel_L1, sizeof(mmu_kernel_L1));
        asm volatile ("dsb sy; msr TTBR1_EL1, %0; isb"::"r"(virt2phys((intptr_t)mmu_kernel_L1)));

        mmu_user_L1[0] = virt2phys((intptr_t)&mmu_user_L2[0 * 512]) | 3;
        mmu_user_L1[1] = virt2phys((intptr_t)&mmu_user_L2[1 * 512]) | 3;
        mmu_user_L1[2] = virt2phys((intptr_t)&mmu_user_L2[2 * 512]) | 3;
        mmu_user_L1[3] = virt2phys((intptr_t)&mmu_user_L2[3 * 512]) | 3;
        arm_flush_cache((intptr_t)mmu_user_L1, sizeof(mmu_user_L1));
        asm volatile ("dsb sy; msr TTBR0_EL1, %0; isb"::"r"(virt2phys((intptr_t)mmu_user_L1)));

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
//            start_emu((void *)((intptr_t)hunks + 4));
        }
        else
        {
            kprintf("[BOOT] No executable to run...\n");
        }
    }

    while(1) asm volatile("wfe");
}
