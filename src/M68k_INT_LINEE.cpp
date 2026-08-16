#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter>

#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter::LineE {

template<uint8_t Mode, uint8_t Reg>
void ROR_EA(uint32_t)
{
    PC += 2;
    readModifyWriteEA<Mode, Reg, uint16_t>([&](uint16_t v) -> uint16_t {
        uint32_t sr = SR & ~SR_NZVC;
        
        v = (v >> 1) | (v << 15);
        
        if (v == 0) {
            sr |= SR_Z;
        } else if ((int16_t)v < 0) {
            sr |= SR_N;
            sr |= SR_Calt;
        }
        SR = sr;

        return v;
    });
}

template<uint8_t Mode, uint8_t Reg>
void ROL_EA(uint32_t)
{
    PC += 2;
    readModifyWriteEA<Mode, Reg, uint16_t>([&](uint16_t v) -> uint16_t {
        uint32_t sr = SR & ~SR_NZVC;
        
        v = (v << 1) | (v >> 15);
        
        if (v == 0) {
            sr |= SR_Z;
        } else if ((int16_t)v < 0) {
            sr |= SR_N;
        }
        if (v & 1) {
            sr |= SR_Calt;
        }
        SR = sr;

        return v;
    });
}

template<uint8_t Dn, class Type>
void ROR_IMM(uint32_t opcode)
{
    uint32_t sr = SR & ~SR_NZVC;
    uint32_t count = (opcode >> 9) & 7;
    constexpr uint32_t bitcount = sizeof(Type) * 8;

    if (count == 0) count = 8;

    Type v = getD<Dn, Type>();

    if (v == 0) {
        sr |= SR_Z;
    } else {
        v = (v >> count) | (v << (bitcount - count));

        if (v & (1 << (bitcount - 1))) {
            sr |= SR_N | SR_Calt;
        }

        setD<Dn, Type>(v);
    }

    SR = sr;
    PC += 2;
}


template<uint8_t Dn, class Type>
void ROL_IMM(uint32_t opcode)
{
    uint32_t sr = SR & ~SR_NZVC;
    uint32_t count = (opcode >> 9) & 7;
    constexpr uint32_t bitcount = sizeof(Type) * 8;

    if (count == 0) count = 8;

    Type v = getD<Dn, Type>();

    if (v == 0) {
        sr |= SR_Z;
    } else {
        v = (v << count) | (v >> (bitcount - count));

        if (v & (1 << (bitcount - 1))) {
            sr |= SR_N;
        }
        if (v & 1) {
            sr |= SR_Calt;
        }

        setD<Dn, Type>(v);
    }

    SR = sr;
    PC += 2;
}

template<bool Left, bool RegCount, uint8_t CountOrReg, uint8_t Reg, class Type>
void ASx_Reg(uint32_t)
{
    PC += 2;
    Type v = getD<Reg, Type>();

    int count;
    if constexpr (RegCount) { count = getD<CountOrReg, uint32_t>() & 63; }
    else                    { count = (CountOrReg == 0) ? 8 : CountOrReg; }

    auto [result, ccr] = [&] {
        if constexpr (Left) { return aslWithFlags<Type>(v, count); }
        else                { return asrWithFlags<Type>(v, count); }
    }();

    if constexpr (!RegCount) {
        // immediate count is always 1..8 -- X always updated, same shape as ADD
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
    } else {
        uint16_t mask = SR_NZVC | (count != 0 ? SR_X : 0);
        uint16_t bits = ccr | ((count != 0 && (ccr & SR_Calt)) ? SR_X : 0);
        SR = (SR & ~mask) | bits;
    }

    setD<Reg, Type>(result);
}

template<uint8_t Mode, uint8_t Reg, bool Left, class Type>
void ASx_Mem(uint32_t)
{
    PC += 2;
    readModifyWriteEA<Mode, Reg, Type>([&](Type v) -> Type {
        auto [result, ccr] = Left ? aslWithFlags<Type>(v, 1) : asrWithFlags<Type>(v, 1);
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
        return result;
    });
}


template<bool Left, bool RegCount, uint8_t CountOrReg, uint8_t Reg, class Type>
void LSx_Reg(uint32_t)
{
    PC += 2;
    Type v = getD<Reg, Type>();

    int count;
    if constexpr (RegCount) { count = getD<CountOrReg, uint32_t>() & 63; }
    else                    { count = (CountOrReg == 0) ? 8 : CountOrReg; }

    auto [result, ccr] = [&] {
        if constexpr (Left) { return lslWithFlags<Type>(v, count); }
        else                { return lsrWithFlags<Type>(v, count); }
    }();

    if constexpr (!RegCount) {
        // immediate count is always 1..8 -- X always updated, same shape as ADD
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
    } else {
        uint16_t mask = SR_NZVC | (count != 0 ? SR_X : 0);
        uint16_t bits = ccr | ((count != 0 && (ccr & SR_Calt)) ? SR_X : 0);
        SR = (SR & ~mask) | bits;
    }

    setD<Reg, Type>(result);
}

template<uint8_t Mode, uint8_t Reg, bool Left, class Type>
void LSx_Mem(uint32_t)
{
    PC += 2;
    readModifyWriteEA<Mode, Reg, Type>([&](Type v) -> Type {
        auto [result, ccr] = Left ? lslWithFlags<Type>(v, 1) : lsrWithFlags<Type>(v, 1);
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
        return result;
    });
}

template<class Type>
static inline constexpr std::pair<Type, bool> roxRotate(Type v, uint32_t count, bool x_in, bool left)
{
    constexpr uint32_t bitcount = sizeof(Type) * 8;
    constexpr uint32_t ringbits = bitcount + 1;
    constexpr uint64_t valmask = (uint64_t(1) << bitcount) - 1;
    constexpr uint64_t ringmask = (uint64_t(1) << ringbits) - 1;
 
    uint64_t ring = (uint64_t(x_in) << bitcount) | v;
    uint32_t c = count % ringbits;
 
    uint64_t rotated = ring;
    if (c != 0) {
        rotated = left
            ? ((ring << c) | (ring >> (ringbits - c))) & ringmask
            : ((ring >> c) | (ring << (ringbits - c))) & ringmask;
    }
 
    bool new_x = (rotated >> bitcount) & 1;
    Type result = static_cast<Type>(rotated & valmask);
    return { result, new_x };
}

template<class Type>
static inline constexpr std::pair<Type, uint32_t> roxWithFlags(Type v, uint32_t count, bool x_in, bool left)
{
    constexpr uint32_t bitcount = sizeof(Type) * 8;
    auto [result, new_x] = roxRotate<Type>(v, count, x_in, left);
 
    uint32_t ccr = 0;
    if (result == 0) {
        ccr |= SR_Z;
    } else if (result & (Type(1) << (bitcount - 1))) {
        ccr |= SR_N;
    }
    if (new_x) {
        ccr |= SR_Calt;
    }
 
    return { result, ccr };
}

template<bool Left, bool RegCount, uint8_t CountOrReg, uint8_t Reg, class Type>
void ROXx_Reg(uint32_t)
{
    PC += 2;
    Type v = getD<Reg, Type>();
    bool x_in = (SR & SR_X) != 0;
 
    uint32_t count;
    if constexpr (RegCount) { count = getD<CountOrReg, uint32_t>() & 63; }
    else                    { count = (CountOrReg == 0) ? 8 : CountOrReg; }
 
    auto [result, ccr] = roxWithFlags<Type>(v, count, x_in, Left);
 
    if constexpr (!RegCount) {
        // immediate count is always 1..8 -- X always updated, same shape as ASx_Reg/LSx_Reg
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
    } else {
        uint32_t mask = SR_NZVC | (count != 0 ? SR_X : 0);
        uint32_t bits = ccr | ((count != 0 && (ccr & SR_Calt)) ? SR_X : 0);
        SR = (SR & ~mask) | bits;
    }
 
    setD<Reg, Type>(result);
}

template<uint8_t Mode, uint8_t Reg>
void ROXR_Mem(uint32_t)
{
    PC += 2;
    readModifyWriteEA<Mode, Reg, uint16_t>([&](uint16_t v) -> uint16_t {
        bool x_in = (SR & SR_X) != 0;
        auto [result, ccr] = roxWithFlags<uint16_t>(v, 1, x_in, false);
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
        return result;
    });
}

template<uint8_t Mode, uint8_t Reg>
void ROXL_Mem(uint32_t)
{
    PC += 2;
    readModifyWriteEA<Mode, Reg, uint16_t>([&](uint16_t v) -> uint16_t {
        bool x_in = (SR & SR_X) != 0;
        auto [result, ccr] = roxWithFlags<uint16_t>(v, 1, x_in, true);
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
        return result;
    });
}

#define FILL_MOD2_to_72(base_offset, name) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7)] = \
            name<2 + (Is >> 3), Is & 7>), ...); \
    }((base_offset), std::make_index_sequence<42>{});

#define FILL_MOD2_to_72_sz(base_offset, name, opt, size) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7)] = \
            name<2 + (Is >> 3), Is & 7, opt, size>), ...); \
    }((base_offset), std::make_index_sequence<42>{});

#define FILL_REG_CNT(base_offset, name, type) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + (Is & 7) + ((Is >> 3) << 9)] = \
            name<Is & 7, type>), ...); \
    }((base_offset), std::make_index_sequence<64>{});

#define FILL_SHIFT_REG(base_offset, name, left, regcount, size) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + ((Is >> 3) << 9) + (Is & 7)] = \
             name<left, regcount, (Is >> 3), Is & 7, size>), ...); \
    }((base_offset), std::make_index_sequence<64>{});


static constexpr std::array<INTERPRET_Function, 4096> buildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};

    auto fill = [&table](int first, int last, INTERPRET_Function func) {
        for (int i = first; i <= last; ++i) {
            table[i] = func;
        }
    };

    auto EA = [](int mode, int reg) constexpr { return (mode << 3) | reg; };

    fill(00000, 07777, ILLEGAL);

    FILL_MOD2_to_72(    03300,  ROR_EA);
    FILL_MOD2_to_72(    03700,  ROL_EA);

    FILL_REG_CNT(       00030,  ROR_IMM, uint8_t);
    FILL_REG_CNT(       00130,  ROR_IMM, uint16_t);
    FILL_REG_CNT(       00230,  ROR_IMM, uint32_t);
    FILL_REG_CNT(       00430,  ROL_IMM, uint8_t);
    FILL_REG_CNT(       00530,  ROL_IMM, uint16_t);
    FILL_REG_CNT(       00630,  ROL_IMM, uint32_t);

    FILL_SHIFT_REG(     00000,  ASx_Reg, false, false, BYTE);   // ASR.B #n,Dn
    FILL_SHIFT_REG(     00040,  ASx_Reg, false, true,  BYTE);   // ASR.B Dn,Dn
    FILL_SHIFT_REG(     00100,  ASx_Reg, false, false, WORD);
    FILL_SHIFT_REG(     00140,  ASx_Reg, false, true,  WORD);
    FILL_SHIFT_REG(     00200,  ASx_Reg, false, false, LONG);
    FILL_SHIFT_REG(     00240,  ASx_Reg, false, true,  LONG);
    FILL_SHIFT_REG(     00400,  ASx_Reg, true,  false, BYTE);   // ASL.B #n,Dn
    FILL_SHIFT_REG(     00440,  ASx_Reg, true,  true,  BYTE);
    FILL_SHIFT_REG(     00500,  ASx_Reg, true,  false, WORD);
    FILL_SHIFT_REG(     00540,  ASx_Reg, true,  true,  WORD);
    FILL_SHIFT_REG(     00600,  ASx_Reg, true,  false, LONG);
    FILL_SHIFT_REG(     00640,  ASx_Reg, true,  true,  LONG);

    FILL_MOD2_to_72_sz( 00300,  ASx_Mem, false, WORD);   // ASR <ea>
    FILL_MOD2_to_72_sz( 00700,  ASx_Mem, true,  WORD);   // ASL <ea>

    FILL_SHIFT_REG(     00010,  LSx_Reg, false, false, BYTE);   // ASR.B #n,Dn
    FILL_SHIFT_REG(     00050,  LSx_Reg, false, true,  BYTE);   // ASR.B Dn,Dn
    FILL_SHIFT_REG(     00110,  LSx_Reg, false, false, WORD);
    FILL_SHIFT_REG(     00150,  LSx_Reg, false, true,  WORD);
    FILL_SHIFT_REG(     00210,  LSx_Reg, false, false, LONG);
    FILL_SHIFT_REG(     00250,  LSx_Reg, false, true,  LONG);
    FILL_SHIFT_REG(     00410,  LSx_Reg, true,  false, BYTE);   // ASL.B #n,Dn
    FILL_SHIFT_REG(     00450,  LSx_Reg, true,  true,  BYTE);
    FILL_SHIFT_REG(     00510,  LSx_Reg, true,  false, WORD);
    FILL_SHIFT_REG(     00550,  LSx_Reg, true,  true,  WORD);
    FILL_SHIFT_REG(     00610,  LSx_Reg, true,  false, LONG);
    FILL_SHIFT_REG(     00650,  LSx_Reg, true,  true,  LONG);

    FILL_MOD2_to_72_sz( 01300,  LSx_Mem, false, WORD);          // ASR <ea>
    FILL_MOD2_to_72_sz( 01700,  LSx_Mem, true,  WORD);          // ASL <ea>

    FILL_SHIFT_REG(     00020,  ROXx_Reg, false, false, BYTE);  // ROXR.B #n,Dn
    FILL_SHIFT_REG(     00060,  ROXx_Reg, false, true,  BYTE);  // ROXR.B Dn,Dn
    FILL_SHIFT_REG(     00120,  ROXx_Reg, false, false, WORD);
    FILL_SHIFT_REG(     00160,  ROXx_Reg, false, true,  WORD);
    FILL_SHIFT_REG(     00220,  ROXx_Reg, false, false, LONG);
    FILL_SHIFT_REG(     00260,  ROXx_Reg, false, true,  LONG);
    FILL_SHIFT_REG(     00420,  ROXx_Reg, true,  false, BYTE);  // ROXL.B #n,Dn
    FILL_SHIFT_REG(     00460,  ROXx_Reg, true,  true,  BYTE);
    FILL_SHIFT_REG(     00520,  ROXx_Reg, true,  false, WORD);
    FILL_SHIFT_REG(     00560,  ROXx_Reg, true,  true,  WORD);
    FILL_SHIFT_REG(     00620,  ROXx_Reg, true,  false, LONG);
    FILL_SHIFT_REG(     00660,  ROXx_Reg, true,  true,  LONG);

    FILL_MOD2_to_72(    02300,  ROXR_Mem);
    FILL_MOD2_to_72(    02700,  ROXL_Mem);

    return table;
}

constexpr auto InsnTable __attribute__((used,aligned(4096),section(".int.jumptable.e"))) = buildInsnTable();

} // Emu68::M68k::Interpreter::LineE
