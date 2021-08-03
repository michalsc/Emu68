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
    "       str x30, [sp, #15*16]           \n"

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


void __stub_vectors()
{ asm(
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
:[pint]"i"(__builtin_offsetof(struct M68KState, PINT))
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

int SYSWriteValToAddr(uint64_t value, int size, uint64_t far)
{
    D(kprintf("[JIT:SYS] SYSWriteValToAddr(0x%x, %d, %p)\n", value, size, far));

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
        if ((opcode & 0x3fe00c00) == 0x38000000)
        {
            if ((opcode & 31) == 31)
                value = 0;
            else
                value = ctx[opcode & 31];

            handled = SYSWriteValToAddr(value, size, far);
        }
        else if ((opcode & 0x3fe00c00) == 0x38000400)
        {
            int16_t offset = ((int16_t)(opcode >> 5)) >> 7;

            if ((opcode & 31) == 31)
                value = 0;
            else
                value = ctx[opcode & 31];

            ctx[(opcode >> 5) & 31] += offset;

            handled = SYSWriteValToAddr(value, size, far);
        }
        else if ((opcode & 0x3fe00c00) == 0x38000c00)
        {
            int16_t offset = ((int16_t)(opcode >> 5)) >> 7;

            if ((opcode & 31) == 31)
                value = 0;
            else
                value = ctx[opcode & 31];

            ctx[(opcode >> 5) & 31] += offset;

            handled = SYSWriteValToAddr(value, size, far);
        }
        else if ((opcode & 0x3fc00000) == 0x39000000)
        {
            if ((opcode & 31) == 31)
                value = 0;
            else
                value = ctx[opcode & 31];

            handled = SYSWriteValToAddr(value, size, far);
        }
        else if ((opcode & 0x3fe00c00) == 0x38200800)
        {
            if ((opcode & 31) == 31)
                value = 0;
            else
                value = ctx[opcode & 31];

            handled = SYSWriteValToAddr(value, size, far);
        }
        /* STP */
        else if ((opcode & 0x7fc00000) == 0x29000000)
        {
            if (opcode & 0x80000000)
                size = 8;
            else
                size = 4;
            
            if ((opcode & 31) == 31)
                value = 0;
            else
                value = ctx[opcode & 31];

            handled = SYSWriteValToAddr(value, size, far);

            if (((opcode >> 10) & 31) == 31)
                value = 0;
            else
                value = ctx[(opcode >> 10) & 31];
            
            handled = SYSWriteValToAddr(value, size, far + size);
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
            
            handled = SYSWriteValToAddr(value, size, far + size);
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
            
            handled = SYSWriteValToAddr(value, size, far + size);
        }
    }
    else
    {
        /* LDP */
        if ((opcode & 0x7fc00000) == 0x29400000)
        {
            if (opcode & 0x80000000)
                size = 8;
            else
                size = 4;
            
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
            
            handled = SYSReadValFromAddr(&ctx[opcode & 31], size, far);
            handled &= SYSReadValFromAddr(&ctx[(opcode >> 10) & 31], size, far + size);
                
            ctx[(opcode >> 5) & 31] += offset;
        }
        /* LDPSW */
        if ((opcode & 0xffc00000) == 0x69400000)
        {
            size = 4;
            
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

    if (!handled)
    {
        kprintf("[JIT:SYS] Exception with vector %04x. ELR=%p, SPSR=%08x, ESR=%p, FAR=%p\n", vector, elr, spsr, esr, far);

        for (int i=0; i < 16; i++)
        {
            kprintf("[JIT:SYS]  X%02d=%p   X%02d=%p\n", 2*i, ctx[2*i], 2*i+1, ctx[2*i+1]);
        }
        
        while(1) { asm volatile("wfe"); };
    }
}
