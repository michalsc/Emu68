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

uint32_t rom_mapped = 0;
uint32_t overlay = 1;
uint32_t z2_ram_autoconf = 1;
uint64_t z2_ram_base = 0;

int SYSWriteValToAddr(uint64_t value, int size, uint64_t far)
{
    D(kprintf("[JIT:SYS] SYSWriteValToAddr(0x%x, %d, %p)\n", value, size, far));

    if (size > 1 && (far & 1))
        kprintf("UNALIGNED WORD/LONG write to %08x\n");

    if (far > (0x1000000ULL - size)) {
        kprintf("Illegal FAR %08x\n", far);
        return 1;
    }

    if (far == 0xBFE001 && size == 1) {
        if ((value & 1) != overlay) {
            kprintf("[JIT:SYS] OVL bit changing to %d\n", value & 1);
            overlay = value & 1;
        }
    }

    if (far >= 0xe80000 && far <= 0xe8ffff && z2_ram_autoconf)
    {
        if (far == 0xe8004a)
            z2_ram_base = (value & 0xf0) << 12;
        else if (far == 0xe80048) {
            z2_ram_base |= (value & 0xf0) << 16;
            kprintf("[JIT:SYS] Z2 RAM autoconfigured for address 0x%08x\n", z2_ram_base);
            mmu_map(z2_ram_base, z2_ram_base, 8 << 20, MMU_ACCESS | MMU_ISHARE | MMU_ALLOW_EL0 | MMU_ATTR(0), 0);
            z2_ram_autoconf = 0;
        }
        else if (far == 0xe8004c || far == 0xe8004e) {
            z2_ram_autoconf = 0;
        }
 
        return 1;
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

#define MANUFACTURER_ID 0x6d73

uint8_t z2_autoconf[] = {
    0xc | 0x2,  // Z2 board, link to memory list
    0x0,        // Size 8 MB
    0x1, 0x0,   // Product ID
    0x8, 0x0,   // ERT MEMSPACE - want to be in 8MB Z2 region
    0x0, 0x0,   // Reserved - must be 0
    (MANUFACTURER_ID >> 12) & 15, (MANUFACTURER_ID >> 8) & 15, (MANUFACTURER_ID >> 4) & 15, MANUFACTURER_ID & 15, // Manufacturer ID
    0xc, 0xa, 0xf, 0xe, 0xb, 0xa, 0xb, 0xe, // Serial number
    0x0, 0x0, 0x0, 0x0, // Diag area missing
};

int SYSReadValFromAddr(uint64_t *value, int size, uint64_t far)
{  
    D(kprintf("[JIT:SYS] SYSReadValFromAddr(%d, %p)\n", size, far));

    uint64_t a, b;

    if (size > 1 && (far & 1))
        kprintf("UNALIGNED WORD/LONG read from %08x\n", far);

    if (far > (0x1000000ULL - size)) {
     //   kprintf("Illegal FAR %08x\n", far);
        *value = 0;
    }

    if (far >= 0xe80000 && far <= 0xe8ffff && size == 1)
    {       
        if (z2_ram_autoconf) {
            if (far & 1)
                *value = 0xff;
            else
            {
                uint64_t off = far - 0xe80000;
                off >>= 1;
                
                if (off < sizeof(z2_autoconf))
                    *value = z2_autoconf[off] << 4;

                if (off > 1)
                    *value ^= 0xff;                
            }

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
                    *value = *(uint8_t*)(0xffffff9000f80000 + far);
                    break;
                case 2:
                    *value = *(uint16_t*)(0xffffff9000f80000 + far);
                    break;
                case 4:
                    *value = *(uint32_t*)(0xffffff9000f80000 + far);
                    break;
                case 8:
                    *value = *(uint64_t*)(0xffffff9000f80000 + far);
                    break;
            }

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
        /* STUR */
        if ((opcode & 0x3fe00c00) == 0x38000000)
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
        /* LDP */
        if ((opcode & 0x7fc00000) == 0x29400000)
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

    if ((esr & ~0xffff) == 0x56000000)
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
