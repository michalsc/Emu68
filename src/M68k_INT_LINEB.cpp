#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter.hpp>

#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter::LineB {

template<uint8_t Mode, uint8_t Reg, bool IsAn, uint8_t DstReg, class Type>
void CMP(uint32_t)
{
    PC += 2;
    Type eaval = loadFromEA<Mode, Reg, Type>();
    Type regval;

    if constexpr (IsAn) { regval = getA<DstReg, Type>(); }
    else                { regval = getD<DstReg, Type>(); }

    auto [result, ccr] = arithWithFlags<Type, true>(regval, eaval);

    (void)result;
    SR = (SR & ~SR_NZVC) | ccr;
}

#define FILL_ALL_CMP(size) \
[&]<std::size_t... Is>(std::index_sequence<Is...>) consteval { \
    auto fillOne = [&]<std::size_t I>() consteval { \
        constexpr std::size_t DstReg = (I >> 6) & 7; \
        constexpr std::size_t SrcMode = (I >> 3) & 7; \
        constexpr std::size_t SrcReg = I & 7; \
        if constexpr ((sizeof(size) > 1 || (SrcMode != 1)) && ((SrcMode < 7) || (SrcMode == 7 && SrcReg <= 4))) { \
            constexpr int base = ((DstReg & 7) << 9) | (sizeof(size) == 1 ? 0 : sizeof(size) == 2 ? 1 : 2) << 6; \
            table[base + EA(SrcMode, SrcReg)] = CMP<SrcMode, SrcReg, false, DstReg, size>; \
        } \
    }; \
    (fillOne.template operator()<Is>(), ...); \
}(std::make_index_sequence<8 * 64>{});

#define FILL_ALL_CMPA(size) \
[&]<std::size_t... Is>(std::index_sequence<Is...>) consteval { \
    auto fillOne = [&]<std::size_t I>() consteval { \
        constexpr std::size_t DstReg = (I >> 6) & 7; \
        constexpr std::size_t SrcMode = (I >> 3) & 7; \
        constexpr std::size_t SrcReg = I & 7; \
        if constexpr ((SrcMode < 7) || (SrcMode == 7 && SrcReg <= 4)) { \
            constexpr int base = ((DstReg & 7) << 9) | (sizeof(size) == 2 ? 3 : 7) << 6; \
            table[base + EA(SrcMode, SrcReg)] = CMP<SrcMode, SrcReg, true, DstReg, size>; \
        } \
    }; \
    (fillOne.template operator()<Is>(), ...); \
}(std::make_index_sequence<8 * 64>{});

static consteval std::array<INTERPRET_Function, 4096> buildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};

    auto fill = [&table](int first, int last, INTERPRET_Function func) {
        for (int i = first; i <= last; ++i) {
            table[i] = func;
        }
    };

    auto EA = [](int mode, int reg) constexpr { return (mode << 3) | reg; };

    fill(00000, 07777, ILLEGAL);

    FILL_ALL_CMP(BYTE);
    FILL_ALL_CMP(WORD);
    FILL_ALL_CMP(LONG);

    FILL_ALL_CMPA(WORD);
    FILL_ALL_CMPA(LONG);

    return table;
}

constexpr auto InsnTable __attribute__((aligned(4096),section(".int.jumptable.b"))) = buildInsnTable();

} // Emu68::M68k::Interpreter::LineB

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_lineB(uint32_t opcode)
{
    Emu68::M68k::Interpreter::LineB::InsnTable[opcode & 0xfff](opcode);
}

