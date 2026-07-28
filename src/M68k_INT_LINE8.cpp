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

template<uint8_t Mode, uint8_t Reg, uint8_t Dn>
void DIVU_W(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    uint16_t src;
    uint32_t orig_PC = PC;

    PC += 2;
    src = LoadFromEA<Mode, Reg, uint16_t>();

    if (src == 0) {
        Exception_F2(VECTOR_DIVIDE_BY_ZERO, orig_PC);
    } else {
        uint32_t dval = getD<Dn, uint32_t>();
        uint32_t drem;

        dval = dval / (uint32_t)src;
        drem = dval % (uint32_t)src;

        if (dval > 0xffff) {
            SR = (SR & ~SR_Calt) | SR_Valt;
            return;
        }

        setD<Dn, uint32_t>((drem << 16) | dval);

        if ((int16_t)dval == 0) {
            sr |= SR_Z;
        } else if ((int16_t)dval < 0) {
            sr |= SR_N;
        }

        SR = sr;
    }
}


#define FILL_ALL_RD_EAs_no_An(base_offset, name) \
    [&]<std::size_t... Dreg>(int base, std::index_sequence<Dreg...>) { \
        ((table[base + EA(0, Dreg)] = \
            name<0, Dreg>), ...); \
    }((base_offset), std::make_index_sequence<8>{}); \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7)] = \
             name<2 + (Is >> 3), Is & 7>), ...); \
    }((base_offset), std::make_index_sequence<45>{});

#define FILL_ALL_RD_EAs_no_An_reg(base_offset, name, reg) \
    [&]<std::size_t... Dreg>(int base, std::index_sequence<Dreg...>) { \
        ((table[base + EA(0, Dreg) + (reg << 9)] = \
            name<0, Dreg, reg>), ...); \
    }((base_offset), std::make_index_sequence<8>{}); \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7) + (reg << 9)] = \
             name<2 + (Is >> 3), Is & 7, reg>), ...); \
    }((base_offset), std::make_index_sequence<45>{});


static constexpr std::array<INTERPRET_Function, 4096> BuildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};

    auto fill = [&table](int first, int last, INTERPRET_Function func) {
        for (int i = first; i <= last; ++i)
            table[i] = func;
    };

    auto EA = [](int mode, int reg) constexpr { return (mode << 3) | reg; };

    fill(00000, 07777, UNIMPLEMENTED);

    FILL_ALL_RD_EAs_no_An_reg(  00300,  DIVU_W, 0);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  DIVU_W, 1);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  DIVU_W, 2);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  DIVU_W, 3);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  DIVU_W, 4);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  DIVU_W, 5);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  DIVU_W, 6);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  DIVU_W, 7);

    return table;
}

} // Emu68::M68k::Interpreter

static constexpr auto InsnTable = Emu68::M68k::Interpreter::BuildInsnTable();

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line8(uint32_t opcode)
{
    InsnTable[opcode & 0xfff](opcode);
}

