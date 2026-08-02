#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter.hpp>

#include "RegisterMapping.h"

#define _REGLOCK_H
extern "C" {
    #include "M68k.h"
    #include "support.h"
    #include "disasm.h"
}


extern "C" {

    void M68K_LoadContext(struct M68KState *ctx);
    void M68K_SaveContext(struct M68KState *ctx);
    void M68K_PrintContext(struct M68KState *ctx);

}

template<class Type>
Type getDn(int reg)
{
    if constexpr (std::is_same<Type, uint8_t>::value || std::is_same<Type, int8_t>::value) {
        switch (reg) {
            case 0: return D0 & 0xff; case 1: return D1 & 0xff;
            case 2: return D2 & 0xff; case 3: return D3 & 0xff;
            case 4: return D4 & 0xff; case 5: return D5 & 0xff;
            case 6: return D6 & 0xff; case 7: return D7 & 0xff;
        }
    } else if constexpr (std::is_same<Type, uint16_t>::value || std::is_same<Type, int16_t>::value) {
        switch (reg) {
            case 0: return D0 & 0xffff; case 1: return D1 & 0xffff;
            case 2: return D2 & 0xffff; case 3: return D3 & 0xffff;
            case 4: return D4 & 0xffff; case 5: return D5 & 0xffff;
            case 6: return D6 & 0xffff; case 7: return D7 & 0xffff;
        }
    } else if constexpr (std::is_same<Type, uint32_t>::value || std::is_same<Type, int32_t>::value) {
        switch (reg) {
            case 0: return D0; case 1: return D1;
            case 2: return D2; case 3: return D3;
            case 4: return D4; case 5: return D5;
            case 6: return D6; case 7: return D7;
        }
    }
    __builtin_unreachable();
}

template BYTE getDn<BYTE>(int);
template WORD getDn<WORD>(int);
template LONG getDn<LONG>(int);
template UBYTE getDn<UBYTE>(int);
template UWORD getDn<UWORD>(int);
template ULONG getDn<ULONG>(int);

template<class Type>
void setDn(int reg, Type val)
{
    if constexpr (std::is_same<Type, uint8_t>::value || std::is_same<Type, int8_t>::value) {
        switch (reg) {
            case 0: D0 = (D0 & 0xffffff00) | (val & 0xff); break; case 1: D1 = (D1 & 0xffffff00) | (val & 0xff); break;
            case 2: D2 = (D2 & 0xffffff00) | (val & 0xff); break; case 3: D3 = (D3 & 0xffffff00) | (val & 0xff); break;
            case 4: D4 = (D4 & 0xffffff00) | (val & 0xff); break; case 5: D5 = (D5 & 0xffffff00) | (val & 0xff); break;
            case 6: D6 = (D6 & 0xffffff00) | (val & 0xff); break; case 7: D7 = (D7 & 0xffffff00) | (val & 0xff); break;
        }
    } else if constexpr (std::is_same<Type, uint16_t>::value || std::is_same<Type, int16_t>::value) {
        switch (reg) {
            case 0: D0 = (D0 & 0xffff0000) | (val & 0xffff); break; case 1: D1 = (D1 & 0xffff0000) | (val & 0xffff); break;
            case 2: D2 = (D2 & 0xffff0000) | (val & 0xffff); break; case 3: D3 = (D3 & 0xffff0000) | (val & 0xffff); break;
            case 4: D4 = (D4 & 0xffff0000) | (val & 0xffff); break; case 5: D5 = (D5 & 0xffff0000) | (val & 0xffff); break;
            case 6: D6 = (D6 & 0xffff0000) | (val & 0xffff); break; case 7: D7 = (D7 & 0xffff0000) | (val & 0xffff); break;
        }
    } else if constexpr (std::is_same<Type, uint32_t>::value || std::is_same<Type, int32_t>::value) {
        switch (reg) {
            case 0: D0 = val; break; case 1: D1 = val; break;
            case 2: D2 = val; break; case 3: D3 = val; break;
            case 4: D4 = val; break; case 5: D5 = val; break;
            case 6: D6 = val; break; case 7: D7 = val; break;
        }
    }
}

template void setDn<BYTE>(int reg, BYTE val);
template void setDn<WORD>(int reg, WORD val);
template void setDn<LONG>(int reg, LONG val);
template void setDn<UBYTE>(int reg, UBYTE val);
template void setDn<UWORD>(int reg, UWORD val);
template void setDn<ULONG>(int reg, ULONG val);

template<class Type>
Type getAn(int reg)
{
    switch (reg) {
        case 0: return (Type)A0; case 1: return (Type)A1;
        case 2: return (Type)A2; case 3: return (Type)A3;
        case 4: return (Type)A4; case 5: return (Type)A5;
        case 6: return (Type)A6; case 7: return (Type)A7;
    }
    __builtin_unreachable();
}

template WORD getAn<WORD>(int reg);
template LONG getAn<LONG>(int reg);
template UWORD getAn<UWORD>(int reg);
template ULONG getAn<ULONG>(int reg);

template<class Type>
void setAn(int reg, Type value)
{
    if constexpr (std::is_same<Type, uint16_t>::value || std::is_same<Type, int16_t>::value) {
        switch (reg) {
            case 0: A0 = (int16_t)value; break; case 1: A1 = (int16_t)value; break;
            case 2: A2 = (int16_t)value; break; case 3: A3 = (int16_t)value; break;
            case 4: A4 = (int16_t)value; break; case 5: A5 = (int16_t)value; break;
            case 6: A6 = (int16_t)value; break; case 7: A7 = (int16_t)value; break;
        }
    } else if constexpr (std::is_same<Type, uint32_t>::value || std::is_same<Type, int32_t>::value) {
        switch (reg) {
            case 0: A0 = value; break; case 1: A1 = value; break;
            case 2: A2 = value; break; case 3: A3 = value; break;
            case 4: A4 = value; break; case 5: A5 = value; break;
            case 6: A6 = value; break; case 7: A7 = value; break;
        }
    }
    __builtin_unreachable();
}

template void setAn<WORD>(int reg, WORD value);
template void setAn<LONG>(int reg, LONG value);
template void setAn<UWORD>(int reg, UWORD value);
template void setAn<ULONG>(int reg, ULONG value);

template<class Type>
Type getFPn(int reg)
{
    switch (reg) {
        case 0: return (Type)FP0; case 1: return (Type)FP1;
        case 2: return (Type)FP2; case 3: return (Type)FP3;
        case 4: return (Type)FP4; case 5: return (Type)FP5;
        case 6: return (Type)FP6; case 7: return (Type)FP7;
    }
    __builtin_unreachable();
}

template float getFPn<float>(int reg);
template double getFPn<double>(int reg);

template<class Type>
void setFPn(int reg, Type value)
{
    switch (reg) {
        case 0: FP0 = value; break; case 1: FP1 = value; break;
        case 2: FP2 = value; break; case 3: FP3 = value; break;
        case 4: FP4 = value; break; case 5: FP5 = value; break;
        case 6: FP6 = value; break; case 7: FP7 = value; break;
    }
    __builtin_unreachable();
}

template void setFPn<float>(int reg, float value);
template void setFPn<double>(int reg, double value);

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

void Exception_F1(uint32_t exception)
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
    *(uint16_t *)((uintptr_t)sp + 6) = exception | 0x1000;

    /* Set SR to supervisor and clear trace flags */
    origSR |= SR_S;
    origSR &= ~(SR_T0 | SR_T1);

    /* Set the new SR */
    SR = origSR;

    /* Get the vector address */
    uint32_t vbr = getCTX()->VBR + (exception & 0x0fff);
    PC = *(uint32_t *)(uintptr_t)vbr;
}

void Exception_F2(uint32_t exception, uint32_t ea)
{
    /* Get the SR register as it is now */
    uint32_t origSR = SR;

    /* If we were in user mode, switch stack and reserve space for exception frame */
    if (likely(origSR & SR_S) == 0) {
        USP = A7;
        if (unlikely(origSR & SR_M)) {
            A7 = MSP - 12;
        }
        else {
            A7 = ISP - 12;
        }
    } else {
        A7 -= 12;
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
    *(uint16_t *)((uintptr_t)sp + 6) = exception | 0x2000;
    *(uint32_t *)((uintptr_t)sp + 8) = ea;

    /* Set SR to supervisor and clear trace flags */
    origSR |= SR_S;
    origSR &= ~(SR_T0 | SR_T1);

    /* Set the new SR */
    SR = origSR;

    /* Get the vector address */
    uint32_t vbr = getCTX()->VBR + (exception & 0x0fff);
    PC = *(uint32_t *)(uintptr_t)vbr;
}

void Exception_F3(uint32_t exception, uint32_t ea)
{
    /* Get the SR register as it is now */
    uint32_t origSR = SR;

    /* If we were in user mode, switch stack and reserve space for exception frame */
    if (likely(origSR & SR_S) == 0) {
        USP = A7;
        if (unlikely(origSR & SR_M)) {
            A7 = MSP - 12;
        }
        else {
            A7 = ISP - 12;
        }
    } else {
        A7 -= 12;
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
    *(uint16_t *)((uintptr_t)sp + 6) = exception | 0x3000;
    *(uint32_t *)((uintptr_t)sp + 8) = ea;

    /* Set SR to supervisor and clear trace flags */
    origSR |= SR_S;
    origSR &= ~(SR_T0 | SR_T1);

    /* Set the new SR */
    SR = origSR;

    /* Get the vector address */
    uint32_t vbr = getCTX()->VBR + (exception & 0x0fff);
    PC = *(uint32_t *)(uintptr_t)vbr;
}

void Exception_F4(uint32_t exception, uint32_t ea, uint32_t pc)
{
    /* Get the SR register as it is now */
    uint32_t origSR = SR;

    /* If we were in user mode, switch stack and reserve space for exception frame */
    if (likely(origSR & SR_S) == 0) {
        USP = A7;
        if (unlikely(origSR & SR_M)) {
            A7 = MSP - 16;
        }
        else {
            A7 = ISP - 16;
        }
    } else {
        A7 -= 16;
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
    *(uint16_t *)((uintptr_t)sp + 6) = exception | 0x4000;
    *(uint32_t *)((uintptr_t)sp + 8) = ea;
    *(uint32_t *)((uintptr_t)sp + 12) = pc;

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
    disasm_open();
    kprintf("[INT] opcode %04x at %08x not implemented\n", opcode, PC);
    disasm_print_m68k_only((uint16_t*)(uintptr_t)getCTX()->PC);
    M68K_PrintContext(getCTX());
    M68K_LoadContext(getCTX());

    Exception_F0(VECTOR_ILLEGAL_INSTRUCTION);
}

void LoadFrom_EA_Mod0(int src_reg, uint8_t size, void *out)
{
    uint32_t dn = getDn<uint32_t>(src_reg);

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
    uint32_t an = getAn<uint32_t>(src_reg);

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
    uint32_t addr = getAn<uint32_t>(src_reg);

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
    uint32_t addr = getAn<uint32_t>(src_reg);

    switch (size) {
        case 4:
            *(uint32_t *)out = *(uint32_t *)(uintptr_t)addr;
            addr += 4;
            setAn<uint32_t>(src_reg, addr);
            return;
        case 2:
            *(uint16_t *)out = *(uint16_t *)(uintptr_t)addr;
            addr += 2;
            setAn<uint32_t>(src_reg, addr);
            return;
        case 1:
            *(uint8_t *)out = *(uint8_t *)(uintptr_t)addr;
            addr += (src_reg == 7) ? 2 : 1;
            setAn<uint32_t>(src_reg, addr);
            return;
    }

    __builtin_unreachable();
}

void LoadFrom_EA_Mod4(uint8_t src_reg, uint8_t size, void *out)
{
    uint32_t addr = getAn<uint32_t>(src_reg);
    switch (size) {
        case 4:
            addr -= 4;
            setAn<uint32_t>(src_reg, addr);
            *(uint32_t *)out = *(uint32_t *)(uintptr_t)addr;
            return;
        case 2:
            addr -= 2;
            setAn<uint32_t>(src_reg, addr);
            *(uint16_t *)out = *(uint16_t *)(uintptr_t)addr;
            return;
        case 1:
            addr -= (src_reg == 7) ? 2 : 1;
            setAn<uint32_t>(src_reg, addr);
            *(uint8_t *)out = *(uint8_t *)(uintptr_t)addr;
            return;
    }

    __builtin_unreachable();
}

void LoadFrom_EA_Mod5(uint8_t src_reg, uint8_t size, void *out)
{
    uint32_t addr = getAn<uint32_t>(src_reg) + *(int16_t *)(uintptr_t)PC;
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
            index_value = (ext_word & M68K_EA_DA) ? getAn<uint32_t>(index_reg) : getDn<uint32_t>(index_reg);

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
        uint32_t index_value = (ext_word & M68K_EA_DA) ? getAn<uint32_t>(index_reg) : getDn<uint32_t>(index_reg);

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
    LoadFrom_EA_EXT(getAn<uint32_t>(src_reg), size, out);
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
    *out = getAn<uint32_t>(src_reg);
}

void Get_EA_Mod3(uint8_t src_reg, uint8_t size, uint32_t *out)
{
    uint32_t addr = getAn<uint32_t>(src_reg);
    
    *out = addr;
    addr += size;
    if (src_reg == 7 && size == 1) addr++;

    setAn<uint32_t>(src_reg, addr);
}

void Get_EA_Mod4(uint8_t src_reg, uint8_t size, uint32_t *out)
{
    uint32_t addr = getAn<uint32_t>(src_reg);

    addr -= size;
    if (src_reg == 7 && size == 1) addr--;
    *out = addr;

    setAn<uint32_t>(src_reg, addr);
}

void Get_EA_Mod5(uint8_t src_reg, uint8_t, uint32_t *out)
{
    *out = getAn<uint32_t>(src_reg) + *(int16_t *)(uintptr_t)PC;
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
            index_value = (ext_word & M68K_EA_DA) ? getAn<uint32_t>(index_reg) : getDn<uint32_t>(index_reg);

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
        uint32_t index_value = (ext_word & M68K_EA_DA) ? getAn<uint32_t>(index_reg) : getDn<uint32_t>(index_reg);

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
    GetExtendedEffectiveAddress(getAn<uint32_t>(src_reg), size, out);
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
    else if (src_reg == 4)
    {
        *out = PC;
        PC += size;
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
            setDn<uint32_t>(dst_reg, value);
            return;
        case 2:
            setDn<uint16_t>(dst_reg, value);
            return;
        case 1:
            setDn<uint8_t>(dst_reg, value);
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
            setAn<int16_t>(dst_reg, (int16_t)value);
            return;
    }

    __builtin_unreachable();
}

void StoreTo_EA_Mod2(uint8_t dst_reg, uint32_t value, uint8_t size)
{
    uintptr_t addr = getAn<uint32_t>(dst_reg);

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
    uintptr_t addr = getAn<uint32_t>(dst_reg);

    switch (size) {
        case 4:
            *(uint32_t *)addr = value;
            addr += 4;
            setAn<uint32_t>(dst_reg, addr);
            return;
        case 2:
            *(uint16_t *)addr = value;
            addr += 2;
            setAn<uint32_t>(dst_reg, addr);
            return;
        case 1:
            *(uint8_t *)addr = value;
            addr += (dst_reg == 7) ? 2 : 1;
            setAn<uint32_t>(dst_reg, addr);
            return;
    }

    __builtin_unreachable();
}

void StoreTo_EA_Mod4(uint8_t dst_reg, uint32_t value, uint8_t size)
{
    uint32_t addr = getAn<uint32_t>(dst_reg);

    switch (size) {
        case 4:
            addr -= 4;
            setAn<uint32_t>(dst_reg, addr);
            *(uint32_t *)(uintptr_t)addr = value;
            return;
        case 2:
            addr -= 2;
            setAn<uint32_t>(dst_reg, addr);
            *(uint16_t *)(uintptr_t)addr = value;
            return;
        case 1:
            addr -= (dst_reg == 7) ? 2 : 1;
            setAn<uint32_t>(dst_reg, addr);
            *(uint8_t *)(uintptr_t)addr = value;
            return;
    }

    __builtin_unreachable();
}

void StoreTo_EA_Mod5(uint8_t dst_reg, uint32_t value, uint8_t size)
{
    uint32_t addr = getAn<uint32_t>(dst_reg) + *(int16_t *)(uintptr_t)PC;
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
            An = getAn<uint32_t>(dst_reg);
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
            index_value = (ext_word & M68K_EA_DA) ? getAn<uint32_t>(index_reg) : getDn<uint32_t>(index_reg);

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
        uint32_t index_value = (ext_word & M68K_EA_DA) ? getAn<uint32_t>(index_reg) : getDn<uint32_t>(index_reg);
        
        An = getAn<uint32_t>(dst_reg);

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
