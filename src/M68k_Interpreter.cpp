#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter.hpp>

#include "RegisterMapping.h"

#define _REGLOCK_H
extern "C" {
    #include "M68k.h"
    #include "support.h"
}

static inline struct M68KState *getCTX()
{
    struct M68KState *ctx;
    __asm__ volatile("mov %0, " CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}

void INTERPRET_Exception_F0(uint32_t exception)
{
    /* Get the SR register as it is now */
    uint32_t origSR = SR;

    /* If we were in user mode, switch stack and reserve space for exception frame */
    if (likely(origSR & SR_S) == 0) {
        USP = A7;
        if (unlikely(origSR & SR_M)) {
            A7 = MSP - 8;
        }
        else {
            A7 = ISP - 8;
        }
    } else {
        A7 -= 8;
    }

    /* Push the exception frame */
    void *sp = (void *)(uintptr_t)A7;

    /* Invert V and C flags */
    uint32_t tmp = origSR;
    uint32_t tmp2;

    asm volatile("rbit %0, %1":"=r"(tmp2):"r"(tmp));
    tmp = (tmp & ~3) | ((tmp2 >> 30) & 3);

    /* Prepare frame */
    *(uint16_t *)(uintptr_t)sp = tmp;
    *(uint32_t *)((uintptr_t)sp + 2) = PC;
    *(uint16_t *)((uintptr_t)sp + 6) = exception;

    /* Set SR to supervisor and clear trace flags */
    origSR |= SR_S;
    origSR &= ~(SR_T0 | SR_T1);

    /* Set the new SR */
    SR = origSR;

    /* Get the vector address */
    uint32_t vbr = getCTX()->VBR + (exception & 0x0fff);
    PC = *(uint32_t *)(uintptr_t)vbr;
}

extern "C" void INTERPRET_UNIMPLEMENTED(uint32_t opcode)
{
    void M68K_LoadContext(struct M68KState *ctx);
    void M68K_SaveContext(struct M68KState *ctx);
    void M68K_PrintContext(struct M68KState *ctx);

    M68K_SaveContext(getCTX());
    kprintf("[INT] opcode %04x at %08x not implemented\n", opcode, PC);
    M68K_PrintContext(getCTX());
    M68K_LoadContext(getCTX());

    INTERPRET_Exception_F0(VECTOR_ILLEGAL_INSTRUCTION);
}
