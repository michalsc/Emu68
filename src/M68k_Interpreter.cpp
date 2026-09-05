#include <cstdint>
#include <cstdarg>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter>

#include "RegisterMapping.h"

#define _REGLOCK_H
extern "C" {

#include "M68k.h"
#include "support.h"
#include "disasm.h"

}


extern "C" {

void M68K_LoadContext(struct M68KState* ctx);
void M68K_SaveContext(struct M68KState* ctx);
void M68K_PrintContext(struct M68KState* ctx);
extern int debug_not_implemented;

}

template<class Type>
Type getD(int reg)
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

template BYTE getD<BYTE>(int);
template WORD getD<WORD>(int);
template LONG getD<LONG>(int);
template UBYTE getD<UBYTE>(int);
template UWORD getD<UWORD>(int);
template ULONG getD<ULONG>(int);

template<class Type>
void setD(int reg, Type val)
{
    if constexpr (std::is_same<Type, uint8_t>::value || std::is_same<Type, int8_t>::value) {
        switch (reg) {
            case 0: D0 = (D0 & 0xffffff00) | (val & 0xff); break; case 1: D1 = (D1 & 0xffffff00) | (val & 0xff); break;
            case 2: D2 = (D2 & 0xffffff00) | (val & 0xff); break; case 3: D3 = (D3 & 0xffffff00) | (val & 0xff); break;
            case 4: D4 = (D4 & 0xffffff00) | (val & 0xff); break; case 5: D5 = (D5 & 0xffffff00) | (val & 0xff); break;
            case 6: D6 = (D6 & 0xffffff00) | (val & 0xff); break; case 7: D7 = (D7 & 0xffffff00) | (val & 0xff); break;
            default: __builtin_unreachable();
        }
    } else if constexpr (std::is_same<Type, uint16_t>::value || std::is_same<Type, int16_t>::value) {
        switch (reg) {
            case 0: D0 = (D0 & 0xffff0000) | (val & 0xffff); break; case 1: D1 = (D1 & 0xffff0000) | (val & 0xffff); break;
            case 2: D2 = (D2 & 0xffff0000) | (val & 0xffff); break; case 3: D3 = (D3 & 0xffff0000) | (val & 0xffff); break;
            case 4: D4 = (D4 & 0xffff0000) | (val & 0xffff); break; case 5: D5 = (D5 & 0xffff0000) | (val & 0xffff); break;
            case 6: D6 = (D6 & 0xffff0000) | (val & 0xffff); break; case 7: D7 = (D7 & 0xffff0000) | (val & 0xffff); break;
            default: __builtin_unreachable();
        }
    } else if constexpr (std::is_same<Type, uint32_t>::value || std::is_same<Type, int32_t>::value) {
        switch (reg) {
            case 0: D0 = val; break; case 1: D1 = val; break;
            case 2: D2 = val; break; case 3: D3 = val; break;
            case 4: D4 = val; break; case 5: D5 = val; break;
            case 6: D6 = val; break; case 7: D7 = val; break;
            default: __builtin_unreachable();
        }
    }
}

template void setD<BYTE>(int reg, BYTE val);
template void setD<WORD>(int reg, WORD val);
template void setD<LONG>(int reg, LONG val);
template void setD<UBYTE>(int reg, UBYTE val);
template void setD<UWORD>(int reg, UWORD val);
template void setD<ULONG>(int reg, ULONG val);

template<class Type>
Type getA(int reg)
{
    switch (reg) {
        case 0: return (Type)A0; case 1: return (Type)A1;
        case 2: return (Type)A2; case 3: return (Type)A3;
        case 4: return (Type)A4; case 5: return (Type)A5;
        case 6: return (Type)A6; case 7: return (Type)A7;
    }
    __builtin_unreachable();
}

template WORD getA<WORD>(int reg);
template LONG getA<LONG>(int reg);
template UWORD getA<UWORD>(int reg);
template ULONG getA<ULONG>(int reg);

template<class Type>
void setA(int reg, Type value)
{
    if constexpr (std::is_same<Type, uint16_t>::value || std::is_same<Type, int16_t>::value) {
        switch (reg) {
            case 0: A0 = (int16_t)value; break; case 1: A1 = (int16_t)value; break;
            case 2: A2 = (int16_t)value; break; case 3: A3 = (int16_t)value; break;
            case 4: A4 = (int16_t)value; break; case 5: A5 = (int16_t)value; break;
            case 6: A6 = (int16_t)value; break; case 7: A7 = (int16_t)value; break;
            default: __builtin_unreachable();
        }
    } else if constexpr (std::is_same<Type, uint32_t>::value || std::is_same<Type, int32_t>::value) {
        switch (reg) {
            case 0: A0 = value; break; case 1: A1 = value; break;
            case 2: A2 = value; break; case 3: A3 = value; break;
            case 4: A4 = value; break; case 5: A5 = value; break;
            case 6: A6 = value; break; case 7: A7 = value; break;
            default: __builtin_unreachable();
        }
    }
}

template void setA<WORD>(int reg, WORD value);
template void setA<LONG>(int reg, LONG value);
template void setA<UWORD>(int reg, UWORD value);
template void setA<ULONG>(int reg, ULONG value);

template<class Type>
Type getFP(int reg)
{
    switch (reg) {
        case 0: return (Type)FP0; case 1: return (Type)FP1;
        case 2: return (Type)FP2; case 3: return (Type)FP3;
        case 4: return (Type)FP4; case 5: return (Type)FP5;
        case 6: return (Type)FP6; case 7: return (Type)FP7;
    }
    __builtin_unreachable();
}

template float getFP<float>(int reg);
template double getFP<double>(int reg);

template<class Type>
void setFP(int reg, Type value)
{
    switch (reg) {
        case 0: FP0 = value; break; case 1: FP1 = value; break;
        case 2: FP2 = value; break; case 3: FP3 = value; break;
        case 4: FP4 = value; break; case 5: FP5 = value; break;
        case 6: FP6 = value; break; case 7: FP7 = value; break;
        default: __builtin_unreachable();
    }
}

template void setFP<float>(int reg, float value);
template void setFP<double>(int reg, double value);

namespace Emu68::M68k::Interpreter {

[[gnu::always_inline]] inline struct M68KState* getCTX()
{
    struct M68KState* ctx;
    __asm__ volatile("mov %0, " CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}

void putByte(void *data, char c)
{
    (void)data;
    *(volatile uint8_t *)0xdeadbeef = c;
}

void bug(const char * format, ...)
{
    va_list v;
    M68K_SaveContext(getCTX());
    va_start(v, format);
    vkprintf_pc(putByte, 0, format, v);
    va_end(v);
    M68K_LoadContext(getCTX());
}

void raiseException(uint32_t exception, ExceptionFrameFormat format, uint32_t ea, uint32_t pc)
{
    /* Get the SR register as it is now */
    uint32_t origSR = SR;
    uint32_t stack_frame_size = 0;

    #if 1
    if (exception != VECTOR_PRIVILEGE_VIOLATION) {
        bug("[INT] raiseException(%x, %d, %08x, %08x) called from %p\n", exception, format, ea, pc, __builtin_return_address(0));

        M68K_SaveContext(getCTX());
        disasm_open();
        disasm_print_m68k_only((uint16_t*)(uintptr_t)getCTX()->PC);

        M68K_PrintContext(getCTX());
        M68K_LoadContext(getCTX());
    }
    #endif

    switch (format) {
        case ExceptionFrameFormat::FORMAT_0: [[fallthrough]];
        case ExceptionFrameFormat::FORMAT_1: stack_frame_size = 8; break;
        case ExceptionFrameFormat::FORMAT_2: [[fallthrough]];
        case ExceptionFrameFormat::FORMAT_3: stack_frame_size = 12; break;
        case ExceptionFrameFormat::FORMAT_4: stack_frame_size = 16; break;
    }

    /* If we were in user mode, switch stack and reserve space for exception frame */
    if (likely(origSR & SR_S) == 0) {
        USP = A7;
        if (unlikely(origSR & SR_M)) {
            A7 = MSP - stack_frame_size;
        } else {
            A7 = ISP - stack_frame_size;
        }
    } else {
        A7 -= stack_frame_size;
    }

    /* Push the exception frame */
    void* sp = (void*)(uintptr_t)A7;

    /* Prepare frame */
    *(uint16_t*)(uintptr_t)sp = swapVC(origSR);
    *(uint32_t*)((uintptr_t)sp + 2) = PC;
    *(uint16_t*)((uintptr_t)sp + 6) = exception | static_cast<uint32_t>(format);
    if (format >= ExceptionFrameFormat::FORMAT_2) {
        *(uint32_t*)((uintptr_t)sp + 8) = ea;
        if (format >= ExceptionFrameFormat::FORMAT_4) {
            *(uint32_t*)((uintptr_t)sp + 12) = pc;
        }
    }

    /* Set SR to supervisor and clear trace flags */
    origSR |= SR_S;
    origSR &= ~(SR_T0 | SR_T1);

    /* Set the new SR */
    SR = origSR;

    /* Get the vector address */
    uint32_t vbr = getCTX()->VBR + (exception & 0x0fff);
    PC = *(uint32_t*)(uintptr_t)vbr;
}

void ILLEGAL(uint32_t opcode)
{
    if (opcode != 0x4afc && debug_not_implemented != 0) {
        safeCall([&](){
            kprintf("[INT] ILLEGAL INSTRUCTION %04x at %08x\n", opcode, getPC());

            disasm_open();
            disasm_print_m68k_only((uint16_t*)(uintptr_t)getCTX()->PC);

            M68K_PrintContext(getCTX());
        });
    }

    raiseException(VECTOR_ILLEGAL_INSTRUCTION, ExceptionFrameFormat::FORMAT_0, 0, 0);
}

template<class Type>
Type loadFromEA_Mod2(uint32_t, uint32_t reg)
{
    uint32_t addr = getA<uint32_t>(reg);

    return *(Type*)(uintptr_t)addr;
}

template<class Type>
Type loadFromEA_Mod3(uint32_t, uint32_t reg)
{
    uint32_t addr = getA<uint32_t>(reg);

    Type retval = *(Type*)(uintptr_t)addr;

    addr += sizeof(Type) + ((reg == 7 && sizeof(Type) == 1) ? 1 : 0);

    setA<uint32_t>(reg, addr);

    return retval;
}

template<class Type>
Type loadFromEA_Mod4(uint32_t, uint32_t reg)
{
    uint32_t addr = getA<uint32_t>(reg);

    addr -= sizeof(Type) + ((reg == 7 && sizeof(Type) == 1) ? 1 : 0);

    setA<uint32_t>(reg, addr);

    return *(Type*)(uintptr_t)addr;
}

template<class Type>
Type loadFromEA_Mod5(uint32_t, uint32_t reg)
{
    uint32_t addr = getA<uint32_t>(reg) + *(int16_t*)(uintptr_t)PC;
    PC += 2;

    return *(Type*)(uintptr_t)addr;
}

template<class Type>
Type loadFromEA_EXT(uint32_t base)
{
    uint16_t ext_word = *(uint16_t*)(uintptr_t)PC;
    uint32_t scale = 1 << ((ext_word & M68K_EA_SCALE) >> 9);
    uint32_t index_reg = (ext_word & M68K_EA_REG) >> 12;

    PC += 2;
    
    if (ext_word & M68K_EA_FULL)
    {
        uint32_t base_displacement = 0;
        uint32_t outer_displacement = 0;
        uint32_t index_value = 0;

        if (ext_word & M68K_EA_BS) {
            base = 0;
        }

        switch (ext_word & M68K_EA_BD_SIZE) {
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
            index_value = (ext_word & M68K_EA_DA) ? getA<uint32_t>(index_reg) : getD<uint32_t>(index_reg);

            if ((ext_word & M68K_EA_WL) == 0) {
                index_value = (int16_t)index_value;
            }

            index_value *= scale;
        }

        switch (ext_word & 3) {
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
            indirect_address = base + base_displacement + index_value;
        } else if ((ext_word & 4) == 0) {
            // Indirect preindexed: index folded in before the dereference
            indirect_address = *(uint32_t *)(uintptr_t)(base + base_displacement + index_value);
        } else {
            // Indirect postindexed: index added after the dereference
            indirect_address = *(uint32_t *)(uintptr_t)(base + base_displacement) + index_value;
        }

        indirect_address += outer_displacement;

        return *(Type*)(uintptr_t)indirect_address;
    }
    else
    {
        int8_t displacement = ext_word & M68K_EA_OFF8;
        uint32_t index_value = (ext_word & M68K_EA_DA) ? getA<uint32_t>(index_reg) : getD<uint32_t>(index_reg);

        if ((ext_word & M68K_EA_WL) == 0) {
            index_value = (int16_t)index_value;
        }

        base += displacement;
        base += index_value * scale;

        return *(Type*)(uintptr_t)base;
    }
}

template<class Type>
Type loadFromEA_Mod6(uint32_t, uint32_t reg)
{
    return loadFromEA_EXT<Type>(getA<uint32_t>(reg));
}

template<class Type>
Type loadFromEA_Mod7(uint32_t, uint32_t reg)
{
    if (reg == 0)
    {
        uintptr_t addr = (uint32_t)*(int16_t*)(uintptr_t)PC;
        PC += 2;
        return *(Type*)addr;
    } else if (reg == 1) {
        uintptr_t addr = *(uint32_t*)(uintptr_t)PC;
        PC += 4;
        return *(Type*)addr;
    } else if (reg == 2) {
        int16_t off = *(int16_t*)(uintptr_t)PC;
        uint32_t addr = (uint32_t)(uintptr_t)PC + off;
        PC += 2;
        return *(Type*)(uintptr_t)addr;
    } else if (reg == 3) {
        return loadFromEA_EXT<Type>(PC);
    } else if (reg == 4) {
        if (sizeof(Type) > 1) {
            Type val = *(Type*)(uintptr_t)PC;
            PC += sizeof(Type);
            return val;
        } else {
            Type val = (Type)*(uint16_t*)(uintptr_t)PC;
            PC += sizeof(uint16_t);
            return val;
        }
    } else {
        __builtin_unreachable();
    }
}

template<class Type>
Type loadFromEA(uint32_t mode, uint32_t reg)
{
    switch (mode) {
        case 0: [[unlikely]] return getD<Type>(reg);
        case 1: [[unlikely]] return WordOrLongSize<Type> ? getA<Type>(reg) : 0;
        case 2: [[unlikely]] return loadFromEA_Mod2<Type>(mode, reg);
        case 3: [[unlikely]] return loadFromEA_Mod3<Type>(mode, reg);
        case 4: [[unlikely]] return loadFromEA_Mod4<Type>(mode, reg);
        case 5: return loadFromEA_Mod5<Type>(mode, reg);
        case 6: return loadFromEA_Mod6<Type>(mode, reg);
        case 7: return loadFromEA_Mod7<Type>(mode, reg);
    }

    __builtin_unreachable();
}

template LONG loadFromEA<LONG>(uint32_t mode, uint32_t reg);
template WORD loadFromEA<WORD>(uint32_t mode, uint32_t reg);
template BYTE loadFromEA<BYTE>(uint32_t mode, uint32_t reg);
template ULONG loadFromEA<ULONG>(uint32_t mode, uint32_t reg);
template UWORD loadFromEA<UWORD>(uint32_t mode, uint32_t reg);
template UBYTE loadFromEA<UBYTE>(uint32_t mode, uint32_t reg);

template<class Type>
uint32_t getEA_Mod3(uint32_t, uint32_t reg)
{
    uint32_t addr = getA<uint32_t>(reg);
    uint32_t retval = addr;

    if constexpr (!std::is_same<Type, void>::value) {
        addr += sizeof(Type);
        if (reg == 7 && sizeof(Type) == 1) addr++;

        setA<uint32_t>(reg, addr);
    }

    return retval;
}

template<class Type>
uint32_t getEA_Mod4(uint32_t, uint32_t reg)
{
    uint32_t addr = getA<uint32_t>(reg);

    if constexpr (!std::is_same<Type, void>::value) {
        addr -= sizeof(Type);
        if (reg == 7 && sizeof(Type) == 1) addr--;
        setA<uint32_t>(reg, addr);
    }
    
    return addr;
}

template<class Type>
uint32_t getEA_Mod5(uint32_t, uint32_t reg)
{
    uint32_t addr = getA<uint32_t>(reg) + *(int16_t*)(uintptr_t)PC;
    PC += 2;
    return addr;
}

template<class Type>
uint32_t getExtendedEA(uint32_t base)
{
    uint16_t ext_word = *(uint16_t*)(uintptr_t)PC;
    uint8_t scale = 1 << ((ext_word & M68K_EA_SCALE) >> 9);
    uint8_t index_reg = (ext_word & M68K_EA_REG) >> 12;

    PC += 2;
    
    if (ext_word & M68K_EA_FULL)
    {
        uint32_t base_displacement = 0;
        uint32_t outer_displacement = 0;
        uint32_t index_value = 0;

        if (ext_word & M68K_EA_BS) {
            base = 0;
        }

        switch (ext_word & M68K_EA_BD_SIZE) {
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
            index_value = (ext_word & M68K_EA_DA) ? getA<uint32_t>(index_reg) : getD<uint32_t>(index_reg);

            if ((ext_word & M68K_EA_WL) == 0) {
                index_value = (int16_t)index_value;
            }

            index_value *= scale;
        }

        switch (ext_word & 3) {
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
            indirect_address = base + base_displacement + index_value;
        } else if ((ext_word & 4) == 0) {
            // Indirect preindexed: index folded in before the dereference
            indirect_address = *(uint32_t *)(uintptr_t)(base + base_displacement + index_value);
        } else {
            // Indirect postindexed: index added after the dereference
            indirect_address = *(uint32_t *)(uintptr_t)(base + base_displacement) + index_value;
        }

        indirect_address += outer_displacement;

        return indirect_address;
    }
    else
    {
        int8_t displacement = ext_word & M68K_EA_OFF8;
        uint32_t index_value = (ext_word & M68K_EA_DA) ? getA<uint32_t>(index_reg) : getD<uint32_t>(index_reg);

        if ((ext_word & M68K_EA_WL) == 0) {
            index_value = (int16_t)index_value;
        }

        base += displacement;
        base += index_value * scale;

        return base;
    }
}

template<class Type>
uint32_t getEA_Mod6(uint32_t, uint32_t reg)
{
    return getExtendedEA<Type>(getA<uint32_t>(reg));
}

template<class Type>
uint32_t getEA_Mod7(uint32_t, uint32_t reg)
{
    if (reg == 0)
    {
        uint32_t addr = (uint32_t)*(int16_t*)(uintptr_t)PC;
        PC += 2;
        return addr;
    } else if (reg == 1) {
        uint32_t addr = *(uint32_t*)(uintptr_t)PC;
        PC += 4;
        return addr;
    } else if (reg == 2) {
        int16_t off = *(int16_t*)(uintptr_t)PC;
        uint32_t addr = (uint32_t)(uintptr_t)PC + off;
        PC += 2;
        return addr;
    } else if (reg == 3) {
        return getExtendedEA<Type>(PC);
    } else if (reg == 4) {
        uint32_t addr = PC;
        if constexpr (!std::is_same<Type, void>::value) {
            PC += sizeof(Type);
            if (sizeof(Type) == 1) { PC++; addr++; }
        }
        return addr;
    } else {
        __builtin_unreachable();
    }
}

template<class Type>
uint32_t getEA(uint32_t mode, uint32_t reg)
{
    switch (mode) {
        // mode 0 and 1 cannot return EA address at all
        case 2: [[unlikely]] return getA<uint32_t>(reg);
        case 3: [[unlikely]] return getEA_Mod3<Type>(mode, reg);
        case 4: [[unlikely]] return getEA_Mod4<Type>(mode, reg);
        case 5: return getEA_Mod5<Type>(mode, reg);
        case 6: return getEA_Mod6<Type>(mode, reg);
        case 7: return getEA_Mod7<Type>(mode, reg);
    }

    __builtin_unreachable();
}

template uint32_t getEA<void>(uint32_t mode, uint32_t reg);
template uint32_t getEA<BYTE>(uint32_t mode, uint32_t reg);
template uint32_t getEA<WORD>(uint32_t mode, uint32_t reg);
template uint32_t getEA<LONG>(uint32_t mode, uint32_t reg);
template uint32_t getEA<UBYTE>(uint32_t mode, uint32_t reg);
template uint32_t getEA<UWORD>(uint32_t mode, uint32_t reg);
template uint32_t getEA<ULONG>(uint32_t mode, uint32_t reg);
template uint32_t getEA<float>(uint32_t mode, uint32_t reg);
template uint32_t getEA<double>(uint32_t mode, uint32_t reg);
template uint32_t getEA<uint32_t[2]>(uint32_t mode, uint32_t reg);
template uint32_t getEA<uint32_t[3]>(uint32_t mode, uint32_t reg);

template<class Type>
void storeToEA_Mod2(uint32_t, uint32_t reg, Type value)
{
    uintptr_t addr = getA<uint32_t>(reg);

    *(Type*)addr = value;
}

template<class Type>
void storeToEA_Mod3(uint32_t, uint32_t reg, Type value)
{
    uintptr_t addr = getA<uint32_t>(reg);
    *(Type*)addr = value;
    addr += sizeof(Type) + ((reg == 7 && sizeof(Type) == 1) ? 1 : 0);
    setA<uint32_t>(reg, addr);
}

template<class Type>
void storeToEA_Mod4(uint32_t, uint32_t reg, Type value)
{
    uintptr_t addr = getA<uint32_t>(reg);
    addr -= sizeof(Type) + ((reg == 7 && sizeof(Type) == 1) ? 1 : 0);
    *(Type*)addr = value;
    setA<uint32_t>(reg, addr);
}

template<class Type>
void storeToEA_Mod5(uint32_t, uint32_t reg, Type value)
{
    uint32_t addr = getA<uint32_t>(reg) + *(int16_t*)(uintptr_t)PC;
    PC += 2;
    *(Type*)(uintptr_t)addr = value;
}

template<class Type>
void storeToEA_Mod6(uint32_t, uint32_t reg, Type value)
{
    uint16_t ext_word = *(uint16_t*)(uintptr_t)PC;
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
            An = getA<uint32_t>(reg);
        }

        switch (ext_word & M68K_EA_BD_SIZE) {
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
            index_value = (ext_word & M68K_EA_DA) ? getA<uint32_t>(index_reg) : getD<uint32_t>(index_reg);

            if ((ext_word & M68K_EA_WL) == 0) {
                index_value = (int16_t)index_value;
            }

            index_value *= scale;
        }

        switch (ext_word & 3) {
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
        } else if ((ext_word & 4) == 0) {
            // Indirect preindexed: index folded in before the dereference
            indirect_address = *(uint32_t *)(uintptr_t)(An + base_displacement + index_value);
        } else {
            // Indirect postindexed: index added after the dereference
            indirect_address = *(uint32_t *)(uintptr_t)(An + base_displacement) + index_value;
        }

        indirect_address += outer_displacement;

        *(Type*)(uintptr_t)indirect_address = value;
    }
    else
    {
        int8_t displacement = ext_word & M68K_EA_OFF8;
        uint32_t index_value = (ext_word & M68K_EA_DA) ? getA<uint32_t>(index_reg) : getD<uint32_t>(index_reg);
        
        An = getA<uint32_t>(reg);

        if ((ext_word & M68K_EA_WL) == 0) {
            index_value = (int16_t)index_value;
        }

        An += displacement;
        An += index_value * scale;

        *(Type*)(uintptr_t)An = value;
    }
}

template<class Type>
void storeToEA_Mod7(uint32_t, uint32_t reg, Type value)
{
    if (reg == 0) {
        uintptr_t addr = (uint32_t)*(int16_t*)(uintptr_t)PC;
        PC += 2;
        *(Type*)addr = value;
    } else if (reg == 1) {
        uintptr_t addr = *(uint32_t*)(uintptr_t)PC;
        PC += 4;
        *(Type*)addr = value;
    } else {
        __builtin_unreachable();
    }
}

template<class Type>
void storeToEA(uint32_t mode, uint32_t reg, Type value)
{
    switch (mode) {
        case 0: [[unlikely]] setD<Type>(reg, value); return;
        case 1: [[unlikely]] if constexpr(WordOrLongSize<Type>) setA<Type>(reg, value); return;
        case 2: [[unlikely]] storeToEA_Mod2<Type>(mode, reg, value); return;
        case 3: [[unlikely]] storeToEA_Mod3<Type>(mode, reg, value); return;
        case 4: [[unlikely]] storeToEA_Mod4<Type>(mode, reg, value); return;
        case 5: storeToEA_Mod5<Type>(mode, reg, value); return;
        case 6: storeToEA_Mod6<Type>(mode, reg, value); return;
        case 7: storeToEA_Mod7<Type>(mode, reg, value); return;
    }

    __builtin_unreachable();
}

template void storeToEA<BYTE>(uint32_t mode, uint32_t reg, BYTE value);
template void storeToEA<WORD>(uint32_t mode, uint32_t reg, WORD value);
template void storeToEA<LONG>(uint32_t mode, uint32_t reg, LONG value);
template void storeToEA<UBYTE>(uint32_t mode, uint32_t reg, UBYTE value);
template void storeToEA<UWORD>(uint32_t mode, uint32_t reg, UWORD value);
template void storeToEA<ULONG>(uint32_t mode, uint32_t reg, ULONG value);

void handleChangedSR(uint32_t sr, uint32_t changed)
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
