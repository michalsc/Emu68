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

void __stub_vectors()
{ asm(
"       .section .vectors               \n"
"       .balign 0x800                   \n"
"curr_el_sp0_sync:                      \n" // The exception handler for a synchronous 
"       stp x0, x1, [sp, -176]!         \n" // exception from the current EL using SP0.
"       stp x2, x3, [sp, #1*16]         \n"
"       stp x4, x5, [sp, #2*16]         \n"
"       stp x6, x7, [sp, #3*16]         \n"
"       stp x8, x9, [sp, #4*16]         \n"
"       stp x10, x11, [sp, #5*16]       \n"
"       stp x12, x13, [sp, #6*16]       \n"
"       stp x14, x15, [sp, #7*16]       \n"
"       stp x16, x17, [sp, #8*16]       \n"
"       stp x18, x30, [sp, #9*16]       \n"
"       mov x0, #0                      \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"curr_el_sp0_irq:                       \n" // The exception handler for an IRQ exception
"       stp x0, x1, [sp, -176]!         \n" // from the current EL using SP0.
"       stp x2, x3, [sp, #1*16]         \n"
"       stp x4, x5, [sp, #2*16]         \n"
"       stp x6, x7, [sp, #3*16]         \n"
"       stp x8, x9, [sp, #4*16]         \n"
"       stp x10, x11, [sp, #5*16]       \n"
"       stp x12, x13, [sp, #6*16]       \n"
"       stp x14, x15, [sp, #7*16]       \n"
"       stp x16, x17, [sp, #8*16]       \n"
"       stp x18, x30, [sp, #9*16]       \n"
"       mov x0, #0x80                   \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"curr_el_sp0_fiq:                       \n" // The exception handler for an FIQ exception
"       stp x0, x1, [sp, -176]!         \n" // from the current EL using SP0.
"       stp x2, x3, [sp, #1*16]         \n"
"       stp x4, x5, [sp, #2*16]         \n"
"       stp x6, x7, [sp, #3*16]         \n"
"       stp x8, x9, [sp, #4*16]         \n"
"       stp x10, x11, [sp, #5*16]       \n"
"       stp x12, x13, [sp, #6*16]       \n"
"       stp x14, x15, [sp, #7*16]       \n"
"       stp x16, x17, [sp, #8*16]       \n"
"       stp x18, x30, [sp, #9*16]       \n"
"       mov x0, #0x100                  \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"curr_el_sp0_serror:                    \n" // The exception handler for a System Error 
"       stp x0, x1, [sp, -176]!         \n" // exception from the current EL using SP0.
"       stp x2, x3, [sp, #1*16]         \n"
"       stp x4, x5, [sp, #2*16]         \n"
"       stp x6, x7, [sp, #3*16]         \n"
"       stp x8, x9, [sp, #4*16]         \n"
"       stp x10, x11, [sp, #5*16]       \n"
"       stp x12, x13, [sp, #6*16]       \n"
"       stp x14, x15, [sp, #7*16]       \n"
"       stp x16, x17, [sp, #8*16]       \n"
"       stp x18, x30, [sp, #9*16]       \n"
"       mov x0, #0x180                  \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"curr_el_spx_sync:                      \n" // The exception handler for a synchrous 
"       stp x0, x1, [sp, -176]!         \n" // exception from the current EL using the
"       stp x2, x3, [sp, #1*16]         \n" // current SP.
"       stp x4, x5, [sp, #2*16]         \n"
"       stp x6, x7, [sp, #3*16]         \n"
"       stp x8, x9, [sp, #4*16]         \n"
"       stp x10, x11, [sp, #5*16]       \n"
"       stp x12, x13, [sp, #6*16]       \n"
"       stp x14, x15, [sp, #7*16]       \n"
"       stp x16, x17, [sp, #8*16]       \n"
"       stp x18, x30, [sp, #9*16]       \n"
"       mov x0, #0x200                  \n"
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
"       ldr w0, [x1, #%[pint]]          \n" // Get pending interrupt reg
"       orr w0, w0, #0x10               \n" // Set level 4 IRQ
"       str w0, [x1, #%[pint]]          \n"
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
"       ldr w0, [x1, #%[pint]]          \n" // Get pending interrupt reg
"       orr w0, w0, #0x20               \n" // Set level 5 IRQ
"       str w0, [x1, #%[pint]]          \n"
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
"       ldr w0, [x1, #%[pint]]          \n" // Get pending interrupt reg
"       orr w0, w0, #0x40               \n" // Set level 6 IRQ
"       str w0, [x1, #%[pint]]          \n"
"       ldp x0, x1, [sp], #16           \n" // Restore scratch registers
"       eret                            \n"
"                                       \n"
"       .balign 0x80                    \n"
"lower_el_aarch64_sync:                 \n" // The exception handler for a synchronous 
"       stp x0, x1, [sp, -176]!         \n" // exception from a lower EL (AArch64).
"       stp x2, x3, [sp, #1*16]         \n"
"       stp x4, x5, [sp, #2*16]         \n"
"       stp x6, x7, [sp, #3*16]         \n"
"       stp x8, x9, [sp, #4*16]         \n"
"       stp x10, x11, [sp, #5*16]       \n"
"       stp x12, x13, [sp, #6*16]       \n"
"       stp x14, x15, [sp, #7*16]       \n"
"       stp x16, x17, [sp, #8*16]       \n"
"       stp x18, x30, [sp, #9*16]       \n"
"       mov x0, #0x400                  \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"lower_el_aarch64_irq:                  \n" // The exception handler for an IRQ from a lower EL
"       stp x0, x1, [sp, -176]!         \n" // (AArch64).
"       stp x2, x3, [sp, #1*16]         \n"
"       stp x4, x5, [sp, #2*16]         \n"
"       stp x6, x7, [sp, #3*16]         \n"
"       stp x8, x9, [sp, #4*16]         \n"
"       stp x10, x11, [sp, #5*16]       \n"
"       stp x12, x13, [sp, #6*16]       \n"
"       stp x14, x15, [sp, #7*16]       \n"
"       stp x16, x17, [sp, #8*16]       \n"
"       stp x18, x30, [sp, #9*16]       \n"
"       mov x0, #0x480                  \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"lower_el_aarch64_fiq:                  \n" // The exception handler for an FIQ from a lower EL
"       stp x0, x1, [sp, -176]!         \n" // (AArch64).
"       stp x2, x3, [sp, #1*16]         \n"
"       stp x4, x5, [sp, #2*16]         \n"
"       stp x6, x7, [sp, #3*16]         \n"
"       stp x8, x9, [sp, #4*16]         \n"
"       stp x10, x11, [sp, #5*16]       \n"
"       stp x12, x13, [sp, #6*16]       \n"
"       stp x14, x15, [sp, #7*16]       \n"
"       stp x16, x17, [sp, #8*16]       \n"
"       stp x18, x30, [sp, #9*16]       \n"
"       mov x0, #0x500                  \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"lower_el_aarch64_serror:               \n" // The exception handler for a System Error 
"       stp x0, x1, [sp, -176]!         \n" // exception from a lower EL(AArch64).
"       stp x2, x3, [sp, #1*16]         \n"
"       stp x4, x5, [sp, #2*16]         \n"
"       stp x6, x7, [sp, #3*16]         \n"
"       stp x8, x9, [sp, #4*16]         \n"
"       stp x10, x11, [sp, #5*16]       \n"
"       stp x12, x13, [sp, #6*16]       \n"
"       stp x14, x15, [sp, #7*16]       \n"
"       stp x16, x17, [sp, #8*16]       \n"
"       stp x18, x30, [sp, #9*16]       \n"
"       mov x0, #0x580                  \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"lower_el_aarch32_sync:                 \n" // The exception handler for a synchronous 
"       stp x0, x1, [sp, -176]!         \n" // exception from a lower EL (AArch32).
"       stp x2, x3, [sp, #1*16]         \n"
"       stp x4, x5, [sp, #2*16]         \n"
"       stp x6, x7, [sp, #3*16]         \n"
"       stp x8, x9, [sp, #4*16]         \n"
"       stp x10, x11, [sp, #5*16]       \n"
"       stp x12, x13, [sp, #6*16]       \n"
"       stp x14, x15, [sp, #7*16]       \n"
"       stp x16, x17, [sp, #8*16]       \n"
"       stp x18, x30, [sp, #9*16]       \n"
"       mov x0, #0x600                  \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"lower_el_aarch32_irq:                  \n" // The exception handler for an IRQ from a lower EL
"       stp x0, x1, [sp, -176]!         \n" // (AArch32).
"       stp x2, x3, [sp, #1*16]         \n"
"       stp x4, x5, [sp, #2*16]         \n"
"       stp x6, x7, [sp, #3*16]         \n"
"       stp x8, x9, [sp, #4*16]         \n"
"       stp x10, x11, [sp, #5*16]       \n"
"       stp x12, x13, [sp, #6*16]       \n"
"       stp x14, x15, [sp, #7*16]       \n"
"       stp x16, x17, [sp, #8*16]       \n"
"       stp x18, x30, [sp, #9*16]       \n"
"       mov x0, #0x680                  \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"lower_el_aarch32_fiq:                  \n" // The exception handler for an FIQ from a lower EL
"       stp x0, x1, [sp, -176]!         \n" // (AArch32).
"       stp x2, x3, [sp, #1*16]         \n"
"       stp x4, x5, [sp, #2*16]         \n"
"       stp x6, x7, [sp, #3*16]         \n"
"       stp x8, x9, [sp, #4*16]         \n"
"       stp x10, x11, [sp, #5*16]       \n"
"       stp x12, x13, [sp, #6*16]       \n"
"       stp x14, x15, [sp, #7*16]       \n"
"       stp x16, x17, [sp, #8*16]       \n"
"       stp x18, x30, [sp, #9*16]       \n"
"       mov x0, #0x700                  \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"       .balign 0x80                    \n"
"lower_el_aarch32_serror:               \n" // The exception handler for a System Error 
"       stp x0, x1, [sp, -176]!         \n" // exception from a lower EL(AArch32).
"       stp x2, x3, [sp, #1*16]         \n"
"       stp x4, x5, [sp, #2*16]         \n"
"       stp x6, x7, [sp, #3*16]         \n"
"       stp x8, x9, [sp, #4*16]         \n"
"       stp x10, x11, [sp, #5*16]       \n"
"       stp x12, x13, [sp, #6*16]       \n"
"       stp x14, x15, [sp, #7*16]       \n"
"       stp x16, x17, [sp, #8*16]       \n"
"       stp x18, x30, [sp, #9*16]       \n"
"       mov x0, #0x780                  \n"
"       bl SYSHandler                   \n"
"       b ExceptionExit                 \n"
"                                       \n"
"ExceptionExit:                         \n"
"       ldp x2, x3, [sp, #1*16]         \n"
"       ldp x4, x5, [sp, #2*16]         \n"
"       ldp x6, x7, [sp, #3*16]         \n"
"       ldp x8, x9, [sp, #4*16]         \n"
"       ldp x10, x11, [sp, #5*16]       \n"
"       ldp x12, x13, [sp, #6*16]       \n"
"       ldp x14, x15, [sp, #7*16]       \n"
"       ldp x16, x17, [sp, #8*16]       \n"
"       ldp x18, x30, [sp, #9*16]       \n"
"       ldp x0, x1, [sp], #176          \n"
"       eret                            \n"
"                                       \n"
"       .section .text                  \n"
:
:[pint]"i"(__builtin_offsetof(struct M68KState, PINT))
);}

void SYSHandler(uint32_t vector)
{
    uint64_t elr, spsr, esr, far;
    asm volatile("mrs %0, ELR_EL1; mrs %1, SPSR_EL1":"=r"(elr),"=r"(spsr));
    asm volatile("mrs %0, ESR_EL1":"=r"(esr));
    asm volatile("mrs %0, FAR_EL1":"=r"(far));
    kprintf("[JIT:SYS] Exception with vector %04x. ELR=%p, SPSR=%08x, ESR=%p, FAR=%p\n", vector, elr, spsr, esr, far);
    while(1);
}
