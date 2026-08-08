#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter.hpp>

#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter {

template<uint8_t Mode, uint8_t Reg, uint8_t An, class Type>
requires (sizeof(Type) > 1)
void SUBA(uint32_t)
{
    PC = PC + 2;
    Type val = loadFromEA<Mode, Reg, Type>(); 
    setA<An, LONG>(getA<An, LONG>() - val);
}

template<uint8_t Mode, uint8_t Reg, uint8_t Dn, class Type>
requires (Mode > 1)
void SUB_Dn_to_EA(uint32_t)
{
    Type dval = getD<Dn, Type>();
    PC = PC + 2;

    readModifyWriteEA<Mode, Reg, Type>([&](Type v) -> Type {
        auto [result, ccr] = arithWithFlags<Type, true>(v, dval);
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
        return result;
    });
}

template<uint8_t Mode, uint8_t Reg, uint8_t Dn, class Type>
requires (Mode != 1 || (Mode == 1 && sizeof(Type) > 1))
void SUB_EA_to_Dn(uint32_t)
{
    PC = PC + 2;
    Type val = loadFromEA<Mode, Reg, Type>();

    auto [result, ccr] = arithWithFlags<Type, true>(getD<Dn, Type>(), val);
    SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
    setD<Dn, Type>(result);
}

template<bool MemForm, uint8_t Rx, uint8_t Ry, class Type>
void SUBX(uint32_t)
{
    PC += 2;
    uint8_t x_in = (SR & SR_X) ? 1 : 0;

    Type a, b;
    Type *dst;

    if constexpr (!MemForm) {
        a = getD<Ry, Type>();
        b = getD<Rx, Type>();
    } else {
        uint32_t srcAddr = getEA<4, Ry, Type>();
        uint32_t dstAddr = getEA<4, Rx, Type>();
        a = *(Type *)(uintptr_t)srcAddr;
        b = *(Type *)(uintptr_t)dstAddr;
        dst = (Type *)(uintptr_t)dstAddr;
    }

    auto [result, ccr] = arithXWithFlags<Type, true>(b, a, x_in);
    ccr = (ccr & ~SR_Z) | (ccr & SR & SR_Z);
    SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);

    if constexpr (!MemForm) { setD<Rx, Type>(result); }
    else                    { *dst = result; }
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

#define FILL_SUBX(base_offset, mem_form, size) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + ((Is >> 3) << 9) + (Is & 7)] = \
             SUBX<mem_form, (Is >> 3), Is & 7, size>), ...); \
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

    FILL_ALL_RD_EAs(        00700,  SUBA,         0,  LONG);
    FILL_ALL_RD_EAs(        01700,  SUBA,         1,  LONG);
    FILL_ALL_RD_EAs(        02700,  SUBA,         2,  LONG);
    FILL_ALL_RD_EAs(        03700,  SUBA,         3,  LONG);
    FILL_ALL_RD_EAs(        04700,  SUBA,         4,  LONG);
    FILL_ALL_RD_EAs(        05700,  SUBA,         5,  LONG);
    FILL_ALL_RD_EAs(        06700,  SUBA,         6,  LONG);
    FILL_ALL_RD_EAs(        07700,  SUBA,         7,  LONG);

    FILL_ALL_RD_EAs(        00300,  SUBA,         0,  WORD);
    FILL_ALL_RD_EAs(        01300,  SUBA,         1,  WORD);
    FILL_ALL_RD_EAs(        02300,  SUBA,         2,  WORD);
    FILL_ALL_RD_EAs(        03300,  SUBA,         3,  WORD);
    FILL_ALL_RD_EAs(        04300,  SUBA,         4,  WORD);
    FILL_ALL_RD_EAs(        05300,  SUBA,         5,  WORD);
    FILL_ALL_RD_EAs(        06300,  SUBA,         6,  WORD);
    FILL_ALL_RD_EAs(        07300,  SUBA,         7,  WORD);

    FILL_ALL_RD_EAs_no_An(  00000,  SUB_EA_to_Dn, 0,  BYTE);
    FILL_ALL_RD_EAs_no_An(  01000,  SUB_EA_to_Dn, 1,  BYTE);
    FILL_ALL_RD_EAs_no_An(  02000,  SUB_EA_to_Dn, 2,  BYTE);
    FILL_ALL_RD_EAs_no_An(  03000,  SUB_EA_to_Dn, 3,  BYTE);
    FILL_ALL_RD_EAs_no_An(  04000,  SUB_EA_to_Dn, 4,  BYTE);
    FILL_ALL_RD_EAs_no_An(  05000,  SUB_EA_to_Dn, 5,  BYTE);
    FILL_ALL_RD_EAs_no_An(  06000,  SUB_EA_to_Dn, 6,  BYTE);
    FILL_ALL_RD_EAs_no_An(  07000,  SUB_EA_to_Dn, 7,  BYTE);

    FILL_ALL_RD_EAs(        00100,  SUB_EA_to_Dn, 0,  WORD);
    FILL_ALL_RD_EAs(        01100,  SUB_EA_to_Dn, 1,  WORD);
    FILL_ALL_RD_EAs(        02100,  SUB_EA_to_Dn, 2,  WORD);
    FILL_ALL_RD_EAs(        03100,  SUB_EA_to_Dn, 3,  WORD);
    FILL_ALL_RD_EAs(        04100,  SUB_EA_to_Dn, 4,  WORD);
    FILL_ALL_RD_EAs(        05100,  SUB_EA_to_Dn, 5,  WORD);
    FILL_ALL_RD_EAs(        06100,  SUB_EA_to_Dn, 6,  WORD);
    FILL_ALL_RD_EAs(        07100,  SUB_EA_to_Dn, 7,  WORD);

    FILL_ALL_RD_EAs(        00200,  SUB_EA_to_Dn, 0,  LONG);
    FILL_ALL_RD_EAs(        01200,  SUB_EA_to_Dn, 1,  LONG);
    FILL_ALL_RD_EAs(        02200,  SUB_EA_to_Dn, 2,  LONG);
    FILL_ALL_RD_EAs(        03200,  SUB_EA_to_Dn, 3,  LONG);
    FILL_ALL_RD_EAs(        04200,  SUB_EA_to_Dn, 4,  LONG);
    FILL_ALL_RD_EAs(        05200,  SUB_EA_to_Dn, 5,  LONG);
    FILL_ALL_RD_EAs(        06200,  SUB_EA_to_Dn, 6,  LONG);
    FILL_ALL_RD_EAs(        07200,  SUB_EA_to_Dn, 7,  LONG);

    FILL_ALL_WR_EAs(        00400,  SUB_Dn_to_EA, 0,  BYTE);
    FILL_ALL_WR_EAs(        01400,  SUB_Dn_to_EA, 1,  BYTE);
    FILL_ALL_WR_EAs(        02400,  SUB_Dn_to_EA, 2,  BYTE);
    FILL_ALL_WR_EAs(        03400,  SUB_Dn_to_EA, 3,  BYTE);
    FILL_ALL_WR_EAs(        04400,  SUB_Dn_to_EA, 4,  BYTE);
    FILL_ALL_WR_EAs(        05400,  SUB_Dn_to_EA, 5,  BYTE);
    FILL_ALL_WR_EAs(        06400,  SUB_Dn_to_EA, 6,  BYTE);
    FILL_ALL_WR_EAs(        07400,  SUB_Dn_to_EA, 7,  BYTE);

    FILL_ALL_WR_EAs(        00500,  SUB_Dn_to_EA, 0,  WORD);
    FILL_ALL_WR_EAs(        01500,  SUB_Dn_to_EA, 1,  WORD);
    FILL_ALL_WR_EAs(        02500,  SUB_Dn_to_EA, 2,  WORD);
    FILL_ALL_WR_EAs(        03500,  SUB_Dn_to_EA, 3,  WORD);
    FILL_ALL_WR_EAs(        04500,  SUB_Dn_to_EA, 4,  WORD);
    FILL_ALL_WR_EAs(        05500,  SUB_Dn_to_EA, 5,  WORD);
    FILL_ALL_WR_EAs(        06500,  SUB_Dn_to_EA, 6,  WORD);
    FILL_ALL_WR_EAs(        07500,  SUB_Dn_to_EA, 7,  WORD);

    FILL_ALL_WR_EAs(        00600,  SUB_Dn_to_EA, 0,  LONG);
    FILL_ALL_WR_EAs(        01600,  SUB_Dn_to_EA, 1,  LONG);
    FILL_ALL_WR_EAs(        02600,  SUB_Dn_to_EA, 2,  LONG);
    FILL_ALL_WR_EAs(        03600,  SUB_Dn_to_EA, 3,  LONG);
    FILL_ALL_WR_EAs(        04600,  SUB_Dn_to_EA, 4,  LONG);
    FILL_ALL_WR_EAs(        05600,  SUB_Dn_to_EA, 5,  LONG);
    FILL_ALL_WR_EAs(        06600,  SUB_Dn_to_EA, 6,  LONG);
    FILL_ALL_WR_EAs(        07600,  SUB_Dn_to_EA, 7,  LONG);

    FILL_SUBX(              00400,  false,            BYTE);
    FILL_SUBX(              00500,  false,            WORD);
    FILL_SUBX(              00600,  false,            LONG);
    
    FILL_SUBX(              00410,  true,             BYTE);
    FILL_SUBX(              00510,  true,             WORD);
    FILL_SUBX(              00610,  true,             LONG);

    return table;
}

} // Emu68::M68k::Interpreter

static constexpr auto InsnTable = Emu68::M68k::Interpreter::buildInsnTable();

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line9(uint32_t opcode)
{
    InsnTable[opcode & 0xfff](opcode);
}

