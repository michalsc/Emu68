/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdint.h>
#include "config.h"

void boot(uintptr_t dummy, uintptr_t arch, uintptr_t atags, uintptr_t dummy2);

asm("   .section .startup           \n"
"       .globl _start               \n"
"       .type _start,%function      \n"
"_start:                            \n"
"       mrs     r4, cpsr_all        \n" /* Check if in hypervisor mode */
"       and     r4, r4, #0x1f       \n"
"       mov     r8, #0x1a           \n"
"       cmp     r4, r8              \n"
"       beq     leave_hyper         \n"
"continue_boot:                     \n"
"       cps     #0x13               \n" /* Should be in SVC (supervisor) mode already, but just incase.. */
#if EMU68_HOST_BIG_ENDIAN
"       setend  be                  \n" /* Switch to big endian mode */
#endif
"       ldr     sp, tmp_stack_ptr   \n"
"       mrc     p15,0,r4,c1,c0,2    \n" /* Enable signle and double VFP coprocessors */
"       orr     r4, r4, #0x00f00000 \n" /* This is necessary since gcc might want to use vfp registers  */
"       mcr     p15,0,r4,c1,c0,2    \n" /* Either as cache for general purpose regs or e.g. for division. This is the case with gcc9 */
"       isb                         \n" /* Synchronize the pipeline */
"       isb                         \n" /* Synchronize the pipeline */
"       isb                         \n" /* Synchronize the pipeline */
"       isb                         \n" /* Synchronize the pipeline */
"       isb                         \n" /* Synchronize the pipeline */
"       vmrs    r4,fpexc            \n" /* Fetch fpexc */
"       orr     r4,r4,#0x40000000   \n" /* Set enable bit */
"       vmsr    fpexc,r4            \n" /* Enable VFP now */
"       mov     r0,#0               \n"
"       dsb                         \n"
"       mcr     p15,0,r0,c7,c5,0    \n" /* invalidate icache */
"       isb                         \n"
"       ldr     r4, mmu_table_ptr   \n" /* Load MMU table pointer */
"       mcr     p15,0,r4,c2,c0,0    \n" /* Write page_dir address to ttbr0 */
"       mov     r8, #0              \n"
"       mcr     p15,0,r8,c2,c0,2    \n" /* Write ttbr control N = 0 (use only ttbr0) */
"       mov     r4, #1              \n"
"       mcr     p15,0,r4,c3,c0,0    \n" /* Set domains - Dom0 is usable, rest is disabled */
"       mrc     p15,0,r4,c1,c0,0    \n" /* Load control register */
"       orr     r4,r4,#8388608      \n" /* v6 page tables, subpages disabled */
"       orr     r4,r4,#1            \n" /* Enable MMU */
#if EMU68_HOST_BIG_ENDIAN
"       orr     r4,r4,#1<<25        \n" /* MMU tables in big endian */
#endif
"       dsb                         \n" /* DSB */
"       mcr     p15,0,r4,c1,c0,0    \n" /* Set control register and thus really enable mmu */
"       isb                         \n"
"       ldr r4, =__bss_start        \n" /* Clear .bss */
"       ldr r9, =__bss_end          \n"
"       mov r5, #0                  \n"
"       mov r6, #0                  \n"
"       mov r7, #0                  \n"
"       mov r8, #0                  \n"
"       b       2f                  \n"
"1:                                 \n"
"       stmia r4!, {r5-r8}          \n"
"2:                                 \n"
"       cmp r4, r9                  \n"
"       blo 1b                      \n"
"       ldr     r4, boot_address    \n"
"       isb                         \n" /* ISB */
"       bx      r4                  \n"
"leave_hyper:                       \n"
#if EMU68_HOST_BIG_ENDIAN
"       setend  be                  \n"
#endif
"       adr     r4, continue_boot   \n"
"       .byte   0x04,0xf3,0x2e,0xe1 \n" /* msr     ELR_hyp, r4  */
"       mrs     r4, cpsr_all        \n"
"       and     r4, r4, #0x1f       \n"
"       orr     r4, r4, #0x13       \n"
"       .byte   0x04,0xf3,0x6e,0xe1 \n" /* msr     SPSR_hyp, r4 */
"       .byte   0x6e,0x00,0x60,0xe1 \n" /* eret                 */
"       .section .text              \n"
".byte 0                            \n"
".string \"$VER: Emu68.img " VERSION_STRING_DATE "\"\n"
".byte 0                            \n"
"\n\t\n\t"
);

static uint32_t arm_stack[10241] __attribute__((aligned(64)));

__attribute__((used)) void * tmp_stack_ptr __attribute__((used, section(".startup @"))) = (void *)(&arm_stack[10240]);
extern int __bootstrap_end;

/*
    Initial MMU map covers first 8 MB of RAM and shadows this RAM at topmost 8 MB of address space. Peripherals will be mapped
    at 0xf2000000 - 0xf2ffffff.

    After successfull start the topmost 8MB of memory will be used for emulation and the code will be moved there. Once ready,
    the MMU map will be updated accordingly
*/
__attribute__((used, section(".mmu"))) uint32_t mmu_table[4096] = {
    [0x000] = 0x00001c0e,   /* caches write-through, write allocate, access for all */
    [0x001] = 0x00101c0e,
    [0x002] = 0x00201c0e,
    [0x003] = 0x00301c0e,
    [0x004] = 0x00401c0e,
    [0x005] = 0x00501c0e,
    [0x006] = 0x00601c0e,
    [0x007] = 0x00701c0e,

    [0xff8] = 0x00001c0e,   /* shadow of first 8 MB with the same attributes */
    [0xff9] = 0x00101c0e,
    [0xffa] = 0x00201c0e,
    [0xffb] = 0x00301c0e,
    [0xffc] = 0x00401c0e,
    [0xffd] = 0x00501c0e,
    [0xffe] = 0x00601c0e,
    [0xfff] = 0x00701c0e
};

/* Trivial virtual to physical translator, fetches data from MMU table and assumes 1M pages */
uintptr_t mmu_virt2phys(uintptr_t virt_addr)
{
    uint32_t page = virt_addr >> 20;
    uint32_t offset = virt_addr & 0x000fffff;

    offset |= mmu_table[page] & 0xfff00000;

    return offset;
}

__attribute__((used)) void * mmu_table_ptr __attribute__((used, section(".startup @"))) = (void *)((uintptr_t)mmu_table - 0xff800000);
__attribute__((used)) void * boot_address __attribute__((used, section(".startup @"))) = (void *)((intptr_t)boot);
