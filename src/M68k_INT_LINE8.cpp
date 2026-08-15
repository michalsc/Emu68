#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter.hpp>

#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter::Line8 {

template<uint8_t Mode, uint8_t Reg, uint8_t Dn>
void DIVS_W(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    int16_t src;
    uint32_t orig_PC = PC;

    PC += 2;
    src = loadFromEA<Mode, Reg, int16_t>();

    if (src == 0) {
        raiseException(VECTOR_DIVIDE_BY_ZERO, ExceptionFrameFormat::FORMAT_2, orig_PC, 0);
    } else {
        int32_t dval = getD<Dn, int32_t>();
        int32_t drem;

        drem = dval % (int32_t)src;
        dval = dval / (int32_t)src;

        if ((int32_t)dval != (int16_t)dval) {
            SR = (SR & ~SR_Calt) | SR_Valt;
            return;
        }

        setD<Dn, uint32_t>((drem << 16) | ((uint32_t)dval & 0xffff));

        if ((int16_t)dval == 0) {
            sr |= SR_Z;
        } else if ((int16_t)dval < 0) {
            sr |= SR_N;
        }

        SR = sr;
    }
}

template<uint8_t Mode, uint8_t Reg, uint8_t Dn>
void DIVU_W(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    uint16_t src;
    uint32_t orig_PC = PC;

    PC += 2;
    src = loadFromEA<Mode, Reg, uint16_t>();

    if (src == 0) {
        raiseException(VECTOR_DIVIDE_BY_ZERO, ExceptionFrameFormat::FORMAT_2, orig_PC, 0);
    } else {
        uint32_t dval = getD<Dn, uint32_t>();
        uint32_t drem;

        drem = dval % (uint32_t)src;
        dval = dval / (uint32_t)src;

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

template<uint8_t Mode, uint8_t Reg, uint8_t Dn, class Type>
requires (Mode > 1)
void OR_Dn_to_EA(uint32_t)
{
    Type dval = getD<Dn, Type>();
    PC = PC + 2;

    readModifyWriteEA<Mode, Reg, Type>([&](Type v) -> Type {
        uint32_t sr = SR & ~SR_NZVC;

        v |= dval;

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
void OR_EA_to_Dn(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    PC = PC + 2;
    Type val = loadFromEA<Mode, Reg, Type>();
    Type dval = getD<Dn, Type>();

    dval |= val;

    if (dval == 0) {
        sr |= SR_Z;
    } else if (dval < 0) {
        sr |= SR_N;
    }
    SR = sr;

    setD<Dn, Type>(dval);
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

    FILL_ALL_RD_EAs_no_An_reg(  00700,  DIVS_W, 0);
    FILL_ALL_RD_EAs_no_An_reg(  00700,  DIVS_W, 1);
    FILL_ALL_RD_EAs_no_An_reg(  00700,  DIVS_W, 2);
    FILL_ALL_RD_EAs_no_An_reg(  00700,  DIVS_W, 3);
    FILL_ALL_RD_EAs_no_An_reg(  00700,  DIVS_W, 4);
    FILL_ALL_RD_EAs_no_An_reg(  00700,  DIVS_W, 5);
    FILL_ALL_RD_EAs_no_An_reg(  00700,  DIVS_W, 6);
    FILL_ALL_RD_EAs_no_An_reg(  00700,  DIVS_W, 7);

    FILL_ALL_RD_EAs_no_An_reg(  00300,  DIVU_W, 0);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  DIVU_W, 1);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  DIVU_W, 2);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  DIVU_W, 3);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  DIVU_W, 4);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  DIVU_W, 5);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  DIVU_W, 6);
    FILL_ALL_RD_EAs_no_An_reg(  00300,  DIVU_W, 7);

    FILL_ALL_RD_EAs_no_An(  00000,  OR_EA_to_Dn, 0,  BYTE);
    FILL_ALL_RD_EAs_no_An(  01000,  OR_EA_to_Dn, 1,  BYTE);
    FILL_ALL_RD_EAs_no_An(  02000,  OR_EA_to_Dn, 2,  BYTE);
    FILL_ALL_RD_EAs_no_An(  03000,  OR_EA_to_Dn, 3,  BYTE);
    FILL_ALL_RD_EAs_no_An(  04000,  OR_EA_to_Dn, 4,  BYTE);
    FILL_ALL_RD_EAs_no_An(  05000,  OR_EA_to_Dn, 5,  BYTE);
    FILL_ALL_RD_EAs_no_An(  06000,  OR_EA_to_Dn, 6,  BYTE);
    FILL_ALL_RD_EAs_no_An(  07000,  OR_EA_to_Dn, 7,  BYTE);

    FILL_ALL_RD_EAs_no_An(  00100,  OR_EA_to_Dn, 0,  WORD);
    FILL_ALL_RD_EAs_no_An(  01100,  OR_EA_to_Dn, 1,  WORD);
    FILL_ALL_RD_EAs_no_An(  02100,  OR_EA_to_Dn, 2,  WORD);
    FILL_ALL_RD_EAs_no_An(  03100,  OR_EA_to_Dn, 3,  WORD);
    FILL_ALL_RD_EAs_no_An(  04100,  OR_EA_to_Dn, 4,  WORD);
    FILL_ALL_RD_EAs_no_An(  05100,  OR_EA_to_Dn, 5,  WORD);
    FILL_ALL_RD_EAs_no_An(  06100,  OR_EA_to_Dn, 6,  WORD);
    FILL_ALL_RD_EAs_no_An(  07100,  OR_EA_to_Dn, 7,  WORD);

    FILL_ALL_RD_EAs_no_An(  00200,  OR_EA_to_Dn, 0,  LONG);
    FILL_ALL_RD_EAs_no_An(  01200,  OR_EA_to_Dn, 1,  LONG);
    FILL_ALL_RD_EAs_no_An(  02200,  OR_EA_to_Dn, 2,  LONG);
    FILL_ALL_RD_EAs_no_An(  03200,  OR_EA_to_Dn, 3,  LONG);
    FILL_ALL_RD_EAs_no_An(  04200,  OR_EA_to_Dn, 4,  LONG);
    FILL_ALL_RD_EAs_no_An(  05200,  OR_EA_to_Dn, 5,  LONG);
    FILL_ALL_RD_EAs_no_An(  06200,  OR_EA_to_Dn, 6,  LONG);
    FILL_ALL_RD_EAs_no_An(  07200,  OR_EA_to_Dn, 7,  LONG);

    FILL_ALL_WR_EAs(        00400,  OR_Dn_to_EA, 0,  BYTE);
    FILL_ALL_WR_EAs(        01400,  OR_Dn_to_EA, 1,  BYTE);
    FILL_ALL_WR_EAs(        02400,  OR_Dn_to_EA, 2,  BYTE);
    FILL_ALL_WR_EAs(        03400,  OR_Dn_to_EA, 3,  BYTE);
    FILL_ALL_WR_EAs(        04400,  OR_Dn_to_EA, 4,  BYTE);
    FILL_ALL_WR_EAs(        05400,  OR_Dn_to_EA, 5,  BYTE);
    FILL_ALL_WR_EAs(        06400,  OR_Dn_to_EA, 6,  BYTE);
    FILL_ALL_WR_EAs(        07400,  OR_Dn_to_EA, 7,  BYTE);

    FILL_ALL_WR_EAs(        00500,  OR_Dn_to_EA, 0,  WORD);
    FILL_ALL_WR_EAs(        01500,  OR_Dn_to_EA, 1,  WORD);
    FILL_ALL_WR_EAs(        02500,  OR_Dn_to_EA, 2,  WORD);
    FILL_ALL_WR_EAs(        03500,  OR_Dn_to_EA, 3,  WORD);
    FILL_ALL_WR_EAs(        04500,  OR_Dn_to_EA, 4,  WORD);
    FILL_ALL_WR_EAs(        05500,  OR_Dn_to_EA, 5,  WORD);
    FILL_ALL_WR_EAs(        06500,  OR_Dn_to_EA, 6,  WORD);
    FILL_ALL_WR_EAs(        07500,  OR_Dn_to_EA, 7,  WORD);

    FILL_ALL_WR_EAs(        00600,  OR_Dn_to_EA, 0,  LONG);
    FILL_ALL_WR_EAs(        01600,  OR_Dn_to_EA, 1,  LONG);
    FILL_ALL_WR_EAs(        02600,  OR_Dn_to_EA, 2,  LONG);
    FILL_ALL_WR_EAs(        03600,  OR_Dn_to_EA, 3,  LONG);
    FILL_ALL_WR_EAs(        04600,  OR_Dn_to_EA, 4,  LONG);
    FILL_ALL_WR_EAs(        05600,  OR_Dn_to_EA, 5,  LONG);
    FILL_ALL_WR_EAs(        06600,  OR_Dn_to_EA, 6,  LONG);
    FILL_ALL_WR_EAs(        07600,  OR_Dn_to_EA, 7,  LONG);

    return table;
}

constexpr auto InsnTable __attribute__((used,aligned(4096),section(".int.jumptable.8"))) = buildInsnTable();

} // Emu68::M68k::Interpreter::Line8
