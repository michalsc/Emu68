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

void M68K_LoadContext(struct M68KState* ctx);
void M68K_SaveContext(struct M68KState* ctx);

}
#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter {

/* Few concepts for constraining the templates below */
template<uint8_t Mode> concept ValidMode  = Mode < 8;
template<uint8_t Mode> concept MemoryMode = Mode >= 2 && Mode < 8;  // excludes Dn/An direct
template<uint8_t Mode> concept PostIncMode = Mode == 3;
template<uint8_t Mode> concept PreDecMode = Mode == 4;
template<uint8_t Reg>  concept ValidReg   = Reg < 8;
template<uint8_t Mode> concept DRegMode   = Mode == 0;
template<uint8_t Mode> concept ARegMode   = Mode == 1;

template<class Type> concept IntEASize = sizeof(Type) == 1 || sizeof(Type) == 2 || sizeof(Type) == 4;
template<class Type> concept AnyEASize = IntEASize<Type> || sizeof(Type) == 8 || sizeof(Type) == 12;

template<uint8_t Mode, uint8_t Reg> concept SourceEA7 = !(Mode == 7 && Reg > 4); // imm/abs/pc-rel all valid
template<uint8_t Mode, uint8_t Reg> concept DestEA7   = !(Mode == 7 && Reg > 1); // abs.W/abs.L only

template<uint8_t Mode, uint8_t Reg, class Type>
concept ValidIntEA = ValidMode<Mode> && ValidReg<Reg> && IntEASize<Type>;

template<uint8_t Mode, class Type>
concept ByteCompatibleMode = (Mode != 1) || sizeof(Type) > 1;

template<class Type> concept WordOrLongSize = sizeof(Type) == 2 || sizeof(Type) == 4;

void bug(const char * format, ...);

void raiseException(uint32_t exception, ExceptionFrameFormat format, uint32_t ea, uint32_t pc);

template<class Type>
Type loadFromEA(uint32_t mode, uint32_t reg);

template<class Type>
uint32_t getEA(uint32_t mode, uint32_t reg);

template<class Type>
uint32_t getExtendedEA(uint32_t reg);

template<class Type>
void storeToEA(uint32_t mode, uint32_t reg, Type value);

void handleChangedSR(uint32_t sr, uint32_t changed);
void ILLEGAL(uint32_t opcode);

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

template<uint8_t Mode, uint8_t Reg, class Type>
requires MemoryMode<Mode> && ValidReg<Reg> && (AnyEASize<Type> || std::is_same<Type, void>::value)
uint32_t getEA()
{
    if constexpr (Mode == 2) {
        return getA<Reg, uint32_t>();
    } else if constexpr (Mode == 3) {
        uint32_t addr = getA<Reg, uint32_t>();
        if constexpr (!std::is_same<Type, void>::value) {
            uint32_t next = addr + sizeof(Type) + ((sizeof(Type) == 1 && Reg == 7) ? 1 : 0);
            setA<Reg, uint32_t>(next);
        }
        return addr;
    } else if constexpr (Mode == 4) {
        if constexpr (!std::is_same<Type, void>::value) {
            uint32_t addr = getA<Reg, uint32_t>() - sizeof(Type) - ((sizeof(Type) == 1 && Reg == 7) ? 1 : 0);
            setA<Reg, uint32_t>(addr);
            return addr;
        } else {
            return getA<Reg, uint32_t>();
        }
    } else if constexpr (Mode == 5) {
        uint32_t addr = getA<Reg, uint32_t>() + *(int16_t *)(uintptr_t)PC;
        PC += 2;
        return addr;
    } else if constexpr (Mode == 6) {
        return getExtendedEA<Type>(getA<Reg, uint32_t>());
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
            return getExtendedEA<Type>(PC);
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
    } else if constexpr (Mode == 1 && WordOrLongSize<Type>) {
        setA<Reg, Type>(modify(getA<Reg, Type>()));
    } else {
        uint32_t addr = getEA<Mode, Reg, Type>();   // side effects (An update, PC advance) happen once, here
        Type old = *(Type *)(uintptr_t)addr;
        *(Type *)(uintptr_t)addr = modify(old);
    }
}

template<class Type, class Fn>
void readModifyWriteEA(uint32_t mode, uint32_t reg, Fn&& modify)
{
    if (mode == 0) {
        setD<Type>(reg, modify(getD<Type>(reg)));
    } else if (mode == 1 && WordOrLongSize<Type>) {
        setA<Type>(reg, modify(getA<Type>(reg)));
    } else {
        uint32_t addr = getEA<Type>(mode, reg);   // side effects happen once, here
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
    if (carry) { ccr |= SR_Calt; }
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
    if (carry) { ccr |= SR_Calt; }
    return { result, ccr };
}

// Specializations for templates

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

template <unsigned BitOffset, unsigned Width>
struct FieldBase {
    static constexpr unsigned bitOffset = BitOffset;
    static constexpr unsigned size      = 1u << Width;
};

// Register operand (Dn/An/Rx/Ry). HotMask bit v => register v is specialized;
// otherwise the handler decodes it from the opcode at runtime via DEFAULT_EA.
template <unsigned BitOffset, unsigned Width = 3, uint64_t HotMask = -1ULL>
struct RegField : FieldBase<BitOffset, Width> {
    static constexpr bool valid(unsigned)   { return true; }
    static constexpr bool hot(unsigned v)   { return (HotMask >> v) & 1u; }
    static constexpr uint8_t arg(unsigned v) { return hot(v) ? uint8_t(v) : DEFAULT_EA; }
};



// A field consumed by the handler at runtime (immediate data, displacement,
// etc.) that never influences which function handles the opcode. Every
// value maps to the same already-selected handler; it contributes no
// template arguments, it just widens the fill loop so the "don't care"
// combinations get a table entry too.
template <unsigned BitOffset, unsigned Width>
struct ImmField : FieldBase<BitOffset, Width> {
    static constexpr bool valid(unsigned) { return true; }
};

template <typename F>
void safeCall(F&& func) {
    M68KState *ctx;
    __asm__ volatile("mov %0, " CTX_POINTER_ASM:"=r"(ctx));
    M68K_SaveContext(ctx);
    func();
    M68K_LoadContext(ctx);
}

} // Emu68::M68k::Interpreter

extern "C" {
#endif

#ifdef __cplusplus
}
#endif
