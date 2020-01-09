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
#include "devicetree.h"
#include "M68k.h"
#include "HunkLoader.h"
#include "DuffCopy.h"
#include "EmuLogo.h"
#include "EmuFeatures.h"
#include "RegisterAllocator.h"

#define DV2P(x) /* x */

void _start();
void _boot();

asm("   .section .startup           \n"
"       .globl _start               \n"
"       .globl _boot                \n"
"       .type _start,%function      \n" /* Our kernel image starts with a standard header */
"_boot: b       _start              \n" /* code0: branch to the start */
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

/* L1 table for bottom half. Filled from startup code */
__attribute__((used, section(".mmu"))) uint64_t mmu_user_L1[512];
/* Four additional directories to map the 4GB address space in 2MB pages here */
__attribute__((used, section(".mmu"))) uint64_t mmu_user_L2[4*512];

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

