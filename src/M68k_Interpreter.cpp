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


extern "C" {

    void M68K_LoadContext(struct M68KState *ctx);
    void M68K_SaveContext(struct M68KState *ctx);
    void M68K_PrintContext(struct M68KState *ctx);


}


namespace Emu68::M68k::Interpreter {

static inline struct M68KState *getCTX()
{
    struct M68KState *ctx;
    __asm__ volatile("mov %0, " CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}

void Exception_F0(uint32_t exception)
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

void UNIMPLEMENTED(uint32_t opcode)
{
    M68K_SaveContext(getCTX());
    kprintf("[INT] opcode %04x at %08x not implemented\n", opcode, PC);
    M68K_PrintContext(getCTX());
    M68K_LoadContext(getCTX());

    Exception_F0(VECTOR_ILLEGAL_INSTRUCTION);
}

void LoadFrom_EA_Mod0(uint8_t src_reg, uint8_t size, void *out)
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

void LoadFrom_EA_Mod1(uint8_t src_reg, uint8_t size, void *out)
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

void LoadFrom_EA_Mod2(uint8_t src_reg, uint8_t size, void *out)
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

void LoadFrom_EA_Mod3(uint8_t src_reg, uint8_t size, void *out)
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

void LoadFrom_EA_Mod4(uint8_t src_reg, uint8_t size, void *out)
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

void LoadFrom_EA_Mod5(uint8_t src_reg, uint8_t size, void *out)
{
    uint32_t addr = getAn(src_reg) + *(int16_t *)(uintptr_t)PC;
    PC += 2;

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

void LoadFrom_EA_EXT(uint32_t An, uint8_t size, void *out)
{
    uint16_t ext_word = *(uint16_t *)(uintptr_t)PC;
    uint8_t scale = 1 << ((ext_word & M68K_EA_SCALE) >> 9);
    uint8_t index_reg = (ext_word & M68K_EA_REG) >> 12;

    PC += 2;
    
    if (ext_word & M68K_EA_FULL)
    {
        uint32_t base_displacement = 0;
        uint32_t outer_displacement = 0;
        uint32_t index_value = 0;

        if (ext_word & M68K_EA_BS) {
            An = 0;
        }

        switch(ext_word & M68K_EA_BD_SIZE) {
            case M68K_EA_BD_SIZE_WORD:
                base_displacement = *(int16_t*)(uintptr_t)PC;
                PC += 2;
                break;
            case M68K_EA_BD_SIZE_LONG:
                base_displacement = *(uint32_t*)(uintptr_t)PC;
                PC += 4;
                break;
            default:
                break;
        }

        if ((ext_word & M68K_EA_IS) == 0) {
            index_value = (ext_word & M68K_EA_DA) ? getAn(index_reg) : getDn(index_reg);

            if ((ext_word & M68K_EA_WL) == 0) {
                index_value = (int16_t)index_value;
            }

            index_value *= scale;
        }

        switch(ext_word & 3) {
            case 2:
                outer_displacement = *(int16_t*)(uintptr_t)PC;
                PC += 2;
                break;
            case 3:
                outer_displacement = *(uint32_t*)(uintptr_t)PC;
                PC += 4;
                break;
            default:
                break;
        }

        uint32_t indirect_address;

        if ((ext_word & 3) == 0) {
            // No memory indirect action (000), or reserved (100, don't-care)
            indirect_address = An + base_displacement + index_value;
        }
        else if ((ext_word & 4) == 0) {
            // Indirect preindexed: index folded in before the dereference
            indirect_address = *(uint32_t *)(uintptr_t)(An + base_displacement + index_value);
        }
        else {
            // Indirect postindexed: index added after the dereference
            indirect_address = *(uint32_t *)(uintptr_t)(An + base_displacement) + index_value;
        }

        indirect_address += outer_displacement;

        switch (size) {
            case 4:
                *(uint32_t *)out = *(uint32_t *)(uintptr_t)indirect_address;
                break;
            case 2:
                *(uint16_t *)out = *(uint16_t *)(uintptr_t)indirect_address;
                break;
            case 1:
                *(uint8_t *)out = *(uint8_t *)(uintptr_t)indirect_address;
                break;
            default:
                __builtin_unreachable();
        }
    }
    else
    {
        int8_t displacement = ext_word & M68K_EA_OFF8;
        uint32_t index_value = (ext_word & M68K_EA_DA) ? getAn(index_reg) : getDn(index_reg);

        if ((ext_word & M68K_EA_WL) == 0) {
            index_value = (int16_t)index_value;
        }

        An += displacement;
        An += index_value * scale;

        switch (size) {
            case 4:
                *(uint32_t *)out = *(uint32_t *)(uintptr_t)An;
                break;
            case 2:
                *(uint16_t *)out = *(uint16_t *)(uintptr_t)An;
                break;
            case 1:
                *(uint8_t *)out = *(uint8_t *)(uintptr_t)An;
                break;
            default:
                __builtin_unreachable();
        }
    }
}

void LoadFrom_EA_Mod6(uint8_t src_reg, uint8_t size, void *out)
{
    LoadFrom_EA_EXT(getAn(src_reg), size, out);
}

void LoadFrom_EA_Mod7(uint8_t src_reg, uint8_t size, void *out)
{
    if (src_reg == 0)
    {
        uintptr_t addr = (uint32_t)*(int16_t *)(uintptr_t)PC;
        PC += 2;
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
                __builtin_unreachable();
        }
    }
    else if (src_reg == 1)
    {
        uintptr_t addr = *(uint32_t *)(uintptr_t)PC;
        PC += 4;
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
                __builtin_unreachable();
        }
    }
    else if (src_reg == 2)
    {
        int16_t off = *(int16_t *)(uintptr_t)PC;
        uint32_t addr = (uint32_t)(uintptr_t)PC + off;
        PC += 2;
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
                __builtin_unreachable();
        }
    }
    else if (src_reg == 3)
    {
        LoadFrom_EA_EXT(PC, size, out);
    }
    else if (src_reg == 4)
    {
        switch (size) {
            case 4:
                *(uint32_t *)out = *(uint32_t *)(uintptr_t)PC;
                PC += 4;
                break;
            case 2:
                *(uint16_t *)out = *(uint16_t *)(uintptr_t)PC;
                PC += 2;
                break;
            case 1:
                *(uint8_t *)out = *(uint16_t *)(uintptr_t)PC;
                PC += 2;
                break;
            default:
                __builtin_unreachable();
        }
    }
    else
    {
        __builtin_unreachable();
    }
}

void LoadFromEffectiveAddress(uint8_t src_reg, uint8_t size, void *out, uint8_t mode)
{
    switch(mode) {
        case 0: [[unlikely]] LoadFrom_EA_Mod0(src_reg, size, out); return;
        case 1: [[unlikely]] LoadFrom_EA_Mod1(src_reg, size, out); return;
        case 2: [[unlikely]] LoadFrom_EA_Mod2(src_reg, size, out); return;
        case 3: [[unlikely]] LoadFrom_EA_Mod3(src_reg, size, out); return;
        case 4: [[unlikely]] LoadFrom_EA_Mod4(src_reg, size, out); return;
        case 5: LoadFrom_EA_Mod5(src_reg, size, out); return;
        case 6: LoadFrom_EA_Mod6(src_reg, size, out); return;
        case 7: LoadFrom_EA_Mod7(src_reg, size, out); return;
    }

    __builtin_unreachable();
}

void Get_EA_Mod2(uint8_t src_reg, uint8_t, uint32_t *out)
{
    *out = getAn(src_reg);
}

void Get_EA_Mod3(uint8_t src_reg, uint8_t size, uint32_t *out)
{
    uint32_t addr = getAn(src_reg);
    
    *out = addr;
    addr += size;
    if (src_reg == 7 && size == 1) addr++;

    setAn(src_reg, addr);
}

void Get_EA_Mod4(uint8_t src_reg, uint8_t size, uint32_t *out)
{
    uint32_t addr = getAn(src_reg);

    addr -= size;
    if (src_reg == 7 && size == 1) addr--;
    *out = addr;

    setAn(src_reg, addr);
}

void Get_EA_Mod5(uint8_t src_reg, uint8_t, uint32_t *out)
{
    *out = getAn(src_reg) + *(int16_t *)(uintptr_t)PC;
    PC += 2;
}

void GetExtendedEffectiveAddress(uint32_t An, uint8_t, uint32_t *out)
{
    uint16_t ext_word = *(uint16_t *)(uintptr_t)PC;
    uint8_t scale = 1 << ((ext_word & M68K_EA_SCALE) >> 9);
    uint8_t index_reg = (ext_word & M68K_EA_REG) >> 12;

    PC += 2;
    
    if (ext_word & M68K_EA_FULL)
    {
        uint32_t base_displacement = 0;
        uint32_t outer_displacement = 0;
        uint32_t index_value = 0;

        if (ext_word & M68K_EA_BS) {
            An = 0;
        }

        switch(ext_word & M68K_EA_BD_SIZE) {
            case M68K_EA_BD_SIZE_WORD:
                base_displacement = *(int16_t*)(uintptr_t)PC;
                PC += 2;
                break;
            case M68K_EA_BD_SIZE_LONG:
                base_displacement = *(uint32_t*)(uintptr_t)PC;
                PC += 4;
                break;
            default:
                break;
        }

        if ((ext_word & M68K_EA_IS) == 0) {
            index_value = (ext_word & M68K_EA_DA) ? getAn(index_reg) : getDn(index_reg);

            if ((ext_word & M68K_EA_WL) == 0) {
                index_value = (int16_t)index_value;
            }

            index_value *= scale;
        }

        switch(ext_word & 3) {
            case 2:
                outer_displacement = *(int16_t*)(uintptr_t)PC;
                PC += 2;
                break;
            case 3:
                outer_displacement = *(uint32_t*)(uintptr_t)PC;
                PC += 4;
                break;
            default:
                break;
        }

        uint32_t indirect_address;
        
        if ((ext_word & 3) == 0) {
            // No memory indirect action (000), or reserved (100, don't-care)
            indirect_address = An + base_displacement + index_value;
        }
        else if ((ext_word & 4) == 0) {
            // Indirect preindexed: index folded in before the dereference
            indirect_address = *(uint32_t *)(uintptr_t)(An + base_displacement + index_value);
        }
        else {
            // Indirect postindexed: index added after the dereference
            indirect_address = *(uint32_t *)(uintptr_t)(An + base_displacement) + index_value;
        }

        indirect_address += outer_displacement;

        *out = indirect_address;
    }
    else
    {
        int8_t displacement = ext_word & M68K_EA_OFF8;
        uint32_t index_value = (ext_word & M68K_EA_DA) ? getAn(index_reg) : getDn(index_reg);

        if ((ext_word & M68K_EA_WL) == 0) {
            index_value = (int16_t)index_value;
        }

        An += displacement;
        An += index_value * scale;

        *out = An;
    }
}

void Get_EA_Mod6(uint8_t src_reg, uint8_t size, uint32_t *out)
{
    GetExtendedEffectiveAddress(getAn(src_reg), size, out);
}

void Get_EA_Mod7(uint8_t src_reg, uint8_t size, uint32_t *out)
{
    if (src_reg == 0)
    {
        *out = (uint32_t)*(int16_t *)(uintptr_t)PC;
        PC += 2;
    }
    else if (src_reg == 1)
    {
        *out = *(uint32_t *)(uintptr_t)PC;
        PC += 4;
    }
    else if (src_reg == 2)
    {
        int16_t off = *(int16_t *)(uintptr_t)PC;
        *out = (uint32_t)(uintptr_t)PC + off;
        PC += 2;
    }
    else if (src_reg == 3)
    {
        GetExtendedEffectiveAddress(PC, size, out);
    }
    else
    {
        __builtin_unreachable();
    }
}

void GetEffectiveAddress(uint8_t src_reg, uint8_t size, uint32_t *out, uint8_t mode)
{
    switch(mode) {
        // mode 0 and 1 cannot return EA address at all
        case 2: [[unlikely]] Get_EA_Mod2(src_reg, size, out); return;
        case 3: [[unlikely]] Get_EA_Mod3(src_reg, size, out); return;
        case 4: [[unlikely]] Get_EA_Mod4(src_reg, size, out); return;
        case 5: Get_EA_Mod5(src_reg, size, out); return;
        case 6: Get_EA_Mod6(src_reg, size, out); return;
        case 7: Get_EA_Mod7(src_reg, size, out); return;
    }
}

void StoreTo_EA_Mod0(uint8_t dst_reg, uint32_t value, uint8_t size)
{
    switch (size) {
        case 4:
            setDn(dst_reg, value);
            return;
        case 2:
            setDn(dst_reg, (getDn(dst_reg) & 0xffff0000) | (value & 0x0000ffff));
            return;
        case 1:
            setDn(dst_reg, (getDn(dst_reg) & 0xffffff00) | (value & 0x000000ff));
            return;
    }

    __builtin_unreachable();
}

void StoreTo_EA_Mod1(uint8_t dst_reg, uint32_t value, uint8_t size)
{
    switch (size) {
        case 4:
            setAn(dst_reg, value);
            return;
        case 2:
            setAn(dst_reg, (int16_t)value);
            return;
    }

    __builtin_unreachable();
}

void StoreTo_EA_Mod2(uint8_t dst_reg, uint32_t value, uint8_t size)
{
    uintptr_t addr = getAn(dst_reg);

    switch (size) {
        case 4:
            *(uint32_t *)addr = value;
            return;
        case 2:
            *(uint16_t *)addr = value;
            return;
        case 1:
            *(uint8_t *)addr = value;
            return;
    }

    __builtin_unreachable();
}

void StoreTo_EA_Mod3(uint8_t dst_reg, uint32_t value, uint8_t size)
{
    uintptr_t addr = getAn(dst_reg);

    switch (size) {
        case 4:
            *(uint32_t *)addr = value;
            setAn(dst_reg, addr + 4);
            return;
        case 2:
            *(uint16_t *)addr = value;
            setAn(dst_reg, addr + 2);
            return;
        case 1:
            *(uint8_t *)addr = value;
            setAn(dst_reg, addr + (dst_reg == 7) ? 2 : 1);
            return;
    }

    __builtin_unreachable();
}

void StoreTo_EA_Mod4(uint8_t dst_reg, uint32_t value, uint8_t size)
{
    uint32_t addr = getAn(dst_reg);

    switch (size) {
        case 4:
            addr -= 4;
            setAn(dst_reg, addr);
            *(uint32_t *)(uintptr_t)addr = value;
            return;
        case 2:
            addr -= 2;
            setAn(dst_reg, addr);
            *(uint16_t *)(uintptr_t)addr = value;
            return;
        case 1:
            addr -= (dst_reg == 7) ? 2 : 1;
            setAn(dst_reg, addr);
            *(uint8_t *)(uintptr_t)addr = value;
            return;
    }

    __builtin_unreachable();
}

void StoreTo_EA_Mod5(uint8_t dst_reg, uint32_t value, uint8_t size)
{
    uint32_t addr = getAn(dst_reg) + *(int16_t *)(uintptr_t)PC;
    PC += 2;

    switch (size) {
        case 4:
            *(uint32_t *)(uintptr_t)addr = value;
            return;
        case 2:
            *(uint16_t *)(uintptr_t)addr = value;
            return;
        case 1:
            *(uint8_t *)(uintptr_t)addr = value;
            return;
    }
    
    __builtin_unreachable();
}

void StoreTo_EA_Mod6(uint8_t dst_reg, uint32_t value, uint8_t size)
{
    uint16_t ext_word = *(uint16_t *)(uintptr_t)PC;
    uint8_t scale = 1 << ((ext_word & M68K_EA_SCALE) >> 9);
    uint8_t index_reg = (ext_word & M68K_EA_REG) >> 12;
    uint32_t An = 0;

    PC += 2;
    
    if (ext_word & M68K_EA_FULL)
    {
        uint32_t base_displacement = 0;
        uint32_t outer_displacement = 0;
        uint32_t index_value = 0;

        if ((ext_word & M68K_EA_BS) == 0) {
            An = getAn(dst_reg);
        }

        switch(ext_word & M68K_EA_BD_SIZE) {
            case M68K_EA_BD_SIZE_WORD:
                base_displacement = *(int16_t*)(uintptr_t)PC;
                PC += 2;
                break;
            case M68K_EA_BD_SIZE_LONG:
                base_displacement = *(uint32_t*)(uintptr_t)PC;
                PC += 4;
                break;
            default:
                break;
        }

        if ((ext_word & M68K_EA_IS) == 0) {
            index_value = (ext_word & M68K_EA_DA) ? getAn(index_reg) : getDn(index_reg);

            if ((ext_word & M68K_EA_WL) == 0) {
                index_value = (int16_t)index_value;
            }

            index_value *= scale;
        }

        switch(ext_word & 3) {
            case 2:
                outer_displacement = *(int16_t*)(uintptr_t)PC;
                PC += 2;
                break;
            case 3:
                outer_displacement = *(uint32_t*)(uintptr_t)PC;
                PC += 4;
                break;
            default:
                break;
        }

        uint32_t indirect_address;
        
        if ((ext_word & 3) == 0) {
            // No memory indirect action (000), or reserved (100, don't-care)
            indirect_address = An + base_displacement + index_value;
        }
        else if ((ext_word & 4) == 0) {
            // Indirect preindexed: index folded in before the dereference
            indirect_address = *(uint32_t *)(uintptr_t)(An + base_displacement + index_value);
        }
        else {
            // Indirect postindexed: index added after the dereference
            indirect_address = *(uint32_t *)(uintptr_t)(An + base_displacement) + index_value;
        }

        indirect_address += outer_displacement;

        switch (size) {
            case 4:
                *(uint32_t *)(uintptr_t)indirect_address = value;
                break;
            case 2:
                *(uint16_t *)(uintptr_t)indirect_address = value;
                break;
            case 1:
                *(uint8_t *)(uintptr_t)indirect_address = value;
                break;
            default:
                __builtin_unreachable();
        }
    }
    else
    {
        int8_t displacement = ext_word & M68K_EA_OFF8;
        uint32_t index_value = (ext_word & M68K_EA_DA) ? getAn(index_reg) : getDn(index_reg);
        
        An = getAn(dst_reg);

        if ((ext_word & M68K_EA_WL) == 0) {
            index_value = (int16_t)index_value;
        }

        An += displacement;
        An += index_value * scale;

        switch (size) {
            case 4:
                *(uint32_t *)(uintptr_t)An = value;
                break;
            case 2:
                *(uint16_t *)(uintptr_t)An = value;
                break;
            case 1:
                *(uint8_t *)(uintptr_t)An = value;
                break;
            default:
                __builtin_unreachable();
        }
    }
}

void StoreTo_EA_Mod7(uint8_t dst_reg, uint32_t value, uint8_t size)
{
    if (dst_reg == 0)
    {
        uintptr_t addr = (uint32_t)*(int16_t *)(uintptr_t)PC;
        PC += 2;
        switch (size) {
            case 4:
                *(uint32_t *)addr = value;
                break;
            case 2:
                *(uint16_t *)addr = value;
                break;
            case 1:
                *(uint8_t *)addr = value;
                break;
            default:
                __builtin_unreachable();
        }
    }
    else if (dst_reg == 1)
    {
        uintptr_t addr = *(uint32_t *)(uintptr_t)PC;
        PC += 4;
        switch (size) {
            case 4:
                *(uint32_t *)addr = value;
                break;
            case 2:
                *(uint16_t *)addr = value;
                break;
            case 1:
                *(uint8_t *)addr = value;
                break;
            default:
                __builtin_unreachable();
        }
    }
    else
    {
        __builtin_unreachable();
    }
}

void StoreToEffectiveAddress(uint8_t dst_reg, uint32_t value, uint8_t size, uint8_t mode)
{
    switch(mode) {
        case 0: [[unlikely]] StoreTo_EA_Mod0(dst_reg, value, size); return;
        case 1: [[unlikely]] StoreTo_EA_Mod1(dst_reg, value, size); return;
        case 2: [[unlikely]] StoreTo_EA_Mod2(dst_reg, value, size); return;
        case 3: [[unlikely]] StoreTo_EA_Mod3(dst_reg, value, size); return;
        case 4: [[unlikely]] StoreTo_EA_Mod4(dst_reg, value, size); return;
        case 5: StoreTo_EA_Mod5(dst_reg, value, size); return;
        case 6: StoreTo_EA_Mod6(dst_reg, value, size); return;
        case 7: StoreTo_EA_Mod7(dst_reg, value, size); return;
    }

    __builtin_unreachable();
}

void HandleChangedSR(uint32_t sr, uint32_t changed)
{
    /* Check if M flag has changed its value */
    if (changed & SR_M) {
        /* M is set, store A7 to ISP and move MSP to A7 */
        if (sr & SR_M) {
            setISP(A7);
            A7 = getMSP();
        } else {
            setMSP(A7);
            A7 = getISP();
        }
    }

    /*
        Check if S was cleared. If this is the case, move A7 to ISP or MSP and
        load USP into A7
    */
    if (changed & SR_S) {
        if ((sr & SR_S) == 0) {
            if (sr & SR_M) {
                setMSP(A7);
            } else {
                setISP(A7);
            }
            A7 = getUSP();
        }
    }

    /* Check if IPL was altered */
    if (changed & SR_IPL) {
        /* IPL higher than 6? Disable ARM interrupts, otherwise enable them */
        if ((sr & SR_IPL) > 0x0600) {
            asm volatile("msr DAIFSet, 7":::"memory");
        } else {
            asm volatile("msr DAIFClr, 7":::"memory");
        }
    }
}

} // Emu68::M68k::Interpreter
