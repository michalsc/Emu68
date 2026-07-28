#ifndef _EMU68_M68K_INTERPRETER_HPP_
#define _EMU68_M68K_INTERPRETER_HPP_

#ifdef __cplusplus
#include <cstdint>
#include <type_traits>
#include <utility>
#else
#include <stdint.h>
#endif

#ifdef __cplusplus

#define _REGLOCK_H
extern "C" {
    #include "M68k.h"
    #include "support.h"
}
#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter {

#define I_SIZE_BYTE   1
#define I_SIZE_WORD   2
#define I_SIZE_LONG   4

void Exception_F0(uint32_t exception);
void Exception_F1(uint32_t exception);
void Exception_F2(uint32_t exception, uint32_t ea);
void Exception_F3(uint32_t exception, uint32_t ea);
void Exception_F4(uint32_t exception, uint32_t ea, uint32_t pc);
void LoadFromEffectiveAddress(uint8_t src_reg, uint8_t size, void *out, uint8_t mode);
void StoreToEffectiveAddress(uint8_t dst_reg, uint32_t value, uint8_t size, uint8_t mode);
void GetEffectiveAddress(uint8_t src_reg, uint8_t size, uint32_t *out, uint8_t mode);
void GetExtendedEffectiveAddress(uint32_t An, uint8_t, uint32_t *out);
void HandleChangedSR(uint32_t sr, uint32_t changed);
void UNIMPLEMENTED(uint32_t opcode);

template<uint8_t InstCC> requires (InstCC < 16)
bool EvalCond()
{
    if constexpr (InstCC == M_CC_T) { return true; }
    else if constexpr (InstCC == M_CC_F) { return false; }
    else {
        const uint8_t cc = SR & 0x0f;
        if constexpr (InstCC == M_CC_HI) { return (cc & (SR_Calt | SR_Z)) == 0; }
        else if constexpr (InstCC == M_CC_LS) { return (cc & (SR_Calt | SR_Z)) != 0; }
        else if constexpr (InstCC == M_CC_CC) { return (cc & SR_Calt) == 0; }
        else if constexpr (InstCC == M_CC_CS) { return (cc & SR_Calt) != 0; }
        else if constexpr (InstCC == M_CC_NE) { return (cc & SR_Z) == 0; }
        else if constexpr (InstCC == M_CC_EQ) { return (cc & SR_Z) != 0; }
        else if constexpr (InstCC == M_CC_VC) { return (cc & SR_Valt) == 0; }
        else if constexpr (InstCC == M_CC_VS) { return (cc & SR_Valt) != 0; }
        else if constexpr (InstCC == M_CC_PL) { return (cc & SR_N) == 0; }
        else if constexpr (InstCC == M_CC_MI) { return (cc & SR_N) != 0; }
        else if constexpr (InstCC == M_CC_GE) { return ((cc & SR_N) >> 3) == ((cc & SR_Valt) >> 1); }
        else if constexpr (InstCC == M_CC_LT) { return ((cc & SR_N) >> 3) != ((cc & SR_Valt) >> 1); }
        else if constexpr (InstCC == M_CC_GT) { return ((cc & SR_Z) == 0) && (((cc & SR_N) >> 3) == ((cc & SR_Valt) >> 1)); }
        else if constexpr (InstCC == M_CC_LE) { return ((cc & SR_Z) != 0) || (((cc & SR_N) >> 3) != ((cc & SR_Valt) >> 1)); }
    }
}

template<auto...> constexpr bool always_false = false;

template<uint8_t Mode, uint8_t Reg, class Type>
requires (Mode >= 2 && Mode < 8 && Reg < 8 && (sizeof(Type) == 1 || sizeof(Type) == 2 || sizeof(Type) == 4))
uint32_t GetEA()
{
    if constexpr (Mode == 2) {
        return getA<Reg, uint32_t>();
    }
    else if constexpr (Mode == 3) {
        uint32_t addr = getA<Reg, uint32_t>();
        uint32_t next = addr + sizeof(Type) + ((sizeof(Type) == 1 && Reg == 7) ? 1 : 0);
        setA<Reg, uint32_t>(next);
        return addr;
    }
    else if constexpr (Mode == 4) {
        uint32_t addr = getA<Reg, uint32_t>() - sizeof(Type) - ((sizeof(Type) == 1 && Reg == 7) ? 1 : 0);
        setA<Reg, uint32_t>(addr);
        return addr;
    }
    else if constexpr (Mode == 5) {
        uint32_t addr = getA<Reg, uint32_t>() + *(int16_t *)(uintptr_t)PC;
        PC += 2;
        return addr;
    }
    else if constexpr (Mode == 6) {
        uint32_t ea;
        GetEffectiveAddress(getA<Reg, uint32_t>(), sizeof(Type), &ea, Mode);
        return ea;
    }
    else if constexpr (Mode == 7) {
        if constexpr (Reg == 0) {
            uint32_t addr = (uint32_t)*(int16_t *)(uintptr_t)PC;
            PC += 2;
            return addr;
        }
        else if constexpr (Reg == 1) {
            uint32_t addr = *(uint32_t *)(uintptr_t)PC;
            PC += 4;
            return addr;
        }
        else if constexpr (Reg == 2) {
            int16_t off = *(int16_t *)(uintptr_t)PC;
            uint32_t addr = (uint32_t)(uintptr_t)PC + off;
            PC += 2;
            return addr;
        }
        else if constexpr (Reg == 3) {
            uint32_t ea;
            GetEffectiveAddress(PC, sizeof(Type), &ea, Mode);
            return ea;
        }
        else {
            static_assert(always_false<Reg>, "Mode 7 Reg 4-7 have no addressable EA (immediate/reserved)");
        }
    }
}

template<uint8_t Mode, uint8_t Reg, class Type>
requires (Mode < 8 && Reg < 8 && (sizeof(Type) == 1 || sizeof(Type) == 2 || sizeof(Type) == 4))
Type LoadFromEA()
{
    if constexpr (Mode == 0) {
        return getD<Reg, Type>();
    }
    else if constexpr (Mode == 1) {
        return getA<Reg, Type>();
    }
    else if constexpr (Mode == 7 && Reg == 4) {
        // immediate: lives inline in the instruction stream, not behind an EA
        if constexpr (sizeof(Type) == 2 || sizeof(Type) == 4) {
            Type val = *(Type *)(uintptr_t)PC;
            PC += sizeof(Type);
            return val;
        }
        else {
            Type val = (Type)*(uint16_t *)(uintptr_t)PC; // byte imm still occupies a full word
            PC += 2;
            return val;
        }
    }
    else {
        uint32_t addr = GetEA<Mode, Reg, Type>();
        return *(Type *)(uintptr_t)addr;
    }
}

template<uint8_t Mode, uint8_t Reg, class Type>
requires (Mode < 8 && Reg < 8 && !(Mode == 7 && Reg > 1) && (sizeof(Type) == 1 || sizeof(Type) == 2 || sizeof(Type) == 4))
void StoreToEA(Type value)
{
    if constexpr (Mode == 0) {
        setD<Reg, Type>(value);
    }
    else if constexpr (Mode == 1) {
        setA<Reg, Type>(value);
    }
    else {
        uint32_t addr = GetEA<Mode, Reg, Type>();
        *(Type *)(uintptr_t)addr = value;
    }
}

template<uint8_t Mode, uint8_t Reg, class Type, class Fn>
requires (Mode < 8 && Reg < 8 && !(Mode == 7 && Reg > 1) && (sizeof(Type) == 1 || sizeof(Type) == 2 || sizeof(Type) == 4))
void ReadModifyWriteEA(Fn&& modify)
{
    if constexpr (Mode == 0) {
        setD<Reg, Type>(modify(getD<Reg, Type>()));
    }
    else if constexpr (Mode == 1) {
        setA<Reg, Type>(modify(getA<Reg, Type>()));
    }
    else {
        uint32_t addr = GetEA<Mode, Reg, Type>();   // side effects (An update, PC advance) happen once, here
        Type old = *(Type *)(uintptr_t)addr;
        *(Type *)(uintptr_t)addr = modify(old);
    }
}

template<class Type, bool IsSub>
static inline std::pair<Type, uint8_t> Arith_WithFlags(Type a, Type b)
{
    constexpr int shift = (sizeof(Type) == 1) ? 24 : (sizeof(Type) == 2) ? 16 : 0;

    int32_t sa = (int32_t)a << shift;
    int32_t sb = (int32_t)b << shift;
    int32_t result;
    uint32_t nzcv;

    if constexpr (IsSub) {
        asm("subs %w0, %w2, %w3\n\t"
            "mrs  %x1, NZCV"
            : "=r"(result), "=r"(nzcv) : "r"(sa), "r"(sb) : "cc");
    } else {
        asm("adds %w0, %w2, %w3\n\t"
            "mrs  %x1, NZCV"
            : "=r"(result), "=r"(nzcv) : "r"(sa), "r"(sb) : "cc");
    }

    uint8_t ccr = (uint8_t)(nzcv >> 28);   // N,Z,C,V already in the right bit positions

    if constexpr (IsSub) ccr ^= SR_Calt;   // carry-polarity fix for subtraction

    Type rv = (Type)((uint32_t)result >> shift);
    return { rv, ccr };
}

template<class Type, bool IsSub>
static inline std::pair<Type, uint8_t> ArithX_WithFlags(Type a, Type b, uint8_t x_in)
{
    constexpr int shift = (sizeof(Type) == 1) ? 24 : (sizeof(Type) == 2) ? 16 : 0;

    int32_t sa = (int32_t)a << shift;
    int32_t sb = (int32_t)b << shift;
    int32_t result;
    uint32_t nzcv;
    uint8_t ccr;

    if constexpr (IsSub) {
        uint32_t seed = (uint32_t)(x_in ^ 1) << 29;   // ARM carry-in for SBC is "no borrow"
        asm volatile(
            "msr  NZCV, %x4\n\t"
            "sbcs %w0, %w2, %w3\n\t"
            "mrs  %x1, NZCV"
            : "=r"(result), "=r"(nzcv) : "r"(sa), "r"(sb), "r"(seed) : "cc");
        ccr = (uint8_t)(nzcv >> 28);
        ccr ^= SR_Calt;
    } else {
        uint32_t seed = (uint32_t)x_in << 29;
        asm volatile(
            "msr  NZCV, %x4\n\t"
            "adcs %w0, %w2, %w3\n\t"
            "mrs  %x1, NZCV"
            : "=r"(result), "=r"(nzcv) : "r"(sa), "r"(sb), "r"(seed) : "cc");
        ccr = (uint8_t)(nzcv >> 28);
    }

    Type rv = (Type)((uint32_t)result >> shift);
    return { rv, ccr };
}

} // Emu68::M68k::Interpreter

extern "C" {
#endif

void INTERPRET_line0(uint32_t opcode);
void INTERPRET_line1(uint32_t opcode);
void INTERPRET_line2(uint32_t opcode);
void INTERPRET_line3(uint32_t opcode);
void INTERPRET_line4(uint32_t opcode);
void INTERPRET_line5(uint32_t opcode);
void INTERPRET_line6(uint32_t opcode);
void INTERPRET_line7(uint32_t opcode);
void INTERPRET_line8(uint32_t opcode);
void INTERPRET_line9(uint32_t opcode);
void INTERPRET_lineA(uint32_t opcode);
void INTERPRET_lineB(uint32_t opcode);
void INTERPRET_lineC(uint32_t opcode);
void INTERPRET_lineD(uint32_t opcode);
void INTERPRET_lineE(uint32_t opcode);
void INTERPRET_lineF(uint32_t opcode);

#ifdef __cplusplus
}
#endif

#endif /* _EMU68_M68K_INTERPRETER_HPP_ */
