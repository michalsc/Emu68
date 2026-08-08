#pragma once

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

void exceptionF0(uint32_t exception);
void exceptionF1(uint32_t exception);
void exceptionF2(uint32_t exception, uint32_t ea);
void exceptionF3(uint32_t exception, uint32_t ea);
void exceptionF4(uint32_t exception, uint32_t ea, uint32_t pc);
void loadFromEffectiveAddress(uint8_t src_reg, uint8_t size, void *out, uint8_t mode);
void storeToEffectiveAddress(uint8_t dst_reg, uint32_t value, uint8_t size, uint8_t mode);
void getEffectiveAddress(uint8_t src_reg, uint8_t size, uint32_t *out, uint8_t mode);
void getExtendedEffectiveAddress(uint32_t An, uint8_t, uint32_t *out);
void handleChangedSR(uint32_t sr, uint32_t changed);
void ILLEGAL(uint32_t opcode);

enum class EAClass : uint8_t {
    Dn, An, Ind, IndPost, IndPre, D16An, D8AnXn,   // modes 0..6
    AbsW, AbsL, D16PC, D8PCXn, Imm                 // mode 7, reg 0..4
};

constexpr EAClass classifyEA(unsigned mode, unsigned reg) {
    if (mode < 7) return static_cast<EAClass>(mode);
    constexpr EAClass sub[5] = {
        EAClass::AbsW, EAClass::AbsL, EAClass::D16PC, EAClass::D8PCXn, EAClass::Imm
    };
    return sub[reg];
}

constexpr uint32_t classBit(EAClass c) { return 1u << static_cast<unsigned>(c); }

template<uint8_t InstCC> requires (InstCC < 16)
bool evalCond()
{
    if constexpr (InstCC == M_CC_T) { return true; }
    else if constexpr (InstCC == M_CC_F) { return false; }
    else {
        const uint8_t cc = SR & 0x0f;
        const bool c = (cc & SR_Calt) != 0;
        const bool z = (cc & SR_Z)    != 0;
        const bool v = (cc & SR_Valt) != 0;
        const bool n = (cc & SR_N)    != 0;

        if constexpr (InstCC == M_CC_HI)      { return !c && !z; }
        else if constexpr (InstCC == M_CC_LS) { return c || z; }
        else if constexpr (InstCC == M_CC_CC) { return !c; }
        else if constexpr (InstCC == M_CC_CS) { return c; }
        else if constexpr (InstCC == M_CC_NE) { return !z; }
        else if constexpr (InstCC == M_CC_EQ) { return z; }
        else if constexpr (InstCC == M_CC_VC) { return !v; }
        else if constexpr (InstCC == M_CC_VS) { return v; }
        else if constexpr (InstCC == M_CC_PL) { return !n; }
        else if constexpr (InstCC == M_CC_MI) { return n; }
        else if constexpr (InstCC == M_CC_GE) { return n == v; }
        else if constexpr (InstCC == M_CC_LT) { return n != v; }
        else if constexpr (InstCC == M_CC_GT) { return !z && (n == v); }
        else if constexpr (InstCC == M_CC_LE) { return z || (n != v); }
    }
}

template<uint8_t InstCC> requires (InstCC < 32)
bool evalCondFPU()
{
    if constexpr (InstCC == F_CC_T) { return true; }
    else if constexpr (InstCC == F_CC_ST) { return true; }
    else if constexpr (InstCC == F_CC_F) { return false; }
    else if constexpr (InstCC == F_CC_SF) { return false; }
    else {
        const uint32_t cc = FPSR;
        const bool n   = (cc & FPSRF_N)   != 0;
        const bool z   = (cc & FPSRF_Z)   != 0;
        const bool nan = (cc & FPSRF_NAN) != 0;

        if constexpr (InstCC == F_CC_EQ)        { return z; }
        else if constexpr (InstCC == F_CC_SEQ)  { return z; }
        else if constexpr (InstCC == F_CC_NE)   { return !z; }
        else if constexpr (InstCC == F_CC_SNE)  { return !z; }
        else if constexpr (InstCC == F_CC_OR)   { return !nan; }
        else if constexpr (InstCC == F_CC_UN)   { return nan; }
        else if constexpr (InstCC == F_CC_GT)   { return !nan && !z && !n; } 
        else if constexpr (InstCC == F_CC_NGT)  { return nan || z || n; }
        else if constexpr (InstCC == F_CC_OGT)  { return !nan && !z && !n; } 
        else if constexpr (InstCC == F_CC_ULE)  { return nan || z || n; }
        else if constexpr (InstCC == F_CC_GL)   { return !nan && !z; }
        else if constexpr (InstCC == F_CC_NGL)  { return nan || z; }
        else if constexpr (InstCC == F_CC_OGL)  { return !nan && !z; }
        else if constexpr (InstCC == F_CC_UEQ)  { return nan || z; }
        else if constexpr (InstCC == F_CC_GE)   { return z || (!nan && !n); }
        else if constexpr (InstCC == F_CC_NGE)  { return nan || (n && !z); }
        else if constexpr (InstCC == F_CC_LT)   { return !nan && n && !z; }
        else if constexpr (InstCC == F_CC_NLT)  { return nan || z || !n; }
        else if constexpr (InstCC == F_CC_LE)   { return !nan && (n || z); }
        else if constexpr (InstCC == F_CC_NLE)  { return nan || (!z && !n); }
        else if constexpr (InstCC == F_CC_GLE)  { return !nan; }
        else if constexpr (InstCC == F_CC_NGLE) { return nan; }
        else if constexpr (InstCC == F_CC_OGE)  { return z || (!nan && !n); }
        else if constexpr (InstCC == F_CC_ULT)  { return nan || (n && !z); }
        else if constexpr (InstCC == F_CC_OLT)  { return !nan && n && !z; }
        else if constexpr (InstCC == F_CC_UGE)  { return nan || z || !n; }
        else if constexpr (InstCC == F_CC_OLE)  { return !nan && (n || z); }
        else if constexpr (InstCC == F_CC_UGT)  { return nan || (!n && !z); }
    }
}

template<auto...> constexpr bool ALWAYS_FALSE = false;

inline constexpr uint8_t DEFAULT_EA = 255;

/* Few concepts for constraining the templates below */
template<uint8_t Mode> concept ValidMode  = Mode < 8;
template<uint8_t Mode> concept MemoryMode = Mode >= 2 && Mode < 8;  // excludes Dn/An direct
template<uint8_t Reg>  concept ValidReg   = Reg < 8;

template<class Type> concept IntEASize = sizeof(Type) == 1 || sizeof(Type) == 2 || sizeof(Type) == 4;
template<class Type> concept AnyEASize = IntEASize<Type> || sizeof(Type) == 8 || sizeof(Type) == 12;

template<uint8_t Mode, uint8_t Reg> concept SourceEA7 = !(Mode == 7 && Reg > 4); // imm/abs/pc-rel all valid
template<uint8_t Mode, uint8_t Reg> concept DestEA7   = !(Mode == 7 && Reg > 1); // abs.W/abs.L only

template<uint8_t Mode, uint8_t Reg, class Type>
concept ValidIntEA = ValidMode<Mode> && ValidReg<Reg> && IntEASize<Type>;

template<uint8_t Mode, uint8_t Reg, class Type>
requires MemoryMode<Mode> && ValidReg<Reg> && AnyEASize<Type>
uint32_t getEA()
{
    if constexpr (Mode == 2) {
        return getA<Reg, uint32_t>();
    } else if constexpr (Mode == 3) {
        uint32_t addr = getA<Reg, uint32_t>();
        uint32_t next = addr + sizeof(Type) + ((sizeof(Type) == 1 && Reg == 7) ? 1 : 0);
        setA<Reg, uint32_t>(next);
        return addr;
    } else if constexpr (Mode == 4) {
        uint32_t addr = getA<Reg, uint32_t>() - sizeof(Type) - ((sizeof(Type) == 1 && Reg == 7) ? 1 : 0);
        setA<Reg, uint32_t>(addr);
        return addr;
    } else if constexpr (Mode == 5) {
        uint32_t addr = getA<Reg, uint32_t>() + *(int16_t *)(uintptr_t)PC;
        PC += 2;
        return addr;
    } else if constexpr (Mode == 6) {
        uint32_t ea;
        getExtendedEffectiveAddress(getA<Reg, uint32_t>(), sizeof(Type), &ea);
        return ea;
    } else if constexpr (Mode == 7) {
        if constexpr (Reg == 0) {
            uint32_t addr = (uint32_t)*(int16_t *)(uintptr_t)PC;
            PC += 2;
            return addr;
        } else if constexpr (Reg == 1) {
            uint32_t addr = *(uint32_t *)(uintptr_t)PC;
            PC += 4;
            return addr;
        } else if constexpr (Reg == 2) {
            int16_t off = *(int16_t *)(uintptr_t)PC;
            uint32_t addr = (uint32_t)(uintptr_t)PC + off;
            PC += 2;
            return addr;
        } else if constexpr (Reg == 3) {
            uint32_t ea;
            getExtendedEffectiveAddress(PC, sizeof(Type), &ea);
            return ea;
        } else if constexpr (Reg == 4) {
            if (sizeof(Type) == 1) PC++;
            uint32_t addr = PC;
            PC += sizeof(Type);
            return addr;
        } else {
            static_assert(ALWAYS_FALSE<Reg>, "Mode 7 Reg 4-7 have no addressable EA (immediate/reserved)");
        }
    }
}

template<uint8_t Mode, uint8_t Reg, class Type>
requires ValidIntEA<Mode, Reg, Type> && SourceEA7<Mode, Reg>
Type loadFromEA()
{
    if constexpr (Mode == 0) {
        return getD<Reg, Type>();
    } else if constexpr (Mode == 1) {
        return getA<Reg, Type>();
    } else if constexpr (Mode == 7 && Reg == 4) {
        // immediate: lives inline in the instruction stream, not behind an EA
        if constexpr (sizeof(Type) == 2 || sizeof(Type) == 4) {
            Type val = *(Type *)(uintptr_t)PC;
            PC += sizeof(Type);
            return val;
        } else {
            Type val = (Type)*(uint16_t *)(uintptr_t)PC; // byte imm still occupies a full word
            PC += 2;
            return val;
        }
    } else {
        uint32_t addr = getEA<Mode, Reg, Type>();
        return *(Type *)(uintptr_t)addr;
    }
}

template<uint8_t Mode, uint8_t Reg, class Type>
requires ValidIntEA<Mode, Reg, Type> && DestEA7<Mode, Reg>
void storeToEA(Type value)
{
    if constexpr (Mode == 0) {
        setD<Reg, Type>(value);
    } else if constexpr (Mode == 1) {
        setA<Reg, Type>(value);
    } else {
        uint32_t addr = getEA<Mode, Reg, Type>();
        *(Type *)(uintptr_t)addr = value;
    }
}

template<uint8_t Mode, uint8_t Reg, class Type, class Fn>
requires ValidIntEA<Mode, Reg, Type> && DestEA7<Mode, Reg>
void readModifyWriteEA(Fn&& modify)
{
    if constexpr (Mode == 0) {
        setD<Reg, Type>(modify(getD<Reg, Type>()));
    } else if constexpr (Mode == 1) {
        setA<Reg, Type>(modify(getA<Reg, Type>()));
    } else {
        uint32_t addr = getEA<Mode, Reg, Type>();   // side effects (An update, PC advance) happen once, here
        Type old = *(Type *)(uintptr_t)addr;
        *(Type *)(uintptr_t)addr = modify(old);
    }
}

template<class Type, bool IsSub>
static inline std::pair<Type, uint8_t> arithWithFlags(Type a, Type b)
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

    if constexpr (IsSub) { 
        ccr ^= SR_Calt;   // carry-polarity fix for subtraction
    }

    Type rv = (Type)((uint32_t)result >> shift);
    return { rv, ccr };
}

template<class Type, bool IsSub>
static inline std::pair<Type, uint8_t> arithXWithFlags(Type a, Type b, uint8_t x_in)
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

template<class Type>
static inline std::pair<Type, uint8_t> aslWithFlags(Type value, int count)
{
    constexpr int W = sizeof(Type) * 8;

    if (count == 0) {
        uint8_t ccr = 0;
        if (value == 0) { ccr |= SR_Z; }
        if (value < 0)  { ccr |= SR_N; }
        return { value, ccr };
    }

    int64_t  v64  = value;
    uint64_t uv64 = (uint64_t)v64;

    Type result = (Type)(uv64 << count);
    uint8_t carry = (uint8_t)(((uv64 << (count - 1)) >> (W - 1)) & 1);

    int64_t  t64 = v64 >> count;
    uint64_t x64 = uv64 ^ (uint64_t)t64;
    int win = (W - count - 1 > 0) ? (W - count - 1) : 0;
    bool overflow = (x64 >> win) != 0;

    uint8_t ccr = 0;
    if (result == 0) { ccr |= SR_Z; }
    if (result < 0)  { ccr |= SR_N; }
    if (overflow)    { ccr |= SR_Valt; }
    if (carry)       { ccr |= SR_Calt; }
    return { result, ccr };
}

template<class Type>
static inline std::pair<Type, uint8_t> asrWithFlags(Type value, int count)
{
    if (count == 0) {
        uint8_t ccr = 0;
        if (value == 0) { ccr |= SR_Z; }
        if (value < 0)  { ccr |= SR_N; }
        return { value, ccr };
    }

    int64_t  v64  = value;
    uint64_t uv64 = (uint64_t)v64;

    Type result = (Type)(v64 >> count);
    uint8_t carry = (uint8_t)((uv64 >> (count - 1)) & 1);

    uint8_t ccr = 0;
    if (result == 0) { ccr |= SR_Z; }
    if (result < 0)  { ccr |= SR_N; }
    // V is always 0 for ASR
    if (carry) { ccr |= SR_Calt; }
    return { result, ccr };
}

template<class Type>
static inline std::pair<Type, uint8_t> lslWithFlags(Type value, int count)
{
    constexpr int W = sizeof(Type) * 8;
    using UT = std::make_unsigned_t<Type>;

    if (count == 0) {
        uint8_t ccr = 0;
        if (value == 0) { ccr |= SR_Z; }
        if (value < 0)  { ccr |= SR_N; }
        return { value, ccr };
    }

    uint64_t uv64 = (uint64_t)(UT)value;
    Type result = (Type)(uv64 << count);
    uint8_t carry = (uint8_t)(((uv64 << (count - 1)) >> (W - 1)) & 1);

    uint8_t ccr = 0;
    if (result == 0) { ccr |= SR_Z; }
    if (result < 0)  { ccr |= SR_N; }
    // V is always 0 for LSL
    if (carry) { ccr |= SR_C; }
    return { result, ccr };
}

template<class Type>
static inline std::pair<Type, uint8_t> lsrWithFlags(Type value, int count)
{
    using UT = std::make_unsigned_t<Type>;

    if (count == 0) {
        uint8_t ccr = 0;
        if (value == 0) { ccr |= SR_Z; }
        if (value < 0)  { ccr |= SR_N; }
        return { value, ccr };
    }

    uint64_t uv64 = (uint64_t)(UT)value;              // zero-extend, not sign-extend
    Type result = (Type)(uv64 >> count);               // logical shift, self-saturates to 0
    uint8_t carry = (uint8_t)((uv64 >> (count - 1)) & 1);

    uint8_t ccr = 0;
    if (result == 0) { ccr |= SR_Z; }
    if (result < 0)  { ccr |= SR_N; }  // provably always false for count >= 1, harmless to leave uniform
    // V is always 0 for LSR
    if (carry) { ccr |= SR_C; }
    return { result, ccr };
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
