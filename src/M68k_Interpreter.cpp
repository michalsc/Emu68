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

#define M68K_EA_DA 0x8000
#define M68K_EA_REG 0x7000
#define M68K_EA_WL 0x0800
#define M68K_EA_SCALE 0x0600
#define M68K_EA_FULL 0x0100
#define M68K_EA_OFF8 0x00FF

#define M68K_EA_BS 0x0080
#define M68K_EA_IS 0x0040
#define M68K_EA_BD_SIZE 0x0030
#define M68K_EA_IIS 0x0007

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

void INTERPRET_LoadFrom_EA_Mod0(uint8_t src_reg, uint8_t, uint8_t size, void *out)
{
    uint32_t dn = getDn(src_reg);

    switch (size) {
        case 4:
            *(uint32_t *)out = dn;
            return;
        case 2:
            *(uint16_t *)out = dn;
            return;
        case 1:
            *(uint8_t *)out = dn;
            return;
    }

    __builtin_unreachable();
}

void INTERPRET_LoadFrom_EA_Mod1(uint8_t src_reg, uint8_t, uint8_t size, void *out)
{
    uint32_t an = getAn(src_reg);

    switch (size) {
        case 4:
            *(uint32_t *)out = an;
            return;
        case 2:
            *(uint16_t *)out = an;
            return;
    }

    __builtin_unreachable();
}

void INTERPRET_LoadFrom_EA_Mod2(uint8_t src_reg, uint8_t, uint8_t size, void *out)
{
    uint32_t addr = getAn(src_reg);

    switch (size) {
        case 4:
            *(uint32_t *)out = *(uint32_t *)(uintptr_t)addr;
            return;
        case 2:
            *(uint16_t *)out = *(uint16_t *)(uintptr_t)addr;
            return;
        case 1:
            *(uint8_t *)out = *(uint8_t *)(uintptr_t)addr;
            return;
    }

    __builtin_unreachable();
}

void INTERPRET_LoadFrom_EA_Mod3(uint8_t src_reg, uint8_t, uint8_t size, void *out)
{
    uint32_t addr = getAn(src_reg);

    switch (size) {
        case 4:
            *(uint32_t *)out = *(uint32_t *)(uintptr_t)addr;
            setAn(src_reg, addr + 4);
            return;
        case 2:
            *(uint16_t *)out = *(uint16_t *)(uintptr_t)addr;
            setAn(src_reg, addr + 2);
            return;
        case 1:
            *(uint8_t *)out = *(uint8_t *)(uintptr_t)addr;
            setAn(src_reg, addr + (src_reg == 7) ? 2 : 1);
            return;
    }

    __builtin_unreachable();
}

void INTERPRET_LoadFrom_EA_Mod4(uint8_t src_reg, uint8_t, uint8_t size, void *out)
{
    uint32_t addr = getAn(src_reg);
    switch (size) {
        case 4:
            addr -= 4;
            setAn(src_reg, addr);
            *(uint32_t *)out = *(uint32_t *)(uintptr_t)addr;
            return;
        case 2:
            addr -= 2;
            setAn(src_reg, addr);
            *(uint16_t *)out = *(uint16_t *)(uintptr_t)addr;
            return;
        case 1:
            addr -= (src_reg == 7) ? 2 : 1;
            setAn(src_reg, addr);
            *(uint8_t *)out = *(uint8_t *)(uintptr_t)addr;
            return;
    }

    __builtin_unreachable();
}

uint32_t INTERPRET_LoadFrom_EA_Mod5(void * pc, uint8_t size, uint8_t ea, struct M68KState *state, void *out)
{
    uint8_t src_reg = ea & 7;
    uint32_t addr = state->A[src_reg].u32 + *(int16_t *)pc;
    uint32_t extra_bytes = 2;

    switch (size) {
        case 4:
            *(uint32_t *)out = *(uint32_t *)(uintptr_t)addr;
            break;
        case 2:
            *(uint16_t *)out = *(uint16_t *)(uintptr_t)addr;
            break;
        case 1:
            *(uint8_t *)out = *(uint8_t *)(uintptr_t)addr;
            break;
        default:
            kprintf("[INT] INTERPRET_LoadFromEffectiveAddress: Invalid size %d\n", size);
            break;
    }
    
    return extra_bytes;
}

uint32_t INTERPRET_LoadFrom_EA_EXT(void * pc, uint8_t size, uint32_t an, struct M68KState *state, void *out)
{
    uint16_t ext_word = *(uint16_t *)pc;
    uint32_t full = ext_word & M68K_EA_FULL;
    uint32_t extra_bytes = 2;
    uint8_t scale = 1 << ((ext_word & M68K_EA_SCALE) >> 9);
    uint8_t index_reg = (ext_word & M68K_EA_REG) >> 12;
    uint8_t index_size = (ext_word & M68K_EA_WL) ? 4 : 2;
    uint16_t index_is_addr = (ext_word & M68K_EA_DA);

#if 0

#endif

    if (full) /* Full format */
    {
        
    }
    else /* Brief format */
    {
        int8_t displacement = ext_word & M68K_EA_OFF8;
        uint32_t index_value = index_is_addr ? state->A[index_reg].u32 : state->D[index_reg].u32;

        if (index_size == 2) {
            index_value = (int16_t)index_value;
        }

        an += displacement;
        an += index_value * scale;

        switch (size) {
            case 4:
                *(uint32_t *)out = *(uint32_t *)(uintptr_t)an;
                break;
            case 2:
                *(uint16_t *)out = *(uint16_t *)(uintptr_t)an;
                break;
            case 1:
                *(uint8_t *)out = *(uint8_t *)(uintptr_t)an;
                break;
            default:
                kprintf("[INT] INTERPRET_LoadFromEffectiveAddress: Invalid size %d\n", size);
                break;
        }
    }

    return extra_bytes;
}

uint32_t INTERPRET_LoadFrom_EA_Mod6(void * pc, uint8_t size, uint8_t ea, struct M68KState *state, void *out)
{
    uint8_t src_reg = ea & 7;
    uint32_t an = state->A[src_reg].u32;

    return INTERPRET_LoadFrom_EA_EXT(pc, size, an, state, out);
}

uint32_t INTERPRET_LoadFrom_EA_Mod7(void * pc, uint8_t size, uint8_t ea, struct M68KState *state, void *out)
{
    uint8_t src_reg = ea & 7;
    uint32_t extra_bytes = 0;

    if (src_reg == 0)
    {
        uintptr_t addr = (uint32_t)*(int16_t *)pc;
        extra_bytes = 2;
        switch (size) {
            case 4:
                *(uint32_t *)out = *(uint32_t *)addr;
                break;
            case 2:
                *(uint16_t *)out = *(uint16_t *)addr;
                break;
            case 1:
                *(uint8_t *)out = *(uint8_t *)addr;
                break;
            default:
                kprintf("[INT] INTERPRET_LoadFromEffectiveAddress: Invalid size %d\n", size);
                break;
        }
    }
    else if (src_reg == 1)
    {
        uintptr_t addr = *(uint32_t *)pc;
        extra_bytes = 4;
        switch (size) {
            case 4:
                *(uint32_t *)out = *(uint32_t *)addr;
                break;
            case 2:
                *(uint16_t *)out = *(uint16_t *)addr;
                break;
            case 1:
                *(uint8_t *)out = *(uint8_t *)addr;
                break;
            default:
                kprintf("[INT] INTERPRET_LoadFromEffectiveAddress: Invalid size %d\n", size);
                break;
        }
    }
    else if (src_reg == 2)
    {
        int16_t off = *(int16_t *)pc;
        uint32_t addr = (uint32_t)(uintptr_t)pc + off;
        extra_bytes = 2;
        switch (size) {
            case 4:
                *(uint32_t *)out = *(uint32_t *)(uintptr_t)addr;
                break;
            case 2:
                *(uint16_t *)out = *(uint16_t *)(uintptr_t)addr;
                break;
            case 1:
                *(uint8_t *)out = *(uint8_t *)(uintptr_t)addr;
                break;
            default:
                kprintf("[INT] INTERPRET_LoadFromEffectiveAddress: Invalid size %d\n", size);
                break;
        }
    }
    else if (src_reg == 3)
    {
        uint32_t an = (uint32_t)(uintptr_t)pc;
        return INTERPRET_LoadFrom_EA_EXT(pc, size, an, state, out);
    }
    else if (src_reg == 4)
    {
        switch (size) {
            case 4:
                *(uint32_t *)out = *(uint32_t *)pc;
                extra_bytes = 4;
                break;
            case 2:
                *(uint16_t *)out = *(uint16_t *)pc;
                extra_bytes = 2;
                break;
            case 1:
                *(uint8_t *)out = *(uint16_t *)pc;
                extra_bytes = 2;
                break;
            default:
                kprintf("[INT] INTERPRET_LoadFromEffectiveAddress: Invalid size %d\n", size);
                break;
        }
    }

    return extra_bytes;
}

void INTERPRET_LoadFromEffectiveAddress(uint8_t src_reg, uint8_t mode, uint8_t size, void *out)
{
    switch(mode) {
        case 0: INTERPRET_LoadFrom_EA_Mod0(src_reg, mode, size, out); return;
        case 1: INTERPRET_LoadFrom_EA_Mod1(src_reg, mode, size, out); return;
        case 2: INTERPRET_LoadFrom_EA_Mod2(src_reg, mode, size, out); return;
        case 3: INTERPRET_LoadFrom_EA_Mod3(src_reg, mode, size, out); return;
        case 4: INTERPRET_LoadFrom_EA_Mod4(src_reg, mode, size, out); return;
    }

    __builtin_unreachable();
}
