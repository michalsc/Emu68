/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdint.h>
#include "A64.h"
#include "config.h"
#include "support.h"
#include "mmu.h"
#include "tlsf.h"
#include "M68k.h"

#define FULL_CONTEXT 1

#define SAVE_SHORT_CONTEXT \
    "       stp x0, x1, [sp, -176]!         \n" \
    "       stp x2, x3, [sp, #1*16]         \n" \
    "       stp x4, x5, [sp, #2*16]         \n" \
    "       stp x6, x7, [sp, #3*16]         \n" \
    "       stp x8, x9, [sp, #4*16]         \n" \
    "       stp x10, x11, [sp, #5*16]       \n" \
    "       stp x12, x13, [sp, #6*16]       \n" \
    "       stp x14, x15, [sp, #7*16]       \n" \
    "       stp x16, x17, [sp, #8*16]       \n" \
    "       stp x18, x30, [sp, #9*16]       \n"

#define SAVE_FULL_CONTEXT \
    "       stp x0, x1, [sp, -256]!         \n" \
    "       stp x2, x3, [sp, #1*16]         \n" \
    "       stp x4, x5, [sp, #2*16]         \n" \
    "       stp x6, x7, [sp, #3*16]         \n" \
    "       stp x8, x9, [sp, #4*16]         \n" \
    "       stp x10, x11, [sp, #5*16]       \n" \
    "       stp x12, x13, [sp, #6*16]       \n" \
    "       stp x14, x15, [sp, #7*16]       \n" \
    "       stp x16, x17, [sp, #8*16]       \n" \
    "       stp x18, x19, [sp, #9*16]       \n" \
    "       stp x20, x21, [sp, #10*16]      \n" \
    "       stp x22, x23, [sp, #11*16]      \n" \
    "       stp x24, x25, [sp, #12*16]      \n" \
    "       stp x26, x27, [sp, #13*16]      \n" \
    "       stp x28, x29, [sp, #14*16]      \n" \
    "       mov x0, sp                      \n" \
    "       stp x30, x0, [sp, #15*16]       \n"

#define LOAD_FULL_CONTEXT \
    "       ldp x2, x3, [sp, #1*16]         \n" \
    "       ldp x4, x5, [sp, #2*16]         \n" \
    "       ldp x6, x7, [sp, #3*16]         \n" \
    "       ldp x8, x9, [sp, #4*16]         \n" \
    "       ldp x10, x11, [sp, #5*16]       \n" \
    "       ldp x12, x13, [sp, #6*16]       \n" \
    "       ldp x14, x15, [sp, #7*16]       \n" \
    "       ldp x16, x17, [sp, #8*16]       \n" \
    "       ldp x18, x19, [sp, #9*16]       \n" \
    "       ldp x20, x21, [sp, #10*16]      \n" \
    "       ldp x22, x23, [sp, #11*16]      \n" \
    "       ldp x24, x25, [sp, #12*16]      \n" \
    "       ldp x26, x27, [sp, #13*16]      \n" \
    "       ldp x28, x29, [sp, #14*16]      \n" \
    "       ldr x30, [sp, #15*16]           \n" \
    "       ldp x0, x1, [sp], #256          \n"

#define LOAD_SHORT_CONTEXT \
    "       ldp x2, x3, [sp, #1*16]         \n" \
    "       ldp x4, x5, [sp, #2*16]         \n" \
    "       ldp x6, x7, [sp, #3*16]         \n" \
    "       ldp x8, x9, [sp, #4*16]         \n" \
    "       ldp x10, x11, [sp, #5*16]       \n" \
    "       ldp x12, x13, [sp, #6*16]       \n" \
    "       ldp x14, x15, [sp, #7*16]       \n" \
    "       ldp x16, x17, [sp, #8*16]       \n" \
    "       ldp x18, x30, [sp, #9*16]       \n" \
    "       ldp x0, x1, [sp], #176          \n"

#if FULL_CONTEXT
#define SAVE_CONTEXT    SAVE_FULL_CONTEXT
#define LOAD_CONTEXT    LOAD_FULL_CONTEXT
#else
#define SAVE_CONTEXT    SAVE_SHORT_CONTEXT
#define LOAD_CONTEXT    LOAD_SHORT_CONTEXT
#endif


void  __attribute__((used)) __stub_vectors()
{ asm volatile(
"       .section .vectors               \n"
"       .balign 0x800                   \n"
"curr_el_sp0_sync:                      \n" // The exception handler for a synchronous 
        SAVE_CONTEXT                        // exception from the current EL using SP0.
"       mov x0, #0                      \n"
"       mov x1, sp                      \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"curr_el_sp0_irq:                       \n" // The exception handler for an IRQ exception
        SAVE_CONTEXT                        // from the current EL using SP0.
"       mov x0, #0x80                   \n"
"       mov x1, sp                      \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"curr_el_sp0_fiq:                       \n" // The exception handler for an FIQ exception
        SAVE_CONTEXT                        // from the current EL using SP0.
"       mov x0, #0x100                  \n"
"       mov x1, sp                      \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"curr_el_sp0_serror:                    \n" // The exception handler for a System Error 
        SAVE_CONTEXT                        // exception from the current EL using SP0.
"       mov x0, #0x180                  \n"
"       mov x1, sp                      \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"curr_el_spx_sync:                      \n" // The exception handler for a synchrous 
        SAVE_CONTEXT                        // exception from the current EL using the
"       mov x0, #0x200                  \n" // current SP.
"       mov x1, sp                      \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"curr_el_spx_irq:                       \n" // The exception handler for an IRQ exception from 
"       stp x0, x1, [sp, -16]!          \n" // the current EL using the current SP.
"       mrs x0, SPSR_EL1                \n" // Get SPSR
"       orr x0, x0, #0x080              \n" // Disable IRQ interrupt so that we are not disturbed on return
"       msr SPSR_EL1, x0                \n"
"       mrs x1, TPIDRRO_EL0             \n" // Load CPU context
"       ldrb w0, [x1, #%[pint]]         \n" // Get pending interrupt reg
"       orr w0, w0, #0x1                \n" // Set level 6 IRQ
"       strb w0, [x1, #%[pint]]         \n"
"       ldp x0, x1, [sp], #16           \n" // Restore scratch registers
"       eret                            \n"
"                                       \n"
"       .balign 0x80                    \n"
"curr_el_spx_fiq:                       \n" // The exception handler for an FIQ from 
"       stp x0, x1, [sp, -16]!          \n" // the current EL using the current SP.
"       mrs x0, SPSR_EL1                \n" // Get SPSR
"       orr x0, x0, #0x0c0              \n" // Disable IRQ and FIQ interrupts so that we are not disturbed on return
"       msr SPSR_EL1, x0                \n"
"       mrs x1, TPIDRRO_EL0             \n" // Load CPU context
"       ldrb w0, [x1, #%[pint]]         \n" // Get pending interrupt reg
"       orr w0, w0, #0x1                \n" // Set level 6 IRQ
"       strb w0, [x1, #%[pint]]         \n"
"       ldp x0, x1, [sp], #16           \n" // Restore scratch registers
"       eret                            \n"
"                                       \n"
"       .balign 0x80                    \n"
"curr_el_spx_serror:                    \n" // The exception handler for a System Error 
"       stp x0, x1, [sp, -16]!          \n" // exception from the current EL using the
"       mrs x0, SPSR_EL1                \n" // Get SPSR
"       orr x0, x0, #0x1c0              \n" // Disable SError, IRQ and FIQ interrupts so that we are not disturbed on return
"       msr SPSR_EL1, x0                \n"
"       mrs x1, TPIDRRO_EL0             \n" // Load CPU context
"       ldrb w0, [x1, #%[pint]]         \n" // Get pending interrupt reg
"       orr w0, w0, #0x2                \n" // Set level 7 IRQ
"       strb w0, [x1, #%[pint]]         \n"
"       ldp x0, x1, [sp], #16           \n" // Restore scratch registers
"       eret                            \n"
"                                       \n"
"       .balign 0x80                    \n"
"lower_el_aarch64_sync:                 \n" // The exception handler for a synchronous 
        SAVE_CONTEXT                        // exception from a lower EL (AArch64).
"       mov x0, #0x400                  \n"
"       mov x1, sp                      \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"lower_el_aarch64_irq:                  \n" // The exception handler for an IRQ from a lower EL
        SAVE_CONTEXT                        // (AArch64).
"       mov x0, #0x480                  \n"
"       mov x1, sp                      \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"lower_el_aarch64_fiq:                  \n" // The exception handler for an FIQ from a lower EL
        SAVE_CONTEXT                        // (AArch64).
"       mov x0, #0x500                  \n"
"       mov x1, sp                      \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"lower_el_aarch64_serror:               \n" // The exception handler for a System Error 
        SAVE_CONTEXT                        // exception from a lower EL(AArch64).
"       mov x0, #0x580                  \n"
"       mov x1, sp                      \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"lower_el_aarch32_sync:                 \n" // The exception handler for a synchronous 
        SAVE_CONTEXT                        // exception from a lower EL (AArch32).
"       mov x0, #0x600                  \n"
"       mov x1, sp                      \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"lower_el_aarch32_irq:                  \n" // The exception handler for an IRQ from a lower EL
        SAVE_CONTEXT                        // (AArch32).
"       mov x0, #0x680                  \n"
"       mov x1, sp                      \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"lower_el_aarch32_fiq:                  \n" // The exception handler for an FIQ from a lower EL
        SAVE_CONTEXT                        // (AArch32).
"       mov x0, #0x700                  \n"
"       mov x1, sp                      \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"lower_el_aarch32_serror:               \n" // The exception handler for a System Error 
        SAVE_CONTEXT                        // exception from a lower EL(AArch32).
"       mov x0, #0x780                  \n"
"       mov x1, sp                      \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"ExceptionExit:                         \n"
        LOAD_CONTEXT
"       eret                            \n"
"                                       \n"
"       .section .text                  \n"
:
:[pint]"i"(__builtin_offsetof(struct M68KState, INT.ARM))
);}

static int getOPsize(uint32_t opcode)
{
    int size = 0;
    switch (opcode & 0xc0000000)
    {
        case 0x00000000:
            size = 1;
            break;
        case 0x40000000:
            size = 2;
            break;
        case 0x80000000:
            size = 4;
            break;
        case 0xc0000000:
            size = 8;
            break;
    }
    return size;
}

#undef D
#define D(x) /* x */

#ifdef PISTORM

#include "ps_protocol.h"

#include <boards.h>

int board_idx;
struct ExpansionBoard **board;

uint32_t rom_mapped = 0;
uint32_t overlay = 1;
uint32_t z2_ram_autoconf = 1;
uint64_t z2_ram_base = 0;

uint32_t swap_df0_with_dfx = 0;
uint32_t spoof_df0_id = 0;

uint32_t move_slow_to_chip = 0;

// Amiga specific registers
enum
{
    CIAAPRA = 0xBFE001,
    CIABPRB = 0xBFD100,
};

int SYSWriteValToAddr(uint64_t value, int size, uint64_t far)
{
    D(kprintf("[JIT:SYS] SYSWriteValToAddr(0x%x, %d, %p)\n", value, size, far));

    /*
        Allow single wrap around the address space. This provides mirror areas for
        simplified aarch64 pointer arithmetic
    */
    if ((far >> 32) == 1 || (far >> 32) == 0xffffffff) {
        far &= 0xffffffff;
    }

    if (far == 0xdeadbeef && size == 1) {
        kprintf("%c", value);
        return 1;
    }

    if (far >= 0xff000000) {
        kprintf("Z3 write access with far %08x\n", far);
    }

    if (far == CIAAPRA && size == 1) {
        if ((value & 1) != overlay) {
            kprintf("[JIT:SYS] OVL bit changing to %d\n", value & 1);
            overlay = value & 1;
        }
    }

    if (far == CIABPRB) {
        if (swap_df0_with_dfx) {
            const int SEL0_BITNUM = 3;

            if ((value & ((1 << (SEL0_BITNUM + swap_df0_with_dfx)) | 0x80)) == 0x80) {
              // If drive selected but motor off, Amiga is reading drive ID.
              spoof_df0_id = 1;
            } else {
              spoof_df0_id = 0;
            }

            // If the value for SEL0/SELx differ
            if (((value >> SEL0_BITNUM) & 1) != ((value >> (SEL0_BITNUM + swap_df0_with_dfx)) & 1)) {
              // Invert both bits to swap them around
              value ^= ((1 << SEL0_BITNUM) | (1 << (SEL0_BITNUM + swap_df0_with_dfx)));
            }
        }
    }

    if (far >= 0xe80000 && far <= 0xe8ffff && board[board_idx])
    {
        if (board[board_idx]->is_z3)
        {
            if (far == 0xe80044) {
                board[board_idx]->map_base = (value & 0xffff) << 16;
                board[board_idx]->map(board[board_idx]);
                board_idx++;
            }
        }
        else
        {
            if (far == 0xe80048) {
                board[board_idx]->map_base = (value & 0xff) << 16;
                board[board_idx]->map(board[board_idx]);
                board_idx++;
            }
        }
        
        if (far == 0xe8004c || far == 0xe8004e) {
            board_idx++;
        }
 
        return 1;
    }

    if (move_slow_to_chip) {
        if (far >= 0x080000 && far <= 0x0FFFFF) {
            // A500 JP2 connects Agnus' A19 input to A23 instead of A19 by default, and decodes trapdoor memory at 0xC00000 instead of 0x080000.
            // We can move the trapdoor to chipram simply by rewriting the address.
            far += 0xB80000;
        } else if (far >= 0xC00000 && far <= 0xC7FFFF) {
            // Block accesses through to trapdoor at slow ram address, otherwise it will be detected at 0x080000 and 0xC00000.
            return 1;
        }
    }

    switch(size)
    {
        case 1:
            ps_write_8(far, value);
            break;
        case 2:
            ps_write_16(far, value);
            break;
        case 4:
            ps_write_32(far, value);
            break;
        case 8:
            ps_write_32(far, value >> 32);
            ps_write_32(far + 4, value & 0xffffffff);
            break;
    }
    return 1;
}

int SYSReadValFromAddr(uint64_t *value, int size, uint64_t far)
{  
    D(kprintf("[JIT:SYS] SYSReadValFromAddr(%d, %p)\n", size, far));

    uint64_t a, b;

    /*
        Allow single wrap around the address space. This provides mirror areas for
        simplified aarch64 pointer arithmetic
    */
    if ((far >> 32) == 1 || (far >> 32) == 0xffffffff) {
        far &= 0xffffffff;
    }

    if (far > (0x1000000ULL - size)) {
     //   kprintf("Illegal FAR %08x\n", far);
        *value = 0;
    }

    if (far >= 0xe80000 && far <= 0xe8ffff && size == 1)
    {
        while(board[board_idx] && !board[board_idx]->enabled) {
            board_idx++;
        }

        if (board[board_idx])
        {
            
            const uint8_t *rom = board[board_idx]->rom_file;
            *value = rom[far - 0xe80000];

            return 1;
        }
    }

    if (rom_mapped && overlay)
    {
        if (far < 0x80000)
        {
            switch (size)
            {
                case 1:
                    *value = *(uint8_t*)(0xffffff9000e00000 + far);
                    break;
                case 2:
                    *value = *(uint16_t*)(0xffffff9000e00000 + far);
                    break;
                case 4:
                    *value = *(uint32_t*)(0xffffff9000e00000 + far);
                    break;
                case 8:
                    *value = *(uint64_t*)(0xffffff9000e00000 + far);
                    break;
            }

            return 1;
        }
    }

    if (move_slow_to_chip) {
        if (far >= 0x080000 && far <= 0x0FFFFF) {
            // A500 JP2 connects Agnus' A19 input to A23 instead of A19 by default, and decodes trapdoor memory at 0xC00000 instead of 0x080000.
            // We can move the trapdoor to chipram simply by rewriting the address.
            far += 0xB80000;
        } else if (far >= 0xC00000 && far <= 0xC7FFFF) {
            // Block accesses through to trapdoor at slow ram address, otherwise it will be detected at 0x080000 and 0xC00000.
            *value = 0;
            return 1;
        }
    }

    switch(size)
    {
        case 1:
            *value = ps_read_8(far);
            break;
        case 2:
            *value = ps_read_16(far);
            break;
        case 4:
            *value = ps_read_32(far);
            break;
        case 8:
            a = ps_read_32(far);
            b = ps_read_32(far + 4);
            *value = (a << 32) | b;
            break;
    }

    if (far == CIAAPRA) {
        if (swap_df0_with_dfx && spoof_df0_id) {
            // DF0 doesn't emit a drive type ID on RDY pin
            // If swapping DF0 with DF1-3 we need to provide this ID so that DF0 continues to function.
            *value = (*value & 0xDF); // Spoof drive id for swapped DF0 by setting RDY low
        }
    }

    if (far == CIABPRB) {
        if (swap_df0_with_dfx) {
            const int SEL0_BITNUM = 3;

            // SEL0 = 0x80, SEL1 = 0x10, SEL2 = 0x20, SEL3 = 0x40
            // If the value for SEL0/SELx differ
            if (((*value >> SEL0_BITNUM) & 1) != ((*value >> (SEL0_BITNUM + swap_df0_with_dfx)) & 1)) {
              // Invert both bits to swap them around
              *value ^= ((1 << SEL0_BITNUM) | (1 << (SEL0_BITNUM + swap_df0_with_dfx)));
            }
        }
    }

    return 1;
}

#else

int SYSWriteValToAddr(uint64_t value, int size, uint64_t far)
{
    D(kprintf("[JIT:SYS] SYSWriteValToAddr(0x%x, %d, %p)\n", value, size, far));
    
    switch(size)
    {
        case 1:
            *(uint8_t*)(far + 0xffffff9000000000) = value;
            break;
        case 2:
            *(uint16_t*)(far + 0xffffff9000000000) = value;
            break;
        case 4:
            *(uint32_t*)(far + 0xffffff9000000000) = value;
            break;
        case 8:
            *(uint64_t*)(far + 0xffffff9000000000) = value;
            break;
    }

    return 1;
}

int SYSReadValFromAddr(uint64_t *value, int size, uint64_t far)
{
    D(kprintf("[JIT:SYS] SYSReadValFromAddr(%d, %p)\n", size, far));
    
    switch(size)
    {
        case 1:
            *value = *(uint8_t*)(far + 0xffffff9000000000);
            break;
        case 2:
            *value = *(uint16_t*)(far + 0xffffff9000000000);
            break;
        case 4:
            *value = *(uint32_t*)(far + 0xffffff9000000000);
            break;
        case 8:
            *value = *(uint64_t*)(far + 0xffffff9000000000);
            break;
    }

    return 1;
}
#endif

#undef D
#define D(x) /* x  */


int SYSValidateUnit(uint32_t vector, uint64_t *ctx, uint64_t elr, uint64_t spsr, uint64_t esr, uint64_t far)
{
    (void)vector;
    (void)ctx;
    (void)elr;
    (void)spsr;
    (void)esr;
    struct M68KTranslationUnit *unit;
    uint16_t *m68k_pc;
    uintptr_t corrected_far = far | (0xff00000000000000ULL);    // Fix the topmost bits
    uintptr_t unit_far = corrected_far & ~0x0000001000000000;   // Clear the executable region bit

    /* Adjust far of the M68KTranslationUnit */
    unit_far = unit_far - __builtin_offsetof(struct M68KTranslationUnit, mt_ARMCode);

    unit = (void*)unit_far;

    m68k_pc = unit->mt_M68kAddress;

    /* Check the unit. The function will free entry if unit was wrong */
    unit = M68K_VerifyUnit(unit);

    if (unit)
    {
        unit->mt_ARMEntryPoint = (void*)corrected_far;
        elr = corrected_far;
        asm volatile("msr ELR_EL1, %0"::"r"(elr));
        return 1;
    }
    else
    {
        unit = M68K_GetTranslationUnit(m68k_pc);
        
        if (unit)
        {
            // Put simple return function to elr
            asm volatile("msr ELR_EL1, %0"::"r"(unit->mt_ARMEntryPoint));

            // Avoid short loop path by invalidating the "last m68k PC" counter. That should trigger full search and translation
            asm volatile("msr tpidr_el1,%0"::"r"(0xffffffff));
            return 1;
        }
    }

    return 0;
}

uint32_t get_fpn_as_single(int fpn) {
    uint32_t ret = 0;

    switch (fpn) {
        case 0:
            asm volatile("mov %w0, v0.s[0]":"=r"(ret));
            break;

        case 1:
            asm volatile("mov %w0, v1.s[0]":"=r"(ret));
            break;

        case 2:
            asm volatile("mov %w0, v2.s[0]":"=r"(ret));
            break;

        case 3:
            asm volatile("mov %w0, v3.s[0]":"=r"(ret));
            break;

        case 4:
            asm volatile("mov %w0, v4.s[0]":"=r"(ret));
            break;

        case 5:
            asm volatile("mov %w0, v5.s[0]":"=r"(ret));
            break;

        case 6:
            asm volatile("mov %w0, v6.s[0]":"=r"(ret));
            break;

        case 7:
            asm volatile("mov %w0, v7.s[0]":"=r"(ret));
            break;

        case 8:
            asm volatile("mov %w0, v8.s[0]":"=r"(ret));
            break;

        case 9:
            asm volatile("mov %w0, v9.s[0]":"=r"(ret));
            break;

        case 10:
            asm volatile("mov %w0, v10.s[0]":"=r"(ret));
            break;

        case 11:
            asm volatile("mov %w0, v11.s[0]":"=r"(ret));
            break;

        case 12:
            asm volatile("mov %w0, v12.s[0]":"=r"(ret));
            break;

        case 13:
            asm volatile("mov %w0, v13.s[0]":"=r"(ret));
            break;

        case 14:
            asm volatile("mov %w0, v14.s[0]":"=r"(ret));
            break;

        case 15:
            asm volatile("mov %w0, v15.s[0]":"=r"(ret));
            break;

        default:
            break;
    }

    return ret;
}

uint64_t get_fpn_as_double(int fpn) {
    uint64_t ret = 0;

    switch (fpn) {
        case 0:
            asm volatile("mov %0, v0.d[0]":"=r"(ret));
            break;

        case 1:
            asm volatile("mov %0, v1.d[0]":"=r"(ret));
            break;

        case 2:
            asm volatile("mov %0, v2.d[0]":"=r"(ret));
            break;

        case 3:
            asm volatile("mov %0, v3.d[0]":"=r"(ret));
            break;

        case 4:
            asm volatile("mov %0, v4.d[0]":"=r"(ret));
            break;

        case 5:
            asm volatile("mov %0, v5.d[0]":"=r"(ret));
            break;

        case 6:
            asm volatile("mov %0, v6.d[0]":"=r"(ret));
            break;

        case 7:
            asm volatile("mov %0, v7.d[0]":"=r"(ret));
            break;

        case 8:
            asm volatile("mov %0, v8.d[0]":"=r"(ret));
            break;

        case 9:
            asm volatile("mov %0, v9.d[0]":"=r"(ret));
            break;

        case 10:
            asm volatile("mov %0, v10.d[0]":"=r"(ret));
            break;

        case 11:
            asm volatile("mov %0, v11.d[0]":"=r"(ret));
            break;

        case 12:
            asm volatile("mov %0, v12.d[0]":"=r"(ret));
            break;

        case 13:
            asm volatile("mov %0, v13.d[0]":"=r"(ret));
            break;

        case 14:
            asm volatile("mov %0, v14.d[0]":"=r"(ret));
            break;

        case 15:
            asm volatile("mov %0, v15.d[0]":"=r"(ret));
            break;

        default:
            break;
    }

    return ret;
}

void set_fpn_as_single(int fpn, uint32_t value) {

    switch (fpn) {
        case 0:
            asm volatile("mov v0.s[0], %w0"::"r"(value));
            break;

        case 1:
            asm volatile("mov v1.s[0], %w0"::"r"(value));
            break;

        case 2:
            asm volatile("mov v2.s[0], %w0"::"r"(value));
            break;

        case 3:
            asm volatile("mov v3.s[0], %w0"::"r"(value));
            break;

        case 4:
            asm volatile("mov v4.s[0], %w0"::"r"(value));
            break;

        case 5:
            asm volatile("mov v5.s[0], %w0"::"r"(value));
            break;

        case 6:
            asm volatile("mov v6.s[0], %w0"::"r"(value));
            break;

        case 7:
            asm volatile("mov v7.s[0], %w0"::"r"(value));
            break;

        case 8:
            asm volatile("mov v8.s[0], %w0"::"r"(value));
            break;

        case 9:
            asm volatile("mov v9.s[0], %w0"::"r"(value));
            break;

        case 10:
            asm volatile("mov v10.s[0], %w0"::"r"(value));
            break;

        case 11:
            asm volatile("mov v11.s[0], %w0"::"r"(value));
            break;

        case 12:
            asm volatile("mov v12.s[0], %w0"::"r"(value));
            break;

        case 13:
            asm volatile("mov v13.s[0], %w0"::"r"(value));
            break;

        case 14:
            asm volatile("mov v14.s[0], %w0"::"r"(value));
            break;

        case 15:
            asm volatile("mov v15.s[0], %w0"::"r"(value));
            break;

        default:
            break;
    }
}

void set_fpn_as_double(int fpn, uint64_t value) {

    switch (fpn) {
        case 0:
            asm volatile("mov v0.d[0], %0"::"r"(value));
            break;

        case 1:
            asm volatile("mov v1.d[0], %0"::"r"(value));
            break;

        case 2:
            asm volatile("mov v2.d[0], %0"::"r"(value));
            break;

        case 3:
            asm volatile("mov v3.d[0], %0"::"r"(value));
            break;

        case 4:
            asm volatile("mov v4.d[0], %0"::"r"(value));
            break;

        case 5:
            asm volatile("mov v5.d[0], %0"::"r"(value));
            break;

        case 6:
            asm volatile("mov v6.d[0], %0"::"r"(value));
            break;

        case 7:
            asm volatile("mov v7.d[0], %0"::"r"(value));
            break;

        case 8:
            asm volatile("mov v8.d[0], %0"::"r"(value));
            break;

        case 9:
            asm volatile("mov v9.d[0], %0"::"r"(value));
            break;

        case 10:
            asm volatile("mov v10.d[0], %0"::"r"(value));
            break;

        case 11:
            asm volatile("mov v11.d[0], %0"::"r"(value));
            break;

        case 12:
            asm volatile("mov v12.d[0], %0"::"r"(value));
            break;

        case 13:
            asm volatile("mov v13.d[0], %0"::"r"(value));
            break;

        case 14:
            asm volatile("mov v14.d[0], %0"::"r"(value));
            break;

        case 15:
            asm volatile("mov v15.d[0], %0"::"r"(value));
            break;

        default:
            break;
    }
}

int SYSPageFaultHandler(uint32_t vector, uint64_t *ctx, uint64_t elr, uint64_t spsr, uint64_t esr, uint64_t far)
{
    int writeFault = (esr & (1 << 6)) != 0;
    int handled = 0;
    int size = 0;
    uint64_t value = 0;
    uint32_t opcode = LE32(*(uint32_t *)elr);
    (void)vector;
    (void)spsr;

    size = getOPsize(opcode);

    D(kprintf("[JIT:SYS] Fage fault: opcode %08x, %s %p\n", opcode, writeFault ? "write to" : "read from", far));

    if (writeFault)
    {
        /**** MISC ****/
        if ((opcode & 0xffffffe0) == 0xd50b7e20)
        {
            handled = 1;
        }
        /**** Floating point stores ****/
        /* FSTS */
        if ((opcode & 0xfee00c00) == 0xbc000000)
        {
            value = get_fpn_as_single(opcode & 31);
            handled = SYSWriteValToAddr(value, 4, far);
        }
        /* FSTS pre-index */
        else if ((opcode & 0xfee00c00) == 0xbc000400)
        {
            int16_t offset = ((int16_t)(opcode >> 5)) >> 7;

            value = get_fpn_as_single(opcode & 31);

            ctx[(opcode >> 5) & 31] += offset;

            handled = SYSWriteValToAddr(value, 4, far);
        }
        /* FSTS post-index */
        else if ((opcode & 0xfee00c00) == 0xbc000c00)
        {
            int16_t offset = ((int16_t)(opcode >> 5)) >> 7;

            value = get_fpn_as_single(opcode & 31);

            handled = SYSWriteValToAddr(value, 4, far);

            ctx[(opcode >> 5) & 31] += offset;
        }
        /* FSTD */
        else if ((opcode & 0xfee00c00) == 0xfc000000)
        {
            value = get_fpn_as_double(opcode & 31);
            handled = SYSWriteValToAddr(value, 8, far);
        }
        /* FSTD pre-index */
        else if ((opcode & 0xfee00c00) == 0xfc000400)
        {
            int16_t offset = ((int16_t)(opcode >> 5)) >> 7;

            value = get_fpn_as_double(opcode & 31);

            ctx[(opcode >> 5) & 31] += offset;

            handled = SYSWriteValToAddr(value, 8, far);
        }
        /* FSTD post-index */
        else if ((opcode & 0xfee00c00) == 0xbc000c00)
        {
            int16_t offset = ((int16_t)(opcode >> 5)) >> 7;

            value = get_fpn_as_double(opcode & 31);

            handled = SYSWriteValToAddr(value, 8, far);

            ctx[(opcode >> 5) & 31] += offset;
        }
        /**** Integer stores ****/
        /* STUR */
        else if ((opcode & 0x3fe00c00) == 0x38000000)
        {
            if ((opcode & 31) == 31)
                value = 0;
            else
                value = ctx[opcode & 31];

            uint64_t ptr = ctx[(opcode >> 5) & 31];
            int64_t offset = ((int16_t)(opcode >> 5)) >> 7;
            if (ptr + offset != far)
                kprintf("address mismatch in STUR!\n");

            handled = SYSWriteValToAddr(value, size, far);
        }
        /* STR immediate post index */
        else if ((opcode & 0x3fe00c00) == 0x38000400)
        {
            int16_t offset = ((int16_t)(opcode >> 5)) >> 7;

            if ((opcode & 31) == 31)
                value = 0;
            else
                value = ctx[opcode & 31];

            uint64_t ptr = ctx[(opcode >> 5) & 31];
            if (ptr != far)
                kprintf("address mismatch in STR immediate post index far = %08x, reg = %08x, off = %d!\n", far, ptr, offset);

            handled = SYSWriteValToAddr(value, size, far);
            
            ctx[(opcode >> 5) & 31] += offset;
        }
        /* STR immediate pre-index */
        else if ((opcode & 0x3fe00c00) == 0x38000c00)
        {
            int16_t offset = ((int16_t)(opcode >> 5)) >> 7;

            if ((opcode & 31) == 31)
                value = 0;
            else
                value = ctx[opcode & 31];

            uint64_t ptr = ctx[(opcode >> 5) & 31];
            if (ptr + offset != far)
                kprintf("address mismatch in STR immediate post index far = %08x, reg = %08x, off = %d!\n", far, ptr, offset);

            ctx[(opcode >> 5) & 31] += offset;

            handled = SYSWriteValToAddr(value, size, far);
        }
        /* STR unsigned offset */
        else if ((opcode & 0x3fc00000) == 0x39000000)
        {
            if ((opcode & 31) == 31)
                value = 0;
            else
                value = ctx[opcode & 31];

            uint16_t offset = ((opcode >> 10) & 0xfff) * size;
            uint64_t ptr = ctx[(opcode >> 5) & 31];
            if (ptr + offset != far)
                kprintf("address mismatch in STR unsigned offset far = %08x, reg = %08x, off = %d!\n", far, ptr, offset);

            handled = SYSWriteValToAddr(value, size, far);
        }
        /* STR register */
        else if ((opcode & 0x3fe00c00) == 0x38200800)
        {
            if ((opcode & 31) == 31)
                value = 0;
            else
                value = ctx[opcode & 31];

            handled = SYSWriteValToAddr(value, size, far);
        }
        /* ST(L)XR register - no exclusive in this case!!! But m68k bus does not support it anyway */
        else if ((opcode & 0x3fe07c00) == 0x08007c00)
        {
            if ((opcode & 31) == 31)
                value = 0;
            else
                value = ctx[opcode & 31];

            // Mark the store as successful
            ctx[(opcode >> 16) & 31] = 0;

            handled = SYSWriteValToAddr(value, size, far);
        }
        /* STP */
        else if ((opcode & 0x7fc00000) == 0x29000000)
        {
            if (opcode & 0x80000000)
                size = 8;
            else
                size = 4;

            int16_t offset = size * (((int16_t)(opcode >> 6)) >> 9);
            uint64_t ptr = ctx[(opcode >> 5) & 31];
            if (ptr + offset != far)
                kprintf("address mismatch in STP offset far = %08x, reg = %08x, off = %d!\n", far, ptr, offset);

            if ((opcode & 31) == 31)
                value = 0;
            else
                value = ctx[opcode & 31];

            handled = SYSWriteValToAddr(value, size, far);

            if (((opcode >> 10) & 31) == 31)
                value = 0;
            else
                value = ctx[(opcode >> 10) & 31];
            
            handled &= SYSWriteValToAddr(value, size, far + size);
        }
        /* STP post index */
        else if ((opcode & 0x7fc00000) == 0x28800000)
        {
            int16_t offset = ((int16_t)(opcode >> 6)) >> 9;
            if (opcode & 0x80000000)
                size = 8;
            else
                size = 4;

            offset *= size;
     
            uint64_t ptr = ctx[(opcode >> 5) & 31];
            if (ptr != far)
                kprintf("address mismatch in STP post index far = %08x, reg = %08x, off = %d!\n", far, ptr, offset);

            ctx[(opcode >> 5) & 31] += offset;

            if ((opcode & 31) == 31)
                value = 0;
            else
                value = ctx[opcode & 31];

            handled = SYSWriteValToAddr(value, size, far);

            if (((opcode >> 10) & 31) == 31)
                value = 0;
            else
                value = ctx[(opcode >> 10) & 31];
            
            handled &= SYSWriteValToAddr(value, size, far + size);
        }
        /* STP pre index */
        else if ((opcode & 0x7fc00000) == 0x29800000)
        {
            int16_t offset = ((int16_t)(opcode >> 6)) >> 9;
            if (opcode & 0x80000000)
                size = 8;
            else
                size = 4;

            offset *= size;
            
            uint64_t ptr = ctx[(opcode >> 5) & 31];
            if (ptr + offset != far)
                kprintf("address mismatch in STP pre index far = %08x, reg = %08x, off = %d!\n", far, ptr, offset);

            ctx[(opcode >> 5) & 31] += offset;
            
            if ((opcode & 31) == 31)
                value = 0;
            else
                value = ctx[opcode & 31];

            handled = SYSWriteValToAddr(value, size, far);

            if (((opcode >> 10) & 31) == 31)
                value = 0;
            else
                value = ctx[(opcode >> 10) & 31];
            
            handled &= SYSWriteValToAddr(value, size, far + size);
        }
    }
    else
    {
        /**** Floating point loads ****/
        /* FLDS */
        if ((opcode & 0xfee00c00) == 0xbc400000)
        {
            handled = SYSReadValFromAddr(&value, 4, far);
            if (handled)
            {
                set_fpn_as_single(opcode & 31, value);
            }
        }
        /* FLDS pre-index */
        else if ((opcode & 0xfee00c00) == 0xbc400400)
        {
            int16_t offset = ((int16_t)(opcode >> 5)) >> 7;

            handled = SYSReadValFromAddr(&value, 4, far);
            if (handled)
            {
                set_fpn_as_single(opcode & 31, value);
                ctx[(opcode >> 5) & 31] += offset;
            }
        }
        /* FLDS post-index */
        else if ((opcode & 0xfee00c00) == 0xbc400c00)
        {
            int16_t offset = ((int16_t)(opcode >> 5)) >> 7;

            handled = SYSReadValFromAddr(&value, 4, far);
            if (handled)
            {
                set_fpn_as_single(opcode & 31, value);
                ctx[(opcode >> 5) & 31] += offset;
            }
        }
        /* FLDD */
        else if ((opcode & 0xfee00c00) == 0xfc400000)
        {
            handled = SYSReadValFromAddr(&value, 8, far);
            if (handled)
            {
                set_fpn_as_double(opcode & 31, value);
            }
        }
        /* FLDD pre-index */
        else if ((opcode & 0xfee00c00) == 0xfc400400)
        {
            int16_t offset = ((int16_t)(opcode >> 5)) >> 7;

            handled = SYSReadValFromAddr(&value, 8, far);
            if (handled)
            {
                set_fpn_as_double(opcode & 31, value);
                ctx[(opcode >> 5) & 31] += offset;
            }
        }
        /* FLDD post-index */
        else if ((opcode & 0xfee00c00) == 0xbc400c00)
        {
            int16_t offset = ((int16_t)(opcode >> 5)) >> 7;

            handled = SYSReadValFromAddr(&value, 8, far);
            if (handled)
            {
                set_fpn_as_double(opcode & 31, value);
                ctx[(opcode >> 5) & 31] += offset;
            }
        }
        /**** Integer loads ****/
        /* LDP */
        else if ((opcode & 0x7fc00000) == 0x29400000)
        {
            if (opcode & 0x80000000)
                size = 8;
            else
                size = 4;

            int16_t offset = size * (((int16_t)(opcode >> 6)) >> 9);
            uint64_t ptr = ctx[(opcode >> 5) & 31];
            if (ptr + offset != far)
                kprintf("address mismatch in LDP offset far = %08x, reg = %08x, off = %d!\n", far, ptr, offset);
            
            handled = SYSReadValFromAddr(&ctx[opcode & 31], size, far);
            handled &= SYSReadValFromAddr(&ctx[(opcode >> 10) & 31], size, far + size);    
        }
        /* LDP post- and pre-index */
        else if ((opcode & 0x7ec00000) == 0x28c00000)
        {
            int16_t offset = ((int16_t)(opcode >> 6)) >> 9;
            if (opcode & 0x80000000)
                size = 8;
            else
                size = 4;

            offset *= size;

            uint64_t ptr = ctx[(opcode >> 5) & 31];

            if (opcode & 0x01000000) {
                if (ptr + offset != far)
                    kprintf("address mismatch in LDP pre index far = %08x, reg = %08x, off = %d!\n", far, ptr, offset);
            }
            else
            {
                if (ptr != far)
                    kprintf("address mismatch in LDP post index far = %08x, reg = %08x, off = %d!\n", far, ptr, offset);
            }

            handled = SYSReadValFromAddr(&ctx[opcode & 31], size, far);
            handled &= SYSReadValFromAddr(&ctx[(opcode >> 10) & 31], size, far + size);
                
            ctx[(opcode >> 5) & 31] += offset;
        }
        /* LDPSW */
        if ((opcode & 0xffc00000) == 0x69400000)
        {
            size = 4;
            
            int16_t offset = size * (((int16_t)(opcode >> 6)) >> 9);
            uint64_t ptr = ctx[(opcode >> 5) & 31];
            if (ptr + offset != far)
                kprintf("address mismatch in LDPSW offset far = %08x, reg = %08x, off = %d!\n", far, ptr, offset);

            handled = SYSReadValFromAddr(&ctx[opcode & 31], size, far);
            if (handled) {
                if (ctx[opcode & 31] & 0x80000000)
                    ctx[opcode & 31] |= 0xffffffff00000000ULL;
            }
            handled &= SYSReadValFromAddr(&ctx[(opcode >> 10) & 31], size, far + size);
            if (handled) {
                if (ctx[(opcode >> 10) & 31] & 0x80000000)
                    ctx[(opcode >> 10) & 31] |= 0xffffffff00000000ULL;
            }  
        }
        /* LDPSW post index */
        else if ((opcode & 0xffc00000) == 0x68c00000)
        {
            int16_t offset = ((int16_t)(opcode >> 6)) >> 9;
            size = 4;
            offset *= size;
            
            uint64_t ptr = ctx[(opcode >> 5) & 31];
            if (ptr != far)
                kprintf("address mismatch in LDPSW post index far = %08x, reg = %08x, off = %d!\n", far, ptr, offset);

            handled = SYSReadValFromAddr(&ctx[opcode & 31], size, far);
            if (handled) {
                if (ctx[opcode & 31] & 0x80000000)
                    ctx[opcode & 31] |= 0xffffffff00000000ULL;
            }
            handled &= SYSReadValFromAddr(&ctx[(opcode >> 10) & 31], size, far + size);
            if (handled) {
                if (ctx[(opcode >> 10) & 31] & 0x80000000)
                    ctx[(opcode >> 10) & 31] |= 0xffffffff00000000ULL;
            }
            ctx[(opcode >> 5) & 31] += offset;
        }
        /* LDPSW pre index */
        else if ((opcode & 0xffc00000) == 0x69c00000)
        {
            int16_t offset = ((int16_t)(opcode >> 6)) >> 9;
            size = 4;
            offset *= size;

            uint64_t ptr = ctx[(opcode >> 5) & 31];
            if (ptr + offset != far)
                kprintf("address mismatch in LDPSW pre index far = %08x, reg = %08x, off = %d!\n", far, ptr, offset);

            handled = SYSReadValFromAddr(&ctx[opcode & 31], size, far);
            if (handled) {
                if (ctx[opcode & 31] & 0x80000000)
                    ctx[opcode & 31] |= 0xffffffff00000000ULL;
            }
            handled &= SYSReadValFromAddr(&ctx[(opcode >> 10) & 31], size, far + size);
            if (handled) {
                if (ctx[(opcode >> 10) & 31] & 0x80000000)
                    ctx[(opcode >> 10) & 31] |= 0xffffffff00000000ULL;
            }
            ctx[(opcode >> 5) & 31] += offset;
        }
        /* LDR (literal) */
        else if ((opcode & 0x3f000000) == 0x18000000)
        {
            int sext = 0;
            if (opcode & 0x40000000)
                size = 8;
            else
                size = 4;
            if (opcode & 0x80000000)
                sext = 1;
            
            handled = SYSReadValFromAddr(&ctx[opcode & 31], size, far);
            if (handled & sext) {
                if (ctx[opcode & 31] & 0x80000000)
                    ctx[opcode & 31] |= 0xffffffff00000000ULL;
            }
        }
        /* LDR register */
        else if ((opcode & 0x3fe00c00) == 0x38600800)
        {
            handled = SYSReadValFromAddr(&ctx[opcode & 31], size, far);
        }
        /* LDXR register - no exclusive in this case!!! But m68k bus does not support it anyway */
        else if ((opcode & 0x3ffffc00) == 0x085f7c00)
        {
            handled = SYSReadValFromAddr(&ctx[opcode & 31], size, far);
        }
        /* LDR immediate */
        else if ((opcode & 0x3fc00000) == 0x39400000)
        {
            handled = SYSReadValFromAddr(&ctx[opcode & 31], size, far);
        }
        /* LDUR(B/W) */
        else if ((opcode & 0x3fe00c00) == 0x38400000)
        {
            handled = SYSReadValFromAddr(&ctx[opcode & 31], size, far);
        }
        /* LDR immediate, post- and pre-index */
        else if ((opcode & 0x3fe00400) == 0x38400400)
        {
            int16_t offset = ((int16_t)(opcode >> 5)) >> 7;
            
            handled = SYSReadValFromAddr(&ctx[opcode & 31], size, far);
            if (handled)
                ctx[(opcode >> 5) & 31] += offset;
        }
        /* LDRSW/LDRSB/LDRSH register */
        else if ((opcode & 0x3fa00c00) == 0x38a00800)
        {
            int sext64 = 1;
            if (opcode & (1 << 22))
                sext64 = 0;
            
            handled = SYSReadValFromAddr(&ctx[opcode & 31], size, far);
            if (handled) {
                int sext = 0;
                switch (size)
                {
                    case 1:
                        sext = ctx[opcode & 31] & 0x80;
                        if (sext) ctx[opcode & 31] |= 0xffffff00;
                        break;
                    case 2:
                        sext = ctx[opcode & 31] & 0x8000;
                        if (sext) ctx[opcode & 31] |= 0xffff0000;
                        break;
                    case 4:
                        sext = ctx[opcode & 31] & 0x80000000;
                        break;
                }

                if (sext && sext64) {
                    ctx[opcode & 31] |= 0xffffffff00000000ULL;
                }
            }
        }
        /* LDRSW/LDRSB/LDRSH immediate */
        else if ((opcode & 0x3f800000) == 0x39800000)
        {
            int sext64 = 1;
            if (opcode & (1 << 22))
                sext64 = 0;
            
            handled = SYSReadValFromAddr(&ctx[opcode & 31], size, far);
            if (handled) {
                int sext = 0;
                switch (size)
                {
                    case 1:
                        sext = ctx[opcode & 31] & 0x80;
                        if (sext) ctx[opcode & 31] |= 0xffffff00;
                        break;
                    case 2:
                        sext = ctx[opcode & 31] & 0x8000;
                        if (sext) ctx[opcode & 31] |= 0xffff0000;
                        break;
                    case 4:
                        sext = ctx[opcode & 31] & 0x80000000;
                        break;
                }

                if (sext && sext64) {
                    ctx[opcode & 31] |= 0xffffffff00000000ULL;
                }
            }
        }
        /* LDURSW/LDURSB/LDURSH immediate */
        else if ((opcode & 0x3fa00c00) == 0x38800000)
        {
            int sext64 = 1;
            if (opcode & (1 << 22))
                sext64 = 0;
            
            handled = SYSReadValFromAddr(&ctx[opcode & 31], size, far);
            if (handled) {
                int sext = 0;
                switch (size)
                {
                    case 1:
                        sext = ctx[opcode & 31] & 0x80;
                        if (sext) ctx[opcode & 31] |= 0xffffff00;
                        break;
                    case 2:
                        sext = ctx[opcode & 31] & 0x8000;
                        if (sext) ctx[opcode & 31] |= 0xffff0000;
                        break;
                    case 4:
                        sext = ctx[opcode & 31] & 0x80000000;
                        break;
                }

                if (sext && sext64) {
                    ctx[opcode & 31] |= 0xffffffff00000000ULL;
                }
            }
        }
        /* LDRSW/LDRSB/LDRSH post- and pre-index */
        else if ((opcode & 0x3fa00400) == 0x38800400)
        {
            int16_t offset = ((int16_t)(opcode >> 5)) >> 7;
            int sext64 = 1;
            if (opcode & (1 << 22))
                sext64 = 0;
            
            handled = SYSReadValFromAddr(&ctx[opcode & 31], size, far);
            if (handled) {
                int sext = 0;
                switch (size)
                {
                    case 1:
                        sext = ctx[opcode & 31] & 0x80;
                        if (sext) ctx[opcode & 31] |= 0xffffff00;
                        break;
                    case 2:
                        sext = ctx[opcode & 31] & 0x8000;
                        if (sext) ctx[opcode & 31] |= 0xffff0000;
                        break;
                    case 4:
                        sext = ctx[opcode & 31] & 0x80000000;
                        break;
                }

                if (sext && sext64) {
                    ctx[opcode & 31] |= 0xffffffff00000000ULL;
                }

                ctx[(opcode >> 5) & 31] += offset;
            }
        }
    }

    if (!handled)
    {    
        kprintf("[JIT:SYS] Unhandled page fault: opcode %08x, %s %p\n", opcode, writeFault ? "write to" : "read from", far);
    }

    elr += 4;
    asm volatile("msr ELR_EL1, %0"::"r"(elr));

    return handled;
}

#undef D
#define D(x)  x 

void SYSHandler(uint32_t vector, uint64_t *ctx)
{
    int handled = 0;
    uint64_t elr, spsr, esr, far;
    asm volatile("mrs %0, ELR_EL1; mrs %1, SPSR_EL1":"=r"(elr),"=r"(spsr));
    asm volatile("mrs %0, ESR_EL1":"=r"(esr));
    asm volatile("mrs %0, FAR_EL1":"=r"(far));

    if ((vector & 0x1ff) == 0x00 && (esr & 0xf8000000) == 0x90000000)
    {
        handled = SYSPageFaultHandler(vector, ctx, elr, spsr, esr, far);
    }
    else if ((vector & 0x1ff) == 0x00 && (esr & 0xf8000000) == 0x80000000)
    {
        if ((far >> 56) == 0xaa)
        {
            handled = SYSValidateUnit(vector, ctx, elr, spsr, esr, far);
        }
    }
    else if ((esr & ~0xffff) == 0x56000000)
    {
        handled = 1;

        if ((esr & 0xffff) == 0x100)
        {
            kprintf("[JIT:SYS] AArch64 RegDump:\n");
            for (int i=0; i < 8; i++)
            {
                kprintf("[JIT:SYS]   X%02d=%p   X%02d=%p   X%02d=%p   X%02d=%p\n", 
                    4*i, ctx[4*i],
                    4*i+1, ctx[4*i+1],
                    4*i+2, ctx[4*i+2],
                    4*i+3, ctx[4*i+3]);
            }
        }

        if ((esr & 0xffff) == 0x101)
        {
            uint64_t sr;

            asm volatile("mrs %0, tpidr_el0":"=r"(sr));

            kprintf("[JIT:SYS] M68k RegDump:\n[JIT] ");
            int reg_dn[] = {REG_D0, REG_D1, REG_D2, REG_D3, REG_D4, REG_D5, REG_D6, REG_D7};
            int reg_an[] = {REG_A0, REG_A1, REG_A2, REG_A3, REG_A4, REG_A5, REG_A6, REG_A7};

            for (int i=0; i < 8; i++) {
                if (i==4)
                    kprintf("\n[JIT] ");
                kprintf("    D%d = 0x%08x", i, BE32(ctx[reg_dn[i]]));
            }
            kprintf("\n[JIT] ");

            for (int i=0; i < 8; i++) {
                if (i==4)
                    kprintf("\n[JIT] ");
                kprintf("    A%d = 0x%08x", i, BE32(ctx[reg_an[i]]));
            }
            kprintf("\n[JIT] ");

            kprintf("    PC = 0x%08x    SR = ", BE32(ctx[REG_PC]));

            kprintf("T%d|", sr >> 14);
    
            if (sr & SR_S)
                kprintf("S");
            else
                kprintf(".");
    
            if (sr & SR_M)
                kprintf("M|");
            else
                kprintf(".|");

            kprintf("IPM%d|", (sr >> 8) & 7);

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
            
            kprintf("\n");

#if EMU68_PC_REG_HISTORY         
            uint32_t pc_0, pc_1, pc_2, pc_3;

            asm volatile("mov %w0, v28.s[0]; mov %w1, v28.s[1]; mov %w2, v28.s[2]; mov %w3, v28.s[3]"
                :"=r"(pc_0), "=r"(pc_1), "=r"(pc_2), "=r"(pc_3));

            kprintf("[JIT]     PC history: %08x, %08x, %08x, %08x\n", pc_0, pc_1, pc_2, pc_3);
            kprintf("[JIT]     Stack:");

            uint32_t *sp = (uint32_t *)(uintptr_t)BE32(ctx[REG_A7]);
            for (int i=-4; i < 8; i++) {
                kprintf(" %s%08x", i == 0 ? "*":"", sp[i]);
            }
            kprintf("\n");
#endif
        }

        if ((esr & 0xffff) == 0x102)
        {
            uint8_t *from = (uint8_t*)(intptr_t)(*(uint32_t *)elr);
            uint32_t len = *(uint32_t *)(elr + 4);

            kprintf("[SYS:JIT] RAM dump from 0x%08x, len 0x%x\n[SYS:JIT]   ", from, len);

            while (len) {
                if ((len & 15) == 0)
                    kprintf("\n[SYS:JIT]   ");
                kprintf("%02x ", *from++);
                len--;
            }

            kprintf("\n");

            elr += 8;
            asm volatile("msr ELR_EL1, %0"::"r"(elr));
        }

        if ((esr & 0xffff) == 0x103)
        {
            uint16_t *from = (uint16_t*)(intptr_t)(*(uint32_t *)elr);
            uint32_t len = (*(uint32_t *)(elr + 4)) / 2;

            kprintf("[SYS:JIT] RAM dump from 0x%08x, len 0x%x\n[SYS:JIT]   ", from, len);

            while (len) {
                if ((len & 7) == 0)
                    kprintf("\n[SYS:JIT]   ");
                kprintf("%04x ", *from++);
                len--;
            }

            kprintf("\n");

            elr += 8;
            asm volatile("msr ELR_EL1, %0"::"r"(elr));
        }
    }

    if (!handled)
    {
        kprintf("[JIT:SYS] Exception with vector %04x. ELR=%p, SPSR=%08x, ESR=%p, FAR=%p\n", vector, elr, spsr, esr, far);
        kprintf("[JIT:SYS] Failed instruction: %08x\n", LE32(*(uint32_t*)elr));

        for (int i=0; i < 16; i++)
        {
            kprintf("[JIT:SYS]  X%02d=%p   X%02d=%p\n", 2*i, ctx[2*i], 2*i+1, ctx[2*i+1]);
        }
        
        while(1) { asm volatile("wfe"); };
    }
}
