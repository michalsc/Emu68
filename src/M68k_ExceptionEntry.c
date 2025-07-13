#include <stdint.h>
#include <A64.h>
#include <M68k.h>
#include <support.h>

static inline struct M68KState *getCTX()
{
    struct M68KState *ctx;
    __asm__ volatile("mov %0, "CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}

/*
    type_and_format: 16 bit value. Bits 0..11 - exception type, bits 15..12 - exception format
*/
uint32_t M68K_Exception(uint32_t M68k_SR, uint32_t type_and_format, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t ea, uint32_t fault)
{
    register uint32_t M68k_PC __asm__("w18");
    register void *M68k_A7 __asm__("x29");

    /* If we are not in supervisor mode, swap stacks */
    if ((M68k_SR & SR_S) == 0) {
        /* Store current A7 to USP */
        __asm__ volatile("mov v31.s[1], %w0": :"r"(M68k_A7));
        /* If SR_M is set, load MSP into A7, otherwise load ISP */
        if (unlikely(M68k_SR & SR_M)) {
            __asm__ volatile("mov %w0, v31.s[3]":"=r"(M68k_A7));
        }
        else {
            __asm__ volatile("mov %w0, v31.s[2]":"=r"(M68k_A7));
        }
    }

    /* Depending on exception format prepare place on stack frame and eventually push extra parameters */
    if (unlikely((type_and_format & 0xf000) == 0x2000 || (type_and_format & 0xf000) == 0x3000))
    {
        // format 2 and format 3 - store EA
        __asm__ volatile("str %w1, [%0, #-4]!":"=r"(M68k_A7):"r"(ea),"0"(M68k_A7));
    }
    else if (unlikely((type_and_format & 0xf000) == 0x4000))
    {
        // format 4 - store fault address and EA
        __asm__ volatile("str %w1, [%0, #-8]!\t\nstr %w2, [%0, #4]":"=r"(M68k_A7):"r"(fault),"r"(ea),"0"(M68k_A7));
    }

    uint32_t tmp = M68k_SR;
    if ((tmp & 3) != 0 && (tmp & 3) < 3)
    tmp ^= 3;

    __asm__ volatile(
        "strh %w1, [%0, #-8]!   \n\t"
        "str  %w2, [%0, #2]     \n\t"
        "strh %w3, [%0, #6]"
    :"=r"(M68k_A7)
    :"r"(tmp),
     "r"(M68k_PC),
     "r"(type_and_format),
     "0"(M68k_A7)
    );

    /* Clear trace flags, set supervisor */
    M68k_SR |= SR_S;
    M68k_SR &= ~(SR_T0 | SR_T1);

    uint32_t vbr = getCTX()->VBR + (type_and_format & 0x0fff);
    M68k_PC = *(uint32_t *)(uintptr_t)vbr;

    __asm__ volatile("": :"r"(M68k_PC));

    return M68k_SR;
}
