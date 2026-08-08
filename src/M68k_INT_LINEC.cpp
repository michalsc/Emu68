#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter.hpp>

#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter {


template<uint8_t Mode, uint8_t Reg, uint8_t Dn>
void MULU_W(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    uint16_t src;

    PC += 2;
    src = loadFromEA<Mode, Reg, uint16_t>();

    uint32_t dval = getD<Dn, uint16_t>() * src;

    setD<Dn, uint32_t>(dval);

    if ((int32_t)dval == 0) {
        sr |= SR_Z;
    } else if ((int32_t)dval < 0) {
        sr |= SR_N;
    }

    SR = sr;
}

template<uint8_t Mode, uint8_t Reg, uint8_t Dn>
void MULS_W(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    int16_t src;

    PC += 2;
    src = loadFromEA<Mode, Reg, int16_t>();

    int32_t dval = getD<Dn, int16_t>() * src;

    setD<Dn, uint32_t>(dval);

    if (dval == 0) {
        sr |= SR_Z;
    } else if (dval < 0) {
        sr |= SR_N;
    }

    SR = sr;
}

template<uint8_t Mode, uint8_t Reg, uint8_t Dn, class Type>
requires (Mode > 1)
void AND_Dn_to_EA(uint32_t)
{
    Type dval = getD<Dn, Type>();
    PC = PC + 2;

    readModifyWriteEA<Mode, Reg, Type>([&](Type v) -> Type {
        uint32_t sr = SR & ~SR_NZVC;

        v &= dval;

        if (v == 0) {
            sr |= SR_Z;
        } else if (v < 0) {
            sr |= SR_N;
        }
        SR = sr;

        return v;
    });
}

template<uint8_t Mode, uint8_t Reg, uint8_t Dn, class Type>
requires (Mode != 1 || (Mode == 1 && sizeof(Type) > 1))
void AND_EA_to_Dn(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    PC = PC + 2;
    Type val = loadFromEA<Mode, Reg, Type>();
    Type dval = getD<Dn, Type>();

    dval &= val;

    if (dval == 0) {
        sr |= SR_Z;
    } else if (dval < 0) {
        sr |= SR_N;
    }
    SR = sr;

    setD<Dn, Type>(dval);
}

template<uint8_t Rx, bool RxIsA, uint8_t Ry, bool RyIsA>
requires (Rx < 8 && Ry < 8)
void EXG(uint32_t)
{
    PC = PC + 2;
    uint32_t tmp;

    if constexpr (RxIsA && RyIsA) {
        tmp = getA<Rx, uint32_t>();
        setA<Rx, uint32_t>(getA<Ry, uint32_t>());
        setA<Ry, uint32_t>(tmp);
    } else if constexpr(!RxIsA && !RyIsA) {
        tmp = getD<Rx, uint32_t>();
        setD<Rx, uint32_t>(getD<Ry, uint32_t>());
        setD<Ry, uint32_t>(tmp);
    } else if constexpr(!RxIsA && RyIsA) {
        tmp = getD<Rx, uint32_t>();
        setD<Rx, uint32_t>(getA<Ry, uint32_t>());
        setA<Ry, uint32_t>(tmp);
    } else static_assert(ALWAYS_FALSE<Rx>, "RegX must be Dn for EXG.L An, Dn");
}

#define FILL_ALL_RD_EAs(base_offset, name, reg, size) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA((Is >> 3), Is & 7)] = \
             name<(Is >> 3), Is & 7, reg, size>), ...); \
    }((base_offset), std::make_index_sequence<61>{});

#define FILL_ALL_RD_EAs_no_An(base_offset, name, reg, size) \
    [&]<std::size_t... Dreg>(int base, std::index_sequence<Dreg...>) { \
        ((table[base + EA(0, Dreg)] = \
            name<0, Dreg, reg, size>), ...); \
    }((base_offset), std::make_index_sequence<8>{}); \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7)] = \
             name<2 + (Is >> 3), Is & 7, reg, size>), ...); \
    }((base_offset), std::make_index_sequence<45>{});

#define FILL_ALL_WR_EAs(base_offset, name, reg, size) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7)] = \
             name<2 + (Is >> 3), Is & 7, reg, size>), ...); \
    }((base_offset), std::make_index_sequence<42>{});

#define FILL_ALL_RD_EAs_no_An_reg(base_offset, name, reg) \
    [&]<std::size_t... Dreg>(int base, std::index_sequence<Dreg...>) { \
        ((table[base + EA(0, Dreg) + (reg << 9)] = \
            name<0, Dreg, reg>), ...); \
    }((base_offset), std::make_index_sequence<8>{}); \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7) + (reg << 9)] = \
             name<2 + (Is >> 3), Is & 7, reg>), ...); \
    }((base_offset), std::make_index_sequence<45>{});

#define FILL_EXG(base_offset, Ax, Ay) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + (Is & 7) + ((Is >> 3) << 9)] = \
             EXG<(Is >> 3), Ax, Is & 7, Ay>), ...); \
    }((base_offset), std::make_index_sequence<64>{});

static constexpr std::array<INTERPRET_Function, 4096> buildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};

    auto fill = [&table](int first, int last, INTERPRET_Function func) {
        for (int i = first; i <= last; ++i)
            table[i] = func;
    };

    auto EA = [](int mode, int reg) constexpr { return (mode << 3) | reg; };

    fill(00000, 07777, ILLEGAL);

    FILL_ALL_RD_EAs_no_An_reg(  00700,  MULS_W, 0);
    FILL_ALL_RD_EAs_no_An_reg(  00700,  MULS_W, 1);
    FILL_ALL_RD_EAs_no_An_reg(  00700,  MULS_W, 2);
    FILL_ALL_RD_EAs_no_An_reg(  00700,  MULS_W, 3);
    FILL_ALL_RD_EAs_no_An_reg(  00700,  MULS_W, 4);
    FILL_ALL_RD_EAs_no_An_reg(  00700,  MULS_W, 5);
    FILL_ALL_RD_EAs_no_An_reg(  00700,  MULS_W, 6);
    FILL_ALL_RD_EAs_no_An_reg(  00700,  MULS_W, 7);

    FILL_ALL_RD_EAs_no_An_reg(  00300,  MULU_W, 0);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  MULU_W, 1);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  MULU_W, 2);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  MULU_W, 3);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  MULU_W, 4);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  MULU_W, 5);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  MULU_W, 6);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  MULU_W, 7);

    FILL_EXG(                   00500,  false,  false);
    FILL_EXG(                   00510,  true,   true);
    FILL_EXG(                   00610,  false,  true);

    FILL_ALL_RD_EAs_no_An(      00000,  AND_EA_to_Dn, 0,  BYTE);
    FILL_ALL_RD_EAs_no_An(      01000,  AND_EA_to_Dn, 1,  BYTE);
    FILL_ALL_RD_EAs_no_An(      02000,  AND_EA_to_Dn, 2,  BYTE);
    FILL_ALL_RD_EAs_no_An(      03000,  AND_EA_to_Dn, 3,  BYTE);
    FILL_ALL_RD_EAs_no_An(      04000,  AND_EA_to_Dn, 4,  BYTE);
    FILL_ALL_RD_EAs_no_An(      05000,  AND_EA_to_Dn, 5,  BYTE);
    FILL_ALL_RD_EAs_no_An(      06000,  AND_EA_to_Dn, 6,  BYTE);
    FILL_ALL_RD_EAs_no_An(      07000,  AND_EA_to_Dn, 7,  BYTE);

    FILL_ALL_RD_EAs_no_An(      00100,  AND_EA_to_Dn, 0,  WORD);
    FILL_ALL_RD_EAs_no_An(      01100,  AND_EA_to_Dn, 1,  WORD);
    FILL_ALL_RD_EAs_no_An(      02100,  AND_EA_to_Dn, 2,  WORD);
    FILL_ALL_RD_EAs_no_An(      03100,  AND_EA_to_Dn, 3,  WORD);
    FILL_ALL_RD_EAs_no_An(      04100,  AND_EA_to_Dn, 4,  WORD);
    FILL_ALL_RD_EAs_no_An(      05100,  AND_EA_to_Dn, 5,  WORD);
    FILL_ALL_RD_EAs_no_An(      06100,  AND_EA_to_Dn, 6,  WORD);
    FILL_ALL_RD_EAs_no_An(      07100,  AND_EA_to_Dn, 7,  WORD);

    FILL_ALL_RD_EAs_no_An(      00200,  AND_EA_to_Dn, 0,  LONG);
    FILL_ALL_RD_EAs_no_An(      01200,  AND_EA_to_Dn, 1,  LONG);
    FILL_ALL_RD_EAs_no_An(      02200,  AND_EA_to_Dn, 2,  LONG);
    FILL_ALL_RD_EAs_no_An(      03200,  AND_EA_to_Dn, 3,  LONG);
    FILL_ALL_RD_EAs_no_An(      04200,  AND_EA_to_Dn, 4,  LONG);
    FILL_ALL_RD_EAs_no_An(      05200,  AND_EA_to_Dn, 5,  LONG);
    FILL_ALL_RD_EAs_no_An(      06200,  AND_EA_to_Dn, 6,  LONG);
    FILL_ALL_RD_EAs_no_An(      07200,  AND_EA_to_Dn, 7,  LONG);

    FILL_ALL_WR_EAs(            00400,  AND_Dn_to_EA, 0,  BYTE);
    FILL_ALL_WR_EAs(            01400,  AND_Dn_to_EA, 1,  BYTE);
    FILL_ALL_WR_EAs(            02400,  AND_Dn_to_EA, 2,  BYTE);
    FILL_ALL_WR_EAs(            03400,  AND_Dn_to_EA, 3,  BYTE);
    FILL_ALL_WR_EAs(            04400,  AND_Dn_to_EA, 4,  BYTE);
    FILL_ALL_WR_EAs(            05400,  AND_Dn_to_EA, 5,  BYTE);
    FILL_ALL_WR_EAs(            06400,  AND_Dn_to_EA, 6,  BYTE);
    FILL_ALL_WR_EAs(            07400,  AND_Dn_to_EA, 7,  BYTE);

    FILL_ALL_WR_EAs(            00500,  AND_Dn_to_EA, 0,  WORD);
    FILL_ALL_WR_EAs(            01500,  AND_Dn_to_EA, 1,  WORD);
    FILL_ALL_WR_EAs(            02500,  AND_Dn_to_EA, 2,  WORD);
    FILL_ALL_WR_EAs(            03500,  AND_Dn_to_EA, 3,  WORD);
    FILL_ALL_WR_EAs(            04500,  AND_Dn_to_EA, 4,  WORD);
    FILL_ALL_WR_EAs(            05500,  AND_Dn_to_EA, 5,  WORD);
    FILL_ALL_WR_EAs(            06500,  AND_Dn_to_EA, 6,  WORD);
    FILL_ALL_WR_EAs(            07500,  AND_Dn_to_EA, 7,  WORD);

    FILL_ALL_WR_EAs(            00600,  AND_Dn_to_EA, 0,  LONG);
    FILL_ALL_WR_EAs(            01600,  AND_Dn_to_EA, 1,  LONG);
    FILL_ALL_WR_EAs(            02600,  AND_Dn_to_EA, 2,  LONG);
    FILL_ALL_WR_EAs(            03600,  AND_Dn_to_EA, 3,  LONG);
    FILL_ALL_WR_EAs(            04600,  AND_Dn_to_EA, 4,  LONG);
    FILL_ALL_WR_EAs(            05600,  AND_Dn_to_EA, 5,  LONG);
    FILL_ALL_WR_EAs(            06600,  AND_Dn_to_EA, 6,  LONG);
    FILL_ALL_WR_EAs(            07600,  AND_Dn_to_EA, 7,  LONG);

    return table;
}

} // Emu68::M68k::Interpreter

static constexpr auto InsnTable = Emu68::M68k::Interpreter::buildInsnTable();

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_lineC(uint32_t opcode)
{
    InsnTable[opcode & 0xfff](opcode);
}

