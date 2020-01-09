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
#include "support_rpi.h"
#include "tlsf.h"
#include "devicetree.h"
#include "M68k.h"
#include "HunkLoader.h"
#include "DuffCopy.h"
#include "EmuLogo.h"
#include "Features.h"
#include "RegisterAllocator.h"

#define DV2P(x) /* x */

#define xstr(s) str(s)
#define str(s) #s

#define KERNEL_RSRVD_PAGES  8

#if EMU68_HOST_BIG_ENDIAN
#define L16(x) ((((x) & 0xff00) >> 8) | (((x) & 0x00ff) << 8))
#define L32(x) (((L16(x)) << 16) | L16(((x) >> 16) & 0xffff))
#define L64(x) (((L32(x)) << 32) | L32(((x) >> 32) & 0xffffffff))
#else
#define L16(x) (x)
#define L32(x) (x)
#define L64(x) (x)
#endif

void _start();
void _boot();

asm("   .section .startup           \n"
"       .globl _start               \n"
"       .type _start,%function      \n" /* Our kernel image starts with a standard header */
"_boot:  b       _start              \n" /* code0: branch to the start */
"       .long   0                   \n" /* code1: not used yet */
"       .quad " xstr(L64(0x00080000)) " \n" /* requested Image offset within the 2MB page */
"       .quad " xstr(L64(KERNEL_RSRVD_PAGES << 21)) "\n" /* Total size of kernel */
#if EMU68_HOST_BIG_ENDIAN
"       .quad " xstr(L64(0xb)) "    \n" /* Flags: Endianess, 4K pages, kernel anywhere in RAM */
#else
"       .quad " xstr(L64(0xa)) "    \n" /* Flags: Endianess, 4K pages, kernel anywhere in RAM */
#endif
"       .quad 0                     \n" /* res2 */
"       .quad 0                     \n" /* res3 */
"       .quad 0                     \n" /* res4 */
"       .long " xstr(L32(0x664d5241)) "\n" /* Magic: ARM\x64 */
"       .long 0                     \n" /* res5 */

".byte 0                            \n"
".align 4                           \n"
".string \"$VER: Emu68.img " VERSION_STRING_DATE "\"\n"
".byte 0                            \n"
".align 5                           \n"

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

/*
    At this point we have correct endianess and the code is executing, but we do not really know where
    we are. The necessary step now is to prepare absolutely basic initial memory map and turn on MMU
*/

"       adr     x9, __mmu_start     \n" /* First clear the memory for MMU tables, in case there was a trash */
"       ldr     w10, =__mmu_size    \n"
"1:     str     xzr, [x9], #8       \n"
"       sub     w10, w10, 8         \n"
"       cbnz    w10, 1b             \n"
"2:                                 \n"

"       adrp    x16, mmu_user_L1    \n" /* x16 - address of user's L1 map */
"       mov     x9, 0x701           \n" /* initial setup: 1:1 cached for first 4GB */
"       mov     x10, #0x40000000    \n"
"       str     x9, [x16, #0]       \n"
"       add     x9, x9, x10         \n"
"       str     x9, [x16, #8]       \n"
"       add     x9, x9, x10         \n"
"       str     x9, [x16, #16]      \n"
"       add     x9, x9, x10         \n"
"       str     x9, [x16, #24]      \n"

"       adrp    x16, mmu_kernel_L1  \n" /* x16 - address of kernel's L1 map */
"       adrp    x17, mmu_kernel_L2  \n" /* x17 - address of kernel's L2 map */

"       orr     x9, x17, #3         \n" /* valid + page tagle */
"       str     x9, [x16]           \n" /* Entry 0 of the L1 kernel map points to L2 map now */

"       mov     x9, 0x70d           \n" /* Prepare 1:1 uncached map at the top of kernel address space */
"       str     x9, [x16, #4064]    \n"
"       add     x9, x9, x10         \n"
"       str     x9, [x16, #4072]    \n"
"       add     x9, x9, x10         \n"
"       str     x9, [x16, #4080]    \n"
"       add     x9, x9, x10         \n"
"       str     x9, [x16, #4088]    \n"

"       adrp    x16, _boot          \n" /* x16 - address of our kernel + offset */
"       sub     x16, x16, #0x80000  \n" /* subtract the kernel offset to get the 2MB page */
"       orr     x16, x16, #0x700    \n" /* set page attributes */
"       orr     x16, x16, #1        \n" /* page is valid */
"       mov     x9, #" xstr(KERNEL_RSRVD_PAGES) "\n" /* Enable all pages used by the kernel */
"1:     str     x16, [x17], #8      \n" /* Store pages in the L2 map */
"       add     x16, x16, #0x200000 \n" /* Advance phys address by 2MB */
"       sub     x9, x9, #1          \n"
"       cbnz    x9, 1b              \n"

/*
    MMU Map is prepared. We can continue
*/

"       ldr     x9, =_boot          \n"
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
"       sub     w10, w10, 8         \n"
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
);

void move_kernel(intptr_t from, intptr_t to);
asm(
"       .globl move_kernel          \n"
"       .type move_kernel,%function \n" /* void move_kernel(intptr_t from, intptr_t to) */
"move_kernel:                       \n" /* x0: from, x1: to */
"       movk    x0, #0xffff, lsl #32\n" /* x0: phys from in topmost part of addr space */
"       movk    x0, #0xffff, lsl #48\n"
"       movk    x1, #0xffff, lsl #32\n" /* x1: phys to in topmost part of addr space */
"       movk    x1, #0xffff, lsl #48\n"
"       mov     x2, #" xstr(KERNEL_RSRVD_PAGES << 21) "\n"
"       mov     x3, x0              \n"
"       mov     x4, x1              \n"
"       sub     x7, x1, x0          \n" /* x7: delta = (to - from) */
"1:     ldp     x5, x6, [x3], #16   \n" /* Copy kernel to new location */
"       stp     x5, x6, [x4], #16   \n"
"       ldp     x5, x6, [x3], #16   \n"
"       stp     x5, x6, [x4], #16   \n"
"       ldp     x5, x6, [x3], #16   \n"
"       stp     x5, x6, [x4], #16   \n"
"       ldp     x5, x6, [x3], #16   \n"
"       stp     x5, x6, [x4], #16   \n"
"       sub     x2, x2, #64         \n"
"       cbnz    x2, 1b              \n"

/* Fix kernel MMU table */

"       adrp    x2, mmu_kernel_L1   \n" /* Take address of L1 MMU map */
"       mov     w5, w2              \n" /* Copy it to x5 and discard the top 32 bits */
"       add     x2, x5, x1          \n" /* Add x5 to x1 and store back in x2 - this gets phys offset of MMU map at new location */
"       ldr     x3, [x2]            \n" /* Get first entry - pointer to L2 table */
"       add     x3, x3, x7          \n" /* Add delta */
"       str     x3, [x2]            \n" /* Store back */
"       adrp    x2, mmu_kernel_L2   \n" /* Take address of L2 MMU map */
"       mov     w5, w2              \n" /* Copy it to x5 and discard the top 32 bits */
"       add     x2, x5, x1          \n" /* Add x5 to x1 and store back in x2 - this gets phys offset of MMU map at new location */
"       mov     x4, #" xstr(KERNEL_RSRVD_PAGES) "\n"
"1:     ldr     x3, [x2]            \n" /* Get first entry - pointer to page */
"       add     x3, x3, x7          \n" /* Add delta */
"       str     x3, [x2], #8        \n" /* Store back */
"       sub     x4, x4, #1          \n" /* Repeat for all kernel pages */
"       cbnz    x4, 1b              \n"

/*
    Fix user MMU table. We know it consists of pointers to L2 entries, and that L2 entries are 
    pointing to 2MB pages and do not need to be adjusted at all. 
    
    Would it be not the case, we would need to iterate through entire L1, check if entries 
    point to L2, in that case fix them and fix every L2 table in the same manner down to the 
    4K pages if necessary.
*/

"       adrp    x2, mmu_user_L1     \n" /* Take address of L1 MMU map */
"       mov     w5, w2              \n" /* Copy it to x5 and discard the top 32 bits */
"       add     x2, x5, x1          \n" /* Add x5 to x1 and store back in x2 - this gets phys offset of MMU map at new location */
"       ldr     x3, [x2]            \n" /* Get first entry - pointer to L2 table */
"       add     x3, x3, x7          \n" /* Add delta */
"       str     x3, [x2], #8        \n" /* Store back */
"       ldr     x3, [x2]            \n" /* Get second entry - pointer to L2 table */
"       add     x3, x3, x7          \n" /* Add delta */
"       str     x3, [x2], #8        \n" /* Store back */
"       ldr     x3, [x2]            \n" /* Get third entry - pointer to L2 table */
"       add     x3, x3, x7          \n" /* Add delta */
"       str     x3, [x2], #8        \n" /* Store back */
"       ldr     x3, [x2]            \n" /* Get fourth entry - pointer to L2 table */
"       add     x3, x3, x7          \n" /* Add delta */
"       str     x3, [x2]            \n" /* Store back */

/*
    All is moved and fixed. Load new tables now!
*/

"       tlbi    VMALLE1             \n" /* Flush tlb */
"       dsb     sy                  \n"
"       adrp    x5, mmu_kernel_L1   \n"
"       add     w5, w5, w1          \n"
"       msr     TTBR1_EL1, x5       \n" /* Load new TTBR1 */
"       adrp    x5, mmu_user_L1     \n"
"       add     w5, w5, w1          \n"
"       msr     TTBR0_EL1, x5       \n" /* Load new TTBR0 */
"       isb                         \n"

"       br      x30                 \n" /* Return! */
);

extern int __bootstrap_end;
extern const struct BuildID g_note_build_id;
void M68K_StartEmu(void *addr);

#if EMU68_HOST_BIG_ENDIAN
static __attribute__((used)) const char bootstrapName[] = "Emu68 runtime/AArch64 BigEndian";
#else
static __attribute__((used)) const char bootstrapName[] = "Emu68 runtime/AArch64 LittleEndian";
#endif


/* L1 table for bottom half. Filled from startup code */
static __attribute__((used, section(".mmu"))) uint64_t mmu_user_L1[512];
/* Four additional directories to map the 4GB address space in 2MB pages here */
static __attribute__((used, section(".mmu"))) uint64_t mmu_user_L2[4*512];

/* L1 table for top half */
static __attribute__((used, section(".mmu"))) uint64_t mmu_kernel_L1[512];
/* One additional directory to map the 1GB kernel address space in 2MB pages here */
static __attribute__((used, section(".mmu"))) uint64_t mmu_kernel_L2[512];

uintptr_t virt2phys(uintptr_t addr)
{
    uintptr_t phys = 0;
    uint64_t *tbl = NULL;
    int idx_l1, idx_l2, idx_l3;
    uint64_t tmp;

    DV2P(kprintf("virt2phys(%p)\n", addr));

    if (addr & 0xffff000000000000) {
        DV2P(kprintf("selecting kernel tables\n"));
        asm volatile("mrs %0, TTBR1_EL1":"=r"(tbl));
        tbl = (uint64_t *)((uintptr_t)tbl | 0xffffffff00000000);
    } else {
        DV2P(kprintf("selecting user tables\n"));
        asm volatile("mrs %0, TTBR0_EL1":"=r"(tbl));
        tbl = (uint64_t *)((uintptr_t)tbl | 0xffffffff00000000);
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

void print_build_id()
{
    const uint8_t *build_id_data = &g_note_build_id.bid_Data[g_note_build_id.bid_NameLen];

    kprintf("[BOOT] Build ID: ");
    for (unsigned i = 0; i < g_note_build_id.bid_DescLen; ++i) {
        kprintf("%02x", build_id_data[i]);
    }
    kprintf("\n");
}

uintptr_t top_of_ram;

void boot(void *dtree)
{
    uintptr_t kernel_top_virt = ((uintptr_t)boot + (KERNEL_RSRVD_PAGES << 21)) & ~((KERNEL_RSRVD_PAGES << 21)-1);
    uintptr_t pool_size = kernel_top_virt - (uintptr_t)&__bootstrap_end;
    uint64_t tmp;
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

        if (raspi4)
            range++;

        top_of_ram = BE32(range[0]) + BE32(range[1]);
        intptr_t kernel_new_loc = top_of_ram - (KERNEL_RSRVD_PAGES << 21);
        intptr_t kernel_old_loc = virt2phys((intptr_t)_boot) & 0xffe00000;
        top_of_ram = kernel_new_loc - 0x1000;

        range[1] = BE32(BE32(range[1])-(KERNEL_RSRVD_PAGES << 21));

        kprintf("[BOOT] System memory: %p-%p\n", BE32(range[0]), BE32(range[0]) + BE32(range[1]) - 1);

        for (uint32_t i=BE32(range[0]) >> 21; i < (BE32(range[0]) + BE32(range[1])) >> 21; i++)
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
