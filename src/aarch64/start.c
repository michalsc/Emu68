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
#include "mmu.h"
#include "tlsf.h"
#include "devicetree.h"
#include "M68k.h"
#include "HunkLoader.h"
#include "ElfLoader.h"
#include "DuffCopy.h"
#include "EmuLogo.h"
#include "EmuFeatures.h"
#include "RegisterAllocator.h"

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
"       .long " xstr(L32(0x644d5241)) "\n" /* Magic: ARM\x64 */
"       .long 0                     \n" /* res5 */

".byte 0                            \n"
".align 4                           \n"
".string \"" VERSION_STRING "\"     \n"
".byte 0                            \n"
".align 5                           \n"

"_start:                            \n"
"       mrs     x9, MPIDR_EL1       \n" /* Non BSP cores should be sleeping, but put them to sleep if case they were not */
"       ands    x9, x9, #3          \n"
"       b.eq    2f                  \n"
"1:     wfe                         \n"
"       b 1b                        \n"
"2:     mrs     x9, CurrentEL       \n" /* Since we do not use EL2 mode yet, we fall back to EL1 immediately */
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
"       mov     x9, #" xstr(MMU_OSHARE|MMU_ACCESS|MMU_ATTR(2)|MMU_PAGE) "\n" /* initial setup: 1:1 uncached for first 4GB */
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

"       orr     x9, x17, #3         \n" /* valid + page table */
"       str     x9, [x16]           \n" /* Entry 0 of the L1 kernel map points to L2 map now */

"       mov     x9, #" xstr(MMU_ISHARE|MMU_ACCESS|MMU_ATTR(0)|MMU_PAGE) "\n" /* Prepare 1:1 cached map at the top of kernel address space */
"       str     x9, [x16, #4064]    \n"
"       add     x9, x9, x10         \n"
"       str     x9, [x16, #4072]    \n"
"       add     x9, x9, x10         \n"
"       str     x9, [x16, #4080]    \n"
"       add     x9, x9, x10         \n"
"       str     x9, [x16, #4088]    \n"

"       adrp    x16, _boot          \n" /* x16 - address of our kernel + offset */
"       and     x16, x16, #~((1 << 21) - 1) \n" /* get the 2MB page */
"       movk    x16, #" xstr(MMU_ISHARE|MMU_ACCESS|MMU_ATTR(0)|MMU_PAGE) "\n" /* set page attributes */
"       mov     x9, #" xstr(KERNEL_SYS_PAGES) "\n" /* Enable all pages used by the kernel */
"1:     str     x16, [x17], #8      \n" /* Store pages in the L2 map */
"       add     x16, x16, #0x200000 \n" /* Advance phys address by 2MB */
"       sub     x9, x9, #1          \n"
"       cbnz    x9, 1b              \n"

/*
    MMU Map is prepared. We can continue
*/

"       ldr     x9, =_boot          \n" /* Set up stack */
"       mov     sp, x9              \n"
"       mov     x10, #0x00300000    \n" /* Enable signle and double VFP coprocessors in EL1 and EL0 */
"       msr     CPACR_EL1, x10      \n"
"       isb     sy                  \n"
"       isb     sy                  \n" /* Drain the insn queue */
"       ic      IALLU               \n" /* Invalidate entire instruction cache */
"       isb     sy                  \n"

                                        /* Attr0 - write-back cacheable RAM, Attr1 - device, Attr2 - non-cacheable */
"       ldr     x10, =" xstr(ATTR_CACHED | (ATTR_DEVICE_nGnRE << 8) | (ATTR_NOCACHE << 16)) "\n"
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
"       ldr     w10, =0x000003c9    \n"
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
"       adrp    x2, _boot           \n"
"       mov     w3, w2              \n"
"1:     sub     x2, x2, #32         \n"
"       dc      civac, x2           \n"
"       sub     w3, w3, #32         \n"
"       cbnz    w3, 1b              \n"
"       dsb     sy                  \n"
"       movk    x0, #0xffff, lsl #32\n" /* x0: phys from in topmost part of addr space */
"       movk    x0, #0xffff, lsl #48\n"
"       movk    x1, #0xffff, lsl #32\n" /* x1: phys to in topmost part of addr space */
"       movk    x1, #0xffff, lsl #48\n"
"       mov     x2, #" xstr(KERNEL_SYS_PAGES << 21) "\n"
"       mov     x3, x0              \n"
"       mov     x4, x1              \n"
"       sub     x7, x1, x0          \n" /* x7: delta = (to - from) */
"2:     ldp     x5, x6, [x3], #16   \n" /* Copy kernel to new location */
"       stp     x5, x6, [x4], #16   \n"
"       ldp     x5, x6, [x3], #16   \n"
"       stp     x5, x6, [x4], #16   \n"
"       ldp     x5, x6, [x3], #16   \n"
"       stp     x5, x6, [x4], #16   \n"
"       ldp     x5, x6, [x3], #16   \n"
"       stp     x5, x6, [x4], #16   \n"
"       sub     x2, x2, #64         \n"
"       cbnz    x2, 2b              \n"

"       mrs     x5, TTBR1_EL1       \n"
"       add     x5, x5, x7          \n"
"       msr     TTBR1_EL1, x5       \n" /* Load new TTBR1 */
"       mrs     x5, TTBR0_EL1       \n"
"       add     x5, x5, x7          \n"
"       msr     TTBR0_EL1, x5       \n" /* Load new TTBR0 */

"       dsb     ish                 \n"
"       tlbi    VMALLE1IS           \n" /* Flush tlb */
"       dsb     sy                  \n"
"       isb                         \n"

/* Fix kernel MMU table */

"       adrp    x5, _boot           \n"
"       adr     x2, 1f              \n"
"       orr     x2, x2, 0xffffffff00000000  \n"
"       add     x2, x2, x7          \n"
"       br      x2                  \n"
"1:     mrs     x2, TTBR1_EL1       \n" /* Take address of L1 MMU map */
"       and     x2, x2, 0xfffff000  \n" /* Discard the top 32 bits and lowest 12 bits */
"       orr     x2, x2, 0xffffffff00000000\n" /* Go to the 1:1 map at top of ram */
"       ldr     x3, [x2]            \n" /* Get first entry - pointer to L2 table */
"       add     x3, x3, x7          \n" /* Add delta */
"       str     x3, [x2]            \n" /* Store back */
"       dsb     ish                 \n"
"       tlbi    VMALLE1IS           \n" /* Flush tlb */
"       dsb     sy                  \n"
"       isb                         \n"
"       and     x2, x3, 0xfffff000  \n" /* Copy L2 pointer to x5, discard the top 32 bits and bottom 12 bits*/
"       and     x5, x5, 0xffffffff00000000 \n"
"       orr     x2, x2, 0xffffffff00000000 \n"
"       mov     x4, #" xstr(KERNEL_SYS_PAGES) "\n"
"1:     ldr     x3, [x2]            \n" /* Get first entry - pointer to page */
"       add     x3, x3, x7          \n" /* Add delta */
"       str     x3, [x2], #8        \n" /* Store back */
"       dsb     ish                 \n"
"       tlbi    vae1, x5            \n"
"       dsb     ish                 \n"
"       isb                         \n"
"       add     x5, x5, #0x200000   \n"
"       sub     x4, x4, #1          \n" /* Repeat for all kernel pages */
"       cbnz    x4, 1b              \n"
"       ret                         \n" /* Return! */
);

#if EMU68_HOST_BIG_ENDIAN
static __attribute__((used)) const char bootstrapName[] = "Emu68 runtime/AArch64 BigEndian";
#else
static __attribute__((used)) const char bootstrapName[] = "Emu68 runtime/AArch64 LittleEndian";
#endif

extern int __bootstrap_end;
extern const struct BuildID g_note_build_id;

void print_build_id()
{
    const uint8_t *build_id_data = &g_note_build_id.bid_Data[g_note_build_id.bid_NameLen];

    kprintf("[BOOT] Build ID: ");
    for (unsigned i = 0; i < g_note_build_id.bid_DescLen; ++i) {
        kprintf("%02x", build_id_data[i]);
    }
    kprintf("\n");
}

void M68K_StartEmu(void *addr, void *fdt);

void boot(void *dtree)
{
    uintptr_t kernel_top_virt = ((uintptr_t)boot + (KERNEL_SYS_PAGES << 21)) & ~((1 << 21)-1);
    uintptr_t pool_size = kernel_top_virt - (uintptr_t)&__bootstrap_end;
    uint64_t tmp;
    uintptr_t top_of_ram = 0;

    of_node_t *e = NULL;

    /* Enable caches and cache maintenance instructions from EL0 */
    asm volatile("mrs %0, SCTLR_EL1":"=r"(tmp));
    tmp |= (1 << 2) | (1 << 12);    // Enable D and I caches
    tmp |= (1 << 26);               // Enable Cache clear instructions from EL0
    asm volatile("msr SCTLR_EL1, %0"::"r"(tmp));

    /* Initialize tlsf */
    tlsf = tlsf_init_with_memory(&__bootstrap_end, pool_size);

    /* Parse device tree */
    dt_parse((void*)dtree);

    /* Prepare MMU */
    mmu_init();

    /* Setup platform (peripherals etc) */
    platform_init();

    /* Setup debug console on serial port */
    setup_serial();

    kprintf("\033[2J[BOOT] Booting %s\n", bootstrapName);
    kprintf("[BOOT] Boot address is %p\n", _start);

    print_build_id();

    kprintf("[BOOT] ARM stack top at %p\n", &_boot);
    kprintf("[BOOT] Bootstrap ends at %p\n", &__bootstrap_end);

    kprintf("[BOOT] Kernel args (%p)\n", dtree);

    e = dt_find_node("/memory");

    if (e)
    {
        of_property_t *p = dt_find_property(e, "reg");
        uint32_t *range = p->op_value;
        int size_cells = dt_get_property_value_u32(e, "#size-cells", 1, TRUE);
        int address_cells = dt_get_property_value_u32(e, "#address-cells", 1, TRUE);
        int addr_pos = address_cells - 1;
        int size_pos = addr_pos + size_cells;

        top_of_ram = BE32(range[addr_pos]) + BE32(range[size_pos]);
        intptr_t kernel_new_loc = top_of_ram - (KERNEL_RSRVD_PAGES << 21);
        intptr_t kernel_old_loc = mmu_virt2phys((intptr_t)_boot) & 0xffe00000;
        top_of_ram = kernel_new_loc - 0x1000;

        range[size_pos] = BE32(BE32(range[size_pos])-(KERNEL_RSRVD_PAGES << 21));

        kprintf("[BOOT] System memory: %p-%p\n", BE32(range[addr_pos]), BE32(range[addr_pos]) + BE32(range[size_pos]) - 1);

        mmu_map(range[addr_pos], range[addr_pos], range[size_pos], MMU_ACCESS | MMU_ISHARE | MMU_ATTR(0), 0);
        mmu_map(kernel_new_loc + (KERNEL_SYS_PAGES << 21), 0xffffffe000000000, KERNEL_JIT_PAGES << 21, MMU_ACCESS | MMU_ISHARE | MMU_ATTR(0), 0);
        mmu_map(kernel_new_loc + (KERNEL_SYS_PAGES << 21), 0xfffffff000000000, KERNEL_JIT_PAGES << 21, MMU_ACCESS | MMU_ISHARE | MMU_ALLOW_EL0 | MMU_READ_ONLY | MMU_ATTR(0), 0);

        jit_tlsf = tlsf_init_with_memory((void*)0xffffffe000000000, KERNEL_JIT_PAGES << 21);

        kprintf("[BOOT] Local memory pools:\n");
        kprintf("[BOOT]    SYS: %p - %p (size: %5d kB)\n", &__bootstrap_end, kernel_top_virt - 1, pool_size / 1024);
        kprintf("[BOOT]    JIT: %p - %p (size: %5d kB)\n", 0xffffffe000000000,
                    0xffffffe000000000 + (KERNEL_JIT_PAGES << 21) - 1, KERNEL_JIT_PAGES << 11);

        kprintf("[BOOT] Moving kernel from %p to %p\n", (void*)kernel_old_loc, (void*)kernel_new_loc);

        /*
            Copy the kernel memory block from origin to new destination, use the top of
            the kernel space which is a 1:1 map of first 4GB region, uncached
        */
        arm_flush_cache((uintptr_t)_boot & 0xffffffff00000000, KERNEL_SYS_PAGES << 21);

        /*
            We use routine in assembler here, because we will move both kernel code *and* stack.
            Playing with C code without knowledge what will happen to the stack after move is ready
            can result in funny Heisenbugs...
        */
        move_kernel(kernel_old_loc, kernel_new_loc);

        kprintf("[BOOT] Kernel moved, MMU tables updated\n");
    }

    platform_post_init();

    e = dt_find_node("/chosen");

    if (e)
    {
        void *image_start, *image_end;
        of_property_t *p = dt_find_property(e, "linux,initrd-start");
        void *fdt = (void*)(top_of_ram - ((dt_total_size() + 4095) & ~4095));
        memcpy(fdt, dt_fdt_base(), dt_total_size());
        top_of_ram -= (dt_total_size() + 4095) & ~4095;

        if (p)
        {
            image_start = (void*)(intptr_t)BE32(*(uint32_t*)p->op_value);
            p = dt_find_property(e, "linux,initrd-end");
            image_end = (void*)(intptr_t)BE32(*(uint32_t*)p->op_value);
            uint32_t magic = BE32(*(uint32_t*)image_start);

            if (magic == 0x3f3)
            {
                kprintf("[BOOT] Loading HUNK executable from %p-%p\n", image_start, image_end);
                void *hunks = LoadHunkFile(image_start);
                (void)hunks;
                M68K_StartEmu((void *)((intptr_t)hunks + 4), fdt);
            }
            else if (magic == 0x7f454c46)
            {
                uint32_t rw, ro;
                if (GetElfSize(image_start, &rw, &ro))
                {
                    rw = (rw + 4095) & ~4095;
                    ro = (ro + 4095) & ~4095;

                    top_of_ram -= rw + ro;
                    top_of_ram &= ~0x1fffff;

                    kprintf("[BOOT] Loading ELF executable from %p-%p to %p\n", image_start, image_end, top_of_ram);
                    void *ptr = LoadELFFile(image_start, (void*)top_of_ram);
                    M68K_StartEmu(ptr, fdt);
                }
            }
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

    asm volatile("ldp w%0, w%1, %2"::"i"(REG_D0),"i"(REG_D1),"m"(ctx->D[0].u32));
    asm volatile("ldp w%0, w%1, %2"::"i"(REG_D2),"i"(REG_D3),"m"(ctx->D[2].u32));
    asm volatile("ldp w%0, w%1, %2"::"i"(REG_D4),"i"(REG_D5),"m"(ctx->D[4].u32));
    asm volatile("ldp w%0, w%1, %2"::"i"(REG_D6),"i"(REG_D7),"m"(ctx->D[6].u32));

    asm volatile("ldp w%0, w%1, %2"::"i"(REG_A0),"i"(REG_A1),"m"(ctx->A[0].u32));
    asm volatile("ldp w%0, w%1, %2"::"i"(REG_A2),"i"(REG_A3),"m"(ctx->A[2].u32));
    asm volatile("ldp w%0, w%1, %2"::"i"(REG_A4),"i"(REG_A5),"m"(ctx->A[4].u32));
    asm volatile("ldp w%0, w%1, %2"::"i"(REG_A6),"i"(REG_A7),"m"(ctx->A[6].u32));

    asm volatile("ldr w%0, %1"::"i"(REG_PC),"m"(ctx->PC));

    asm volatile("ldr d%0, %1"::"i"(REG_FP0),"m"(ctx->FP[0]));
    asm volatile("ldr d%0, %1"::"i"(REG_FP1),"m"(ctx->FP[1]));
    asm volatile("ldr d%0, %1"::"i"(REG_FP2),"m"(ctx->FP[2]));
    asm volatile("ldr d%0, %1"::"i"(REG_FP3),"m"(ctx->FP[3]));
    asm volatile("ldr d%0, %1"::"i"(REG_FP4),"m"(ctx->FP[4]));
    asm volatile("ldr d%0, %1"::"i"(REG_FP5),"m"(ctx->FP[5]));
    asm volatile("ldr d%0, %1"::"i"(REG_FP6),"m"(ctx->FP[6]));
    asm volatile("ldr d%0, %1"::"i"(REG_FP7),"m"(ctx->FP[7]));

    asm volatile("ldrh w1, %0; msr tpidr_EL0, x1"::"m"(ctx->SR):"x1");
}

void M68K_SaveContext(struct M68KState *ctx)
{
    asm volatile("stp w%0, w%1, %2"::"i"(REG_D0),"i"(REG_D1),"m"(ctx->D[0].u32));
    asm volatile("stp w%0, w%1, %2"::"i"(REG_D2),"i"(REG_D3),"m"(ctx->D[2].u32));
    asm volatile("stp w%0, w%1, %2"::"i"(REG_D4),"i"(REG_D5),"m"(ctx->D[4].u32));
    asm volatile("stp w%0, w%1, %2"::"i"(REG_D6),"i"(REG_D7),"m"(ctx->D[6].u32));

    asm volatile("stp w%0, w%1, %2"::"i"(REG_A0),"i"(REG_A1),"m"(ctx->A[0].u32));
    asm volatile("stp w%0, w%1, %2"::"i"(REG_A2),"i"(REG_A3),"m"(ctx->A[2].u32));
    asm volatile("stp w%0, w%1, %2"::"i"(REG_A4),"i"(REG_A5),"m"(ctx->A[4].u32));
    asm volatile("stp w%0, w%1, %2"::"i"(REG_A6),"i"(REG_A7),"m"(ctx->A[6].u32));
#if 0
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
#endif
    asm volatile("str w%0, %1"::"i"(REG_PC),"m"(ctx->PC));

    asm volatile("str d%0, %1"::"i"(REG_FP0),"m"(ctx->FP[0]));
    asm volatile("str d%0, %1"::"i"(REG_FP1),"m"(ctx->FP[1]));
    asm volatile("str d%0, %1"::"i"(REG_FP2),"m"(ctx->FP[2]));
    asm volatile("str d%0, %1"::"i"(REG_FP3),"m"(ctx->FP[3]));
    asm volatile("str d%0, %1"::"i"(REG_FP4),"m"(ctx->FP[4]));
    asm volatile("str d%0, %1"::"i"(REG_FP5),"m"(ctx->FP[5]));
    asm volatile("str d%0, %1"::"i"(REG_FP6),"m"(ctx->FP[6]));
    asm volatile("str d%0, %1"::"i"(REG_FP7),"m"(ctx->FP[7]));

    asm volatile("mrs x1, tpidr_EL0; strh w1, %0"::"m"(ctx->SR):"x1");
}

void M68K_PrintContext(struct M68KState *m68k)
{
//    M68K_SaveContext(m68k);

    kprintf("[JIT] M68K Context:\n[JIT] ");

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
    
    kprintf("T%d", sr >> 14);
    
    if (sr & SR_S)
        kprintf("S");
    else
        kprintf(".");
    
    if (sr & SR_M)
        kprintf("M.");
    else
        kprintf("..");
    
    kprintf("IP%d...", (sr >> 8) & 7);

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

    kprintf("\n[JIT]     CACR=0x%08x    VBR= 0x%08x", BE32(m68k->CACR), BE32(m68k->VBR));
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

uint32_t last_PC = 0xffffffff;

uint16_t *framebuffer __attribute__((weak)) = NULL;
uint32_t pitch  __attribute__((weak))= 0;
uint32_t fb_width  __attribute__((weak))= 0;
uint32_t fb_height  __attribute__((weak))= 0;

void ExecutionLoop(struct M68KState *ctx);

struct M68KTranslationUnit *_FindUnit(uint16_t *ptr)
{
    return M68K_FindTranslationUnit(ptr);
}

void stub_FindUnit()
{
    asm volatile(
"FindUnit:                                  \n"
"       adr     x4, ICache                  \n"
"       eor     w0, w%[reg_pc], w%[reg_pc], lsr #16 \n"
"       and     x0, x0, #0xffff             \n"
"       ldr     x4, [x4]                    \n" // 1 -> 4
"       add     x0, x0, x0, lsl #1          \n"
"       ldr     x0, [x4, x0, lsl #3]        \n"
"       b       1f                          \n"
"3:     ldr     x5, [x0, #32]               \n" // 2 -> 5
"       cmp     w5, w%[reg_pc]              \n"
"       b.eq    2f                          \n"
"       mov     x0, x4                      \n"
"1:     ldr     x4, [x0]                    \n"
"       cbnz    x4, 3b                      \n"
"       mov     x0, #0                      \n"
"4:     ret                                 \n"
"2:     ldr     x4, [x0, #24]               \n"
"       ldr     x5, [x4, #8]                \n"
"       cbz     x5, 4b                      \n"
"       ldr     x6, [x0, #16]               \n" // 3 -> 6
"       stp     x4, x5, [x0, #16]           \n"
"       add     x7, x0, #0x10               \n" // 4 -> 7
"       str     x7, [x5]                    \n"
"       stp     x6, x7, [x4]                \n"
"       str     x4, [x6, #8]                \n"
"       ret                                 \n"

::[reg_pc]"i"(REG_PC));
}

void stub_ExecutionLoop()
{
    asm volatile(
"ExecutionLoop:                             \n"
"       stp     x29, x30, [sp, #-128]!      \n"
"       stp     x27, x28, [sp, #1*16]       \n"
"       stp     x25, x26, [sp, #2*16]       \n"
"       stp     x23, x24, [sp, #3*16]       \n"
"       stp     x21, x22, [sp, #4*16]       \n"
"       stp     x19, x20, [sp, #5*16]       \n"
"       bl      M68K_LoadContext            \n"

"1:     cmp     wzr, w%[reg_pc]             \n"
"       b.eq    4f                          \n"
"       adr     x9, last_PC                 \n"
"       ldr     w1, [x9]                    \n"
"       mrs     x0, TPIDRRO_EL0             \n"
"       ldr     w1, [x0, #%[cacr]]          \n"
"       ands    wzr, w1, #%[cacr_ie]        \n"
"       b.eq    2f                          \n"

"       cmp     w1, w%[reg_pc]              \n"
"       b.ne    13f                         \n"
"       str     x2, [sp, #96]               \n"
"       blr     x2                          \n"
"       ldr     x2, [sp, #96]               \n"
"       b       1b                          \n"

"13:                                        \n"
"       adr     x4, ICache                  \n"
"       eor     w0, w%[reg_pc], w%[reg_pc], lsr #16 \n"
"       and     x0, x0, #0xffff             \n"
"       ldr     x4, [x4]                    \n" // 1 -> 4
"       add     x0, x0, x0, lsl #1          \n"
"       ldr     x0, [x4, x0, lsl #3]        \n"
"       b       51f                         \n"
"53:     ldr     x5, [x0, #32]              \n" // 2 -> 5
"       cmp     w5, w%[reg_pc]              \n"
"       b.eq    52f                         \n"
"       mov     x0, x4                      \n"
"51:     ldr     x4, [x0]                   \n"
"       cbnz    x4, 53b                     \n"
"       b 5f                                \n"
"52:     ldr     x4, [x0, #24]              \n"
"       ldr     x5, [x4, #8]                \n"
"       cbz     x5, 55f                     \n"
"       ldr     x6, [x0, #16]               \n" // 3 -> 6
"       stp     x4, x5, [x0, #16]           \n"
"       add     x7, x0, #0x10               \n" // 4 -> 7
"       str     x7, [x5]                    \n"
"       stp     x6, x7, [x4]                \n"
"       str     x4, [x6, #8]                \n"

"55:                                        \n"
"       ldr     x2, [x0, #%[offset]]        \n"
"       str     x2, [sp, #96]               \n"
"       str     w%[reg_pc], [x9]            \n"
"       blr     x2                          \n"
"       ldr     x2, [sp, #96]               \n"
"       b       1b                          \n"

"5:     mrs     x0, TPIDRRO_EL0             \n"
"       bl      M68K_SaveContext            \n"
"       mov     w0, w%[reg_pc]              \n"
"       str     w%[reg_pc], [x9]            \n"
"       bl      M68K_GetTranslationUnit     \n"
"       ldr     x2, [x0, #%[offset]]        \n"
"       str     x2, [sp, #96]               \n"
"       mrs     x0, TPIDRRO_EL0             \n"
"       bl      M68K_LoadContext            \n"
"       blr     x2                          \n"
"       ldr     x2, [sp, #96]               \n"
"       b       1b                          \n"


#if 0
"2:     cmp     w1, w%[reg_pc]              \n"
"       b.ne    23f                         \n"
"       str     x2, [sp, #96]               \n"
"       blr     x2                          \n"
"       ldr     x2, [sp, #96]               \n"
"       b       1b                          \n"
#else
"2:                                         \n"
#endif
"23:    bl      M68K_SaveContext            \n"
"       mvn     w0, wzr                     \n"
"       str     w0, [x9]                    \n"
"       mov     w0, w%[reg_pc]              \n"
"       bl      M68K_TranslateNoCache       \n"
"       mov     x2, x0                      \n"
"       str     x0, [sp, #96]               \n"
"       mrs     x0, TPIDRRO_EL0             \n"
"       bl      M68K_LoadContext            \n"
"       blr     x2                          \n"
"       ldr     x2, [sp, #96]               \n"
"       b       1b                          \n"

"4:     mrs     x0, TPIDRRO_EL0             \n"
"       bl      M68K_SaveContext            \n"
"       ldp     x27, x28, [sp, #1*16]       \n"
"       ldp     x25, x26, [sp, #2*16]       \n"
"       ldp     x23, x24, [sp, #3*16]       \n"
"       ldp     x21, x22, [sp, #4*16]       \n"
"       ldp     x19, x20, [sp, #5*16]       \n"
"       ldp     x29, x30, [sp], #128        \n"
"       ret                                 \n"

:
:[reg_pc]"i"(REG_PC),
 [cacr_ie]"i"(CACR_IE),
 [cacr]"i"(__builtin_offsetof(struct M68KState, CACR)),
 [offset]"i"(__builtin_offsetof(struct M68KTranslationUnit, mt_ARMEntryPoint))

    );


}



void M68K_StartEmu(void *addr, void *fdt)
{
    void (*arm_code)();
    struct M68KTranslationUnit * unit = (void*)0;
    struct M68KState __m68k;
    uint64_t t1=0, t2=0;
    uint32_t m68k_pc;

    M68K_InitializeCache();

    bzero(&__m68k, sizeof(__m68k));
    bzero((void *)4, 1020);

    *(uint32_t*)4 = 0;

    __m68k.D[0].u32 = BE32((uint32_t)pitch);
    __m68k.D[1].u32 = BE32((uint32_t)fb_width);
    __m68k.D[2].u32 = BE32((uint32_t)fb_height);
    __m68k.A[0].u32 = BE32((uint32_t)(intptr_t)framebuffer);

    __m68k.A[6].u32 = BE32((intptr_t)fdt);
    __m68k.A[7].u32 = BE32(((intptr_t)addr - 4096)& 0xfffff000);
    __m68k.ISP.u32 = __m68k.A[7].u32;
    __m68k.PC = BE32((intptr_t)addr);
    __m68k.A[7].u32 = BE32(BE32(__m68k.A[7].u32) - 4);
    __m68k.SR = BE16(SR_S | SR_IPL);
    //__m68k.CACR = BE32(0x80008000);
    *(uint32_t*)(intptr_t)(BE32(__m68k.A[7].u32)) = 0;

/*
    M68K_LoadContext(&__m68k);
*/
    kprintf("[JIT]\n");
    M68K_PrintContext(&__m68k);

    kprintf("[JIT] Let it go...\n");

    t1 = LE32(*(volatile uint32_t*)0xf2003004) | (uint64_t)LE32(*(volatile uint32_t *)0xf2003008) << 32;

    asm volatile("mov %0, x%1":"=r"(m68k_pc):"i"(REG_PC));

    /*
    unit = M68K_GetTranslationUnit((uint16_t *)(uintptr_t)m68k_pc);
    last_PC = m68k_pc;
    *(void**)(&arm_code) = unit->mt_ARMEntryPoint;
    */
    last_PC = 0xffffffff;
    *(void**)(&arm_code) = NULL;

#if 1
    (void)unit;
    ExecutionLoop(&__m68k);
#if 0
    uint32_t tmp = 0;
    asm volatile(
"1:     cmp     wzr, w%[reg_pc]             \n"
"       b.eq    4f                          \n"
"       adrp %[tmp], last_PC                \n"
"       ldr %w[last_pc], [%[tmp]]           \n"
"       mrs %[tmp], TPIDRRO_EL0             \n"
"       ldr %w[tmp], [%[tmp], #%[cacr]]     \n"
"       ands xzr, %[tmp], #%[cacr_ie]       \n"
"       b.eq 2f                             \n"

"       cmp     %w[last_pc], w%[reg_pc]     \n"
"       b.ne    13f                         \n"
"       str     x0, [sp, #8]                \n"
"       blr     x0                          \n"
"       ldr     x0, [sp, #8]                \n"
"       b       1b                          \n"

"13:    mov     w0, w%[reg_pc]              \n"
"       adrp    %[tmp], last_PC             \n"
"       str     w%[reg_pc], [%[tmp]]        \n"
"       bl      M68K_GetTranslationUnit     \n"
"       ldr     x0, [x0, #%[offset]]        \n"
"       str     x0, [sp, #8]                \n"
"       blr     x0                          \n"
"       ldr     x0, [sp, #8]                \n"
"       b       1b                          \n"
#if 0
"2:     cmp     %w[last_pc], w%[reg_pc]     \n"
"       b.ne    23f                          \n"
"       str     x0, [sp, #8]                \n"
"       blr     x0                          \n"
"       ldr     x0, [sp, #8]                \n"
"       b       1b                          \n"
#else
"2:\n"
#endif
"23:    mov     w0, w%[reg_pc]              \n"
"       adrp    %[tmp], last_PC             \n"
"       str     w%[reg_pc], [%[tmp]]        \n"
"       bl      M68K_TranslateNoCache       \n"
"       str     x0, [sp, #8]                \n"
"       blr     x0                          \n"
"       ldr     x0, [sp, #8]                \n"
"       b       1b                          \n"

"4:     \n"
:
:[last_pc]"r"(last_PC),
 [tmp]"r"(tmp),
 [reg_pc]"i"(REG_PC),
 [cacr_ie]"i"(CACR_IE),
 [cacr]"i"(__builtin_offsetof(struct M68KState, CACR)),
 [offset]"i"(__builtin_offsetof(struct M68KTranslationUnit, mt_ARMEntryPoint))
:"x0","x30"
    );
#endif
#else
    do
    {
        if (__m68k.CACR & CACR_IE)
        {
            if (last_PC != m68k_pc)
            {
                //unit = M68K_FindTranslationUnit((uint16_t *)(uintptr_t)m68k_pc);
                //if (!unit)
                    unit = M68K_GetTranslationUnit((uint16_t *)(uintptr_t)m68k_pc);
                last_PC = m68k_pc;
                *(void**)(&arm_code) = unit->mt_ARMEntryPoint;
            }
        }
        else
        {
            //if (last_PC != m68k_pc)
            {
                *(void**)(&arm_code) = M68K_TranslateNoCache((uint16_t *)(uintptr_t)m68k_pc);
                //last_PC = m68k_pc;
                last_PC = 0xffffffff;
            }
        }
        arm_code();
        asm volatile("mov %0, x%1":"=r"(m68k_pc):"i"(REG_PC));

    } while(m68k_pc != 0);
#endif
    t2 = LE32(*(volatile uint32_t*)0xf2003004) | (uint64_t)LE32(*(volatile uint32_t *)0xf2003008) << 32;

    kprintf("[JIT] Time spent in m68k mode: %lld us\n", t2-t1);

    kprintf("[JIT] Back from translated code\n");

    kprintf("[JIT]\n");
    M68K_PrintContext(&__m68k);

    M68K_DumpStats();
}
