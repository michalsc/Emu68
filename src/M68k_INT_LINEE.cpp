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

namespace Emu68::M68k::Interpreter {

template<uint8_t Mode, uint8_t Reg>
void ROR_EA(uint32_t)
{
    PC += 2;
    ReadModifyWriteEA<Mode, Reg, uint16_t>([&](uint16_t v) -> uint16_t {
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
    ReadModifyWriteEA<Mode, Reg, uint16_t>([&](uint16_t v) -> uint16_t {
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
    }
    else {
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
    }
    else {
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

#define FILL_MOD2_to_72(base_offset, name) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7)] = \
            name<2 + (Is >> 3), Is & 7>), ...); \
    }((base_offset), std::make_index_sequence<42>{});

#define FILL_REG_CNT(base_offset, name, type) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + (Is & 7) + ((Is >> 3) << 9)] = \
            name<Is & 7, type>), ...); \
    }((base_offset), std::make_index_sequence<64>{});


static constexpr std::array<INTERPRET_Function, 4096> BuildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};

    auto fill = [&table](int first, int last, INTERPRET_Function func) {
        for (int i = first; i <= last; ++i)
            table[i] = func;
    };

    auto EA = [](int mode, int reg) constexpr { return (mode << 3) | reg; };

    fill(00000, 07777, UNIMPLEMENTED);

    FILL_MOD2_to_72(    03300,  ROR_EA);
    FILL_MOD2_to_72(    03700,  ROL_EA);

    FILL_REG_CNT(       00030,  ROR_IMM, uint8_t);
    FILL_REG_CNT(       00130,  ROR_IMM, uint16_t);
    FILL_REG_CNT(       00230,  ROR_IMM, uint32_t);
    FILL_REG_CNT(       00430,  ROL_IMM, uint8_t);
    FILL_REG_CNT(       00530,  ROL_IMM, uint16_t);
    FILL_REG_CNT(       00630,  ROL_IMM, uint32_t);

    return table;
}

} // Emu68::M68k::Interpreter

static constexpr auto InsnTable = Emu68::M68k::Interpreter::BuildInsnTable();

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_lineE(uint32_t opcode)
{
    InsnTable[opcode & 0xfff](opcode);
}

